---
title: "`consteval` variables"
document: P4339R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
toc: true
status: progress
tag: constexpr
---

# Proposal

[@P4101R1]{.title} introduced support for _implicit_ `consteval` variables — variables that only exist during compile-time in a way that is enforced by the language.

This paper proposes support for _explicit_ `consteval` variables. This is valuable simply for visibility. But another important benefit of `consteval` variables is that they are _guaranteed_ to not occupy space at runtime. You just don't hit issues [like this](https://www.reddit.com/r/cpp/comments/1i36ahd/is_this_an_msvc_bug_or_am_i_doing_something_wrong/). `constexpr` variables, even if never accessed at runtime, may occupy space anyway. It's just QoI. But in the same way that `consteval` functions _cannot_ lead to codegen, `consteval` variables _cannot_ either. That's a pretty nice benefit.

With [@P4101R1], this is a very small change, since the notion of `consteval` variable already exists — and implementations already have to track this to properly enforce the rules. So this proposal is a small amount of implementation (mostly just allowing `consteval` on variable declarations) and wording.

## Mutability

We proposal that a variable declared `consteval` is similar to a variable declared `constexpr` — it is implicitly `const`. When we tackle compile-time mutable state, we believe that those variables should be declared `consteval mutable` instead.

## Allocation

This proposal also doesn't provide any enhancements to the kinds of things we could do with `consteval` variables either. As mentioned in both [@P3603R1] and [@P4101R1], a `consteval` variable could be allowed to "persist" its allocation. It's a compile-time only variable, so none of the issues around non-transient allocation exist. But that will come in a future paper.

## Implementation Experience

This has been implemented in my fork of Clang since Sofia. You can see it on [compiler explorer](https://compiler-explorer.com/z/fz9r54P6f).

# Wording

Allow variables to be declared `consteval` in [dcl.constexpr]{.sref}:

::: std
[1]{.pnum} The constexpr [and `consteval`]{.addu} specifier[s]{.addu} shall be applied only to the definition of a variable or variable template, a structured binding declaration, or the declaration of a function or function template.
[The `consteval` specifier shall be applied only to the declaration of a function or function template.]{.rm}
A function or static data member declared with the `constexpr` or `consteval` specifier on its first declaration is implicitly an inline function or variable ([dcl.inline]).
If any declaration of a function or function template has a `constexpr` or `consteval` specifier, then all its declarations shall contain the same specifier.

[...]

[6]{.pnum} A `constexpr` [or `consteval`]{.addu} specifier used in an object declaration declares the object as `const`.
Such an object shall have literal type and shall be initialized.
A `constexpr` [or `consteval`]{.addu} variable shall be constant-initializable ([expr.const]).
A `constexpr` [or `consteval`]{.addu} variable that is an object, as well as any temporary to which a `constexpr` [or `consteval`]{.addu} reference is bound, shall have constant destruction.
:::

Change the definitions of _potentially-constant_ and _usable in constant expressions_ to include variables declared `consteval` in [expr.const.init]{.sref}:

::: {.std .wording}
[6]{.pnum} A variable is potentially-constant if it is constexpr [or consteval]{.addu} or it has reference or non-volatile const-qualified integral or enumeration type.

[7]{.pnum} A variable `$V$` is *usable in constant expressions* at a point `$P$` if `$V$` is constant-initialized and potentially-constant, `$V$`'s initializing declaration `$D$` is reachable from `$P$`, and

* [#.1]{.pnum} `$V$` is constexpr [or consteval]{.addu},
* [#.#]{.pnum} `$V$` is not initialized to a TU-local value, or
* [#.#]{.pnum} `$P$` is in the same translation unit as `$D$`.
:::

And then extend the definition of _immediate object_ to include those declared by consteval variables in [expr.const.const]{.sref}:

::: {.std .wording}
[2]{.pnum} An object is an *immediate object* if its complete object [has]{.rm}

* [#.0]{.pnum} [is the object declared by a consteval variable,]{.addu}
* [#.1]{.pnum} [has]{.addu} a constituent value that is consteval-only[,]{.addu} or
* [#.#]{.pnum} [has]{.addu} a constituent reference that refers to an immediate object or immediate function.
:::

## Feature-test Macro

Bump the value of `__cpp_consteval` in [cpp.predefined]{.sref}.