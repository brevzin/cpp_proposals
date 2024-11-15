---
title: "`define_static_string` and `define_static_array`"
document: P3491R0
date: today
audience: LEWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
tag: constexpr
---

# Introduction

These functions were originally proposed as part of [@P2996R7], but are being split off into their own paper.

There are situations where it is useful to take a string (or array) from compile time and promote it to static storage for use at runtime. We currently have neither:

* non-transient constexpr allocation (see [@P1974R0], [@P2670R1]), nor
* generalized support for class types as non-type template parameters (see [@P2484R0], [@P3380R0])

If we had non-transient constexpr allocation, we could just directly declare a static constexpr variable. And if we could use these container types like `std::string` and `std::vector<T>` as non-type template parameter types, then we would use those directly too.

But until we have such a language solution, people have over time come up with their own workarounds. For instance, Jason Turner in a [recent talk](https://www.youtube.com/watch?v=_AefJX66io8) presents what he calls the "constexpr two-step." It's a useful pattern, although limited and cumbersome (it also requires specifying a maximum capacity).

Similarly, the lack of general support for non-type template parameters means we couldn't have a `std::string` template parameter (even if we had non-transient constexpr allocation), but promoting the contents of a string to an external linkage, static storage duration array of `const char` means that you can use a pointer to that array as a non-type template parameter just fine.

So having facilities to solve these problems until the general language solution arises is very valuable.

# Proposal

This paper proposes two new additions — `std::define_static_string` and `std::define_static_array`, as well as a helper function for dealing with string literals:

::: std
```cpp
namespace std {
  consteval auto is_string_literal(char const* p) -> bool;
  consteval auto is_string_literal(char8_t const* p) -> bool;

  template <ranges::input_range R> // only if the value_type is char or char8_t
  consteval auto define_static_string(R&& r) -> ranges::range_value_t<R> const*;

  template <ranges::input_range R>
  consteval auto define_static_array(R&& r) -> span<ranges::range_value_t<R> const>;
}
```
:::

`is_string_literal` takes a pointer to either `char const` or `char8_t const`. If it's a pointer to either a string literal `V` or a subobject thereof, these functions return `true`. Otherwise, they return `false`. Note that we can't necessarily return a pointer to the start of the string literal because in the case of overlapping string literals — how do you know which pointer to return?

`define_static_string` is limited to ranges over `char` or `char8_t` and returns a `char const*` or `char8_t const*`, respectively. They return a pointer instead of a `string_view` (or `u8string_view`) specifically to make it clear that they return something null terminated. If `define_static_string` is passed a string literal that is already null-terminated, it will not be doubly null terminated.

`define_static_array` exists to handle the general case for other types, and now has to return a `span` so the caller would have any idea how long the result is. This function requires that the underlying type `T` be copyable, but does not mandate structural.

Technically, `define_static_array` can be used to implement `define_static_string`:

::: std
```cpp
consteval auto define_static_string(string_view str) -> char const* {
  return define_static_array(views::concat(str, views::single('\0'))).data();
}
```
:::

But that's a fairly awkward implementation, and the string use-case is sufficiently common as to merit a more ergonomic solution.

## To Overlap or Not To Overlap

Consider the existence of `template <char const*> struct C;` and the following two translation units:

<table>
<tr><th>TU #1</th><th>TU #2</th></tr>
<tr><td>
```cpp
C<define_static_string("dedup")> c1;
C<define_static_string("dup")> c2;
```
</td>
<td>
```cpp
C<define_static_string("holdup")> c3;
C<define_static_string("dup")> c4;
```
</td>
</tr>
</table>

In the specification in [@P2996R7], the results of `define_static_string` were allowed to overlap. That is, a possible result of this program could be:

<table>
<tr><th>TU #1</th><th>TU #2</th></tr>
<tr><td>
```cpp
inline char const __arr_dedup[] = "dedup";
C<__arr_dedup> c1;
C<__arr_dedup + 2> c2;
```
</td>
<td>
```cpp
inline char const __arr_holdup[] = "holdup";
C<__arr_holdup> c3;
C<__arr_holdup + 3> c4;
```
</td>
</tr>
</table>

This means whether `c2` and `c4` have the same type is unspecified. They could have the same type if the implementation chooses to not overlap (or no overlap is possible). Or they could have different types.

They would have the same type if the implementation produced a distinct array for each value, more like this (as suggested by [@P0424R2]):

<table>
<tr><th>TU #1</th><th>TU #2</th></tr>
<tr><td>
```cpp
inline char const __arr_dedup[] = "dedup";
inline char const __arr_dup[] = "dup";
C<__arr_dedup> c1;
C<__arr_dup> c2;
```
</td>
<td>
```cpp
inline char const __arr_holdup[] = "holdup";
inline char const __arr_dup[] = "dup";
C<__arr_holdup> c3;
C<__arr_dup> c4;
```
</td>
</tr>
</table>

We think the value of *ensuring* template argument equivalence is more valuable than the potential size savings with overlap. So this paper ensures this.

For `define_static_array`, if the underlying type `T` is not structural, this isn't actually feasible: how would we know how to return the same array? If `T` is structural, we can easily ensure that equal invocations produce the _same_ `span` result.

But if `T` is not structural, we have a problem, because `T*` is, regardless. So we have to answer the question of what to do with:

::: std
```cpp
template <auto V> struct C { };

C<define_static_array(r).data()> c1;
C<define_static_array(r).data()> c2;
```
:::

Either:

* this works, and it is unspecified whether `c1` and `c2` have the same type.
* the call to `define_static_array` works, but the resulting pointer is not usable as a non-type template argument (in the same way that string literals are not).
* the call to `define_static_array` mandates that the underlying type is structural.

None of these options is particularly appealing. The last prevents some very motivating use-cases since neither `span` nor `string_view` are structural types yet, which means you cannot reify a `vector<string>` into a `span<string_view>`, but hopefully that can be resolved soon ([@P3380R0]). You can at least reify it into a `span<char const*>`?

For now, this paper proposes the last option, as it's the simplest (and the relative cost will hopefully decrease over time). Allowing the call but rejecting use as non-type template parameters is appealing though.

## Possible Implementation

`define_static_string` can be nearly implemented with the facilities in [@P2996R7], we just need `is_string_literal` to handle the different signature proposed in this paper.

 `define_static_array` for structural types is similar, but for non-structural types requires compiler intrinsic:

::: std
```cpp
template <auto V>
inline constexpr auto __array = V.data();

template <size_t N, class T, class R>
consteval auto define_static_string_impl(R& r) -> T const* {
    array<T, N+1> arr;
    ranges::copy(r, arr.data());
    arr[N] = '\0'; // null terminator
    return extract<T const*>(substitute(^^__array, {meta::reflect_value(arr)}));
}

template <ranges::input_range R>
consteval auto define_static_string(R&& r) -> ranges::range_value_t<R> const* {
    using T = ranges::range_value_t<R>;
    static_assert(std::same_as<T, char> or std::same_as<T, char8_t>);

    if constexpr (not ranges::forward_range<R>) {
        return define_static_string(ranges::to<std::vector>(r));
    } else {
        if constexpr (requires { is_string_literal(r); }) {
            // if it's an array, check if it's a string literal and adjust accordingly
            if (is_string_literal(r)) {
                return define_static_string(basic_string_view(r));
            }
        }

        auto impl = extract<auto(*)(R&) -> T const*>(
            substitute(^^define_static_string_impl,
                       {
                           meta::reflect_value(ranges::distance(r)),
                           ^^T,
                           remove_reference(^^R)
                       }));
        return impl(r);
    }
}
```
:::

[Demo](https://compiler-explorer.com/z/x5c3c7zKE).

Note that this implementation gives the guarantee we talked about in the [previous section](#to-overlap-or-not-to-overlap). Two invocations of `define_static_string` with the same contents will both end up returning a pointer into the same specialization of the (extern linkage) variable template `__array<V>`. We rely on the mangling of `V` (and `std::array` is a structural type if `T` is, which `char` and `char8_t` are) to ensure this for us.

## Examples

### Use as non-type template parameter

::: std
```cpp
template <const char *P> struct C { };

const char msg[] = "strongly in favor";  // just an idea..

C<msg> c1;                          // ok
C<"nope"> c2;                       // ill-formed
C<define_static_string("yay")> c3;  // ok
```
:::

### Pretty-printing

In the absence of general support for non-transient constexpr allocation, such a facility is essential to building utilities like pretty printers.

An example of such an interface might be built as follow:

::: std
```cpp
template <std::meta::info R> requires is_value(R)
  consteval auto render() -> std::string;

template <std::meta::info R> requires is_type(R)
  consteval auto render() -> std::string;

template <std::meta::info R> requires is_variable(R)
  consteval auto render() -> std::string;

// ...

template <std::meta::info R>
consteval auto pretty_print() -> std::string_view {
  return define_static_string(render<R>());
}
```
:::

This strategy [lies at the core](https://github.com/bloomberg/clang-p2996/blob/149cca52811b59b22608f6f6e303f6589969c999/libcxx/include/experimental/meta#L2317-L2321) of how the Clang/P2996 fork builds its example implementation of the `display_string_of` metafunction.

### Promoting Containers

In the Jason Turner talk cited earlier, he demonstrates an [example](https://compiler-explorer.com/z/E7n1T357T) of taking a function that produces a `vector<string>` and promoting that into static storage, in a condensed way so that the function

::: std
```cpp
constexpr std::vector<std::string> get_strings() {
    return {"Jason", "Was", "Here"};
}
```
:::

Gets turned into an array of string views. We could do that fairly straightforwardly, without even needing to take the function `get_strings()` as a template parameter:

::: std
```cpp
consteval auto promote_strings(std::vector<std::string> vs)
    -> std::span<std::string_view const>
{
    // promote the concatenated strings to static storage
    std::string_view promoted = std::define_static_string(
        std::ranges::fold_left(vs, std::string(), std::plus()));

    // now build up all our string views into promoted
    std::vector<std::string_view> views;
    for (size_t offset = 0; std::string const& s : vs) {
        views.push_back(promoted.substr(offset, s.size()));
        offset += s.size();
    }

    // promote our array of string_views
    return std::define_static_array(views);
}

constexpr auto views = promote_strings(get_strings());
```
:::

Or at least, this will work once `string_view` becomes structural. Until then, this can be worked around with a `structural_string_view` type that just has public members for the data and length with an implicit conversion to `string_view`.

### With Expansion Statements

Something like this ([@P1306R2]) is not doable without non-transient constexpr allocation :

::: std
```cpp
constexpr auto f() -> std::vector<int> { return {1, 2, 3}; }

consteval void g() {
    template for (constexpr int I : f()) {
        // doesn't work
    }
}
```
:::

But if we promote the contents of `f()` first, then this would work fine:

::: std
```cpp
consteval void g() {
    template for (constexpr int I : define_static_array(f())) {
        // ok!
    }
}
```
:::

## Related Papers in the Space

A number of other papers have been brought up as being related to this problem, so let's just enumerate them.

* [@P3094R5] proposed `std::basic_fixed_string<char, N>`. It exists to solve the problem that `C<"hello">` needs support right now. Nothing in this paper would make `C<"hello">` work, although it might affect the way that you would implement the type that makes it work.
* [@P3380R0] proposes to extend non-type template parameter support, which could eventually make `std::string` usable as a non-type template parameter. But without non-transient constexpr allocation, this doesn't obviate the need for this paper.
* [@P1974R0] and [@P2670R1] propose approaches to tackle the non-transient allocation problem.

Given non-transient allocation _and_ a `std::string` and `std::vector` that are usable as non-type template parameters, this paper likely becomes unnecessary. Or at least, fairly trivial:

::: std
```cpp
template <auto V>
inline constexpr auto __S = V.c_str();

template <ranges::input_range R>
consteval auto define_static_string(R&& r) -> ranges::range_value_t<R> const* {
    using T = ranges::range_value_t<R>;
    static_assert(std::same_as<T, char> or std::same_as<T, char8_t>);

    auto S = ranges::to<basic_string<T>>(r);
    return extract<T const*>(substitute(^^__S, {meta::reflect_value(S)}));
}
```
:::

The more interesting paper is actually [@P0424R2]. If we bring that paper back, then extend the normalization model described in [@P3380R0] so that string literals are normalized to external linkage arrays as demonstrated in this paper, then it's possible that [@P3094R5] becomes obsolete instead — since then you could _just_ take `char const*` template parameters and `define_static_string` would become a mechanism for producing new string literals.

# Wording

Add to [meta.syn]{.sref}:

::: std
```diff
namespace std {
+ // [meta.string.literal], checking string literals
+ consteval bool is_string_literal(const char* p);
+ consteval bool is_string_literal(const char8_t* p);

+ // [meta.define.static], promoting to runtime storage
+ template <ranges::input_range R>
+   consteval const ranges::range_value_t<R>* define_static_string(R&& r);

+  template <ranges::input_range R>
+    consteval span<const ranges::range_value_t<R>> define_static_array(R&& r);
}
```
:::

Add to the new clause [meta.string.literal]:

::: std
::: addu
```cpp
consteval bool is_string_literal(const char* p);
consteval bool is_string_literal(const char8_t* p);
```

[1]{.pnum} *Returns*: If `p` points to a string literal or a subobject thereof, `true`. Otherwise, `false`.

:::
:::

Add to the new clause [meta.define.static]

::: std
::: addu

[1]{.pnum} The functions in this clause are useful for promoting compile-time storage into runtime storage.

```cpp
template <ranges::input_range R>
consteval const ranges::range_value_t<R>* define_static_string(R&& r);
```

[#]{.pnum} Let `$CharT$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$CharT$` is either `char` or `char8_t`.

[#]{.pnum} Let `$Str$` be the variable template

```cpp
template <class T, T... Vs> inline constexpr T $Str$[] = {Vs..., T{}}; // exposition-only
```

[#]{.pnum} Let `$V$` be the pack of elements of type `$CharT$` in `r`. If `r` is a string literal, then `$V$` does not include the trailing null terminator of `r`.

[#]{.pnum} *Returns*: `$Str$<$CharT$, $V$...>`.

```cpp
template <ranges::input_range R>
consteval span<const ranges::range_value_t<R>> define_static_array(R&& r);
```

[#]{.pnum} Let `$T$` be `ranges::range_value_t<R>`.

[#]{.pnum} *Mandates*: `$T$` is a structural type ([temp.param]) and `constructible_from<$T$, ranges::range_reference_t<R>>` is `true` and `copy_constructible<$T$>` is `true`.

[#]{.pnum} Let `$Arr$` be the variable template

```cpp
template <class T, T... Vs> inline constexpr T $Arr$[] = {Vs...}; // exposition-only
```

[#]{.pnum} Let `$V$` be the pack of elements of type `$T$` constructed from the elements of `r`.

[#]{.pnum} *Returns*: `span($Arr$<$T$, $V$...>)`.
:::
:::

## Feature-Test Macro

Add to [version.syn]{.sref}:

::: bq
::: addu
```
#define __cpp_lib_define_static 2024XX // freestanding, also in <meta>
```
:::
:::