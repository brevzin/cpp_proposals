---
title: "Designated-initializers for Base Classes"
document: P2287R2
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

[@P2287R1] proposed a novel way of naming base classes, as well as a way for naming indirect non-static data members. This revision _only_ supports naming direct or indirect non-static data members, with no mechanism to name base classes. Basically only what Matthias suggested.

[@P2287R0] proposed a single syntax for a _designated-initializer_ that identifies a base class. Based on a reflector suggestion from Matthias Stearn, this revision extends the syntax to allow the brace-elision version of _designated-initializer_: allow naming indirect non-static data members as well. Also actually correctly targeting EWG this time.

# Introduction

[@P0017R1] extended aggregates to allow an aggregate to have a base class. [@P0329R4] gave us designated initializers, which allow for much more expressive and functional initialization of aggregates. However, the two do not mix: a designated initializer can currently only refer to a direct non-static data members. This means that if I have a type like:

::: bq
```cpp
struct A {
    int a;
};

struct B : A {
    int b;
};
```
:::

While I can initialize an `A` like `A{.a=1}`, I cannot designated-initialize `B`. An attempt like `B{@{1}@, .b=2}` runs afoul of the rule that the initializers must either be all designated or none designated. But there is currently no way to designate the base class here.

Which means that my only options for initializing a `B` are to fall-back to regular aggregate initialization and write either `B{@{1}@, 2}` or `B{1, 2}`. Neither are especially satisfactory.

# Proposal

This paper proposes extending designated initialization syntax to include both the ability to name base classes and also the ability to name base class members. In short, based on the above declarations of `A` and `B`, this proposal allows all of the following declarations:

::: bq
```cpp
B{@{1}@, 2}         // already valid in C++17
B{1, 2}           // already valid in C++17

B{.a=1, .b=2}     // proposed
B{.a{1}, .b@{2}@}   // proposed
B{.b=2, .a=1}     // still ill-formed
```
:::

## Naming the base classes

THe original revisions of this paper dealt with how to name the `A` base class of `B`, and what this means for more complicated base classes (such at those with template parameters). This revision eschews that approach entirely: it's simpler to just stick with naming members, direct and indirect. After all, that's how these aggregates will be interacted with.

What this means is that while this paper proposes that this works:

::: bq
```cpp
struct A { int a; };
struct B : A { int b; };

auto b = B{.a=1, .b=2};
```
:::

There would be no way to designated-initialize a type like this:

::: bq
```cpp
struct C : std::string { int c; };
```
:::

Because there would be no way to designated-initialize the base `std::string` suboject.

Likewise, there would be no way to designated-initialize both of the `x` subobjects in this example:

::: bq
```cpp
struct D { int x; };
struct E : D { int x; };

auto e = E{.x=1}; // initializes E::x, not D::x
```
:::

However, even without the inability to perfectly initialize objects of types `C` and `E` here, it is still quite beneficial to initialize objects of type `B` - and this is still the pretty typical case for aggregates with base classes: those base classes are also aggregates.

Coming up with a way to _name_ the base class subobject of a class seems useful, but that's largely orthogonal. It can be done later.

## Naming all the subobjects

The current wording we have says that, from [dcl.init.aggr]{.sref}/3.1:

::: bq
[3.1]{.pnum} If the initializer list is a brace-enclosed *designated-initializer-list*, the aggregate shall be of class type, the *identifier* in each designator shall name a direct non-static data member of the class [...]
:::

And, from [dcl.init.list]{.sref}/3.1 (conveniently, it's 3.1 in both cases):

::: bq
[3.1]{.pnum} If the *braced-init-list* contains a *designated-initializer-list*, `T` shall be an aggregate class. The ordered identifiers in the designators of the *designated-initializer-list* shall form a subsequence of the ordered identifiers in the direct non-static data members of `T`. Aggregate initialization is performed ([dcl.init.aggr]).
:::

The proposal here is to extend both of these rules to cover not just the direct non-static data members of `T` but also all indirect members, such that every interleaving bae class is also an aggregate class. That is:

::: bq
```cpp
struct A { int a; };
struct B : A { int b; };
struct C : A { C(); int c; };
struct D : C { int d; };

A{.a=1};       // okay since C++17
B{.a=1, .b=2}; // proposed okay, 'a' is a direct member of an aggregate class
               // and A is a direct base
C{.c=1};       // error: C is not an aggregate
D{.a=1};       // error: 'a' is a direct member of an aggregate class
               // but an intermediate base class (C) is not an aggregate
```
:::

Or, put differently, every identifier shall name a non-static data member that is not a (direct or indirect) member of any base class that is not an aggregate.

Also this is still based on lookup rules, so if the same name appears in multiple base classes, then either it's only the most derived one that counts:

```cpp
struct X { int x; };
struct Y : X { int x; };
Y{.x=1}; // initializes Y::x
```

or is ambiguous:

```cpp
struct X { int x; };
struct Y { int x; };
struct Z : X, Y { };
Z{.x=1}; // error:: ambiguous which X
```

would be ill-formed on the basis that `Z::x` is ambiguous.

# Wording

## Strategy

The wording strategy here is as follows. Let's say we have this simple case:

::: bq
```cpp
struct A { int a; };
struct B : A { int b; };
```
:::

In the current wording, `B` has two elements (the `A` direct base class and then the `b` member). The initialization `B{.b=2}` is considered to have one explicitly initialized element (the `b`, initialized with `2`) and then the `A` is not expliictly initialized and cannot have a default member initializer, so it is copy-initialized from `{}`.

The strategy to handle `B{.a=1, .b=2}` is to group the indirect non-static data members under their corresponding direct base class and to treat those base class elements as being explicitly initialized. So here, the `A` element is explicitly initialized from `{.a=1}` and the `b` element continues to be explicitly initialized from `2`. And then this applies recursively, so given:

```cpp
struct C : B { int c; };
```

With `C{.a=1, .c=2}`, we have:

* the `B` element is explicitly initialized from `{.a=1}`, which leads to:
  * the `A` element is explicitly initialized from `{.a=1}`, which leads to:
    * the `a` element is explicitly initialized from `1`
  * the `b` element is not explicitly initialized and has no default member initializer, so it is copy-initialized from `{}`
* the `c` element is explicitly initialized from `2`



## Actual Wording

Extend [dcl.init.aggr]{.sref}/3.1:

::: bq
[3.1]{.pnum} If the initializer list is a brace-enclosed *designated-initializer-list*, [then]{.addu} the aggregate shall be of class type[,]{.rm} [and]{.addu} the *identifier* in each *designator* shall name [either]{.addu} a direct non-static data member of the class [ or an indirect non-static data member in which every interleaving derived class is an aggregate.]{.addu} [, and the]{.rm} [The]{.addu} explicitly initialized elements of the aggregate are[:]{.addu}

* [3.1.1]{.pnum} [if any of the *identifier*s name an indirect non-static data member, then those direct base classes which have any direct or indirect non-static data member named, and]{.addu}
* [3.1.2]{.pnum} [those]{.addu} [the]{.rm} elements that are, or contain [(in the case of a member of an anonymous union)]{.addu}, [those members]{.rm} [the named direct non-static data members]{.addu}.
:::

And extend [dcl.init.aggr]{.sref}/4 to cover base class elements:

::: bq
[4]{.pnum} For each explicitly initialized element:

::: addu
* [4.0]{.pnum} If the the initializer list is a brace-enclosed *designated-initializer-list* and element is a direct base class, then let `C` denote that direct base class. The element is initialized from a synthesized brace-enclosed *designated-initializer-list* containing each designator that names a direct or indirect non-static data member of `C`.

::: bq
[*Example*
```
struct A { int a; };
struct B : A { int b; };
struct C : B { int c; };

// the A element is intialized from {.a=1}
B x = B{.a=1};

// the B element is initialized from {.a=2, .b=3}
// which leads to its A element being initialized from {.a=2}
C y = C{.a=2, .b=3, .c=4};

struct A2 : A { int a; };

// the A element is not explicitly initialized
A2 z = {.a=1};
```
*-end example*]
:::
:::

* [4.1]{.pnum} [If]{.rm} [Otherwise, if]{.addu} the element is an anonymous union [...]
* [4.2]{.pnum} Otherwise, the element is copy-initialized from the corresponding *initializer-clause* or is initialized with the *brace-or-equal-initializer* of the corresponding *designated-initializer-clause*. [...]
:::

Extend [dcl.init.list]{.sref}/3.1:

::: bq
[3.1]{.pnum} If the _braced-init-list_ contains a _designated-initializer-list_, `T` shall be an aggregate class.
The ordered *identifier*s in the designators of the *designated-initializer-list* shall form a subsequence of [*designatable members* of `T`, defined as follows:]{.addu} [the ordered *identifier*s in the direct non-static data members of `T`.]{.rm}

:::addu
* [3.1.1]{.pnum} For each direct base class `C` of `T` that is an aggregate class, the designatable members of `C` that can be denoted by direct class member access ([expr.ref]) from an object of type `T` , followed by
* [3.1.2]{.pnum} The ordered *identifiers* in the direct non-static members of `T`.
:::

Aggregate initialization is performed ([dcl.init.aggr]).
[*Example 2*:
```diff
    struct A { int x; int y; int z; };
    A a{.y = 2, .x = 1};                // error: designator order does not match declaration order
    A b{.x = 1, .z = 2};                // OK, b.y initialized to 0

+   struct B : A { int q; };
+   B e{.x = 1, .q = 3};                // OK, e.y and e.z initialized to 0
+   B f{.q = 3, .x = 1};                // error: designator order does not match declaration order

+   struct C { int p; int x; };
+   struct D : A, C { };
+   D g{.y=1, .p=2};                    // OK
+   D h{.x=2};                          // error: x is not a designatable member

+   struct NonAggr { int na; NonAggr(int); };
+   struct E : NonAggr { int e; };
+   E i{.na=1, .e=2};                   // error: na is not a designatable member
```
— *end example*]
:::
