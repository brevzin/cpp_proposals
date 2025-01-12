---
title: "`consteval` blocks"
document: P3289R1
date: today
audience: EWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
---

# Revision History

Since [@P3289R0], updated wording to make a consteval block distinct from a `static_assert`.

# Introduction

Several proposals that produce side effects as part of constant evaluation are in flight.  That includes [@P2996R9]{.title} and [@P2758R3]{.title}. Such a capability, in turn, quickly gives rise to the desire to evaluate such constant expressions in declarative contexts.

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

In this example, there is just a single expression statement being evaluated.  However, we are anticipating reflection code where more complex statement sequences will be used (you can see some examples in previous papers, e.g. [@P1717R0]{.title} [@P2237R0]{.title}).

We did consider other syntax variations such as

* `@eval $expression$;`
* `@ $expression$`;
* `@ $statement$`;

but found those alternatives less general and not worth the slight improvement in brevity.

# Implementation Status

Implemented in both EDG and Clang.

# Wording

[The simplest way to do the wording is to add a `consteval` block as a _kind_ of `$static_assert-declaration$`. That's the minimal diff. However, it's kind of weird to say that a `consteval` block literally is a `static_assert` - plus we need to give specific evaluation guarantees to a `consteval` block ("plainly constant-evaluated"), so we'd rather take a few more words to get somewhere that feels more sensible. Plus this change reduces a lot of duplication between `$empty-declaration$` and `$static_assert-declaration$`, which are treated the same in a lot of places anyway.]{.ednote}

Change [basic.def]{.sref}/2:

::: std
[2]{.pnum} Each entity declared by a `$declaration$` is also defined by that `$declaration$` unless:

* [2.1]{.pnum} it declares a function without specifying the function's body ([dcl.fct.def]),
* [2.2]{.pnum} [...]
* [2.13]{.pnum} it is a [`$static_assert-declaration$`]{.rm} [`$vacant-declaration$`]{.addu} ([dcl.pre]),
* [2.14]{.pnum} it is an `$attribute-declaration$` ([dcl.pre]),
* [2.15]{.pnum} [it is an `$empty-declaration$` ([dcl.pre]),]{.rm}
* [2.16]{.pnum} [...]
:::

Extend the wording for plainly constant-evaluated to allow a `consteval` block [this is adjusting wording that is added by [@P2996R9]]{.draftnote}:

::: std

[21pre]{.pnum} A non-dependent expression or conversion is _plainly constant-evaluated_ if [it is not in a complete-class context ([class.mem.general]{.sref}) and either]{.addu}

* [21.#]{.pnum} [it is the evaluating expression of a `$consteval-block-declaration$` ([dcl.pre]{.sref}), or]{.addu}
* [21.#]{.pnum} it is an initializer of a `constexpr` ([dcl.constexpr]{.sref}) or `constinit` ([dcl.constinit]{.sref}) variable [that is not in a complete-class context ([class.mem.general]{.sref})]{.rm}.

[The evaluation of a plainly constant-evaluated expression `$E$` can produce injected declarations (see below) and happens exactly once ([lex.phases]{.sref}). Any such declarations are reachable from a point that follows immediately after `$E$`.]{.note}

:::

Change [dcl.pre]{.sref}:

::: std
```diff
  $name-declaration$:
    $block-declaration$
    $nodeclspec-function-declaration$
    $function-definition$
    $friend-type-declaration$
    $template-declaration$
    $deduction-guide$
    $linkage-specification$
    $namespace-definition$
-   $empty-declaration$
    $attribute-declaration$
    $module-import-declaration$

  $block-declaration$:
    $simple-declaration$
    $asm-declaration$
    $namespace-alias-definition$
    $using-declaration$
    $using-enum-declaration$
    $using-directive$
-   $static_assert-declaration$
    $alias-declaration$
    $opaque-enum-declaration$
+   $vacant-declaration$

+ $vacant-declaration$:
+    $static_assert-declaration$
+    $empty-declaration$
+    $consteval-block-declaration$

  $static_assert-declaration$:
    static_assert ( $constant-expression$ ) ;
    static_assert ( $constant-expression$ , $static_assert-message$ ) ;

+ $consteval-block-declaration$:
+   consteval $compound-statement$
```
:::

And then after [dcl.pre]{.sref}/13:

::: std
[13]{.pnum} *Recommended practice*: When a `$static_assert-declaration$` fails, [...]

::: addu
[*]{.pnum} If a `$consteval-block-declaration$` is within a template definition, it has no effect. The evaluating expression of a `$consteval-block-declaration$` is
```cpp
[]() -> void consteval $compound-statement$ ()
```
[This expression is plainly constant-evaluated ([expr.const]).]{.note}
:::

[14]{.pnum} An `$empty-declaration$` has no effect.
:::

Adjust the grammar in [class.mem.general]{.sref} and the rule in p3:

::: std
```diff
  $member-declaration$:
    $attribute-specifier-seq$@~opt~@ $decl-specifier-seq$@~opt~@ $member-declarator-list$@~opt~@;
    $function-definition$
    $friend-type-declaration$
    $using-declaration$
    $using-enum-declaration$
-   $static_assert-declaration$
+   $vacant-declaration$
    $template-declaration$
    $explicit-specialization$
    $deduction-guide$
    $alias-declaration$
    $opaque-enum-declaration$
-   $empty-declaration$
```

[3]{.pnum} A `$member-declaration$` does not declare new members of the class if it is

* [#.#]{.pnum} a friend declaration ([class.friend]),
* [#.#]{.pnum} a `$deduction-guide$` ([temp.deduct.guide]),
* [#.#]{.pnum} a `$template-declaration$` whose declaration is one of the above,
* [#.#]{.pnum} a [`$static_assert-declaration$`,]{.rm}
* [#.#]{.pnum} a `$using-declaration$` ([namespace.udecl]) , or
* [#.#]{.pnum} [an `$empty-declaration$`.]{.rm} [a `$vacant-declaration$`.]{.addu}
:::

And similar in [class.union.anon]{.sref}/1. [This refactor allows putting in an `$empty-declaration$` into an anonymous union, which is kind of a consistency drive by with other classes.]{.ednote}

::: std
[1]{.pnum} [...] Each `$member-declaration$` in the `$member-specification$` of an anonymous union shall either define one or more public non-static data members or be a [`$static_assert-declaration$`]{.rm} [`$vacant-declaration$`]{.addu}.  [...]
:::

## Feature-Test Macro

Add to the table in [cpp.predefined]{.sref}:

::: std
```diff
  __cpp_consteval       202211L
+ __cpp_consteval_block 2025XXL
  __cpp_constinit       201907L
```
:::

---
references:
  - id: P2996R9
    citation-label: P2996R9
    title: "Reflection for C++26"
    author:
      - family: Wyatt Childers
      - family: Peter Dimov
      - family: Dan Katz
      - family: Barry Revzin
      - family: Andrew Sutton
      - family: Faisal Vali
      - family: Daveed Vandevoorde
    issued:
      - year: 2025
        month: 1
        day: 12
    URL: https://wg21.link/p2996r9
---
