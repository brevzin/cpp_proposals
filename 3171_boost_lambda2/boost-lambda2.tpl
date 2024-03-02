---
title: "Adding functionality to placeholder types"
document: P3171R0
date: today
audience: LEWG
author:
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>

toc: true
toc-depth: 4
---

# Introduction

As noted in [@P2760R0], there are a lot of function objects for operators in the standard library, but several operators are missing. This paper proposes to add the functionality for all the missing operators, but to also do it in a different way than simply by adding function objects.

[@Boost.Lambda2] is a Boost library (writen by Peter) which makes it possible to write very terse, simple operations, by building upon the `std::bind` machinery. When Barry was implementing `std::views::zip` [@P2321R2], a range adaptor whose implementation requires forwarding various operators across a `tuple`, Boost.Lambda2 provided a very nice way to implement those operations. Here is a comparison between a hand-written lambda solution, function objects, and the placeholder solution that Lambda2 offers:

<table>
<tr><th>Handwritten Lambdas</th><th>Named Function Objects</th><th>Boost.Lambda2</th></tr>
<tr>
<td>
```cpp
auto operator*() const {
  return tuple_transform($current_$,
    [](auto& it) -> decltype(auto) {
      return *it;
    });
}
```
</td>
<td>
```cpp
auto operator*() const {
  return tuple_transform($current_$,
    dereference{});
}
```
</td>
<td>
```cpp
auto operator*() const {
  return tuple_transform($current_$, *_1);
}
```
</td>
</tr>
<tr>
<td>
```cpp
auto operator++() -> iterator& {
  tuple_for_each($current_$,
    [](auto& it) { ++it; });
  return *this;
}
```
</td>
<td>
```cpp
auto operator++() -> iterator& {
  tuple_for_each($current_$,
    prefix_increment{});
  return *this;
}
```
</td>
<td>
```cpp
auto operator++() -> iterator& {
  tuple_for_each($current_$, ++_1);
  return *this;
}
```
</td>
</tr>
</table>

It's not just that the Lambda2 alternatives are overwhelmingly terser (it's very hard to beat 3 characters for the dereference operation, especially compared to the handwritten lambda that must use `-> decltype(auto)` and is thus 46 characters long), they more directly express exactly the work being done.

Lambda2 also offers a more expressive way of doing common predicates, even in the case where the named function object already exists. Let's take an example where you want to write a predicate for if the argument is negative (an example Barry previously wrote about on his blog [here](https://brevzin.github.io/c++/2020/06/18/lambda-lambda-lambda/)), there are several ways to do it:

::: bq
```cpp
// hand-written lambda (28 characters)
[](auto e) { return e < 0; }

// attempting to use std::less{} (19 characters, but... uh...)
bind(less{}, _1, 0)

// Boost.Lambda2 (6 characters)
_1 < 0
```
:::

It also allows for an approach to address the question of projections. Let's say that rather than finding a negative number, we want to find a `Point` whose `x` coordinate is negative:

::: bq
```cpp
// hand-written lambda (30 characters)
find_if(points, [](Point p){ return p.x < 0; })

// Boost.Lambda 2 (18 characters)
find_if(points, _1->*&Point::x < 0)
```
:::

Or if the `x` coordinate is 0:

::: bq
```cpp
// hand-written lambda (31 characters)
find_if(points, [](Point p){ return p.x == 0; })

// using projection (!2 characters, but cryptic)
find(points, 0, &Point::x);

// Boost.Lambda 2 (19 characters)
find_if(points, _1->*&Point::x == 0)
```
:::

Note that this latter usage could be improved significantly with something like [@P0060R0], which would actually allow for writing the predicate `_1.x == 0`. Which is difficult to beat.

You can see more examples in the [@Boost.Lambda2] docs.

# Proposal

We propose to solve the issue of missing operator function objects in the standard library, as well as less-than-ergonomic lambda syntax for common predicates, by standardizing Boost.Lambda2. That is not a large proposal. The standard library already provides placeholders, `std::namespace::_1` and friends. The standard library also already provides `std::bind`, which is already implemented in a way that supports composition of bind expressions. All we need to do is add operators.

We additionally add the missing operator function objects. Now, most of the missing operator function objects and placeholder operators are easy enough to add, except one: taking the address. `&_1` is a bit squeamish, although this actually seems like justified place to overload this operator. But as far as function objects go, the obvious name for it would be `std::addressof`. But this one already exists as an overloaded function template.

This leaves us with two options:

1. Respecify `std::addressof` as a customziation point object with the same behavior. This would be inconsistent with the other function objects (in all the other cases, we specify a type - `std::compare_three_way` is a type, the proposed `std::subscript` is a type, etc., whereas this would be the only object) and would break attempts to use `addressof` unqualified outside of `namespace std`.
2. Come up with a new name for the object, such that `std::addressof(x)` and maybe `std::address_of{}(x)` are equivalent.

We propose the former.

## Implementation Experience

Has been shipping in Boost since 1.77 (August 2021).

## Wording

Change [memory.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...
  // [specialized.addressof], addressof
- template<class T>
-   constexpr T* addressof(T& r) noexcept;                                          // freestanding
- template<class T>
-   const T* addressof(const T&&) = delete;                                         // freestanding
+ inline constexpr $unspecified$ addressof;                                           // freestanding
  // ...
}
```
:::

Change [specialized.addressof]{.sref}:

::: bq
::: addu
```
struct $addressof_t$ { // exposition-only
  template<class T>
    constexpr T* operator()(T& r) const noexcept;
  template<class T>
    constexpr T* operator()(const T&&) const = delete;
};

inline constexpr $addressof_t$ addressof{};
```

```
template<clsas T>
  constexpr T* $addressof_t$::operator()(T& r) const noexcept;
```
:::
::: rm
```
template<class T> constexpr T* addressof(T& r) noexcept;
```
:::
[1]{.pnum} *Returns*: The actual address of the object or function referenced by `r`, even in the presence of an overloaded `operator&`.

[#]{.pnum} *Remarks*: An expression `addressof(E)` is a constant subexpression ([defns.const.subexpr]) if `E` is an lvalue constant subexpression.
:::

Extend [functional.syn]{.sref} to add the additional function objects:

::: bq
```diff
namespace std {

  // ...

  // [bitwise.operations], bitwise operations
  template<class T = void> struct bit_and;                                          // freestanding
  template<class T = void> struct bit_or;                                           // freestanding
  template<class T = void> struct bit_xor;                                          // freestanding
  template<class T = void> struct bit_not;                                          // freestanding
  template<> struct bit_and<void>;                                                  // freestanding
  template<> struct bit_or<void>;                                                   // freestanding
  template<> struct bit_xor<void>;                                                  // freestanding
  template<> struct bit_not<void>;                                                  // freestanding

+ // [additional.operations], additional transparent operations
+ struct subscript;                                                                 // freestanding
+ struct left_shift;                                                                // freestanding
+ struct right_shift;                                                               // freestanding
+ struct unary_plus;                                                                // freestanding
+ struct dereference;                                                               // freestanding
+ struct increment;                                                                 // freestanding
+ struct decrement;                                                                 // freestanding
+ struct postfix_increment;                                                         // freestanding
+ struct postfix_decrement;                                                         // freestanding
+
+ // [compound.operations], compound assignment operations
+ struct plus_equal;                                                                // freestanding
+ struct minus_equal;                                                               // freestanding
+ struct multiplies_equal;                                                          // freestanding
+ struct divides_equal;                                                             // freestanding
+ struct modulus_equal;                                                             // freestanding
+ struct bit_and_equal;                                                             // freestanding
+ struct bit_or_equal;                                                              // freestanding
+ struct bit_xor_equal;                                                             // freestanding
+ struct left_shift_equal;                                                          // freestanding
+ struct right_shift_equal;                                                         // freestanding

  // ...

}
```
:::

Extend [functional.syn]{.sref} to add operators:

::: bq
```diff
namespace std {
  // ...
  namespace placeholders {
    // M is the implementation-defined number of placeholders
    $see below$ _1;                                                                   // freestanding
    $see below$ _2;                                                                   // freestanding
               .
               .
               .
    $see below$ _M;                                                                   // freestanding

+   template<class A, class B> constexpr auto operator+(A&&, B&&);
+   template<class A, class B> constexpr auto operator-(A&&, B&&);
+   template<class A, class B> constexpr auto operator*(A&&, B&&);
+   template<class A, class B> constexpr auto operator/(A&&, B&&);
+   template<class A, class B> constexpr auto operator%(A&&, B&&);
+   template<class A> constexpr auto operator-(A&&);
+
+   template<class A, class B> constexpr auto operator==(A&&, B&&);
+   template<class A, class B> constexpr auto operator!=(A&&, B&&);
+   template<class A, class B> constexpr auto operator<(A&&, B&&);
+   template<class A, class B> constexpr auto operator>(A&&, B&&);
+   template<class A, class B> constexpr auto operator<=(A&&, B&&);
+   template<class A, class B> constexpr auto operator>=(A&&, B&&);
+   template<class A, class B> constexpr auto operator<=>(A&&, B&&);
+
+   template<class A, class B> constexpr auto operator&&(A&&, B&&);
+   template<class A, class B> constexpr auto operator||(A&&, B&&);
+   template<class A> constexpr auto operator!(A&&);
+
+   template<class A, class B> constexpr auto operator&(A&&, B&&);
+   template<class A, class B> constexpr auto operator|(A&&, B&&);
+   template<class A, class B> constexpr auto operator^(A&&, B&&);
+   template<class A> constexpr auto operator~(A&&);
+
+   template<class A, class B> constexpr auto operator<<(A&&, B&&);
+   template<class A, class B> constexpr auto operator<<(A&, B&&);
+
+   template<class A, class B> constexpr auto operator>>(A&&, B&&);
+   template<class A, class B> constexpr auto operator>>(A&, B&&);
+
+   template<class A> constexpr auto operator+(A&&);
+   template<class A> constexpr auto operator*(A&&);
+   template<class A> constexpr auto operator++(A&&);
+   template<class A> constexpr auto operator--(A&&);
+   template<class A> constexpr auto operator++(A&&, int);
+   template<class A> constexpr auto operator--(A&&, int);
+
+   template<class A, class B> constexpr auto operator+=(A&&, B&&);
+   template<class A, class B> constexpr auto operator-=(A&&, B&&);
+   template<class A, class B> constexpr auto operator*=(A&&, B&&);
+   template<class A, class B> constexpr auto operator/=(A&&, B&&);
+   template<class A, class B> constexpr auto operator%=(A&&, B&&);
+   template<class A, class B> constexpr auto operator&=(A&&, B&&);
+   template<class A, class B> constexpr auto operator|=(A&&, B&&);
+   template<class A, class B> constexpr auto operator^=(A&&, B&&);
+   template<class A, class B> constexpr auto operator<<=(A&&, B&&);
+   template<class A, class B> constexpr auto operator>>=(A&&, B&&);
+
+   template<class A, class B> constexpr auto operator->*(A&&, B&&);
  }
  // ...
}
```
:::

Add two new sections after [bitwise.operations]{.sref}:

{% macro make_binary_func(name, op) %}
```
struct {{ name }} {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) {{op}} std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) {{op}} std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) {{op}} std::forward<U>(u)`.
{% endmacro %}

{% macro make_unary_func(name, op, is_prefix) %}
{% set prefix = op if is_prefix else "" %}
{% set postfix = "" if is_prefix else op %}
```
struct {{ name }} {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype({{prefix}}std::forward<T>(t){{postfix}});

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype({{prefix}}std::forward<T>(t){{postfix}});
```

[#]{.pnum} *Returns*: `{{prefix}}std::forward<T>(t){{postfix}}`.
{% endmacro %}

::: bq
::: addu

### Additional operations [additional.operations]

#### Class `subscript` [additional.operations.subscript]

```
struct subscript {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t)[std::forward<U>(u)]);

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t)[std::forward<U>(u)]);
```

[#]{.pnum} *Returns*: `std::forward<T>(t)[std::forward<U>(u)]`.

#### Class `left_shift` [additional.operations.left_shift]

{{ make_binary_func("left_shift", "<<") }}

#### Class `right_shift` [additional.operations.right_shift]

{{ make_binary_func("right_shift", ">>") }}

#### Class `unary_plus` [additional.operations.unary_plus]

{{ make_unary_func("unary_plus", "+", True) }}

#### Class `dereference` [additional.operations.dereference]

{{ make_unary_func("dereference", "*", True) }}

#### Class `increment` [additional.operations.increment]

{{ make_unary_func("increment", "++", True) }}

#### Class `decrement` [additional.operations.decrement]

{{ make_unary_func("decrement", "--", True) }}

#### Class `postfix_increment` [additional.operations.postfix_increment]

{{ make_unary_func("postfix_increment", "++", False) }}

#### Class `postfix_decrement` [additional.operations.postfix_decrement]

{{ make_unary_func("postfix_decrement", "--", False) }}

### Compound assignment operations [compound.operations]

#### Class `plus_equal` [compound.operations.plus_equal]

{{ make_binary_func("plus_equal", "+=") }}

#### Class `minus_equal` [compound.operations.minus_equal]

{{ make_binary_func("minus_equal", "-=") }}

#### Class `multiplies_equal` [compound.operations.multiplies_equal]

{{ make_binary_func("multiplies_equal", "*=") }}

#### Class `divides_equal` [compound.operations.divides_equal]

{{ make_binary_func("divides_equal", "/=") }}

#### Class `modulus_equal` [compound.operations.modulus_equal]

{{ make_binary_func("modulus_equal", "%=") }}

#### Class `bit_and_equal` [compound.operations.bit_and_equal]

{{ make_binary_func("bit_and_equal", "&=") }}

#### Class `bit_or_equal` [compound.operations.bit_or_equal]

{{ make_binary_func("bit_or_equal", "|=") }}

#### Class `bit_xor_equal` [compound.operations.bit_xor_equal]

{{ make_binary_func("bit_xor_equal", "^=") }}

#### Class `left_shift_equal` [compound.operations.left_shift_equal]

{{ make_binary_func("left_shift_equal", "<<=") }}

#### Class `right_shift_equal` [compound.operations.right_shift_equal]

{{ make_binary_func("right_shift_equal", ">>=") }}

:::
:::

Extend [func.bind.place]{.sref}:

::: bq
```diff
namespace std::placeholders {
  // M is the number of placeholders
+ template <int J>
+ struct $placeholder$ { // exposition only
+   template <class... Args>
+     constexpr decltype(auto) operator()(Args&&... ) const noexcept;
+  template <class T>
+    constexpr auto operator[](T&& ) const;
+ constexpr auto operator&() const;
+ };

  $see below$ _1;
  $see below$ _2;
              .
              .
              .
  $see below$ _M;
}
```

[1]{.pnum} The number `M` of placeholders is implementation-defined.

[2]{.pnum} All placeholder types meet the *Cpp17DefaultConstructible* and Cpp17CopyConstructible requirements, and their default constructors and copy/move constructors are constexpr functions that do not throw exceptions. It is implementation-defined whether placeholder types meet the *Cpp17CopyAssignable* requirements, but if so, their copy assignment operators are constexpr functions that do not throw exceptions.

[3]{.pnum} Placeholders should be defined as:
```diff
- inline constexpr $unspecified$ _1{};
+ inline constexpr $placeholder$<1> _1{};
```
If they are not, they are declared as:
```diff
- extern $unspecified$ _1;
+ extern $placeholder$<1> _1;
```
[4]{.pnum}  Placeholders are freestanding items ([freestanding.item]).

::: addu
```
template <int J>
template <class... Args>
decltype(auto) $placeholder$<J>::operator()(Args&&... args) const noexcept;
```

[#]{.pnum} *Constraints*: `sizeof...(Args) >= J` is `true`.

[#]{.pnum} *Returns*: `std::forward<Args>(args)...[J - 1]`.

```
template <int J>
template <class T>
auto $placeholder$<J>::operator[](T&& t) const;
```

[#]{.pnum} *Returns*: `bind(subscript(), *this, std::forward<T>(t))`.

```
template <int J>
auto $placeholder$<J>::operator&() const;
```

[#]{.pnum} *Returns*: `bind(addressof, *this)`.

{% macro make_binary_operator(op, func) %}
```
template<class A, class B> constexpr auto operator{{ op }}(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind({{ func }}, std::forward<A>(a), std::forward<B>(b))`.{% endmacro %}

{% macro make_unary_operator(op, func) %}
```
template<class A> constexpr auto operator{{ op }}(A&& a);
```

[#]{.pnum} *Returns*: `bind({{ func }}, std::forward<A>(a))`.{% endmacro %}

[#]{.pnum} Each operator function declared in this clause is constrained on at least one of the parameters having a type `T` which satisfies `is_placeholder_v<remove_cvref_t<T>> || is_bind_expression_v<remove_cvref_t<T>>` is `true`.

{{ make_binary_operator("+", "plus<>()") }}
{{ make_binary_operator("-", "minus<>()") }}
{{ make_binary_operator("*", "multiplies<>()") }}
{{ make_binary_operator("/", "divides<>()") }}
{{ make_binary_operator("%", "modulus<>()") }}
{{ make_unary_operator("-", "negate<>()") }}
{{ make_binary_operator("==", "equal_to<>()") }}
{{ make_binary_operator("!=", "not_equal_to<>()") }}
{{ make_binary_operator("<", "less<>()") }}
{{ make_binary_operator(">", "greater<>()") }}
{{ make_binary_operator("<=", "less_equal<>()") }}
{{ make_binary_operator(">=", "greater_equal<>()") }}
{{ make_binary_operator("<=>", "compare_three_way()") }}
{{ make_binary_operator("&&", "logical_and<>()") }}
{{ make_binary_operator("||", "logical_or<>()") }}
{{ make_unary_operator("!", "logical_not<>()") }}
{{ make_binary_operator("&", "bit_and<>()") }}
{{ make_binary_operator("|", "bit_or<>()") }}
{{ make_binary_operator("^", "bit_xor<>()") }}
{{ make_unary_operator("~", "bit_not<>()") }}

```
template<class A, class B> constexpr auto operator<<(A&& a, B&& b);
```

[#]{.pnum} *Constraints*: `is_base_of_v<ios_base, remove_cvref_t<A>>` is `false`.

[#]{.pnum} *Returns*: `bind(left_shift(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<<(A& a, B&& b);
```

[#]{.pnum} *Constraints*: `is_base_of_v<ios_base, remove_cvref_t<A>>` is `true`.

[#]{.pnum} *Returns*: `bind(left_shift(), ref(a), std::forward<B>(b))`.

[#]{.pnum} *Remarks*: This overload allows expressions like `std::cout << _1 << '\n'` to work.

```
template<class A, class B> constexpr auto operator>>(A&& a, B&& b);
```

[#]{.pnum} *Constraints*: `is_base_of_v<ios_base, remove_cvref_t<A>>` is `false`.

[#]{.pnum} *Returns*: `bind(right_shift(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator>>(A& a, B&& b);
```

[#]{.pnum} *Constraints*: `is_base_of_v<ios_base, remove_cvref_t<A>>` is `true`.

[#]{.pnum} *Returns*: `bind(right_shift(), ref(a), std::forward<B>(b))`.

{{ make_unary_operator("+", "unary_plus()") }}
{{ make_unary_operator("*", "dereference()") }}
{{ make_unary_operator("++", "increment()") }}
{{ make_unary_operator("--", "decrement()") }}

```
template<class A> constexpr auto operator++(A&& a, int);
```

[#]{.pnum} *Returns*: `bind(postfix_increment(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator--(A&& a, int);
```

[#]{.pnum} *Returns*: `bind(postfix_decrement(), std::forward<A>(a))`.

{{ make_binary_operator("+=", "plus_equal()") }}
{{ make_binary_operator("-=", "minus_equal()") }}
{{ make_binary_operator("*=", "multiplies_equal()") }}
{{ make_binary_operator("/=", "divides_equal()") }}
{{ make_binary_operator("%=", "modulus_equal()") }}
{{ make_binary_operator("&=", "bit_and_equal()") }}
{{ make_binary_operator("|=", "bit_or_equal()") }}
{{ make_binary_operator("^=", "bit_xor_equal()") }}
{{ make_binary_operator("<<=", "left_shift_equal()") }}
{{ make_binary_operator(">>=", "right_shift_equal()") }}

```
template<class A, class B> constexpr auto operator->*(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(std::forward<B>(b), std::forward<A>(a))`.
:::
:::

---
references:
    - id: Boost.Lambda2
      citation-label: Boost.Lambda2
      title: "Lambda2: A C++14 Lambda Library"
      author:
        - family: Peter Dimov
      issued:
        - year: 2020
      URL: https://www.boost.org/doc/libs/master/libs/lambda2/doc/html/lambda2.html
---
