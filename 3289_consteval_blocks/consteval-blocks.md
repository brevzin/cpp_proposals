---
title: "`consteval` blocks"
document: P3289R0
date: today
audience: EWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
---

# Introduction

Several proposals that produce side effects as part of constant evaluation are in flight.  That includes [@P2996R2] (“Reflection for C++26”) and [@P2758R2] (“Emitting messages at compile time”). Such a capability, in turn, quickly gives rise to the desire to evaluate such constant expressions in declarative contexts.

Currently, this effect can be shoe-horned into `static_assert` declarations, but the result looks arcane. For example, P2996 contains the following code in an example:

::: std
```cpp
#include <meta>

template<typename... Ts> struct Tuple {
  struct storage;
  static_assert(
    is_type(define_class(^storage, {data_member_spec(^Ts)...})));
  storage data;

  Tuple(): data{} {}
  Tuple(Ts const& ...vs): data{ vs... } {}
};
```
:::

Here, `define_class(...)` is a constant expression with a side-effect that we want to evaluate before parsing the member declaration that follows.  It works, but it is somewhat misleading: We’re not really trying to assert anything; we just want to force that evaluation.

We therefore propose a simple, intuitive syntax to express that we simply want to constant-evaluate a bit of code wherever a static_assert declaration could appear by enclosing that code in `consteval { ... }` (a construct we’ll call a `consteval` block).

# Proposal

Formally, we propose that a construct of the form

::: std
```cpp
consteval {
    $statement-seq$@~opt~@
}
```
:::

is equivalent to:

::: std
```cpp
static_assert(
  (
    []() -> void consteval {
      $statement-seq$@~opt~@
    }(),
    true
  )
);
```
:::

The `static_assert` condition is a *comma-expression* with the first operand an immediately-invoked `consteval` lambda.

Note that this allows a plain `return;` statement or even a `return f();` statement where `f()` has type `void. We could go out of our way to disallow that, but we cannot find any benefit in doing so.

With the feature as proposed, the example above becomes:

::: cmptable
### Status Quo
```cpp
#include <meta>

template<typename... Ts> struct Tuple {
  struct storage;
  static_assert(
    is_type(define_class(^storage,
                         {data_member_spec(^Ts)...})));
  storage data;
};
```

### Proposed
```cpp
#include <meta>

template<typename... Ts> struct Tuple {
  struct storage;
  consteval {
    define_class(^storage,
                 {data_member_spec(^Ts)...});
  }
  storage data;
};
```
:::

In this example, there is just a single expression statement being evaluated.  However, we are anticipating reflection code where more complex statement sequences will be used.

We did consider other syntax variations such as

* `@eval $expression$;`
* `@ $expression$`;
* `@ $statement$`;

but found those alternatives less general and not worth the slight improvement in brevity.

# Implementation Status

The Lock3 implementation of reflection facilities based on [@P1240R2] (and other papers) includes this feature.  The EDG front end is expected to add this feature shortly as part of its reflection extensions.

# Wording

Change [dcl.pre]{.sref}:

::: std
```diff
  $block-declaration$:
    $simple-declaration$
    $asm-declaration$
    $namespace-alias-definition$
    $using-declaration$
    $using-enum-declaration$
    $using-directive$
    $static_assert-declaration$
+   $consteval-block-declaration$
    $alias-declaration$
    $opaque-enum-declaration$

  $static_assert-declaration$:
    static_assert ( $constant-expression$ ) ;
    static_assert ( $constant-expression$ , $static_assert-message$ ) ;

+ $consteval-block-declaration$:
+   consteval { $statement-seq$@~opt~@ }
```
:::

And then after [dcl.pre]{.sref}/13:

::: std
[13]{.pnum} *Recommended practice*: When a `$static_assert-declaration$` fails, [...]

::: addu
[*]{.pnum} The `$consteval-block-declaration$`
```cpp
consteval { $statement-seq$@~opt~@ }
```
is equivalent to
```cpp
static_assert(([]() -> void consteval { $statement-seq$@~opt~@ }(), true));
```
[Such a `$static_assert-declaration$` never fails.]{.note}
:::

[14]{.pnum} An `$empty-declaration$` has no effect.
:::
