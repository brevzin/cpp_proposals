---
title: "Revert string support in `std::constant_wrapper`"
document: P4206R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Zach Laine
      email: <whatwasthataddress@gmail.com>
    - name: Matthias Kretz
      email: <m.kretz@gsi.de>
    - name: Jonathan Wakely
      email: <cxx@kayari.org>
toc: true
status: progress
---

# Introduction

Originally, the design of `std::constant_wrapper` [@P2781R4] was :

::: std
```cpp
template <auto X, class = remove_cvref_t<decltype(X)>>
struct constant_wrapper {
    // operators and stuff
};

template <auto X>
constexpr auto cw = constant_wrapper<X>();
```
:::

In [@P2781R5] of the paper, the shape changed significantly in order to support strings, and was eventually adopted in this form:

::: std
```cpp
template <typename T>
  struct $cw-fixed-value$; // exposition-only

template <$cw-fixed-value$ X, class = typename decltype(X)::type>
struct constant_wrapper {
    // operators and stuff
};

template <$cw-fixed-value$ X>
constexpr auto cw = constant_wrapper<X>();
```
:::

We think that this change harms the usability of `std::constant_wrapper` for little benefit. The string support isn't much in the way of support, and can be achieved with future language improvements — but if we keep the current shape, the usability is bad forever. We should revert it.

# Usage of `constant_wrapper`

Generally speaking, `constant_wrapper` has two broad, overlapping uses:

1. just wrapping a constant in a way that's preserved through layers of function calls, such that the eventual receiver can deduce a value and make use of it.
2. as a DSL where the operators preserve constant-ness, e.g. `cw<5> + cw<3>` yields `cw<8>` rather than simply `8`.

This paper is going to focus on the first one.

A fairly typical use of constant wrapping would be:

::: std
```cpp
template <int I>
auto f(std::constant_wrapper<I>) -> void {
    // ...
}

auto main() -> int {
    f(std::cw<5>);
}
```
:::

But, as /u/Massive-Bottle-5394 pointed out on [r/cpp](https://old.reddit.com/r/cpp/comments/1s92kq3/stdconstant_wrapper_acts_unexpectedly/) soon after the Croydon meeting, this actually _does not work_ with `std::constant_wrapper`. The error you get is:

::: std
```
<source>:9:6: error: no matching function for call to 'f(const std::constant_wrapper<std::_CwFixedValue<int>{5}, int>&)'
    9 |     f(std::cw<5>); // error
      |     ~^~~~~~~~~~~~
  • there is 1 candidate
    • candidate 1: 'template<int I> void f(std::constant_wrapper<((std::_CwFixedValue<int>)I)>)'
      <source>:4:6:
          4 | auto f(std::constant_wrapper<I>) -> void {
            |      ^
      • template argument deduction/substitution failed:
        •   mismatched types 'int' and 'const std::_CwFixedValue<int>'
          <source>:9:6:
              9 |     f(std::cw<5>); // error
                |     ~^~~~~~~~~~~~
```
:::

It is fairly surprising that this doesn't work. A fairly typical way of providing a simple constant wrapper since C++17 is:

::: std
```cpp
template <auto V>
using constant = integral_constant<decltype(V), V>;
```
:::

For instance, Boost.Mp11's [mp_value](https://github.com/boostorg/mp11/blob/48019a04608c09f09f5baf4b63133f8c54df3758/include/boost/mp11/detail/mp_value.hpp#L18) is implemented like this. And the above example _does_ work with this formulation.

But it doesn't work with C++26's `std::constant_wrapper` because `std::cw<5>` does not give you a `constant_wrapper<int(5)>`, it yields a `constant_wrapper<cw_fixed_value<int>(5)>`. And `cw_fixed_value<int>(5)` is not some kind of `int`, hence deduction fails.

You could instead do this:

::: std
```cpp
template <auto I>
auto f(std::constant_wrapper<I>) -> void {
    // ...
}

auto main() -> int {
    f(std::cw<5>); // ok
}
```
:::

But now, what can you do with `I`? It's a value of an exposition-only type. It's not an `int` or convertible to one. The ways to get at that `int` are to go through the wrapper:

::: std
```cpp
template <auto I>
auto f(std::constant_wrapper<I> c) -> void {
    constexpr int A = I;                // error
    constexpr int B = std::cw<I>.value; // ok
    constexpr int C = c.value;          // ok
    constexpr int D = c;                // ok
}
```
:::

This means that the template parameter itself is somewhat useless, you might as well just use `c`.

Now, the original formulation also _required_ that the constant be an `int`. How do we do that with `std::constant_wrapper`? The easiest way is recognizing that there actually is a second template parameter:

::: std
```cpp
template <auto I>
auto f(std::constant_wrapper<I, int>) -> void {
    // ...
}

auto main() -> int {
    f(std::cw<1>);  // ok
    f(std::cw<2u>); // error
}
```
:::

Which is also somewhat surprising, since that template parameter isn't really there to be user facing, it's just an implementation detail to get argument-dependent lookup to work.

One consequence of this implementation detail that the name and type of the implementation detail are exposed in compiler diagnostics, but another is increased symbol sizes. For instance:

::: std
```cpp
auto f(auto x) -> void;

auto g() -> void {
    f(std::cw<1>);
}
```
:::

The mangled name of the function being invoked here is:

|Revision|Mangled|Demangled|
|-|-|-|
|R4|`_Z1fISt16constant_wrapperILi1EiEEvT_`|`void f<std::constant_wrapper<1, int> >(std::constant_wrapper<1, int>)`|
|R5|`_Z1fISt16constant_wrapperIXtlSt13_CwFixedValueIiELi1EEEiEEvT_`|`void f<std::constant_wrapper<std::_CwFixedValue<int>{1}, int> >(std::constant_wrapper<std::_CwFixedValue<int>{1}, int>)`|

That's a change from 36 characters to 61.

Another issue is the question of address uniqueness. Consider:

::: std
```cpp
template <auto V> auto foo() -> void const* { return &V; }
template <auto V> auto bar(std::constant_wrapper<V> cw) -> void const& { return &cw.value; }
```
:::

For an object `o` of class type, `foo<o>()` and `bar(std::cw<o>)` do not return the same address — because the former returns the address of the template parameter object, but the latter returns the address of a subobject of a template parameter object of different kind.

# String Support

This raises the question: what benefit do we get from this less convenient interface? The stated motivation for the change was to support strings. What does that support look like?

In the original design, `std::cw<"foo">` was ill-formed because we're not allowed to pass a pointer to a string literal as a constant template argument. Now, it is valid — by virtue of wrapping the string literal in an array. And the other DSL stuff just works too, so `std::cw<"foo">[0]` yields `std::cw<'f'>`. And these are null terminated, so `std::cw<"foo">[3]` yields `std::cw<'\0'>`.

But, outside of being able to write `std::cw<"foo">`, what string operations can you do?

You cannot do comparisons:

::: std
```cpp
auto a = std::cw<"foo">;
auto b = std::cw<"bar">;
a == b; // error
```
:::

You cannot use non-literal strings:

::: std
```cpp
constexpr char const* msg = "amount";
auto c = std::cw<msg>; // error
```
:::

And on the receiver side, as in the previous section, you can't do anything with the template argument until you wrap it locally:

::: std
```cpp
template <auto V, size_t N>
auto f(std::constant_wrapper<V, char const[N]>) -> void {
    constexpr auto S = std::string_view(std::cw<V>.value);
}

auto main() -> int {
    f(std::cw<"amount">);
}
```
:::

Now, it is true that string support in C++26 template arguments is still weak. `std::string` doesn't work because we don't have non-transient constexpr allocation, `std::string_view` isn't structural, so users have to write their own fixed-length string literal types — like the `strlit<N>` illustrated in [R4](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2781r4.html).

But such a thing actually gives meaningful string support, with some additional work:

::: std
```cpp
template <strlit S>
struct string_constant : constant_wrapper<S> {
    // possibly some string specific operations here
    // like a conversion to string_view.
    // but even if string_constant<S> were an alias
    // to constant_wrapper<S>, that's a big improvement
};

template <strlit S>
inline constexpr auto cstr = string_constant<S>();
```
:::

For example, [here](https://github.com/mattkretz/vir-reflect-light/blob/main/vir/fixed_string.h) those types are named `fixed_string` and `constexpr_string`.


This requires explicit typing on the user side, but has the added benefit that more string operations work directly:

::: std
```cpp
static_assert(cstr<"bob"> == cstr<"bob">); // ok

template <strlit S>
auto f(string_constant<S>) -> void {
    if constexpr (S == "alice") {
        // ...
    } else if constexpr (S == "bob") {
        // ...
    }
}
```
:::

# Future Language Evolution

We made `std::cw<"eve">` work, at the cost of making the whole type less convenient to use properly, to work around a language limitation. But it's worth considering what we could do in the language to solve this problem instead and how likely those changes are:

* [@P0424R2]{.title} proposed making string literals work as constant template arguments by way of implicitly transforming them into static storage duration arrays. [@P3380R1]{.title} picked up that idea and proposes the same. This would make `std::cw<"eve">` just work directly.
* Extending support for structural types in general ([@P3380R1]) would allow us to make `std::cw<"eve"sv>` also work.
* Solving non-transient allocation would even allow us to make `std::cw<"eve"s>` work.

If we get these language changes, will we regret having made the array change to `std::constant_wrapper`? Absolutely. The lack of string support is temporary, and isn't really solved by the workaround in `std::constant_wrapper` anyway, but the API of `std::constant_wrapper` is permanent.

Moreover, removing the string literal support from `std::constant_wrapper` doesn't even prevent using it with strings in the present day. With a suitable `strlit<N>`/`fixed_string<N>` type (or a dynamically typed solution using `std::define_static_string`), users can add a `cstr<"alice">` or `cw_str<"bob">` or however they want to spell it, and get all the functionality with minor cost.

# Proposal

As a DR against C++26: Revert the `std::constant_wrapper` and `std::cw` design to the R4 shape, where both templates just take an `auto` parameter, and remove `std::$cw-fixed-value$`.

## Implementation Status

Both libstdc++ and libc++ have already implemented the `std::constant_wrapper` design that we shipped in C++26, and will ship a release with that utility. Nevertheless, both implementations support making the change proposed here as a C++26 DR.

## Design Note

The status quo is that we have:

::: std
```cpp
template <$cw-fixed-value$ X>
struct constant_wrapper {
    static constexpr const auto& value = X.data;
};
```
:::

In this scenario, `X.data` is always an lvalue, so `constant_wrapper<V>::value` is always a reference to a static storage duration object. Specifically, it is not a reference to a temporary object. However, if we change the design to:

::: std
```cpp
template <auto X>
struct constant_wrapper {
    static constexpr const auto& value = X;
};
```
:::

Then for scalar types, `X` is a prvalue, which means that `value` is a reference to lifetime-extended temporary. The consequence of that would be that `constant_wrapper<42>::value` is not usable as a constant template argument for a template parameter of reference type:

::: std
```cpp
template <auto X>
struct constant_wrapper {
    static constexpr const auto& value = X;
};

template <int const& R>
auto f() -> int;

int r = f<constant_wrapper<42>::value>(); // error
```
:::

That would be pretty surprising also, so instead we specify it this way:

::: std
```cpp
template <auto X>
struct constant_wrapper {
    static constexpr decltype(auto) value = (X);
};
```
:::

When `X` is a class type, `value` is the same lvalue reference to constant object that it was before. But for prvalues, `value` is now simply a value, so everything continues to work.

## Wording

Change [utility.syn]{.sref}:

::: std
```diff
namespace std {
  // [const.wrap.class], class template constant_wrapper
- template<class T>
-   struct $cw-fixed-value$;              // exposition only

- template<$cw-fixed-value$ X, class = typename decltype(X)::type>
+ template<auto X, class = decltype(X)>
    struct constant_wrapper;

  template<class T>
    concept $constexpr-param$ =           // exposition only
      requires { typename constant_wrapper<T::value>; };

  struct $cw-operators$;                  // exposition only

- template<$cw-fixed-value$ X>
+ template<auto X>
    constexpr auto cw = constant_wrapper<X>{};
}
```
:::

Change [const.wrap.class]{.sref}.

::: std
```diff
namespace std {
- template<class T>
- struct $cw-fixed-value$ {                                               // exposition only
-   using type = T;                                                     // exposition only
-   constexpr $cw-fixed-value$(type v) noexcept : data(v) {}
-   T data;                                                             // exposition only
- };
-- template<class T, size_t Extent>
- struct $cw-fixed-value$<T[Extent]> {                                    // exposition only
-   using type = T[Extent];                                             // exposition only
-   constexpr $cw-fixed-value$(T (&arr)[Extent]) noexcept;
-   T data[Extent];                                                     // exposition only
- };
-- template<class T, size_t Extent>
-   $cw-fixed-value$(T (&)[Extent]) -> $cw-fixed-value$<T[Extent]>;         // exposition only

  struct $cw-operators$ {                                                 // exposition only
    // ...
  };

- template<$cw-fixed-value$ X, class>
+ template<auto X, class T>
  struct constant_wrapper : $cw-operators$ {
-   static constexpr const auto & value = X.data;
+   static constexpr decltype(auto) value = (X);
    using type = constant_wrapper;
-   using value_type = decltype(X)::type;
+   using value_type = decltype(X);

    template<$constexpr-param$ R>
      constexpr auto operator=(R) const noexcept
        -> constant_wrapper<(value = R::value)> { return {}; }

    constexpr operator decltype(value)() const noexcept { return value; }

    template<class... Args>
      static constexpr decltype(auto) operator()(Args&&... args) noexcept($see below$);
    template<class... Args>
      static constexpr decltype(auto) operator[](Args&&... args) noexcept($see below$);
  };
}
```
:::

And remove the constructor in [const.wrap.class]{.sref}/4 and replace it with a Mandates that ensures that you can't do `constant_wrapper<42, float>()`.

::: std
::: rm
```
constexpr $cw-fixed-value$(T (&arr)[Extent]) noexcept;
```

[4]{.pnum} *Effects*: Initialize elements of `data` with corresponding elements of `arr`.
:::

::: addu
[4]{.pnum} *Mandates*: `is_same_v<T, value_type>` is `true`.
:::
```cpp
template<class... Args>
  static constexpr decltype(auto) operator()(Args&&... args) noexcept($see below$);
```
[5]{.pnum} Let `$call-expr$` be [...].
:::

## Feature-Test Macro

Bump the value of `__cpp_lib_constant_wrapper` in [version.syn]{.sref}:

::: std
```diff
- #define __cpp_lib_constant_wrapper 202603L // freestanding, also in <utility>
+ #define __cpp_lib_constant_wrapper 2026XXL // freestanding, also in <utility>
```
:::