---
title: "Remove `std::weak_equality` and `std::strong_equality`"
document: P1959R0
date: today
audience: EWG, LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
tag: spaceship
---

# Introduction

This paper resolves NB comments [US 170](https://github.com/cplusplus/nbballot/issues/168):

::: bq
The `strong_equality` and `weak_equality` comparison categories don’t make sense now
that we split equality from ordering. It doesn’t make sense to declare an
`operator<=>` that returns one of these – they just add needless complexity.
:::

and [CA 173](https://github.com/cplusplus/nbballot/issues/171):

::: bq
With the separation of `<=>` and `==`, `weak_equality` has lost its primary use
(of being a potential return type of `<=>`). Currently weak_equality serves no
useful purpose in the standard (i.e., nothing in std acts on it), and just causes
confusion (what’s the difference between weak and strong, when should I use which?)
The difference between the two is ill-defined (involving substitutability and
“salient” properties, which are also vaguely defined). The best definition of
equality for a type is the type’s own `==` operator. We should not try to
sub-divide the concept of equality.
:::

The first of these comments subsumes the other, and this paper provides the
wording for that change.

# Wording

Change 7.6.8 [expr.spaceship], paragraph 7, to remove the ability to call `<=>`
on function pointers, pointers to members, and `nullptr_t`.

::: bq
[7]{.pnum} [If the composite pointer type is a function pointer type, a pointer-to-member type, or `std​::​nullptr_t`, the result is of type `std​::​strong_equality`; the result is `std​::​strong_equality​::​equal` if the (possibly converted) operands compare equal ([expr.eq]) and `std​::​strong_equality​::​nonequal` if they compare unequal, otherwise the result of the operator is unspecified.]{.rm}
:::

Change 7.6.8 [expr.spaceship], paragraph 10:

::: bq
[10]{.pnum} The [five]{.rm} [three]{.addu} comparison category types (the types `std​::​strong_ordering`, [`std​::​strong_equality`,]{.rm} `std​::​weak_ordering`, [`std​::​weak_equality`,]{.rm} and `std​::​partial_ordering`) are not predefined; [...]
:::

Change 11.11.1 [class.compare.default], paragraph 4:

::: bq
[4]{.pnum} A type `C` has _strong structural equality_ if, given a glvalue `x`
of type `const C`, either:

- [4.1]{.pnum} `C` is a non-class type and `x <=> x` is a valid expression of
type `std::strong_ordering` [or `std::strong_equality`]{.rm}, or
- [4.2]{.pnum} `C` is a class type where all of the following hold: [...]
:::

Remove the `XXX_equality` cases from 11.11.3 [class.spaceship], paragraph 1:

::: bq
[1]{.pnum} The _synthesized three-way comparison_ for comparison category type `R`
([cmp.categories]) of glvalues `a` and `b` of the same type is defined as follows:

- [1.1]{.pnum} [...]
- [1.5]{.pnum} Otherwise, if `R` is `partial_ordering`, then

  > ```
  > a == b ? partial_ordering::equivalent :
  > a < b  ? partial_ordering::less :
  > b < a  ? partial_ordering::greater :
  >          partial_ordering::unordered
  > ```
- [1.6]{.pnum} [Otherwise, if `R` is `strong_equality`, then
`a == b ? strong_equality::equal : strong_equality::nonequal`;]{.rm}
- [1.7]{.pnum} [Otherwise, if `R` is `weak_equality`, then
`a == b ? weak_equality::equivalent : weak_equality::nonequivalent`;]{.rm}
- [1.8]{.pnum} Otherwise, the synthesized three-way comparison is not defined.
:::

Remove the `XXX_equality` cases from 11.11.3 [class.spaceship], paragraph 3:

::: bq
The _common comparison type_ `U` of a possibly-empty list of `n` types `T0`, `T1`, …, `Tn−1` is defined as follows:

- [4.1]{.pnum} If any `Ti` is not a comparison category type ([cmp.categories]), `U` is void.
- [4.2]{.pnum} [Otherwise, if at least one `Ti` is `std​::​weak_equality`, or at least one `Ti` is `std​::​strong_equality` and at least one `Tj` is `std​::​partial_ordering` or `std​::​weak_ordering`, `U` is `std​::​weak_equality` ([cmp.weakeq]).]{.rm}
- [4.3]{.pnum} [Otherwise, if at least one `Ti` is `std​::​strong_equality`, `U` is `std​::​strong_equality` ([cmp.strongeq]).]{.rm}
- [4.4]{.pnum} Otherwise, if at least one `Ti` is `std​::​partial_ordering`, `U` is `std​::​partial_ordering` ([cmp.partialord]).
- [4.5]{.pnum} Otherwise, if at least one `Ti` is `std​::​weak_ordering`, `U` is `std​::​weak_ordering` ([cmp.weakord]).
- [4.6]{.pnum} Otherwise, `U` is `std​::​strong_ordering` ([cmp.strongord]).
:::

Change the example in 11.11.4 [class.rel], paragraph 3, to use a different type that has no `<`:

::: bq
```diff
+ struct HasNoLessThan { };

  struct C {
-   friend std::strong_equality operator<=>(const C&, const C&);
+   friend HasNoLessThan operator<=>(const C&, const C&);
    bool operator<(const C&) const = default;             // OK, function is deleted
  };
```
:::

Remove `<=>` from 12.7 [over.built], paragraph 19:

::: bq
[19]{.pnum} For every `T`, where `T` is a pointer-to-member type or `std​::​nullptr_t`, there exist candidate operator functions of the form:

```diff
  bool                 operator==(T, T);
  bool                 operator!=(T, T);
- std::strong_equality operator<=>(T, T);
```
:::

Change 13.2 [temp.param]/4 to add back the bullets that [@P0732R2] removed, now that these other types no longer have strong structural equality:

::: bq
[4]{.pnum} A non-type template-parameter shall have one of the following (optionally cv-qualified) types:

- [4.1]{.pnum} a literal type that has strong structural equality ([class.compare.default]),
- [4.2]{.pnum} an lvalue reference type,
- [4.3]{.pnum} a type that contains a placeholder type ([dcl.spec.auto]), [or]{.rm}
- [4.4]{.pnum} a placeholder for a deduced class type ([dcl.type.class.deduct])[.]{.rm} [,]{.addu}
- [4.5]{.pnum} [pointer to object or pointer to function,]{.addu}
- [4.6]{.pnum} [pointer to member, or]{.addu}
- [4.7]{.pnum} [`std::nullptr_t`.]{.addu}
:::

Remove the `XXX_equality` types from the compare synopsis in 17.11.1 [compare.syn]:

```diff
namespace std {
  // [cmp.categories], comparison category types
- class weak_equality;
- class strong_equality;
  class partial_ordering;
  class weak_ordering;
  class strong_ordering;

  // named comparison functions
- constexpr bool is_eq  (weak_equality cmp) noexcept    { return cmp == 0; }
- constexpr bool is_neq (weak_equality cmp) noexcept    { return cmp != 0; }
+ constexpr bool is_eq  (partial_ordering cmp) noexcept { return cmp == 0; }
+ constexpr bool is_neq (partial_ordering cmp) noexcept { return cmp != 0; }
  constexpr bool is_lt  (partial_ordering cmp) noexcept { return cmp < 0; }
  constexpr bool is_lteq(partial_ordering cmp) noexcept { return cmp <= 0; }
  constexpr bool is_gt  (partial_ordering cmp) noexcept { return cmp > 0; }
  constexpr bool is_gteq(partial_ordering cmp) noexcept { return cmp >= 0; }
}
```

Change 17.11.2.1 [cmp.categories.pre], paragraphs 1-2:

::: bq
[1]{.pnum} The types [`weak_equality`, `strong_equality`,]{.rm} `partial_ordering`, `weak_ordering`, and `strong_ordering` are collectively termed the comparison category types.
Each is specified in terms of an exposition-only data member named value whose value typically corresponds to that of an enumerator from one of the following exposition-only enumerations:

```
enum class eq { equal = 0, equivalent = equal,
                nonequal = 1, nonequivalent = nonequal };   // exposition only
enum class ord { less = -1, greater = 1 };                  // exposition only
enum class ncmp { unordered = -127 };                       // exposition only
```

[2]{.pnum} [ Note: The type[s]{.rm} `strong_ordering` [and weak_equality]{.rm} correspond[s]{.addu}[, respectively,]{.rm} to the term[s]{.rm} total ordering [and equivalence]{.rm} in mathematics.
— end note
 ]

:::

Remove 17.11.2.2 [cmp.weakeq] (the subclause that defines `std::weak_equality`).

Remove 17.11.2.3 [cmp.strongeq] (the subclause that defines `std::strong_equality`).

Remove the conversion operator to `weak_equality` from 17.11.2.4 [cmp.partialord]:

::: bq
```diff
namespace std {
  class partial_ordering {
    [...]

-   // conversion
-   constexpr operator weak_equality() const noexcept;

    [...]
  };
}
```

::: rm
```constexpr operator weak_equality() const noexcept;```

[2]{.pnum} *Returns*: `value == 0 ? weak_equality​::​equivalent : weak_equality​::​nonequivalent`.
[ Note: The result is independent of the `is_ordered` member.
— end note
 ]
:::
:::

Remove the conversion operator to `weak_equality` from 17.11.2.5 [cmp.weakord]:

::: bq
```diff
namespace std {
  class weak_ordering  {
    [...]

    // conversion
-   constexpr operator weak_equality() const noexcept;
    constexpr operator partial_ordering() const noexcept;

    [...]
  };
}
```

::: rm
```constexpr operator weak_equality() const noexcept;```

[2]{.pnum} *Returns*: `value == 0 ? weak_equality​::​equivalent : weak_equality​::​nonequivalent`.
:::
:::

Remove the conversion operators to `XXX_equality` from 17.11.2.6 [cmp.strongord]:

::: bq
```diff
namespace std {
  class strong_ordering   {
    [...]

    // conversions
-   constexpr operator weak_equality() const noexcept;
-   constexpr operator strong_equality() const noexcept;
    constexpr operator partial_ordering() const noexcept;
    constexpr operator weak_ordering() const noexcept;

    [...]
  };
}
```

::: rm
```constexpr operator weak_equality() const noexcept;```

[2]{.pnum} *Returns*: `value == 0 ? weak_equality​::​equivalent : weak_equality​::​nonequivalent`.

```constexpr operator strong_equality() const noexcept;```

[3]{.pnum} *Returns*: `value == 0 ? strong_equality​::​equal : strong_equality​::nonequal`.
:::
:::

Simplify the three-way comparable concepts in 17.11.4 [cmp.concept]:

::: bq
```diff
template <typename T, typename Cat = partial_ordering>
  concept three_way_comparable  =
    @_weakly-equality-comparable-with_@<T, T> &&
-   (!convertible_to<Cat, partial_ordering> || @_partially-ordered-with_@<T, T>) &&
+   @_partially-ordered-with_@<T, T> &&
    requires(const remove_reference_t<T>& a,
             const remove_reference_t<T>& b) {
      { a <=> b } -> @_compares-as_@<Cat>;
    };
```

[2]{.pnum} Let `a` and `b` be lvalues of type `const remove_reference_t<T>`. `T` and `Cat` model `three_way_comparable<T, Cat>` only if:

- [2.1]{.pnum} `(a <=> b == 0) == bool(a == b)`.
- [2.2]{.pnum} `(a <=> b != 0) == bool(a != b)`.
- [2.3]{.pnum} `((a <=> b) <=> 0)` and `(0 <=> (b <=> a))` are equal.
- [2.4]{.pnum} [If `Cat` is convertible to `strong_equality`, `T` models ` equality_comparable_with` ([concept.equalitycomparable]).]{.rm}
- [2.5]{.pnum} [If `Cat` is convertible to `partial_ordering`:]{.rm} [Make the following subbullets into normal bullets]{.ednote}
	- [2.5.1]{.pnum} `(a <=> b < 0) == bool(a < b)`.
    - [2.5.2]{.pnum} `(a <=> b > 0) == bool(a > b)`.
    - [2.5.3]{.pnum} `(a <=> b <= 0) == bool(a <= b)`.
    - [2.5.4]{.pnum} `(a <=> b >= 0) == bool(a >= b)`.
- [2.5.5]{.pnum} If `Cat` is convertible to `strong_ordering`, `T` models ` totally_ordered` ([concept.totallyordered]).

```diff
template <typename T, typename U,
          typename Cat = partial_ordering>
  concept three_way_comparable_with =
    @_weakly-equality-comparable-with_@<T, U> &&
-   (!convertible_to<Cat, partial_ordering> || @_partially-ordered-with_@<T, U>) &&
+   @_partially-ordered-with_@<T, U> &&
    three_way_comparable<T, Cat> &&
    three_way_comparable<U, Cat> &&
    common_reference_with<const remove_reference_t<T>&, const remove_reference_t<U>&> &&
    three_way_comparable<
      common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>,
      Cat> &&
    requires(const remove_reference_t<T>& t,
             const remove_reference_t<U>& u) {
      { t <=> u } -> @_compares-as_@<Cat>;
      { u <=> t } -> @_compares-as_@<Cat>;
    };
```

[3]{.pnum} Let `t` and `u` be lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`, respectively. Let `C` be `common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>`. `T`, `U`, and `Cat` model `ThreeWayComparableWith<T, U, Cat>` only if:

- [3.1]{.pnum} `t <=> u` and `u <=> t` have the same domain.
- [3.2]{.pnum} `((t <=> u) <=> 0)` and `(0 <=> (u <=> t))` are equal.
- [3.3]{.pnum} `(t <=> u == 0) == bool(t == u)`.
- [3.4]{.pnum} `(t <=> u != 0) == bool(t != u)`.
- [3.5]{.pnum} `Cat(t <=> u) == Cat(C(t) <=> C(u))`.
- [3.6]{.pnum} [If `Cat` is convertible to `strong_equality`, `T` and `U` model `equality_comparable_with<T, U>` ([concepts.equalitycomparable]).]{.rm}
- [3.7]{.pnum} [If `Cat` is convertible to `partial_ordering`:]{.rm} [Make the following subbullets into normal bullets]{.ednote}
	- [3.7.1]{.pnum} `(t <=> u < 0) == bool(t < u)`
    - [3.7.2]{.pnum} `(t <=> u > 0) == bool(t > u)`
    - [3.7.3]{.pnum} `(t <=> u <= 0) == bool(t <= u)`
    - [3.7.4]{.pnum} `(t <=> u >= 0) == bool(t >= u)`
- [3.8]{.pnum} If `Cat` is convertible to `strong_ordering`, `T` and `U` model `totally_ordered_with<T, U>` ([concepts.totallyordered]).
:::

Change the root comparison category in some of the iterator `operator<=>`s from `weak_equality` to `partial_ordering` (that is, just remove the provided argument) in 23.2 [iterator.synopsis]:

::: bq
```diff
#include <concepts>

namespace std {
  [...]

- template<class Iterator1, three_way_comparable_with<Iterator1@[, weak_equality]{.diffdel}@> Iterator2>
+ template<class Iterator1, three_way_comparable_with<Iterator1> Iterator2>
    constexpr compare_three_way_result_t<Iterator1, Iterator2>
      operator<=>(const reverse_iterator<Iterator1>& x,
                  const reverse_iterator<Iterator2>& y);

  [...]

- template<class Iterator1, three_way_comparable_with<Iterator1@[, weak_equality]{.diffdel}@> Iterator2>
+ template<class Iterator1, three_way_comparable_with<Iterator1> Iterator2>
    constexpr compare_three_way_result_t<Iterator1, Iterator2>
      operator<=>(const move_iterator<Iterator1>& x,
                  const move_iterator<Iterator2>& y);

  [...]
}
```
:::

And the same in 23.5.1.7 [reverse.iter.cmp]:

::: bq
```diff
- template<class Iterator1, three_way_comparable_with<Iterator1@[, weak_equality]{.diffdel}@> Iterator2>
+ template<class Iterator1, three_way_comparable_with<Iterator1> Iterator2>
    constexpr compare_three_way_result_t<Iterator1, Iterator2>
      operator<=>(const reverse_iterator<Iterator1>& x,
                  const reverse_iterator<Iterator2>& y);
```
[13]{.pnum} *Returns*: `y.base() <=> x.base()`.
:::

And the same in 23.5.3.7 [move.iter.pop.cmp]:

::: bq
```diff
- template<class Iterator1, three_way_comparable_with<Iterator1@[, weak_equality]{.diffdel}@> Iterator2>
+ template<class Iterator1, three_way_comparable_with<Iterator1> Iterator2>
    constexpr compare_three_way_result_t<Iterator1, Iterator2>
      operator<=>(const move_iterator<Iterator1>& x,
                  const move_iterator<Iterator2>& y);
```
[13]{.pnum} *Returns*: `x.base() <=> y.base()`.
:::

