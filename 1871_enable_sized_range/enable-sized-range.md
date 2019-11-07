---
title: Concept traits should be named after concepts
document: D1871R1
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Revision History

R0 [@P1871R0] was presented in Belfast and originally proposed making all of the variable template opt-ins for concepts named `enable_NAME`. LEWG rejected this direction. However, one of the other changes in the paper was to ensure at least that the traits were named the same as the concepts. This used to be the case, except when we renamed the concept to `sized_sentinel_for` we kept the trait named `disable_sized_sentinel`, so this paper proposes simply updating that trait to have the same name - so that all such traits are either `enable_NAME` or `disable_NAME`.

# Motivation

The concept `sized_sentinel_for`, from [\[iterator.concept.sizedsentinel\]](http://eel.is/c++draft/iterator.concept.sizedsentinel), is defined as:

```cpp
template<class S, class I>
  concept sized_sentinel_for =
    sentinel_for<S, I> &&
    !disable_sized_sentinel<remove_cv_t<S>, remove_cv_t<I>> &&
    requires(const I& i, const S& s) {
      { s - i } -> same_as<iter_difference_t<I>>;
      { i - s } -> same_as<iter_difference_t<I>>;
    };
```

Note that the concept is named `sized_sentinel_for` but the trait is named `disable_sized_sentinel`. These used to be closely related because the concept used to be named `SizedSentinel` and got the suffix `_for` added as part of [@P1754R1].

This paper proposes that the trait and the concept should have the same name.

# Wording

Change 23.2 [iterator.synopsis]:

::: bq
```diff
  // [iterator.concept.sizedsentinel], concept sized_sentinel_for
  template<class S, class I>
-   inline constexpr bool disable_sized_sentinel = false;
+   inline constexpr bool disable_sized_sentinel@[_for]{.diffins}@ = false;
```
:::

and later:

::: bq
```diff
  template<class Iterator1, class Iterator2>
      requires (!sized_sentinel_for<Iterator1, Iterator2>)
-   inline constexpr bool disable_sized_sentinel<reverse_iterator<Iterator1>,
-                                                reverse_iterator<Iterator2>> = true;
+   inline constexpr bool disable_sized_sentinel@[_for]{.diffins}@<reverse_iterator<Iterator1>,
+                                                    reverse_iterator<Iterator2>> = true;
```
:::

Change 23.3.4.8 [iterator.concept.sizedsentinel]:

::: bq
[1]{.pnum} The `sized_sentinel_for` concept specifies requirements on an `input_or_output_iterator` and a corresponding `sentinel_for` that allow the use of the `-` operator to compute the distance between them in constant time.

```diff
template<class S, class I>
  concept sized_sentinel_for =
    sentinel_for<S, I> &&
-   !disable_sized_sentinel<remove_cv_t<S>, remove_cv_t<I>> &&
+   !disable_sized_sentinel@[_for]{.diffins}@<remove_cv_t<S>, remove_cv_t<I>> &&
    requires(const I& i, const S& s) {
      { s - i } -> same_as<iter_difference_t<I>>;
      { i - s } -> same_as<iter_difference_t<I>>;
    };
```

[2]{.pnum} Let `i` be an iterator of type `I`, and `s` a sentinel of type `S` such that `[i, s)` denotes a range.
Let `N` be the smallest number of applications of `++i` necessary to make `bool(i == s)` be `true`.
`S` and `I` model `sized_sentinel_for<S, I>` only if

    - [2.1]{.pnum} If `N` is representable by `iter_difference_t<I>`, then `s - i` is well-defined and equals `N`.
    - [2.2]{.pnum} If `−N` is representable by `iter_difference_t<I>`, then `i - s` is well-defined and equals `−N`.


```diff
  template<class S, class I>
-   inline constexpr bool disable_sized_sentinel = false;
+   inline constexpr bool disable_sized_sentinel@[_for]{.diffins}@ = false;
```

[3]{.pnum} *Remarks*: Pursuant to [namespace.std], users may specialize [`disable_sized_sentinel`]{.rm} [`disable_sized_sentinel_for`]{.addu} for *cv*-unqualified non-array object types `S` and `I` if `S` and/or `I` is a program-defined type.
Such specializations shall be usable in constant expressions ([expr.const]) and have type `const bool`.

[4]{.pnum} [ *Note*: [`disable_sized_sentinel`]{.rm} [`disable_sized_sentinel_for`]{.addu} allows use of sentinels and iterators with the library that satisfy but do not in fact model `sized_sentinel_for`. — *end note*  ]

:::

---
references:
---