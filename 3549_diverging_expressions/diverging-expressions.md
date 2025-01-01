---
title: "Diverging Expressions"
document: P3549R0
date: today
audience: EWG
author:
    - name: Bruno Cardoso Lopes
      email: <bruno.cardoso@gmail.com>
    - name: Zach Laine
      email: <whatwasthataddress@gmail.com>
    - name: Michael Park
      email: <mcypark@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

One pattern that will occur with some regularity with pattern matching [@P2688R4] (and `do` expressions [@P2806R2]) is the desire to produce values for some patterns but not for all cases. Consider the following simplified example:

<table>
<tr><td>
```cpp
void f(int i) {
  int j = i match {
    0 => 0;
    _ => std::terminate();
  };
  use(j);
}
```
</td>
<td>
```cpp
void g(int i) {
  int j = i match {
    0 => 0;
    _ => do {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
<td>
```cpp
void h(int i) {
  int j = i match {
    0 => 0;
    _ => do -> int {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
</tr>
</table>

In all of these cases, the desire is that if `i == 0` then we initialize `j` to the value `0`, otherwise we `std::terminate()`. Given that `std::terminate` does in fact, as the name suggests, terminate, we don't actually have to worry about producing a value in that case. These examples are all equivalent — or at least, they should be. But C++ does not currently recognize them as such (with one exception that we'll get to later).

The rule for pattern matching by default is that the type of the match expression is deduced from each arm and each arm has to have the same type. The same rule we have for `auto` deduction in functions and lambdas. In the above examples, the first arm always has type `int`. But for `f()` and `g()`, the second arm has type `void`. In order to type check, we have to wrap our call to `std::terminate` in an expression that actually has type `int`. The `do` expression in `h()` is one such way.

The extra `-> int` does solve the problem, but it's also misleading. We're not actually ever producing an `int` here, we're just writing a type annotation to hammer the compiler into submission. That's just not a great place to be.

Our goal with this paper is to have all of `f()`, `g()`, and `h()` above type check. Concretely: we need to recognize that an expression can _diverge_ and then have pattern matching's type deduction rules only consider those arms that do not diverge. Importantly, we want to treat the expressions:

::: std
```cpp
std::terminate()
```
:::

and

::: std
```cpp
do { std::terminate(); }
```
:::

and

::: std
```cpp
do { log::error("hasta la vista"); std::terminate(); }
```
:::

as all being diverging expressions in order to facilitate code evolution. It would be annoying if a naked call to `std::terminate` worked but once you wrap it in a `do` expression that does so much as add a single log statement that it suddenly doesn't. We want to properly recognize divergence, not just the most trivial of cases.

# Diverging Expressions

An expression is said to _diverge_ if it unconditionally escapes control flow. Currently in C++, we have two such expressions in the language:

* a `$throw-expression$`, and
* a call to a `[[noreturn]]` function.

The former explicitly diverges (control flow immediately escapes and doesn't return to keep evaluating in the original slot) while the latter implicitly diverges (technically a function marked `[[noreturn]]` can still return, we just state that doing so is undefined behavior in [dcl.attr.noreturn]{.sref}).

As hinted at earlier, the language already recognizes that expressions can diverge in one spot: the conditional operator. Consider:

::: std
```cpp
int x = $condition$ ? 42 : throw std::runtime_error("oops"); // OK
```
:::

The conditional operator has to merge two values into one. But if one of the operands is a `$throw-expression$`, then the value of the conditional operator is trivially the other operand — or there is just no value to speak of. The above is perfectly valid code, the conditional expression is a prvalue `int` (that sometimes throws).

The pattern matching paper recognizes this as well, and explicitly recognizes pattern arms that are `$throw-expression$`s as not participating in deduction either:

::: std
```cpp
int x = v match { // OK
  0 => 0;
  _ => throw std::runtime_error("oops");
};
```
:::

But this does not work today, despite being just as diverging as a `throw`:

::: std
```cpp
int x = $condition$ ? 42 : std::terminate();
```
:::

Instead you could do something like this (which we think is the kind of thing that really belongs on a T-shirt):

::: std
```cpp
int x = $condition$ ? 42 : throw (std::terminate(), 0);
```
:::

Note that this _does_ work — we will terminate before we evaluate the `throw`.

But there's not any difference between throwing an exception and invoking a non-returning function when we're talking about whether an expression produces a value. Neither produces a value, so neither needs to participate in any language rules that involve merging multiple expressions!

We think it's important to recognize invocations of non-returning functions as diverging because we want people to be able to write the code on the left, and not have to write either of the two workarounds presented:

<table>
<tr><th>Desired</ht><th>Workaround with `throw`</th><th>Workaround with explicit type</th></tr>
<tr><td>
```cpp
void f(int i) {
  int j = i match {
    0 => 0;
    _ => std::terminate();
  };
  use(j);
}
```
</td>
<td>
```cpp
void g(int i) {
  int j = i match {
    0 => 0;
    _ => throw (std::terminate(), 0);
  };
  use(j);
}
```
</td>
<td>
```cpp
void h(int i) {
  int j = i match {
    0 => 0;
    _ => do -> int {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
</tr>
</table>

## Diverging `do` expressions

Consider the following expressions:

1. `throw 42`
2. `std::terminate()`
3. `do { throw 42; }`
4. `do { std::terminate(); }`

All four of these expressions diverge, the bottom two just trivially wrap the top two. So we'd want these to have the same properties when it comes to divergence — otherwise any slight refactoring could have too much impact. For instance, the desire to change:

::: std
```cpp
std::terminate()
```
:::

into

::: std
```cpp
do {
    log::fatal("oops, I did it again");
    std::terminate();
}
```
:::

should ideally not change any properties of the expression. Both diverge, one just additionally adds some logging.

One question we have to answer is: What is the `decltype` of such an expression?

Right now, `decltype(throw 42)` is defined as `void` [explicitly](https://eel.is/c++draft/expr.throw#1). For some reason. `decltype(std::terminate())` is more obviously `void` simply because that's how the function is defined — it is a `void()` (although a `[[noreturn]]` one). So there's certainly something to be said for going ahead and wanting to define `decltype(do { throw 42; })` as `void` as well.

This approach leads to the following pair of rules:

1. the _type_ of a `do` expression is either
    * the _trailing-return-type_, if one is explicitly provided, or
    * the type that is `do_return`-ed consistently (same as the lambda rule except with `do_return` instead of `return`).
2. A `do` expression is said to _diverge_ if every control flow path leads to executing a diverging expression (i.e. a `$throw-expression$`, a call to a `[[noreturn]]` function, or evaluating another such diverging expression).

There is another approach though.

## A bottom type

Several languages have a notion of a bottom type (`⊥`) for diverging expressions. This type is spelled `Nothing` in Scala, `never` in TypeScript, `Never` in Python, `noreturn` in D, `!` in Rust, `void` in Haskell (but notably not `void` in C++), etc. This type represents an expression that diverges.

Let's imagine what it would look like if C++ also had such a type. We'll call it `noreturn_t`. It would some interesting properties:

* You cannot produce an instance of such a type. It wouldn't be very divergent otherwise! In this case, it's not just that this type isn't constructible, but we would also have to eliminate all of C++'s other clever ways of producing a value (like `reinterpret_cast`).
* On the other hand, `noreturn_t` is convertible to any other type (including reference types). This also would include function pointer conversions — `auto(*)(T) -> noreturn_t` is convertible to `auto(*)(T) -> U` for all `U`. This makes sense from a type-theoretic perspective and makes a lot of other uses just work.

We would then change the type of a `$throw-expression$` to be `noreturn_t` (instead of `void`). This change, coupled with the conversion rule above, means we'd no longer need the special case in conditional expressions. Consider:

::: std
```cpp
$condition$ ? 42 : throw std::runtime_error("oops")
```
:::

The types of the second and third operand are `int` and `noreturn_t`, respectively. `noreturn_t` is convertible to `int` (because it is convertible to anything), but `int` is not convertible to `noreturn_t` (since nothing is), thus the result of the expression has type `int`, as desired.

We would also want to change the signatures of all of our never-returning functions to have this in their types:

::: std
```diff
namespace std {
- [[noreturn]] auto terminate() -> void;
+              auto terminate() -> noreturn_t;
}
```
:::

Which would likewise allow for the desired conditional with `std::terminate()` to work for the same reasons as the conditional with `throw`:

::: std
```cpp
$condition$ ? 42 : std::terminate()
```
:::

## Doing Our Due Diligence to Deduce `do` Divergence

Consider the following pattern arms:

::: std
```cpp
@*pattern*~1~@ => do { std::println("never gonna give you up"); throw 2; };
@*pattern*~2~@ => do { std::println("never gonna let you down"); std::terminate(); };
@*pattern*~3~@ => do { std::println("never gonna run around"); do_return std::terminate(); };
@*pattern*~4~@ => do { std::println("and desert you"); continue; };
@*pattern*~5~@ => break;
```
:::

The last case here is simple — we can special case escaping statements, and the pattern matching paper already does this.

The first four cases though also all unconditionally diverge. We know that from simply examining the code. But what is the specific language rule by which we could ensure that the pattern match expression can exclude these arms when deducing the type? In `@*pattern*~3~@`, we actually have a `do_return` statement, so the `do` expression there could straightforwardly be said to have type `noreturn_t` and thus diverge. But the other three have no `do_return` and thus would, by default, end up having type `void` — which we do not want. So what do we do?

One option is to copy the logic from some other languages (like Rust) and also the initial GCC statement-expression model and have an implicit value from `do` expressions. That is, the other three patterns above implicitly take their values from `throw 2`, `std::terminate()`, and `continue` respectively — which all have type `noreturn_t`. So this just works. But the reason we didn't initially want to do this for `do` expressions was that we don't already have a way to turn `if` statements and `for`/`while` statements into expressions — which makes early returns challenging. From the gcc doc example:

::: cmptable
### gcc
```cpp
({
    int y = foo();
    int z;
    if (y > 0) z = y;
    else z = -y;
    z;
})
```

### Proposed
```cpp
do {
    int y = foo();
    if (y > 0) {
        do_return y;
    } else {
        do_return -y;
    }
}
```
:::


With implicit last value (or, furthermore, with an `if` expression which operates under implicit last value rules):

::: cmptable
### Explicit `do_return`
```cpp
do {
    int y = foo();
    if (y > 0) {
        do_return y;
    } else {
        do_return -y;
    }
}
```

### Implicit last value
```cpp
do {
    int y = foo();
    if (y > 0) {
        do_return y;
    }
    -y;
}
```

### An `if` expression
```cpp
do {
    int y = foo();
    if (y > 0) {
        y;
    } else {
        -y;
    }
}
```
:::

But we're concerned that the mixing and matching of explicit and implicit yields would be confusing. It's easy to miss the implicit `do_return` in the presence of explicit ones. Likewise, attempting to turn `if` into an expression this late in C++'s lifetime might be too novel? We would also then have to find a way to turn loops into expressions (or resurface `do_return`, again running into the explicit/implicit issue).

So if we don't want to use implicit last value, how else can we deduce divergence? We suggest the following, simple rule, inspired by implicit last value:

::: std
[1]{.pnum} A `$statement$` is a *diverging statement* if it is:

* [1.1]{.pnum} A `$compound-statement$` where the last statement is a diverging statement,
* [1.2]{.pnum} an `$escaping-statement$`,
* [1.3]{.pnum} a `$statement-expression$` whose `$expression$` is a diverging expression, or
* [1.4]{.pnum} an `if` statement with an `else` branch, where both substatements are diverging statements.
* [1.5]{.pnum} a constexpr `if` statement where the taken substatement is a diverging statement.

[2]{.pnum} The type of a `$do-expression$` is determined as follows.

* [2.1]{.pnum} If there is a `$trailing-return-type$` that is not a placeholder, that type.
* [2.2]{.pnum} Otherwise, let `T` be the type deduced from the non-discarded `do_return` statements, if any, within the body of the `$do-expression$`. If the type deduced is not the same in each deduction, the program is ill-formed.
    * [2.2.1]{.pnum} If `T` is `void` and the last `$statement$` is a diverging statement, then type of the `$do-expression$` is `noreturn_t`.
    * [2.2.2]{.pnum} Otherwise, the type is `T`.

[3]{.pnum} An expression is a *diverging expression* if its type is `noreturn_t`.
:::

We need to deduce `void` because if there are any `do_return`s that yield a value, then the expression does not diverge. Then, in the `void`-returning cases, we need to see which ones of those actually diverge. Which is non-trivial because we need to handle things like:

::: std
```cpp
// just an escaping-statement
do { continue; }

// a statement-expression that is a diverging expressoin
do { std::terminate(); }

// an if statement with just an if, this is NOT diverging
// because if the $condition$ is false, then we're just a void expression
do { if ($condition$) { throw 1; } }

// but this one DOES diverge (just differently in both branches)
do { if ($condition$) { continue; } else { break; } }

// this diverges if the condition is true
do { if constexpr ($condition$) { std::terminate(); } }
```
:::

This rule ensures that all of our initial pattern arms diverge, as desired:

::: std
```cpp
@*pattern*~1~@ => do { std::println("never gonna make you cry"); throw 2; };
@*pattern*~2~@ => do { std::println("never gonna say goodbye"); std::terminate(); };
@*pattern*~3~@ => do { std::println("never gonna tell a lie"); do_return std::terminate(); };
@*pattern*~4~@ => do { std::println("and hurt you"); continue; };
@*pattern*~5~@ => break;
```
:::

## Alternative with `[[noreturn]]`

The advantage of the approach described with `noreturn_t` is that diverging expressions can appear in the type system. This obsoletes `[[noreturn]]`, providing a better way to express the issue — in a way that can further generalize to other scenarios. We can have a `std::function<std::noreturn_t()>`, we can have `std::expected<T, std::noreturn_t>` (a specialization that would not require additional storage for the error type or the discriminant, since it would always be a value), etc.

But an alternative approach would be to avoid changing any of the existing functions (like `std::abort()`, `std::terminate()`, etc.) or changing the type of a `$throw-expression$`, and instead recognize `[[noreturn]]` more explicitly.

The rule we lay out above would instead become:

::: std
[1]{.pnum} A `$statement$` is a *diverging statement* if it is:

* [1.1]{.pnum} A `$compound-statement$` where the last statement is a diverging statement,
* [1.2]{.pnum} an `$escaping-statement$`,
* [1.3]{.pnum} a `$statement-expression$` whose `$expression$` is a diverging expression, or
* [1.4]{.pnum} an `if` statement with an `else` branch, where both substatements are diverging statements.
* [1.5]{.pnum} a constexpr `if` statement where the taken substatement is a diverging statement.

[2]{.pnum} An expression is a *diverging expression* if:

* [2.1]{.pnum} it is an invocation of a `[[noreturn]]` function,
* [2.2]{.pnum} it is a `$throw-expression$`, or
* [2.3]{.pnum} it is a `$do-expression$` whose type is `void` and whose last statement is a diverging statement.
:::

This is a simpler change. It doesn't involve introducing a new language/library type or changing existing standard library functions or the meanings of some code — although we doubt too many people are relying on `decltype(throw 1)` being `void`. However, it doesn't compose. `[[noreturn]]` isn't deduced, so wrapping becomes challenging.

Although in order to making wrapping work, we'd have to further change deduction rules so that `[]{ std::abort(); }` returns `noreturn_t` in the same way that `do { std::abort(); }` does. But without `noreturn_t`, you'd have to write `[][[noreturn]]{ std::abort(); }`.

Leaning on `[[noreturn]]` also means that we end up leaning on `void` even harder to mean two completely different things: `void f() { }` is a function that returns a value, while `[[noreturn]] void g() { std::exit(-1); }` is a function that doesn't.

# Proposal

We propose to:

* to introduce a new type `std::noreturn_t` which represents an expression with no value, that unconditionally diverges.
  * `std::noreturn_t` is convertible to any type
  * `std::noreturn_t` is not constructible, and you cannot `reinterpret_cast` into it.
  * `std::noreturn_t(*)(Args...)` is convertible to `R(*)(Args...)` for all `R`.
* change all the standard library functions that are currently `[[noreturn]] void f()` to instead be `std::noreturn_t f()`
* change the type of `$throw-expression$` to `std::noreturn_t` and remove the current... uh... exception for exceptions in the conditional operator (it will be subsumed by the usual convertibility rule).
* change function deduction rules to recognize diverging statements and to deduce `std::noreturn_t` in cases where they currently deduce `void`
* adopt the same rules for `do` expressions [@P2806R2].