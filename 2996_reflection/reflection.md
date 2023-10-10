---
title: "Reflection for C++26"
document: P2996R0
date: today
audience: EWG
author:
    - name: Lots
    - name: Of
    - name: People
toc: true
---

# Introduction

This is a proposal for a reduced initial set of features to support static reflection in C++.
Specifically, we are mostly proposing a subset of features suggested in [@P1240R2]:

  - the representation of program elements via constant-expressions producing
     _reflection values_ — _reflections_ for short — of an opaque type `std::meta::info`,
  - a _reflection operator_ (prefix `^`) that produces a reflection value for its operand construct,
  - a number of `consteval` _metafunctions_ to work with reflections (including deriving other reflections), and
  - constructs called _splicers_ to produce grammatical elements from reflections (e.g., `[: $refl$ :]`).

This proposal is not intended to be the end-game as far as reflection and compile-time
metaprogramming are concerned.  Instead, we expect it will be a useful core around which more
powerful features will be added incrementally over time.  In particular, we believe that most
or all the remaining features explored in P1240R2 and that code injection
(along the lines described in [@P2237R0]) are desirable directions to pursue.

Our choice to start with something smaller is primarily motivated by the belief that that
improves the chances of these facilities making it into the language sooner rather than
later.


## Why a single opaque reflection type?

Perhaps the most common suggestion made regarding the framework outlined in P1240 is to
switch from the single `std::meta::info` type to a family of types covering various
language elements (e.g., `std::meta::variable`, `std::meta::type`, etc.).

We believe that doing so would be mistake with very serious consequences for the future of C++.

Specifically, it would codify the language design into the type system.  We know from
experience that it has been quasi-impossible to change the semantics of standard types once they
were standardized, and there is no reason to think that such evolution would become easier in
the future.  Suppose for example that we had standardized a reflection type `std::meta::variable`
in C++03 to represent what the standard called "variables" at the time.  In C++11, the term
"variable" was extended to include "references".  Such an change would have been difficult to
do given that C++ by then likely would have had plenty of code that depended on a type arrangement
around the more restricted definition of "variable".  That scenario is clearly backward-looking,
but there is no reason to believe that similar changes might not be wanted in the future and we
strongly believe that it behooves us to avoid adding undue constraints on the evolution of the
language.

Other advantages of a single opaque type include:

  - it makes no assumptions about the representation used within the implementation
    (e.g., it doesn't advantage one compiler over another),
  - it is trivially extensible (no types need to be added to represent additional
    language elements and meta-elements as the language evolves), and
  - it allows convenient collections of heterogeneous constructs without having
    to surface reference semantics (e.g., a `std::vector<std::meta::info>`
    can easily represent a mixed template argument list — containing types and
    nontypes — without fear of slicing values).

# Examples

## Back-And-Forth

Our first example is not meant to be compelling but to show how to go back and forth between the reflection domain and the grammatical domain:

```c++
constexpr auto r = ^int;
typename[:r:] x = 42;  // Same as "int x = 42;".
typename[:^char:] c = '*';  // Same as "char c = '*';".
```

## Selecting Members

Our second example enables selecting a member "by number" for a specific type.  It also shows the use of a metafunction dealing with diagnostics:

```c++
struct S { unsigned i:2, j:6; } s{0, 0};
consteval auto member_nr(int n) {
  if (n == 0) return ^S::i;
  else if (n == 1) return ^S::j;
  else return std::meta::invalid_reflection("Only field numbers 0 and 1 permitted");
}
int main() {
  s.[:member_nr(1):] = 42;  // Same as "s.j = 42;".
  s.[:member_nr(5):] = 0;   // Error (likely with "Only field numbers 0 and 1 permitted" in text).
}
```

This example also illustrates that bit fields are not beyond the reach of this proposal.

# Proposed Features

## `std::meta::info`

The type `std::meta::info` can be defined as follows:

```c++
namespace std {
  namespace meta {
    using info = decltype(^int);
  }
}
```

In our initial proposal a value of type `std::meta::info` can represent:

  - an error (corresponding to an "invalid reflection")
  - any (C++) type and type-alias
  - any function or member function
  - any namespace-scope variable or any C++ static data member
  - any non-static data member
  - any constant value
  - any template
  - any namespace

[ (DV) I'm currently excluding local variable representation.  We can change that if we have a use for them that we feel must be in the initial core functionality. ]


The type `std::meta::info` is a _scalar_ type with associated namespace `std::meta`.  Nontype template arguments of type `std::meta::info` are permitted.
The entity being reflected can affect the linkage of a template instance involving a reflection.  For example:

```c++
template<auto R> struct S {};

extern int x;
static int y;

S<^x> sx;  // S<^x> has external name linkage.
S<^y> sy;  // S<^y> has internal name linkage.
```


## The Reflection Operator (`^`)

The reflection operator produces a reflection value from a grammatical construct (its operand):

> | _unary-expression_:
> |       ...
> |       `^` `::`
> |       `^` _namespace-name_
> |       `^` _type-id_
> |       `^` _cast-expression_

Note that _cast-expression_ includes _id-expression_, which in turn can designate templates, member names, etc.

The current proposal requires that the _cast-expression_ be:

  - a _primary-expression_ referring to a function or member function, or
  - a _primary-expression_ referring to a namespace-scope variable or to a static data member, or
  - a _primary-expression_ referring to a nonstatic data member, or
  - a _primary-expression_ referring to a template, or
  - a constant-expression.

In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to evaluate to an invalid reflection.

## Splicers (`[:`...`:]`)

A reflection that is not an invalid reflection can be "spliced" into source code using one of several _splicer_ forms:

 - `[: r :]` produces an _expression_ evaluating to the entity or constant value represented by `r`.
 - `typename[: r :]` produces a _simple-type-specifier_ corresponding to the type represented by `r`.
 - `template[: r :]` produces a _template-name_ corresponding to the template represented by `r`.
 - `namespace[: r :]` produces a _namespace-name_ corresponding to the namespace represented by `r`.
 - `[:r:]::` produces a _nested-name-specifier_ corresponding to the namespace, enumeration type, or class type represented by `r`.

Attempting to splice a reflection value that does not meet the requirement of the splice is ill-formed.
For example:

```c++
typename[: ^:: :] x = 0;  // Error.
```


## Metafunctions


