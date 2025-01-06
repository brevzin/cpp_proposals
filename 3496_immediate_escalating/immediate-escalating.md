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

This paper splits off part of [@P3032R2]. The goal of this paper is to allow this example from that paper to be valid (based also on [@P2996R8]):

::: std
```cpp
enum E { a1, a2, a3 };

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

## We Already Have Special Cases

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

Change the rules around what constitutes an immediate-escalating expression such that we only consider a consteval call to "bubble up" if it isn't already enclosed in a constant expression.

Examining the original example in the context of the terms that this paper will introduce/adjust in the wording:

::: std
```cpp
constexpr int f2() {
    return enumerators_of(^E).size();
}
```
:::

In the expression `enumerators_of(^E).size()` (which is not in an immediate function context):

* `enumerators_of` is a consteval-only expression (because it names an immediate function)
* `^E` will also be a consteval-only expression (because it has consteval-only type)
* the call expression `enumerators_of(^E)` is a consteval-only expression (because it has an immediate subexpression that is consteval-only). It is not an immediate invocation because it is not a constant expression.
* the larger expression `enumerators_of(^E).size()` _is_ a constant expression, has a non-constant subexpression that is consteval-only, and is thus an immediate invocation. This makes `enumerators_of(^E)` no longer an immediate-escalating expression (which it is in the status quo).

The intent of this proposal is that it is a DR against [@P2564R3].

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

Under the original C++20 rules, `f<int>` is ill-formed already. [@P2564R3] allowed `f<int>` to become implicitly `consteval`, which allowed the initialization of `r` because we're doing tentative constant evaluation and the initializer is a constant expression. But the initialization of `s` was still ill-formed because `f(1)` itself isn't a constant expression (in the same way that `enumerators_of(^E)` is not) and the full initializer is also not a constant expression.

But with this proposal, because `f(1)(2)` is a constant expression — both initializations are valid.

## Wording

Replace the wording in [expr.const]{.sref}/17-18 (note that *consteval-only* will be extended by [@P2996R8] to include consteval-only types and conversions to and from them):

::: std
::: rm
[17]{.pnum}  An invocation is an *immediate invocation* if it is a potentially-evaluated explicit or implicit invocation of an immediate function and is not in an immediate function context.
An aggregate initialization is an immediate invocation if it evaluates a default member initializer that has a subexpression that is an immediate-escalating expression.

[18]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and it is either

* [#.#]{.pnum} a potentially-evaluated id-expression that denotes an immediate function that is not a subexpression of an immediate invocation, or
* [#.#]{.pnum} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.
:::

::: addu
[17]{.pnum} An expression is *consteval-only* if it directly names ([basic.def.odr]) an immediate function.

::: draftnote
The intent of this wording is that given:
```cpp
consteval int f(int i) { return i; }
consteval int g(int i) { return i + 1; };
```
in the expression `f(x) + g(y)`, the subexpressions `f(x)` and `g(y)` are consteval-only (they directly name `f` and `g`, respectively) but the whole expression is not. If this wording doesn't accomplish that, we'll have to come up with something more specific. But the idea is we identify which kernels are consteval-only, then start bubbling up until we find an immediate invocation or an immediate-escalating function.

Also regardless, we need to introduce a term here because with [@P2996R8], we'll add consteval-only types to the mixture.
:::

[18]{.pnum} An expression is an *immediate invocation* if:

* [#.#]{.pnum} it is a constant expression,
* [#.#]{.pnum} it is not in an immediate function context, and
* [#.#]{.pnum} one of its immediate subexpressions is a consteval-only expression that is not a constant expression.

[18+]{.pnum} An expression is *immediate-escalating* if it is a consteval-only expression that is not a subexpression of an immediate invocation.
:::

[19]{.pnum} An *immediate-escalating* function is

* [#.#]{.pnum} the call operator of a lambda that is not declared with the `consteval` specifier,
* [#.#]{.pnum} a defaulted special member function that is not declared with the `consteval` specifier, or
* [#.#]{.pnum} a function that results from the instantiation of a templated entity defined with the `constexpr `specifier.

An immediate-escalating expression shall appear only in an immediate-escalating function
:::

And extend the example:

::: std
::: example9
```diff
  struct A {
    int x;
    int y = id(x);
  };

  template<class T>
  constexpr int k(int) {          // k<int> is not an immediate function because A(42) is a
    return A(42).y;               // constant expression and thus not immediate-escalating
  }

+ struct unique_ptr {
+    int* p;
+    constexpr unique_ptr(int i) : p(new int(i)) { }
+    constexpr ~unique_ptr() { delete p; }
+    constexpr int deref() const { return *p; }
+ };
+
+ consteval unique_ptr make_unique(int i) {
+     return unique_ptr(i);
+ }
+
+ constexpr int overly_complicated() {
+   return make_unique(121).deref(); // OK, make_unique(121) is consteval-only but it is not
+                                    //     immediate-escalating because make_unique(121).deref()
+                                    //     is a constant expression and thus an immediate invocation.
+
+ }
+
+ static_assert(overly_complicated() == 121);
```
:::
:::

## Feature-Test Macro

Bump `__cpp_consteval` in [cpp.predefined]{.sref}:

::: std
```diff
- __cpp_­consteval @[202211L]{.diffdel}@
+ __cpp_­consteval @[20XXXXL]{.diffins}@
```
:::

# Acknowledgements

Thanks to Jason Merrill for the implementation and help with the wording. Thanks to Dan Katz, Tim Song, and Daveed Vandevoorde for other discussions around the design and wording.