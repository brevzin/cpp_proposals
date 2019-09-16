---
title: Rename `disable_` traits to `enable_`
document: D1871R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Motivation

The `sized_range` concept is currently defined as follows, in [\[range.sized\]](http://eel.is/c++draft/range.sized):

```cpp
template<class T>
  concept sized_range =
    range<T> &&
    !disable_sized_range<remove_cvref_t<T>> &&
    requires(T& t) { ranges::size(t); };
```

The reason for the extra `!disable_sized_range<remove_cvref_t<T>>` check is that some types can meet the syntactic requirements of `ranges::size` without meeting the semantic requirement that this call must have constant time. For instance, a pre-C++11 `std::list` had `O(N)` `size()`, but this wouldn't be detectable, so the type trait exists to allow for such containers to opt out of being considered `sized_range`s.

The existence of the type trait makes sense. However, why does it have to be a _negative_, that is then checked _against_? Double negatives are needlessly difficult to understand. Moreover, this type trait will be very rarely opted into.

The same argument can be made for the `disable_sized_sentinel` trait, currently used as:

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

We already have `enable_view` as the type trait to opt into the `view` concept. If we rename `disable_sized_range` to `enable_sized_range` and `disable_sized_sentinel` to `enable_sized_sentinel_for`, then all of type traits spelled the same way: specifically `enable_concept_name`.

# Proposal

Change 23.2 [iterator.synopsis]:

::: bq
```diff
  // [iterator.concept.sizedsentinel], concept sized_sentinel_for
  template<class S, class I>
-   inline constexpr bool disable_sized_sentinel = false;
+   inline constexpr bool enable_sized_sentinel_for = true;
```
:::

and later:

::: bq
```diff
  template<class Iterator1, class Iterator2>
      requires (!sized_sentinel_for<Iterator1, Iterator2>)
-   inline constexpr bool disable_sized_sentinel<reverse_iterator<Iterator1>,
-                                                reverse_iterator<Iterator2>> = true;
+   inline constexpr bool enable_sized_sentinel_for<reverse_iterator<Iterator1>,
+                                                   reverse_iterator<Iterator2>> = false;
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
+   enable_sized_sentinel_for<remove_cv_t<S>, remove_cv_t<I>> &&
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
+   inline constexpr bool enable_sized_sentinel_for = true;
```

[3]{.pnum} *Remarks*: Pursuant to [namespace.std], users may specialize [`disable_sized_sentinel`]{.rm} [`enable_sized_sentinel_for`]{.addu} for *cv*-unqualified non-array object types `S` and `I` if `S` and/or `I` is a program-defined type.
Such specializations shall be usable in constant expressions ([expr.const]) and have type `const bool`.

[4]{.pnum} [ *Note*: [`disable_sized_sentinel`]{.rm} [`enable_sized_sentinel_for`]{.addu} allows use of sentinels and iterators with the library that satisfy but do not in fact model `sized_sentinel_for`. — *end note*  ]

:::


Change 24.2 [ranges.syn]:

::: bq
```diff
   // [range.sized], sized ranges
   template<class>
-    inline constexpr bool disable_sized_range = false;
+    inline constexpr bool enable_sized_range = true;
```
:::

Change 24.3.9 [range.prim.size]:

::: bq
[1]{.pnum} The name `size` denotes a customization point object.
The expression `ranges​::​size(E)` for some subexpression `E` with type `T` is expression-equivalent to:

- [1.1]{.pnum} `decay-copy(extent_v<T>)` if `T` is an array type ([basic.compound]).
- [1.2]{.pnum} Otherwise, if [`disable_sized_range<remove_cv_t<T>>`]{.rm} [`enable_sized_range<remove_cv_t<T>>`]{.addu} ([range.sized]) is [`false`]{.rm} [`true`]{.addu}:

    - [1.2.1]{.pnum} `decay-copy(E.size())` if it is a valid expression and its type `I` is integer-like ([iterator.concept.winc]).
    - [1.2.2]{.pnum} Otherwise, `decay-copy(size(E))` if it is a valid expression and its type `I` is integer-like with overload resolution performed in a context that includes the declaration: [...]
:::

Change 24.4.3 [range.sized]:

::: bq
[1]{.pnum} The `sized_range` concept specifies the requirements of a `range` type that knows its size in constant time with the `size` function.

```diff
 template<class T>
   concept sized_range =
     range<T> &&
-    !disable_sized_range<remove_cvref_t<T>> &&
+    enable_sized_range<remove_cvref_t<T>> &&
     requires(T& t) { ranges::size(t); };
```
:::

and

::: bq
```diff
  template<class>
-   inline constexpr bool disable_sized_range = false;
+   inline constexpr bool enable_sized_range = true;
```
[4]{.pnum} *Remarks*: Pursuant to [namespace.std], users may specialize [`disable_sized_range`]{.rm} [`enable_sized_range`]{.addu} for *cv*-unqualified program-defined types.
Such specializations shall be usable in constant expressions ([expr.const]) and have type `const bool`.

[5]{.pnum} [ *Note*: [`disable_sized_range`]{.rm} [`enable_sized_range`]{.addu} allows use of range types with the library that satisfy but do not in fact model `sized_range`. — *end note* ]

:::

---
references:
---