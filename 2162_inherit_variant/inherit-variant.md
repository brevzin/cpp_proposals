---
pagetitle: "Inheriting from std::variant"
title: "Inheriting from `std::variant`"
subtitle: Resolving LWG3052
document: D2162R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

[@LWG3052] describes an under-specification to `std::visit`:

>  the _Requires_ element imposes no explicit requirements on the types in `Variant`s. Notably, the `Variant`s are not required to be `variant`s. This lack of constraints appears to be simply an oversight. 

The original proposal [@P0088R3] makes no mention of other kinds of of variants besides `std::variant`, and this does not appear to have been discussed in LEWG. 

The proposed resolution in the library issue is to make `std::visit` only work if all of the `Variant`s are, in fact, `std::variant`s:

> _Remarks_: This function shall not participate in overload resolution unless `remove_cvref_t<Variants@~i~@>` is a specialization of `variant` for all `0 <= i < n`.

This paper suggests a different direction. Instead of restricting to _just_ `std::variant` (and certainly not wanting to go all out and design a "variant-like" interface), this paper proposes to allow an additional category of useful types to be `std::visit()`-ed: those that publicly and unambiguously inherit from a specialization of `std::variant`.

Notably, the MSVC implementation already supports this design. It has been shipping exactly this behavior since the first Visual Studio 2019 release in April 2019.

# Inheriting from `variant`

There are two primary motivators for inheriting from `std::variant`.

One is to simply extend functionality. If we're using `variant` to represent a state machine, we may want additional operations that are relevant to our state that `variant` doesn't itself provide:

```cpp
struct State : variant<Disconnected, Connecting, Connected>
{
    using variant::variant;
    
    bool is_connected() const {
        return std::holds_alternative<Connected>(*this);
    }
    
    friend std::ostream& operator<<(std::ostream&, State const&) {
        // ...
    }
};
```

Another may be to create a recursive variant, as in the example from [@P1371R2]:

```cpp
struct Expr;

struct Neg {
    std::shared_ptr<Expr> expr;
};

struct Add {
    std::shared_ptr<Expr> lhs, rhs;
};

struct Mul {
    std::shared_ptr<Expr> lhs, rhs;
};

struct Expr : std::variant<int, Neg, Add, Mul> {
    using variant::variant;
};

namespace std {
    template <> struct variant_size<Expr> : variant_size<Expr::variant> {};
    
    template <std::size_t I> struct variant_alternative<I, Expr> : variant_alternative<I, Expr::variant> {};
}
```

That paper even has an example of passing an `Expr` to `std::visit()` directly, a use-case that this paper is seeking to properly specify. It would be pretty
nice if that just worked.

Note also that the example includes an explicit specialization of `variant_size` and `variant_alternative` that just forward along to `Expr`'s base class. These specializations are pure boilerplate - they basically have to look the way they do, so they don't really offer much in the way of adding value to the program.

# Implementation Approach

The proposed resolution of LWG3052 is to, basically, add this constraint onto
`std::visit`:

```cpp
template <typename Visitor, typename... Variants>
    requires (is_specialization_of_v<remove_cvref_t<Variants>, variant> && ...)
constexpr decltype(auto) visit(Visitor&&, Variants&&) {
    // as today
}
```

This paper proposes instead that `visit` conditionally upcasts all of its incoming variants to `std::variant` specializations:

```cpp
template <typename Visitor, typename... Variants>
    requires (is_specialization_of_v<remove_cvref_t<Variants>, variant> && ...)
constexpr decltype(auto) visit(Visitor&&, Variants&&) {
    // as today
}

template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...>& v) -> std::variant<Ts...>& {
    return v;
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...> const& v) -> std::variant<Ts...> const& {
    return v;
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...>&& v) -> std::variant<Ts...>&& {
    return std::move(v);
}
template <typename... Ts>
constexpr auto variant_cast(std::variant<Ts...> const&& v) -> std::variant<Ts...> const&& {
    return std::move(v);
}

template <typename Visitor, typename... Variants>
constexpr decltype(auto) visit(Visitor&& vis, Variants&&... vars) {
    return visit(std::forward<Visitor>(vis),
        variant_cast(std::forward<Variants>(vars))...);
}
```

This means the body of `std::visit` for implementations can remain unchanged -
it, as today, can just assume that all the variants are indeed `std::variant`s. Such an implementation would allow visitation of the `State` and `Expr` examples provided earlier.

Now, this means we can `std::visit` a variant that we can't even invoke `std::get` or `std::get_if` on, such as with this delightful type courtesy of Tim Song:

```cpp
struct MyEvilVariantBase {
    int index;
    char valueless_by_exception;
};

struct MyEvilVariant : std::variant<int, long>, std::tuple<int>, MyEvilVariantBase { };
```

But... who cares. Don't write types like that.

# Wording

Change [variant.visit]{.sref}:

::: bq
```diff
template<class Visitor, class... Variants>
  constexpr see below visit(Visitor&& vis, Variants&&... vars);
template<class R, class Visitor, class... Variants>
  constexpr R visit(Visitor&& vis, Variants&&... vars);
```

::: addu
[-2]{.pnum} Let _`as-variant`_ denote the exposition-only function template


```cpp
template<class... Ts>
const variant<Ts...>& @_as-variant_@(const variant<Ts...>& var) { return var; }
```


Let `n` be `sizeof...(Variants)`. For each `0 <= i < n`, let `V@~i~@` denote the
the type `remove_cvref_t<decltype(@_as-variant_@(vars@~i~@))>`.

[-1]{.pnum} _Constraints_: `V@~i~@` is a valid type for all `0 <= i < n`.

[0]{.pnum} Let `VR@~i~@` denote the type `V@~i~@` with the addition of `Variant@~i~@`'s cv and reference qualifiers. Let `VR` denote the pack of types `VR@~i~@`.
::: 

[1]{.pnum} [Let `n` be `sizeof...(Variants)`.]{.rm}
Let `m` be a pack of n values of type `size_t`.
Such a pack is called valid if `0 <= m@~i~@ < variant_size_v<@[remove_reference_t<Variants~i~>]{.rm} [V~i~]{.addu}@>` for all `0 <= i < n`.
For each valid pack `m`, let `e(m)` denote the expression:

```diff
- INVOKE(std::forward<Visitor>(vis), get<m>(std::forward<Variants>(vars))...) // see [func.require]
+ INVOKE(std::forward<Visitor>(vis), get<m>(std::forward<VR>(vars))...) // see [func.require]
```

for the first form and

```diff
- INVOKE<R>(std::forward<Visitor>(vis), get<m>(std::forward<Variants>(vars))...) // see [func.require]
+ INVOKE<R>(std::forward<Visitor>(vis), get<m>(std::forward<VR>(vars))...) // see [func.require]
```

for the second form.

[2]{.pnum} _Mandates_: For each valid pack `m`, `e(m)` is a valid expression.
All such expressions are of the same type and value category.

[3]{.pnum} _Returns_: `e(m)`, where `m` is the pack for which `m@~i~@` is [`vars@~i~@.index()`]{.rm} [`@_as-variant_@(vars@~i~@).index()`]{.addu} for all `0 <= i < n`. The return type is `decltype(e(m))` for the first form.

[4]{.pnum} _Throws_: `bad_variant_access` if [any `variant` in `vars` is `valueless_by_exception()`]{.rm} if [`(@_as-variant_@(vars).valueless_by_exception() || ...)` is `true`]{.addu}. 

[5]{.pnum} _Complexity_: For `n <= 1`, the invocation of the callable object is implemented in constant time, i.e., for `n=1`, it does not depend on the number of alternative types of [`Variants@~0~@`]{.rm} [`V@~0~@`]{.addu}.
For `n>1`, the invocation of the callable object has no complexity requirements.

:::

## Feature-test macro

This paper proposes to bump the value `__cpp_lib_variant`. The macro already exists, so this is, in a sense, free. And users can use the value of this macro to avoid having to specialize `variant_size` and `variant_alternative` for their inherited variants.

# Acknowledgments

Thanks to Casey Carter, Ville Voutilainen, and the unfortunately non-alliterative Tim Song for design discussion and help with the wording.

