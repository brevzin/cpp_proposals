---
title: "`std::iter_swap` should be constrained"
document: P2641R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tags: ranges
---

# Introduction

[@P2602R1] seeks to relax the poison pills that currently exist for the customization point objects in the standard library, since they fail to reject invalid code but succeed in rejecting valid code. In all cases save for one. That paper preserves the poison pill for `iter_swap` because `std::iter_swap` is unconstrained.

`std::iter_swap` is currently specified as, in [alg.swap]{.sref}/6-7:

::: bq
```cpp
template<class ForwardIterator1, class ForwardIterator2>
  constexpr void iter_swap(ForwardIterator1 a, ForwardIterator2 b);
```
[6]{.pnum} *Preconditions*: `a` and `b` are dereferenceable. `*a` is swappable with ([swappable.requirements]) `*b`.

[7]{.pnum} *Effects*: As if by `swap(*a, *b)`.
:::

But... why is `std::iter_swap` unconstrained?

`std::swap` used to be unconstrained as well. It was `std::swap` specifically being unconstrained that led to the design of poison pills to begin with [@P0370R3]. Nevertheless, Ranges TS constrained `std::swap` [@P0021R0] and then it was constrained in the standard library [@P0185R1]. But none of these papers so far as mention `std::iter_swap`, which is seems like it should have similar behavior. Indeed, the Ranges TS looks like it even at some point wanted to remove `std::iter_swap`.

Is there a reason to avoid constraining `std::iter_swap`? Any code that currently checks for the validity of `std::iter_swap(a, b)` is meaningless, and would become meaningful. Code that currently calls `std::iter_swap` (qualified or not) would continue to work fine. It's oddly inconsistent with `std::swap` today.

## What should `std::iter_swap`'s constraints be?

Ranges brings with it a hierarchy of concepts for iterators. That library tends to be pretty picky in what it requires iterators to provide -- too picky for what we can do for `std::iter_swap`, since we don't want to reject valid code.

For example:

::: bq
```cpp
struct X {
  auto operator*() /* not const */ -> int&;
};

void old_iter_swap(X& a, X& b) {
  std::iter_swap(a, b); // compiles and works
}

void new_iter_swap(X& a, X& b) {
  std::ranges::iter_swap(a, b); // error
}
```
:::

The issue here is that `std::ranges::iter_swap` requires `*x` to be a const operation. Which it probably should be, but iterators tend to not be `const` (they are mutated and often passed by value) so it's an easy `const` to just forget. And that code needs to not suddenly break.

But we don't actually have to do anything fancy. `std::iter_swap(x, y)` just calls `swap(*x, *y)` (NB: unqualified). `std::swap` is now properly constrained, so checking the validity of that expression is meaningful.

Note that for the type `X` above, `std::iter_swap(a, b)` would remain valid (since `swap(*a, *b)` is valid) and `std::ranges::iter_swap(a, b)` would remain invalid (because argument dependent lookup for `iter_swap` wouldn't find `std::iter_swap`, so the rest remains the same). But a hypothetical `X<std::byte>` (or `std::optional<int>`), with otherwise the same behavior, would suddenly allow `std::ranges::iter_swap(a, b)` - because now `std::iter_swap` would be found by ADL without the poison pill to reject it.

Does that... matter? Most of the code that uses `std::ranges::iter_swap` is in a context in the standard library where it is invoked with two `input_iterator`s. Types like `std::optional<int>` aren't iterators, so they'd have been rejected long before we get to this point.

If there's harm in allowing this case, then this paper should be rejected. If it's fine to allow, then this paper is fine.

# Wording

The wording of this paper is based on [@P2602R1].

Add a constraint to [alg.swap]{.sref}:

::: bq
```cpp
template<class ForwardIterator1, class ForwardIterator2>
  constexpr void iter_swap(ForwardIterator1 a, ForwardIterator2 b);
```
::: addu
[*]{.pnum} *Constraints*: `swap(*a, *b)` is well-formed.
:::

[6]{.pnum} *Preconditions*: `a` and `b` are dereferenceable. `*a` is swappable with ([swappable.requirements]) `*b`.

[7]{.pnum} *Effects*: As if by `swap(*a, *b)`.
:::

Change [iterator.cust.swap]{.sref}:

::: bq
* [4.1]{.pnum} `(void)iter_­swap(E1, E2)`, if either `E1` or `E2` has class or enumeration type and `iter_­swap(E1, E2)` is a well-formed expression [with overload resolution performed in a context that includes the declaration]{.rm}

::: rm
```
template<class I1, class I2>
  void iter_swap(I1, I2) = delete;
```
:::

[and does not include a declaration of `ranges​::​iter_­swap`]{.rm} [where `iter_swap` undergoes argument dependent lookup]{.addu}. If the function selected by overload resolution does not exchange the values denoted by `E1` and `E2`, the program is ill-formed, no diagnostic required. [*Note*: [This precludes calling unconstrained `std::iter_swap`. When the deleted overloads are viable, program-defined overloads need to be more specialized ([temp.func.order]) or more constrained ([temp.constr.order]) to be used.]{.rm} [Ordinary unqualified lookup is not performed.]{.addu} - *end note*]
:::

## Feature-Test Macro

Bump the value for `__cpp_lib_ranges` in [version.syn]{.sref}:

::: bq
```diff
- #define __cpp_­lib_­ranges @[202202L]{.diffdel}@
+ #define __cpp_­lib_­ranges @[2022XXL]{.diffins}@
  // also in <algorithm>, <functional>, <iterator>, <memory>, <ranges>
```
:::

Ideally this paper is either passed together along with [@P2602R1] or rejected entirely to make this easier.
