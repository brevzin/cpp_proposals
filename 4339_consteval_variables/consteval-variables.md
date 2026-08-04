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

We propose that a variable declared `consteval` is similar to a variable declared `constexpr` — it is implicitly `const`. When we tackle compile-time mutable state, we believe that those variables should be declared `consteval mutable` instead.

## Allocation

This proposal also doesn't provide any enhancements to the kinds of things we could do with `consteval` variables. As mentioned in both [@P3603R1] and [@P4101R1], a `consteval` variable could be allowed to "persist" its allocation. It's a compile-time only variable, so none of the issues around non-transient allocation exist. But that will come in a future paper.

## Implementation Experience

This has been implemented in Barry's fork of Clang since Sofia. You can see it on [compiler explorer](https://compiler-explorer.com/z/fz9r54P6f).

# Wording

[We're defining the term "constexpr variable" to be those variables declared either `constexpr` or `consteval`, since all rules we have apply to both (except for a new one specifically for those declared `consteval`). This is simpler than making sure we catch all the uses of "constexpr" to be "constexpr or consteval", and is how Barry implemented it too.]{.draftnote}

Allow variables to be declared `consteval` in [dcl.constexpr]{.sref}:

::: std
[1]{.pnum} The constexpr [and `consteval`]{.addu} specifier[s]{.addu} shall be applied only to the definition of a variable or variable template, a structured binding declaration, or the declaration of a function or function template.
[The `consteval` specifier shall be applied only to the declaration of a function or function template.]{.rm}
A function or static data member declared with the `constexpr` or `consteval` specifier on its first declaration is implicitly an inline function or variable ([dcl.inline]).
If any declaration of a function or function template has a `constexpr` or `consteval` specifier, then all its declarations shall contain the same specifier.

[ An explicit specialization can differ from the template declaration with respect to the `constexpr` or `consteval` specifier.]{.note}

[Function parameters cannot be declared `constexpr` [or `consteval`]{.addu}.]{.note}

[...]

[6]{.pnum} A `constexpr` [or `consteval`]{.addu} specifier used in an object declaration declares the object as `const`.
Such an object shall have literal type and shall be initialized. [A `constexpr` or `consteval` specifier used in the declaration of a variable declares that variable to be a *constexpr variable*.]{.addu}
A [`constexpr`]{.rm} [constexpr]{.addu} variable shall be constant-initializable ([expr.const.init]).
A [`constexpr`]{.rm} [constexpr]{.addu} variable that is an object, as well as any temporary to which a [`constexpr`]{.rm} [constexpr]{.addu} reference is bound, shall have constant destruction.
:::

Extend the definition of _immediate object_ to include those declared by consteval variables in [expr.const.const]{.sref}:

::: {.std .wording}
[2]{.pnum} An object is an *immediate object* if its complete object [has]{.rm}

* [#.0]{.pnum} [is the object declared by, or a temporary object whose lifetime is extended to that of, a variable declared with the `consteval` specifier,]{.addu}
* [#.1]{.pnum} [has]{.addu} a constituent value that is consteval-only[,]{.addu} or
* [#.#]{.pnum} [has]{.addu} a constituent reference that refers to an immediate object or immediate function.
:::

Add `consteval` to what you can do in an expansion statement, first in [stmt.pre]{.sref}:

::: {.std .wording}
[8]{.pnum} For any *condition* or *for-range-declaration* `$D$`, each *decl-specifier* in the *decl-specifier-seq* of `$D$`, including that of any *structured-binding-declaration* of `$D$`, shall be either a *type-specifier*[, `consteval`,]{.addu} or `constexpr`.
:::

And in [stmt.expand]{.sref}, mirroring the wording in [dcl.struct.bind]{.sref}:

::: {.std .wording}
[1]{.pnum} Expansion statements specify repeated instantiations ([temp.decls.general]) of their substatement.

::: addu
Let `$CONST-SPECIFIER$` consist of each *decl-specifier* of the *decl-specifier-seq* of the *for-range-declaration* that is either `consteval` or `constexpr`.
:::

[...]

* [5.2]{.pnum} Otherwise, if `$S$` is an iterating expansion statement, `$S$` is equivalent to:
  ```cpp
  {
    $init-statement$
    @[constexpr~opt~]{.rm} [*CONST-SPECIFIER*]{.addu}@ decltype(auto) $range$ = ( $expansion-initializer$ );
    @[constexpr~opt~]{.rm} [*CONST-SPECIFIER*]{.addu}@ auto $begin$ = $begin-expr$; // see [stmt.ranged]

    $S$@~_0_~@
    @...@
    $S$@~_$N$-1_~@
  }
  ```

   where `$N$` is the result of evaluating the expression

   [...]

  and `$S$@~_i_~@` is

  ```cpp
  {
    @[constexpr~opt~]{.rm} [*CONST-SPECIFIER*]{.addu}@ auto @*iter*@ = $begin$ + i;
    $for-range-declaration$ = *@*iter*@;
    $compound-statement$
  }
  ```

   The variables `$range$`, `$begin$`, and `$iter$` are defined for exposition only. [The keyword constexpr is present in the declarations of range, begin, and iter if and only if constexpr is one of the decl-specifiers of the decl-specifier-seq of the for-range-declaration.]{.rm} The identifier `$i$` is considered to be a prvalue of type `std​::​ptrdiff_t`; the program is ill-formed if `$i$` is not representable as such a value.
- [#.#]{.pnum} Otherwise, `$S$` is a destructuring expansion statement and, if `$N$` is `0`,  `$S$` is equivalent to:

  ```cpp
  {
    $init-statement$
    @[constexpr~opt~]{.rm} [*CONST-SPECIFIER*]{.addu}@ auto&& $range$ = $expansion-initializer$;
  }
  ```
  otherwise, `$S$` is equivalent to:
  ```cpp
  {
    $init-statement$
    @[constexpr~opt~]{.rm} [*CONST-SPECIFIER*]{.addu}@ auto&& [u@~0~@, u@~1~@, @...@, u@~_$N$-1_~@] = $expansion-initializer$ ;
    $S$@~_0_~@
    @...@
    $S$@~_N-1_~@
  }
  ```

  where `$N$` is the structured binding size of the type of the `$expansion-initializer$` and `$S$@~_i_~@` is

  ```cpp
  {
    $for-range-declaration$ = $v$@~_i_~@ ;
    $compound-statement$
  }
  ```

   If the *expansion-initializer* is an lvalue, then `v@~i~@` is `u@~i~@`; otherwise, `v@~i~@` is `static_cast<decltype(u@~i~@)&&>(u@~i~@)`. [The keyword `constexpr` is present in the declaration of `u@~0~@, u@~1~@, @...@, u@~_$N$-1_~@` if and only if `constexpr` is one of the `$decl-specifier$`s of the `$decl-specifier-seq$` of the `$for-range-declaration$`.]{.rm}

:::

Add `consteval` to the list of structured bindings declarations in [dcl.pre]{.sref}:

::: {.std .wording}
[7]{.pnum} A *simple-declaration* or a *condition* with a *structured-binding-declaration* is called a *structured binding declaration* ([dcl.struct.bind]).
Each *decl-specifier* in the *decl-specifier-seq* shall be [`consteval`,]{.addu} `constexpr`, `constinit`, `static`, `thread_local`, `auto` ([dcl.spec.auto]), or a *cv*-qualifier.
The declaration shall contain at most one *sb-identifier* whose *identifier* is preceded by an ellipsis.
If the declaration contains any such *sb-identifier*, it shall declare a templated entity ([temp.pre]).
:::

Adjust [dcl.init.general]{.sref}:

::: {.std .wording}
[2]{.pnum} Except for objects declared with the `constexpr` [or `consteval`]{.addu} specifier, for which see [dcl.constexpr], [...]
:::

Update `consteval` handling in [dcl.struct.bind]{.sref}:

::: {.std .wording}
[1]{.pnum} [...] Let *cv* denote the *cv-qualifier*s in the *decl-specifier-seq* and `$S$` consist of each *decl-specifier* of the *decl-specifier-seq* that is `constexpr`, [`consteval`,]{.addu} `constinit`, or a *storage-class-specifier*. [...]
:::

## Feature-test Macro

Bump the value of `__cpp_consteval` in [cpp.predefined]{.sref}.