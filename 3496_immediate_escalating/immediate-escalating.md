---
title: "Immediate-Escalating Expressions"
document: P3496R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

# Introduction

This paper splits off part of [@P3032R2]. The goal of this paper is to allow this example from that paper to be valid (based also on [@P2996R7]):

::: std
```cpp
enum E { };

constexpr int f2() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r2 = f2();
    return r2;
}
```
:::

Here, `enumerators_of` is a `consteval` function. `enumerators_of(^E)` is not a constant expression (because of non-transient `constexpr` allocation). Using the original C++20 rules, that makes it ill-formed on the spot. [@P2564R3] relaxed this to allow this expression to appear in what's called an immediate-escalating function — but those are just lambdas, defaulted special members, and function templates, of which this is not one. So the call is still ill-formed.

However, the _larger_ expression `enumerators_of(^E).size()` _is_ a constant expression. The `vector` is gone, so there's no non-transient allocation issue anymore. So it's kind of weird that this program is rejected. And examples like this have come up multiple times during reflection development.

## We Already Have Exceptions

Notably, we already have two explicit cases in the wording where we have `E1` is a subexpression of `E2`, where `E1` is not a constant expression but `E2` is, where we do not reject the overall program as not being constant.

One is the most basic: naming a `consteval` function is not a constant expression, but invoking one could be. That is:

::: std
```cpp
consteval int id(int i) { return i; }

/* not constexpr */ void f(int x) {
    auto a = id;    // error
    auto b = id(1); // ok
    auto c = id(x); // error
}
```
:::

`id` isn't a constant expression, but `id(1)` is, so we allow it.

The other exception has explicit wording in [expr.const]{.sref}/17:

::: std
[17]{.pnum} An aggregate initialization is an immediate invocation if it evaluates a default member initializer that has a subexpression that is an immediate-escalating expression.
:::

Which corresponds to this example in the standard:

::: std
```cpp
struct A {
  int x;
  int y = id(x);
};

template<class T>
constexpr int k(int) {          // k<int> is not an immediate function because A(42) is a
  return A(42).y;               // constant expression and thus not immediate-escalating
}
```
:::

Here also, the call to `id(x)` internally isn't a constant expression, but `A(42)` is, so it's allowed — without having to make `k<int>` an immediate function.

# Proposal

Change the rules around what constitutes an immediate-escalating expression such that we only consider a consteval call to "bubble up" if there isn't simply a larger expression that already.

## Implementation Experience

Thanks to Jason Merrill for implementing this proposal, suggesting I split it off from [@P3032R2], and giving wording help.

Jason also pointed out that this proposal changes the meaning of an [interesting example](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2564r3.html#implementation-experience) brought up during the implementation of [@P2564R3]:

::: std
```cpp
consteval int g(int p) { return p; }
template<typename T> constexpr auto f(T) { return g; }
int r = f(1)(2);
int s = f(1)(2) + r;
```
:::

Under the original C++20 rules, `f(1)` is ill-formed already. [@P2564R3] allowed `f<int>` to become implicitly `consteval`, which allowed the initialization of `r` because we're doing tentative constant evaluation. But the initialization of `s` was still-formed because `f(1)` itself isn't a constant expression (in the same way that `enumerators_of(^E)` is not).

But with this proposal, because `f(1)(2)` is a constant expression — both initializations are valid.

## Wording

The wording around [expr.const]{.sref}/17 currently reads:

::: std
::: rm
[17]{.pnum}  An invocation is an *immediate invocation* if it is a potentially-evaluated explicit or implicit invocation of an immediate function and is not in an immediate function context.
An aggregate initialization is an immediate invocation if it evaluates a default member initializer that has a subexpression that is an immediate-escalating expression.

[18]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and it is either

* [#.#]{.pnum} a potentially-evaluated id-expression that denotes an immediate function that is not a subexpression of an immediate invocation, or
* [#.#]{.pnum} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.
:::
:::

Replace that entirely with (note that *consteval-only* will be extended by [@P2996R7] to include consteval-only types):

::: std
::: addu
[17]{.pnum} An expression is *consteval-only* if it is:

* [#.#]{.pnum} an *id-expression* naming an immediate function, or
* [#.#]{.pnum} an explicit or implicit call to an immediate function.

[18]{.pnum} An expression is *immediate-escalating* if it is a consteval-only expression unless:

* [#.#]{.pnum} it is initially in an immediate function context, or
* [#.#]{.pnum} it either is a constant expression or is a subexpression of a constant expression.

[19]{.pnum} An expression is an *immediate invocation* if:

* [#.#]{.pnum} it either is consteval-only or has a subexpression that is consteval-only,
* [#.#]{.pnum} it is not in an immediate function context, and
* [#.#]{.pnum} it is a constant expression.
:::
:::