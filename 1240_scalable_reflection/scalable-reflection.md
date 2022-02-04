---
title: "Scalable Reflection in C++"
document: P1240R2
date: 01-15-2022
audience: SG7
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Andrew Sutton
      email: <Andrew.sutton@beyondidentity.com>
    - name: Faisal Vali
      email: <faisalv@yahoo.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
---

# Revision history

[@P1240R0] Initial revision introducing scalar reflection model, reifiers (now called splicers), extensive API,
and many examples.

[@P1240R1] Added `reflexpr(... xyz)`. Revised reifier/splicer syntax somewhat. Report on implementations.
Introduce `<meta>` header. Reorganized presentation slightly.

R2 (this revision) Harmonized with other papers in this area, including the use of the term “splicing” instead of
“reifying” and the syntax developed in [@P2320R0]. Various fixes and presentation improvements.

# Acknowledgments

Many thanks to Nina Ranns and Barry Revzin for significant feedback on a draft of this paper.

# Introduction

The first Reflection TS (based on [@N4766]) exposes reflection information as types (to simplify integration
with template metaprogramming techniques). However, SG7 agreed some time ago that the future of
reflective constructs in C++ should be value-based (see also [@P0425R0]). Specifically, the compile-time
computations required for reflective metaprogramming should make use of constant-evaluation, which,
unlike template metaprogramming, allows for ephemeral intermediate results (i.e., they don’t persist
throughout the compilation process) and for mutable values. This approach was described in [@P0993R0],
_Value-based Reflection_. To support that reflection design, we have passed a number of `constexpr`
extensions in C++20: consteval functions ([@P1073R3]), `std::is_constant_evaluated()`
([@P0595R2]), and constexpr dynamic allocation ([@P0784R7]), amongst others. We have also proposed _expansion
statements_ ([@P1306R1]), which are more broadly useful but especially convenient for reflective
metaprogramming: That feature was approved by the evolution working group for C++20, but did not
make it to a WG21 vote for lack of time completing the Core wording review. We still hope expansion
statements will be added to the language in the relatively near future.

That in itself still leaves plenty of design options for the reflection interface itself. What follows is an
extensive document describing:

* The representation and properties of “reflections” (with argumentation for our specific design and
considerations of alternatives).
* Mechanisms for *splicing*: Turning reflections into ordinary C++ source constructs (again, with
design discussions).
* A brief discussion about templates and their instances.
* Principles to translate existing standard template metaprogramming facilities to the reflection
domain.
* Principles to translate the Reflection TS facilities to the value-based reflection domain.
* Some examples to argue that proposals to add additional template metaprogramming facilities are
unneeded because the underlying functionality is better handled in the reflection domain.
* An appendix listing the meta-functions being worked on one ongoing implementation.

This paper doesn’t exist in a vacuum. Related topics have been separately explored in [@P2320R0] (“The
Syntax of Static Reflection”), [@P2237R0] (“Metaprogramming”), [@P2050R0] (“Tweaks to the design of source
code fragments”), [@P1717R0] (“Compile-time Metaprogramming in C++”), and [@P1306R1] (“Expansion
statements”).
Earlier versions of this paper were more exploratory in nature; this version uses experience with
implementations based on earlier versions to narrow down a first set of metaprogramming features that
are primarily aimed at providing reflection facilities (with splicing and ordinary template instantiation
handling generative programming). However, additional facilities (particularly, for code injection) have
been explored along with this proposal and we are not confident that they can be added incrementally on
top of this proposal.

## A simple example
The following function uses static reflection facilities presented in this paper to compute the string
representation of an enumerator value.

::: bq
```cpp
#include <meta>

template<Enum T>
std::string to_string(T value) { // Could also be marked constexpr
  template for (constexpr auto e : std::meta::members_of(^T)) {
    if ([:e:] == value) {
      return std::string(std::meta::name_of(e));
    }
  }
  return "<unnamed>";
}
```
:::

In broad strokes, the function does the following:

1. Gets the sequence enumerators from the enumeration type `T`,
2. Iterates over those enumerators, searching for the first that matches `value`,
3. Returns the name of that iterator.

Each of these operations relies on a feature included in this proposal. In particular, getting the sequence of
iterators requires that we first get a queryable representation of the enumeration type `T`. This is done using
the prefix `^` operator; it returns a *reflection*: a handle to an internal representation of type `T` maintained by
the compiler. The `members_of` function (declared in a newly proposed standard header `<meta>`)
returns a compile-time `std::span`, whose elements are the reflections of each enumerator in the enum.

To iterate over the span we use an *expansion-statement* (proposed through a separate paper [@P1306R1], and
previously approved by EWG but still in CWG review), spelled `template for`. This isn’t true
“iteration”, however. The body of the statement is repeated for each element in the `span` so that the loop
variable (`e` above) is initialized to `s[0]`, `s[1]`, ..., `s[n - 1]` in each successive repetition. The
expansion variable is declared `constexpr` and that carries into each repeated body. In other words, each
repetition is equivalent to:

::: bq
```cpp
{
  constexpr std::meta::info e = s[I];
  if ([:e:] == value)
    return std::meta::name_of(e);
}
```
:::

where `I` counts the repetitions of the loop’s body.

Within the expansion body, the `[: refl :]` construct recovers the value of a reflected entity. We call
this recovery process *splicing* and the constructs — like `[:...:]` — that enable it *splicers*. This can be
compared with the parameter `value` to determine if they are the same. Finally, the `name_of` function
returns a compile-time `string_view` for the identifier spelling of the matched enumerator. If none of
the enumerators matched (possible, e.g., when bit-ORing together enumerator values), we return a string
`"<unnamed>"` (which won’t collide with a valid identifier).

This is called *static reflection* because all of the operations used to query types and enumerators are
computed at compile time (i.e., statically). There is no additional runtime meta-information that must be
generated with such facilities, which reinforces the zero-overhead principle that is so fundamental to C++.
There is no runtime representation of the enumeration type and its enumerators. Only information that is
ODR-used is present in the final program.

## Implementation status

Two implementations of this proposal are underway.

The first and most complete is a fork of Clang by Lock3 Software (by, among others, Andrew and Wyatt,
authors of this paper). It includes a large portion of the capabilities presented here, albeit not always with
the exact syntax or interfaces proposed. In addition to these capabilities, Lock3’s implementation supports
expansion statements and injection primitives (including “fragment” support). Lock3 is currently not
maintaining this implementation, however.

The second is based on the EDG front end (by Faisal and Daveed) and is less complete: It implements the
reflection operator and most single splicers (but not the pack splicers; see below), and a few meta-library
interfaces. It does not currently implement features in other proposals like expansion statements or
injection primitives.

# Reflections

## The `^` operator

The first Reflection TS introduced the `reflexpr` operator to obtain reflection values encoded as types.
Previous versions of this paper attempted to avoid repeating the considerable bikeshedding that went into
selecting the `reflexpr` keyword by simply reusing it. Ironically, the spelling is more appropriate for
the value-based reflection since the corresponding operation is indeed an “expression” (i.e., a construct
that produces a value; in the TS it produces a type).

However, with months of practice with implementations that used `reflexpr(...)` we experienced
consistent feedback that that syntax is too “heavy”. So we went back to the drawing board and found that
the `^` prefix operator — suggesting “lifting” or “raising” representation — is available . This new syntax[^cli]
was agreed to by SG-7 during the discussion of [@P2320R0]. Thus, we can write:

[^cli]: We used to think that C++/CLI had already appropriated that syntax, but C++/CLI (and related C++ dialects) only
uses the caret for handle declarations and not for handle indirections.

::: bq
```cpp
constexpr std::meta::info reflection = ^name_or_postfix_expr;
```
:::

The value of `reflection` (i.e. the result of this *lifting operator*) is a compile-time value that *designates*
some view of the indicated program element by the implementation (specifically, the compiler front end).
I.e., it can be thought of as a handle to an internal structure of the compiler. In the rest of this proposal,
we refer to the result of `^` as a *reflection* or a *reflection value*.

Note that the lifting operator is the “gateway” into the reflected world, but it is not the only source of
reflections (or reflection values): We will further introduce a variety of functions that derive reflections
from other reflections (e.g., we’ll present a function that returns reflections for the members of a class
given a reflection for that class). Whatever the source of a reflection, we say that it *designates* language
concepts such as entities or value categories. As will be shown later, a reflection can *designate* multiple
notions. For example, `^f(x)` designates the called function `f` (if indeed that is what is called) and the
type and value category of the call result.

The operand of `^` must be one of the following:

* a *type-id*, including possibly a *simple-type-specifier* that designates a *template-name*
* a possibly qualified *namespace-name*
* the scope-qualifier token `::` (designating the global namespace)
* a *postfix-expression*[^postfix]

[^postfix]: Which includes any parenthesized expression.

In the case where the `name_or_postfix_expr` is an expression, it is unevaluated but *potentially
constant evaluated*. That implies that given `struct S { int x; };`, the expression `^S::x` is
permissible in this context. We will elaborate the available reflected semantics later in this paper. Since
`^name_or_postfix_expr` is an expression, `^(^name_or_postfix_expr)` is also valid
(generally producing a distinct reflection).

In this paper, we call *declared entity* any of the following: a namespace (but not a namespace alias), a
function or member function (that includes implicit special members, but not inherited constructors), a
function or template parameter, a variable, a type (but not a type alias), a data member, a base class, a
capture, or a template (including an alias template, but not a deduction guide template). Note that this is
slightly different from the standard term entity (which, e.g., includes “values” but not “captures”[^bindings] ). We
call *alias* a namespace alias or a type alias.

[^bindings]: This paper does not currently deal with structured bindings because their exact nature in the standard is still
somewhat in flux at the time of this writing. Once they’re clarified, we intend to revisit their status as a “declared
entity”.

## Reflection type

What should the type of a reflection be? We propose it to be a new scalar type[^scalar], distinct from all other
scalar types, that supports — aside from reading, assigning, and copying — only the scalar operations of
equality/inequality and contextual conversion to `bool`. In addition we propose specific splicers (that
transform a reflection value into a type or a name, see below) and library functions that can operate on
constexpr reflections and constexpr sequences of reflections and generate new reflection-values as
needed. All other operations on reflection values are then composed from these aforementioned
operations. We present our rationale below for this design choice.

[^scalar]: We could define it via `using info = decltype(^void);`

It is tempting to organize reflection values as class type values using a hierarchy of class types that try to
model the language constructs. For example, one could imagine a base class `Reflection`, from which
we might derive a class `ReflectedDeclaration`, itself the base class of `ReflectedFunction`
and `ReflectedVariable`.

We do not believe that is the best approach for at least the following reasons:

* Although the relationship between major language concepts is relatively stable, we do
occasionally make fundamental changes to our vocabulary (e.g., during the C++11 cycle we
changed the definition of “variable”). Such a vocabulary change is more disruptive to a class
hierarchy design than it is to certain other kinds of interfaces (we are thinking of function-based
interfaces here).
* Class hierarchy values aren’t friendly to value-based programming because of slicing; instead, it
works better with “reference” programming, which is particularly expensive for constant
evaluation (because it requires address computations, which involve additional bookkeeping to
check for potential undefined behavior).
* Class types are not easily used as nontype template arguments, particularly when we want to
restrict effects to compile time (the recently added support for nontype class-type template
arguments ([@P0732R2] + [@P1907R1]) imposes draconian limitations on class types). As it turns out,
instantiating templates over reflection values is an important idiom when it comes to generative
programming (e.g., through splicers or, eventually, code injection).
* Implementations of constant-evaluation usually handle non-pointer scalar values significantly
more efficiently than class values.

Regarding this last point, the following compile-time test:

::: bq
```cpp
constexpr int f() {
  int i = 0;
  for (int k = 0; k<10000; ++k) {
    i += k;
  }
  return i/10000;
}
template<int N> struct S {
  static constexpr int sm = S<N-1>::sm+f();
};
template<> struct S<0> {
  static constexpr int sm = 0;
};
constexpr int r = S<200>::sm;
```
:::

compiles in about 0.6 seconds on a compact laptop (2016 MacBook m7), but wrapping the integers as
follows:

::: bq
```cpp
struct Int { int v; };
constexpr int f() {
  Int i = {0};
  for (Int k = {0}; k.v<10000; ++k.v) {
    i.v += k.v;
  }
  return i.v/10000;
}
template<int N> struct S {
  static constexpr int sm = S<N-1>::sm+f();
};
template<> struct S<0> {
  static constexpr int sm = 0;
};
constexpr int r = S<200>::sm;
```
:::

doubles the compile time to 1.2 seconds. Adding a derived-class layer would further increase the time.
Another increase would result from attempting to access the classes through references (as would be
tempting with a class hierarchy) because address computations require some work to guard against
undefined behavior.

Because of these various considerations, we therefore propose that the type of a reflection is an
unspecified scalar type, distinct from all other scalar types, whose definition is:

::: bq
```cpp
namespace std::meta {
  using info = decltype(^void);
}
```
:::

Namespace `std::meta` is an associated namespace of `std::meta::info` for the purposes of
argument-dependent lookup (ADL): That makes the use of various other facilities in that namespace
considerably more convenient. (In this sense, `std::meta::info` is similar to an enumeration type.)

By requiring the type to be scalar, we avoid implementation overheads associated with the compile-time
evaluation of class objects, indirection, and inheritance. By making the type unspecified but distinct, we
avoid accidental conversions to other scalar types, and we gain the ability to define core language rules
that deal specifically with these values. Moreover, no special header is required before using the lifting
operator.

## Reflection categories

As noted earlier, reflection values behave as handles to internal structures of the compiler. To reason
about the kind of semantic information one can obtain through these reflection values, we categorize the
values into one or more of four groups:

* Declared-entity reflections
* Alias reflections
* Expression reflections
* Invalid reflections
*
Note, declared-entity-reflections *only* designate the declared-entity; alias-reflections always designate a
declared-entity in addition to providing the name of the alias; and, expression-reflections might or might
not designate a declared-entity (e.g., an *id-expression* might designate a variable), but always designate
properties of the expression. *Invalid reflections* will be discussed in more detail later, but they represent
various kinds of failures when creating reflections using means other than the `^` operator.

For the most part, reflections of names (including type-ids) designate the declared entity those names
denote: variables, functions, types, namespaces, templates, etc. For example:

::: bq
```cpp
^const int            // Designates the type const int.
^std                  // Designates the namespace std.
^std::pair            // Designates the template pair.
^std::pair<int, int>  // Designates the specialization.

int* f(int);
^decltype(f(3))       // Designates the type int*.
```
:::

Reflections of *expressions* designate a limited set of characteristics of those expressions, including at least
their type and value category. For example:

::: bq
```cpp
^1 // Designates the property “prvalue of type int” (but also the constant value 1)
```
:::

(Further on we will present facilities to examine and/or splice the designated notions.)

If an expression also names a declared entity (via a possibly-parenthesized *id-expression*), then it also
designates that entity. For example:

::: bq
```cpp
int x;
^(x)        // Designates the declared-entity 'x' (variable) as well as the properties of
            // the expression 'x' (type and value category, in this case).

^(x+1)      // Does not designate a declared-entity but does designate the property
            // “prvalue of type int” (if ’x+1’ had been constant-valued, it would also
            // designate the value it represents).

^std::cout  // Designates the object named by std::cout as well as the
            // type and value category (lvalue) of the expression.
```
:::

If an expression is a *constant expression* it also designates that constant value:

::: bq
```cpp
^0                      // Designates the value zero and the property “prvalue of
                        // type int”. It does not capture that the expression is a
                        // is a literal or that it is usable as a null pointer value.

^nullptr                // Designates the null pointer value and the property “prvalue
                        // of type decltype(nullptr)”.

^std::errc::bad_message // Designates the enumerator, its constant value, and the
                        // property “prvalue of type std::errc”.
```
:::

If an expression represents a call at its top level, it also designates the function being called (but not, e.g.,
the arguments to that call):

::: bq
```cpp
^printf(“Hello, “)        // Designates printf and the property “prvalue of type int”.

^(std::cout << “World!”)  // Designates the applicable operator<<
                          // and “lvalue of type std::ostream”.

constexpr int f(int p) { return p+1; };
^f(41)                    // Designates f, the (returned) value 42, and
                          // “prvalue of type int”.

^(f(41)+1)                // Designates the (returned) value 43 and
                          // “prvalue of type int”; does not designate f
                          // because the call is not “top level”.
```
:::

Now consider:

::: bq
```cpp
constexpr int const i = 42;
constexpr auto r = ^i;
```
:::

As mentioned before, reflections can be categorized into four groups: declared-entity, alias, expression, or
invalid. In this example, the reflection value `r` is an “expression reflection” and thus designates both the
*expression* `i` (i.e. you can obtain information about properties of the expression such as its lvalueness)
and the *variable* `i`. However, sometimes it is useful to obtain a reflection that designates only the entity
(and not the expression). For example, we might want to query the type of the *variable* `i` (`int const`)
instead of the type of the *expression* `i` (`int`). It also can be useful when comparing if two reflections
refer to the same entity, as we will show later. We therefore provide the special function[^trailing-ret]

[^trailing-ret]: We use trailing return types for standard meta functions, but that’s just a stylistic preference. The traditional return
type style is just as valid.

::: bq
```cpp
namespace std::meta {
  consteval auto entity(info reflection)->info {...};
}
```
:::

which when applied to r produces a reflection designating just the *variable* (i.e., a “declared-entity
reflection”).

More generally, `std::meta::entity` extracts the declared-entity from its argument by returning:

* its argument — if its argument is a declared-entity reflection or an invalid reflection,
* a declared-entity reflection designating an entity `E` — if the argument is an alias or expression
reflection that also designates `E`, or
* an invalid reflection in all other cases (e.g., `entity(^42)` is an invalid reflection).

When the `^` operand is the name of an *alias* (type or namespace) the reflection designates the aliased
entity indirectly (i.e., properties of the alias can be queried directly). For example:

::: bq
```cpp
using T0 = int;
using T1 = const T0;
constexpr meta::info ref = ^T1;
```
:::

Here, ref designates both `T1` (directly) and the type `const int` (indirectly). This allows users to work
both with the alias and its meaning. However, underlying aliases are not designated: There is no way to
find about `T0` through `ref`.

In a more abstract sense, reflections designate semantic notions (names, types, value categories, etc.)
rather than syntax (tokens that comprise an expression and the relation of those tokens to others). This
principle helps guide decisions about the design of language and library support for reflection.

The queryable properties of these reflections are determined by the kind of “thing” they reflect. Details
are provided below.

## Equality and equivalence

Reflections can be compared using `==` and `!=` operators. Intuitively, the rule for these comparisons is that
we compare the underlying declared entity, except that we cannot compare the reflections of most
expressions nor can we compare invalid reflections. The exact rules are as follows...

1. If two reflections designate declared entities or aliases of such entities and do not designate
expression properties of an expression that is not an *id-expression*, the reflections compare equal
if the entities are identical and unequal if the entities are not identical (i.e., the comparison “looks
through” aliases).
2. Any reflection also (obviously) compares equal to itself and to copies of itself.
3. An invalid reflection compares unequal to a reflection that is not invalid.
4. A reflection that designates a declared entity or an alias of such an entity and does not designate
expression properties of an expression that is not an *id-expression* (e.g., it is not the reflection of a
function call) compares unequal to a reflection that either does not designate an entity or an alias
of such entity, or that designates properties of an expression that is not an *id-expression*.
5. All other cases are unspecified: That includes comparing reflections of expressions other than
*id-expressions* and invalid reflections. For example:

::: bq
```cpp
typedef int I1;
typedef int I2;
static_assert(^I1 == ^I2);        // Rule 1: Same underlying declared entity (int).
static_assert(^I1 == ^int);       // Ditto.

float f = 3.0, e;
static_assert(^f == ^(f));        // Rule 1: Same underlying declared entity (f).
static_assert(^f == ^::f);        // Ditto.
static_assert(^f != ^e);          // Rule 1: Different underlying declared entities.
static_assert(^I1 != ^float);     // Ditto.

void g(int);
constexpr auto r = ^g(1), s = r;
static_assert(r == s);            // Rule 2: One is a copy of the other.
static_assert(^f != ^g(1));       // Rule 4: f is an id-expression and g(1) is not.
static_assert(^g != ^g(1));       // Rule 4: One is the reflection of an id-expression
                                  // and the other is the reflection of an expression
                                  // that is not an id-expression.
static_assert(^g(1) == ^g(1));    // Rule 5: May fail because g(1) is an expression
                                  // that is not an id-expression
```
:::

Programmers can more precisely specify whether they intend to compare entities or computed values (if
possible) using splicers (e.g., `typename[:r:]` vs. just `[:r:]`) or library facilities like
`std::meta::entity` described above. For example:

::: bq
```cpp
void f();   // #1
int f(int); // #2
constexpr auto r = std::meta::entity(^f(42)); // Designates function #2.
static_assert(r != ^f(42));                   // Fails.
static_assert(r == entity(^f(0)));            // Always succeeds..
```
:::

Note that rule 1 above also applies to namespace aliases:

::: bq
```cpp
namespace N {};
namespace N1 = N;
namespace N2 = N;
static_assert(^N1 == ^N2);
static_assert(^N1 == ^N);
namespace M {};
static_assert(^N != ^M);
```
:::

For reflections obtained from operands that involve template parameters, the result depends on the
template arguments used for substitution:

::: bq
```cpp
template<typename T, typename U> struct Fun {
  static_assert(^T == ^U);
};
Fun<int, int> whee;   // Okay.
Fun<int, char> oops; // Error: static assertion fails.
```
:::

We already mentioned that it is unspecified whether reflections obtained from expressions that do not
designate a declared entity compare equal. That also applies to expressions that just consist of a literal.
For example:

::: bq
```cpp
static_assert(^1 == ^1); // May or may not fail.
```
:::

(These rules allow us to avoid having to provide a general definition of “expression equivalence”.)
Note that the properties associated with a declared entity may change over various contexts, but that does
not change the reflection. For example:

::: bq
```cpp
struct S;
constexpr auto r1 = ^S;
struct S {};
constexpr auto r2 = ^S;
static_assert(r1 == r2);
```
:::

However, queries against the reflection value (e.g., to obtain a list of class members) may change as a
consequence of the changes in the underlying entity.

An additional comparison function is proposed:

::: bq
```cpp
namespace std::meta {
  consteval auto same_reflections(info, info)->bool { ... };
}
```
:::

If either `x` or `y` designate an alias (type or namespace) `same_reflections(x, y)` returns `true` if `x`
and `y` designate the same alias and `false` otherwise. Otherwise (i.e., if neither `x` nor `y` designate an
alias), `same_reflections(x, y)` returns `x == y`. In other words, `same_reflections(x, y)`
is like the equality operator except that it doesn’t “look through” aliases. For example, with the
namespace aliases `N1` and `N2` as above:

::: bq
```cpp
using std::meta::same_reflections;
static_assert(!same_reflections(^N1, ^N2));
static_assert(same_reflections(^(^N1), ^(^N1)));  // May fail.
static_assert(!same_reflections(^(^N1), ^(^N2))); // May fail.
```
:::

The latter two assertions have unspecified behavior because `^N1` (or `^N2`) is an expression that is not an
*id-expression* and (as was noted above) the equality of the reflections of such expressions is unspecified.

To compare the values of reflected objects, references, functions, or types, the reflection can first be
spliced (see below).

## A Note About Linkage

Although in most respects we propose that `std::meta::info` is an ordinary scalar type, we also give
it one “magical” property with respect to linkage.

Before explaining this property, consider again what a reflection value represents in practice: It is a handle
to internal structures the compiler builds up for the current translation unit. So for code like:

::: bq
```cpp
struct S {};
consteval auto f() {
  return ^S;
}
```
:::

the compiler will construct an internal representation for struct `S` and when it encounters `^S` it will
update a two-way map between the internal representation of `S` and a small structure underlying the
std::meta::info value returned by `^S`.

Now consider:

::: bq
```cpp
// Header t.hpp:
struct S {};
template<std::meta::info reflection> struct X {};

// File t1.cpp:
#include "t.hpp"
enum E {};
consteval auto d() {
  return ^E;
}
X<^S> g() {
  return X<^S>{};
}

// File t2.cpp:
#include "t.hpp"
extern X<^S> g();
int main() {
  g();
}
```
:::

The files t1.cpp and t2.cpp are compiled separately. The contexts in which the `^S` construct is
encountered are therefore different and it is not practical to ensure that the *underlying* values (“bits”) of
the `std::meta::info` results are identical. However, it is *very* desirable that the types `X<^S>` are the
same types in both translation units and that the above example not produce an ODR violation.

We therefore specify “by fiat” that:
* `reinterpret_cast` to or from `std::meta::info` is ill-formed
* accessing the byte representation of `std::meta:info` lvalues produces unspecified (possibly
inconsistent) values
* `std::meta::info` values `A1` and `A2` produce equivalent template arguments if
`std::meta::same_reflections(A1, A2)` produces `true`.

However, it is unspecified if the following variation of the previous example is valid:

::: bq
```cpp
// File t1.cpp:
enum E {};
consteval auto d() {
  return ^(^E);
}
X<^(^S)> g() {
  return X<^(^S)>{};
}

// File t2.cpp:
extern X<^(^S)> g();
int main() {
  g();
}
```
:::

because it is unspecified if two occurrences of `^(^S)` are equivalent.

(In practice, this means that reflection values are mangled symbolically, according to what the reflection
value actually designates.)

## Invalid reflections

In what follows we are going to propose a large collection of standard reflection operations, some of
which generate new reflection values. Sometimes, the application of some of these operations will be
meaningless. E.g., consider:

::: bq
```cpp
namespace std::meta {
  consteval auto add_const(info)->info {...};
}
```
:::

which is meant to take a reflection of a type and add a type qualifier on top. However, what happens with
something like:

::: bq
```cpp
constexpr auto r = add_const(^std);
```
:::

which suggests the meaningless operation of adding a `const` qualifier to namespace `std`? Our answer
is that an implementation will not immediately trigger an error in that case, but instead create a reflection
value that represents an error. Any attempt to splice such a reflection is ill-formed (but subject to
SFINAE).

It is useful for user code to also be able to produce invalid reflections. To that end, we propose the
following function:

::: bq
```cpp
namespace std::meta {
  consteval
    auto invalid_reflection(std::string_view message,
                            std::source_location src_loc = std::source_location::current())
      ->info {...};
}
```
:::

which constructs a reflection that triggers a diagnostic if it is spliced outside a SFINAE context (ideally,
with the given message and source location information). Here, the functions

Invalid reflections can also be used to generate compiler diagnostics during constant evaluation using the
`diagnose_error` function. This can be a valuable debugging aid for authors of metaprogramming
libraries, and when used effectively, should improve the usability of those libraries.

::: bq
```cpp
namespace std::meta {
  consteval void diagnose_error(info invalid_refl) {...};
}
```
:::

This function causes the compiler to emit an error diagnostic (formally: it makes the program ill-formed if
it is invoked outside a deduction/SFINAE context), hopefully with the message and location provided by
the argument. For example:

::: bq
```cpp
auto r = std::meta::invalid_reflection(“Oops!”);
int main() {
  diagnose_error(r); // Error.
}
```
:::

That last example is ill-formed and might trigger an error like:

::: bq
```
“Test.cpp”, line 3: error: Invalid reflection
  diagnose_error(r); // Error.
  ^

“Test.cpp”, line 1: note: Oops!
  auto r = std::meta::invalid_reflection(“Oops!”);
           ^
```
:::

Finally, we propose a predicate:

::: bq
```cpp
namespace std::meta {
  consteval auto is_invalid(info)->bool {...};
}
```
:::

that can be used to, e.g., filter out invalid reflective operations. We also provide a convenience overload of
this function:

::: bq
```cpp
namespace std::meta {
  consteval auto is_invalid(std::span<info>)->bool {...};
}
```
:::

which returns `true` if any element of the given span is an invalid reflection. This is particularly useful
because some important reflection facilities return spans of reflection values that callers are likely to want
to check for invalid entries.

## Initialization of reflections

Objects of reflection type are zero-initialized to an invalid reflection value (with unspecified associated
information).

## Conversions on reflections

A prvalue of reflection type can be contextually converted to a prvalue of type bool. An invalid
reflection converts to false; all other reflections convert to true.

## Hashing reflections

We propose that the `std::hash` template be specialized for `std::meta::info`. We also propose
that the resulting hash value be consistent across translation units.

# Splicing

In the context of this paper, “splicing” refers to the process of turning a “reflection value” back into a
“program source thing”. We propose a basic *splice* construct to be of the form

::: bq
```cpp
[: reflection :]
```
:::

where `[:` and `:]` are each a sequence of two tokens and reflection is a constant-expression of type
`std::meta::info`. (Prior versions of this paper discuss various alternative syntax options. The choice
presented here was first proposed in [@P2320R0], which obtained strong support in SG-7.)

In general, and without qualification, `[: R :]` splices an expression into the program (assuming `R` reflects
a variable, function, or a constant expression). If `R` reflects both a constant value and a declared entity

---
references:
  - id: std-discussion
    citation-label: std-discussion
    title: "Should a `std::basic_format_string` be specified?"
    author:
      - family: Joseph Thomson
    issued:
      year: 2021
    URL: https://lists.isocpp.org/std-discussion/2021/12/1526.php
---
