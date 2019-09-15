---
title: Rename `disable_sized_range` to `enable_sized_range`
document: D1871R1
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

If we make it positive, that is `enable_sized_range`, and adopt [@P1870R0], then we well have three opt-in concepts which come with type traits named `enable_FOO`.

# Proposal

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
    - id: P1870R0
      citation-label: P1870R0
      title: "_`forwarding-range`_`<T>` is too subtle"
      author:
        - family: Barry Revzin
      issued:
        - year: 2019
      URL: https://wg21.link/p1870r0
---