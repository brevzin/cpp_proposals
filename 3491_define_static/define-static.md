---
title: "`define_static_{string,object,array}`"
document: P3491R3
date: today
audience: CWG, LWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
tag: constexpr
---

# Revision History

Since [@P3491R2], merged in [@P3617R0]{.title} following LEWG telecon on May 20th.

Since [@P3491R1], added support for all string literal types. Having discovered [@CWG2765]{.title}, referring to that issue and updating wording to assume its adoption. Retargeting CWG and LWG.

Since [@P3491R0], wording improvements.

# Introduction

These functions were originally proposed as part of [@P2996R7]{.title}, but are being split off into their own paper.

There are situations where it is useful to take a string (or array) from compile time and promote it to static storage for use at runtime. We currently have neither:

* non-transient constexpr allocation (see [@P1974R0]{.title}, [@P2670R1]{.title}), nor
* generalized support for class types as non-type template parameters (see [@P2484R0]{.title}, [@P3380R1]{.title})

If we had non-transient constexpr allocation, we could just directly declare a static constexpr variable. And if we could use these container types like `std::string` and `std::vector<T>` as non-type template parameter types, then we would use those directly too.

But until we have such a language solution, people have over time come up with their own workarounds. For instance, Jason Turner in a [recent talk](https://www.youtube.com/watch?v=_AefJX66io8) presents what he calls the "constexpr two-step." It's a useful pattern, although limited and cumbersome (it also requires specifying a maximum capacity).

Similarly, the lack of general support for non-type template parameters means we couldn't have a `std::string` template parameter (even if we had non-transient constexpr allocation), but promoting the contents of a string to an external linkage, static storage duration array of `const char` means that you can use a pointer to that array as a non-type template parameter just fine.

So having facilities to solve these problems until the general language solution arises is very valuable.

# Proposal

This paper proposes several new library functions, grouped into three kinds:

* helper function for identifying string literals
* three higher-level functions for defining static storage objects: `define_static_string`, `define_static_object`, and `define_static_array`. These are just in `std` as their interface is not strictly reflection-related
* two lower-level functions for returning a reflection of an array: `meta::reflect_constant_string` and `meta::reflect_constant_array` (from [@P3617R0]).


::: std
```cpp
namespace std {
  consteval auto is_string_literal(char const* p) -> bool;
  consteval auto is_string_literal(wchar_t const* p) -> bool;
  consteval auto is_string_literal(char8_t const* p) -> bool;
  consteval auto is_string_literal(char16_t const* p) -> bool;
  consteval auto is_string_literal(char32_t const* p) -> bool;

  namespace meta {
    template <ranges::input_range R> // only if the value_type is char or char8_t
    consteval auto reflect_constant_string(R&& r) -> info;

    template <ranges::input_range R>
    consteval auto reflect_constant_array(R&& r) -> info;
  }

  template <ranges::input_range R> // only if the value_type is char or char8_t
  consteval auto define_static_string(R&& r) -> ranges::range_value_t<R> const*;

  template <class T>
  consteval auto define_static_object(T&& v) -> remove_reference_t<T> const*;

  template <ranges::input_range R>
  consteval auto define_static_array(R&& r) -> span<ranges::range_value_t<R> const>;
}
```
:::

`is_string_literal` takes a pointer to either `char const` or `char8_t const`. If it's a pointer to either a string literal `V` or a subobject thereof, these functions return `true`. Otherwise, they return `false`. Note that we can't necessarily return a pointer to the start of the string literal because in the case of overlapping string literals — how do you know which pointer to return?

`define_static_string` is limited to ranges over `char` or `char8_t` and returns a `char const*` or `char8_t const*`, respectively. They return a pointer instead of a `string_view` (or `u8string_view`) specifically to make it clear that they return something null terminated. If `define_static_string` is passed a string literal that is already null-terminated, it will not be doubly null terminated.

`define_static_array` exists to handle the general case for other types, and now has to return a `span` so the caller would have any idea how long the result is. This function requires that the underlying type `T` be structural.

`define_static_object` is a special case of `define_static_array` for handling a single object. Technically, `define_static_object(v)` can also be achieved via `define_static_array(views::single(v)).data()`, but it's has its own use as we'll show.

Technically, `define_static_array` can be used to implement `define_static_string`:

::: std
```cpp
consteval auto define_static_string(string_view str) -> char const* {
  return define_static_array(views::concat(str, views::single('\0'))).data();
}
```
:::

But that's a fairly awkward implementation, and the string use-case is sufficiently common as to merit a more ergonomic solution.

There are three design questions that we have to address: whether objects can overlap, whether `define_static_array` needs to mandate structural, and what the return type should be.

## The Overlapping Question

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

However, that's not the right way to think about overlapping.

A more accurate way to present the ability to support overlapping arrays from `define_static_string` would be that the two TUs would merge more like this:

<table>
<tr><th>TU #1</th><th>TU #2</th></tr>
<tr><td colspan="2">
```cpp
// all our static strings merged
inline constexpr char const __arr_dedup[] = "dedup";
inline constexpr char const __arr_holdup[] = "holdup";

// this behaves like an array for all purposes, including that
// __arr_dup[-1] is not a valid constant expression (because out of bounds)
// but the implementation is allowed to have &__arr_dup[0] == &__arr_dedup[2]
inline constexpr char const __arr_dup[] = "dup";
```
</td>
</tr>
<tr><td>
```cpp
// C<define_static_string("dedup")>
C<__arr_dedup> c1;

// C<define_static_string("dup")>
C<__arr_dup> c2;
```
</td>
<td>
```cpp
// C<define_static_string("holdup")>
C<__arr_holdup> c3;

// C<define_static_string("dup")>
C<__arr_dup> c4;
```
</td>
</tr>
</table>

At this point, the usual template-argument-equivalence rules apply, so `c4` and `c2` would definitely have the same type, because their template arguments point to the same array. As desired.

The one thing we really have to ensure with this route, as pointed out by Tomasz Kamiński, is that comparison between distinct non-unique objects needs to be unspecified. This is so that you cannot ensure overlap. In other words:

::: std
```cpp
constexpr char const* a = define_static_string("dedup");
constexpr char const* b = define_static_string("dup");

static_assert(b == b);                               // ok, #1
static_assert(b + 1 == b + 1);                       // ok, #2
static_assert(a != b);                               // ok, #3
static_assert(a + 2 != b);                           // error: unspecified
static_assert(string_view(a + 2) == string_view(b)); // ok, #4
```
:::

Now, it had better be the case that `b == b` and `b + 1 == b + 1` are both valid checks. It would be fairly strange otherwise. Similarly, the goal is to not be able to observe whether `a + 2 == b`. It could be `true` at runtime. Or not. We have no idea at compile-time yet, so it'd be better to just not even allow an answer.

The interesting one is `a != b`. We could say that the comparison is unspecified (because they're pointers into distinct non-unique objects). But in this case, regardless of whether `a` and `b` overlap, `a != b` is _definitely_ going to be `true` at runtime. So we should only make unspecified the case that we actually cannot specify. After all, it would be strange if `a == b` were unspecified but `a[0] == b[0]` was `false`.

Note that, regardless, the `string_view` comparison is valid, since that is comparing the contents.

This does present an interesting situation where `a == b` could be invalid but `is_same_v<C<a>, C<b>>` would be valid.

### Update from CWG

[@CWG2765] now provides us a good solution for this problem. The wording (as of March 2025) introduces the idea that a pointer value pointing to a potentially non-unique object is associated with an evaluation — and that equality between pointers associated with different evaluations is non-constant.

We could simply add ourselves to the list of cases in that wording:

::: std
[?]{.pnum} A pointer value pointing to a potentially non-unique object `$O$` ([intro.object]) is *associated with* the evaluation of the `$string-literal$` ([lex.string])[,]{.addu} [or]{.rm} initializer list ([dcl.init.list])[, or a call to either `std::define_static_string` or `std::define_static_array` ([???])]{.addu} that resulted in the string literal object or backing array, respectively, that is `$O$` or of which `$O$` is a subobject. [A pointer value obtained by pointer arithmetic ([expr.add]) from a pointer value associated with an evaluation `$E$` is also associated with `$E$`.]{.note}
:::

This would give us exactly the behavior stipulated above. `b == b` is associated with the same evaluation, so that's fine. But comparing `a + 2` to `b` would run afoul of the new rule that we're not allowed to evaluate

::: std
* [25.?]{.pnum} an equality operator comparing pointers to potentially non-unique objects, if the pointer values of both operands are associated with different evaluations ([basic.compound]) and they can both point to the same offset within the same potentially non-unique object
:::

Because `a + 2` _could be_ `b`, it's non-constant. Problem solved.

## The Structural Question

For `define_static_string`, we have it easy because we know that `char` and `char8_t` are both structural types. But for `define_static_array`, we get an arbitrary `T`. How can we produce overlapping arrays in this case? If `T` is structural, we can easily ensure that equal invocations produce the _same_ `span` result.

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

None of these options is particularly appealing. The last prevents some very motivating use-cases since neither `span` nor `string_view` are structural types yet, which means you cannot reify a `vector<string>` into a `span<string_view>`, but hopefully that can be resolved soon ([@P3380R1]). You can at least reify it into a `span<char const*>`?

For now, this paper proposes the last option, as it's the simplest (and the relative cost will hopefully decrease over time). Allowing the call but rejecting use as non-type template parameters is appealing though.

## Return Type and Layering

[This section was migrated from [@P3617R0]]{.ednote}

`define_static_array` returning a `span<T const>` and `define_static_string` returning a `char const*` is useful, but sometimes it's insufficient. Matthias Wippich sent us some examples of such cases. Consider C++20 code such as:

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

However, some situations require _all_ the members at once. Such as in this [struct of arrays](https://brevzin.github.io/c++/2025/05/02/soa/) implementation. `define_static_array` simply returns a `span`, but if we had a function that returned a reflection of an array, then combining [@P1061R10]{.title} with [@P2686R5]{.title} lets us do this instead:

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

## Possible Implementation

`define_static_string` can be nearly implemented with the facilities in [@P2996R7], we just need `is_string_literal` to handle the different signature proposed in this paper.

 `define_static_array` is similar and simpler, so we'll present that here:

::: std
```cpp
namespace meta {
    template <typename T, T... Vs>
    inline constexpr T __fixed_array[sizeof...(Vs)]{Vs...};

    template <ranges::input_range R>
    consteval auto reflect_constant_array(R&& r) -> info {
        auto args = vector<info>{^^ranges::range_value_t<R>};
        for (auto&& elem : r) {
            args.push_back(reflect_constant(elem));
        }
        return substitute(^^__fixed_array, args);
    }

    template <ranges::input_range R>
    consteval auto reflect_constant_string(R&& r) -> info {
        using T = ranges::range_value_t<R>;
        static_assert(std::same_as<T, char> or std::same_as<T, char8_t>);

        auto args = vector<info>{^^ranges::range_value_t<R>};
        for (auto&& elem : r) {
            args.push_back(reflect_constant(elem));
        }

        // add a the null terminator, unless we're a string literal
        bool const add_null = [&]{
            if constexpr (requires { is_string_literal(r); }) {
                if (is_string_literal(r)) {
                    return false;
                }
            }
            return true;
        }();

        if (add_null) {
            args.push_back(reflect_constant(T()));
        }
        return substitute(^^__fixed_array, args);
    }

}

template <ranges::input_range R>
consteval auto define_static_string(R&& r)
    -> ranges::range_value_t<R> const*
{
    using T = ranges::range_value_t<R>;
    static_assert(std::same_as<T, char> or std::same_as<T, char8_t>);

    // produce the array
    auto array = meta::reflect_constant_string(r);

    // extract the pointer
    return extract<T const*>(array);
}

template <ranges::input_range R>
consteval auto define_static_array(R&& r)
    -> span<ranges::range_value_t<R> const>
{
    using T = ranges::range_value_t<R>;

    // produce the array
    auto array = meta::reflect_constant_array(r);

    // turn the array into a span
    return span<T const>(extract<T const*>(array), extent(type_of(array)));
}
```
:::

[Demo](https://compiler-explorer.com/z/jThqbcEMj) (note that the p2996 fork already has these functions, so they're named slightly differently in the link)

Note that this implementation gives the guarantee we talked about in the [previous section](#the-overlapping-question). Two invocations of `define_static_string` with the same contents will both end up returning a pointer into the same specialization of the (extern linkage) variable template `__array<V>`. We rely on the mangling of `V` (and `std::array` is a structural type if `T` is, which `char` and `char8_t` are) to ensure this for us. This won't ever produce overlapping arrays, would need implementation help for that, but it is a viable solution for all use-cases.

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

Something like this — [@P1306R4]{.title} — is not doable without non-transient constexpr allocation :

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

And the lower layer would let us [expand the whole pack](https://compiler-explorer.com/z/sGbf8Y3vT) in one go:

::: std
```cpp
consteval void g() {
    constexpr auto [...m] = [: reflect_constant_array(f()) :];
    static_assert((... + m) == 6);
}
```
:::

### Implementing `source_location`

One interesting use of a specific `define_static_object` (for the single object case), courtesy of Richard Smith, is to implement the single-pointer optimization for `std::source_location` without compiler support:

::: std
```cpp
class source_location {
    struct impl {
        char const* filename;
        int line;
    };
    impl const* p_;

public:
    static consteval auto current(char const* file = __builtin_FILE(),
                                  int line = __builtin_LINE()) noexcept
        -> source_location
    {
        // first, we canonicalize the file
        impl data = {.filename = define_static_string(file), .line = line};

        // then we canonicalize the data
        impl const* p = define_static_object(data);

        // and now we have an external linkage object mangled with this location
        return source_location{p};
    }
};
```
:::

## Related Papers in the Space

A number of other papers have been brought up as being related to this problem, so let's just enumerate them.

* [@P3094R5]{.title} proposed `std::basic_fixed_string<char, N>`. It exists to solve the problem that `C<"hello">` needs support right now. Nothing in this paper would make `C<"hello">` work, although it might affect the way that you would implement the type that makes it work.
* [@P3380R1]{.title} proposes to extend non-type template parameter support, which could eventually make `std::string` usable as a non-type template parameter. But without non-transient constexpr allocation, this doesn't obviate the need for this paper. Note that that paper even depends on this paper for how to normalize string literals, making string literals usable as non-type template arguments.
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


# Wording

Change [intro.object]{.sref} to add the results of `reflect_constant_array` and `reflect_constant_string` as being potentially non-unique:

::: std
[9]{.pnum} An object is a *potentially non-unique object* if it is

* [9.1]{.pnum} a string literal object ([lex.string]),
* [9.2]{.pnum} the backing array of an initializer list ([dcl.init.ref]),
* [9.3]{.pnum} [the object declared by a call to `std::meta::reflect_constant_array` or `std::meta::reflect_constant_string`]{.addu}, or
* [9.4]{.pnum} a subobject thereof.
:::

Update the wording in [@CWG2765] to account for these as well, in [basic.compound]{.sref}/x:

::: std
[?]{.pnum} A pointer value pointing to a potentially non-unique object `$O$` ([intro.object]) is *associated with* the evaluation of the `$string-literal$` ([lex.string])[,]{.addu} [or]{.rm} initializer list ([dcl.init.list])[, or a call to either `std::meta::reflect_constant_string` or `std::meta::reflect_constant_array` ([meta.reflection.array])]{.addu} that resulted in the string literal object or backing array, respectively, that is `$O$` or of which `$O$` is a subobject. [A pointer value obtained by pointer arithmetic ([expr.add]) from a pointer value associated with an evaluation `$E$` is also associated with `$E$`.]{.note}
:::

No change to [expr.eq]{.sref} is necessary, [@CWG2765] takes care of it.

Add to [meta.syn]{.sref}:

::: std
```diff
#include <initializer_list>

- namespace std::meta {
+ namespace std {
+   // [meta.string.literal], checking string literals
+   consteval bool is_string_literal(const char* p);
+   consteval bool is_string_literal(const wchar_t* p);
+   consteval bool is_string_literal(const char8_t* p);
+   consteval bool is_string_literal(const char16_t* p);
+   consteval bool is_string_literal(const char32_t* p);

+   // [meta.define.static], promoting to runtime storage
+   template <ranges::input_range R>
+     consteval const ranges::range_value_t<R>* define_static_string(R&& r);
+
+    template <ranges::input_range R>
+      consteval span<const ranges::range_value_t<R>> define_static_array(R&& r);
+
+   template <class T>
+     consteval const remove_cvref_t<T>* define_static_object(T&& r);
+
+ namespace meta {
    using info = decltype(^^::);
    // ...

    // [meta.reflection.result], expression result reflection
    template<class T>
      consteval info reflect_value(const T& value);
    template<class T>
      consteval info reflect_object(T& object);
    template<class T>
      consteval info reflect_function(T& fn);

+   // [meta.reflection.array], promoting to runtime storage
+   template <ranges::input_range R>
+   consteval info reflect_constant_string(R&& r);
+
+  template <ranges::input_range R>
+    consteval info reflect_constant_array(R&& r);

    // ...
+ }
  }
```
:::

Add the new subclause [meta.reflection.array]:

::: std
::: addu
[1]{.pnum} The functions in this subclause are useful for promoting compile-time storage into runtime storage.

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

[#]{.pnum} *Mandates*: `$T$` is a structural type ([temp.param]), `is_constructible_v<$T$, ranges::range_reference_t<R>>` is `true`, and `is_copy_constructible_v<$T$>` is `true`.

[#]{.pnum} Let `$V$` be the pack of elements of type `$T$` constructed from the elements of `r`.

[#]{.pnum} Let `$P$` be an invented variable that would be introduced by the declaration

```cpp
const $T$ $P$[sizeof...($V$)]{$V$...};
```

[#]{.pnum} *Returns*: A reflection of the template parameter object that is template-argument-equivalent to the object denoted by `$P$` ([temp.param]).

[#]{.pnum} [That template parameter object is a potentially non-unique object ([intro.object])]{.note}
:::
:::

Add to the new subclause [meta.string.literal]:

::: std
::: addu
```cpp
consteval bool is_string_literal(const char* p);
consteval bool is_string_literal(const wchar_t* p);
consteval bool is_string_literal(const char8_t* p);
consteval bool is_string_literal(const char16_t* p);
consteval bool is_string_literal(const char32_t* p);
```

[1]{.pnum} *Returns*: If `p` points to a string literal or a subobject thereof, `true`. Otherwise, `false`.

:::
:::

Add to the new subclause [meta.define.static]

::: std
::: addu

[1]{.pnum} The functions in this clause are useful for promoting compile-time storage into runtime storage.

```cpp
template <ranges::input_range R>
consteval const ranges::range_value_t<R>* define_static_string(R&& r);
```

[#]{.pnum} *Effects*: Equivalent to:

```cpp
return extract<const ranges::range_value_t<R>*>(meta::reflect_constant_string(r));
```

```cpp
template <ranges::input_range R>
consteval span<const ranges::range_value_t<R>> define_static_array(R&& r);
```

[#]{.pnum} *Effects*: Equivalent to:

  ```cpp
  using T = ranges::range_value_t<R>;
  meta::info array = meta::reflect_constant_array(r);
  return span<const T>(extract<const T*>(array), extent(type_of(array)));
  ```

```cpp
template <class T>
consteval const remove_cvref_t<T>* define_static_object(T&& t);
```

[#]{.pnum} *Effects*: Equivalent to:

```cpp
return define_static_array(span(std::addressof(t), 1)).data();
```

:::
:::

## Feature-Test Macro

Add to [version.syn]{.sref}:

::: bq
::: addu
```
#define __cpp_lib_define_static 2025XX // freestanding, also in <meta>
```
:::
:::

---
references:
  - id: P3380R1
    citation-label: P3380R1
    title: "Extending support for class types as non-type template parameters"
    author:
      - family: Barry Revzin
    issued:
      - year: 2024
        month: 12
        day: 4
    URL: https://wg21.link/p3380r1
  - id: CWG2765
    citation-label: CWG2765
    title: "Address comparisons between potentially non-unique objects during constant evaluation"
    author:
      - family: CWG
    issued:
      - year: 2023
        month: 07
        day: 14
    URL: https://cplusplus.github.io/CWG/issues/2765.html
---
