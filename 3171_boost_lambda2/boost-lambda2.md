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

As noted in [@P2760R0], there are a lot of function objects for operators in the standard library, but several operators are missing. This paper proposes to add the functionality for all the missing operators, but to do it in a different way than by adding function objects.

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

We propose to solve the issue of missing operator function objects in the standard library, as well as less-than-ergonomic lambda syntax for common predicates, by standardizing Boost.Lambda2.

This is not a large proposal. The standard library already provides placeholders, `std::namespace::_1` and friends. The standard library also already provides `std::bind`, which is already implemented in a way that supports composition of bind expressions. All we need to do is add operators.

## Implementation Experience

Has been shipping in Boost since 1.77 (August 2021).

## Wording

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
+   template<class A, class B> constexpr auto operator>>(A&&, B&&);
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

[#]{.pnum} *Returns*: `std::forward<Args>(args)...[J - 1]`.

```
template <int J>
template <class T>
auto $placeholder$<J>::operator[](T&& t) const;
```

[#]{.pnum} Let `$F$` be a binary function object such that `$F$(x, y)` returns `x[y]`.

[#]{.pnum} *Returns*: `bind($F$, *this, std::forward<T>(t))`.

```
template<class A, class B> constexpr auto operator+(A&& a, B&& b);
template<class A, class B> constexpr auto operator-(A&& a, B&& b);
template<class A, class B> constexpr auto operator*(A&& a, B&& b);
template<class A, class B> constexpr auto operator/(A&& a, B&& b);
template<class A, class B> constexpr auto operator%(A&& a, B&& b);
template<class A> constexpr auto operator-(A&& a);
template<class A, class B> constexpr auto operator==(A&& a, B&& b);
template<class A, class B> constexpr auto operator!=(A&& a, B&& b);
template<class A, class B> constexpr auto operator<(A&& a, B&& b);
template<class A, class B> constexpr auto operator>(A&& a, B&& b);
template<class A, class B> constexpr auto operator<=(A&& a, B&& b);
template<class A, class B> constexpr auto operator>=(A&& a, B&& b);
template<class A, class B> constexpr auto operator<=>(A&& a, B&& b);
template<class A, class B> constexpr auto operator&&(A&& a, B&& b);
template<class A, class B> constexpr auto operator||(A&& a, B&& b);
template<class A> constexpr auto operator!(A&& a);
template<class A, class B> constexpr auto operator&(A&& a, B&& b);
template<class A, class B> constexpr auto operator|(A&& a, B&& b);
template<class A, class B> constexpr auto operator^(A&& a, B&& b);
template<class A> constexpr auto operator~(A&& a);
template<class A, class B> constexpr auto operator<<(A&& a, B&& b);
template<class A, class B> constexpr auto operator>>(A&& a, B&& b);
template<class A> constexpr auto operator+(A&& a);
template<class A> constexpr auto operator*(A&& a);
template<class A> constexpr auto operator++(A&& a);
template<class A> constexpr auto operator--(A&& a);
template<class A> constexpr auto operator++(A&& a, int);
template<class A> constexpr auto operator--(A&& a, int);
template<class A, class B> constexpr auto operator+=(A&& a, B&& b);
template<class A, class B> constexpr auto operator-=(A&& a, B&& b);
template<class A, class B> constexpr auto operator*=(A&& a, B&& b);
template<class A, class B> constexpr auto operator/=(A&& a, B&& b);
template<class A, class B> constexpr auto operator%=(A&& a, B&& b);
template<class A, class B> constexpr auto operator&=(A&& a, B&& b);
template<class A, class B> constexpr auto operator|=(A&& a, B&& b);
template<class A, class B> constexpr auto operator^=(A&& a, B&& b);
template<class A, class B> constexpr auto operator<<=(A&& a, B&& b);
template<class A, class B> constexpr auto operator>>=(A&& a, B&& b);
```

[#]{.pnum} For each operator `op`, let `@*F*~op~@` be a function object such that:

* [#.1]{.pnum} For a binary operator, `@*F*~op~@(x, y)` returns `x $op$ y`.
* [#.2]{.pnum} For a unary prefix operator, `@*F*~op~@(x)` returns `$op$ x`.
* [#.2]{.pnum} For a unary postfix operator, `@*F*~op~@(x)` returns `x $op$`.

[#]{.pnum} *Returns* `bind(@*F*~op~@, std::forward<A>(a))` for the unary operators and `bind(@*F*~op~@, std::forward<A>(a), std::forward<B>(b))` for the binary operators.

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
