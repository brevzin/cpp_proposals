---
title: "`std::reflect_constant_{array,string}`"
document: P3617R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>

toc: true
tag: reflection
---

# Introduction

One of the Reflection facilities in flight for C++26 is [@P3491R2]{.title}. It provides three functions:

::: std
```cpp
template <ranges::input_range R> // only if the value_type is char or char8_t
consteval auto define_static_string(R&& r) -> ranges::range_value_t<R> const*;

template <class T>
consteval auto define_static_object(T&& v) -> remove_reference_t<T> const*;

template <ranges::input_range R>
consteval auto define_static_array(R&& r) -> span<ranges::range_value_t<R> const>;
```
:::

These are very useful additions to C++26.

However, there are cases where having a `span<T const>` or `char const*` is insufficient.

Matthias Wippich sent us some examples of such cases. Consider C++20 code such as:

::: std
```cpp
template <size_t N>
struct FixedString {
    char data[N] = {};

    constexpr FixedString(char const(&str)[N]) {
        std::ranges::copy(str, str+N, data);
    }
};

template <FixedString S>
struct Test { };
```
:::

This is a widely used pattern for being able to pass string literals as template arguments. However, there is no way to programmatically produce a string to pass in as an argument:

|Approach|Result|
|--|-|
|`using A = Test<"foo">;`|✅|
|`using B = [: substitute(^^Test, {reflect_constant("foo"sv)}) :];`|❌ Error: `std::string_view` isn't structural, but even if it was, this wouldn't work because you couldn't deduce the size of `S`.|
|`using C = Test<define_static_string("foo")>;`|❌ Error: cannot deduce the size of `S`|
|`using D = [:substitute(^^Test, {reflect_constant(define_static_string("foo"))}):];`|❌ Error: cannot deduce the size of `S`|

The issue here is that `define_static_string` returns a `char const*`, which loses size information. If, instead, we had a lower layer function that returned a reflection of an array, we could easily use that:

|Approach|Result|
|--|-|
|`using E = Test<[:reflect_constant_string("foo"):]>;`|✅|
|`using F = [:substitute(^^Test, {reflect_constant_string("foo")}):];`|✅|

Another situation in which this comes up is in dealing with reflections of members. When you want to iterate over your members one-at-a-time, then `define_static_array` coupled with [@P1306R4]{.title} is perfectly sufficient:

::: std
```cpp
template <class T>
auto f(T const& var) -> void {
    template for (constexpr auto M : define_static_array(nsdms(^^T))) {
        do_something_with(var.[:M:]);
    }
}
```
:::

However, some situations require _all_ the members at once. Such as in my [struct of arrays](https://brevzin.github.io/c++/2025/05/02/soa/) implementation. `define_static_array` simply returns a `span`, but if we had a function that returned a reflection of an array, then combining [@P1061R10]{.title} with [@P2686R5]{.title} lets us do this instead:

::: cmptable
### Status Quo
```cpp
template <auto... V>
struct replicator_type {
template<typename F>
    constexpr auto operator>>(F body) const -> decltype(auto) {
        return body.template operator()<V...>();
    }
};

template <auto... V>
replicator_type<V...> replicator = {};

consteval auto expand_all(std::span<std::meta::info const> r)
    -> std::meta::info
{
    std::vector<std::meta::info> rv;
    for (std::meta::info i : r) {
        rv.push_back(reflect_value(i));
    }
    return substitute(^^replicator, rv);
}

template <class T>
struct SoaVector {
    // ...
    // this is a span<info const>
    static constexpr auto ptr_mems =
        define_static_array(nsdms(^^Pointers));
    // ...

    auto operator[](size_t idx) const -> T {
        return [: expand_all(ptr_mems) :] >> [this, idx]<auto... M>{
            return T{pointers_.[:M:][idx]...};
        };
    }
};
```

### Proposed
```cpp





















template <class T>
struct SoaVector {
    // ...
    // reflection of an object of type info const[N]
    static constexpr auto ptr_mems =
        reflect_constant_array(nsdms(^^Pointers));
    // ...

    auto operator[](size_t idx) const -> T {
        constexpr auto [...M] = [: ptr_mems :];
        return T{pointers_.[:M:][idx]...};
    }
};
```
:::

With the reflection of an array object, we can directly splice it and use structured bindings to get the pack of members we need all in one go — without any layers of indirection. It's a much more direct solution.

Having the higher level facilities is very useful, having this additional lower level facility would also be useful.

# Implementation Approach

The implementation is actually very straightforward. `define_static_array` already has to produce a reflection of an array in order to do its job. So we simply split that step in two:

::: cmptable
### Status Quo
```cpp
template <typename T, T... Vs>
inline constexpr T __fixed_array[sizeof...(Vs)]{Vs...};

template <ranges::input_range R>
consteval auto define_static_array(R&& r)
    -> span<ranges::range_value_t<R> const>
{
    using T = ranges::range_value_t<R>;

    // produce the array
    auto args = vector<meta::info>{
        ^^ranges::range_value_t<R>};
    for (auto&& elem : r) {
        args.push_back(meta::reflect_constant(elem));
    }
    auto array =  substitute(^^__fixed_array, args);

    // turn the array into a span
    return span<T const>(
        extract<T const*>(array),
        extent(type_of(array)));
}
```

### Proposed
```cpp
template <typename T, T... Vs>
inline constexpr T __fixed_array[sizeof...(Vs)]{Vs...};

template <ranges::input_range R>
consteval auto reflect_constant_array(R&& r) -> meta::info {
    auto args = vector<meta::info>{
        ^^ranges::range_value_t<R>};
    for (auto&& elem : r) {
        args.push_back(meta::reflect_constant(elem));
    }
    return substitute(^^__fixed_array, args);
}

template <ranges::input_range R>
consteval auto define_static_array(R&& r)
    -> span<ranges::range_value_t<R> const>
{
    using T = ranges::range_value_t<R>;

    // produce the array
    auto array = reflect_constant_array(r);

    // turn the array into a span
    return span<T const>(
        extract<T const*>(array),
        extent(type_of(array)));
}
```
:::

The only difference is exposing `std::reflect_constant_array` instead of it being an implementation detail of `std::define_static_array`. And similar for `std::reflect_constant_string`. It's a simple refactoring.

# Scheduling for C++26

This is a pure library addition and doesn't strictly need to be in C++26. Moreover, it's implementable on top of [@P2996R12] today.

However, I suspect it's a useful one that people will do themselves and that will proliferate anyway. It really seems closer to completing/fixing [@P3491R2] than being a novel proposal in its own right.

The other advantage of having it in the compiler rather than user-defined is that the implementation can do better than this by allowing the `__fixed_array` objects to overlap (see discussion in other paper).

So I think we should try to do it for C++26. There is minimal wording review overhead, given that (as you can see below), this feature simply moves the [@P3491R2] wording somewhere else, rather than introducing a lot of new wording.

# Proposal

Add the two facilities:

::: std
```cpp
namespace std::meta {
  template <ranges::input_range R> // only if the value_type is char or char8_t
  consteval auto reflect_constant_string(R&& r) -> info;

  template <ranges::input_range R>
  consteval auto reflect_constant_array(R&& r) -> info;
}
```
:::

The naming here mirrors `std::meta::reflect_constant`. Since these facilities return a reflection, it makes sense for them to live in `std::meta`, unlike `define_static_{string,array}` — which only use reflection as an implementation detail.

## Wording

The wording is presented as a diff on [@P3491R2].

Change [intro.object]{.sref} to instead have the results of `reflect_constant_{string,array}` be potentially non-unique:

::: std
[9]{.pnum} An object is a *potentially non-unique object* if it is

* [9.1]{.pnum} a string literal object ([lex.string]),
* [9.2]{.pnum} the backing array of an initializer list ([dcl.init.ref]),
* [9.3]{.pnum} the object declared by a call to [`std::define_static_string`]{.rm} [`std::meta::reflect_constant_string`]{.addu} or [`std::define_static_array`]{.rm} [`std::meta::reflect_constant_array`]{.addu}, or
* [9.4]{.pnum} a subobject thereof.
:::

And likewise in [basic.compound]{.sref}:

::: std
[?]{.pnum} A pointer value pointing to a potentially non-unique object `$O$` ([intro.object]) is *associated with* the evaluation of the `$string-literal$` ([lex.string])[,]{.addu} [or]{.rm} initializer list ([dcl.init.list]), or a call to either [`std::define_static_string` or `std::define_static_array` ([meta.define.static])]{.rm} [`std::meta::reflect_constant_string` or `std::meta::reflect_constant_array` ([meta.reflection.array])]{.addu} that resulted in the string literal object or backing array, respectively, that is `$O$` or of which `$O$` is a subobject. [A pointer value obtained by pointer arithmetic ([expr.add]) from a pointer value associated with an evaluation `$E$` is also associated with `$E$`.]{.note}
:::

Add to [meta.syn], by `reflect_constant`:

::: std
```diff
namespace std::meta {

  // [meta.reflection.result], expression result reflection
  template<class T>
    consteval info reflect_value(const T& value);
  template<class T>
    consteval info reflect_object(T& object);
  template<class T>
    consteval info reflect_function(T& fn);

+ // [meta.reflection.array], promoting to runtime storage
+ template <ranges::input_range R>
+   consteval info reflect_constant_string(R&& r);
+
+  template <ranges::input_range R>
+    consteval info reflect_constant_array(R&& r);
}
```
:::

Move the corresponding wording from [meta.define.static] to the new clause [meta.reflection.array]. The words here are the same as in the other paper — it's just that we're returning a reflection now instead of extracting a value out of it:

::: std
::: addu
[1]{.pnum} The functions in this clause are useful for promoting compile-time storage into runtime storage.

```cpp
template <ranges::input_range R>
consteval info reflect_constant_string(R&& r);
```

[#]{.pnum} Let `$CharT$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$CharT$` is one of `char`, `wchar_t`, `char8_t`, `char16_t`, or `char32_t`.

[#]{.pnum} Let `$V$` be the pack of elements of type `$CharT$` in `r`. If `r` is a string literal, then `$V$` does not include the trailing null terminator of `r`.

[#]{.pnum} Let `$P$` be the template parameter object ([temp.param]) of type `const $CharT$[sizeof...(V)+1]` initialized with `{V..., $CharT$()}`.

[#]{.pnum} *Returns*: `^^$P$`.

[#]{.pnum} [`$P$` is a potentially non-unique object ([intro.object])]{.note}

```cpp
template <ranges::input_range R>
consteval info reflect_constant_array(R&& r);
```

[#]{.pnum} Let `$T$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$T$` is a structural type ([temp.param]) and `constructible_from<$T$, ranges::range_reference_t<R>>` is `true` and `copy_constructible<$T$>` is `true`.

[#]{.pnum} Let `$V$` be the pack of elements of type `$T$` constructed from the elements of `r`.

[#]{.pnum} Let `$P$` be the template parameter object ([temp.param]) of type `const $T$[sizeof...(V)]` initialized with `{V...}`.

[#]{.pnum} *Returns*: `^^$P$`.

[#]{.pnum} [`$P$` is a potentially non-unique object ([intro.object])]{.note}
:::
:::

And then simplify the corresponding wording in [meta.define.static]:

::: std

[1]{.pnum} The functions in this clause are useful for promoting compile-time storage into runtime storage.

```cpp
template <ranges::input_range R>
consteval const ranges::range_value_t<R>* define_static_string(R&& r);
```

::: rm
[#]{.pnum} Let `$CharT$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$CharT$` is one of `char`, `wchar_t`, `char8_t`, `char16_t`, or `char32_t`.

[#]{.pnum} Let `$V$` be the pack of elements of type `$CharT$` in `r`. If `r` is a string literal, then `$V$` does not include the trailing null terminator of `r`.

[#]{.pnum} Let `$P$` be the template parameter object ([temp.param]) of type `const $CharT$[sizeof...(V)+1]` initialized with `{V..., $CharT$()}`.

[#]{.pnum} *Returns*: `$P$`.

[#]{.pnum} [`$P$` is a potentially non-unique object ([intro.object])]{.note}
:::

::: addu
[#]{.pnum} *Effects*: Equivalent to: `return extract<const ranges::range_value_t<R>*>(meta::reflect_constant_string(r));`
:::

```cpp
template <class T>
consteval const remove_cvref_t<T>* define_static_object(T&& t);
```

[#]{.pnum} Let `U` be `remove_cvref_t<T>`.

[#]{.pnum} *Mandates*: `U` is a structural type ([temp.param]) and `constructible_from<U, T>` is `true`.

[#]{.pnum} Let `$P$` be the template parameter object ([temp.param]) of type `const U` initialized with `t`.

[#]{.pnum} *Returns*: `std::addressof($P$)`.

```cpp
template <ranges::input_range R>
consteval span<const ranges::range_value_t<R>> define_static_array(R&& r);
```

::: rm
[#]{.pnum} Let `$T$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$T$` is a structural type ([temp.param]) and `constructible_from<$T$, ranges::range_reference_t<R>>` is `true` and `copy_constructible<$T$>` is `true`.

[#]{.pnum} Let `$V$` be the pack of elements of type `$T$` constructed from the elements of `r`.

[#]{.pnum} Let `$P$` be the template parameter object ([temp.param]) of type `const $T$[sizeof...(V)]` initialized with `{V...}`.

[#]{.pnum} *Returns*: `span<const $T$>($P$)`.

[#]{.pnum} [`$P$` is a potentially non-unique object ([intro.object])]{.note}
:::

::: addu
[#]{.pnum} *Effects*: Equivalent to:

  ```cpp
  using T = ranges::range_value_t<R>;
  meta::info array = meta::reflect_constant_array(r);
  return span<const T>(extract<const T*>(array), extent(type_of(array)));
  ```
:::
:::

## Feature-Test Macro

Bump the value of `__cpp_lib_define_static` in [version.syn]{.sref}:

::: std
```diff
- #define __cpp_lib_define_static 2025XX // freestanding, also in <meta>
+ #define __cpp_lib_define_static 2025XX // freestanding, also in <meta>
```
:::

# Acknowledgements

Thanks to Matthias Wippich for the insightful observation that led to this paper.