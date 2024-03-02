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

```
struct subscript {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) << std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) << std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) << std::forward<U>(u)`.

#### Class `right_shift` [additional.operations.right_shift]

```
struct right_shift {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) >> std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) >> std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) >> std::forward<U>(u)`.

#### Class `unary_plus` [additional.operations.unary_plus]

```
struct unary_plus {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(+std::forward<T>(t));

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(+std::forward<T>(t));
```

[#]{.pnum} *Returns*: `+std::forward<T>(t)`.

#### Class `dereference` [additional.operations.dereference]

```
struct dereference {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(*std::forward<T>(t));

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(*std::forward<T>(t));
```

[#]{.pnum} *Returns*: `*std::forward<T>(t)`.

#### Class `increment` [additional.operations.increment]

```
struct increment {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(++std::forward<T>(t));

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(++std::forward<T>(t));
```

[#]{.pnum} *Returns*: `++std::forward<T>(t)`.

#### Class `decrement` [additional.operations.decrement]

```
struct decrement {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(--std::forward<T>(t));

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(--std::forward<T>(t));
```

[#]{.pnum} *Returns*: `--std::forward<T>(t)`.

#### Class `postfix_increment` [additional.operations.postfix_increment]

```
struct postfix_increment {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(std::forward<T>(t)++);

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(std::forward<T>(t)++);
```

[#]{.pnum} *Returns*: `std::forward<T>(t)++`.

#### Class `postfix_decrement` [additional.operations.postfix_decrement]

```
struct postfix_decrement {
  template<class T> constexpr auto operator()(T&& t) const
    -> decltype(std::forward<T>(t)--);

  using is_transparent = $unspecified$;
};
```

```
template<class T> constexpr auto operator()(T&& t) const
  -> decltype(std::forward<T>(t)--);
```

[#]{.pnum} *Returns*: `std::forward<T>(t)--`.

### Compound assignment operations [compound.operations]

#### Class `plus_equal` [compound.operations.plus_equal]

```
struct plus_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) += std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) += std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) += std::forward<U>(u)`.

#### Class `minus_equal` [compound.operations.minus_equal]

```
struct minus_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) -= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) -= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) -= std::forward<U>(u)`.

#### Class `multiplies_equal` [compound.operations.multiplies_equal]

```
struct multiplies_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) *= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) *= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) *= std::forward<U>(u)`.

#### Class `divides_equal` [compound.operations.divides_equal]

```
struct divides_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) /= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) /= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) /= std::forward<U>(u)`.

#### Class `modulus_equal` [compound.operations.modulus_equal]

```
struct modulus_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) %= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) %= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) %= std::forward<U>(u)`.

#### Class `bit_and_equal` [compound.operations.bit_and_equal]

```
struct bit_and_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) &= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) &= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) &= std::forward<U>(u)`.

#### Class `bit_or_equal` [compound.operations.bit_or_equal]

```
struct bit_or_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) |= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) |= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) |= std::forward<U>(u)`.

#### Class `bit_xor_equal` [compound.operations.bit_xor_equal]

```
struct bit_xor_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) ^= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) ^= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) ^= std::forward<U>(u)`.

#### Class `left_shift_equal` [compound.operations.left_shift_equal]

```
struct left_shift_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) <<= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) <<= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) <<= std::forward<U>(u)`.

#### Class `right_shift_equal` [compound.operations.right_shift_equal]

```
struct right_shift_equal {
  template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
    -> decltype(std::forward<T>(t) >>= std::forward<U>(u));

  using is_transparent = $unspecified$;
};
```

```
template<class T, class U> constexpr auto operator()(T&& t, U&& u) const
  -> decltype(std::forward<T>(t) >>= std::forward<U>(u));
```

[#]{.pnum} *Returns*: `std::forward<T>(t) >>= std::forward<U>(u)`.

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

[#]{.pnum} *Returns*: `bind(subscript(), *this, std::forward<T>(t))`.

```
template<class A, class B> constexpr auto operator+(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(plus<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator-(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(minus<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator*(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(multiplies<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator/(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(divides<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator%(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(modulus<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A> constexpr auto operator-(A&& a);
```

[#]{.pnum} *Returns*: `bind(negate<>(), std::forward<A>(a))`.

```
template<class A, class B> constexpr auto operator==(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(equal_to<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator!=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(not_equal_to<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(less<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator>(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(greater<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(less_equal<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator>=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(greater_equal<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<=>(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(compare_three_way(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator&&(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(logical_and<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator||(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(logical_or<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A> constexpr auto operator!(A&& a);
```

[#]{.pnum} *Returns*: `bind(logical_not<>(), std::forward<A>(a))`.

```
template<class A, class B> constexpr auto operator&(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_and<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator|(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_or<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator^(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_xor<>(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A> constexpr auto operator~(A&& a);
```

[#]{.pnum} *Returns*: `bind(bit_not<>(), std::forward<A>(a))`.

```
template<class A, class B> constexpr auto operator<<(A&& a, B&& b);
```

[#]{.pnum} *Constraints*: `!std::is_base_of_v<std::ios_base, remove_cvref_t<A>>`.

[#]{.pnum} *Returns*: `bind(left_shift(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<<(A& a, B&& b);
```

[#]{.pnum} *Constraints*: `std::is_base_of_v<std::ios_base, remove_cvref_t<A>>`.

[#]{.pnum} *Returns*: `bind(left_shift(), std::ref(a), std::forward<B>(b))`.

[#]{.pnum} *Remarks*: This overload allows expressions like `std::cout << _1 << '\n'` to work.

```
template<class A, class B> constexpr auto operator>>(A&& a, B&& b);
```

[#]{.pnum} *Constraints*: `!std::is_base_of_v<std::ios_base, remove_cvref_t<A>>`.

[#]{.pnum} *Returns*: `bind(right_shift(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator>>(A& a, B&& b);
```

[#]{.pnum} *Constraints*: `std::is_base_of_v<std::ios_base, remove_cvref_t<A>>`.

[#]{.pnum} *Returns*: `bind(right_shift(), std::ref(a), std::forward<B>(b))`.

```
template<class A> constexpr auto operator+(A&& a);
```

[#]{.pnum} *Returns*: `bind(unary_plus(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator*(A&& a);
```

[#]{.pnum} *Returns*: `bind(dereference(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator++(A&& a);
```

[#]{.pnum} *Returns*: `bind(increment(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator--(A&& a);
```

[#]{.pnum} *Returns*: `bind(decrement(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator++(A&& a, int);
```

[#]{.pnum} *Returns*: `bind(postfix_increment(), std::forward<A>(a))`.

```
template<class A> constexpr auto operator--(A&& a, int);
```

[#]{.pnum} *Returns*: `bind(postfix_decrement(), std::forward<A>(a))`.

```
template<class A, class B> constexpr auto operator+=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(plus_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator-=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(minus_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator*=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(multiplies_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator/=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(divides_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator%=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(modulus_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator&=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_and_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator|=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_or_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator^=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(bit_xor_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator<<=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(left_shift_equal(), std::forward<A>(a), std::forward<B>(b))`.

```
template<class A, class B> constexpr auto operator>>=(A&& a, B&& b);
```

[#]{.pnum} *Returns*: `bind(right_shift_equal(), std::forward<A>(a), std::forward<B>(b))`.

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
