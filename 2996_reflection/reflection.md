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
Specifically, we are mostly proposing a subset of features suggested in P1240R2:

  - the representation of program elements via constant-expressions producing
     _reflection values_ — _reflections_ for short — of an opaque type `std::meta::info`,
  - a _reflection operator_ (prefix `^`) that produces a reflection value for its operand construct,
  - a number of `consteval` _metafunctions_ to work with reflections (including deriving other reflections), and
  - constructs called _splicers_ to produce grammatical elements from reflections (e.g., `[:` _refl_ `:]`).

This proposal is not intended to be the end-game as far as reflection and compile-time
metaprogramming are concerned.  Instead, we expect it will be a useful core around which more
powerful features can be added incrementally over time.  In particular, we believe that most
or all the remaining features explored in P1240R2 and that code injection
(along the lines described in P2237R0) are desirable directions to pursue.

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

  - an error
  - any (C++) type and type-alias
  - any function or member function
  - any namespace-scope variable or any C++ static data member
  - any nonstatic data member
  - any constant value
  - any template

[ (DV) I'm currently excluding local variable representation.  We can change that if we have a use for them that we feel must be in the initial core functionality. ]

## The Reflection Operator (`^`)


## Splicers (`[:`...`:]`)

## Metafunctions


