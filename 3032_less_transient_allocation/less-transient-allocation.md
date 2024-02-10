---
title: "Less transient constexpr allocation"
document: P3032R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

# Introduction

C++20 introduced constexpr allocation, but in a limited form: any allocation must be deallocated during that constant evaluation.

The intent of the rule is that no constexpr allocation persists to runtime. For more on why we currently need to avoid that, see Jeff Snyder's [@P1974R0] and also [@P2670R1].

But the rule cited above does slightly more than prevent constexpr allocation to persist until runtime. The goal of this paper is to allow more examples of allocations that _do not_ persist until runtime, that nevertheless are still rejected by the C++23 rules.

For the purposes of this paper, we'll consider the example of wanting to get the number of enumerators of a given enumeration. While the specific example is using reflection ([@P2996R1]), there isn't anything particularly reflection-specific about the example - it just makes for a good example. All you need to know about reflection to understand the example is that `^E` gives you an object of type `std::meta::info` and that this function exists:

::: bq
```cpp
namespace std::meta {
  using info = /* ... */;
  consteval vector<info> enumerators_of(info);
}
```
:::

With that, let's go through several attempts at trying to get the _number_ of enumerators of a given enumeration `E` as a constant:

<table>
<tr><th/><th>Attempt</th><th>Result</th></tr>
<tr><td style="text-align: center;vertical-align: middle">1</td><td>
```cpp
int main() {
    constexpr int r1 = enumerators_of(^E).size();
    return r1;
}
```
</td><td>✅. This one is valid - because `r1` is a `constexpr` variable, it's initializer starts a constant evaluation - which includes the entire expression.
The temporary `vector` is destroyed at the end of that expression, so it doesn't persist outside of any constant evaluation.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">2</td><td>
```cpp
constexpr int f2() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r2 = f2();
    return r2;
}
```
</td><td>❌. This one is invalid.

The same idea about initializing `r2` as mentioned in the previous example is valid - but because `f2` is a `constexpr` function, the invocation of `enumerators_of(^E)` is not in an immediate function context - so it needs to be a constant expression on its own. That is, we start a new constant evaluation within the original one - but this constant evaluation is just the expression `enumerators_of(^E)`. It is not the full expression `enumerators_of(^E).size()`.

As a result, the temporary vector returned by `enumerators_of(^E)` persists outside of its constant expression in order to invoke `.size()` on it, which is not allowed.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">3</td><td>
```cpp
consteval int f3() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r3 = f();
    return r3;
}
```
</td><td>✅. Both this row and the next row are subtle refinements of the second row that make it valid.

The only difference between this and the previous row is that `f2` was `constexpr` but `f3` is `consteval`. This distinction matters, because now `enumerators_of(^E)` is no longer an immediate invocation - it is now in an immediate function context. As a result, the only thing that matters is that the entire expression `enumerators_of(^E).size()` is constant - and the temporary `vector<info>` does not persist past that.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">4</td><td>
```cpp
template<class E> constexpr int f4()
{
    return enumerators_of(^E).size();
}

int main()
{
    constexpr int r4 = f4<E>();
    return r4;
}
```
</td><td>✅. Here `f4` is a `constexpr` function *template*, whereas `f2` was a regular `constexpr` function. This matters because of [@P2564R3] - the fact that `enumerators_of(^E).size()` isn't a constant expression now causes `f4` to become a `consteval` function template - and thus we're in the same state that we were in `f3`: it's not `enumerators_of(^E)` that needs to be a core constant expression but rather all of `enumerators_of(^E).size()`.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">5</td><td>
```cpp
consteval int f5() {
    constexpr auto es = enumerators_of(^E);
    return es.size();
}

int main() {
    constexpr int r5 = f();
    return r5;
}
```
</td><td>❌. Even though `f5` is `consteval`, we are still explicitly starting a new constant evaluation within `f5` by declaring `es` to be `constexpr`. That allocation persists past that declaration - *even though* it does not persist past `f5`, which by being `consteval` means that it does not persist until runtime.
</td></tr>
</table>

Three of these rows are valid C++23 programs (modulo the fact that they're using reflection), but `2` and `5` are invalid - albeit for different reasons:

* in the 2nd row, we are required to consider `enumerators_of(^E)` as a constant expression all by itself - even if `enumerators_of(^E).size()` is definitely a constant expression.
* in the 5th row, we are required to consider `es` as a non-transient constexpr allocation - even though it definitely does not persist until runtime, and thus does not actually cause any of the problems that non-transient constexpr allocation has to address.

## Immediate-escalating expressions

The wording in [@P2564R3] introduced the term *immediate-escalating expression* in [expr.const]{.sref}:

::: bq
[17]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and it is either

* [17.1]{.pnum} a potentially-evaluated *id-expression* that denotes an immediate function that is not a subexpression of an immediate invocation, or
* [17.2]{.pnum} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.
:::

In the second example:

::: bq
```cpp
constexpr int f2() {
    return enumerators_of(^E).size();
}
```
:::

The expression `enumerators_of(^E)` is immediate-escalating - it is an immediate invocation (`enumerators_of` is a `consteval` function) that is not a constant expression (because the temporary vector persists outside of this expression). This is what causes `f4` to become a `consteval` function template.

But `enumerators_of(^E).size()` is not an immediate invocation (it simply has a subexpression that is an immediate invocation). However, if we were to define it as an immediate invocation  - then it would not be an immediate-escalating expression anymore because it is actually a constant expression. And that would be enough to fix this example (as well as `f4` which would then itself not escalate to `consteval` since it wouldn't need to).

Put differently: instead of escalating `enumerators_of(^E)` up to the nearest function, which we then try to make `consteval` (and fail in the case of `f2` because `constexpr` functions are not *immediate-escalating*), we only need to escalate up to the nearest enclosing expression that could be a constant expression.

## Transient allocations

The wording in [expr.const]{.sref} for rejecting non-transient allocations rejects an expression `E` as being a core constant expressions if `E` evaluates:

::: bq
* [5.18]{.pnum} a *new-expression* ([expr.new]), unless the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is deallocated within the evaluation of `E`;
* [5.20]{.pnum} a call to an instance of `std​::​allocator<T>​::​allocate` ([allocator.members]), unless the allocated storage is deallocated within the evaluation of `E`;
:::

That is - an allocation within `E` has to be transient to `E`. However, the rule we really want is that a constant allocation is transient to constant evaluation. In the fifth example:

::: bq
```cpp
consteval int f5() {
    constexpr auto es = enumerators_of(^E);
    return es.size();
}
```
:::

The allocation in `enumerators_of(^E)` isn't transient to that expression, but it is definitely destroyed within `f5`, which is `consteval`. That's important: if `f5` were `constexpr`, we'd have access to that allocation at runtime.

We can loosen the restriction such that an allocation within `E` must be deallocated within `E` or, if `E` is in an immediate function context, the end of that context. This would be the end of the `if consteval { }` block or the end of the `consteval` function. Such a loosening would allow `f5` above, but not if it's `constexpr`, and not if `es` were also declared `static`.


---
references:
  - id: P2747R1
    citation-label: P2747R1
    title: "`constexpr`` placement new"
    author:
      - family: Barry Revzin
    issued:
      - year: 2023
    URL: https://wg21.link/p2747r1
---
