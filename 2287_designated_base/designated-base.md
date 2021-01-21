---
title: "Designated-initializers for Base Classes"
document: P2287R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

[@P0017R1] extended aggregates to allow an aggregate to have a base class. [@P0329R4] gave us designated initializers, which allow for much more expressive and functional initialization of aggregates. However, the two do not mix: a designated initializer can currently only refer to a direct non-static data members. This means that if I have a type like:

```cpp
struct A {
    int a;
};

struct B : A {
    int b;
};
```

While I can initialize an `A` like `A{.a=1}`, I cannot designated-initialize `B`. An attempt like `B{@{1}@, .b=2}` runs afoul of the rule that the initializers must either be all designated or none designated. But there is currently no way to designate the base class here.

Which means that my only options for initializing a `B` are to fall-back to regular aggregate initialization and write either `B{@{1}@, 2}` or `B{1, 2}`. Neither are especially satisfactory. 

The goal of this paper is to extend designated initialization to include base classes. 

# Proposal

The tricky part here is: how do we name the `A` base class of `B` in the _designated-initializer-list_? While non-static data members have to _identifier_s, base classes can be much more complicated. We do not actually have a way to name the `A` base class subobject of a `B` today &mdash; the only way to get there is by casting. This means there's no corresponding consistent syntax to choose along with the `.` that we already have.

Daveed Vandevoorde makes the suggestion that we can use `:` to introduce an _id-expression_ that names a base class. This would allow the following initialization syntax:

```cpp
B{:A={.a=1}, .b=2}
```

Using a `:` mimics the way we introduce base classes in class directions and is otherwise unambiguous with the rest of the _designated-initializer_ syntax. It can also prepare the parser for the fact that a more complicated name might be coming.

This paper does not change any of the other existing designated-initialization rules: the initializers must still be all designated or none designated, and the designators must be in order. I'm simply extending the order being matched against with all the base classes. That is, while `B{:A={.a=1}, .b=2}` would be a valid way to initialize a `B`, `B{.b=2, :A={.a=1}}` is ill-formed (out of order), as is `B{@{.a=1}@, .b=2}` (some designated but not all).

This generalizes to more complex aggregates like:

```cpp
template <typename T> struct C { T val; };
struct D : C<int>, C<char> { };

D{:C<int>={.val=1}, :C<char>={.val='x'}};
```

Which provides protection against `D{'x', 1}` which compiles fine but probably isn't what was desired.

# Wording

Change the grammar of a _designator_ in [dcl.init.general]{.sref}/1. Technically this allows a _designated-initializer-list_ like `{.a=1, :A={}}` which we could forbid grammatically, but that seems more complicated than simply extending the ordering rule to forbid it (which has to be done anyway).

::: bq
```diff
  @_designator_@:
      . @_identifier_@
+     : @_id-expression_@
```
:::

Extend [dcl.init.general]{.sref}/18:

::: bq
[18]{.pnum} The same _identifier_ shall not appear in multiple designators of a _designated-initializer-list_. [The same _id-expression_ shall not appear in multiple designators of a _designated-initializer-list_.]{.addu}
:::

Extend [dcl.init.aggr]{.sref}/3.1:

::: bq
[3.1]{.pnum} If the initializer list is a _designated-initializer-list_, the aggregate shall be of class type, the _identifier_ in each designator shall name a direct non-static data member of the class, [the _id-expression_ in each designator shall name a direct base class of the class,]{.addu} and the explicitly initialized elements of the aggregate are the elements that are, or contain, those members.
:::

Extend [dcl.init.list]{.sref}/3.1:

::: bq
[3.1]{.pnum} If the _braced-init-list_ contains a _designated-initializer-list_, `T` shall be an aggregate class.
The ordered [*id-expression*s and]{.addu} *identifier*s in the designators of the *designated-initializer-list* shall form a subsequence of the ordered [direct base classes of `T` and]{.addu} *identifier*s in the direct non-static data members of `T`.
Aggregate initialization is performed ([dcl.init.aggr]).
[*Example 2*:
```diff
  struct A { int x; int y; int z; };
  A a{.y = 2, .x = 1};                // error: designator order does not match declaration order
  A b{.x = 1, .z = 2};                // OK, b.y initialized to 0
  
+ struct B : A { int q; };
+ B c{.q = 3, :A{}};                  // error: designator order does not match declaration order
+ B d{:A{}, .q = 3};                  // OK, d.x, d.y, and d.z all initialized to 0
```
— *end example*]
:::