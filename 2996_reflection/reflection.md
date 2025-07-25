---
title: "Reflection for C++26"
document: P2996R13
date: today
audience: CWG, LWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Andrew Sutton
      email: <andrew.n.sutton@gmail.com>
    - name: Faisal Vali
      email: <faisalv@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
toc-depth: 4
tag: reflection
---

# Revision History

Since [@P2996R12]:

* core wording updates
  * handle members of static anonymous unions / integrate suggested fix for [@CWG3026]
  * integrate EWG updates from [@P3687R0]{.title}
    * removed splice template arguments
    * unparenthesized `$splice-expression$`s are ill-formed when used as template arguments
    * `$reflect-expression$`s are ill-formed when the operand names a `$using-declarator$`
* library wording updates
  * specification of `reflect_constant`/`reflect_object`/`reflect_function`

Since [@P2996R11]:

* core wording updates
  * better specify interaction between spliced function calls and overload resolution; integrate fix to [@CWG2701]
  * disallow reflection of local parameters introduced by `$requires-expression$`s
  * change "naming class" to "designating class" and define for `$splice-expression$`s
  * replaced `reflect_value` and `value_of` with `reflect_constant` and `constant_of`.
* library wording updates
  * improve specification of `access_context::current()` (including examples)
  * `size_of(r)` is no longer constant if `r` is a bit-field
  * allowing `extract` to pull a pointer from an array

Since [@P2996R10]:

* replaced `has_complete_definition` function with more narrow `is_enumerable_type`
* core wording updates
  * disallow splicing constructors and destructors (inadvertently removed between R7 and R8)
  * prevent dependent `$splice-specifier$`s from appearing in CTAD (following CWG3003)
  * fixed parsing rules for a `$reflect-expression$` followed by `<`
  * iterate on wording for overload resolution for `$splice-expression$` function calls
  * disallow default arguments for splices ([over.match.viable])
  * added more core examples
  * rebase onto latest working draft (as of 2025-04-09)
* library wording updates
  * functions whose types contain placeholder types are not _members-of-representable_
  * fixed wording for `extract` and `object_of` to ensure that both functions can be used with reflections of local variables declared in immediate functions
  * specified `type_of` for enumerators called from within the containing `$enum-specifier$`
  * minor editing and phrasing updates to address CWG feedback
  * added type traits from [@P2786R13]{.title}
  * in response to CWG feedback: added `has_c_language_linkage`, `has_parent`, `is_consteval_only`
  * added `scope()` and `naming_class()` members to `access_context`
  * improve wording for `access_context::current()`, `is_accessible`, `members_of`


Since [@P2996R9]:

* core wording updates
  * merge [@P3289R1]{.title} into P2996. Replace the category of "plainly constant-evaluated expressions" with consteval blocks.
  * make the [expr.const] "scope rule" for injected declarations more rigorous; disallow escape from function parameter scopes
  * revise [expr.reflect] grammar according to CWG feedback
  * handle concept splicing in type constraints with template arguments
  * bring notes and examples into line with current definitions
  * rebase [expr.const] onto latest from working draft (in particular, integrate changes from [@P2686R5])
  * prefer "core constant expressions" to "manifestly constant-evaluated expression" in several places
  * producing an injected declaration from a non-plainly constant-evaluated context is prohibited for core constant expressions, rather than rendering the program ill-formed
  * do not specify `sizeof(std::meta::info)`
  * removed concept splicers
* library wording updates
  * add `<meta>` to [headers]
  * fix definition of "Constant When" (use "constant subexpression" in lieu of "core constant expression")
  * avoid referring to "permitted results of constant expressions" in wording for `reflect_value` and `reflect_object` (term was retired by [@P2686R5])
  * template specializations and non-static data members of closure types are not _members-of-representable_
  * integrated [@P3547R1]{.title} following its approval in (L)EWG.

Since [@P2996R8]:

* ensure `value_of` and `extract` are usable with reflections of local variables in consteval functions
* specifically state that `define_aggregate` cannot be used for a class currently being defined
* `members_of($closure-type$)` returns an unspecified sequence of reflections
* introduced a strong ordering on evaluations during constant evaluation
* assertions of `$static_assert-declaration$`s are not plainly constant-evaluated
* core wording changes
  * classify the injection of a declaration as a side effect ([intro.execution]); remove the related IFNDR condition from [expr.const].
  * rework [lex.phases]: refactor out the "semantically follows" relation introduced in R7: define when a plainly constant-evaluated expression is evaluated in terms of side effects and reachability. Inline the "semantically sequenced" relation into the [expr.const] conditions that render an injected declaration ill-formed.
  * improved [expr.const] examples demonstrating injected declaration rules
* library wording changes
  * minor wording improvements to: `type_of`, `alignment_of`, `bit_size_of`, `data_member_spec`, `define_aggregate`, type traits
  * fix wording bug that prevents application of `value_of` to a reflection of a local variable in a `consteval` function
  * clarify in note for `substitute` that instantiation may be triggered if needed to deduce a placeholder type
  * clarify which members of closure types are _members-of-representable_
  * fix wording bug in _members-of-reachable_ related to complete-class contexts; rename to "_members-of-precedes_"
  * remove idempotency from `define_aggregate`
* fleshed out revision history for R8


Since [@P2996R7]:

* changed reflection operator from `^` to `^^` following adoption of [@P3381R0]
* renamed `(u8)operator_symbol_of` to `(u8)symbol_of`
* renamed some `operators` (`exclaim` -> `exclamation_mark`, `three_way_comparison` -> `spaceship`, and `ampersand_and` -> `ampersand_ampersand`)
* renamed `define_class` to `define_aggregate`
* removed `define_static_array`, `define_static_string`, and `reflect_invoke`
* clarified that `sizeof(std::meta::info) == `sizeof(void *)`
* rename `data_member_options_t` to `data_member_options`, as per LEWG feedback
* clarified that `data_member_options` and `name_type` are non-structural consteval-only types
* clarified that everything in `std::meta` is addressable
* renaming `member_offsets` to `member_offset` and changing `member_offset` members to be `ptrdiff_t` instead of `size_t`, to allow for future use with negative offsets
* renamed the type traits from all being named `type_meow` to a more bespoke naming scheme.
* changing signature of `reflect_value` to take a `T const&` instead of a `T`.
* added an [informal section](#restrictions-on-injected-declarations) explaining restrictions on injected declarations
* removed `is_trivial_type`, since the corresponding type trait was deprecated.
* core wording changes
  * as per CWG feedback, merge phases 7 and 8 of [lex.phases]; get rid of "instantiation units". introduce "semantically sequenced" and "semantically follows" relations to specify ordering of plainly constant-evaluated expressions.
  * give type aliases and namespace aliases status as entities (note: incurs many changes throughout wording); introduce the notion of an "underlying entity"
  * audit for places where `$id-expression$` is specially handled for which `$splice-expression$` should be handled similarly
  * move splice specifiers into [basic.splice]; re-word all splicers
  * lift template splicers out from `$template-name$` (i.e., introduce a `$splice-specialization-specifier$` parallel to `$simple-template-id$`)
  * add examples of the reflection operator to [basic.fundamental]
  * clarify that overload resolution is performed on splices of functions
  * clarify parsing and semantic rules for [expr.reflect]
  * define special rules for consteval-only types in [expr.const]
  * restrict _plainly constant-evaluated expression_s to only constexpr/constinit initializers and static-assert declarations
  * introduce an IFNDR condition to [expr.const] when injected declarations are unsequenced or indeterminaly sequenced (note: removed in R9)
  * NTTPs and pack-index-expressions cannot appear as operands of the reflection operator
  * properly handle type splicers in CTAD and placeholder types
  * add a new [dcl.type.splice] section for type splicers
  * introduce notions of _data member description_ and _direct base class relationship_ to assist with specification of `data_member_spec` and `bases_of`
  * flesh out handling of `$splice-template-argument$`s in [temp.arg.general]

Since [@P2996R6]:

* removed the `accessible_members` family of functions
* added the `get_public` family of functions
* added missing `tuple` and `variant` traits
* added missing `is_mutable_member` function
* added `(u8)operator_symbol_of` functions, tweaked enumerator names in `std::meta::operators`
* stronger guarantees on order reflections returned by `members_of`
* several core wording fixes
* added `is_user_declared` for completeness with `is_user_provided`

Since [@P2996R5]:

* fixed broken "Emulating typeful reflection" example.
* removed linkage restrictions on objects of consteval-only type that were introduced in St. Louis.
* make friends with modules: define _injected declarations_ and _injected points_, as well as the _evaluation context_; modify _TU-local_ and related definitions, clarify behavior of `members_of` and `define_class`. An informal elaboration on this is included in a new section on "Reachability and injected declarations".
* `type_of` no longer returns reflections of `$typedef-name$`s; added elaboration of reasoning to the ["Handling Aliases"](#handling-aliases) section.
* added `define_static_array`, `has_complete_definition`.
* removed `subobjects_of` and `accessible_subobjects_of` (will be reintroduced by [@P3293R1]).
* specified constraints for `enumerators_of` in terms of `has_complete_definition`.
* constraints on type template parameter of `reflect_{value, object, function}` are expressed as mandates.
* changed `is_special_member` to `is_special_member_function` to align with core language terminology.
* revised wording for several metafunctions (`(u8)identifier_of`, `has_identifier`, `extract`, `data_member_spec`, `define_class`, `reflect_invoke`, `source_location_of`).
* more changes and additions to core language wording.
* minor edits: "representing" instead of "reflecting"; "ordinary ~~string~~ literal encoding"; prefer "`$typedef-name$`" over "alias of a type" in formal wording.

Since [@P2996R4]:

* removed filters from query functions
* cleaned up accessibility interface, removed `access_pair` type, and redid API to be based on an `access_context`
* reduced specification of `is_noexcept`
* changed `span<info const>` to `initializer_list<info>`
* removed `test_trait`
* removed `(u8)name_of` and `(u8)qualified_name_of`; added `(u8)identifier_of`, `operator_of`, `define_static_string`.
* renamed `display_name_of` to `display_string_of`
* adding a number of missing predicates: `is_enumerator`, `is_copy_constructor`, `is_move_constructor`, `is_assignment`, `is_move_assignment`, `is_copy_assignment`, `is_default_constructor`, `has_default_member_initializer`, `is_lvalue_reference_qualified`, `is_rvalue_reference_qualified`, `is_literal_operator(_template)`, `is_conversion_function(_template)`, `is_operator(_template)`, `is_data_member_spec`, `has_(thread|automatic)_storage_duration`
* changed offset API to be one function that returns a type with named members
* Tightened constraints on calls to `data_member_spec`, and defined comparison among reflections returned by it.
* changed `is_alias` to `is_(type|namespace)_alias`
* changed `is_incomplete_type` to `is_complete_type`
* Many wording updates in response to feedback from CWG.

Since [@P2996R3]:

* changes to name functions to improve Unicode-friendliness; added `u8name_of`, `u8qualified_name_of`, `u8display_name_of`.
* the return of `reflect_value`: separated `reflect_result` into three functions: `reflect_value`, `reflect_object`, `reflect_function`
* more strongly specified comparison and linkage rules for reflections of aliases
* changed `is_noexcept` to apply to a wider class of entities
* reworked the API for reflecting on accessible class members
* renamed `test_type` and `test_types` to `test_trait`
* added missing `has_module_linkage` metafunction
* clarified difference between a reflection of a variable and its object; added `object_of` metafunction
* more wording

Since [@P2996R2]:

* many wording changes, additions, and improvements
* elaborated on equivalence among reflections and linkage of templated entities specialized by reflections
* added `accessible_members_of` variants to restore a TS-era agreement
* renamed function previously called `value_of` to `extract`, and expanded it to operate on functions
* clarified support for reflections of values and objects rather than constant expressions
* added Godbolt links to Clang/P2996 implementation
* added `can_substitute`, `is_value`, `is_object`, and (new) `value_of`
* added explanation of a naming issue with the [type traits](#other-type-traits)
* added an alternative [named tuple](#named-tuple) implementation
* made default/value/zero-initializing a `meta::info` yield a null reflection
* added addressed splicing, which is implemented but was omitted from the paper
* added another overload to `reflect_invoke` to support template arguments
* renamed all the type traits to start with `type_` to avoid name clashes. added more generalized `is_const`, `is_final`, and `is_volatile`
* added `is_noexcept` and fixed `is_explicit` to only apply to member functions, not member function templates
* added section on [handling text](#reflecting-source-text)
* added a section discussing ODR concerns

Since [@P2996R1], several changes to the overall library API:

* added `qualified_name_of` (to partner with `name_of`)
* removed `is_static` for being ambiguous, added `has_internal_linkage` (and `has_linkage` and `has_external_linkage`) and `is_static_member` instead
* added `is_class_member`, `is_namespace_member`, and `is_concept`
* added `reflect_invoke`
* added [all the type traits](#other-type-traits)

Other paper changes:

* some updates to examples, including a new examples which add a [named tuple](#named-tuple) and [emulate typeful reflection](#emulating-typeful-reflection).
* more discussion of syntax, constant evaluation order, aliases, and freestanding.
* adding lots of wording

Since [@P2996R0]:

* added links to Compiler Explorer demonstrating just about all of the examples
* respecified `synth_struct` to `define_class`
* respecified a few metafunctions to be functions instead of function templates
* introduced section on error handling mechanism and our preference for exceptions (removing invalid reflections)
* added ticket counter and variant examples
* collapsed `entity_ref` and `pointer_to_member` into `value_of`

# Introduction

This is a proposal for a reduced initial set of features to support static reflection in C++.
Specifically, we are mostly proposing a subset of features suggested in [@P1240R2]:

  - the representation of program elements via constant-expressions producing
     _reflection values_ — _reflections_ for short — of an opaque type `std::meta::info`,
  - a _reflection operator_ (prefix `^^`) that computes a reflection value for its operand construct,
  - a number of `consteval` _metafunctions_ to work with reflections (including deriving other reflections), and
  - constructs called _splicers_ to produce grammatical elements from reflections (e.g., `[: $refl$ :]`).

(Note that this aims at something a little broader than pure "reflection".
 We not only want to observe the structure of the program: We also want to ease generating code that depends on those observations.
 That combination is sometimes referred to as "reflective metaprogramming", but within WG21 discussion the term "reflection" has often been used informally to refer to the same general idea.)

This proposal is not intended to be the end-game as far as reflection and compile-time
metaprogramming are concerned.  Instead, we expect it will be a useful core around which more
powerful features will be added incrementally over time.  In particular, we believe that most
or all the remaining features explored in P1240R2 and that code injection
(along the lines described in [@P2237R0]) are desirable directions to pursue.

Our choice to start with something smaller is primarily motivated by the belief that that
improves the chances of these facilities making it into the language sooner rather than
later.

## Notable Additions to P1240

While we tried to select a useful subset of the P1240 features, we also made a few additions and changes.
Most of those changes are minor.
For example, we added a `std::meta::test_trait` interface that makes it convenient to use existing standard type predicates (such as `is_class_v`) in reflection computations.

One addition does stand out, however: We have added metafunctions that permit the synthesis of simple struct and union types.
While it is not nearly as powerful as generalized code injection (see [@P2237R0]), it can be remarkably effective in practice.

## Why a single opaque reflection type?

Perhaps the most common suggestion made regarding the framework outlined in P1240 is to
switch from the single `std::meta::info` type to a family of types covering various
language elements (e.g., `std::meta::variable`, `std::meta::type`, etc.).

We believe that doing so would be a mistake with very serious consequences for the future of C++.

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


## Implementation Status

Lock3 implemented the equivalent of much that is proposed here in a fork of Clang (specifically, it worked with the P1240 proposal, but also included several other capabilities including a first-class injection mechanism).

EDG has an ongoing implementation of this proposal that is currently available on Compiler Explorer (thank you, Matt Godbolt).

Additionally, Bloomberg has open sourced a fork of Clang which provides a second implementation of this proposal, also available on Compiler Explorer (again thank you, Matt Godbolt), which can be found here: [https://github.com/bloomberg/clang-p2996](https://github.com/bloomberg/clang-p2996).

Neither implementation is complete, but all significant features proposed by this paper have been implemented by at least one implementation (including namespace and template splicers). Both implementations have their "quirks" and continue to evolve alongside this paper.

Nearly all of the examples below have links to Compiler Explorer demonstrating them in both EDG and Clang.

The implementations notably lack some of the other proposed language features that dovetail well with reflection; most notably, expansion statements are absent.
A workaround that will be used in the linked implementations of examples is the following facility:

::: std
```cpp
namespace __impl {
  template<auto... vals>
  struct replicator_type {
    template<typename F>
      constexpr void operator>>(F body) const {
        (body.template operator()<vals>(), ...);
      }
  };

  template<auto... vals>
  replicator_type<vals...> replicator = {};
}

template<typename R>
consteval auto expand(R range) {
  std::vector<std::meta::info> args;
  for (auto r : range) {
    args.push_back(reflect_constant(r));
  }
  return substitute(^^__impl::replicator, args);
}
```

:::

Used like:

::: cmptable
### With expansion statements
```cpp
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
    if (value == [:e:]) {
      return std::string(std::meta::identifier_of(e));
    }
  }

  return "<unnamed>";
}
```

### With `expand` workaround
```cpp
template<typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  std::string result = "<unnamed>";
  [:expand(std::meta::enumerators_of(^^E)):] >> [&]<auto e>{
    if (value == [:e:]) {
      result = std::meta::identifier_of(e);
    }
  };
  return result;
}
```
:::

# Examples

We start with a number of examples that show off what is possible with the proposed set of features.
It is expected that these are mostly self-explanatory.
Read ahead to the next sections for a more systematic description of each element of this proposal.

A number of our examples here show a few other language features that we hope to progress at the same time. This facility does not strictly rely on these features, and it is possible to do without them - but it would greatly help the usability experience if those could be adopted as well:

* [@P1306R2]{.title}
* [@P3289R1]{.title}
* non-transient constexpr allocation – [@P0784R7]{.title}, [@P1974R0]{.title}, [@P2670R1]{.title}, [@P3554R0]{.title}

## Back-And-Forth

Our first example is not meant to be compelling but to show how to go back and forth between the reflection domain and the grammatical domain:

::: std
```c++
constexpr auto r = ^^int;
typename[:r:] x = 42;       // Same as: int x = 42;
typename[:^^char:] c = '*';  // Same as: char c = '*';
```
:::

The `typename` prefix can be omitted in the same contexts as with dependent qualified names (i.e., in what the standard calls _type-only contexts_).
For example:

::: std
```c++
using MyType = [:sizeof(int)<sizeof(long)? ^^long : ^^int:];  // Implicit "typename" prefix.
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/4hK564scs), [Clang](https://godbolt.org/z/71647q5Mo).


## Selecting Members

Our second example enables selecting a member "by number" for a specific type:

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_number(int n) {
  if (n == 0) return ^^S::i;
  else if (n == 1) return ^^S::j;
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (member_number(5) is not a constant).
}
```
:::

This example also illustrates that bit fields are not beyond the reach of this proposal.

On Compiler Explorer: [EDG](https://godbolt.org/z/cKaK4v8nr), [Clang](https://godbolt.org/z/Tb57jEn8a).

Note that a "member access splice" like `s.[:member_number(1):]` is a more direct member access mechanism than the traditional syntax.
It doesn't involve member name lookup, access checking, or --- if the spliced reflection value represents a member function --- overload resolution.

This proposal includes a number of consteval "metafunctions" that enable the introspection of various language constructs.
Among those metafunctions is `std::meta::nonstatic_data_members_of` which returns a vector of reflection values that describe the non-static members of a given type.
We could thus rewrite the above example as:

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_number(int n) {
  auto ctx = std::meta::access_context::current();
  return std::meta::nonstatic_data_members_of(^^S, ctx)[n];
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (member_number(5) is not a constant).
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/7P3ax5K16), [Clang](https://godbolt.org/z/naTTzGebr).

This proposal specifies that namespace `std::meta` is associated with the reflection type (`std::meta::info`); the `std::meta::` qualification can therefore be omitted in the example above.

Another frequently-useful metafunction is `std::meta::identifier_of`, which returns a `std::string_view` describing the identifier with which an entity represented by a given reflection value was declared.
With such a facility, we could conceivably access non-static data members "by string":

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_named(std::string_view name) {
  auto ctx = std::meta::access_context::current();
  for (std::meta::info field : nonstatic_data_members_of(^^S, ctx)) {
    if (has_identifier(field) && identifier_of(field) == name)
      return field;
  }
}

int main() {
  S s{0, 0};
  s.[:member_named("j"):] = 42;  // Same as: s.j = 42;
  s.[:member_named("x"):] = 0;   // Error (member_named("x") is not a constant).
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/hhd9vePW7), [Clang](https://godbolt.org/z/q3c55jsKE).


## List of Types to List of Sizes

Here, `sizes` will be a `std::array<std::size_t, 3>` initialized with `{sizeof(int), sizeof(float), sizeof(double)}`:

::: std
```c++
constexpr std::array types = {^^int, ^^float, ^^double};
constexpr std::array sizes = []{
  std::array<std::size_t, types.size()> r;
  std::ranges::transform(types, r.begin(), std::meta::size_of);
  return r;
}();
```
:::

Compare this to the following type-based approach, which produces the same array `sizes`:

::: std
```c++
template<class...> struct list {};

using types = list<int, float, double>;

constexpr auto sizes = []<template<class...> class L, class... T>(L<T...>) {
    return std::array<std::size_t, sizeof...(T)>{{ sizeof(T)... }};
}(types{});
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/83zK4erj7), [Clang](https://godbolt.org/z/raa87vMjf).

## Implementing `make_integer_sequence`

We can provide a better implementation of `make_integer_sequence` than a hand-rolled approach using regular template metaprogramming (although standard libraries today rely on an intrinsic for this):

::: std
```c++
#include <utility>
#include <vector>

template<typename T>
consteval std::meta::info make_integer_seq_refl(T N) {
  std::vector args{^^T};
  for (T k = 0; k < N; ++k) {
    args.push_back(std::meta::reflect_constant(k));
  }
  return substitute(^^std::integer_sequence, args);
}

template<typename T, T N>
  using make_integer_sequence = [:make_integer_seq_refl<T>(N):];
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/G3TM9Tbad), [Clang](https://godbolt.org/z/57bcYqbv8).

Note that the memoization implicit in the template substitution process still applies.
So having multiple uses of, e.g., `make_integer_sequence<int, 20>` will only involve one evaluation of `make_integer_seq_refl<int>(20)`.


## Getting Class Layout

::: std
```c++
struct member_descriptor
{
  std::size_t offset;
  std::size_t size;
};

// returns std::array<member_descriptor, N>
template <typename S>
consteval auto get_layout() {
  constexpr auto ctx = std::meta::access_context::current();
  constexpr size_t N = std::meta::nonstatic_data_members_of(^^S, ctx).size();
  auto members = std::meta::nonstatic_data_members_of(^^S, ctx);

  std::array<member_descriptor, N> layout;
  for (int i = 0; i < members.size(); ++i) {
      layout[i] = {
          .offset=static_cast<std::size_t>(std::meta::offset_of(members[i]).bytes),
          .size=std::meta::size_of(members[i])
      };
  }
  return layout;
}

struct X
{
    char a;
    int b;
    double c;
};

/*constexpr*/ auto Xd = get_layout<X>();

/*
where Xd would be std::array<member_descriptor, 3>{@{@
  { 0, 1 }, { 4, 4 }, { 8, 8 }
}}
*/
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/ss9hfaMKT), [Clang](https://godbolt.org/z/doe83nGze).

## Enum to String

One of the most commonly requested facilities is to convert an enum value to a string (this example relies on expansion statements):

::: std
```c++
template<typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr std::string_view enum_to_string(E value) {
  if constexpr (Enumerable)
    template for (constexpr auto e :
                  std::define_static_array(std::meta::enumerators_of(^^E)))
      if (value == [:e:])
        return std::meta::identifier_of(e);

  return "<unnamed>";
}

int main() {
  enum Color : int;
  static_assert(enum_to_string(Color(0)) == "<unnamed>");
  std::println("Color 0: {}", enum_to_string(Color(0)));  // prints '<unnamed>'

  enum Color : int { red, green, blue };
  static_assert(enum_to_string(Color::red) == "red");
  static_assert(enum_to_string(Color(42)) == "<unnamed>");
  std::println("Color 0: {}", enum_to_string(Color(0)));  // prints 'red'
}
```
:::

We can also do the reverse in pretty much the same way:

::: std
```c++
template <typename E, bool Enumerable = std::meta::is_enumerable_type(^^E)>
  requires std::is_enum_v<E>
constexpr std::optional<E> string_to_enum(std::string_view name) {
  if constexpr (Enumerable)
    template for (constexpr auto e :
                  std::define_static_array(std::meta::enumerators_of(^^E)))
      if (name == std::meta::identifier_of(e))
        return [:e:];

  return std::nullopt;
}
```
:::

But we don't have to use expansion statements - we can also use algorithms. For instance, `enum_to_string` can also be implemented this way (this example relies on non-transient constexpr allocation), which also demonstrates choosing a different algorithm based on the number of enumerators:

::: std
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  constexpr auto get_pairs = []{
    return std::meta::enumerators_of(^^E)
      | std::views::transform([](std::meta::info e){
          return std::pair<E, std::string>(std::meta::extract<E>(e), std::meta::identifier_of(e));
        })
  };

  constexpr auto get_name = [](E value) -> std::optional<std::string> {
    if constexpr (enumerators_of(^^E).size() <= 7) {
      // if there aren't many enumerators, use a vector with find_if()
      constexpr auto enumerators = get_pairs() | std::ranges::to<std::vector>();
      auto it = std::ranges::find_if(enumerators, [value](auto const& pr){
        return pr.first == value;
      });
      if (it == enumerators.end()) {
        return std::nullopt;
      } else {
        return it->second;
      }
    } else {
      // if there are lots of enumerators, use a map with find()
      constexpr auto enumerators = get_pairs() | std::ranges::to<std::map>();
      auto it = enumerators.find(value);
      if (it == enumerators.end()) {
        return std::nullopt;
      } else {
        return it->second;
      }
    }
  };

  return get_name(value).value_or("<unnamed>");
}
```
:::

Note that this last version has lower complexity: While the versions using an expansion statement use an expected O(N) number of comparisons to find the matching entry, a `std::map` achieves the same with O(log(N)) complexity (where N is the number of enumerator constants).

On Compiler Explorer: [EDG](https://godbolt.org/z/hf777PfGo), [Clang](https://godbolt.org/z/TTxMs4fMa).


Many many variations of these functions are possible and beneficial depending on the needs of the client code.
For example:

  - the "\<unnamed>" case could instead output a valid cast expression like "E(5)"
  - a more sophisticated lookup algorithm could be selected at compile time depending on the length of `enumerators_of(^^E)`
  - a compact two-way persistent data structure could be generated to support both `enum_to_string` and `string_to_enum` with a minimal footprint
  - etc.


## Parsing Command-Line Options

Our next example shows how a command-line option parser could work by automatically inferring flags based on member names. A real command-line parser would of course be more complex, this is just the beginning.

::: std
```c++
template<typename Opts>
auto parse_options(std::span<std::string_view const> args) -> Opts {
  Opts opts;

  constexpr auto ctx = std::meta::access_context::current();
  template for (constexpr auto dm : nonstatic_data_members_of(^^Opts, ctx)) {
    auto it = std::ranges::find_if(args,
      [](std::string_view arg){
        return arg.starts_with("--") && arg.substr(2) == identifier_of(dm);
      });

    if (it == args.end()) {
      // no option provided, use default
      continue;
    } else if (it + 1 == args.end()) {
      std::print(stderr, "Option {} is missing a value\n", *it);
      std::exit(EXIT_FAILURE);
    }

    using T = typename[:type_of(dm):];
    auto iss = std::ispanstream(it[1]);
    if (iss >> opts.[:dm:]; !iss) {
      std::print(stderr, "Failed to parse option {} into a {}\n", *it, display_string_of(^^T));
      std::exit(EXIT_FAILURE);
    }
  }
  return opts;
}

struct MyOpts {
  std::string file_name = "input.txt";  // Option "--file_name <string>"
  int    count = 1;                     // Option "--count <int>"
};

int main(int argc, char *argv[]) {
  MyOpts opts = parse_options<MyOpts>(std::vector<std::string_view>(argv+1, argv+argc));
  // ...
}
```
:::

This example is based on a presentation by Matúš Chochlík.

On Compiler Explorer: [EDG](https://godbolt.org/z/jGfGv84oh), [Clang](https://godbolt.org/z/5rYqnYWq4).


## A Simple Tuple Type

::: std
```c++
#include <meta>

template<typename... Ts> struct Tuple {
  struct storage;
  consteval {
    define_aggregate(^^storage, {data_member_spec(^^Ts)...})
  }
  storage data;

  Tuple(): data{} {}
  Tuple(Ts const& ...vs): data{ vs... } {}
};

template<typename... Ts>
  struct std::tuple_size<Tuple<Ts...>>: public integral_constant<size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
  struct std::tuple_element<I, Tuple<Ts...>> {
    static constexpr std::array types = {^^Ts...};
    using type = [: types[I] :];
  };

consteval std::meta::info get_nth_field(std::meta::info r, std::size_t n) {
  return nonstatic_data_members_of(r, std::meta::access_context::current())[n];
}

template<std::size_t I, typename... Ts>
  constexpr auto get(Tuple<Ts...> &t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>>& {
    return t.data.[:get_nth_field(^^decltype(t.data), I):];
  }
// Similarly for other value categories...
```
:::

This example uses a "magic" `std::meta::define_aggregate` template along with member reflection through the `nonstatic_data_members_of` metafunction to implement a `std::tuple`-like type without the usual complex and costly template metaprogramming tricks that that involves when these facilities are not available.
`define_aggregate` takes a reflection for an incomplete class or union plus a vector of non-static data member descriptions, and completes the give class or union type to have the described members.

On Compiler Explorer: [EDG](https://godbolt.org/z/76EojjcEe), [Clang](https://godbolt.org/z/E9hxqKzE9).

## A Simple Variant Type

Similarly to how we can implement a tuple using `define_aggregate` to create on the fly a type with one member for each `Ts...`, we can implement a variant that simply defines a `union` instead of a `struct`.
One difference here is how the destructor of a `union` is currently defined:

::: std
```cpp
union U1 {
  int i;
  char c;
};

union U2 {
  int i;
  std::string s;
};
```
:::

`U1` has a trivial destructor, but `U2`'s destructor is defined as deleted (because `std::string` has a non-trivial destructor).
This is a problem because we need to define this thing... somehow.
However, for the purposes of `define_aggregate`, there really is only one reasonable option to choose here:

::: std
```cpp
template <class... Ts>
union U {
  // all of our members
  Ts... members;

  // a defaulted destructor if all of the types are trivially destructible
  constexpr ~U() requires (std::is_trivially_destructible_v<Ts> && ...) = default;

  // ... otherwise a destructor that does nothing
  constexpr ~U() { }
};
```
:::

If we make [`define_aggregate`](#data_member_spec-define_aggregate) for a `union` have this behavior, then we can implement a `variant` in a much more straightforward way than in current implementations.
This is not a complete implementation of `std::variant` (and cheats using libstdc++ internals, and also uses Boost.Mp11's `mp_with_index`) but should demonstrate the idea:

::: std
```cpp
template <typename... Ts>
class Variant {
    union Storage;
    struct Empty { };

    consteval {
      define_aggregate(^^Storage, {
          data_member_spec(^^Empty, {.name="empty"}),
          data_member_spec(^^Ts)...
      });
    }

    static consteval std::meta::info get_nth_field(std::size_t n) {
      auto ctx = std::meta::access_context::current();
      return nonstatic_data_members_of(^^Storage, ctx)[n+1];
    }

    Storage storage_;
    int index_ = -1;

    // cheat: use libstdc++'s implementation
    template <typename T>
    static constexpr size_t accepted_index = std::__detail::__variant::__accepted_index<T, std::variant<Ts...>>;

    template <class F>
    constexpr auto with_index(F&& f) const -> decltype(auto) {
        return mp_with_index<sizeof...(Ts)>(index_, (F&&)f);
    }

public:
    constexpr Variant() requires std::is_default_constructible_v<Ts...[0]>
        // should this work: storage_{. [: get_nth_field(0) :]{} }
        : storage_{.empty={}}
        , index_(0)
    {
        std::construct_at(&storage_.[: get_nth_field(0) :]);
    }

    constexpr ~Variant() requires (std::is_trivially_destructible_v<Ts> and ...) = default;
    constexpr ~Variant() {
        if (index_ != -1) {
            with_index([&](auto I){
                std::destroy_at(&storage_.[: get_nth_field(I) :]);
            });
        }
    }

    template <typename T, size_t I = accepted_index<T&&>>
        requires (!std::is_base_of_v<Variant, std::decay_t<T>>)
    constexpr Variant(T&& t)
        : storage_{.empty={}}
        , index_(-1)
    {
        std::construct_at(&storage_.[: get_nth_field(I) :], (T&&)t);
        index_ = (int)I;
    }

    // you can't actually express this constraint nicely until P2963
    constexpr Variant(Variant const&) requires (std::is_trivially_copyable_v<Ts> and ...) = default;
    constexpr Variant(Variant const& rhs)
            requires ((std::is_copy_constructible_v<Ts> and ...)
                and not (std::is_trivially_copyable_v<Ts> and ...))
        : storage_{.empty={}}
        , index_(-1)
    {
        rhs.with_index([&](auto I){
            constexpr auto field = get_nth_field(I);
            std::construct_at(&storage_.[: field :], rhs.storage_.[: field :]);
            index_ = I;
        });
    }

    constexpr auto index() const -> int { return index_; }

    template <class F>
    constexpr auto visit(F&& f) const -> decltype(auto) {
        if (index_ == -1) {
            throw std::bad_variant_access();
        }

        return mp_with_index<sizeof...(Ts)>(index_, [&](auto I) -> decltype(auto) {
            return std::invoke((F&&)f,  storage_.[: get_nth_field(I) :]);
        });
    }
};
```
:::

Effectively, `Variant<T, U>` synthesizes a union type `Storage` which looks like this:

::: std
```cpp
union Storage {
    Empty empty;
    T @*unnamed~0~*@;
    U @*unnamed~1~*@;

    ~Storage() requires std::is_trivially_destructible_v<T> && std::is_trivially_destructible_v<U> = default;
    ~Storage() { }
}
```
:::

The question here is whether we should be should be able to directly initialize members of a defined union using a splicer, as in:

::: std
```cpp
: storage{.[: get_nth_field(0) :]={}}
```
:::

Arguably, the answer should be yes - this would be consistent with how other accesses work. This is instead proposed in [@P3293R1].

On Compiler Explorer: [EDG](https://godbolt.org/z/W74qxqnhf), [Clang](https://godbolt.org/z/eqj6e3Tjr).

## Struct to Struct of Arrays

::: std
```c++
#include <meta>
#include <array>

template <typename T, size_t N>
struct struct_of_arrays_impl {
  struct impl;

  consteval {
    auto ctx = std::meta::access_context::current();

    std::vector<std::meta::info> old_members = nonstatic_data_members_of(^^T, ctx);
    std::vector<std::meta::info> new_members = {};
    for (std::meta::info member : old_members) {
        auto array_type = substitute(^^std::array, {
            type_of(member),
            std::meta::reflect_constant(N),
        });
        auto mem_descr = data_member_spec(array_type, {.name = identifier_of(member)});
        new_members.push_back(mem_descr);
    }

    define_aggregate(^^impl, new_members);
  }
};

template <typename T, size_t N>
using struct_of_arrays = struct_of_arrays_impl<T, N>::impl;
```
:::

Example:

::: std
```c++
struct point {
  float x;
  float y;
  float z;
};

using points = struct_of_arrays<point, 30>;
// equivalent to:
// struct points {
//   std::array<float, 30> x;
//   std::array<float, 30> y;
//   std::array<float, 30> z;
// };
```
:::

Again, the combination of `nonstatic_data_members_of` and `define_aggregate` is put to good use.

This example also illustrates some requirements that we have on `define_aggregate`. In particular, that function is said to produce an "injected declaration" and the target scope of the declaration must be within the same "cone of instantiation" as the evaluation that produced it. Which means that the following similar structure is ill-formed:

::: std
```cpp
template <class T, size_t N>
struct struct_of_arrays_impl;

template <typename T, size_t N>
using struct_of_arrays = [: []{
  // ... same logic ..

  // error: the target scope of this declaration is a
  // different instantiation from the one we are currently in.
  define_aggregate(^^struct_of_arrays_impl<T, N>, new_members);
}() :];
```
:::

That could be fixed if we reorganize it like this:

::: std
```cpp
template <typename T, size_t N>
using struct_of_arrays = [: []{
  // ... same logic ..

  // OK, same instantiation
  struct impl;
  define_aggregate(^^impl, new_members);
}() :];
```
:::

But now `struct_of_arrays<point, 30>` has no linkage, whereas we wanted it to have external linkage. Hence the structure in the example above where we are instead defining a nested class in a class template — so that we have a type with external linkage but don't run afoul of the "cone of instantiation" rule.

On Compiler Explorer: [EDG](https://godbolt.org/z/jWrPGhn5s), [Clang](https://godbolt.org/z/vqxzMoPcj).


## Parsing Command-Line Options II

Now that we've seen a couple examples of using `std::meta::define_aggregate` to create a type, we can create a more sophisticated command-line parser example.

This is the opening example for [clap](https://docs.rs/clap/latest/clap/) (Rust's **C**ommand **L**ine **A**rgument **P**arser):

::: std
```c++
struct Args : Clap {
  Option<std::string, {.use_short=true, .use_long=true}> name;
  Option<int, {.use_short=true, .use_long=true}> count = 1;
};

int main(int argc, char** argv) {
  auto opts = Args{}.parse(argc, argv);

  for (int i = 0; i < opts.count; ++i) {  // opts.count has type int
    std::print("Hello {}!", opts.name);   // opts.name has type std::string
  }
}
```
:::

Which we can implement like this:

::: std
```c++
struct Flags {
  bool use_short;
  bool use_long;
};

template <typename T, Flags flags>
struct Option {
  std::optional<T> initializer = {};

  // some suitable constructors and accessors for flags
};

// convert a type (all of whose non-static data members are specializations of Option)
// to a type that is just the appropriate members.
// For example, if type is a reflection of the Args presented above, then this
// function would evaluate to a reflection of the type
// struct {
//   std::string name;
//   int count;
// }
consteval auto spec_to_opts(std::meta::info opts,
                            std::meta::info spec) -> std::meta::info {
  auto ctx = std::meta::access_context::current();

  std::vector<std::meta::info> new_members;
  for (std::meta::info member : nonstatic_data_members_of(spec, ctx)) {
    auto type_new = template_arguments_of(type_of(member))[0];
    new_members.push_back(data_member_spec(type_new, {.name=identifier_of(member)}));
  }
  return define_aggregate(opts, new_members);
}

struct Clap {
  template <typename Spec>
  auto parse(this Spec const& spec, int argc, char** argv) {
    std::vector<std::string_view> cmdline(argv+1, argv+argc)

    // check if cmdline contains --help, etc.

    struct Opts;
    consteval {
      spec_to_opts(^^Opts, ^^Spec);
    }

    constexpr auto ctx = std::meta::access_context::current();
    template for (constexpr auto [sm, om] :
                  std::define_static_array(
                      std::views::zip(nonstatic_data_members_of(^^Spec, ctx),
                                      nonstatic_data_members_of(^^Opts, ctx)) |
                      std::views::transform([](auto z) { return std::pair(get<0>(z), get<1>(z)); }))) {
      auto const& cur = spec.[:sm:];
      constexpr auto type = type_of(om);

      // find the argument associated with this option
      auto it = std::ranges::find_if(cmdline,
        [&](std::string_view arg){
          return (cur.use_short && arg.size() == 2 && arg[0] == '-' && arg[1] == identifier_of(sm)[0])
              || (cur.use_long && arg.starts_with("--") && arg.substr(2) == identifier_of(sm));
        });

      // no such argument
      if (it == cmdline.end()) {
        if constexpr (has_template_arguments(type) and template_of(type) == ^^std::optional) {
          // the type is optional, so the argument is too
          continue;
        } else if (cur.initializer) {
          // the type isn't optional, but an initializer is provided, use that
          opts.[:om:] = *cur.initializer;
          continue;
        } else {
          std::print(stderr, "Missing required option {}\n", display_string_of(sm));
          std::exit(EXIT_FAILURE);
        }
      } else if (it + 1 == cmdline.end()) {
        std::print(stderr, "Option {} for {} is missing a value\n", *it, display_string_of(sm));
        std::exit(EXIT_FAILURE);
      }

      // found our argument, try to parse it
      auto iss = ispanstream(it[1]);
      if (iss >> opts.[:om:]; !iss) {
        std::print(stderr, "Failed to parse {:?} into option {} of type {}\n",
          it[1], display_string_of(sm), display_string_of(type));
        std::exit(EXIT_FAILURE);
      }
    }
    return opts;
  }
};
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/4aseo5eGq), [Clang](https://godbolt.org/z/Yjv1dM4eK).

## A Universal Formatter

This example is taken from Boost.Describe:

::: std
```cpp
struct universal_formatter {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }

  template <typename T>
  auto format(T const& t, auto& ctx) const {
    auto out = std::format_to(ctx.out(), "{}@{@{", has_identifier(^^T) ? identifier_of(^^T)
                                                                      : "(unnamed-type)";);

    auto delim = [first=true]() mutable {
      if (!first) {
        *out++ = ',';
        *out++ = ' ';
      }
      first = false;
    };

    constexpr auto ctx = std::meta::access_context::unchecked();

    template for (constexpr auto base : define_static_array(bases_of(^^T, ctx))) {
      delim();
      out = std::format_to(out, "{}", (typename [: type_of(base) :] const&)(t));
    }

    template for (constexpr auto mem :
                  define_static_array(nonstatic_data_members_of(^^T, ctx))) {
      delim();
      std::string_view mem_label = has_identifier(mem) ? identifier_of(mem)
                                                       : "(unnamed-member)";
      out = std::format_to(out, ".{}={}", mem_label, t.[:mem:]);
    }

    *out++ = '}';
    return out;
  }
};

struct B { int m0 = 0; };
struct X { int m1 = 1; };
struct Y { int m2 = 2; };
class Z : public X, private Y { int m3 = 3; int m4 = 4; };

template <> struct std::formatter<B> : universal_formatter { };
template <> struct std::formatter<X> : universal_formatter { };
template <> struct std::formatter<Y> : universal_formatter { };
template <> struct std::formatter<Z> : universal_formatter { };

int main() {
    std::println("{}", Z());
      // Z{X{B{.m0=0}, .m1 = 1}, Y{{.m0=0}, .m2 = 2}, .m3 = 3, .m4 = 4}
}
```
:::

On Compiler Explorer: [Clang](https://godbolt.org/z/99dTErdG6).

Note that currently, we do not have the ability to access a base class subobject using the `t.[: base :]` syntax - which means that the only way to get at the base is to use a cast:

*  `static_cast<[: type_of(base) const& :]>(t)`, or
*  `(typename [: type_of(base) :] const&)t`

Both have to explicitly specify the `const`-ness of the type in the cast. The `static_cast` additionally has to check access. The C-style cast is one many people find unsavory, though in this case it avoids checking access - but requires writing `typename` since this isn't a type-only context.

## Implementing member-wise `hash_append`

Based on the [@N3980] API:

::: std
```cpp
template <typename H, typename T> requires std::is_standard_layout_v<T>
void hash_append(H& algo, T const& t) {
  constexpr auto ctx = std::meta::access_context::unchecked();
  template for (constexpr auto mem : nonstatic_data_members_of(^^T, ctx)) {
      hash_append(algo, t.[:mem:]);
  }
}
```
:::

Of course, any production-ready `hash_append` would include a facility for classes to opt members in and out of participation in hashing. Annotations as proposed by [@P3394R2]{.title} provides just such a mechanism.

## Converting a Struct to a Tuple

This approach requires allowing packs in structured bindings [@P1061R10], but can also be written using `std::make_index_sequence`:

::: std
```c++
template <typename T>
constexpr auto struct_to_tuple(T const& t) {
  constexpr auto ctx = std::meta::access_context::current();

  constexpr std::size_t N = nonstatic_data_members_of(^^T, ctx).size();
  auto members = nonstatic_data_members_of(^^T, ctx);

  constexpr auto indices = []{
    std::array<int, N> indices;
    std::ranges::iota(indices, 0);
    return indices;
  }();

  constexpr auto [...Is] = indices;
  return std::make_tuple(t.[: members[Is] :]...);
}
```
:::

An alternative approach is:

::: std
```cpp
consteval auto type_struct_to_tuple(info type) -> info {
  constexpr auto ctx = std::meta::access_context::current();
  return substitute(^^std::tuple,
                    nonstatic_data_members_of(type, ctx)
                    | std::views::transform(std::meta::type_of)
                    | std::views::transform(std::meta::remove_cvref)
                    | std::ranges::to<std::vector>());
}

template <typename To, typename From, std::meta::info ... members>
constexpr auto struct_to_tuple_helper(From const& from) -> To {
  return To(from.[:members:]...);
}

template<typename From>
consteval auto get_struct_to_tuple_helper() {
  using To = [: type_struct_to_tuple(^^From): ];
  auto ctx = std::meta::access_context::current();

  std::vector args = {^^To, ^^From};
  for (auto mem : nonstatic_data_members_of(^^From, ctx)) {
    args.push_back(reflect_constant(mem));
  }

  /*
  Alternatively, with Ranges:
  args.append_range(
    nonstatic_data_members_of(^^From, ctx)
    | std::views::transform(std::meta::reflect_constant)
    );
  */

  return extract<To(*)(From const&)>(
    substitute(^^struct_to_tuple_helper, args));
}

template <typename From>
constexpr auto struct_to_tuple(From const& from) {
  return get_struct_to_tuple_helper<From>()(from);
}
```
:::

Here, `type_struct_to_tuple` takes a reflection of a type like `struct { T t; U const& u; V v; }` and returns a reflection of the type `std::tuple<T, U, V>`.
That gives us the return type.
Then, `struct_to_tuple_helper` is a function template that does the actual conversion --- which it can do by having all the reflections of the members as a non-type template parameter pack.
This is a `constexpr` function and not a `consteval` function because in the general case the conversion is a run-time operation.
However, determining the instance of `struct_to_tuple_helper` that is needed is a compile-time operation and has to be performed with a `consteval` function (because the function invokes `nonstatic_data_members_of`), hence the separate function template `get_struct_to_tuple_helper()`.

Everything is put together by using `substitute` to create the instantiation of `struct_to_tuple_helper` that we need, and a compile-time reference to that instance is obtained with `extract`.
Thus `f` is a function reference to the correct specialization of `struct_to_tuple_helper`, which we can simply invoke.

On Compiler Explorer (with a different implementation than either of the above): [EDG](https://godbolt.org/z/1Tffn4vzn), [Clang](https://godbolt.org/z/dn58s5Pvz).

## Implementing `tuple_cat`

Courtesy of Tomasz Kaminski, [on compiler explorer](https://godbolt.org/z/M38b3a7z4):

::: std
```cpp
template<std::pair<std::size_t, std::size_t>... indices>
struct Indexer {
   template<typename Tuples>
   // Can use tuple indexing instead of tuple of tuples
   auto operator()(Tuples&& tuples) const {
     using ResultType = std::tuple<
       std::tuple_element_t<
         indices.second,
         std::remove_cvref_t<std::tuple_element_t<indices.first, std::remove_cvref_t<Tuples>>>
       >...
     >;
     return ResultType(std::get<indices.second>(std::get<indices.first>(std::forward<Tuples>(tuples)))...);
   }
};

template <class T>
consteval auto subst_by_value(std::meta::info tmpl, std::vector<T> args)
    -> std::meta::info
{
    std::vector<std::meta::info> a2;
    for (T x : args) {
        a2.push_back(std::meta::reflect_constant(x));
    }

    return substitute(tmpl, a2);
}

consteval auto make_indexer(std::vector<std::size_t> sizes)
    -> std::meta::info
{
    std::vector<std::pair<int, int>> args;

    for (std::size_t tidx = 0; tidx < sizes.size(); ++tidx) {
        for (std::size_t eidx = 0; eidx < sizes[tidx]; ++eidx) {
            args.push_back({tidx, eidx});
        }
    }

    return subst_by_value(^^Indexer, args);
}

template<typename... Tuples>
auto my_tuple_cat(Tuples&&... tuples) {
    constexpr typename [: make_indexer({tuple_size(remove_cvref(^^Tuples))...}) :] indexer;
    return indexer(std::forward_as_tuple(std::forward<Tuples>(tuples)...));
}
```
:::


## Named Tuple

The tricky thing with implementing a named tuple is actually strings as non-type template parameters.
Because you cannot just pass `"x"` into a non-type template parameter of the form `auto V`, that leaves us with two ways of specifying the constituents:

1. Can introduce a `pair` type so that we can write `make_named_tuple<pair<int, "x">, pair<double, "y">>()`, or
2. Can just do reflections all the way down so that we can write
```cpp
make_named_tuple<^^int, std::meta::reflect_constant("x"),
                 ^^double, std::meta::reflect_constant("y")>()
```

We do not currently support splicing string literals, and the `pair` approach follows the similar pattern already shown with `define_aggregate` (given a suitable `fixed_string` type):

::: std
```cpp
template <class T, fixed_string Name>
struct pair {
    static constexpr auto name() -> std::string_view { return Name.view(); }
    using type = T;
};

template <class... Tags>
consteval auto make_named_tuple(std::meta::info type, Tags... tags) {
    std::vector<std::meta::info> nsdms;
    auto f = [&]<class Tag>(Tag tag){
        nsdms.push_back(data_member_spec(
            dealias(^^typename Tag::type),
            {.name=Tag::name()}));

    };
    (f(tags), ...);
    return define_aggregate(type, nsdms);
}

struct R;
consteval {
  make_named_tuple(^^R, pair<int, "x">{}, pair<double, "y">{});
}

constexpr auto ctx = std::meta::access_context::current();
static_assert(type_of(nonstatic_data_members_of(^^R, ctx)[0]) == ^^int);
static_assert(type_of(nonstatic_data_members_of(^^R, ctx)[1]) == ^^double);

int main() {
    [[maybe_unused]] auto r = R{.x=1, .y=2.0};
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/64qTe4KG1), [Clang](https://godbolt.org/z/rPM5xsbvW).

Alternatively, can side-step the question of non-type template parameters entirely by keeping everything in the value domain:

::: std
```cpp
consteval auto make_named_tuple(std::meta::info type,
                                std::initializer_list<std::pair<std::meta::info, std::string_view>> members) {
    std::vector<std::meta::data_member_spec> nsdms;
    for (auto [type, name] : members) {
        nsdms.push_back(data_member_spec(type, {.name=name}));
    }
    return define_aggregate(type, nsdms);
}

struct R;
consteval {
  make_named_tuple(^^R, {{^^int, "x"}, {^^double, "y"}});
}

constexpr auto ctx = std::meta::access_context::current();
static_assert(type_of(nonstatic_data_members_of(^^R, ctx)[0]) == ^^int);
static_assert(type_of(nonstatic_data_members_of(^^R, ctx)[1]) == ^^double);

int main() {
    [[maybe_unused]] auto r = R{.x=1, .y=2.0};
}
```
:::

On Compiler Explorer: [EDG and Clang](https://godbolt.org/z/oY6ETbv9x) (the EDG and Clang implementations differ only in Clang having the updated `data_member_spec` API that returns an `info`, and the updated name `define_aggregate`).


## Compile-Time Ticket Counter

The features proposed here make it a little easier to update a ticket counter at compile time.
This is not an ideal implementation (we'd prefer direct support for compile-time —-- i.e., `consteval` --- variables), but it shows how compile-time mutable state surfaces in new ways.

::: std
```cpp
template<int N> struct Helper;

struct TU_Ticket {
  static consteval int latest() {
    int k = 0;
    while (is_complete_type(substitute(^^Helper,
                                       { std::meta::reflect_constant(k) })))
      ++k;
    return k;
  }

  static consteval void increment() {
    define_aggregate(substitute(^^Helper,
                                { std::meta::reflect_constant(latest())}),
                     {});
  }
};

constexpr int x = TU_Ticket::latest();  // x initialized to 0.

consteval { TU_Ticket::increment(); }
constexpr int y = TU_Ticket::latest();  // y initialized to 1.

consteval { TU_Ticket::increment(); }
constexpr int z = TU_Ticket::latest();  // z initialized to 2.

static_assert(x == 0);
static_assert(y == 1);
static_assert(z == 2);
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/e1r8q3sWv), [Clang](https://godbolt.org/z/z4KKe5e57).

# Proposed Features

## The Reflection Operator (`^^`)

The reflection operator produces a reflection value from a grammatical construct (its operand):

> | `$unary-expression$`:
> |       ...
> |       `^^` `::`
> |       `^^` `$namespace-name$`
> |       `^^` `$type-id$`
> |       `^^` `$id-expression$`

The expression `^^::` evaluates to a reflection of the global namespace. When the operand is a `$namespace-name$` or `$type-id$`, the resulting value is a reflection of the designated namespace or type.

When the operand is an `$id-expression$`, the resulting value is a reflection of the designated entity found by lookup. This might be any of:

- a variable, static data member, or structured binding
- a function (including member functions)
- a non-static data member
- a template or member template
- an enumerator

For all other operands, the expression is ill-formed. In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to not evaluate to constant.

Earlier revisions of this paper allowed for taking the reflection of any `$cast-expression$` that could be evaluated as a constant expression, as we believed that a constant expression could be internally "represented" by just capturing the value to which it evaluated. However, the possibility of side effects from constant evaluation (introduced by this very paper) renders this approach infeasible: even a constant expression would have to be evaluated every time it's spliced. It was ultimately decided to defer all support for expression reflection, but we intend to introduce it through a future paper using the syntax `^^(expr)`.

This paper does, however, support reflections of _values_ and of _objects_ (including subobjects). Such reflections arise naturally when iterating over template arguments.

::: std
```cpp
template <int P1, const int &P2> void fn() {}

static constexpr int p[2] = {1, 2};
constexpr auto spec = ^^fn<p[0], p[1]>;

static_assert(is_value(template_arguments_of(spec)[0]));
static_assert(is_object(template_arguments_of(spec)[1]));
static_assert(!is_variable(template_arguments_of(spec)[1]));

static_assert([:template_arguments_of(spec)[0]:] == 1);
static_assert(&[:template_arguments_of(spec)[1]:] == &p[1]);
```
:::

Such reflections cannot generally be obtained using the `^^`-operator, but the `std::meta::reflect_constant` and `std::meta::reflect_object` functions make it easy to reflect particular values or objects. The `std::meta::constant_of` metafunction can also be used to map a reflection of an object to a reflection of its value.

### Syntax discussion

The original TS landed on `@[reflexpr]{.cf}@(...)` as the syntax to reflect source constructs and [@P1240R0] adopted that syntax as well.
As more examples were discussed, it became clear that that syntax was both (a) too "heavy" and (b) insufficiently distinct from a function call.
SG7 eventually agreed upon the prefix `^` operator. The "upward arrow" interpretation of the caret matches the "lift" or "raise" verbs that are sometimes used to describe the reflection operation in other contexts.

The caret already has a meaning as a binary operator in C++ ("exclusive OR"), but that is clearly not conflicting with a prefix operator.
In C++/CLI (a Microsoft C++ dialect) the caret is also used as a new kind of `$ptr-operator$` ([dcl.decl.general]) to declare ["handles"](https://learn.microsoft.com/en-us/cpp/extensions/handle-to-object-operator-hat-cpp-component-extensions?view=msvc-170).
That is also not conflicting with the use of the caret as a unary operator because C++/CLI uses the usual prefix `*` operator to dereference handles.

Apple also uses the caret in [syntax "blocks"](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/WorkingwithBlocks/WorkingwithBlocks.html) and unfortunately we believe that does conflict with our proposed use of the caret.

Since the syntax discussions in SG7 landed on the use of the caret, new basic source characters have become available: `@`, `` ` ``{.op}, and `$`{.op}. While we have since discussed some alternatives (e.g., `@` for lifting, `\` and `/` for "raising" and "lowering"), we have grown quite fond of the existing syntax.

In Wrocław 2024, SG7 and EWG voted to adopt `^^` as the new reflection operator (as proposed by [@P3381R0]). The R8 revision of this paper integrates that change.


## Splicers (`[:`...`:]`)

A reflection can be "spliced" into source code using one of several _splicer_ forms:

 - `[: r :]` produces an _expression_ evaluating to the entity represented by `r` in grammatical contexts that permit expressions.  In type-only contexts ([temp.res.general]{.sref}/4), `[: r :]` produces a type (and `r` must be the reflection of a type). In contexts that only permit a namespace name, `[: r :]` produces a namespace (and `r` must be the reflection of a namespace or alias thereof).
 - `typename[: r :]` produces a _simple-type-specifier_ corresponding to the type represented by `r`.
 - `template[: r :]` produces a _template-name_ corresponding to the template represented by `r`.
 - `[:r:]::` produces a _nested-name-specifier_ corresponding to the namespace, enumeration type, or class type represented by `r`.


The operand of a splicer is implicitly converted to a `std::meta::info` prvalue (i.e., if the operand expression has a class type that with a conversion function to convert to `std::meta::info`, splicing can still work).

Attempting to splice a reflection value that does not meet the requirement of the splice is ill-formed.
For example:

::: std
```c++
typename[: ^^:: :] x = 0;  // Error.
```
:::

### Addressed Splicing

In the same way that `&C::mem` can produce a pointer, pointer to member data, pointer to function, or pointer to member function depending on what `mem` refers to, `&[: r :]` can likewise produce the same set of pointers if `r` is a reflection of a suitable entity:

*  If `r` is a reflection of a static data member or a variable, `&[:r:]` is a pointer.
*  Otherwise if `r` is a reflection of a non-static data member, `&[:r:]` is a pointer to data member.
*  Otherwise, if `r` is a reflection of a static member function, a function, or a non-static member function with an explicit object parameter, `&[:r:]` is a pointer to function
*  Otherwise, if `r` is a reflection of a non-static member function with an implicit object parameter, `&[:r:]` is a pointer to member function.
*  Otherwise, if `r` is a reflection of a function template, `&[:r:]` is the address of that overload set - which would then require external context to resolve as usual.

For most members, this doesn't even require any additional wording since that's just what you get when you take the address of the splice based on the current rules we have today.

Now, there are a couple interesting cases to point out when `&[:r:]` isn't just the same as `&X::f`.

When `r` is a reflection of a function or function template that is part of an overload set, overload resolution will not consider the whole overload set, just the specific function or function template that `r` represents:

::: std
```cpp
struct C {
    template <class T> void f(T); // #1
    void f(int); // #2
};

void (C::*p1)(int) = &C::f;  // error: ambiguous

constexpr auto f1 =
    ((members_of(^^C) |
      std::views::filter(std::meta::is_function_template)).front());
constexpr auto f2 =
    ((members_of(^^C) |
      std::views::filter(std::meta::is_function)).front());
void (C::*p2)(int) = &[:f1:]; // ok, refers to C::f<int> (#1)
void (C::*p3)(int) = &[:f2:]; // ok, refers to C::f      (#2)
```
:::

Another interesting question is what does this mean when `r` is the reflection of a constructor or destructor? Consider the type:

::: std
```cpp
struct X {
    X(int, int);
};
```
:::

And let `rc` be a reflection of the constructor and `rd` be a reflection of the destructor. The sensible syntax and semantics for how you would use `rc` and `rd` should be as follows:

::: std
```cpp
auto x = [: rc :](1, 2); // gives you an X
x.[: rd :]();            // destroys it
```
:::

Or, with pointers:

::: std
```cpp
auto pc = &[: rc :];
auto pd = &[: rd :];

auto x = (*pc)(1, 2);   // gives you an X
(x.*pd)();              // destroys it
```
:::

That is, splicing a constructor behaves like a free function that produces an object of that type, so `&[: rc :]` has type `X(*)(int, int)`. On the other hand, splicing a destructor behaves like a regular member function, so `&[: rd :]` has type `void (X::*)()`.

However, we are _not_ proposing splicing constructors or destructors at the moment.

### Limitations

Splicers can appear in many contexts, but our implementation experience has uncovered a small set of circumstances in which a splicer must be disallowed. Mostly these are because any entity designated by a splicer can be dependent on a template argument, so any context in which the language already disallows a dependent name must also disallow a dependent splicer. It also becomes possible for the first time to have the "name" of a namespace or concept become dependent on a template argument. Our implementation experience has helped to sort through which uses of these dependent names pose no difficulties, and which must be disallowed.

This proposal places the following limitations on splicers.

#### Splicing reflections of constructors

Iterating over the members of a class (e.g., using `std::meta::members_of`) allows one, for the first time, to obtain "handles" representing constructors. An immediate question arises of whether it's possible to reify these constructors to construct objects, or even to take their address. While we are very interested in exploring these ideas, we defer their discussion to a future paper; this proposal disallows splicing a reflection of a constructor (or constructor template) in any context.

#### Splicing namespaces in namespace definitions

::: std
```cpp
namespace A {}
constexpr std::meta::info NS_A = ^^A;

namespace B {
  namespace [:NS_A:] {
    void fn();  // Is this '::A::fn' or '::B::A::fn' ?
  }
}
```
:::

We found no satisfying answer as to how to interpret examples like the one given above. Neither did we find motivating use cases: many of the "interesting" uses for reflections of namespaces are either to introspect their members, or to pass them as template arguments - but the above example does nothing to help with introspection, and neither can namespaces be reopened within any dependent context. Rather than choose between unintuitive options for a syntax without a motivating use case, we are disallowing splicers from appearing in the opening of a namespace.

#### Splicing namespaces in using-directives and using-enum-declarators

::: std
```cpp
template <std::meta::info R> void fn1() {
  using enum [:R:]::EnumCls;  // #1
  // ...
}
template <std::meta::info R> void fn2() {
  using namespace [:R:];      // #2
  // ...
}
```
:::

C++20 already disallowed dependent enumeration types from appearing in _using-enum-declarators_ (as in #1), as it would otherwise force the parser to consider every subsequent identifier as possibly a member of the substituted enumeration type. We extend this limitation to splices of dependent reflections of enumeration types, and further disallow the use of dependent reflections of namespaces in _using-directives_ (as in #2) following the same principle.

#### Splicing concepts in declarations of template parameters

::: std
```cpp
template <typename T> concept C = requires { requires true; };

template <std::meta::info R> struct Outer {
  template <template [:R:] S> struct Inner { /* ... */ };
};
```
:::

What kind of parameter is `S`? If `R` represents a class template, then it is a non-type template parameter of deduced type, but if `R` represents a concept, it is a type template parameter. There is no other circumstance in the language for which it is not possible to decide at parse time whether a template parameter is a type or a non-type, and we don't wish to introduce one for this use case.

The most obvious solution would be to introduce a `concept [:R:]` syntax that requires that `R` reflect a concept, and while this could be added going forward, we weren't convinced of its value at this time - especially since the above can easily be rewritten:

::: std
```cpp
template <std::meta::info R> struct Outer {
  template <typename T> requires template [:R:]<T>
  struct Inner { /* ... */ };
};
```
:::

We are resolving this ambiguity by simply disallowing a reflection of a concept, whether dependent or otherwise, from being spliced in the declaration of a template parameter (thus in the above example, the parser can assume that `S` is a non-type parameter).

#### Splicing class members as designators in designated-initializer-lists

```cpp
struct S { int a; };

constexpr S s = {.[:^^S::a:] = 2};
```

Although we would like for splices of class members to be usable as designators in an initializer-list, we lack implementation experience with the syntax and would first like to verify that there are no issues with dependent reflections. We are very likely to propose this as an extension in a future paper.

### Range Splicers

The splicers described above all take a single object of type `std::meta::info` (described in more detail below).
However, there are many cases where we don't have a single reflection, we have a range of reflections - and we want to splice them all in one go.
For that, the predecessor to this paper, [@P1240R0], proposed an additional form of splicer: a range splicer.

Construct the [struct-to-tuple](#converting-a-struct-to-a-tuple) example from above. It was demonstrated using a single splice, but it would be simpler if we had a range splice:

::: cmptable
### With Single Splice
```c++
template <typename T>
constexpr auto struct_to_tuple(T const& t) {
  constexpr auto members = nonstatic_data_members_of(^^T);

  constexpr auto indices = []{
    std::array<int, members.size()> indices;
    std::ranges::iota(indices, 0);
    return indices;
  }();

  constexpr auto [...Is] = indices;
  return std::make_tuple(t.[: members[Is] :]...);
}
```

### With Range Splice
```c++
template <typename T>
constexpr auto struct_to_tuple(T const& t) {
  constexpr auto members = nonstatic_data_members_of(^^T);
  return std::make_tuple(t.[: ...members :]...);
}
```
:::

A range splice, `[: ... r :]`, would accept as its argument a constant range of `meta::info`, `r`, and  would behave as an unexpanded pack of splices. So the above expression

::: std
```c++
make_tuple(t.[: ... members :]...)
```
:::

would evaluate as

::: std
```c++
make_tuple(t.[:members[0]:], t.[:members[1]:], ..., t.[:members[$N-1$]:])
```
:::

This is a very useful facility indeed!

However, range splicing of dependent arguments is at least an order of magnitude harder to implement than ordinary splicing. We think that not including range splicing gives us a better chance of having reflection in C++26.
Especially since, as this paper's examples demonstrate, a lot can be done without them.

Another way to work around a lack of range splicing would be to implement `with_size<N>(f)`, which would behave like `f(integral_constant<size_t, 0>{}, integral_constant<size_t, 1>{}, ..., integral_constant<size_t, N-1>{})`.
Which is enough for a tolerable implementation:

::: std
```c++
template <typename T>
constexpr auto struct_to_tuple(T const& t) {
  constexpr auto members = nonstatic_data_members_of(^^T);
  return with_size<members.size()>([&](auto... Is){
    return std::make_tuple(t.[: members[Is] :]...);
  });
}
```
:::

### Syntax discussion

Early discussions of splice-like constructs (related to the TS design) considered using `@[unreflexpr]{.cf}@(...)` for that purpose.
[@P1240R0] adopted that option for _expression_ splicing, observing that a single splicing syntax could not viably be parsed (some disambiguation is needed to distinguish types and templates).
SG-7 eventually agreed to adopt the `[: ... :]` syntax --- with disambiguating tokens such as `typename` where needed --- which is a little lighter and more distinctive.

We propose `[:` and `:]` be single tokens rather than combinations of `[`, `]`, and `:`.
Among others, it simplifies the handling of expressions like `arr[[:refl():]]`.
On the flip side, it requires a special rule like the one that was made to handle `<::` to leave the meaning of `arr[::N]` unchanged and another one to avoid breaking a (somewhat useless) attribute specifier of the form `[[using ns:]]`.

A syntax that is delimited on the left and right is useful here because spliced expressions may involve lower-precedence operators. Additionally, it's important that the left- and right-hand delimiters are different so as to allow nested splices when that comes up.

However, there are other possibilities.
For example, now that `$`{.op} or `@`{.op} are available in the basic source character set, we might consider those. One option that was recently brought up was `@ $primary-expression$` which would allow writing `@e` for the simple `$identifier$` splices but for the more complex operations still require parenthesizing for readability. `@[$]{.op}@<$expr$>` is somewhat natural to those of us that have used systems where `$`{.op} is used to expand placeholders in document templates:

|`[::]`|`[: :]` (with space)|`@`|`$`|
|-|-|-|-|
|`[:refl:]`|`[: refl :]`|`@refl`|`$refl`|
|`[:type_of(refl):]`|`[: type_of(refl) :]`|`@(type_of(refl))`|`$(type_of(refl))`|

There are two other pieces of functionality that we will probably need syntax for in the future:

* code injection (of whatever form), and
* annotations (reflectable attributes, as values. [@P1887R1] suggested `+` as an annotation introducer, but `+` can begin an expression so another token is probably better. See also: [this thread](https://lists.isocpp.org/sg7/2023/10/0450.php)).

So any syntax discussion needs to consider the entirety of the feature.


The prefixes `typename` and `template` are only strictly needed in some cases where the operand of the splice is a dependent expression.
In our proposal, however, we only make `typename` optional in the same contexts where it would be optional for qualified names with dependent name qualifiers.
That has the advantage to catch unfortunate errors while keeping a single rule and helping human readers parse the intended meaning of otherwise ambiguous constructs.


## `std::meta::info`

The type `std::meta::info` can be defined as follows:

::: std
```c++
namespace std {
  namespace meta {
    using info = decltype(^^::);
  }
}
```
:::

In our initial proposal a value of type `std::meta::info` can represent:

  - any (C++) type and type alias
  - any function (or member function)
  - any variable, static data member, or structured binding
  - any non-static data member
  - any enumerator
  - any template
  - any namespace (including the global namespace) or namespace alias
  - any object that is a _permitted result of a constant expression_
  - any value with _structural type_ that is a permitted result of a constant expression
  - the null reflection (when default-constructed)

We for now restrict the space of reflectable values to those of structural type in order to meet two requirements:

1. The compiler must know how to mangle any reflectable value (i.e., when a reflection thereof is used as a template argument).
2. The compiler must know how to compare any two reflectable values, ideally without interpreting user-defined comparison operators (i.e., to implement comparison between reflections).

Values of structural types can already be used as template arguments (so implementations must already know how to mangle them), and the notion of _template-argument-equivalent_ values defined on the class of structural types helps guarantee that `&fn<^^value1> == &fn<^^value2>` if and only if `&fn<value1> == &fn<value2>`.

Notably absent at this time are reflections of expressions. For example, one might wish to walk over the subexpressions of a function call:

::: std
```c++
template <typename T> void fn(T) {}

void g() {
  constexpr auto call = ^^(fn(42));
  static_assert(
      template_arguments_of(function_of(call))[0] ==
      ^^int);
}
```
:::

Previous revisions of this proposal suggested limited support for reflections of constant expressions. The introduction of side effects from constant evaluations (by this very paper), however, renders this roughly as difficult for constant expressions as it is for non-constant expressions. We instead defer all expression reflection to a future paper, and only present value and object reflection in the present proposal.

### Comparing reflections

The type `std::meta::info` is a _scalar_ type for which equality and inequality are meaningful, but for which no ordering relation is defined.

::: std
```c++
static_assert(^^int == ^^int);
static_assert(^^int != ^^const int);
static_assert(^^int != ^^int &);

using Alias = int;
static_assert(^^int != ^^Alias);
static_assert(^^int == dealias(^^Alias));

namespace AliasNS = ::std;
static_assert(^^::std != ^^AliasNS);
static_assert(^^:: == parent_of(^^::std));
```
:::

When the `^^` operator is followed by an _id-expression_, the resulting `std::meta::info` represents the entity named by the expression. Such reflections are equivalent only if they reflect the same entity.

::: std
```c++
int x;
struct S { static int y; };
static_assert(^^x == ^^x);
static_assert(^^x != ^^S::y);
static_assert(^^S::y == static_data_members_of(^^S)[0]);
```
:::

Special rules apply when comparing certain kinds of reflections. A reflection of an alias compares equal to another reflection if and only if they are both aliases, alias the same type, and share the same name and scope. In particular, these rules allow e.g., `fn<^^std::string>` to refer to the same instantiation across translation units.

::: std
```c++
using Alias1 = int;
using Alias2 = int;
consteval std::meta::info fn() {
  using Alias1 = int;
  return ^^Alias;
}
static_assert(^^Alias1 == ^^Alias1);
static_assert(^^Alias1 != ^^int);
static_assert(^^Alias1 != ^^Alias2);
static_assert(^^Alias1 != fn());
}
```
:::

A reflection of an object (including variables) does not compare equally to a reflection of its value. Two values of different types never compare equally.

::: std
```c++
constexpr int i = 42, j = 42;

constexpr std::meta::info r = ^^i, s = ^^i;
static_assert(r == r && r == s);

static_assert(^^i != ^^j);  // 'i' and 'j' are different entities.
static_assert(constant_of(^^i) == constant_of(^^j));    // Two equivalent values.
static_assert(^^i != std::meta::reflect_object(i))      // A variable is distinct from the
                                                        // object it designates.
static_assert(^^i != std::meta::reflect_constant(42));  // A reflection of an object
                                                        // is not the same as its value.
```
:::


### The associated `std::meta` namespace

The namespace `std::meta` is an associated namespace of `std::meta::info`, which allows standard library meta functions to be invoked without explicit qualification. For example:

::: std
```c++
#include <meta>
struct S {};
std::string name2 = std::meta::identifier_of(^^S);  // Okay.
std::string name1 = identifier_of(^^S);             // Also okay.
```
:::

Default constructing or value-initializing an object of type `std::meta::info` gives it a null reflection value.
A null reflection value is equal to any other null reflection value and is different from any other reflection that refers to one of the mentioned entities.
For example:

::: std
```c++
#include <meta>
struct S {};
static_assert(std::meta::info() == std::meta::info());
static_assert(std::meta::info() != ^^S);
```
:::

### Consteval-only

It's important that `std::meta::info` not be allowed to propagate to runtime. This has no meaning, so it would be ideal to simply prevent the type from being usable to runtime in any way whatsoever.

We propose doing this by saying that `std::meta::info`, and all types compounded from it (meaning anythig from `info*` to classes with `info` members, e.g. `tuple<info>` or `vector<info>`) are consteval-only types. We then add two kinds of restrictions (both of which are necessary):

* an _object_ of consteval-only type can only exist at compile time.
* an _expression_ with an operand of consteval-only type can only be evaluated at compile time.

The first we can achieve by requiring such objects to either be part of a `constexpr` variable, a template parameter object, or have its lifetime entirely within constant evaluation. The latter we can achieve by plugging into the already-existing immediate escalating machinery that we have for immediate functions to also consider consteval-only types as escalating.

This has an interesting consequence that necessitates an `is_consteval_only` type trait that we discovered. In libc++, `std::sort` is implemented [like this](https://github.com/llvm/llvm-project/blob/acc6bcdc504ad2e8c09a628dc18de0067f7344b8/libcxx/include/__algorithm/sort.h):

::: std
```cpp
template <class _AlgPolicy, class _RandomAccessIterator, class _Comp>
inline _LIBCPP_HIDE_FROM_ABI _LIBCPP_CONSTEXPR_SINCE_CXX20 void
__sort_impl(_RandomAccessIterator __first, _RandomAccessIterator __last, _Comp& __comp) {
  std::__debug_randomize_range<_AlgPolicy>(__first, __last);

  if (__libcpp_is_constant_evaluated()) {
    std::__partial_sort<_AlgPolicy>(
        std::__unwrap_iter(__first), std::__unwrap_iter(__last), std::__unwrap_iter(__last), __comp);
  } else {
    std::__sort_dispatch<_AlgPolicy>(std::__unwrap_iter(__first), std::__unwrap_iter(__last), __comp);
  }
  std::__check_strict_weak_ordering_sorted(std::__unwrap_iter(__first), std::__unwrap_iter(__last), __comp);
}
```
:::

During constant evaluation, we call `__partial_sort` (which is `constexpr`). Otherwise, we call `__sort_dispatch` (which is not). If we instantiate `__sort_impl` with a `_RandomAccessIterator` type of `std::meta::info*`, then this eventually ends up also instantiating `std::__introsort` ([here](https://github.com/llvm/llvm-project/blob/acc6bcdc504ad2e8c09a628dc18de0067f7344b8/libcxx/include/__algorithm/sort.h#L715), also not `constexpr`) which in the body does this:

::: std
```cpp
template <class _AlgPolicy, class _Compare, class _RandomAccessIterator, bool _UseBitSetPartition>
void __introsort(_RandomAccessIterator __first,
                 _RandomAccessIterator __last,
                 _Compare __comp,
                 typename iterator_traits<_RandomAccessIterator>::difference_type __depth,
                 bool __leftmost = true) {
  // ...
  while (true) {
    difference_type __len = __last - __first;
    // ...
  }
}
```
:::

The expression `__last - __first`, because these are `std::meta::info*`s, must be a constant. Because it's not here (`__first` and `__last` are just function parameters), that triggers the immediate-escalation machinery from [@P2564R3]{.title} (before that paper, it would have been ill-formed on the spot). But because `__introsort` is not `constexpr`, propagation fails at that point, and the program is ill-formed.

Even though during we would've never actually gotten to this code during runtime.

Let's go back to the problem function and reduce it and un-uglify the names:

::: std
```cpp
template <class RandomAccessIterator>
constexpr void sort(RandomAccessIterator first, RandomAccessIterator last) {
  if consteval {
    std::__consteval_only_sort(first, last);
  } else {
    std::__runtime_only_sort(first, last);
  }
}
```
:::

At issue is that `__runtime_only_sort` isn't `constexpr` and instantiating it with `std::meta::info*` fails.

We thought of a few ways to approach this issue:

We could just mark `__runtime_only_sort` `constexpr` — but marking something `constexpr` that we explicitly do not want to evaluate during constant evaluation time (as our rename makes obvious), seems like a bad approach. It's a confusing annotation at best.

We considered changing the semantics of `if consteval` so that it could discard (in the `if constexpr` sense) the non-taken branch if actually evaluated during constant evaluation time. In this case, that would avoid having to instantiate `__runtime_only_sort<meta::info*>` and we'd be okay. But that kind of change seemed very complicated.

We also considered just implicitly making function templates with any function parameter having consteval-only type be `consteval` anyway. That is, `__runtime_only_sort`, despite the name, actually is `consteval`. That also seemed a bit adventurous, and while it addresses this particular issue, we weren't sure what other possible issues might come up.

Instead, we're taking a simpler and easier-to-justify approach: adding a new type trait to detect whether a type is consteval-only (a trait which is fairly straightforward to implement on top of the other facilities provided in this proposal). With that trait, the fix is simple (if perhaps surprising to the reader): explicitly discard the runtime branch:

::: std
```cpp
template <class RandomAccessIterator>
constexpr void sort(RandomAccessIterator first, RandomAccessIterator last) {
  if consteval {
    std::__consteval_only_sort(first, last);
  } else if constexpr (not is_consteval_only_type(^^RandomAccessIterator)) {
    std::__runtime_only_sort(first, last);
  }
}
```
:::

### Default arguments

Reflection represents "entities", not "source constructs".
For example, a reflection of a variable represents that variable, not the _declarations_ of that variable.
This generally works well, and corresponds to the way most implementations operate.
(Clang does keep track of declarations, but ultimately mostly deals in terms of the entities the produce.)

However, the language specificationhas a bit of a split personality when dealing with overload resolution and especially when selecting default arguments in function calls: Default arguments are obtained from the specific _declaration(s)_ that are found when collecting the candidates for overload resolution.
Consider the following code:

::: std
```cpp
int f(int = 1);
int g() {
  int f(int = 2);
  return f();  // Valid and calls f(2).
}
int r = f();  // Valid and calls f(1).
```
:::

Such code is highly unusual, but it is valid C++ and requires tying the resolution of the call to a specific declaration (or, in more complex cases, _set_ of declarations).
Now consider a similar example but with reflections:

::: std
```cpp
int f(int = 1);
constexpr auto r = ^^f;
int g() {
  int f(int = 2);
  return [:r:]();  // (1) ???
}
int r = [:r:]();  // (2) ???
```
:::
Reflection represents the _entity_ that is the _function_ described by the various _declarations_ of `f`.
This reflection is not tied to a _particular_ declaration, but instead it represents the accumulated properties of all the declarations of `f`.
That in turns means that it is not obvious _which_ default arguments should be selected.
Moreover, some implementations are highly constrained as to which options are viable.

We therefore propose that if a block-scope declaration has introduced a default argument for the N-th parameter of a function, then calling that function through a splice-specifier can not make use of the default argument.
I.e., both (1) and (2) above are ill-formed.
In other words, a default argument on a block-scope function declaration "poisons" _all_ default arguments for the corresponding parameter of the corresponding function.


## Metafunctions

We propose a number of metafunctions declared in namespace `std::meta` to operator on reflection values.
Adding metafunctions to an implementation is expected to be relatively "easy" compared to implementing the core language features described previously.
However, despite offering a normal consteval C++ function interface, each on of these relies on "compiler magic" to a significant extent.

### Constant evaluation order

In C++23, "constant evaluation" produces pure values without observable side-effects and thus the order in which constant-evaluation occurs is immaterial.
In fact, while the language is designed to permit constant evaluation to happen at compile time, an implementation is not strictly required to take advantage of that possibility.

Some of the proposed metafunctions, however, have side-effects that have an effect on the remainder of the program.
For example, we provide a `define_aggregate` metafunction that provides a definition for a given class.
Clearly, we want the effect of calling that metafunction to be "prompt" in a lexical-order sense.
For example:

::: std
```c++
#include <meta>
struct S;

void g() {
  consteval {
    define_aggregate(^^S, {});
  }
  S s;  // S should be defined at this point.
}
```
:::


Hence this proposal also introduces constraints on constant evaluation as follows...

First, consteval blocks (from [@P3289R1]) have the property that their evaluation must occur and must succeed in a valid C++ program.
We require that a programmer can count on those evaluations occurring exactly once and completing at translation time.

Second, we sequence consteval blocks within the lexical order.
Specifically, we require that the evaluation of a non-dependent consteval block occurs before the implementation checks the validity of source constructs lexically following them.

Those constraints are mostly intuitive, but they are a significant change to the underlying principles of the current standard in this respect.

[@P2758R4]{.title} also has to deal with side effects during constant evaluation.
However, those effects ("output") are of a slightly different nature in the sense that they can be buffered until a manifestly constant-evaluated expression/conversion has completed.
"Buffering" a class type completion is not practical (e.g., because other metafunctions may well depend on the completed class type).
Still, we are not aware of incompatibilities between our proposal and P2758.


### Error-Handling in Reflection

Earlier revisions of this proposal suggested several possible approaches to handling errors in reflection metafunctions. This question arises naturally when considering, for instance, examples like `template_of(^^int)`: the argument is a reflection of a type, but that type is not a specialization of a template, so there is no valid template that we can return.

Some of the possibilities that we have considered include:

1. Returning an invalid reflection (similar to `NaN` for floating point) which carries source location info and some useful message (i.e., the approach suggested by P1240)
2. Returning a `std::expected<std::meta::info, E>` for some reflection-specific error type `E`, which carries source location info and some useful message
3. Failing to be a constant expression
4. Throwing an exception of type `E`, which requires a language extension for such exceptions to be catchable during `constexpr` evaluation

We found that we disliked (1) since there is no satisfying value that can be returned for a call like `template_arguments_of(^^int)`: We could return a `std::vector<std::meta::info>` having a single invalid reflection, but this makes for awkward error handling. The experience offered by (3) is at least consistent, but provides no immediate means for a user to "recover" from an error.

Either `std::expected` or constexpr exceptions would allow for a consistent and straightforward interface. Deciding between the two, we noticed that many of usual concerns about exceptions do not apply during translation:

* concerns about runtime performance, object file size, etc. do not exist, and
* concerns about code evolving to add new uncaught exception types do not apply

An interesting example illustrates one reason for our preference for exceptions over `std::expected`:

::: std
```cpp
template <typename T>
  requires (template_of(^^T) == ^^std::optional)
void foo();
```
:::

* If `template_of` returns an `expected<info, E>`, then `foo<int>` is a substitution failure --- `expected<T, E>` is equality-comparable to `T`, that comparison would evaluate to `false` but still be a constant expression.

* If `template_of` returns `info` but throws an exception, then `foo<int>` would cause that exception to be uncaught, which would make the comparison not a constant expression.
This actually makes the constraint ill-formed - not a substitution failure.
In order to have `foo<int>` be a substitution failure, either the constraint would have to first check that `T` is a template or we would have to change the language rule that requires constraints to be constant expressions (we would of course still keep the requirement that the constraint is a `bool`).

Since the R2 revision of this paper, [@P3068R1] has proposed the introduction of constexpr exceptions. The proposal addresses hurdles like compiler modes that disable exception support, and a Clang-based implementation is underway. We believe this to be the most desirable error-handling mechanism for reflection metafunctions.

Because constexpr exceptions have not yet been adopted into the working draft, we do not specify any functions in this paper that throw exceptions. Rather, we propose that they fail to be constant expressions (i.e., case 3 above), and note that this approach will allow us to forward-compatibly add exceptions at a later time. In the interim period, implementations should have all of the information needed to issue helpful diagnostics (e.g., "_note: `R` does not reflect a template specialization_") to improve the experience of writing reflection code.

### Range-Based Metafunctions

There are a number of functions, both in the "core" reflection API that we intend to provide as well as converting some of the standard library type traits that can accept or return a range of `std::meta::info`.

For example:

* `template_arguments_of(^^std::tuple<int>)` is `{^^int}`
* `substitute(^^std::tuple, {^^int})` is `^^std::tuple<int>`

This requires us to answer the question: how do we accept a range parameter and how do we provide a range return.

For return, we intend on returning `std::vector<std::meta::info>` from all such APIs. This is by far the easiest for users to deal with. We definitely don't want to return a `std::span<std::meta::info const>`, since this requires keeping all the information in the compiler memory forever (unlike `std::vector` which could free its allocation). The only other option would be a custom container type which is optimized for compile-time by being able to produce elements lazily on demand - i.e. so that `nonstatic_data_members_of(^^T)[3]` wouldn't have to populate _all_ the data members, just do enough work to be able to return the 4th one. But that adds a lot of complexity that's probably not worth the effort.

For parameters, there are basically three options:

1. Accept `std::span<std::meta::info const>`, which now accepts braced-init-list arguments so it's pretty convenient in this regard.
2. Accept `std::vector<std::meta::info>`
3. Accept _any_ range whose `type_value` is `std::meta::info`.

Now, for compiler efficiency reasons, it's definitely better to have all the arguments contiguously. So the compiler wants `span` (or something like it). There's really no reason to prefer `vector` over `span`. Accepting any range would look something like this:

::: std
```cpp
namespace std::meta {
    template <typename R>
    concept reflection_range = ranges::input_range<R>
                            && same_as<ranges::range_value_t<R>, info>;

    template <reflection_range R = initializer_list<info>>
    consteval auto substitute(info tmpl, R&& args) -> info;
}
```
:::

This API is more user friendly than accepting `span<info const>` by virtue of simply accepting more kinds of ranges. The default template argument allows for braced-init-lists to still work. [Example](https://godbolt.org/z/P49MPhn4T).

Specifically, if the user is doing anything with range adaptors, they will either end up with a non-contiguous or non-sized range, which will no longer be convertible to `span` - so they will have to manually convert their range to a `vector<info>` in order to pass it to the algorithm. Because the implementation wants contiguity anyway, that conversion to `vector` will happen either way - so it's just a matter of whether every call needs to do it manually or the implementation can just do it once.

For example, converting a struct to a tuple type:

::: cmptable
### `span` only
```cpp
consteval auto type_struct_to_tuple(info type) -> meta::info {
    return substitute(
        ^^tuple,
        nonstatic_data_members_of(type)
        | views::transform(meta::type_of)
        | views::transform(meta::remove_cvref)
        | ranges::to<vector>());
}
```

### any range
```cpp
consteval auto type_struct_to_tuple(info type) -> meta::info {
    return substitute(
        ^^tuple,
        nonstatic_data_members_of(type)
        | views::transform(meta::type_of)
        | views::transform(meta::remove_cvref)
        );
}
```
:::

This shouldn't cause much compilation overhead. Checking convertibility to `span` _already_ uses Ranges machinery. And implementations can just do the right thing interally:

::: std
```cpp
consteval auto __builtin_substitute(info tmpl, info const* arg, size_t num_args) -> info;

template <reflection_range R = initializer_list<info>>
consteval auto substitute(info tmpl, R&& args) -> info {
    if constexpr (ranges::sized_range<R> && ranges::contiguous_range<R>) {
        return __builtin_substitute(tmpl, ranges::data(args), ranges::size(args));
    } else {
        auto as_vector = ranges::to<vector<info>>((R&&)args);
        return __builtin_substitute(tmpl, as_vector.data(), as_vector.size());
    }
}
```
:::

As such, we propose that all the range-accepting algorithms accept any range.

### Handling Aliases

Consider

::: std
```cpp
using A = int;
```
:::

In C++ today, `A` and `int` can be used interchangeably and there is no distinction between the two types.
With reflection as proposed in this paper, that will no longer be the case.
`^^A` yields a reflection of an alias to `int`, while `^^int` yields a reflection of `int`.
`^^A == ^^int` evaluates to `false`, but there will be a way to strip aliases - so `dealias(^^A) == ^^int` evaluates to `true`.

This opens up the question of how various other metafunctions handle aliases and it is worth going over a few examples:

::: std
```cpp
using A = int;
using B = std::unique_ptr<int>;
template <class T> using C = std::unique_ptr<T>;
```
:::

This paper is proposing that:

* `is_type(^^A)` is `true`.
   `^^A` is an alias, but it's an alias to a type, and if this evaluated as `false` then everyone would have to `dealias` everything all the time.
* `has_template_arguments(^^B)` is `false` while `has_template_arguments(^^C<int>)` is `true`.
  Even though `B` is an alias to a type that itself has template arguments (`unique_ptr<int>`), `B` itself is simply a type alias and does not.
  This reflects the actual usage.
* Meanwhile, `template_arguments_of(^^C<int>)` yields `{^^int}` while `template_arguments_of(^^std::unique_ptr<int>)` yields `{^^int, ^^std::default_deleter<int>}`.
  This is because `C` has its own template arguments that can be reflected on.

What about when querying the type of an entity?

::: std
```cpp
std::string Str;
const std::string &Ref = Str;

constexpr std::meta::info StrTy = type_of(^^Str);
constexpr std::meta::info RefTy = type_of(^^Ref);
```
:::

What are `StrTy` and `RefTy`? This question is more difficult. Two distinct issues complicate the answer:

1. Our experience using these facilities has consistently shown that if `StrTy` represents `std::string`, many uses of `StrTy` require writing `dealias(StrTy)` rather than using `StrTy` directly (because a reflection of a type aliases compares unequal with a reflection of the aliased type). Failure to do so often yields subtle bugs.

2. While we would like for `RefTy` to represent `const std::string &`, it can only represent `const std::basic_string<char, std::allocator<char>> &`. Why? Because since `std::string` is only a "name" for `std::basic_string<char, std::allocator<char>>`, the language provides no semantic answer to what "`const std::string &`" _is_. It is only a source-level "grammatical" construct: A _type-id_. Reflecting type-ids is a brittle path, since it opens questions like whether a reflection of `const int` is the same as a reflection of `int const`. Furthermore, nothing currently requires an implementation to "remember" that the type of `Ref` was "spelled" with the alias `std::string` after parsing it, and we aren't confident that all major implementations do so today. Lastly, even if we _could_ form a reflection of `const std::string &`, our existing metafunction and type-trait "machinery" gives no means of unwrapping the cv-ref qualification to get `^^std::string` without decaying all the way to `^^std::basic_string<char, std::allocator<char>>`.

In light of the above, our position is that `type_of` should never return aliases: That is, `StrTy` represents `std::basic_string<char, std::allocator<char>>`. We believe that it would be desirable to in the future introduce an `aliased_type_of` function capable of returning representations of both `std::string` and `const std::string &` for `Str` and `Ref` respectively - but this requires both discussions with implementers, and likely new wording technology for the Standard. To avoid jeopardizing the goal declared by the title of this paper, we are not proposing such a function at this time.

### Reflecting source text

One of the most "obvious" abilities of reflection --- retrieving the name of an entity --- turns out to raise
issues that aren't obvious at all: How do we represent source text in a C++ program?

Thanks to recent work originating in SG16 (the "Unicode" study group) we can assume that all source code is
ultimately representable as Unicode code points.  C++ now also has types to represent UTF-8-encoded text
(incl. `char8_t`, `u8string`, and `u8string_view`) and corresponding literals like `u8"Hi"`.  Unfortunately,
what can be done with those types is still limited at the time of this writing.  For example,

::: std
```cpp
#include <iostream>
int main() {
  std::cout << u8"こんにちは世界\n";
}
```
:::

is not standard C++ because the standard output stream does not have support for UTF-8 literals.

In practice ordinary strings encoded in the "ordinary literal encoding" (which may or may not be UTF-8)
are often used.  We therefore need mechanisms to produce the corresponding ordinary string types as well.

Orthogonal to the character representation is the data structure used to traffic in source text.  An
implementation can easily have at least three potential representations of reflected source text:

  a) the internal representation used, e.g., in the compiler front end's AST-like structures (persistent)

  b) the representation of string literals in the AST (persistent)

  c) the representation of array of character values during constant-evaluation (transient)

(some compilers might share some of those representations).  For transient text during constant evaluation we'd
like to use `string`/`u8string` values, but because of the limitations on non-transient allocation during
constant evaluation we cannot easily transfer such types to the non-constant (i.e., run-time) environment.
E.g., if `identifier_of` were a (consteval) metafunction returning a `std::string` value, the following simple
example would not work:

::: std
```cpp
#include <iostream>
#include <meta>
int main() {
  int hello_world = 42;
  std::cout << identifier_of(^^hello_world) << "\n";  // Doesn't work if identifier_of produces a std::string.
}
```
:::

We can instead return a `std::string_view` or `std::u8string_view`, but that has the downside
that it effectively makes all results of querying source text persistent for the compilation.

For now, however, we propose that queries like `identifier_of` do produce "string view" results.
For example:

::: std
```cpp
consteval std::string_view identifier_of(info);
consteval std::u8string_view identifier_of(info);
```
:::


An alternative strategy that we considered is the introduction of a "proxy type" for source text:

::: std
```cpp
namespace std::meta {
  struct source_text_info {
    ...
    template<typename T>
      requires (^^T == dealias(^^std::string_view) || ^^T == dealias(^^std::u8string_view) ||
                ^^T == dealias(^^std::string) || ^^T == dealias(^^std::u8string))
      consteval T as();
    ...
  };
}
```
:::

where the `as<...>()` member function produces a string-like type as desired.  That idea was dropped,
however, because it became unwieldy in actual use cases.

With a source text query like `identifier_of(refl)` it is possible that the some source
characters of the result are not representable.  We can then consider multiple options, including:

  1) the query fails to evaluate,

  2) any unrepresentable source characters are translated to a different presentation,
     such as universal-character-names of the form `\u{ $hex-number$ }`,

  3) any source characters not in the basic source character set are translated to a different
     presentation (as in (2)).

Following much discussion with SG16, we propose #1: The query fails to evaluate if the identifier cannot be represented in the ordinary literal encoding.

### Reflecting names

Earlier revisions of this proposal (and its predecessor, [@P1240R2]) included a metafunction called `name_of`, which we defined to return a `string_view` containing the "name" of the reflected entity. As the paper evolved, it became necessary to sharpen the specification of what this "name" contains. Subsequent revisions (beginning with P2996R2, presented in Tokyo) specified that `name_of` returns the unqualified name, whereas a new `qualified_name_of` would give the fully qualified name.

Most would agree that `qualified_name_of(^^size_t)` might reasonably return `"std::size_t"`, or that `qualified_name_of(^^std::any::reset)` could return `"std::any::reset"`. But what about for local variables, or members of local classes? Should inline and anonymous namespaces be rendered as a part of the qualified name? Should we standardize the spelling of such scopes, or leave it implementation defined?

The situation is possibly even less clear for unqualified names. Should cv-qualified types be rendered as `const int` or `int const`? Should the type for a function returning a pointer be rendered as `T *(*)()`, `T* (*)()`, or `T * (*)()`? Should such decisions be standardized, or left to implementations? But the real kicker is when one considers non-type template arguments, which can (and do) contain arbitrarily complex values of arbitrary structural types (along with any complete object, or subobject thereof, which has static storage duration).

The more that we tried to specify formatting behavior for just the unqualified names of arbitrary types, the more convinced we became that this did not feel like an algorithm that should be frozen in the standard library - at least, not at this time. There are just too many toggles that a programmer might reasonably want to flip (one need only look at [Clang's `PrettyPrinter` class](https://github.com/llvm/llvm-project/blob/248c53429427034f45705af60d47f3b1090c4799/clang/include/clang/AST/PrettyPrinter.h#L59-L80) for inspiration). On the other hand, it is perfectly reasonable to ask that implementations give _some_ means of describing what it is that a reflection contains - that is exactly the purpose of the `display_string_of` function.

Our stance is therefore that reflection pretty printers, for now, should be left to organically develop within the ecosystem of open-source C++ libraries. To ensure that this is possible, the Clang/P2996 fork has implemented its `display_string_of` metafunction entirely within the library. It is capable of printing type names, value representations, template arguments, and much more. Best of all, it can be extended without modifying the compiler.

What of `name_of` and `qualified_name_of`? As of the R5 revision of this paper, we have removed them. In their stead is `identifier_of`, which is only a constant expression if the name of the represented construct is an identifier, and `has_identifier` for checking this condition. A few other metafunctions fill in some gaps: `operator_of` determines the identity of an overloaded operator, and predicates like `is_operator_function` and `is_conversion_function_template` let printing libraries handle those unqualified names that are not identifiers. `parent_of` supports walking up the chain of functions, namespaces, and classes enclosing the declaration of an entity, thus enabling homegrown implementations of `qualified_name_of`. Meanwhile, the prime real estate of `name_of` remains available for future library extensions.

As a nice side-effect, the `identifier_of` model altogether dodges some contentious questions that arose during LEWG discussions in St Louis: Should asking the "name" of an anonymous entity (e.g., anonymous unions) return the empty string, or fail to be a constant expression? Since the C++ grammar requires that an `$identifier$` contain at least one character, the `identifier_of` function never returns an empty string: it is seen that the only possibility is to fail to be a constant expression.

### Reachability and injected declarations

Certain metafunctions (e.g., `members_of`) return reflections that represent entities without ever naming those entities in source code (i.e., eliding lookup). Although it is often clear which entities should be returned from the perspective of a reader, or even the perspective of an implementation, core wording has no notion that directly corresponds to "compilation state".

Lookup is rather defined in terms of "reachability", which is roughly a mapping from a "program point" to the set of declarations _reachable_ from that point. Lookup frequently occurs from a single point, but template instantiation (and a few other niche circumstances) can lead to lookup taking place from multiple points (i.e., the point in a template from which a name is specified, and the point from which the template was instantiated). The set of points from which lookup takes place is the _instantiation context_ ([module.context]).

::: std
```c++
template <typename T> int fn() {
  return /*P1*/ T::value;
}

struct S { static const int value = 42; }

int main() {
  return /*P2*/ fn<S>();
}

// The instantiation context when looking up 'S::value' in 'fn<T>' is {P1, P2}.
// Even though 'S' is not found from P1, it is found from P2; lookup succeeds.
```
:::

This works because the notion of template instantiation is baked into the definition of "instantiation context", which is thereafter used to define lookup. But we have no such benefit in the case of metafunctions like `members_of`, which do not utilize template instantiation.

::: std
```c++
consteval size_t count_fields(std::meta::info Ty) {
  return /*P1*/ nonstatic_data_members_of(Ty).size();
}

struct S { int i, j, k; }
static_assert(/*P2*/ count_fields(^^S) == 3);
```
:::

If we naively define `nonstatic_data_members_of` to return members reachable from the "point of call", then the above code would fail: after all, `S` is not reachable from `$P1$`. We instead must define the declarations to be those reachable from where constant evaluation begins (i.e., `$P2$`). We encode this idea in our definition of the _evaluation context_:

::: std
::: addu
[22]{.pnum} During the evaluation of a manifestly constant-evaluated expression `$M$`, the evaluation context of an expression `$E$` comprises [...] the instantiation context of `$M$` ([module.context]), [...] .
:::
:::

This gives the tool needed to define the declarations returned by `members_of` to be (roughly) those reachable from the _evaluation context_. However, a second problem related to reachability is posed by `define_aggregate`.

::: std
```c++
consteval std::meta::info make_defn(std::meta::info Cls, std::meta::info Mem) {
  // Synthesizes:
  //   struct Mem {};
  //   struct Cls { Mem m; };
  return /*P1*/ define_aggregate(Cls, {
    data_member_spec(/*P2*/ define_aggregate(Mem, {}), {.name="m"})
  });
}

/* P3*/ struct C;
/* P4*/ struct M;
static_assert(/*P5*/ is_type(make_defn(^^C, ^^M)) /*P6*/);

/*P7*/ C obj;
```
:::

Although we want this code to be valid, we have several obstacles to navigate.

1. How can definitions for `C` and `M` be defined from `$P1$` and `$P2$` when no declarations of those classes are reachable from those program points?
2. Where are the points of declaration for the generated definitions of `C` and `M` (i.e., from what program points will the generated definitions be reachable)?
3. How can we ensure that the definition of `M` is reachable during the evaluation of `define_aggregate` on `C`?

The prior discourse regarding `members_of` gives a straightforward answer to (1); the `define_aggregate` function is defined in terms of the _evaluation context_, which makes available all declarations reachable from `$P5$`.

An answer to (2) can be seen by considering the declarations at `$P3$`, `$P4$`, and `$P7$`: Since we want the declaration of `obj` to be well-formed, the generated definition of `C` must precede `$P7$`. On the other hand, placing the definition of `$C$` prior to `$P4$` would weirdly place the definition of the class `C`, which contains a data member of type `M`, prior to the declaration of `M` itself. We propose that the point of declaration for all definitions generated by `define_aggregate` immediately follows the end of the manifestly constant-evaluated expression that produces the definition: In this case, just prior to `$P6$`.

This leaves one gap, and it is the question posed by (3): If the definition of `M`, generated by evaluation of `define_aggregate(Mem, {})`, is located just prior to `$P6$`, then the definition is still not reachable from the evaluation context (such as we have defined it) during evaluation of `define_aggregate(Cls, ...)`.

Circling back to "reachability" as a mapping from program points to declarations, there are two clear paths forward: Either modify which declarations are reachable from a program point, or modify the set of program points in the evaluation context. We choose the later approach, and attempt to provide some machinery that can be reused for future "generative reflection" proposals.

We begin by specially indicating that the generated definitions of `C` and `M` are not just declarations, but _injected declarations_, and that such injected declarations are _produced_ by an evaluation of an expression. The reachability of these declarations is evidently different from other declarations: It depends not only on a program point, but also on which compile-time evaluations of expressions (which have no relation to lexical ordering) are _sequenced after_ the production of the injected declarations.

To bridge the world of program points to the world of sequenced evaluations, we introduce a notion dual to "injected declarations": For every injected declaration, there is a corresponding _synthesized point_. Injected points have a special property: the _only_ declaration reachable from a synthesized point is its corresponding injected declaration. Jumping back to our above example, joining the synthesized point of the injected declaration of `M` to our evaluation context gives exactly what is needed for `M` to be usable during the definition of `C`. More precisely: `M` is reachable during the definition of `C` because the evaluation of the expression that produces the definition of `M` is _sequenced before_ the evalauation of the expression that produces `C`. This is captured by our full and final definition of the evaluation context:

::: std
::: addu
[22]{.pnum} The _evaluation context_ is a set of points within the program that determines which declarations are found by certain expressions used for reflection. During the evaluation of a manifestly constant-evaluated expression `$M$`, the evaluation context of an expression `$E$` comprises the union of

* [#.#]{.pnum} the instantiation context of `$M$` ([module.context]), and
* [#.#]{.pnum} the synthesized points corresponding to any injected declarations ([expr.const]) produced by evaluations sequenced before the next evaluation of `$E$`.
:::
:::

Lastly, we clarify that during the definition of an _injected declaration_, the instantiation context consists of the _evaluation context_ of the expression that is producing the declaration. In our example above, this ensures that the definition of `$M$` is reachable not just from the invocation of `define_aggregate` for `C`, but from within the actual generated definition of `$C$`.

This machinery is "off in the weeds" of technicalities related to modules, lookup, etc., but we believe (hope?) that it provides a sound basis upon which to build generative reflection within the framework provided by core language wording: not only for P2996, but for future papers as well.


### Restrictions on injected declarations

The advancement of this proposal through WG21 has naturally led to increased scrutiny of the mechanisms here proposed. One such area is the possibility of leveraging injected declarations to observe failed template substitutions. Consider the following example:

::: std
```cpp
struct S;

template <typename> struct TCls {
  static consteval bool sfn()  // #1
      requires ([] {
        consteval {
          define_aggregate(^^S, {});
        }
      }(), false) {
    return false;  // never selected
  }

  static consteval bool sfn()  // #2
      requires (true) {
    return true;   // always selected
  }
};

static_assert(TCls<void>::sfn());
static_assert(is_complete_type(^^S));
```
:::

The above example observes the effects of the failed substitution of `#1` by way of the completeness of `S`. Such tricks can be used to observe implementation details, like the order in which overloads are checked, that may be unportable (and which implementations might desire to change over time).

Our proposed solution, specified in [expr.const]/23.2, is to make it ill-formed to produce an injected declaration from a manifestly constant-evaluated expression _inside of_ an instantiation to _outside of_ that instantiation, or visa versa. Because that expression in the example above (`define_aggregate(^^S, {})`) is within the instantiation of the requires clause of `TCls<void>::sfn`, and the target scope of the injected declaration is outside of that same instantiaton, the example becomes ill-formed (diagnostic required). Note that this does not prevent writing `consteval` function templates that wrap `define_aggregate`:

::: std
```cpp
template <std::meta::info R> consteval bool tfn() {
  define_aggregate(R, {});
  return true;
}

struct S;
constexpr bool b = tfn<^^S>();
  // OK, both manifestly constant-evaluated expression tfn<^^S>() and target scope of
  // injected declaration for 'S' are in the global namespace
```
:::

Nor does this rule prevent a class template from producing a declaration whose target scope is the same specialization.

::: std
```cpp
template <typename> struct TCls1 {
  struct Incomplete;

  consteval {
    define_aggregate(^^Incomplete, {});
      // OK, Incomplete is in the same instantiation as the define_aggregate call
  }

  static constexpr bool b = false;
};

template <typename T> struct TCls2 {
  static consteval bool sfn()  // #1
      requires (TCls1<T>::b) {
    return false;  // never selected
  }

  static consteval bool sfn()  // #2
      requires (true) {
    return true;   // always selected
  }
};

static_assert(TCls<void>::sfn());
```
:::

Athough the instantiation of `TCls1<void>` in the requires-clause of `#1` causes an injected declaration to be produced, it is not discernibly a side-effect of the failed substitution: Observing the side effect will first require one to write (some moral  equivalent of) `TCLs1<void>::Incomplete`, the act of which would otherwise itself trigger the same side-effect.

Although this rule constrains the manner with which `define_aggregate` can be used, we are not aware of any motivating use cases for P2996 that are harmed. Worth mentioning, however: the rule has more dire implications for other code injection papers being considered by WG21, most notably [@P3294R2] ("_Code Injection With Token Sequences_"). With this rule as it is, it becomes impossible for e.g., the instantiation of a class template specialization `TCls<Foo>` to produce an injected declaration of `std::formatter<TCls<Foo>>` (since the target scope would be the global namespace).

In this context, we do believe that relaxations of the rule can be considered: For instance, we ought to be able to say that the instantiation of `std::formatter<TCls<Foo>>` is sequenced strictly after the instantiation of `TCls<Foo>`, and observations such as these might make it possible to permit such injections without making it "discernible" whether they resulted from failed substitutions. The key to such an approach would be to define a partial order over the instantiations of a program, and to allow constructs to be injected _across_ instantiations when the relative order of their respective instantiations is defined.

All of that said, these relaxations are not needed for the code injection introduced by this proposal, and we do not seek to introduce them at this time.

### Freestanding implementations

Several important metafunctions, such as `std::meta::nonstatic_data_members_of`, return a `std::vector` value.
Unfortunately, that means that they are currently not usable in a freestanding environment, but [@P3295R0]{.title} currently proposes freestanding `std::vector`, `std::string`, and `std::allocator` in constant evaluated contexts, explicitly to make the facilities proposed by this paper work in freestanding.

### Synopsis

Here is a synopsis for the proposed library API. The functions will be explained below.

::: std
```c++
namespace std::meta {
  using info = decltype(^^::);

  template <typename R>
    concept reflection_range = /* @*see [above](#range-based-metafunctions)*@ */;

  // @[name and location](#name-loc)@
  consteval auto identifier_of(info r) -> string_view;
  consteval auto u8identifier_of(info r) -> u8string_view;

  consteval auto display_string_of(info r) -> string_view;
  consteval auto u8display_string_of(info r) -> u8string_view;

  consteval auto source_location_of(info r) -> source_location;

  // @[type queries](#type_of-parent_of-dealias)@
  consteval auto type_of(info r) -> info;
  consteval auto parent_of(info r) -> info;
  consteval auto dealias(info r) -> info;

  // @[object and constant queries](#object_of-constant_of)@
  consteval auto object_of(info r) -> info;
  consteval auto constant_of(info r) -> info;

  // @[template queries](#template_of-template_arguments_of)@
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;

  // @[member queries](#member-queries)@
  consteval auto members_of(info r) -> vector<info>;
  consteval auto bases_of(info type_class) -> vector<info>;
  consteval auto static_data_members_of(info type_class) -> vector<info>;
  consteval auto nonstatic_data_members_of(info type_class) -> vector<info>;
  consteval auto enumerators_of(info type_enum) -> vector<info>;

  // @[substitute](#substitute)@
  template <reflection_range R = initializer_list<info>>
    consteval auto can_substitute(info templ, R&& args) -> bool;
  template <reflection_range R = initializer_list<info>>
    consteval auto substitute(info templ, R&& args) -> info;

  // @[reflect expression results](#reflect-expression-results)@
  template <typename T>
    consteval auto reflect_constant(const T& value) -> info;
  template <typename T>
    consteval auto reflect_object(T& value) -> info;
  template <typename T>
    consteval auto reflect_function(T& value) -> info;

  // @[extract<T>](#extractt)@
  template <typename T>
    consteval auto extract(info) -> T;

  // other type predicates (see @[the wording](#meta.reflection.queries-reflection-queries)@)
  consteval auto is_public(info r) -> bool;
  consteval auto is_protected(info r) -> bool;
  consteval auto is_private(info r) -> bool;
  consteval auto is_virtual(info r) -> bool;
  consteval auto is_pure_virtual(info r) -> bool;
  consteval auto is_override(info r) -> bool;
  consteval auto is_final(info r) -> bool;
  consteval auto is_deleted(info r) -> bool;
  consteval auto is_defaulted(info r) -> bool;
  consteval auto is_explicit(info r) -> bool;
  consteval auto is_noexcept(info r) -> bool;
  consteval auto is_bit_field(info r) -> bool;
  consteval auto is_enumerator(info r) -> bool;
  consteval auto is_const(info r) -> bool;
  consteval auto is_volatile(info r) -> bool;
  consteval auto is_mutable_member(info r) -> bool;
  consteval auto is_lvalue_reference_qualified(info r) -> bool;
  consteval auto is_rvalue_reference_qualified(info r) -> bool;
  consteval auto has_static_storage_duration(info r) -> bool;
  consteval auto has_thread_storage_duration(info r) -> bool;
  consteval auto has_automatic_storage_duration(info r) -> bool;
  consteval auto has_internal_linkage(info r) -> bool;
  consteval auto has_module_linkage(info r) -> bool;
  consteval auto has_external_linkage(info r) -> bool;
  consteval auto has_linkage(info r) -> bool;
  consteval auto is_class_member(info r) -> bool;
  consteval auto is_namespace_member(info r) -> bool;
  consteval auto is_nonstatic_data_member(info r) -> bool;
  consteval auto is_static_member(info r) -> bool;
  consteval auto is_base(info r) -> bool;
  consteval auto is_data_member_spec(info r) -> bool;
  consteval auto is_namespace(info r) -> bool;
  consteval auto is_function(info r) -> bool;
  consteval auto is_variable(info r) -> bool;
  consteval auto is_type(info r) -> bool;
  consteval auto is_type_alias(info r) -> bool;
  consteval auto is_namespace_alias(info r) -> bool;
  consteval auto is_complete_type(info r) -> bool;
  consteval auto is_enumerable_type(info r) -> bool;
  consteval auto is_template(info r) -> bool;
  consteval auto is_function_template(info r) -> bool;
  consteval auto is_variable_template(info r) -> bool;
  consteval auto is_class_template(info r) -> bool;
  consteval auto is_alias_template(info r) -> bool;
  consteval auto is_conversion_function_template(info r) -> bool;
  consteval auto is_operator_function_template(info r) -> bool;
  consteval auto is_literal_operator_template(info r) -> bool;
  consteval auto is_constructor_template(info r) -> bool;
  consteval auto is_concept(info r) -> bool;
  consteval auto is_structured_binding(info r) -> bool;
  consteval auto is_value(info r) -> bool;
  consteval auto is_object(info r) -> bool;
  consteval auto has_template_arguments(info r) -> bool;
  consteval auto has_default_member_initializer(info r) -> bool;

  consteval auto is_special_member_function(info r) -> bool;
  consteval auto is_conversion_function(info r) -> bool;
  consteval auto is_operator_function(info r) -> bool;
  consteval auto is_literal_operator(info r) -> bool;
  consteval auto is_constructor(info r) -> bool;
  consteval auto is_default_constructor(info r) -> bool;
  consteval auto is_copy_constructor(info r) -> bool;
  consteval auto is_move_constructor(info r) -> bool;
  consteval auto is_assignment(info r) -> bool;
  consteval auto is_copy_assignment(info r) -> bool;
  consteval auto is_move_assignment(info r) -> bool;
  consteval auto is_destructor(info r) -> bool;
  consteval auto is_user_provided(info r) -> bool;
  consteval auto is_user_declared(info r) -> bool;

  // @[define_aggregate](#data_member_spec-define_aggregate)@
  struct data_member_options;
  consteval auto data_member_spec(info type_class,
                                  data_member_options options) -> info;
  template <reflection_range R = initializer_list<info>>
    consteval auto define_aggregate(info type_class, R&&) -> info;

  // @[data layout](#data-layout-reflection)@
  struct member_offset {
    ptrdiff_t bytes;
    ptrdiff_t bits;
    constexpr auto total_bits() const -> ptrdiff_t;
    auto operator<=>(member_offset const&) const = default;
  };

  consteval auto offset_of(info r) -> member_offset;
  consteval auto size_of(info r) -> size_t;
  consteval auto alignment_of(info r) -> size_t;
  consteval auto bit_size_of(info r) -> size_t;

}
```
:::

### `identifier_of`, `display_string_of`, `source_location_of` {#name-loc}

::: std
```c++
namespace std::meta {
  consteval auto identifier_of(info) -> string_view;
  consteval auto u8identifier_of(info) -> u8string_view;

  consteval auto display_string_of(info) -> string_view;
  consteval auto u8display_string_of(info) -> u8string_view;

  consteval auto has_identifier(info) -> bool;

  consteval auto source_location_of(info r) -> source_location;
}
```
:::

Given a reflection `r` representing a language construct `X` whose declaration introduces an identifier, and if that identifier is representable using the ordinary literal encoding, then `identifier_of(r)` returns a non-empty `string_view` containing that identifier. Otherwise, it is not a constant expression. Whether a reflected construct has an identifier can be checked with the `has_identifier` metafunction.

The function `u8identifier_of` returns the same identifier but as a `u8string_view`. Note that since all identifiers can be represented as UTF-8 string literals, `u8identifier_of` never fails to be a constant expression because of representability concerns.

Given any reflection `r`, `display_string_of(r)` and `u8display_string_of(r)` return an unspecified non-empty `string_view` and `u8string_view`, respectively.
Implementations are encouraged to produce text that is helpful in identifying the reflected construct (note: as an exercise, the Clang implementation of this proposal implements a pretty-printing `display_string_of` [as a non-intrinsic library function](https://github.com/bloomberg/clang-p2996/blob/8ce6449538510a2330f7227f53b40be7671b0b91/libcxx/include/experimental/meta#L2088-L2731)).

Given a reflection `r`, `source_location_of(r)` returns an unspecified `source_location`.
Implementations are encouraged to produce the correct source location of the item designated by the reflection.

### `type_of`, `parent_of`, `dealias`

::: std
```c++
namespace std::meta {
  consteval auto type_of(info r) -> info;
  consteval auto parent_of(info r) -> info;
  consteval auto dealias(info r) -> info;
}
```
:::

If `r` is a reflection designating a typed entity, `type_of(r)` is a reflection designating its type.
If `r` is already a type, `type_of(r)` is not a constant expression.
This can be used to implement the C `typeof` feature (which works on both types and expressions and strips qualifiers):

::: std
```cpp
consteval auto type_doof(std::meta::info r) -> std::meta::info {
  return remove_cvref(is_type(r) ? r : type_of(r));
}

#define typeof(e) [: type_doof(^^e) :]
```
:::

`parent_of(r)` is a reflection designating its immediately enclosing class, function, or (possibly inline or anonymous) namespace.

If `r` represents an alias, `dealias(r)` represents the underlying entity.
Otherwise, `dealias(r)` produces `r`.
`dealias` is recursive - it strips all aliases:

::: std
```cpp
using X = int;
using Y = X;
static_assert(dealias(^^int) == ^^int);
static_assert(dealias(^^X) == ^^int);
static_assert(dealias(^^Y) == ^^int);
```
:::

### `object_of`, `constant_of`

::: std
```c++
namespace std::meta {
  consteval auto object_of(info r) -> info;
  consteval auto constant_of(info r) -> info;
}
```
:::

If `r` is a reflection of a variable denoting an object with static storage duration, then `object_of(r)` is a reflection of the object designated by the variable. If `r` is already a reflection of an object, `object_of(r)` is `r`. For all other inputs, `object_of(r)` is not a constant expression.

::: std
```c++
int x;
int &y = x;

static_assert(^^x != ^^y);
static_assert(object_of(^^x) == object_of(^^y));
```
:::

If `r` is a reflection of an enumerator, then `constant_of(r)` is a reflection of the value of the enumerator. Otherwise, if `r` is a reflection of an object _usable in constant expressions_, then:

* if `r` has scalar type, then `constant_of(r)` is a reflection of the value of the object.
* otherwise, `constant_of(r)` is a reflection of the object.

For all other inputs, `constant_of(r)` is not a constant expression. For more, see [`reflect_constant`](#reflect-expression-results).

### `template_of`, `template_arguments_of`

::: std
```c++
namespace std::meta {
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;
}
```
:::

If `r` is a reflection designating a specialization of some template, then `template_of(r)` is a reflection of that template and `template_arguments_of(r)` is a vector of the reflections of the template arguments. In other words, the preconditions on both is that `has_template_arguments(r)` is `true`.

For example:

::: std
```c++
std::vector<int> v = {1, 2, 3};
static_assert(template_of(type_of(^^v)) == ^^std::vector);
static_assert(template_arguments_of(type_of(^^v))[0] == ^^int);
```
:::



### `members_of`, `static_data_members_of`, `nonstatic_data_members_of`, `bases_of`, `enumerators_of` {#member-queries}

::: std
```c++
namespace std::meta {
  consteval auto members_of(info r) -> vector<info>;
  consteval auto bases_of(info type_class) -> vector<info>;

  consteval auto static_data_members_of(info type_class) -> vector<info>;
  consteval auto nonstatic_data_members_of(info type_class) -> vector<info>;

  consteval auto enumerators_of(info type_enum) -> vector<info>;
}
```
:::

The template `members_of` returns a vector of reflections representing the direct members of the class type or namespace represented by its first argument.
Any non-static data members appear in declaration order within that vector.
Anonymous unions appear as a non-static data member of corresponding union type.
Reflections of structured bindings shall not appear in the returned vector.

The template `bases_of` returns the direct base classes of the class type represented by its first argument, in declaration order.

`static_data_members_of` and `nonstatic_data_members_of` return reflections of the static and non-static data members, preserving their order, respectively.

`enumerators_of` returns the enumerator constants of the indicated enumeration type in declaration order.

### `substitute`

::: std
```c++
namespace std::meta {
  template <reflection_range R = initializer_list<info>>
  consteval auto can_substitute(info templ, R&& args) -> bool;
  template <reflection_range R = initializer_list<info>>
  consteval auto substitute(info templ, R&& args) -> info;
}
```
:::

Given a reflection for a template and reflections for template arguments that match that template, `substitute` returns a reflection for the entity obtained by substituting the given arguments in the template.
If the template is a concept template, the result is a reflection of a constant of type `bool`.

For example:

::: std
```c++
constexpr auto r = substitute(^^std::vector, std::vector{^^int});
using T = [:r:]; // Ok, T is std::vector<int>
```
:::

This process might kick off instantiations outside the immediate context, which can lead to the program being ill-formed.

Note that the template is only substituted, not instantiated.  For example:

::: std
```c++
template<typename T> struct S { typename T::X x; };

constexpr auto r = substitute(^^S, std::vector{^^int});  // Okay.
typename[:r:] si;  // Error: T::X is invalid for T = int.
```
:::

`can_substitute(templ, args)` simply checks if the substitution can succeed (with the same caveat about instantiations outside of the immediate context).
If `can_substitute(templ, args)` is `false`, then `substitute(templ, args)` will be ill-formed.

### `reflect_constant`, `reflect_object`, `reflect_function` {#reflect-expression-results}

::: std
```c++
namespace std::meta {
  template<typename T> consteval auto reflect_constant(const T& expr) -> info;
  template<typename T> consteval auto reflect_object(T& expr) -> info;
  template<typename T> consteval auto reflect_function(T& expr) -> info;
}
```
:::

These metafunctions produce a reflection of the _result_ from evaluating the provided expression. One of the most common use-cases for such reflections is to specify the template arguments with which to build a specialization using `std::meta::substitute`.

`reflect_constant(expr)` can best be understood from the equivalence that given the template

::: std
```cpp
template <auto P> struct C { };
```
:::

that:

::: std
```cpp
reflect_constant(V) == template_arguments_of(^^C<V>)[0]
```
:::

In other words, letting `T` be the cv-unqualified, de-aliased type of `expr`.

* if `expr` has scalar type, then `reflect_constant(expr)` is a reflection of the value of `expr`, whose type is `T`.
* if `expr` has class type, then `reflect_constant(expr)` is a reflection of the template parameter object that is template-argument-equivalent to an object of type `T` copy-initialized from `expr`.

Either way, the result needs to be a permitted result of a constant expression. Notably, `reflect_constant(e)` can be either a reflection of a value or a reflection of an object, depending on the type of `e`. This seeming inconsistence is actually useful for two reasons:

1. As mentioned above, it allows an equivalence with template arguments — where the argument is already either a value or an object.
2. It avoids having to invest complexity into defining what it means to have a reflection of a value of class type. Particularly with regards to when/if copies happen.

```cpp
static_assert(substitute(^^std::array, {^^int, std::meta::reflect_constant(5)}) ==
              ^^std::array<int, 5>);
```

`reflect_object(expr)` produces a reflection of the object designated by `expr`. This is frequently used to obtain a reflection of a subobject, which might then be used as a template argument for a non-type template parameter of reference type.

```cpp
template <int &> void fn();

int p[2];
constexpr auto r = substitute(^^fn, {std::meta::reflect_object(p[1])});
```

`reflect_function(expr)` produces a reflection of the function designated by `expr`. It can be useful for reflecting on the properties of a function for which only a reference is available.

```cpp
consteval bool is_global_with_external_linkage(void(*fn)()) {
  std::meta::info rfn = std::meta::reflect_function(*fn);

  return (has_external_linkage(rfn) && parent_of(rfn) == ^^::);
}
```

### `extract<T>`

::: std
```c++
namespace std::meta {
  template<typename T> consteval auto extract(info) -> T;
}
```
:::

If `r` is a reflection for a value of type `T`, `extract<T>(r)` is a prvalue whose evaluation computes the reflected value.

If `r` is a reflection for an object of non-reference type `T`, `extract<T&>(r)` and `extract<T const&>(r)` are lvalues referring to that object.
If the object is usable in constant expressions [expr.const], `extract<T>(r)` evaluates to its value.

If `r` is a reflection for an object of reference type `T` usable in constant-expressions, `extract<T>(r)` evaluates to that reference.

If `r` is a reflection for a function of type `F`, `extract<F*>(r)` evaluates to a pointer to that function.

If `r` is a reflection for a non-static member function and `T` is the type for a pointer to the reflected member function, `extract<T>(r)` evaluates to a pointer to the member function.

If `r` is a reflection for an enumerator constant of type `E`, `extract<E>(r)` evaluates to the value of that enumerator.

If `r` is a reflection for a non-bit-field non-reference non-static member of type `M` in a class `C`, `extract<M C::*>(r)` is the pointer-to-member value for that non-static member.

For other reflection values `r`, `extract<T>(r)` is ill-formed.

The function template `extract` may feel similar to splicers, but unlike splicers it does not require its operand to be a constant-expression itself.
Also unlike splicers, it requires knowledge of the type associated with the entity represented by its operand.

### `data_member_spec`, `define_aggregate`

::: std
```c++
namespace std::meta {
  struct data_member_options {
    struct name_type {
      template <typename T> requires constructible_from<u8string, T>
        consteval name_type(T &&);

      template <typename T> requires constructible_from<string, T>
        consteval name_type(T &&);
    };

    optional<name_type> name;
    optional<int> alignment;
    optional<int> bit_width;
    bool no_unique_address = false;
  };
  consteval auto data_member_spec(info type,
                                  data_member_options options) -> info;
  template <reflection_range R = initializer_list<info>>
  consteval auto define_aggregate(info type_class, R&&) -> info;
}
```
:::

`data_member_spec` returns a reflection of a data member description for a data member of given type. Optional alignment, bit-field-width, and name can be provided as well. An inner class `name_type`, which may be implicitly constructed from any of several "string-like" types (e.g., `string_view`, `u8string_view`, `char8_t[]`, `char_t[]`), is used to represent the name. If a `name` is provided, it must be a valid identifier when interpreted as a sequence of code-units. Otherwise, the name of the data member is unspecified.

`define_aggregate` takes the reflection of an incomplete class/struct/union type and a range of reflections of data member descriptions and completes the given class type with data members as described (in the given order).
The given reflection is returned. For now, only data member reflections are supported (via `data_member_spec`) but the API takes in a range of `info` anticipating expanding this in the near future.

For example:

::: std
```c++
union U;
consteval {
  define_aggregate(^^U, {
  data_member_spec(^^int),
  data_member_spec(^^char),
  data_member_spec(^^double),
});
}

// U is now defined to the equivalent of
// union U {
//   int $_0$;
//   char $_1$;
//   double $_2$;
// };

template<typename T> struct S;
constexpr auto s_int_refl = define_aggregate(^^S<int>, {
  data_member_spec(^^int, {.name="i", .alignment=64}),
  data_member_spec(^^int, {.name=u8"こんにち"}),
});

// S<int> is now defined to the equivalent of
// template<> struct S<int> {
//   alignas(64) int i;
//               int こんにち;
// };
```
:::

When defining a `union`, if one of the alternatives has a non-trivial destructor, the defined union will _still_ have a destructor provided - that simply does nothing.
This allows implementing [variant](#a-simple-variant-type) without having to further extend support in `define_aggregate` for member functions.

If `define_aggregate` is called multiple times with the same arguments, all calls after the first will have no effect. Calling `define_aggregate` for a type that was defined using other arguments, defined through other means, or is in the process of being defined, is not a constant expression.

Revisions of this paper prior to P2996R8 named this function `define_class`. We find `define_aggregate` to be a better name for a few reasons:

1. The capabilities of the function are quite limited, and are mostly good for constructing aggregate types.
2. The new name provides good cause for forcing all data members to be public. Private data members created through such an interface are of very limited utility.
3. The name `define_class` is left available for a future, more fully-featured API.

### Data Layout Reflection
::: std
```c++
namespace std::meta {
  struct member_offset {
    ptrdiff_t bytes;
    ptrdiff_t bits;

    constexpr auto total_bits() const -> ptrdiff_t {
      return CHAR_BIT * bytes + bits;
    }

    auto operator<=>(member_offset const&) const = default;
  };

  consteval auto offset_of(info r) -> member_offset;
  consteval auto size_of(info r) -> size_t;
  consteval auto alignment_of(info r) -> size_t;
  consteval auto bit_size_of(info r) -> size_t;

}
```
:::

These are generalized versions of some facilities we already have in the language.

* `offset_of` takes a reflection of a non-static data member or a base class subobject and returns the offset of it - in bytes and then leftover bits (always between `0` and `7` inclusive).
* `size_of` takes the reflection of a type, object, variable, non-static data member, or base class subobject and returns its size.
* `alignment_of` takes the reflection of a type, non-static data member, or base class subobject and returns its alignment.
* `bit_size_of` gives the size of a base class subobject or non-static data member, except in bits.

::: std
```cpp
struct Msg {
    uint64_t a : 10;
    uint64_t b :  8;
    uint64_t c : 25;
    uint64_t d : 21;
};

static_assert(offset_of(^^Msg::a) == member_offset{0, 0});
static_assert(offset_of(^^Msg::b) == member_offset{1, 2});
static_assert(offset_of(^^Msg::c) == member_offset{2, 2});
static_assert(offset_of(^^Msg::d) == member_offset{5, 3});

static_assert(bit_size_of(^^Msg::a) == 10);
static_assert(bit_size_of(^^Msg::b) == 8);
static_assert(bit_size_of(^^Msg::c) == 25);
static_assert(bit_size_of(^^Msg::d) == 21);

static_assert(offset_of(^^Msg::a).total_bits() == 0);
static_assert(offset_of(^^Msg::b).total_bits() == 10);
static_assert(offset_of(^^Msg::c).total_bits() == 18);
static_assert(offset_of(^^Msg::d).total_bits() == 43);

```
:::


### Other Type Traits

There is a question of whether all the type traits should be provided in `std::meta`.
For instance, a few examples in this paper use `std::meta::remove_cvref(t)` as if that exists.
Technically, the functionality isn't strictly necessary - since it can be provided indirectly:

::: cmptable
### Direct
```cpp
remove_cvref(type)
```

### Indirect
```cpp
dealias(substitute(^^std::remove_cvref_t, {type}))
```

---

```cpp
is_const_type(type)
```

```cpp
extract<bool>(substitute(^^std::is_const_v, {type}))
```
:::

The indirect approach is a lot more typing, and you have to remember to `dealias` the result of the type traits as well (because `substitute(^^std::remove_cvref_t, {^^int const})` gives you a reflection of an alias to `int`, not a reflection of `int`), so it's both more tedious and more error prone.

Having `std::meta::meow` for every trait `std::meow` is more straightforward and will likely be faster to compile, though means we will have a much larger library API. There are quite a few traits in [meta]{.sref} - but it should be easy enough to specify all of them.
So we're doing it.

Now, one thing that came up is that the straightforward thing we want to do is to simply add a `std::meta::meow` for every trait `std::meow` and word it appropriately. That's what we initially tried to do. However, we've run into some conflicts.

The standard library type traits are all *type* traits - they only accept types.
As such, their names are simply things like `std::is_pointer`, `std::is_const`, `std::is_lvalue_reference`, and so forth.
Renaming it to `std::type_is_pointer`, for instance, would be a waste of characters since there's nothing else the argument could be save for a type.

But this is no longer the case. Consider the name `is_function`. It could be:

1. A consteval function equivalent of the type trait `std::is_function<T>`, such that `std::meta::is_function(e)` mandates that `e` represents a type and checks if that type is a function type.

2. A new kind of reflection query `std::meta::is_function(e)` which asks if `e` is the reflection of a function (as opposed to a type or a namespace or a template, etc.).
  This is the same category of query as `std::meta::is_template` or `std::meta::is_concept` or `std::meta::is_namespace`.

Both of these are useful, yet they mean different things entirely - the first is ill-formed when passed a reflection of a function (as opposed to a function type), and the second would simply answer `false` for the reflection of _any_ type (function type or otherwise).

Moreover, in this case it's actually important that the reflection query `std::meta::is_function` does _not_ return `true` for a function type so that using `is_function` as a filter for `members_of` does the expected thing — only giving you back functions, rather than also types.

A similar kind of clash could occur with other functions — for instance, we don't have an `is_array(r)` right now that would check if `r` were the reflection of an array (as opposed to an array type), but we could in the future.

There are a few other examples of name clashes where we want the reflection query to apply to more inputs than simply types. For example, the type trait `std::is_final` can only ask if a type is a final class type, but the metafunction `std::meta::is_final` can ask if a member function is a final member function. Likewise `std::meta::is_const` can apply to objects or types too, and so forth.

The question becomes — how can we incorporate the type traits into the consteval metafunction domain while avoiding these name clash issues. We know of a few approaches.

1. Put all the type traits in their own namespace, like `std::meta::traits::meow`. This has the benefit that we preserve the existing name, but now we lose ADL. We can't write `traits::remove_cvref(type)` unless we bring in `traits` as a namespace alias for `std::meta::traits`, and if we bring in the entire namespace then we're back to the name clash problem (it's just that now the calls become ambiguous).

2. Add a prefix or suffix to every type trait. This preserves the ability to use ADL and makes the new names easy to remember (since `std::meow_v<T>` just directly translates into `std::meta::type_meow(type)` for all `meow`), at the cost of worse names.

3. Do something more tailored on a case-by-case basis.

We don't think the nested namespace approach (#1) is a good idea because of the loss of ADL and the more inconvenient call syntax.

Previous revisions of this proposal used the `type_` prefix (#2) uniformly. This had the downside that some type traits end up reading awkwardly (`type_is_pointer` as opposed to `is_pointer_type`) but several others do read much better (`type_has_virtual_destructor` as opposed to `has_virtual_destructor_type`). Some type traits look equally ridiculous with either a prefix or suffix (`type_common_type` vs `common_type_type`).

A more bespoke approach (#3) would be to do something based on the grammar of the existing type traits:

* All the type traits of the form `is_meow` can become `is_meow_type`. This reads quite nicely for most of them (`is_pointer_type`, `is_trivially_copyable_type`, `is_void_type`, etc.). `is_swappable_with_type` or `is_pointer_convertible_base_of_type` or `is_invocable_type` maybe aren't amazing, but they're not terrible either. There are 76 of these and having a uniform transformation is valuable. We could even simply special case the few that are known to conflict (or, in the case of `is_array`, might conflict in the future, but that's a little harder to internalize).
* The remaining ones could potentially keep their current name in `std::meta::` form as well. There are a few things to point out with these remaining traits though:
  * a couple type transformations like `add_const` could potentially also apply to member functions for the purposes of generating code (although some of these, like `add_lvalue_reference`, we'd want to spell in terms of the qualifier, so those wouldn't conflict). It'd probably be okay to start with an `add_const` that only applies to types and eventually extend it, if we go that route though.
  * `alignment_of` goes away entirely (since we already have `std::meta::alignment_of`). Nobody will notice.
  * a couple of these could meaningfully apply to objects as well as types, like `has_virtual_destructor`, but we would still only apply to types.

Note that either way, we're also including a few common traits that aren't defined in the same places — those are the tuple traits (`tuple_size`/`tuple_element`) and the variant traits (`variant_size`/`variant_alternative`).

Starting from R8, this paper uses option #3. That is: every type trait `std::is_meow` is introduced as `std::meta::is_meow_type`, while all other type traits `std::meow` are introduced as `std::meta::meow`.

## ODR Concerns

Static reflection invariably brings new ways to violate ODR.

::: std
```cpp
// File 'cls.h'
struct Cls {
  void odr_violator() {
    if constexpr (members_of(parent_of(^^std::size_t)).size() % 2 == 0)
      branch_1();
    else
      branch_2();
  }
};
```
:::

Two translation units including `cls.h` can generate different definitions of `Cls::odr_violator()` based on whether an odd or even number of declarations have been imported from `std`. Branching on the members of a namespace is dangerous because namespaces may be redeclared and reopened: the set of contained declarations can differ between program points.

The creative programmer will find no difficulty coming up with other predicates which would be similarly dangerous if substituted into the same `if constexpr` condition: for instance, given a branch on `is_complete_type(^^T)`, if one translation unit `#include`s a forward declaration of `T`, another `#include`s a complete definition of `T`, and they both afterwards `#include "cls.h"`, the result will be an ODR violation.

Additional papers are already in flight proposing additional metafunctions that pose similar dangers. For instance, [@P3096R2] proposes the `parameters_of` metafunction. This feature is important for generating language bindings (e.g., Python, JavaScript), but since parameter names can differ between declarations, it would be dangerous for a member function defined in a header file to branch on the name of a parameter.

These cases are not difficult to identify: Given an entity `E` and two program points `P1` and `P2` from which a reflection of `E` may be optained, it is unsafe to branch runtime code generation on any property of `E` (e.g., namespace members, parameter names, completeness of a class) that can be modified between `P1` and `P2`. Worth noting as well, these sharp edges are not unique (or new) to reflection: It is already possible to build an ODR trap based on the completeness of a class using C++23.

Education and training are important to help C++ users avoid such sharp edges, but we do not find them sufficiently concerning to give pause to our enthusiasm for the features proposed by this paper.

# Proposed Wording

[Throughout the wording, we say that a reflection (an object of type `std::meta::info`) *represents* some source construct, while splicing that reflection *designates* that source construct. For instance, `^^int` represents the type `int` and `[: ^^int :]` designates the type `int`.]{.ednote}

## Language

### [lex.phases]{.sref} Phases of translation {-}

[In addition to changes necessary for this proposal, we are applying the "drive-by fix" of merging phases 7/8, in order to clarify that template instantiation is interleaved with translation. In so doing, we replace the notion of "instantiation units" with a partial ordering among all program constructs in a translation unit.]{.ednote}

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

::: std

[7-8]{.pnum} Each preprocessing token is converted into a token ([lex.token]). Whitespace characters separating tokens are no longer significant. The resulting tokens constitute a _translation unit_ and are syntactically and semantically analyzed as a `$translation-unit$` ([basic.link]) and translated.

  [The process of analyzing and translating the tokens can occasionally result in one token being replaced by a sequence of other tokens ([temp.names])]{.note3}

  It is implementation-defined whether the sources for module-units and header units on which the current translation unit has an interface dependency ([module.unit], [module.import]) are required to be available.

  [Source files, translation units and translated translation units need not necessarily be stored as files, nor need there be any one-to-one correspondence between these entities and any external representation. The description is conceptual only, and does not specify any particular implementation.]{.note}

  [Translated translation units and instantiation units are combined as follows:]{.rm}

  [[Some or all of these can be supplied from a library.]{.note5}]{.rm}

  [Each translated translation unit is examined to produce a list of required instantiations.]{.rm}

  [While the tokens constituting translation units are being analyzed and translated, required instantiations are performed.]{.addu}

  [This can include instantiations which have been explicitly requested ([temp.explicit]).]{.note5}

  [The contexts from which instantiations may be performed are determined by their respective points of instantiation ([temp.point]).]{.addu}

  [[Other requirements in this document can further constrain the  context from which an instantiation can be performed. For example, a constexpr function template specialization might have a point of instantation at the end of a translation unit, but its use in certain constant expressions could require that it be instantiated at an earlier point ([temp.inst]).]{.note}]{.addu}

  [The definitions of the required templates are located. It is implementation-defined whether the source of the translation units containing these definitions is required to be available.]{.rm}

  [[An implementation can choose to encode sufficient information into the translated translation unit so as to ensure the source is not required here.]{.note}]{.rm}

  [All required instantiations are perfomed to produce _instantiation units_.]{.rm}

  [[These are similar to translated translation units, but contain no references to uninstantiated templates and no template definitions.]{.note}]{.rm}

  [Each instantiation results in new program constructs.]{.addu} The program is ill-formed if any instantiation fails.

::: addu
  During the analysis and translation of tokens, certain expressions are evaluated ([expr.const]). Constructs appearing at a program point `$P$` are analyzed in a context where each side effect of evaluating an expression `$E$` as a full-expression is complete if and only if

  - [7-8.#]{.pnum} `$E$` is the expression corresponding to a `$consteval-block-declaration$` ([dcl.pre]), and
  - [7-8.#]{.pnum} either that `$consteval-block-declaration$` or the template definition from which it is instantiated is reachable from ([module.reach])

    - [7-8.#.#]{.pnum} `$P$`, or
    - [7-8.#.#]{.pnum} the point immediately following the `$class-specifier$` of the outermost class for which `$P$` is in a complete-class context ([class.mem.general]).

::: example
```cpp
class S {
  class Incomplete;

  class Inner {
    void fn() {
      /* p1 */ Incomplete i; // OK, constructs at P1 are analyzed in a context where the side effect of
                             // the call to define_aggregate is evaluated because:
                             // * E is the expression corresponding to a consteval block, and
                             // * P1 is in a complete-class context of S and the consteval block
                             //   is reachable from P3.
    }
  }; /* p2 */

  consteval {
    define_aggregate(^^Incomplete, {});
  }
}; /* p3 */
```
:::
:::

  [8]{.pnum} [All]{.rm} [Translated translation units are combined, and all]{.addu} external entity references are resolved. Library components are linked to satisfy external references to entities not defined in the current translation. All such translator output is collected into a program image which contains information needed for execution in its execution environment.

:::

### [lex.pptoken]{.sref} Preprocessing tokens {-}

Add a bullet after bullet (4.2):

::: std
* [4]{.pnum} If the input stream has been parsed into preprocessing tokens up to a given character:

  * [#.#]{.pnum} ...
  * [#.#]{.pnum} Otherwise, if the next three characters are `<::` and the subsequent character is neither `:` nor `>`, the `<` is treated as a preprocessing token by itself and not as the first character of the alternative token `<:`.
  * [[#.#]{.pnum} Otherwise, if the next three characters are `[::` and the subsequent character is not `:`, or if the next three characters are `[:>`, the `[` is treated as a preprocessing token by itself and not as the first character of the preprocessing token `[:`.]{.addu}

      [[The tokens `[:` and `:]` cannot be composed from digraphs.]{.note}]{.addu}
  * [#.#]{.pnum} ...
:::

### [lex.operators]{.sref} Operators and punctuators {-}

Change the grammar for `$operator-or-punctuator$` in paragraph 1 of [lex.operators]{.sref} to include the reflection operator and the `$splice-specifier$` delimiters:

::: std
```
  $operator-or-punctuator$: @_one of_@
         {        }        [        ]        (        )        @[`[:        :]`]{.addu}@
         <:       :>       <%       %>       ;        :        ...
         ?        ::       .       .*        ->       ->*      ~
         !        +        -        *        /        %        ^        @[`^^`]{.addu}@       &
         |        =        +=       -=       *=       /=       %=       ^=       &=
         |=       ==       !=       <        >        <=       >=       <=>      &&
         ||       <<       >>       <<=      >>=      ++       --       ,
         and      or       xor      not      bitand   bitor    compl
         and_eq   or_eq    xor_eq   not_eq
```
:::

### [basic.pre]{.sref} Preamble {-}

Modify paragraph 7 such that denoting a variable by its name finds the variable, not the associated object.

::: std
[7]{.pnum} A _variable_ is introduced by the declaration of a reference other than a non-static data member or of an object. [The variable's name, if any, denotes the reference or object.]{.rm}

:::

Add type aliases and namespace aliases to the list of entities in paragraph 8. As drive-by fixes, remove "value", "object", "reference", and "template specialization"; replace "class member" with "non-static data member", since all other cases are subsumed by existing one. Add "template parameters" and "`$init-capture$`s", which collectively subsume "packs". Introduce a notion of an “underlying entity” in the same paragraph, and utilize it for the definition of a name “denoting” an entity. Type aliases are now entities, so also modify accordingly.

::: std
[8]{.pnum} An _entity_ is a [value, object, reference]{.rm} [variable,]{.addu} structured binding, result binding, function, enumerator, type, [type alias]{.addu}, [class]{.rm} [non-static data]{.addu} member, bit-field, template, [template specialization,]{.rm} namespace, [namespace alias, template parameter, function parameter]{.addu}, or [`$init-capture$`]{.addu} [pack]{.rm}. [The _underlying entity_ of an entity is that entity unless otherwise specified. A name _denotes_ the underlying entity of the entity declared by each declaration that introduces the name.]{.addu} [An entity `$E$` is denoted by the name (if any) that is introduced by a declaration of `$E$` or by a `$typedef-name$` introduced by a declaration specifying `$E$`.]{.rm}

[[Type aliases and namespace aliases have underlying entities that are distinct from themselves.]{.note}]{.addu}

:::

### [basic.def]{.sref} Declarations and definitions {-}

Modify the third sentence of paragraph 1 to clarify that type aliases are now entities.

::: std
[1]{.pnum} [...] A declaration of an entity [or `$typedef-name$`]{.rm} `$X$` is a redeclaration of `$X$` if another declaration of `$X$` is reachable from it ([module.reach]); otherwise, it is a _first declaration_. [...]

:::

Since namespace aliases are now entities but their declarations are not definitions, add `$namespace-alias-definition$` to the list of declarations in paragraph 2, just before `$using-declaration$`. Also add `$consteval-block-declaration$`s to the list of declarations in paragraph 2:

::: std
[2]{.pnum} Each entity declared by a `$declaration$` is also _defined_ by that declaration unless:

* [#.#]{.pnum} it declares a function without specifying the function's body ([dcl.fct.def]),

[...]

* [2.10]{.pnum} it is an `$alias-declaration$` ([dcl.typedef]),
* [[2.11-]{.pnum} it is a `$namespace-alias-definition$` ([namespace.alias]),]{.addu}
* [2.11]{.pnum} it is a `$using-declaration$` ([namespace.udecl]),
* [2.12]{.pnum} it is a `$deduction-guide$` ([temp.deduct.guide]),
* [2.13]{.pnum} it is a `$static_assert-declaration$`,
* [[2.13]{.pnum} it is a `$consteval-block-declaration$`,]{.addu}
* [2.14]{.pnum} it is an `$attribute-declaration$` ([dcl.pre]),
* [2.15]{.pnum} it is an `$empty-declaration$` ([dcl.pre]){.rm},

[...]

:::

Also modify the example that follows:

::: example
All but one of the following are definitions:
```diff
  int a;                          // defines a
  extern const int c = 1;         // defines c
  int f(int x) { return x+a; }    // defines f and defines x
  struct S { int a; int b; };     // defines S, S::a, and S::b
  struct X {                      // defines X
    int x;                        // defines non-static data member x
    static int y;                 // declares static data member y
    X() : x(0) { }                // defines a constructor of X
  };
  int X::y = 1;                   // defines X::y
  enum { up, down };              // defines up and down
  namespace N {int d; }           // defines N and N::d
- namespace N1 = N;               // defines N1
  X anX;                          // defines anX
```
whereas these are just declarations:
```diff
  extern int a;                   // declares a
  extern const int c;             // declares c
  int f(int);                     // declares f
  struct S;                       // declares S
  typedef int Int;                // declares Int
+ namespace N1 = N;               // declares N1
  extern X anotherX;              // declares anotherX
  using N::d;                     // declares d
```
:::

### [basic.def.odr]{.sref} One-definition rule {-}

Add `$splice-expression$`s to the set of potential results of an expression in paragraph 3.

::: std
[3]{.pnum} An expression or conversion is _potentially evaluated_ unless it is an unevaluated operand ([expr.context]), a subexpression thereof, or a conversion in an initialization or conversion sequence in such a context. The set of _potential results_ of an expression `$E$` is defined as follows:

- [#.#]{.pnum} If `$E$` is an `$id-expression$` ([expr.prim.id]) [or a `$splice-expression$` ([expr.prim.splice])]{.addu}, the set contains only `$E$`.
- [#.#]{.pnum} [...]

[This set is a (possibly-empty) set of `$id-expression$`s [and `$splice-expression$`s]{.addu}, each of which is either `$E$` or a subexpression of `$E$`.]{.note}

::: example
In the following example, the set of potential results of the initializer of `n` contains the first `S::x` subexpression, but not the second `S::x` subexpression. [The set of potential results of the initializer of `o` contains the `[:^^S::x:]` subexpression.]{.addu}

```diff
  struct S { static const int x = 0; };
  const int &f(const int &r);
  int n = b ? (1, S::x)           // S::x is not odr-used here
            : f(S::x);            // S::x is odr-used here, so a definition is required
+ int o = [:^^S::x:];
```

:::
:::

Modify the first sentence of paragraph 5 to cover splicing of variables:

::: std
[5]{.pnum} A variable is named by an expression if the expression is an `$id-expression$` [or `$splice-expression$` ([expr.prim.splice])]{.addu} that [denotes]{.rm} [designates]{.addu} it.
:::

Modify paragraph 6 to cover splicing of structured bindings:

::: std
[6]{.pnum} A structured binding is [odr-used if it appears as a potentially-evaluated]{.rm} [named by an]{.addu} expression [if that expression is either an]{.addu} `$id-expression$` [or a `$splice-expression$` that designates that structured binding. A structured binding is odr-used if it is named by a potentially-evaluated expression.]{.addu}

:::

Prepend before paragraph 15 of [basic.def.odr]{.sref}:

::: std

::: addu
[15pre]{.pnum} If a definable item `D` is defined in a translation unit by an injected declaration `$X$` ([expr.const]) and another translation unit contains a definition of `D`, that definition shall be an injected declaration having the same characteristic sequence as `$X$`; a diagnostic is required only if `D` is attached to a named module and a prior definition is reachable at the point where a later definition occurs.

:::


[15]{.pnum} For any [other]{.addu} definable item `D` with definitions in multiple translation units,

* if `D` is a non-inline non-templated function or variable, or
* if the definitions in different translation units do not satisfy the following requirements,

the program is ill-formed; a diagnostic is required only if the definable item is attached to a named module and a prior definition is reachable at the point where a later definition occurs. [...]
:::

Prefer the verb "denote" in bullet 15.5 to emphasize that ODR "looks through" aliases, and clarify that objects are not entities in bullet 15.5.2.

::: std
- [15.5]{.pnum} In each such definition, corresponding names, looked up according to [basic.lookup], shall [refer to]{.rm} [denote]{.addu} the same entity, after overload resolution ([over.match]) and after matching of partial template specialization ([temp.over]), except that a name can refer to
  - [#.#.#]{.pnum} a non-volatile const object [...], or
  - [#.#.#]{.pnum} a reference with internal or no linkage initialized with a constant expression such that the reference refers to the same [entity]{.rm} [object or function]{.addu} in all definitions of `$D$`.

:::

Clarify in bullet 15.11 that default template-arguments in `$splice-specialization-specifier$`s also factor into ODR:

::: std
- [15.11]{.pnum} In each such definition, a default argument used by an (implicit or explicit) function call or a default template argument used by an (implicit or explicit) `$template-id$`[,]{.addu} [or]{.rm} `$simple-template-id$`[, or `$splice-specialization-specifier$`]{.addu} is treated as if its token sequence were present in the definition of `$D$`; that is, the default argument or default template argument is subject to the requirements described in this paragraph (recursively).

:::

And add a bullet thereafter that factors the result of a `$reflect-expression$` into ODR.

::: std
::: addu
- [15.11+]{.pnum} In each such definition, corresponding `$reflect-expression$`s ([expr.reflect]) compute equivalent values ([expr.eq]).

:::
:::

### [basic.scope.scope]{.sref} General {-}

[The introduction of a "host scope" in paragraph 2 is part of the resolution to [@CWG2701].]{.ednote}

Define the "host scope" of a declaration in paragraph 2:

::: std
[2]{.pnum} Unless otherwise specified:

- [#.#]{.pnum} The smallest scope that contains a scope `$S$` is the _parent scope_ of `$S$`.

[...]

- [#.5]{.pnum} Any names (re)introduced by a declaration are _bound_ to it in its target scope.

[The _host scope_ of a declaration is the inhabited scope if that scope is a block scope and the target scope otherwise.]{.addu} An entity _belongs_ to a scope `$S$` if `$S$` is the target scope of a declaration of the entity.

:::

Change bullet 4.2 to refer to the declaration of a "type alias" instead of a `$typedef-name$`.

::: std
[4.2]{.pnum} one declares a type (not a [`$typedef-name$`]{.rm} [type alias]{.addu}) and the other declares a variable, non-static data member other than an anonymous union ([class.union.anon]), enumerator, function, or function template, or

:::

### [basic.lookup.general]{.sref} General {-}

Adjust paragraph 4 since type aliases are now entities.

::: std
[4]{.pnum} In certain contexts, only certain kinds of declarations are included. After any such restriction, any declarations of classes or enumerations are discarded if any other declarations are found.

[A type (but not a [`$typedef-name$`]{.rm} [type alias]{.addu} or template) is therefore hidden by any other entity in its scope.]{.note4}

However, if lookup is _type-only_, only declarations of types and templates whose specializations are types are considered; furthermore, if declarations of a [`$typedef-name$`]{.rm} [type alias]{.addu} and of [the type to which it refers]{.rm} [its underlying entity]{.addu} are found, the declaration of the [`$typedef-name$`]{.rm} [type alias]{.addu} is discarded instead of the type declaration.

:::

### [basic.lookup.argdep]{.sref} Argument-dependent name lookup {-}

Modify the first bullet of paragraph 3 of [basic.lookup.argdep]{.sref} as follows:

::: std

[3]{.pnum} ... Any `$typedef-name$`s and `$using-declaration$`s used to specify the types do not contribute to this set. The set of entities is determined in the following way:

* [[#.1-]{.pnum} If `T` is `std::meta::info` ([meta.reflection.synop]), its associated set of entities is the singleton containing the enumeration type `std::meta::operators` ([meta.reflection.operators]).]{.addu}

  [[The `std::meta::info` type is a type alias, so an explicit rule is needed to associate calls whose arguments are reflections with the namespace `std::meta`.]{.note}]{.addu}

* [#.1]{.pnum} If `T` is [a]{.rm} [any other]{.addu} fundamental type, its associated set of entities is empty.
* [#.#]{.pnum} If `T` is a class type ...

:::

### [basic.lookup.qual.general]{.sref} General {-}

Extend paragraph 1 to cover `$splice-specifier$`s:

::: std
[1]{.pnum} Lookup of an *identifier* followed by a `::` scope resolution operator considers only namespaces, types, and templates whose specializations are types. If a name, `$template-id$`, [`$splice-scope-specifier$`,]{.addu} or `$computed-type-specifier$` is followed by a `::`, it shall [either be a dependent `$splice-scope-specifier$` ([temp.dep.splice]) or it shall]{.addu} designate a namespace, class, enumeration, or dependent type, and the `::` is never interpreted as a complete `$nested-name-specifier$`.

:::

Add `$reflection-name$` ([expr.reflect]) to the list of non-terminals in bullet 2.4.5 that are "qualified names" in the presence of a  `$nested-name-specifier$`:

::: std
[2]{.pnum} [...] A _qualified name_ is

- [2.3]{.pnum} a member-qualified name or
- [#.#]{.pnum} the terminal name of
  - [#.#.#]{.pnum} a `$qualified-id$`,
  - [#.#.#]{.pnum} a `$using-declarator$`,
  - [#.#.#]{.pnum} a `$typename-specifier$`,
  - [#.#.#]{.pnum} a `$qualified-namespace-specifier$`, or
  - [#.#.#]{.pnum} a `$nested-name-specifier$`, [`$reflection-name$`, ]{.addu} `$elaborated-type-specifier$`, or `$class-or-decltype$` that has a `$nested-name-specifier$` ([expr.prim.id.qual]).

:::

### 6.5+ [basic.splice] Splice specifiers {-}

Add a new subsection after [basic.lookup]{.sref}, and renumber accordingly:

::: std
::: addu
**Splice specifiers   [basic.splice]**

```
$splice-specifier$:
  [: $constant-expression$ :]

$splice-specialization-specifier$:
  $splice-specifier$ < $template-argument-list$@~_opt_~@ >
```

[1]{.pnum} The `$constant-expression$` of a `$splice-specifier$` shall be a converted constant expression of type `std::meta::info` ([expr.const]). A `$splice-specifier$` whose converted `$constant-expression$` represents a construct `$X$` is said to _designate_ either

* [#.#]{.pnum} the underlying entity of `$X$` if `$X$` is an entity ([basic.pre]), or
* [#.#]{.pnum} `$X$` otherwise.

[A `$splice-specifier$` is dependent if the converted `$constant-expression$` is value-dependent ([temp.dep.splice]).]{.note}

[#]{.pnum} A non-dependent `$splice-specifier$` of a `$splice-specialization-specifier$` shall designate a template.

[#]{.pnum} [A `<` following a `$splice-specifier$` is interpreted as the delimiter of a `$template-argument-list$` when the `$splice-specifier$` is preceded by the keyword `template` or the keyword `typename`, or when it appears in a type-only context ([temp.names]).]{.note}

::: example
```cpp
constexpr int v = 1;
template <int V> struct TCls {
  static constexpr int s = V + 1;
};

using alias = [:^^TCls:]<([:^^v:])>;
  // OK, a splice-specialization-specifier with a splice-expression
  // as a template argument

static_assert(alias::s == 2);

auto o1 = [:^^TCls:]<([:^^v:])>();          // error: < means less than
auto o2 = typename [:^^TCls:]<([:^^v:])>(); // OK, o2 is an object of type TCls<1>

consteval int bad_splice(std::meta::info v) {
    return [:v:]; // error: v is not constant
}
```
:::

:::
:::

### [basic.link]{.sref} Program and Linkage {-}

Define when two aliases are equivalent in paragraph 8:

::: std
[8]{.pnum} Two declarations of entities declare the same entity if, considering declarations of unnamed types to introduce their names for linkage purposes, if any ([dcl.typedef], [dcl.enum]), they correspond ([basic.scope.scope]), have the same target scope that is not a function or template parameter scope, neither is a name-independent declaration, and either

- [#.#]{.pnum} they appear in the same translation unit, or
- [[#.#]{.pnum} they both declare type aliases or namespace aliases that have the same underlying entity, or]{.addu}
- [#.#]{.pnum} they both declare names with module linkage and are attached to the same module, or
- [#.#]{.pnum} they both declare names with external linkage.

:::

Consider `$reflect-expression$`s and `$splice-specifier$`s to name entities and extend the definition of TU-local values and objects to include reflections. Define TU-local namespaces, namespace aliases, and type aliases. The below addition of "value or object of a TU-local type" is a drive-by fix to make sure that enumerators in a TU-local enumeration are also TU-local. There is a second drive-by fix which adds namespaces to the list of entities that can be TU-local. Lastly, the recategorization of aliases as entities fixes an existing issue in which paragraph 17 did not previously apply to aliases.

::: std

[13]{.pnum} A declaration `$D$` _names_ an entity `$E$` if

* [13.1]{.pnum} `$D$` contains a `$lambda-expression$` whose closure type is `$E$`,
* [#.*]{.pnum} [`$D$` contains a `$reflect-expression$` or a `$splice-specifier$` that, respectively, represents or designates `$E$`,]{.addu}
* [#.2]{.pnum} `$E$` is not a function or function template and `$D$` contains an `$id-expression$`, `$type-specifier$`, `$nested-name-specifier$`, `$template-name$`, or `$concept-name$` denoting `$E$`, [or]{.rm}
* [#.#]{.pnum} `$E$` is a function or function template and `$D$` contains an expression that names `$E$` ([basic.def.odr]) or an `$id-expression$` that refers to a set of overloads that contains `$E$`[, or]{.addu}
* [[#.#]{.pnum} `$D$` is an injected declaration ([expr.const]) whose characteristic sequence contains a reflection that represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) for which `$T$` is `$E$`]{.addu}.

  [Non-dependent names in an instantiated declaration do not refer to a set of overloads ([temp.res]).]{.note7}

[14]{.pnum} A declaration is an _exposure_ if it either names a TU-local entity (defined below), ignoring

- [#.#]{.pnum} the `$function-body$` for a non-inline function or function template (but not the deduced return type for a (possibly instantiated) definition of a function with a declared return type that uses a placeholder ytpe ([dcl.spec.auto])),
- [#.#]{.pnum} the `$initializer$` for a variable or variable template (but not the variable's type),
- [#.#]{.pnum} friend declarations in a class definition, and
- [#.#]{.pnum} any reference to a non-volatile const object or reference with internal or no linkage initialized with a constant expression that is not an odr-use ([basic.def.odr]),

or defines a constexpr variable initialized to a TU-local value (defined below).

[An inline function template can be an exposure even though certain explicit specializations of it would be usable in other translation units.]{.note8}

[15]{.pnum} An entity is _TU-local_ if it is

- [#.#]{.pnum} a type, [type alias, namespace, namespace alias,]{.addu} function, variable, or template that
  - [#.#.#]{.pnum} has a name with internal linkage, or
  - [#.#.#]{.pnum} does not have a name with linkage and is declared, or introduced by a `$lambda-expression$`, within the definition of a TU-local entity,
- [#.#]{.pnum} a type with no name that is defined outside a `$class-specifier$`, function body, or `$initializer$` or is introduced by a `$defining-type-specifier$` that is used to declare only TU-local entities,
- [#.#]{.pnum} a specialization of a TU-local template,
- [#.#]{.pnum} a specialization of a template with any TU-local template arguments, or
- [#.#]{.pnum} a specialization of a template whose (possibly instantiated) declaration is an exposure.

  [A specialization can be produced by implicit or explicit instantiation.]{.note9}

[16]{.pnum} A value or object is _TU-local_ if

* [[16.0]{.pnum} it is of TU-local type,]{.addu}
* [16.1]{.pnum} it is, or is a pointer to, a TU-local function or the object associated with a TU-local variable, [or]{.rm}
* [16.2]{.pnum} it is an object of class or array type and any of its subobjects or any of the objects or functions to which its non-static data members of reference type refer is TU-local and is usable in constant expressions[.]{.rm}[, or]{.addu}

:::addu
* [16.3]{.pnum} it is a reflection value ([basic.fundamental]) that represents
  * [16.#.#]{.pnum} an entity, value, or object that is TU-local,
  * [16.#.#]{.pnum} a direct base class relationship ([class.derived.general]) (`$D$`, `$B$`) for which either `$D$` or `$B$` is TU-local, or
  * [16.#.#]{.pnum} a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) for which `$T$` is TU-local.
:::

[17]{.pnum} If a (possibly instantiated) declaration of, or a deduction guide for, a non-TU-local entity in a module interface unit (outside the `$private-module-fragment$`, if any) or module partition ([module.unit]) is an exposure, the program is ill-formed. Such a declaration in any other context is deprecated ([depr.local]).

[18]{.pnum} If a declaration that appears in one translation unit names a TU-local entity declared in another translation unit that is not a header unit, the program is ill-formed. A declaration instantiated for a template specialization ([temp.spec]) appears at the point of instantiation of the specialization ([temp.point]).

:::

Add examples demonstrating the above rules to the example in paragraph 19:

::: std
::: example4
Translation unit #1:
```cpp
export module A;
static void f() {}
inline void it() { f(); }          // error: is an exposure of f
static inline void its() { f(); }  // OK
template<int> void g() { its(); }  // OK
template void g<0>();

[...]

inline void h(auto x) { adl(x); }  // OK, but certain specializations are exposures

@[`constexpr std::meta::info r1 = ^^g<0>;  // OK`]{.addu}@
@[`namespace N2 {`]{.addu}@
@[`static constexpr std::meta::info r2 = ^^g<1>;  // OK, r2 is TU-local`]{.addu}@
@[`}`]{.addu}@
@[`constexpr std::meta::info r3 = ^^f; // error: r3 is an exposure of f`]{.addu}@

@[`constexpr auto ctx = std::meta::access_context::current();`]{.addu}@
@[`constexpr std::meta::info r4 = std::meta::members_of(^^N2, ctx)[0];`]{.addu}@
@[\ \ `// error: r4 is an exposure of N2::r2`]{.addu}@
```

Translation unit #2:
```cpp
module A;
[...]
```
:::
:::

### [basic.types.general]{.sref} General {-}

Change the first sentence in paragraph 9 of [basic.types.general]{.sref} as follows:

::: std
[9]{.pnum} Arithmetic types ([basic.fundamental]), enumeration types, pointer types, pointer-to-member types ([basic.compound]), [`std::meta::info`,]{.addu} `std::nullptr_t`, and cv-qualified versions of these types are collectively called _scalar types_. ...
:::

Add a new paragraph at the end of [basic.types.general]{.sref} as follows:

::: std
::: addu

[12]{.pnum} A type is _consteval-only_ if it is either `std::meta::info` or a type compounded from a consteval-only type ([basic.compound]). Every object of consteval-only type shall be

  - [#.#]{.pnum} the object associated with a constexpr variable or a subobject thereof,
  - [#.#]{.pnum} a template parameter object ([temp.param]) or a subobject thereof, or
  - [#.#]{.pnum} an object whose lifetime begins and ends during the evaluation of a core constant expression.

:::
:::

### [basic.fundamental]{.sref} Fundamental types {-}

Add new paragraphs before the last paragraph of [basic.fundamental]{.sref} as follows:

::: std
[16]{.pnum} The types denoted by `$cv$ std​::​nullptr_t` are distinct types. [...]

::: addu

[x]{.pnum} A value of type `std::meta::info` is called a _reflection_. There exists a unique _null reflection_; every other reflection is a representation of

* [x.#]{.pnum} a value of scalar type ([temp.param]),
* [x.#]{.pnum} an object with static storage duration ([basic.stc]),
* [x.#]{.pnum} a variable ([basic.pre]),
* [x.#]{.pnum} a structured binding ([dcl.struct.bind]),
* [x.#]{.pnum} a function ([dcl.fct]),
* [x.#]{.pnum} an enumerator ([dcl.enum]),
* [x.#]{.pnum} a type alias ([dcl.typedef]),
* [x.#]{.pnum} a type ([basic.types]),
* [x.#]{.pnum} a class member ([class.mem]),
* [x.#]{.pnum} an unnamed bit-field ([class.bit]),
* [x.#]{.pnum} a class template ([temp.pre]),
* [x.#]{.pnum} a function template ([temp.pre]),
* [x.#]{.pnum} a variable template ([temp.pre]),
* [x.#]{.pnum} an alias template ([temp.alias]),
* [x.#]{.pnum} a concept ([temp.concept]),
* [x.#]{.pnum} a namespace alias ([namespace.alias]),
* [x.#]{.pnum} a namespace ([basic.namespace.general]),
* [x.#]{.pnum} a direct base class relationship ([class.derived.general]), or
* [x.#]{.pnum} a data member description ([class.mem.general]).

A reflection is said to _represent_ the corresponding construct.

[A reflection of a value can be produced by library functions such as `std::meta::constant_of` and `std::meta::reflect_constant`.]{.note}

::: example
```cpp
int arr[] = {1, 2, 3};
auto [a1, a2, a3] = arr;
void fn();
enum Enum { A };
using Alias = int;
struct B {};
struct S : B {
  int mem;
  int : 0;
};
template <auto> struct TCls {};
template <auto> void TFn();
template <auto> int TVar;
template <auto> concept Concept = requires { true; };
namespace NS {};
namespace NSAlias = NS;

constexpr auto ctx = std::meta::access_context::current();

constexpr auto r1 = std::meta::reflect_constant(42);  // represents int value of 42

constexpr auto r2 = std::meta::reflect_object(arr[1]);  // represents int object

constexpr auto r3 = ^^arr;      // represents a variable
constexpr auto r4 = ^^a3;       // represents a structured binding
constexpr auto r5 = ^^fn;       // represents a function
constexpr auto r6 = ^^Enum::A;  // represents an enumerator
constexpr auto r7 = ^^Alias;    // represents a type alias
constexpr auto r8 = ^^S;        // represents a type
constexpr auto r9 = ^^S::mem;   // represents a class member

constexpr auto r10 = std::meta::members_of(^^S, ctx)[1];
    // represents an unnamed bit-field

constexpr auto r11 = ^^TCls;     // represents a class template
constexpr auto r12 = ^^TFn;      // represents a function template
constexpr auto r13 = ^^TVar;     // represents a variable template
constexpr auto r14 = ^^Concept;  // represents a concept
constexpr auto r15 = ^^NSAlias;  // represents a namespace alias
constexpr auto r16 = ^^NS;       // represents a namespace

constexpr auto r17 = std::meta::bases_of(^^S, ctx)[0];
    // represents a direct base class relationship

constexpr auto r18 = std::meta::data_member_spec(^^int, {.name="member"});
    // represents a data member description
```

:::

[y]{.pnum} *Recommended practice*: Implementations should not represent other constructs specified in this document, such as `$using-declarator$`s, partial template specializations, attributes, placeholder types, statements, or expressions, as values of type `std::meta::info`.

[z]{.pnum} [Future revisions of this document can specify semantics for reflections representing any such constructs.]{.note}

:::

[17]{.pnum} The types described in this subclause are called *fundamental types*.
:::

### [intro.execution]{.sref} Sequential execution {-}

Introduce a new kind of side effect in paragraph 7 (i.e., injecting a declaration).

::: std
[7]{.pnum} Reading an object designated by a `volatile` glvalue ([basic.lval]), modifying an object, [producing an injected declaration ([expr.const]),]{.addu} calling a library I/O function, or calling a function that does any of those operations are all _side effects_, which are changes in the state of the execution [or translation]{.addu} environment. _Evaluation_ of an expression (or a subexpression) in general includes both value computations (including determining the identity of an object for glvalue evaluation and fetching a value previously assigned to an object for prvalue evaluation) and initiation of side effects. When a call to a library I/O function returns or an access through a volatile glvalue is evaluated, the side effect is considered complete, even though some external actions implied by the call (such as the I/O itself) or by the `volatile` access may not have completed yet.

:::

Add a new paragraph to the end of [intro.execution] specifying a stronger sequencing during constant evaluation.

::: std
::: addu
[15+]{.pnum} During the evaluation of an expression as a core constant expression ([expr.const]), evaluations of operands of individual operators and of subexpressions of individual expressions that are otherwise either unsequenced or indeterminately sequenced are evaluated in lexical order.
:::

:::

### [basic.lval]{.sref} Value category {-}

Apply a drive-by fix to bullet 1.1 clarifying that a glvalue can also determine the identity of a non-static data member.

::: std
* [1.1]{.pnum} A _glvalue_ is an expression whose evaluation determines the identity of an object[,]{.addu} [or]{.rm} function[, or non-static data member]{.addu}.

:::

Account for move-eligible `$splice-expression$`s in bullet 4.1 of Note 3.

::: std
* [4.1]{.pnum} a move-eligible `$id-expression$` ([expr.prim.id.unqual]) [or `$splice-expression$` ([expr.prim.splice])]{.addu},

:::

### [expr.context]{.sref} Context dependence {-}

Add `$reflect-expression$`s to the list of unevaluated operands in paragraph 1.

::: std
[1]{.pnum} In some contexts, _unevaluated operands_ appear ([expr.prim.req], [expr.typeid], [expr.sizeof], [expr.unary.noexcept], [[expr.reflect],]{.addu} [dcl.type.decltype], [temp.pre], [temp.concept]). An unevaluated operand is not evaluated.

:::

Add `$splice-expression$` to the list of expressions in paragraph 2.

::: std
[2]{.pnum} In some contexts, an expression only appears for its side effects. Such an expression is called a _discarded-value expression_. The array-to-pointer and function-to-pointer standard conversions are not applied. The lvalue-to-rvalue conversion is applied if and only if the expression is a glvalue of volatile-qualified type and it is one of the following:

- [#.#]{.pnum} `( $expression$ )`, where `$expression$` is one of these expressions,
- [#.#]{.pnum} `$id-expression$` ([expr.prim.id]),
- [[#.2+]{.pnum} `$splice-expression$` ([expr.prim.splice]),]{.addu}
- [#.3]{.pnum} [...]

:::

### [expr.prim]{.sref} Primary expressions {-}

Add `$splice-expression$` to the grammar for `$primary-expression$`:

::: std
```diff
  $primary-expression$:
     $literal$
     this
     ( $expression$ )
     $id-expression$
     $lambda-expression$
     $fold-expression$
     $requires-expression$
+    $splice-expression$
```
:::

### [expr.prim.id.general]{.sref} General {-}

Modify paragraph 2 to avoid transforming non-static members into implicit member accesses when named as operands to `$reflect-expression$`s.

::: std
[2]{.pnum} If an `$id-expression$` `$E$` denotes a non-static non-type member of some class `C` at a point where the current class ([expr.prim.this]) is `X` and

* [#.#]{.pnum} `$E$` is potentially evaluated or `C` is `X` or a base class of `X`, and
* [#.#]{.pnum} `$E$` is not the `$id-expression$` of a class member access expression ([expr.ref]), and
* [[#.2+]{.pnum} `$E$` is not the `$id-expression$` of a `$reflect-expression$` ([expr.reflect]), and]{.addu}
* [#.3]{.pnum} if `$E$` is a `$qualified-id$`, `$E$` is not the un-parenthesized operand of the unary `&` operator ([expr.unary.op]),

the `$id-expression$` is transformed into a class member access expression using `(*this)` as the object expression.
:::

And extend paragraph 4 to account for splices:

::: std
[4]{.pnum} An `$id-expression$` [or `$splice-expression$`]{.addu} that [denotes]{.rm} [designates]{.addu} a non-static data member or implicit object member function of a class can only be used:

* [#.#]{.pnum} as part of a class member access (after any implicit transformation (see above)) in which the object expression refers to the member's class or a class derived from that class, or
* [#.#]{.pnum} to form a pointer to member ([expr.unary.op]), or
* [#.#]{.pnum} if that `$id-expression$` [or `$splice-expression$` designates]{.addu} [denotes]{.rm} a non-static data member and it appears in an unevaluated operand.

::: example
```diff
  struct S {
    int m;
  };
  int i     = sizeof(S::m);       // OK
  int j     = sizeof(S::m + 42);  // OK
+ int S::*k = &[:^^S::m:];        // OK
```
:::
:::

### [expr.prim.id.unqual]{.sref} Unqualified names {-}

Modify paragraph 15 to allow `$splice-expression$`s to be move-eligible:

::: std
[15]{.pnum} An _implicitly movable entity_ is a variable with automatic storage duration that is either a non-volatile object or an rvalue reference to a non-volatile object type. An `$id-expression$` [or `$splice-expression$` ([expr.prim.splice])]{.addu} is _move-eligible_ if

- [#.#]{.pnum} it [names]{.rm} [designates]{.addu} an implicitly movable entity,
- [#.#]{.pnum} it is the (possibly parenthesized) operand of a `return` ([stmt.return]) or `co_return` ([stmt.return.coroutine]) statement or of a `$throw-expression$` ([expr.throw]), and
- [#.#]{.pnum} each intervening scope between the declaration of the entity and the innermost enclosing scope of the [`$id-expression$`]{.rm} [expression]{.addu} is a block scope and, for a `$throw-expression$`, is not the block scope of a `$try-block$` or `$function-try-block$`.

:::

### [expr.prim.id.qual]{.sref} Qualified names {-}

Extend the grammar for `$nested-name-specifier$` as follows:

::: std
```diff
  $nested-name-specifier$:
      ::
      $type-name$ ::
      $namespace-name$ ::
      $computed-type-specifier$ ::
+     $splice-scope-specifier$ ::
      $nested-name-specifier$ $identifier$ ::
      $nested-name-specifier$ template@~_opt_~@ $simple-template-id$ ::
+
+  $splice-scope-specifier$:
+     $splice-specifier$
+     template@~_opt_~@ $splice-specialization-specifier$
```
:::

Add a paragraph after paragraph 1 specifying the rules for parsing a `$splice-scope-specifier$`, as well as an example:

::: std
::: addu
[1+]{.pnum} A `$splice-specifier$` or `$splice-specialization-specifier$` that is not followed by `::` is never interpreted as part of a `$splice-scope-specifier$`. The keyword `template` may only be omitted from the form `template@~_opt_~@ $splice-specialization-specifier$ ::` when the `$splice-specialization-specifier$` is preceded by `typename`.

::: example
```cpp
template <int V>
struct TCls {
  static constexpr int s = V;
  using type = int;
};

int v1 = [:^^TCls<1>:]::s;
int v2 = template [:^^TCls:]<2>::s;
    // OK, template binds to splice-scope-specifier

typename [:^^TCls:]<3>::type v3 = 3;
    // OK, typename binds to the qualified name

template [:^^TCls:]<3>::type v4 = 4;
    // OK, template binds to the splice-scope-specifier

typename template [:^^TCls:]<3>::type v5 = 5;
    // OK, same as v3

[:^^TCls:]<3>::type v6 = 6;
    // error: unexpected <
```

:::
:::
:::

Clarify in paragraph 2 that a splice cannot appear in a declarative `$nested-name-specifier$`:

::: std
[2]{.pnum} A `$nested-name-specifier$` is _declarative_ if it is part of

* a `$class-head-name$`,
* an `$enum-head-name$`,
* a `$qualified-id$` that is the `$id-expression$` of a `$declarator-id$`, or
* a declarative `$nested-name-specifier$`.

A declarative `$nested-name-specifier$` shall not have a `$computed-type-specifier$` [or a `$splice-scope-specifier$`]{.addu}. A declaration that uses a declarative `$nested-name-specifier$` shall be a friend declaration or inhabit a scope that contains the entity being redeclared or specialized.
:::

Break the next paragraph into a bulleted list, extend it to also cover splices, and prefer the verb "designate" over "nominate":

[Here and in a few other places, the wording for the entity referred to by a `$splice-specialization-specifier$` is complicated. This is primarily because a `$splice-specialization-specifier$` whose `$splice-specifier$` designates a function template can have a partially deduced set of template arguments, such that the `$splice-specialization-specifier$` alone cannot designate an entity.]{.draftnote}

::: std
[3]{.pnum} [The entity designated by a `$nested-name-specifier$` is determined as follows:]{.addu}

  - [#.#]{.pnum} The `$nested-name-specifier$` `::` [nominates]{.rm} [designates]{.addu} the global namespace.[\ ]{.addu}
  - [#.#]{.pnum} A `$nested-name-specifier$` with a `$computed-type-specifier$` [nominates]{.rm} [designates]{.addu} the [same]{.addu} type [denoted]{.rm} [designated]{.addu} by the `$computed-type-specifier$`, which shall be a class or enumeration type.[\ ]{.addu}
  - [[#.#]{.pnum} For a `$nested-name-specifier$` of the form `$splice-specifier$ ::`, the `$splice-specifier$` shall designate a class or enumeration type or a namespace. The `$nested-name-specifier$` designates the same entity as the `$splice-specifier$`.]{.addu}
  - [[#.#]{.pnum} For a `$nested-name-specifier$` of the form `template@~_opt_~@ $splice-specialization-specifier$ ::`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a class template or an alias template `$T$`. Letting `$S$` be the specialization of `$T$` corresponding to the template argument list of the `$splice-specialization-specifier$`, `$S$` shall either be a class template specialization or an alias template specialization that denotes a class or enumeration type. The `$nested-name-specifier$` designates the underlying entity of `$S$`.]{.addu}
  - [#.#]{.pnum} If a `$nested-name-specifier$` _N_ is declarative and has a `$simple-template-id$` with a template argument list _A_ that involves a template parameter, let _T_ be the template nominated by _N_ without _A_. _T_ shall be a class template.
    - [#.#.#]{.pnum} If `$A$` is the template argument list ([temp.arg]) of the corresponding `$template-head$` `$H$` ([temp.mem]), `$N$` [nominates]{.rm} [designates]{.addu} the primary template of `$T$`; `$H$` shall be equivalent to the `$template-head$` of `$T$` ([temp.over.link]).
    - [#.#.#]{.pnum} Otherwise, `$N$` [nominates]{.rm} [designates]{.addu} the partial specialization ([temp.spec.partial]) of `$T$` whose template argument list is equivalent to `$A$` ([temp.over.link]); the program is ill-formed if no such partial specialization exists.

  - [#.#]{.pnum} Any other `$nested-name-specifier$` [nominates]{.rm} [designates]{.addu} the entity denoted by its `$type-name$`, `$namespace-name$`, `$identifier$`, or `$simple-template-id$`. If the `$nested-name-specifier$` is not declarative, the entity shall not be a template.

:::

### [expr.prim.lambda.closure]{.sref} Closure types {-}

We have to say that a closure type is not complete until the `}`:

::: std
[1]{.pnum} The type of a lambda-expression (which is also the type of the closure object) is a unique, unnamed non-union class type, called the closure type, whose properties are described below.

::: addu
[x]{.pnum} The closure type is not complete until the end of its corresponding `$compound-statement$`.
:::
:::

And say that the call operator is a direct member:

::: std
[4]{.pnum} The closure type for a *lambda-expression* has a public inline function call operator (for a non-generic lambda) or function call operator template (for a generic lambda) ([over.call]) whose parameters and return type are those of the lambda-expression's parameter-declaration-clause and trailing-return-type respectively,  and whose template-parameter-list consists of the specified template-parameter-list, if any. [The function call operator or the function call operator template are direct members of the closure type.]{.addu} The *requires-clause* of the function call operator template [...]
:::

Carve out an exception to the implicit definition of `__func__` in consteval blocks from paragraph 15:

::: std
[15]{.pnum} The `$lambda-expression$`'s `$compound-statement$` yields the `$function-body$` ([dcl.fct.def]) of the function call operator, but it is not within the scope of the closure type.

:::example10
```cpp
struct S1 {
  [...]
};
```
:::

[Further]{.rm} [Unless the `$compound-statement$` is that of a `$conteval-block-declaration$` ([dcl.pre])]{.addu}, a variable `__func__` is implicitly defined at the beginning of the `$compound-statement$` of the `$lambda-expression$`, with semantics as described in [dcl.fct.def.general].

:::

### [expr.prim.req.type]{.sref} Type requirements {-}

Allow splices in type requirements:

::: std
```diff
  $type-requirement$:
    typename $nested-name-specifier$@~_opt_~@ $type-name$ ;
+   typename $splice-specifier$
+   typename $splice-specialization-specifier$
```

[1]{.pnum} A `$type-requirement$` asserts the validity of a type. The component names of a `$type-requirement$` are those of its `$nested-name-specifier$` (if any) and `$type-name$` [(if any)]{.addu}.

::: example
```diff
template<typename  T, typename T::type = 0> struct S;
template<typename T> using Ref = T&;

template<typename T> concept C = requires {
  typename T::inner;        // required nested member name
  typename S<T>;            // required valid ([temp.names]) template-id;
                            // fails if T::type does not exist as a type to which 0 can be implicitly converted
  typename Ref<T>;          // required alias template substitution, fails if T is void
+ typename [:T::r1:];       // fails if T::r1 is not a reflection of a type
+ typename [:T::r2:]<int>;  // fails if T::r2 is not a reflection of a template Z for
+                           // which Z<int> is a type
};
```
:::
:::

### 7.5.8* [expr.prim.splice] Expression splicing {-}

Add a new subsection of [expr.prim]{.sref} following [expr.prim.req]{.sref}

::: std
::: addu
**Expression Splicing   [expr.prim.splice]**

```
$splice-expression$:
   $splice-specifier$
   template $splice-specifier$
   template $splice-specialization-specifier$
```

[#]{.pnum} A `$splice-specifier$` or `$splice-specialization-specifier$` immediately followed by `::` or preceded by `typename` is never interpreted as part of a `$splice-expression$`.

::: example
```cpp
struct S { static constexpr int a = 1; };
template <typename> struct TCls { static constexpr int b = 2; };

constexpr int c = [:^^S:]::a;                   // OK, [:^^S:] is not an expression

constexpr int d = template [:^^TCls:]<int>::b;  // OK, template [:^^TCls:]<int> is not
                                                // an expression

template <auto V> constexpr int e = [:V:];   // OK
constexpr int f = template [:^^e:]<^^S::a>;  // OK

constexpr auto g = typename [:^^int:](42);
  // OK, typename [:^^int:] is a splice-type-specifier

constexpr auto h = ^^g;
constexpr auto i = e<[:^^h:]>;
  // error: unparenthesized splice-expression used as template argument
constexpr auto j = e<([:^^h:])>;  // OK
```

:::

[#]{.pnum} For a `$splice-expression$` of the form `$splice-specifier$`, let `$S$` be the construct designated by `$splice-specifier$`.

* [#.#]{.pnum} The expression is ill-formed if `$S$` is
  * [#.#.#]{.pnum} a constructor,
  * [#.#.#]{.pnum} a destructor,
  * [#.#.#]{.pnum} an unnamed bit-field, or
  * [#.#.#]{.pnum} a local entity ([basic.pre]) such that
    * [#.#.#.#]{.pnum} there is a lambda scope that intervenes between the expression and the point at which `$S$` was introduced and
    * [#.#.#.#]{.pnum} the expression would be potentially evaluated if the effect of any enclosing `typeid` expressions ([expr.typeid]) were ignored.

* [#.#]{.pnum} Otherwise, if `$S$` is a function `$F$`, the expression denotes an overload set containing all declarations of `$F$` that precede either the expression or the point immediately following the `$class-specifier$` of the outermost class for which the expression is in a complete-class context; overload resolution is performed ([over.match], [over.over]).

* [#.#]{.pnum} Otherwise, if `$S$` is an object or a non-static data member, the expression is an lvalue designating `$S$`. The expression has the same type as that of `$S$`, and is a bit-field if and only if `$S$` is a bit-field. [The implicit transformation ([expr.prim.id]) whereby an `$id-expression$` denoting a non-static member becomes a class member access does not apply to a `$splice-expression$`.]{.note}

* [#.#]{.pnum} Otherwise, if `$S$` is a variable or a structured binding, `$S$` shall either have static or thread storage duration or shall inhabit a scope enclosing the expression. The expression is an lvalue referring to the object or function `$X$` associated with or referenced by `$S$`, has the same type as that of `$S$`, and is a bit-field if and only if `$X$` is a bit-field.

  [The type of a `$splice-expression$` designating a variable or structured binding of reference type will be adjusted to a non-reference type ([expr.type]).]{.note}

* [#.#]{.pnum} Otherwise, if `$S$` is a value or an enumerator, the expression is a prvalue that computes `$S$` and whose type is the same as that of `$S$`.

* [#.#]{.pnum} Otherwise, the expression is ill-formed.

[#]{.pnum} For a `$splice-expression$` of the form  `template $splice-specifier$`, the `$splice-specifier$` shall designate a function template `$T$` that is not a constructor template. The expression denotes an overload set containing all declarations of `$T$` that precede either the expression or the point immediately following the `$class-specifier$` of the outermost class for which the expression is in a complete-class context; overload resolution is performed. [During overload resolution, candidate function templates undergo template argument deduction and the resulting specializations are considered as candidate functions.]{.note}

[#]{.pnum} For a `$splice-expression$` of the form `template $splice-specialization-specifier$`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a template `$T$`.

* [#.#]{.pnum} If `$T$` is a function template, the expression denotes an overload set containing all declarations of `$T$` that precede either the expression or the point immediately following the `$class-specifier$` of the outermost class for which the expression is in a complete-class context; overload resolution is performed ([over.match], [over.over]).

* [#.#]{.pnum} Otherwise, if `$T$` is a variable template, let `$S$` be the specialization of `$T$` corresponding to the template argument list of the `$splice-specialization-specifier$`. The expression is an lvalue referring to the object associated with `$S$` and has the same type as that of `$S$`.

* [#.#]{.pnum} Otherwise, the expression is ill-formed.

[Class members are accessible from any point when designated by `$splice-expression$`s ([class.access.base]). A class member access expression whose right operand is a `$splice-expression$` is ill-formed if the left operand (considered as a pointer) cannot be implicitly converted to a pointer to the designating class of the right operand.]{.note}

:::
:::

### [expr.post.general]{.sref} General {-}

Add a production to `$postfix-expression$` for splices in member access expressions:

::: std
```diff
[1]{.pnum} Postfix expressions group left-to-right.
  $postfix-expression$:
    ...
    $postfix-expression$ . $template$@~_opt_~@ $id-expression$
+   $postfix-expression$ . $splice-expression$
    $postfix-expression$ -> $template$@~_opt_~@ $id-expression$
+   $postfix-expression$ -> $splice-expression$
```
:::

### [expr.ref]{.sref} Class member access {-}

Modify paragraph 1 to account for splices in member access expressions. Also modify the note that follows to make it clear it's not referring to a `$splice-expression$` of the form `template [:R:]`.

::: std
[1]{.pnum} A postfix expression followed by a dot `.` or an arrow `->`, optionally followed by the keyword `template`, and then followed by an `$id-expression$` [or a `$splice-expression$`]{.addu}, is a postfix expression.

[If the keyword `template` is used [and followed by an `$id-expression$`]{.addu}, the [following]{.rm} unqualified name is considered to refer to a template ([temp.names]). If a `$simple-template-id$` results and is followed by a `::`, the `$id-expression$` is a `$qualified-id$`.]{.note}

:::

Modify paragraph 2 to account for splices in member access expressions:

::: std
[2]{.pnum} For [the first option (dot), if the]{.rm} [a dot that is followed by an]{.addu} [`$id-expression$` names]{.rm} [expression that designates]{.addu} a static member or an enumerator, the first expression is a discarded-value expression ([expr.context]); if the [`$id-expression$` names]{.rm} [expression after the dot designates]{.addu} a non-static data member, the first expression shall be a glvalue. [For the second option (arrow), the first expression]{.rm} [A postfix expression that is followed by an arrow]{.addu} shall be a prvalue having pointer type. The expression `E1->E2` is converted to the equivalent form `(*(E1)).E2`; the remainder of [expr.ref] will address only [the first option (dot)]{.rm} [the form using a dot]{.addu}^49^.
:::

Modify paragraph 3 to account for splices in member access expressions:

::: std
[3]{.pnum} The postfix expression before the dot is evaluated;^50^ the result of that evaluation, together with the `$id-expression$` [or `$splice-expression$`]{.addu}, determines the result of the entire postfix expression.
:::

Modify paragraph 4 to account for splices in member access expressions:

::: std
[4]{.pnum} Abbreviating `$postfix-expression$`.`$id-expression$` [or `$postfix-expression$.$splice-expression$`]{.addu} as `E1.E2`, `E1` is called the `$object expression$`. [...]

:::

Adjust the language in paragraphs 6-9 to account for `$splice-expression$`s. Explicitly add a fallback to paragraph 7 that makes other cases ill-formed. Update the term "naming class" to "designated class" in paragraph 8.

::: std

::: addu
[*]{.pnum} If `E2` is a `$splice-expression$`, then `E2` shall designate a member of the type of `E1`.
:::

[6]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a bit-field, `E1.E2` is a bit-field. [...]

[#]{.pnum} If `E2` [designates an entity that]{.addu} is declared to have type "reference to `T`", then `E1.E2` is an lvalue of type `T`. [If]{.rm} [In that case, if]{.addu} `E2` [is]{.rm} [designates]{.addu} a static data member, `E1.E2` designates the object or function to which the reference is bound, otherwise `E1.E2` designates the object or function to which the corresponding reference member of `E1` is bound. Otherwise, one of the following rules applies.

* [#.#]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a static data member and the type of `E2` is `T`, then `E1.E2` is an lvalue; [...]
* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a non-static data member and the type of `E1` is "_cq1_ _vq1_ `X`", and the type of `E2` is "_cq2 vq2_ `T`", [...]. If [the entity designated by]{.addu} `E2` is declared to be a `mutable` member, then the type of `E1.E2` is "_vq12_ `T`". If [the entity designated by]{.addu} `E2` is not declared to be a `mutable` member, then the type of `E1.E2` is "_cq12_ _vq12_ `T`".

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [denotes]{.addu} an overload set, [...]

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a nested type, the expression `E1.E2` is ill-formed.

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a member enumerator and the type of `E2` is `T`, the expression `E1.E2` is a prvalue of type `T` whose value is the value of the enumerator.

* [[#.#]{.pnum} Otherwise, the program is ill-formed.]{.addu}

[#]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a non-static member [(possibly after overload resolution)]{.addu}, the program is ill-formed if the class of which `E2` [is directly]{.rm} [designates]{.addu} a [direct]{.addu} member is an ambiguous base ([class.member.lookup]) of the [naming]{.rm} [designating]{.addu} class ([class.access.base]) of `E2`.

[#]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a non-static member [(possibly after overload resolution)]{.addu} and the result of `E1` is an object whose type is not similar ([conv.qual]) to the type of `E1`, the behavior is undefined.

:::

### [expr.unary.general]{.sref} General {-}

Add `$reflect-expression$` to the grammar for `$unary-expression$` in paragraph 1:

::: std
[1]{.pnum} Expressions with unary operators group right-to-left.
```diff
  $unary-expression$:
     ...
     $delete-expression$
+    $reflect-expression$
```
:::

### [expr.unary.op]{.sref} Unary operators {-}

[The changes to paragraph 3.1 are part of the resolution to [@CWG3026].]{.ednote}

Modify paragraphs 3 and 4 to permit forming a pointer-to-member with a splice.

::: std
[3]{.pnum} The operand of the unary `&` operator shall be an lvalue of some type `T`.

* [#.#]{.pnum} If the operand is a `$qualified-id$` [or `$splice-expression$`]{.addu} [naming]{.rm} [designating]{.addu} a non-static [or variant]{.rm} member `m` [of some class `C`]{.rm}, other than an explicit object member function, [`m` shall be a direct member of some class `C` that is not an anonymous union.]{.addu} [the]{.rm} [The]{.addu} result has type "pointer to member of class `C` of type `T`" and designates `C::m`. [[A `$qualified-id$` that names a member of a namespace-scope anonymous union is considered to be a class member access expression ([expr.prim.id.general]) and cannot be used to form a pointer to member.]{.note}]{.addu}

* [#.#]{.pnum} Otherwise, the result has type "pointer to `T`" and points to the designated object ([intro.memory]) or function ([basic.compound]). If the operand designates an explicit object member function ([dcl.fct]), the operand shall be a `$qualified-id$` [or a `$splice-expression$`]{.addu}.

[4]{.pnum} A pointer to member is only formed when an explicit `&` is used and its operand is a `$qualified-id$` [or `$splice-expression$`]{.addu} not enclosed in parentheses.

:::

### 7.6.2.10* [expr.reflect] The reflection operator {-}

Add a new subsection of [expr.unary]{.sref} following [expr.delete]{.sref}

::: std
::: addu
**The reflection operator   [expr.reflect]**

```
$reflect-expression$:
   ^^ ::
   ^^ $reflection-name$
   ^^ $type-id$
   ^^ $id-expression$

$reflection-name$:
   $nested-name-specifier$@~_opt_~@ $identifier$
   $nested-name-specifier$ template $identifier$
```

[#]{.pnum} The unary `^^` operator, called the _reflection operator_, yields a prvalue of type `std::meta::info` ([basic.fundamental]).

[This document places no restriction on representing, by reflections, constructs not described by this document or using the names of such constructs as operands of `$reflect-expression$`s.]{.note}

[#]{.pnum} The component names of a `$reflection-name$` are those of its `$nested-name-specifier$` (if any) and its `$identifier$`. The terminal name of a `$reflection-name$` of the form `$nested-name-specifier$ template $identifier$` shall denote a template.

[#]{.pnum} A `$reflect-expression$` is parsed as the longest possible sequence of tokens that could syntactically form a `$reflect-expression$`. An unparenthesized `$reflect-expression$` that represents a template shall not be followed by `<`.

::: example
```cpp
static_assert(std::meta::is_type(^^int()));  // ^^ applies to the type-id "int()"

template<bool> struct X {};
consteval bool operator<(std::meta::info, X<false>) { return false; }
consteval void g(std::meta::info r, X<false> xv) {
  r == ^^int && true;    // error: ^^ applies to the type-id "int&&"
  r == ^^int & true;     // error: ^^ applies to the type-id "int&"
  r == (^^int) && true;  // OK
  r == ^^int &&&& true;  // error: 'int &&&&' is not a valid type
  ^^X < xv;              // error: reflect-expression that represents a template
                         // is followed by <
  (^^X) < xv;            // OK
  ^^X<true> < xv;        // OK
}
```
:::

[#]{.pnum} A `$reflect-expression$` of the form `^^ ::` represents the global namespace.

[#]{.pnum} If a `$reflect-expression$` `$R$` matches the form `^^ $reflection-name$`, it is interpreted as such; the `$identifier$` is looked up and the representation of `$R$` is determined as follows:

- [#.#]{.pnum} If lookup finds a declaration that replaced a `$using-declarator$` during a single search ([basic.lookup.general], [namespace.udecl]), `$R$` is ill-formed.

  :::example
  ```cpp
  struct A { struct S {}; };
  struct B : A { using A::S; };
  constexpr std::meta::info r1 = ^^B::S; // error: A::S found through using-declarator

  struct C : virtual B { struct S {}; };
  struct D : virtual B, C {};
  D::S s; // OK, names C::S per [class.member.lookup]
  constexpr std::meta::info r2 = ^^D::S; // OK, result C::S not found through using-declarator
  ```
  :::

- [#.#]{.pnum} Otherwise, if lookup finds a namespace alias ([namespace.alias]), `$R$` represents that namespace alias. For any other `$namespace-name$`, `$R$` represents the denoted namespace.
- [#.#]{.pnum} Otherwise, if lookup finds a namespace ([namespace.alias]), `$R$` represents that namespace.
- [#.#]{.pnum} Otherwise, if lookup finds a concept ([temp.concept]), `$R$` represents the denoted concept.
- [#.#]{.pnum} Otherwise, if lookup finds a template ([temp.names]), the representation of `$R$` is determined as follows:
  - [#.#.#]{.pnum} If lookup finds an injected-class-name ([class.pre]), then:
    - [#.#.#.#]{.pnum} If the `$reflection-name$` is of the form `$nested-name-specifier$ template $identifier$`, then `$R$` represents the class template named by the injected-class-name.
    - [#.#.#.#]{.pnum} Otherwise, the injected-class-name shall be unambiguous when considered as a `$type-name$` and `$R$` represents the class template specialization so named.
  - [#.#.#]{.pnum} Otherwise, if lookup finds an overload set, that overload set shall contain only declarations of a unique function template `$F$`; `$R$` represents `$F$`.
  - [#.#.#]{.pnum} Otherwise, if lookup finds a class template, variable template, or alias template, `$R$` represents that template. [Lookup never finds a partial or explicit specialization.]{.note}
- [#.#]{.pnum} Otherwise, if lookup finds a type alias `$A$`, `$R$` represents the underlying entity of `$A$` if `$A$` was introduced by the declaration of a template parameter; otherwise, `$R$` represents `$A$`.
- [#.#]{.pnum} Otherwise, if lookup finds a class or an enumeration, `$R$` represents the denoted type.
- [#.#]{.pnum} Otherwise, if lookup finds a class member of an anonymous union ([class.union.anon]), `$R$` represents that class member.
- [#.#]{.pnum} Otherwise, the `$reflection-name$` shall be an `$id-expression$` `$I$` and `$R$` is `^^ $I$` (see below).

[#]{.pnum} A `$reflect-expression$` `$R$` of the form `^^ $type-id$` represents an entity determined as follows:

- [#.#]{.pnum} If the `$type-id$` designates a placeholder type ([dcl.spec.auto.general]), `$R$` is ill-formed.
- [#.#]{.pnum} Otherwise, if the `$type-id$` names a type alias that is a specialization of an alias template ([temp.alias]), `$R$` represents that type alias.
- [#.#]{.pnum} Otherwise, `$R$` represents the type denoted by the `$type-id$`.

[#]{.pnum} A `$reflect-expression$` `$R$` of the form `^^ $id-expression$` represents an entity determined as follows:

  - [#.#]{.pnum} If the `$id-expression$` denotes
    - [#.#.#]{.pnum} a variable declared by an `$init-capture$` ([expr.prim.lambda.capture]),
    - [#.#.#]{.pnum} a function-local predefined variable ([dcl.fct.def.general]),
    - [#.#.#]{.pnum} a local parameter introduced by a `$requires-expression$` ([expr.prim.req]), or
    - [#.#.#]{.pnum} a local entity `$E$` ([basic.pre]) for which a lambda scope intervenes between the point at which `$E$` was introduced and `$R$`,

    then `$R$` is ill-formed.
  - [#.#]{.pnum} Otherwise, if the `$id-expression$` denotes an overload set `$S$`, overload resolution for the expression `&$S$` with no target shall select a unique function ([over.over]); `$R$` represents that function.
  - [#.#]{.pnum} Otherwise, if the `$id-expression$` denotes a variable, structured binding, enumerator, or non-static data member, `$R$` represents that entity.
  - [#.#]{.pnum} Otherwise, `$R$` is ill-formed. [This includes `$unqualified-id$`s that name a constant template parameter and `$pack-index-expression$`s.]{.note}

  The `$id-expression$` of a `$reflect-expression$` is an unevaluated operand ([expr.context]).

::: example
```cpp
template <typename T> void fn() requires (^^T != ^^int);
template <typename T> void fn() requires (^^T == ^^int);
template <typename T> void fn() requires (sizeof(T) == sizeof(int));

constexpr std::meta::info a = ^^fn<char>;     // OK
constexpr std::meta::info b = ^^fn<int>;      // error: ambiguous

constexpr std::meta::info c = ^^std::vector;  // OK

template <typename T>
struct S {
  static constexpr std::meta::info r = ^^T;
  using type = T;
};
static_assert(S<int>::r == ^^int);
static_assert(^^S<int>::type != ^^int);

typedef struct X {} Y;
typedef struct Z {} Z;
constexpr std::meta::info e = ^^Y;  // OK, represents the type alias Y
constexpr std::meta::info f = ^^Z;
  // OK, represents the type alias Z, not the type ([basic.lookup.general])
```
:::

:::

:::

### [expr.eq]{.sref} Equality Operators {-}

Add a new paragraph between paragraphs 5 and 6:

::: std
[5]{.pnum} Two operands of type `std::nullptr_t` or one operand of type `std::nullptr_t` and the other a null pointer constant compare equal.

::: addu
[5+]{.pnum} If both operands are of type `std::meta::info`, they compare equal if both operands

* [5+.#]{.pnum} are null reflection values,
* [5+.#]{.pnum} represent values that are template-argument-equivalent ([temp.type]),
* [5+.#]{.pnum} represent the same object,
* [5+.#]{.pnum} represent the same entity,
* [5+.#]{.pnum} represent the same direct base class relationship, or
* [5+.#]{.pnum} represent equal data member descriptions ([class.mem.general]),

and they compare unequal otherwise.
:::

[6]{.pnum} If two operands compare equal, the result is `true` for the `==` operator and `false` for the `!=` operator. If two operands compare unequal, the result is `false` for the `==` operator and `true` for the `!=` operator. Otherwise, the result of each of the operators is unspecified.
:::


### [expr.const]{.sref} Constant Expressions {-}

Add a bullet to paragraph 10 between 10.27 and 10.28 to disallow the production of injected declarations from any core constant expression that isn't a consteval block.

::: std
[10]{.pnum} An expression `$E$` is a _core constant expression_ unless the evaluation of `$E$`, following the rules of the abstract machine ([intro.execution]), would evaluate one of the following:

[...]

- [#.27]{.pnum} a `dynamic_cast` ([expr.dynamic.cast]) expression, `typeid` ([expr.typeid]) expression, or `$new-expression$` ([expr.new]) that would throw an exception where no definition of the exception type is reachable;
- [[#.27+]{.pnum} an expression that would produce an injected declaration (see below), unless `$E$` is the corresponding expression of a `$consteval-block-declaration$` ([dcl.pre]);]{.addu}
- [#.28]{.pnum} an `$asm-declaration$` ([dcl.asm]);
- [#.#]{.pnum} [...]

:::

Modify paragraph 17 to mention `$splice-expression$`s:

::: std
[17]{.pnum} During the evaluation of an expression `$E$` as a core constant expression, all `$id-expression$`s[, `$splice-expression$`s,]{.addu} and uses of `*this` that refer to an object or reference whose lifetime did not begin with the evaluation of `$E$` are treated as referring to a specific instance of that object or reference whose lifetime and that of all subobjects (including all union members) includes the entire constant evaluation. [...]

:::

Modify paragraph 22 to disallow returning non-consteval-only pointers and references to consteval-only objects from constant expressions.

::: std
[22]{.pnum} A _constant expression_ is either

- [#.#]{.pnum} a glvalue core constant expression [`$E$`]{.addu} [that]{.rm} [for which]{.addu}

  - [#.#.#]{.pnum} [`$E$`]{.addu} refers to [an object or]{.rm} a non-immediate function [or]{.addu}
  - [[#.#.#]{.pnum} `$E$` designates an object `$o$`, and if the complete object of `$o$` is of consteval-only type then so is `$E$`,]{.addu}

  ::: addu
  ::: example
  ```cpp
  struct Base { };
  struct Derived : Base { std::meta::info r; };

  consteval const Base& fn(const Derived& derived) { return derived; }

  constexpr Derived obj{.r=^^::}; // OK
  constexpr const Derived& d = obj; // OK
  constexpr const Base& b = fn(obj); // error: not a constant expression
    // because Derived is a consteval-only type but Base is not.
  ```

  :::
  :::

  or

- [#.#]{.pnum} a prvalue core constant expression whose result object ([basic.lval]) satisfies the following constraints:

  * [#.#.#]{.pnum} each constituent reference refers to an object or a non-immediate function,
  * [#.#.#]{.pnum} no constituent value of scalar type is an indeterminate value or erroneous value ([basic.indet]),
  * [#.#.#]{.pnum} no constituent value of pointer type is a pointer to an immediate function or an invalid pointer value ([basic.compound]), [and]{.rm}
  * [#.#.#]{.pnum} no constituent value of pointer-to-member type designates an immediate function[.]{.rm}[, and]{.addu}

  - [[#.#.#]{.pnum} unless the value is of consteval-only type,]{.addu}

    - [[#.#.#.#]{.pnum} no constituent value of pointer-to-member type points to a direct member of a consteval-only class type]{.addu}
    - [[#.#.#.#]{.pnum} no constituent value of pointer type points to or past an object whose complete object is of consteval-only type, and]{.addu}
    - [[#.#.#.#]{.pnum} no constituent reference refers to an object whose complete object is of consteval-only type.]{.addu}

:::

Modify (and clean up) the definition of _immediate-escalating expression_ in paragraph 25 to also apply to expressions of consteval-only type.

::: std
[25]{.pnum} A[n]{.rm} [potentially-evaluated]{.addu} expression or conversion is _immediate-escalating_ if it is [not]{.rm} [neither]{.addu} initially in an immediate function context [nor a subexpression of an immediate invocation,]{.addu} and [it is either]{.rm}

* [#.#]{.pnum} [a potentially-evaluated]{.rm} [it is an]{.addu} `$id-expression$` [or `$splice-expression$`]{.addu} that [denotes]{.rm} [designates]{.addu} an immediate function[,]{.addu} [that is not a subexpression of an immediate invocation, or]{.rm}
* [#.#]{.pnum} [it is]{.addu} an immediate invocation that is not a constant expression[, or]{.addu} [and is not a subexpression of an immediate invocation.]{.rm}
* [[#.#]{.pnum} [it is]{.addu} of consteval-only type ([basic.types.general]).]{.addu}

:::

Extend the definition of _immediate function_ in paragraph 27 to include functions containing a declaration of a variable of consteval-only type.

::: std
[27]{.pnum} An _immediate function_ is a function or constructor that is [either]{.addu}

* [#.#]{.pnum} declared with the `consteval` specifier, or
* [#.#]{.pnum} an immediate-escalating function `$F$` whose function body contains [either]{.addu}
  * [#.#.#]{.pnum} an immediate-escalating expression [`$E$`]{.rm} [or]{.addu}
  * [[#.#.#]{.pnum} a definition of a non-constexpr variable with consteval-only type]{.addu}

  [such that `$E$`'s]{.rm} [whose]{.addu} innermost enclosing non-block scope is `$F$`'s function parameter scope.
:::

After the example following the definition of manifestly constant-evaluated, introduce new terminology and rules for injecting declarations and renumber accordingly:

::: std
[28]{.pnum} An expression or conversion is _manifestly constant-evaluated_ if it is [...]

[Except for a `$static_assert-message$`, a manifestly constant-evaluated expression is evaluated even in an unevaluated operand ([expr.context]).]{.note}

::: addu
[#]{.pnum} The evaluation of an expression can introduce one or more _injected declarations_. The evaluation is said to _produce_ the declarations.

[An invocation of the library function template `std::meta::define_aggregate` produces an injected declaration ([meta.reflection.define.aggregate]).]{.note}

Each such declaration has

* [#.#]{.pnum} an associated _synthesized point_, which follows the last non-synthesized program point in the translation unit containing that declaration, and
* [#.#]{.pnum} an associated _characteristic sequence_ of values.

[Special rules concerning reachability apply to synthesized points ([module.reach]).]{.note13}

[The program is ill-formed if injected declarations with different characteristic sequences define the same entity in different translation units ([basic.def.odr]).]{.note}

[#]{.pnum} A member of an entity defined by an injected declaration shall not have a name reserved to the implementation ([lex.name]); no diagnostic is required.

[#]{.pnum} Let `$C$` be a `$consteval-block-declaration$`, the evaluation of whose corresponding expression produces an injected declaration for an entity `$E$`. The program is ill-formed if either

- [#.#]{.pnum} `$C$` is enclosed by a scope associated with `$E$` or
- [#.#]{.pnum} letting `$P$` be a point whose immediate scope is that to which `$E$` belongs, there is a function parameter scope or class scope that encloses exactly one of `$C$` or `$P$`.

::: example
```cpp
struct S0 {
  consteval {
    std::meta::define_aggregate(^^S0, {});
      // error: scope associated with S0 encloses the consteval block
  }
};

struct S1;
consteval { std::meta::define_aggregate(^^S1, {}); }  // OK

template <std::meta::info R> consteval void tfn1() {
  std::meta::define_aggregate(R, {});
}

struct S2;
consteval { tfn1<^^S2>(); }  // OK

template <std::meta::info R> consteval void tfn2() {
  consteval { std::meta::define_aggregate(R, {}); }
}

struct S3;
consteval { tfn2<^^S3>(); }
  // error: function parameter scope of tfn2<^^S3> intervenes between the declaration of S3
  // and the consteval block that produces the injected declaration

template <typename> struct TCls {
  struct S4;
  static void sfn() requires ([] {
    consteval { std::meta::define_aggregate(^^S4, {}); }
    return true;
  }()) { }
};

consteval { TCls<void>::sfn(); }
  // error: TCls<void>::S4 is not enclosed by requires-clause lambda

struct S5;
struct Cls {
  consteval { std::meta::define_aggregate(^^S5, {}); }
    // error: S5 is not enclosed by class Cls
};

struct S6;
consteval { // #1
  struct S7; // local class
  std::meta::define_aggregate(^^S7, {});
      // error: consteval block #1 doesn't enclose itself, but encloses S7
  consteval { // #2
    std::meta::define_aggregate(^^S6, {});
      // error: consteval block #1 encloses consteval block #2 but not S6
    std::meta::define_aggregate(^^S7, {});  // OK, consteval block #1 encloses both #2 and S7
  }
}
```
:::

[#]{.pnum} The _evaluation context_ is a set of program points that determines the behavior of certain functions used for reflection ([meta.reflection]). During the evaluation `$V$` of an expression `$E$` as a core constant expression, the evaluation context of an evaluation `$X$` ([intro.execution]) consists of the following points:

  - [#.#]{.pnum} The program point `$EVAL-PT$($L$)`, where `$L$` is the point at which `$E$` appears, and where `$EVAL-PT$($P$)`, for a point `$P$`, is a point `$R$` determined as follows:
    - [#.#.#]{.pnum} If a potentially-evaluated subexpression ([intro.execution]) of a default member initializer `$I$` appears at `$P$`, and a (possibly aggregate) initialization during `$V$` is using `$I$`, then `$R$` is `$EVAL-PT$($Q$)` where `$Q$` is the point at which that initialization appears.
    - [#.#.#]{.pnum} Otherwise, if a potentially-evaluated subexpression of a default argument ([dcl.fct.default]) appears at `$P$`, and an invocation of a function ([expr.call]) during `$V$` is using that default argument, then `$R$` is `$EVAL-PT$($Q$)` where `$Q$` is the point at which that invocation appears.
    - [#.#]{.pnum} Otherwise, `$R$` is `$P$`.
  - [#.#]{.pnum} Each synthesized point corresponding to an injected declaration produced by any evaluation sequenced before `$X$` ([intro.execution]).
:::
:::

### [dcl.pre]{.sref} Preamble {-}

Introduce the non-terminal `$consteval-block-declaration$` in paragraph 9.1:

::: std
```diff
 block-declaration:
    simple-declaration
    asm-declaration
    namespace-alias-definition
    using-declaration
    using-enum-declaration
    using-directive
    static_assert-declaration
+   consteval-block-declaration
    alias-declaration
    opaque-enum-declaration

[...]

  $static_assert-declaration$:
    static_assert ( $constant-expression$ ) ;
    static_assert ( $constant-expression$ , $static_assert-message$ ) ;

+ $consteval-block-declaration$:
+   consteval $compound-statement$
```
:::

Strike the assertion that a `$typedef-name$` is synonymous with its associated type from paragraph 8 (type aliases are entities now).

::: std
[8]{.pnum} If the `$decl-specifier-seq$` contains the `typedef` specifier, the declaration is a _typedef declaration_ and each `$declarator-id$` is declared to be a `$typedef-name$`[, synonymous with its associated type]{.rm} ([dcl.typedef]).

:::

Insert the following after paragraph 14 in relation to consteval blocks:

::: std
[14]{.pnum} *Recommended practice*: When a `$static_assert-declaration$` fails, [...]

::: addu
[*]{.pnum} For a `$consteval-block-declaration$` `$D$`, the expression `$E$` corresponding to `$D$` is:

```cpp
[] -> void static consteval $compound-statement$ ()
```

`$E$` shall be a constant expression ([expr.const]).

[The evaluation of the expression corresponding to a `$consteval-block-declaration$` ([lex.phases]) can produce injected declarations as side effects.]{.note}

::: example
```cpp
struct S;
consteval {
  std::meta::define_aggregate(^^S, {}); // OK

  template <class T>
  struct X { }; // error: local templates are not allowed

  template <class T>
  concept C = true; // error: local concepts are not allowed

  return; // OK
}
```
:::
:::

[15]{.pnum} An `$empty-declaration$` has no effect.

:::

### [dcl.typedef]{.sref} The `typedef` specifier {-}

Modify paragraphs 1-2 to clarify that the `typedef` specifier now introduces an entity.

::: std
[1]{.pnum} Declarations containing the `$decl-specifier$` `typedef` declare [identifiers that can be used later for naming fundamental ([basic.fundamental]) or compound ([basic.compound]) types]{.rm} [_type aliases_]{.addu}. The `typedef` specifier shall not be combined in a `$decl-specifier-seq$` with any other kind of specifier except a `$defining-type-specifier$`, and it shall not be used in the `$decl-specifier-seq$` of a `$parameter-declaration$` ([dcl.fct]) nor in the `$decl-specifier-seq$` of a `$function-definition$` ([dcl.fct.def]). If a `$typedef-specifier$` appears in a declaration without a `$declarator$`, the program is ill-formed.

```
  $typedef-name$:
      $identifier$
      $simple-template-id$
```

A name declared with the `typedef` specifier becomes a `$typedef-name$`. [A `$typedef-name$` names]{.rm} [The underlying entity of the type alias is]{.addu} the type associated with the `$identifier$` ([dcl.decl]) or `$simple-template-id$` ([temp.pre])[; a `$typedef-name$` is thus a synonym for another type]{.rm}. A `$typedef-name$` does not introduce a new type the way a class declaration ([class.name]) or enum declaration ([dcl.enum]) does.

[2]{.pnum} A [`$typedef-name$`]{.rm} [type alias]{.addu} can also be [introduced]{.rm} [declared]{.addu} by an `$alias-declaration$`. The `$identifier$` following the `using` keyword is not looked up; it becomes [a]{.rm} [the]{.addu} `$typedef-name$` [of a type alias]{.addu} and the optional `$attribute-specifier-seq$` following the `$identifier$` appertains to that [`$typedef-name$`]{.rm} [type alias]{.addu}. Such a [`$typedef-name$`]{.rm} [type alias]{.addu} has the same semantics as if it were introduced by the `typedef` specifier. [In particular, it does not define a new type.]{.rm}

:::

### [dcl.type.simple]{.sref} Simple type specifiers {-}

Extend the grammar for `$computed-type-specifier$` as follows:

::: std
```diff
  $computed-type-specifier$:
      $decltype-specifier$
      $pack-index-specifier$
+     $splice-type-specifier$
```
:::

Extend the definition of "placeholder for a deduced class type" in p3 to accommodate `$splice-type-specifier$`s.

::: std
[3]{.pnum} A `$placeholder-type-specifier$` is a placeholder for a type to be deduced ([dcl.spec.auto]). A `$type-specifier$` [of the form `typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ $template-name$`]{.rm} is a placeholder for a deduced class type ([dcl.type.class.deduct]) [if either\ ]{.addu}

- [[#.#]{.pnum} it is of the form `typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ $template-name$` or]{.addu}
- [[#.#]{.pnum} it is of the form `typename@~_opt_~@ $splice-specifier$` and the `$splice-specifier$` designates a class template or alias template.]{.addu}

The `$nested-name-specifier$` [or `$splice-specifier$`]{.addu}, if any, shall be non-dependent and the `$template-name$` [or `$splice-specifier$`]{.addu} shall [name]{.rm} [designate]{.addu} a deducible template. A _deducible template_ is either a class template or is an alias template whose `$defining-type-id$` is of the form

```cpp
typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ template@~_opt_~@ $simple-template-id$
```

where the `$nested-name-specifier$` (if any) is non-dependent and the `$template-name$` of the `$simple-template-id$` names a deducible template.

:::

Add a row to [tab:dcl.type.simple] to cover the `$splice-type-specifier$` production.

::: std
<center>Table 17: `$simple-type-specifier$`s and the types they specify [tab:dcl.type.simple]</center>
|Specifier(s)|Type|
|:-|:-|
|`$type-name$`|the type named|
|`$simple-template-id$`|the type as defined in [temp.names]|
|`$decltype-specifier$`|the type as defined in [dcl.type.decltype]|
|`$pack-index-specifier$`|the type as defined in [dcl.type.pack.index]|
|`$placeholder-type-specifier$`|the type as defined in [dcl.spec.auto]|
|`$template-name$`|the type as defined in [dcl.type.class.deduct]|
|[`$splice-type-specifier$`]{.addu}|[the type as defined in [dcl.type.splice]]{.addu}|
|`...`|...|
:::

### [dcl.type.decltype]{.sref} Decltype specifiers {-}

Add a bullet after bullet 1.3 to apply to `$splice-expression$`s, and extend the example that follows the paragraph:

::: std
[1]{.pnum} For an expression `$E$`, the type denoted by `decltype($E$)` is defined as follows:

[...]

- [1.3]{.pnum} otherwise, if `$E$` is an unparenthesized `$id-expression$` or an unparenthesized class member access ([expr.ref]), `decltype($E$)` is the type of the entity named by `$E$`. If there is no such entity, the program is ill-formed;
- [[1.3+]{.pnum} otherwise, if `$E$` is an unparenthesized `$splice-expression$`, `decltype($E$)` is the type of the entity, object, or value designated by the `$splice-specifier$` of `$E$`;]{.addu}

[...]


The operand of the `decltype` specifier is an unevaluated operand.

::: example
```cpp
const int && foo();
int i;
struct A {double x; };
const A* a = new A();
decltype(foo()) x1 = 17;       // type is const int&&
decltype(i) x2;                // type is int
decltype(a->x) x3;             // type is double
decltype((a->x)) x4 = x3;      // type is const double&
@[`decltype([:^^x1:]) x5 = 18;    // type is const int&&`]{.addu}@
@[`decltype(([:^^x1:])) x6 = 19;  // type is const int&`]{.addu}@

void f() {
  [](auto ...pack) {
    decltype(pack...[0]) @[x5]{.rm} [x7]{.addu}@;    // type is int
    decltype((pack...[0])) @[x6]{.rm} [x8]{.addu}@;  // type is int&
  }
}
```
:::

:::

### 9.2.9.8+ [dcl.type.splice] Type splicing {-}

Add a new subsection of ([dcl.type]) following ([dcl.type.class.deduct]).

::: std
::: addu
**Type Splicing   [dcl.type.splice]**

```
$splice-type-specifier$:
   typename@~_opt_~@ $splice-specifier$
   typename@~_opt_~@ $splice-specialization-specifier$
```

[#]{.pnum} A `$splice-specifier$` or `$splice-specialization-specifier$` immediately followed by `::` is never interpreted as part of a `$splice-type-specifier$`. A `$splice-specifier$` or `$splice-specialization-specifier$` not preceded by `typename` is only interpreted as a `$splice-type-specifier$` within a type-only context ([temp.res.general]).

::: example
```cpp
template <std::meta::info R> void tfn() {
  typename [:R:]::type m;  // OK, typename applies to the qualified name
}

struct S { using type = int; };
void fn() {
  [:^^S::type:] *var;           // error: [:^^S::type:] is an expression
  typename [:^^S::type:] *var;  // OK, declares variable with type int*
}

using alias = [:^^S::type:];    // OK, type-only context
```
:::

[#]{.pnum} For a `$splice-type-specifier$` of the form `typename@~_opt_~@ $splice-specifier$`, the `$splice-specifier$` shall designate a type, a class template, or an alias template. The `$splice-type-specifier$` designates the same entity as the `$splice-specifier$`.

[#]{.pnum} For a `$splice-type-specifier$` of the form `typename@~_opt_~@ $splice-specialization-specifier$`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a template `$T$` that is either a class template or an alias template. The `$splice-type-specifier$` designates the specialization of `$T$` corresponding to the template argument list of the `$splice-specialization-specifier$`.

:::
:::

### [dcl.mptr]{.sref} Pointers to members {-}

Prefer "designates", and disallow pointers to anonymous unions, in paragraph 2.

::: std
[2]{.pnum} In a declaration `T D` where `D` has the form

```
$nested-name-specifier$ * $attribute-specifier-seq$@~_opt_~@ $cv-qualifier-seq$@~_opt_~@ D1
```

and the `$nested-name-specifier$` [denotes]{.rm} [designates]{.addu} a class, and the type of the contained `$declarator-id$` in the declaration `T D1` is "_derived-declarator-type-list_ `T`", the type of the `$declarator-id$` in `D` is "_derived-declarator-type-list_ `$cv-qualifier-seq$` pointer to member of class `$nested-name-specifier$` of type `T`". The optional `$attribute-specifier-seq$` ([dcl.attr.grammar]) appertains to the pointer-to-member. [The `$nested-name-specifier$` shall not designate an anonymous union.]{.addu}

:::

### [dcl.array]{.sref} Arrays {-}

[This change is part of the resolution to [@CWG2701].]{.ednote}

Use "host scope" in lieu of "inhabits" in paragraph 8 and add a pertinent example to example 3 which follows:

::: std
[8]{.pnum} Furthermore, if there is a reachable declaration of the entity [that specifies a bound and has]{.addu} [that inhabits]{.rm} the same [host]{.addu} scope [([basic.scope.scope])]{.addu} [in which the bound was specified]{.rm}, an omitted array bound is taken to be the same as in that earlier declaration, and similarly for the definition of a static data member of a class.

::: example3
```cpp
extern int x[10];
struct S {
  static int y[10];
};

int x[];              // OK, bound is 10
int S::y[];           // OK, bound is 10

void f() {
  extern int x[];
  int i = sizeof(x);  // error: incomplete object type
}

@[`namespace A { extern int z[3]; }`]{.addu}@
@[`int A::z[] = {};   // OK, defines an array of 3 elements`]{.addu}@
```
:::
:::

### [dcl.fct]{.sref} Functions {-}

Use "denoted by" instead of "named by" in paragraph 9 to be more clear about the entity being referred to, and add a bullet to allow for reflections of abominable function types:

::: std
[9]{.pnum} A function type with a `$cv-qualifier-seq$` or a `$ref-qualifier$` (including a type [named]{.rm} [denoted]{.addu} by `$typedef-name$` ([dcl.typedef], [temp.param])) shall appear only as:

* [#.#]{.pnum} the function type for a non-static member function,
* [#.#]{.pnum} the function type to which a pointer to member refers,
* [#.#]{.pnum} the top-level function type of a function typedef declaration or `$alias-declaration$`,
* [#.#]{.pnum} the `$type-id$` in the default argument of a `$type-parameter$` ([temp.param]),
* [#.#]{.pnum} the `$type-id$` of a `$template-argument$` for a `$type-parameter$` ([temp.arg.type])[.]{.rm}[, or]{.addu}

::: addu
* [9.6]{.pnum} the operand of a `$reflect-expression$` ([expr.reflect]).
:::

:::

Extend the example that follows to demonstrate taking the reflection of an abominable function type:

::: std
::: example4
```cpp
typedef int FIC(int) const;
FIC f;                                          // error: does not declare a member function
struct S {
  FIC f;                                        // OK
};
FIC S::*pm = &S::f;                             // OK
@[`constexpr std::meta::info yeti = ^^void(int) const &; // OK`]{.addu}@

```
:::
:::

### [dcl.fct.default]{.sref} Default arguments {-}

[The changes related to "host scopes" in paragraphs 4 and 9 are part of the resolution to [@CWG2701].]{.ednote}

Use "host scope" in lieu of "inhabits" in paragraph 4:

::: std
[4]{.pnum} For non-template functions, default arguments can be added in later declarations of a function that [inhabit]{.rm} [have]{.addu} the same [host]{.addu} scope. Declarations that [inhabit]{.rm} [have]{.addu} different [host]{.addu} scopes have completely distinct sets of default arguments. [...]

:::

Modify paragraph 9 to allow reflections of non-static data members to appear in default function arguments, extend example 8 which follows, and use "host scope" rather than "inhabits" following example 9. Break the list of exemptions in paragraph 9 into bullets for better readability.

::: std
[9]{.pnum} A default argument is evaluated each time the function is called with no argument for the corresponding parameter.

[...]

A non-static member shall not [appear]{.rm} [be designated]{.addu} in a default argument unless[\ ]{.addu}

- [#.#]{.pnum} it [appears as]{.rm} [is designated by]{.addu} the `$id-expression$` [or `$splice-expression$`]{.addu} of a class member access expression ([expr.ref]), [or unless]{.rm}
- [#.#]{.pnum} it is [designated by an expression]{.addu} used to form a pointer to member ([expr.unary.op])[, or]{.addu}
- [[#.#]{.pnum} it appears as the operand of a `$reflect-expression$` ([expr.reflect])]{.addu}.

::: example8
The declaration of `X::mem1()` in the following example is ill-formed because no object is supplied for the non-static member `X::a` used as an initializer.

```cpp
int b;
class X {
  int a;
  int mem1(int i = a);    // error: non-static member a used as default argument
  int mem2(int i = b);    // OK; use X::b
  @[`consteval void mem3(std::meta::info r = ^^a) {}    // OK`]{.addu}@
  @[`int mem4(int i = [:^^a:]); // error: non-static member a designated in default argument`]{.addu}@

  static int b;
};
```

[...]

[ [*Note 1*:]{.addu} When an overload set contains a declaration of a function [that inhabits a]{.rm} [whose host]{.addu} scope [is]{.addu} `$S$`, any default argument associated with any reachable declaration [that inhabits]{.rm} [whose host scope is]{.addu} `$S$` is available to the call [([over.match.viable])]{.addu}. [*—end note*]{.addu} ]

[The candidate might have been found through a `$using-declarator$` from which the declaration that provides the default argument is not reachable.]{.note}

:::
:::

### [dcl.init.general]{.sref} Initializers (General) {-}

Change paragraphs 6-8 of [dcl.init.general] [No changes are necessary for value-initialization, which already forwards to zero-initialization for scalar types]{.ednote}:

::: std
[6]{.pnum} To *zero-initialize* an object or reference of type `T` means:

* [6.0]{.pnum} [if `T` is `std::meta::info`, the object is initialized to a null reflection value;]{.addu}
* [6.1]{.pnum} if `T` is [a]{.rm} [any other]{.addu} scalar type ([basic.types.general]), the object is initialized to the value obtained by converting the integer literal `0` (zero) to `T`;
* [6.2]{.pnum} [...]

[7]{.pnum} To *default-initialize* an object of type `T` means:

* [7.1]{.pnum} If `T` is a (possibly cv-qualified) class type ([class]), [...]
* [7.2]{.pnum} If T is an array type, [...]
* [7.*]{.pnum} [If `T` is `std::meta::info`, the object is zero-initialized.]{.addu}
* [7.3]{.pnum} Otherwise, no initialization is performed.

[8]{.pnum} A class type `T` is *const-default-constructible* if default-initialization of `T` would invoke a user-provided constructor of `T` (not inherited from a base class) or if

* [8.1]{.pnum} [...]

If a program calls for the default-initialization of an object of a const-qualified type `T`, `T` shall be [`std::meta::info` or]{.addu} a const-default-constructible class type, or array thereof.

[9]{.pnum} To value-initialize an object of type T means: [...]
:::

### [dcl.fct.def.delete]{.sref} Deleted definitions {-}

Change paragraph 2 of [dcl.fct.def.delete]{.sref} to allow for reflections of deleted functions:

::: std

[2]{.pnum} A [program that refers to]{.rm} [construct that designates]{.addu} a deleted function implicitly or explicitly, other than to declare it [or to appear as the operand of a `$reflect-expression$` ([expr.reflect])]{.addu}, is ill-formed.
:::

### [enum.udecl]{.sref} The `using enum` declaration {-}

Extend the grammar for `$using-enum-declarator$` as follows:

::: std
```diff
  $using-enum-declaration$:
     using enum $using-enum-declarator$ ;

  $using-enum-declarator$:
     $nested-name-specifier$@~_opt_~@ $identifier$
     $nested-name-specifier$@~_opt_~@ $simple-template-id$
+    $splice-type-specifier$
```
:::

Modify paragraph 1 to handle `$splice-type-specifier$`s:

::: std

[1]{.pnum} [A `$using-enum-declarator$` of the form `$splice-type-specifier$` designates the same type designated by the `$splice-type-specifier$`. Any other]{.addu} [A]{.rm} `$using-enum-declarator$` names the set of declarations found by type-only lookup ([basic.lookup.general]) for the `$using-enum-declarator$` ([basic.lookup.unqual], [basic.lookup.qual]). The `$using-enum-declarator$` shall designate a non-dependent type with a reachable `$enum-specifier$`.
:::

### [namespace.unnamed]{.sref} Unnamed namespaces {-}

Clarify that identifiers are unique for the entire program rather than the translation unit.

::: std
[1]{.pnum} An `$unnamed-namespace-definition$` behaves as if it were replaced by
```cpp
inline@~_opt_~@ namespace @_unique_@ { /* empty body */ }
using namespace @_unique_@ ;
namespace @_unique_@ { $namespace-body$ }
```

  where `inline` appears if and only if it appears in the `$unnamed-namespace-definition$` and all occurrences of _`unique`_ in a translation unit are replaced by the same identifier; and this identifier differs from all other identifiers in the [translation unit]{.rm} [program]{.addu}. [...]

:::

### [namespace.alias]{.sref} Namespace alias {-}

Modify the grammar for `$namespace-alias-definition$` in paragraph 1, and clarify that such declarations declare a "namespace alias" (which is now an entity as per [basic.pre]).

::: std
[1]{.pnum} A `$namespace-alias-definition$` declares [an alternative name for a namespace]{.rm} [a _namespace alias_]{.addu} according to the following grammar:

```diff
  $namespace-alias$:
      $identifier$

  $namespace-alias-definition$:
      namespace $identifier$ = $qualified-namespace-specifier$
+     namespace $identifier$ = $splice-specifier$

  $qualified-namespace-specifier$:
      $nested-name-specifier$@~_opt_~@ $namespace-name$
```

[The `$splice-specifier$` (if any) shall designate a namespace that is not the global namespace.]{.addu}
:::

Remove the details about what the `$namespace-alias$` denotes; this will fall out from the "underlying entity" of the namespace alias defined below:

::: std
[2]{.pnum} The `$identifier$` in a `$namespace-alias-definition$` becomes a `$namespace-alias$` [and denotes the namespace denoted by the `$qualified-namespace-specifier$`]{.rm}.

:::

Add the following paragraph after paragraph 2 and before the note:

::: std
::: addu
[2+]{.pnum} The underlying entity ([basic.pre]) of the namespace alias is the namespace either denoted by the `$qualified-namespace-specifier$` or designated by the `$splice-specifier$`.

:::
:::

### [namespace.udir]{.sref} Using namespace directive {-}

Add `$splice-specifier$` to the grammar for `$using-directive$`:

::: std
```diff
  $using-directive$:
      $attribute-specifier-seq$@~_opt_~@ using namespace $nested-name-specifier$@~_opt_~@ $namespace-name$
+     $attribute-specifier-seq$@~_opt_~@ using namespace $splice-specifier$
```
:::

Add the following prior to the first paragraph of [namespace.udir], and renumber accordingly:

::: std
::: addu
[0]{.pnum} The `$splice-specifier$` (if any) shall designate a namespace that is not the global namespace. The `$nested-name-specifier$`, `$namespace-name$`, and `$splice-specifier$` shall not be dependent.
:::

[1]{.pnum} A `$using-directive$` shall not appear in class scope, but may appear in namespace scope or in block scope.

[...]
:::

Prefer the verb "designate" rather than "nominate" in the notes that follow:

::: std
[A `$using-directive$` makes the names in the [nominated]{.rm} [designated]{.addu} namespace usable in the scope [...]. During unqualified name lookup, the names appear as if they were declared in the nearest enclosing namespace which contains both the `$using-directive$` and the [nominated]{.rm} [designated]{.addu} namespace.]{.note2}

[...]

[A `$using-directive$` is transitive: if a scope contains a `$using-directive$` that [nominates]{.rm} [designates]{.addu} a namespace that itself contains `$using-directive$`s, the namespaces [nominated]{.rm} [designated]{.addu} by those `$using-directive$`s are also eligible to be considered.]{.note4}
:::


### [dcl.attr.grammar]{.sref} Attribute syntax and semantics {-}

Update the grammar for balanced token as follows:

::: std
```diff
  $balanced-token$ :
      ( $balanced-token-seq$@~_opt_~@ )
      [ $balanced-token-seq$@~_opt_~@ ]
      { $balanced-token-seq$@~_opt_~@ }
-     any token other than a parenthesis, a bracket, or a brace
+     [: $balanced-token-seq$@~_opt_~@ :]
+     any token other than (, ), [, ], {, }, [:, or :]
```
:::

Change a sentence in paragraph 4 of [dcl.attr.grammar]{.sref} as follows:

::: std

[4]{.pnum} [...] An `$attribute-specifier$` that contains no `$attribute$`s [and no `$alignment-specifier$`]{.addu} has no effect. ...
:::

### [dcl.attr.deprecated]{.sref} Deprecated attribute {-}

Prefer "type alias" to "`$typedef-name$`" in paragraph 2.

::: std
[2]{.pnum} The attribute may be applied to the declaration of a class, a [`$typedef-name$`]{.rm} [type alias]{.addu}, a variable, a non-static data member, a function, a namespace, an enumeration, an enumerator, a concept, or a template specialization.

:::

### [dcl.attr.unused]{.sref} Maybe unused attribute {-}

Prefer "type alias" to "`$typedef-name$`" in paragraph 2.

::: std
[2]{.pnum} The attribute may be applied to the declaration of a class, [`$typedef-name$`]{.rm} [type alias]{.addu}, variable (including a structured binding declaration), structured binding, non-static data member, function, enumeration, or enumerator, or to an `$identifier$` label ([stmt.label]).
:::

### [module.unit]{.sref} Module units and purviews {-}

Update paragraph 7 to attach declarations of type aliases and namespace aliases (which are now entities) to the global module.

::: std
[7]{.pnum} A _module_ is either a named module or the global module. A declaration is _attached_ to a module as follows:

- [#.#]{.pnum} If the declaration is a non-dependent friend declaration [...]
- [#.#]{.pnum} Otherwise, if the declaration
  - [#.#.#]{.pnum} [is a `$namespace-definition$` with]{.rm} [declares a namespace whose name has]{.addu} external linkage,
  - [#.#.#]{.pnum} [declares a type alias,]{.addu}
  - [#.#.#]{.pnum} [declares a namespace alias,]{.addu} or
  - [#.#.#]{.pnum} appears within a `$linkage-specification$` ([dcl.link])

  it is attached to the global module.

- [#.#]{.pnum} Otherwise, the declaration is attached to the module in whose purview it appears.

:::

### [module.interface]{.sref} Module interface {-}

Update paragraph 5, and the note that follows, to account for type aliases now being entities.

::: std
[5]{.pnum} If an exported declaration is a `$using-declaration$` ([namespace.udecl]) and is not within a header unit, all entities [to which all of]{.rm} [named by]{.addu} the `$using-declarator$`s [ultimately refer]{.rm} (if any) shall [either be a type alias or]{.addu} have been introduced with a name having external linkage.

::: example2
[...]
:::

::: note2
[The underlying entity of an exported type alias need not have a name with external linkage.]{.addu} [These constraints do not apply to type names introduced by `typedef` declarations and `$alias-declaration$`s]{.rm}.

::: example3
[...]
:::
:::
:::

Carve out an exception for type aliases in paragraph 6 and remove the claim in example 4 that `S` does not declare an entity:

::: std
[6]{.pnum} A redeclaration of an entity `$X$` is implicitly exported if `$X$` was introduced by an exported declaration; otherwise it shall not be exported unless it is a [type alias, a]{.addu} namespace[, or a namespace alias]{.addu}.

::: example4
```cpp
[...]
export typedef S S;  // OK@[`, does not redeclare an entity`]{.rm}@

[...]
```
:::
:::

### [module.global.frag]{.sref} Global module fragment {-}

Specify in paragraph 3 that it is unspecified whether spliced types are replaced by their designated types, and renumber accordingly. Add an additional bullet further clarifying that it is unspecified whether any splice specifier is replaced.

::: std
[3]{.pnum} [...]

In this determination, it is unspecified

- [3.6]{.pnum} whether a reference to an `$alias-declaration$`, `typedef` declaration, `$using-declaration$`, or `$namespace-alias-definition$` is replaced by the declarations they name prior to this determination,
- [#.#]{.pnum} whether a `$simple-template-id$` that does not denote a dependent type and whose `$template-name$` names an alias template is replaced by its denoted type prior to this determination,
- [#.#]{.pnum} whether a `$decltype-specifier$` that does not denote a dependent type is replaced by its denoted type prior to this determination, [and]{.rm}
- [#.#]{.pnum} whether a non-value-dependent constant expression is replaced by the result of constant evaluation prior to this determination[.]{.rm}[, and]{.addu}
- [[#.#]{.pnum} whether a `$splice-expression$`, a `$splice-type-specifier$`, a `$splice-scope-specifier$`, or any `$splice-specifier$` or `$splice-specialization-specifier$` outside of the preceding is replaced in any non-dependent context by the construct that it designates prior to this determination.]{.addu}

:::

### [module.context]{.sref} Instantiation context {-}

Modify paragraphs 2 through 6 to relax the phrasing used to define the points in the instantiation context, add a new paragraph to include synthesized points in the instantiation context, and add a paragraph clarifying that the context contains only these points.

::: std
[2]{.pnum} During the implicit definition of a defaulted function ([special], [class.compare.default]), the instantiation context [is]{.rm} [contains each point in]{.addu} [the union of]{.rm} the instantiation context from the definition of the class and [each point in]{.addu} the instantiation context of the program construct that resulted in the implicit definition of the defaulted function.

[3]{.pnum} During the implicit instantiation of a template whose point of instantiation is specified as that of an enclosing specialization ([temp.point]), the instantiation context [is]{.rm} [contains each point in]{.addu} [the union of]{.rm} the instantiation context of the enclosing specialization and, if the template is defined in a module interface unit of a module `$M$` and the point of instantiation is not in a module interface unit of `$M$`, the point at the end of the `$declaration-seq$` of the primary module interface unit of `$M$` (prior to the `$private-module-fragment$`, if any).

[4]{.pnum} During the implicit instantiation of a template that is implicitly instantiated because it is referenced from within the implicit definition of a defaulted function, the instantiation context [is]{.rm} [contains each point in]{.addu} the instantiation context of the defaulted function.

[5]{.pnum} During the instantiation of any other template specialization, the instantiation context [comprises]{.rm} [contains]{.addu} the point of instantiation of the template.

[[5+]{.pnum} During the implicit instantiation of any construct that resulted from the evaluation of an expression as a core constant expression, the instantiation context contains each point in the evaluation context ([expr.const]).]{.addu}

[[Implicit instantiations can result from invocations of library functions ([meta.reflection]). The evaluation context can include synthesized points associated with injected declarations produced by `std::meta::define_aggregate` ([meta.reflection.define.aggregate]).]{.note}]{.addu}

[6]{.pnum} In any other case, the instantiation context at a point within the program [comprises]{.rm} [contains]{.addu} that point.

[[6+]{.pnum} The instantiation context contains only the points specified above.]{.addu}
:::

### [module.reach]{.sref} Reachability {-}

Modify the definition of reachability to account for injected declarations:

::: std
[3]{.pnum} A declaration `$D$` is _reachable from_ a point `$P$` if

* [#.#]{.pnum} [`$P$` is a non-synthesized point and]{.addu}
  * [#.#.#]{.pnum} `$D$` appears prior to `$P$` in the same translation unit, or
  * [#.#.#]{.pnum} `$D$` is not discarded ([module.global.frag]), appears in a translation unit that is reachable from `$P$`, and does not appear within a `$private-module-fragment$`[; or]{.addu}
* [#.#]{.pnum} [`$D$` is the injected declaration for which `$P$` is the corresponding synthesized point]{.addu}.

::: addu
::: example
```cpp
class Incomplete;

consteval {
  int n = nonstatic_data_members_of(
      define_aggregate(^^Incomplete, {data_member_spec(^^int, {.name="x"})}),
      std::meta::access_context::current()
    ).size();

  Incomplete y;  // error: type of y is incomplete
}
/* P */
```

The value of `n` is 1. The member `Incomplete::x` members-of-precedes ([meta.reflection.member.queries]) the synthesized point `P` associated with the injected declaration produced by the call to `define_aggregate`.
:::
:::

A declaration is _reachable_ if it is reachable from any point in the instantiation context ([module.context]).
:::

### [class.mem.general]{.sref} General {-}

Modify the grammar for `$member-declaration$` as follows:

::: std
```diff
  $member-declaration$:
    $attribute-specifier-seq$@~opt~@ $decl-specifier-seq$@~opt~@ $member-declarator-list$@~opt~@;
    $function-definition$
    $friend-type-declaration$
    $using-declaration$
    $using-enum-declaration$
    $static_assert-declaration$
+   $consteval-block-declaration$
    $template-declaration$
    $explicit-specialization$
    $deduction-guide$
    $alias-declaration$
    $opaque-enum-declaration$
    $empty-declaration$
```
:::

Update paragraph 4 accordingly:

::: std
[4]{.pnum} A `$member-declaration$` does not [itself]{.addu} declare new members of the class if it is

* [#.#]{.pnum} a friend declaration ([class.friend]),
* [#.#]{.pnum} a `$deduction-guide$` ([temp.deduct.guide]),
* [#.#]{.pnum} a `$template-declaration$` whose declaration is one of the above,
* [#.#]{.pnum} a `$static_assert-declaration$`,
* [[#.#]{.pnum} a `$consteval-block-declaration$`,]{.addu}
* [#.#]{.pnum} a `$using-declaration$` ([namespace.udecl]) , or
* [#.#]{.pnum} an `$empty-declaration$`.

:::

Strike note 3; non-static data members of reference type also have associated member subobjects.

::: std
[[A non-static data member of non-reference type is a member subobject of a class object.]{.note3}]{.rm}
:::

Add a new paragraph after paragraph 6 specifying the size, alignment, and offset of member subobjects of a class.

::: std
[6+]{.pnum} [Every object of class type has a unique member subobject corresponding to each of its direct non-static data members. If any non-static data member of a class `C` is of reference type, then let `D` be an invented class that is identical to `C` except that each non-static member of `D` corresponding to a member of `C` of type "reference to `T`" instead has type "pointer to `T`". Every member subobject of a complete object of type `C` has the same size, alignment, and offset as that of the corresponding subobject of a complete object of type `D`. The size and alignment of `C` are the same as the size and alignment of `D`.]{.addu}

:::

Add a new paragraph to the end of the section defining _data member description_:

::: std
::: addu
[30+]{.pnum} A _data member description_ is a quintuple (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) describing the potential declaration of a non-static data member where

- [30+.#]{.pnum} `$T$` is a type,
- [30+.#]{.pnum} `$N$` is an `$identifier$` or ⊥,
- [30+.#]{.pnum} `$A$` is an alignment or ⊥,
- [30+.#]{.pnum} `$W$` is a bit-field width or ⊥, and
- [30+.#]{.pnum} `$NUA$` is a boolean value.

Two data member descriptions are equal if each of their respective components are the same entities, are the same identifiers, have equal values, or are both ⊥.

::: note
The components of a data member description describe a data member such that

- [30+.#]{.pnum} its type is specified using the type given by `$T$`,
- [30+.#]{.pnum} it is declared with the name given by `$N$` if `$N$` is not ⊥ and is otherwise unnamed,
- [30+.#]{.pnum} it is declared with the `$alignment-specifier$` ([dcl.align]) given by `alignas($A$)` if `$A$` is not ⊥ and is otherwise declared without an `$alignment-specifier$`,
- [30+.#]{.pnum} it is a bit-field ([class.bit]) with the width given by `$W$` if `$W$` is not ⊥ and is otherwise not a bit-field, and
- [30+.#]{.pnum} it is declared with the attribute `[[no_unique_address]]` ([dcl.attr.nouniqueaddr]) if `$NUA$` is `true` and is otherwise declared without that attribute.

Data member descriptions are represented by reflections ([basic.fundamental]) returned by `std::meta::data_member_spec` ([meta.reflection.define.aggregate]) and can be reified as data members of a class using `std::meta::define_aggregate` ([meta.reflection.define.aggregate]).
:::

:::
:::

### [class.derived.general]{.sref} General {-}

Introduce the term "direct base class relationship" to paragraph 2.

::: std
[2]{.pnum} The component names of a `$class-or-decltype$` are those of its `$nested-name-specifier$`, `$type-name$`, and/or `$simple-template-id$`. A `$class-or-decltype$` shall denote a (possily cv-qualified) class type that is not an incompletely defined class ([class.mem]); any cv-qualifiers are ignored. The class denoted by the `$class-or-decltype$` of a `$base-specifier$` is called a _direct base class_ for the class being defined[; for each such `$base-specifier$`, the corresponding  _direct base class relationship_ is the ordered pair (`$D$`, `$B$`) where `$D$` is the class being defined and `$B$` is the direct base class]{.addu}. The lookup for the component name of the `$type-name$` or `$simple-template-id$` is type-only ([basic.lookup]). [...]
:::

### [class.access.general]{.sref} General {-}

Prefer "type alias" rather than `$typedef-name$` in the note that follows paragraph 4.

::: std
[Because access control applies to the declarations named, if access control is applied to a [`$typedef-name$`]{.rm} [type alias]{.addu}, only the accessibility of the typedef or alias declaration itself is considered. The accessibility of the [entity referred to by the `$typedef-name$`]{.rm} [underlying entity]{.addu} is not considered.]{.note3}
:::

### [class.access.base]{.sref} Accessibility of base classes and base class members {-}

Update paragraph 5 to handle `$splice-expression$`s, and to make more clear that the "naming class" (renamed "designating class" here) is a property of the expression. State explicitly that members designated through `$splice-expression$`s are accessible.

::: std
[5]{.pnum} [...]

[The access to a member is affected by the class in which the member is named. This naming class is the]{.rm} [An expression `E` that designates a member `m` has a _designating class_ that affects the access to `m`. This designating class is either]{.addu}

- [[#.#]{.pnum} the innermost class of which `m` is directly a member if `E` is a `$splice-expression$` or]{.addu}
- [#.#]{.pnum} [the]{.addu} class in whose scope lookup performed a search that found [`m`]{.addu} [the member]{.rm} [otherwise]{.addu}.

[This class can be explicit, e.g., when a `$qualified-id$` is used, or implicit, e.g., when a class member access operator ([expr.ref]) is used (including cases where an implicit "`this->`" was added). If both a class member access operator and a `$qualified-id$` are used to name the member (as in `p->T::m`), the class [naming]{.rm} [designating]{.addu} the member is the class [denoted]{.rm} [designated]{.addu} by the `$nested-name-specifier$` of the `$qualified-id$` (that is, `T`).]{.note3}

A member `m` is accessible at the point `$R$` when [named]{.rm} [designated]{.addu} in class `N` if

- [[#.#]{.pnum} `m` is designated by a `$splice-expression$`, or]{.addu}
- [#.#]{.pnum} `m` as a member of `N` is public, or
- [#.#]{.pnum} `m` as a member of `N` is private, and `$R$` occurs in a direct member or friend of class `$N$`, or
- [#.#]{.pnum} `m` as a member of `N` is protected, and `$R$` occurs in a direct member or friend of class `$N$`, or in a member of a class `P` derived from `N`, where `m` as a member of `P` is public, private, or protected, or
- [#.#]{.pnum} there exists a base class `B` of `N` that is accessible at `$R$`, and `m` is accessible at `$R$` when [named]{.rm} [designated]{.addu} in class `B`.
:::

Update paragraph 6, and the note which follows, to use the term "designated class":

::: std
[6]{.pnum} If a class member access operator, including an implicit "`this->`", is used to access a non-static data member or non-static member function, the reference is ill-formed if the left operand (considered as a pointer in the "`.`" case) cannot be implicitly converted to a pointer to the [naming]{.rm} [designating]{.addu} class of the right operand.

[This requirement is in addition to the requirement that the member be accessible as [named]{.rm} [designated]{.addu}.]{.note}

:::

### [over.pre]{.sref} Preamble {-}

Add a note explaining the expressions that form overload sets after paragraph 2.

::: std
[2]{.pnum} When a function is [named]{.rm} [designated]{.addu} in a call, which function declaration is being referenced and the validity of the call are determined by comparing the types of the arguments at the point of use with the types of the parameters in the declarations in the overload set. This function selection process is called _overload resolution_ and is defined in [over.match].

  [[Overload sets are formed by `$id-expression$`s naming functions and function templates and by `$splice-expression$`s designating entities of the same kinds.]{.note}]{.addu}

:::

### [over.call.func]{.sref} Call to named function {-}

Change the section title:

::: std
### Call to [named]{.rm} [designated]{.addu} function
:::

Modify paragraph 1 to clarify that this section will also apply to splices of function templates.

::: std
[1]{.pnum} Of interest in [over.call.func] are only those function calls in which the `$posfix-expression$` ultimately contains an `$id-expression$` [or `$splice-expression$`]{.addu} that [denotes]{.rm} [designates]{.addu} one or more functions. Such a `$postfix-expression$`, perhaps nested arbitrarily deep in parentheses, has one of the following forms:

```diff
  $postfix-expression$:
     $postfix-expression$ . $id-expression$
+    $postfix-expression$ . $splice-expression$
     $postfix-expression$ -> $id-expression$
+    $postfix-expression$ -> $splice-expression$
-    $primary-expression$
+    $id-expression$
+    $splice-expression$
```

These represent two syntactic subcategories of function calls: qualified function calls and unqualified function calls.

:::

Modify paragraph 2 to account for overload resolution of `$splice-expression$`s. Massage the wording to better account for member function templates.

::: std
[2]{.pnum} In qualified function calls, the function is [named]{.rm} [designated]{.addu} by an `$id-expression$` [or `$splice-expression$` `$E$`]{.addu} preceded by an `->` or `.` operator. Since the construct `A->B` is generally equivalent to `(*A).B`, the rest of [over] assumes, without loss of generality, that all member function calls have been normalized to the form that uses an object and the `.` operator. Furthermore, [over] assumes that the `$postfix-expression$` that is the left operand of the `.` operator has type "_cv_ `T`" where `T` denotes a class.^102^ [The function declarations found by name lookup ([class.member.lookup]) constitute the set of candidate functions.]{.rm} [The set of candidate functions either is the set found by name lookup ([class.member.lookup]) if `$E$` is an `$id-expression$` or is the set determined as specified in [expr.prim.splice] if `$E$` is a `$splice-expression$`]{.addu}. The argument list is the `$expression-list$` in the call augmented by the addition of the left operand of the `.` operator in the normalized member function call as the implied object argument ([over.match.funcs]).

:::

Modify paragraph 3 to account for overload resolution of `$splice-expression$`s. Massage the wording to better account for member function templates.

::: std
[3]{.pnum} In unqualified function calls, the function is [named]{.rm} [designated]{.addu} by [a `$primary-expression$`]{.rm} [an `$id-expression$` or a `$splice-expression$` `$E$`]{.addu}. [The function declarations found by name lookup ([basic.lookup]) constitute the set of candidate functions]{.rm}
[The set of candidate functions either is the set found by name lookup ([basic.lookup]) if `$E$` is an `$id-expression$` or is the set determined as specified in [expr.prim.splice] if `$E$` is a `$splice-expression$`]{.addu}. [Because of the rules for name lookup, the]{.rm} [The]{.addu} set of candidate functions consists either entirely of non-member functions or entirely of member functions of some class `T`. In the former case or if [the `$primary-expression$`]{.rm} [`$E$`]{.addu} is [either a `$splice-expression$` or]{.addu} the address of an overload set, the argument list is the same as the `$expression-list$` in the call. Otherwise, the argument list is the `$expression-list$` in the call augmented by the addition of an implied function argument as in a qualified function call. If the current class is, or is derived from, `T`, and the keyword `this` ([expr.prim.this]) refers to it, then the implied object argument is `(*this)`. Otherwise, a contrived object of type `T` becomes the implied object argument;^103^ if overload resolution selects a non-static member function, the call is ill-formed.

:::

### [over.match.class.deduct]{.sref} Class template argument deduction {-}

Extend paragraph 1 to work with `$splice-type-specifier$`s.

::: std
[1]{.pnum} When resolving a placeholder for a deduced class type ([dcl.type.class.deduct]) where the `$template-name$` [or `$splice-type-specifier$`]{.addu} [names]{.rm} [designates]{.addu} a class template `C`, a set of functions and function templates, called the guides of `C`, is formed comprising:

* [#.#]{.pnum} ...

:::

Extend paragraph 3 to also cover `$splice-type-specifier$`s.

:::std
[3]{.pnum} When resolving a placeholder for a deduced class type ([dcl.type.simple]) where the `$template-name$` [or `$splice-type-specifier$`]{.addu} [names]{.rm} [designates]{.addu} an alias template `A`, the `$defining-type-id$` of `A` must be of the form

```cpp
typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ template@~_opt_~@ $simple-template-id$
```

as specified in [dcl.type.simple]. The guides of `A` are the set of functions or function templates formed as follows. ...

:::

### [over.match.viable]{.sref} Viable functions {-}

[The changes to paragraph 2.3 (except for the wording related to `$splice-expression$s`) are a part of the resolution to [@CWG2701]. These changes render [over.match.best.general]/4 redundant, hence the relocation of its associated example to this section.]{.ednote}

Specify rules for overload sets denoted by `$splice-expression$`s in paragraph 2, make drive-by fixes to help clear up the situation more generally, and move the example that formerly followed [over.match.best.general]/4 to follow after paragraph 2 (with new examples covering `$splice-expression$`s).

::: std
[2]{.pnum} First, to be a viable function, a candidate function shall have enough parameters to agree in number with the arguments in the list.

* [#.#]{.pnum} If there are `$m$` arguments in the lists, all candidate functions having exactly `$m$` parameters are viable.
* [#.#]{.pnum} A candidate function having fewer than `$m$` parameters is viable only if it has an ellipsis in its parameter list ([dcl.fct]). For the purposes of overload resolution, any argument for which there is no corresponding parameter is considered to "match the ellipsis" ([over.ics.ellipsis]).
* [#.#]{.pnum} A candidate function [`C`]{.addu} having more than `$m$` parameters is viable only if [all parameters following the `$m$@^th^@` have default arguments ([dcl.fct.default])]{.rm} [the set of scopes `$G$`, as defined below, is not empty. `$G$` consists of every scope `$X$` that satisfies all of the following:]{.addu}
  * [[#.#.#]{.pnum} There is a declaration of `C`, whose host scope is `$X$`, considered by the overload resolution.]{.addu}
  * [[#.#.#]{.pnum} For every `$k$@^th^@` parameter `P` where `$k$` > `$m$`, there is a reachable declaration, whose host scope is `$X$`, that specifies a default argument ([dcl.fct.default]) for `P`.]{.addu}

  [If `C` is selected as the best viable function ([over.match.best]):]{.addu}

  * [[#.#.#]{.pnum} `$G$` shall contain exactly one scope (call it `$S$`).]{.addu}
  * [[#.#.#]{.pnum} If the candidates are denoted by a `$splice-expression$`, then `$S$` shall not be a block scope.]{.addu}
  * [[#.#.#]{.pnum} The default arguments used in the call `$C$` are the default arguments specified by the reachable declarations whose host scope is `$S$`.]{.addu}

  For the purposes of overload resolution, the parameter list is truncated on the right, so that there are exactly `$m$` parameters.

::: addu
::: example
```cpp
namespace A {
  extern "C" void f(int, int = 5);
  extern "C" void f(int = 6, int);
}
namespace B {
  extern "C" void f(int, int = 7);
}

void use() {
  [:^^A::f:](3, 4);  // OK, default argument was not used for viability
  [:^^A::f:](3);     // error: default argument provided by declarations from two scopes
  [:^^A::f:]();      // OK, default arguments provided by declarations in the scope of A

  using A::f;
  using B::f;
  f(3, 4);           // OK, default argument was not used for viability
  f(3);              // error: default argument provided by declaration from two scopes
  f();               // OK, default arguments provided by declarations in the scope of A

  void g(int = 8);
  g();               // OK
  [:^^g:]();         // error: host scope is block scope
}

void h(int = 7);
constexpr std::meta::info r = ^^h;
void poison() {
  void h(int = 8);
  h();       // ok, calls h(8)
  [:^^h:](); // error: default argument provided by declarations from two scopes
}
void call_h() {
  [:^^h:](); // error: default argument provided by declarations from two scopes
  [:r:]();   // error: default argument provided by declarations from two scopes
}

template <typename... Ts>
int k(int = 3, Ts...);
int i = k<int>();  // error: no default argument for the second parameter
int j = k<>();     // OK
```

:::
:::

:::

### [over.match.best.general]{.sref} General {-}

[The changes to [over.match.viable]/2.3 included in this proposal (part of the resolution to [@CWG2701]) render paragraph 4 redundant; the contents of example 9 now follow [over.match.viable]/2.]{.ednote}

Delete paragraph 4 and example 9 and replace with a note:

::: std
::: addu
[If the best viable function was made viable by one or more default arguments, additional requirements apply ([over.match.viable]).]{.note}
:::
::: rm
[4]{.pnum} If the best viable function resolves to a function for which multiple declarations were found, and if any two of these declarations inhabit different scopes and specify a default argument that made the function viable, the program is ill-formed.

::: example9
```cpp
namespace A {
  extern "C" void f(int = 5);
}
namespace B {
  extern "C" void f(int = 5);
}

using A::f;
using B::f;

void use() {
  f(3);        // OK, default argument was not used for viability
  f();         // error: found default argument twice
}
```

:::

:::
:::

### [over.over]{.sref} Address of an overload set {-}

Remove the explicit references to `$id-expression$`s from paragraph 1 to allow taking the address of an overload set specified by a `$splice-expression$`:

::: std
[1]{.pnum} An [`$id-expression$` whose terminal name refers to]{.rm} [expression that designates]{.addu} an overload set `$S$` and that appears without arguments is resolved to a function, a pointer to function, or a pointer to member function for a specific function that is chosen from a set of functions selected from `$S$` determined based on the target type required in the context (if any), as described below. [...]

The [`$id-expression$`]{.rm} [expression]{.addu} can be preceded by the `&` operator.

:::

### [over.built]{.sref} Built-in operators {-}

Add built-in operator candidates for `std::meta::info` to [over.built]{.sref}:

::: std
[16]{.pnum} For every `T`, where `T` is a pointer-to-member type[, `std::meta::info`,]{.addu} or `std::nullptr_t`, there exist candidate operator functions of the form
```cpp
bool operator==(T, T);
bool operator!=(T, T);
```
:::

### [temp.param]{.sref} Template parameters {-}

Add a paragraph after paragraph 3 to disallow dependent concepts being used in a `$type-constraint$`:

::: std
::: addu
[3+]{.pnum} The `$nested-name-specifier$` of a `$type-constraint$`, if any, shall not be dependent.
:::
:::

### [temp.names]{.sref} Names of template specializations {-}

Extend and re-format paragraph 3 of [temp.names]{.sref}:

::: std

[3]{.pnum} A `<` is interpreted as the delimiter of a `$template-argument-list$` if either

::: addu
* [#.#]{.pnum} it follows a `$splice-specifier$` that either
  * [#.#.#]{.pnum} appears in a type-only context or
  * [#.#.#]{.pnum}is preceded by `template` or `typename`, or
:::

* [#.#]{.pnum} it follows a name that is not a `$conversion-function-id$` and
  * [#.#.#]{.pnum} that follows the keyword `template` or a `~` after a `$nested-name-specifier$` or in a class member access expression, or
  * [#.#.#]{.pnum} for which name lookup finds the `$injected-class-name$` of a class template or finds any declaration of a template, or
  * [#.#.#]{.pnum} that is an unqualified name for which name lookup either finds one or more functions or finds nothing, or
  * [#.#.#]{.pnum} that is a terminal name in a `$using-declarator$` ([namespace.udecl]), in a `$declarator-id$` ([dcl.meaning]), or in a type-only context other than a `$nested-name-specifier$` ([temp.res]).

[If the name is an identifier, it is then interpreted as a `$template-name$`. The keyword `template` is used to indicate that a dependent qualified name ([temp.dep.type]) denotes a template where an expression might appear.]{.note}

::: example
```diff
struct X {
  template<std::size_t> X* alloc();
  template<std::size_t> static X* adjust();
};
template<class T> void f(T* p) {
  T* p1 = p->alloc<200>();              // error: < means less than
  T* p2 = p->template alloc<200>();     // OK, < starts template argument list
  T::adjust<100>();                     // error: < means less than
  T::template adjust<100>();            // OK, < starts template argument list

+ static constexpr std::meta::info r = ^^T::adjust;
+ T* p3 = [:r:]<200>();                 // error: < means less than
+ T* p4 = template [:r:]<200>();        // OK, < starts template argument list
}
```
:::
:::

Clarify that the `>` disambiguation in paragraph 4 also applies to the parsing of `$splice-specialization-specifier$`s:

::: std
[4]{.pnum} When parsing a `$template-argument-list$`, the first non-nested `>`^108^ is taken as the ending delimiter rather than a greater-than operator. Similarly, the first non-nested `>>` is treated as two consecutive but distinct `>` tokens, the first of which is taken as the end of the `$template-argument-list$` and completes the `$template-id$` [or `$splice-specialization-specifier$`]{.addu}.

[The second `>` token produced by this replacement rule can terminate an enclosing `$template-id$` [or `$splice-specialization-specifier$`]{.addu} construct or it can be part of a different construct (e.g., a cast).]{.note}

:::

Add a new paragraph and example after paragraph 5 that disallows unparenthesized splice expressions as template arguments.

::: std
[5]{.pnum} The keyword `template` shall not appear immediately after a declarative `$nested-name-specifier$` ([expr.prim.id.qual]).

::: addu
[5+]{.pnum} The `$constant-expression$` of a `$template-argument$` shall not be an unparenthesized `$splice-expression$`.

::: example2
```cpp
template<int> struct S { };

constexpr int k = 5;
constexpr std::meta::info r = ^^k;
S<[:r:]> s1;      // error: unparenthesized splice-expression used as template argument
S<([:r:])> s2;    // OK
S<[:r:] + 1> s3;  // OK
```
:::
:::
:::

Extend the definition of a _valid_ `$template-id$` to also cover `$splice-specialization-specifier$`s:

::: std
[7]{.pnum} A `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is _valid_ if

* [#.#]{.pnum} there are at most as many arguments as there are parameters or a parameter is a template parameter pack ([temp.variadic]),
* [#.#]{.pnum} there is an argument for each non-deducible non-pack parameter that does not have a default `$template-argument$`,
* [#.#]{.pnum} each `$template-argument$` matches the corresponding `$template-parameter$` ([temp.arg]),
* [#.#]{.pnum} substitution of each template argument into the following template parameters (if any) succeeds, and
* [#.#]{.pnum} if the `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is non-dependent, the associated constraints are satisfied as specified in the next paragraph.

A `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} shall be valid unless [it names]{.rm} [its respective `$template-name$` or `$splice-specifier$` names or designates]{.addu} a function template [specialization]{.rm} ([temp.deduct]).

:::

Extend paragraph 8 to require constraints to also be satisfied by `$splice-specialization-specifier$`s:

::: std
[8]{.pnum} When the `$template-name$` of a `$simple-template-id$` [or the `$splice-specifier$` of a `$splice-specialization-specifier$` designates]{.addu} [names]{.rm} a constrained non-function template or a constrained template `$template-parameter$`, and all `$template-argument$`s in the `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} are non-dependent ([temp.dep.temp]), the associated constraints ([temp.constr.decl]) of the constrained template shall be satisfied ([temp.constr.constr]).

:::

Modify footnote 108 to account for `$splice-specialization-specifier$`s:

::: std
[108)]{.pnum} A `>` that encloses the `$type-id$` of a `dynamic_cast`, `static_cast`, `reinterpret_cast` or `const_cast`, or which encloses the `$template-argument$`s of a subsequent `$template-id$` [or `$splice-specialization-specifier$`]{.addu}, is considered nested for the purpose of this description.

:::


### [temp.arg.general]{.sref} General {-}

Modify paragraph 1 to account for `$splice-specialization-specifier$`s.

::: std
[1]{.pnum} The type and form of each `$template-argument$` specified in a `$template-id$` [or in a `$splice-specialization-specifier$`]{.addu} shall match the type and form specified for the corresponding parameter declared by the template in its `$template-parameter-list$`. When the parameter declared by the template is a template parameter pack, it will correspond to zero or more `$template-argument$`s.

:::

Clarify in paragraph 9 that default template arguments also apply to `$splice-specialization-specifier$`s:

::: std
[9]{.pnum} When a `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} does not [name]{.rm} [designate]{.addu} a function, a default `$template-argument$` is implicitly instantiated when the value of that default argument is needed.

:::

### [temp.type]{.sref} Type equivalence {-}

Extend _template-argument-equivalent_ in paragraph 2 to handle `std::meta::info`, and add a note between that paragraph and the following example:

::: std
[2]{.pnum} Two values are _template-argument-equivalent_ if they are of the same type and

* [2.1]{.pnum} they are of integral type and their values are the same, or
* [2.2]{.pnum} they are of floating-point type and their values are identical, or
* [2.3]{.pnum} they are of type `std::nullptr_t`, or
* [2.*]{.pnum} [they are of type `std::meta::info` and their values compare equal ([expr.eq]), or]{.addu}
* [2.4]{.pnum} they are of enumeration type and their values are the same, or
* [2.5]{.pnum} [...]

::: example1
```cpp
template<class E, int size> class buffer { /* ... */ };
[...]
```
:::
:::

### [temp.deduct.guide]{.sref} Deduction guides {-}

Extend paragraph 1 to clarify that `$splice-type-specifier$`s can also leverage deduction guides.

::: std
[1]{.pnum} Deduction guides are used when a `$template-name$` [or `$splice-type-specifier$`]{.addu} appears as a type specifier for a deduced class type ([dcl.type.class.deduct]). Deduction guides are not found by name lookup. Instead, when performing class template argument deduction ([over.match.class.deduct]), all reachable deduction guides declared for the class template are considered.

:::

### [temp.mem]{.sref} Member templates {-}

Clarify in Note 1 that a specialization of a conversion function template can be formed through a `$splice-expression$`.

::: std
::: note
A specialization of a conversion function template is [referenced]{.rm} [named]{.addu} in the same way as a non-template conversion function that converts to the same type ([class.conv.fct]).

...

[An expression designating a particular specialization of a conversion function template can only be formed with a `$splice-expression$`.]{.addu} There is no [analogous]{.addu} syntax to form a `$template-id$` ([temp.names]) [for such a function]{.addu} by providing an explicit template argument list ([temp.arg.explicit]).

:::
:::

### [temp.alias]{.sref} Alias templates {-}

Extend paragraph 2 to enable reflection of alias template specializations.

::: std
[2]{.pnum} [When a]{.rm} [A]{.addu}

* [#.#]{.pnum} `$template-id$` [that is not the operand of a `$reflect-expression$` or]{.addu}
* [#.#]{.pnum} [`$splice-specialization-specifier$`]{.addu}

[that designates]{.addu} [refers to]{.rm} the specialization of an alias template[, it]{.rm}  is equivalent to the associated type obtained by substitution of its `$template-argument$`s for the `$template-parameter$`s in the `$defining-type-id$` of the alias template. [Any other `$template-id$` that names a specialization of an alias template is a `$typedef-name$` for a type alias.]{.addu}
:::

### [temp.res.general]{.sref} General {-}

Extend paragraph 4 to define what it means for a `$splice-specifier$` to appear in a type-only context. Add `$using-enum-declarator$`s to the list of type-only contexts, as it allows the `typename` to be elided from a `$splice-type-specifier$` in non-dependent contexts.

::: std
[4]{.pnum} A qualified or unqualified name is said to be in a _type-only context_ if it is the terminal name of

* [#.#]{.pnum} a `$typename-specifier$`, `$type-requirement$`, `$nested-name-specifier$`, `$elaborated-type-specifier$`, `$class-or-decltype$`, [`$using-enum-declarator$`]{.addu} or
* [#.#]{.pnum} [...]
  * [4.4.6]{.pnum} `$parameter-declaration$` of a `$template-parameter$` (which necessarily declares a constant template parameter).

[A `$splice-specifier$` or `$splice-specialization-specifier$` ([basic.splice]) is said to be in a _type-only context_ if a hypothetical qualified name appearing in the same position would be in a type-only context.]{.addu}

::: example5
```cpp
template<class T> T::R f();
template<class T> void f(T::R);   // ill-formed, no diagnostic required: attempt to
                                  // declare a `void` variable template
@[`enum class Enum { A, B, C };`]{.addu}@

template<class T> struct S {
  using Ptr = PtrTraits<T>::Ptr;  // OK, in a $defining-type-id$
  @[`using Alias = [:^^int:];        // OK, in a $defining-type-id$`]{.addu}@
  T::R f(T::P p) {                // OK, class scope
    return static_cast<T::R>(p);  // OK, $type-id$ of a `static_cast`
  }
  auto g() -> S<T*>::Ptr;         // OK, $trailing-return-type$
  @[`auto h() -> [:^^S:]<T*>;        // OK, $trailing-return-type$`]{.addu}@
  @[`using enum [:^^Enum:];          // OK, $using-enum-declarator$`]{.addu}@
};
template<typename T> void f() {
  void (*pf)(T::X);               // variable `pf` of type `void*` initialized
                                  // with `T::X`
  void g(T::X);                   // error: `T::X` at block scope does not denote
                                  // a type (attempt to declare a `void` variable)
}
```
:::
:::

### [temp.dep.type]{.sref} Dependent types {-}

Account for dependent `$splice-type-specifier$`s in paragraph 10:

::: std
[10]{.pnum} A type is dependent if it is

* [#.#]{.pnum} a template parameter,
* [#.#]{.pnum} ...
* [#.13]{.pnum} denoted by `decltype($expression$)`, where `$expression$` is type-dependent[.]{.rm}[, or]{.addu}
* [[#.#]{.pnum} denoted by a `$splice-type-specifier$` in which either the `$splice-specifier$` or `$splice-specialization-specifier$` is dependent ([temp.dep.splice]).]{.addu}

:::

### [temp.dep.expr]{.sref} Type-dependent expressions {-}

Add to the list of never-type-dependent expression forms in paragraph 4:

::: std
```diff
     $literal$
     sizeof $unary-expression$
     sizeof ( $type-id$ )
     sizeof ... ( $identifier$ )
     alignof ( $type-id$ )
     typeid ( $expression$ )
     typeid ( $type-id$ )
     ::@~_opt_~@ delete $cast-expression$
     ::@~_opt_~@ delete [ ] $cast-expression$
     throw $assignment-expression$@~_opt_~@
     noexcept ( $expression$ )
     $requires-expression$
+    $reflect-expression$
```
:::

Add a new paragraph at the end of [temp.dep.expr]{.sref}:

::: std
::: addu

[9]{.pnum} A `$splice-expression$` is type-dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent ([temp.dep.splice]).
:::
:::

### [temp.dep.constexpr]{.sref} Value-dependent expressions {-}

Add two new paragraphs to the end of [temp.dep.constexpr] to specify the value-dependence of `$reflect-expression$`s and `$splice-expression$`s:

::: std
:::addu
[7]{.pnum} A `$reflect-expression$` is value-dependent if

- [#.#]{.pnum} it is of the form `^^ $reflection-name$` and the `$reflection-name$`
  - [#.#.#]{.pnum} is a dependent qualified name,
  - [#.#.#]{.pnum} is a dependent `$namespace-name$`,
  - [#.#.#]{.pnum} is the name of a template parameter, or
  - [#.#.#]{.pnum} names a dependent member of the current instantiation ([temp.dep.type]),
- [#.#]{.pnum} it is of the form `^^ $type-id$` and the `$type-id$` denotes a dependent type, or
- [#.#]{.pnum} it is of the form `^^ $id-expression$` and the `$id-expression$` is value-dependent.

[8]{.pnum} A `$splice-expression$` is value-dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent ([temp.dep.splice]).

:::
:::

### 13.8.3.4+ [temp.dep.splice] Dependent splice specifiers {-}


Add a new subsection of [temp.dep]{.sref} following [temp.dep.constexpr]{.sref}, and renumber accordingly.

::: std
::: addu
**Dependent splice specifiers   [temp.dep.splice]**

[1]{.pnum} A `$splice-specifier$` is dependent if its converted `$constant-expression$` is value-dependent. A `$splice-specialization-specifier$` is dependent if its `$splice-specifier$` is dependent or if any of its template arguments are dependent. A `$splice-scope-specifier$` is dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent.

[#]{.pnum}

::: example
```cpp
template <auto T, auto NS>
void fn() {
  using a = [:T:]<1>;  // [:T:]<1> is dependent because [:T:] is dependent

  static_assert([:NS:]::template TCls<1>::v == a::v);  // [:NS:] is dependent
}

namespace N {
template <auto V> struct TCls { static constexpr int v = V; };
}

int main() {
  fn<^^N::TCls, ^^N>();
}
```

:::

[#]{.pnum}

::: example
```cpp
template<template<class> class X>
struct S {
  [:^^X:]<int, float> m;
};

template<class> struct V1 {};
template<class, class = int> struct V2 {};

S<V1> s1; // error: V1<int, float> has too many template arguments
S<V2> s2; // OK
```

:::

:::
:::

### 13.8.3.6 [temp.dep.namespace] Dependent namespaces {-}

Add a new section to cover dependent namespace aliases.

::: std
::: addu
**Dependent namespaces   [temp.dep.namespace]**

[1]{.pnum} A namespace alias is dependent if it is introduced by a `$namespace-alias-definition$` whose `$qualified-namespace-specifier$` (if any) is a dependent qualified name or whose `$splice-specifier$` (if any) is dependent. A `$namespace-name$` is dependent if it names a dependent namespace alias.

::: example
```cpp
template <std::meta::info R> int fn() {
  namespace Alias = [:R:];  // [:R:] is dependent
  return typename Alias::T{};  // Alias is dependent
}

namespace NS {
  using T = int;
}

int a = fn<^^NS>();
```
:::

:::
:::

### [temp.expl.spec]{.sref} Explicit specialization {-}

Modify paragraph 9 to apply to incompletely-defined specializations in the abstract, rather than to how they are named. This avoids the question of whether they are named by a `$simple-template-id$` or designated by a `$splice-specialization-specifier$`. Make it a note.

::: std
[9]{.pnum} [ [*Note 1*:]{.addu} A [`$simple-template-id$` that names a]{.rm} class template explicit specialization that has been declared but not defined can be used exactly like [the names of]{.rm} other incompletely-defined classes ([basic.types]). [*—end note*]{.addu} ]

:::

### [temp.deduct.general]{.sref} General {-}

Cover `$splice-specialization-specifier$`s in paragraph 2:

::: std
[2]{.pnum} When an explicit template argument list is specified, if the given `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is not valid ([temp.names]), type deduction fails. Otherwise, the specified template argument values are substituted for the corresponding template parameters as specified below.

:::

### [temp.deduct.call]{.sref} Deducing template arguments from a function call {-}

Modify paragraph 4.3 to treat parameter types of function templates that are specified using `$splice-specialization-specifier$`s the same as parameter types that are specified using `$simple-template-id$`s.

::: std
- [4.3]{.pnum} If `P` is a class and `P` has the form `$simple-template-id$` [or `typename@~_opt_~@ $splice-specialization-specifier$`]{.addu}, then the transformed `A` can be a derived class `D` of the deduced `A`. Likewise, if `P` is a pointer to a class of the form `$simple-template-id$` [or `typename@~_opt_~@ $splice-specialization-specifier$`]{.addu}, the transformed `A` can be a pointer to a derived class `D` pointed to by the deduced `A`. However, if there is a class `C` that is a (direct or indirect) base class of `D` and derived (directly or indirectly) from a class `B` and that would be a valid deduced `A`, the deduced `A` cannot be `B1` or pointer to `B`, respectively.

:::

### [temp.deduct.type]{.sref} Deducing template arguments from a type {-}

Add the operand of a `$splice-specifier$` to the list of non-deduced contexts in paragraph 5:

::: std
[5]{.pnum} The non-deduced contexts are:

* [#.#]{.pnum} The `$nested-name-specifier$` of a type that was specified using a `$qualified-id$`.
* [#.#]{.pnum} A `$pack-index-specifier$` or a `$pack-index-expression$`.
* [#.#]{.pnum} The `$expression$` of a `$decltype-specifier$`.
* [[#.3+]{.pnum} The `$constant-expression$` of a `$splice-specifier$`.]{.addu}
* [#.4]{.pnum} A constant template argument or an array bound in which a subexpression references a template parameter.
* [#.#]{.pnum} ...

:::

Modify paragraph 20 to clarify that the construct enclosing a template argument might also be a `$splice-specialization-specifier$`.

::: std
[20]{.pnum} If `P` has a form that contains `<i>`, and if the type of `i` differs from the type of the corresponding template parameter of the template named by the enclosing `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu}, deduction fails. If `P` has a form that contains `[i]`, and if the type of `i` is not an integral type, deduction fails.^123^ If `P` has a form that includes `noexcept(i)` and the type of `i` is not `bool`, deduction fails.

:::

## Library

### [structure.specifications]{.sref} Detailed specifications {-}

For convenience, we're going to add a new library element to [structure.specifications]{.sref}/3:

::: std
[3]{.pnum} Descriptions of function semantics contain the following elements (as appropriate):

* [#.1]{.pnum} *Constraints*: [...]

* [#.2]{.pnum} *Mandates*: the conditions that, if not met, render the program ill-formed. [...]

::: addu
* [#.2+1]{.pnum} *Constant When*: the conditions that are required for a call to the function to be a constant subexpression ([defns.const.subexpr]).
:::

[4]{.pnum} [...] Next, the semantics of the code sequence are determined by the *Constraints*, *Mandates*, [*Constant When*,]{.addu} *Preconditions*, *Effects*, *Synchronization*, *Postconditions*, *Returns*, *Throws*, *Complexity*, *Remarks*, and *Error* conditions specified for the function invocations contained in the code sequence. [...]
:::

### [headers]{.sref} Headers {-}

Add `<meta>` to [tab:headers.cpp].

::: std
<table>
<tr><td>`<algorithm>`</td><td>`<forward_list>`</td><td>[`<meta>`]{.addu}</td><td>`<stack>`</td></tr>
<tr><td>`<any>`</td><td>`<fstream>`</td><td>`<mutex>`</td><td>`<stacktrace>`</td></tr>
<tr><td>`<array>`</td><td>`<functional>`</td><td>`<new>`</td><td>`<stdexcept>`</td></tr>
<tr><td>`<atomic>`</td><td>`<future>`</td><td>`<numbers>`</td><td>`<stdfloat>`</td></tr>
<tr><td>`<barrier>`</td><td>`<generator>`</td><td>`<numeric>`</td><td>`<stop_token>`</td></tr>
<tr><td>`<bit>`</td><td>`<hazard_pointer>`</td><td>`<optional>`</td><td>`<streambuf>`</td></tr>
<tr><td>`<bitset>`</td><td>`<initializer_list>`</td><td>`<ostream>`</td><td>`<string>`</td></tr>
<tr><td>`<charconv>`</td><td>`<inplace_vector>`</td><td>`<print>`</td><td>`<string_view>`</td></tr>
<tr><td>`<chrono>`</td><td>`<iomanip>`</td><td>`<queue>`</td><td>`<syncstream>`</td></tr>
<tr><td>`<compare>`</td><td>`<ios>`</td><td>`<random>`</td><td>`<system_error>`</td></tr>
<tr><td>`<complex>`</td><td>`<iosfwd>`</td><td>`<ranges>`</td><td>`<text_encoding>`</td></tr>
<tr><td>`<concepts>`</td><td>`<iostream>`</td><td>`<ratio>`</td><td>`<thread>`</td></tr>
<tr><td>`<condition_variable>`</td><td>`<istream>`</td><td>`<rcu>`</td><td>`<tuple>`</td></tr>
<tr><td>`<coroutine>`</td><td>`<iterator>`</td><td>`<regex>`</td><td>`<type_traits>`</td></tr>
<tr><td>`<debugging>`</td><td>`<latch>`</td><td>`<scoped_allocator>`</td><td>`<typeindex>`</td></tr>
<tr><td>`<deque>`</td><td>`<limits>`</td><td>`<semaphore>`</td><td>`<typeinfo>`</td></tr>
<tr><td>`<exception>`</td><td>`<linalg>`</td><td>`<set>`</td><td>`<unordered_map>`</td></tr>
<tr><td>`<execution>`</td><td>`<list>`</td><td>`<shared_mutex>`</td><td>`<unordered_set>`</td></tr>
<tr><td>`<expected>`</td><td>`<locale>`</td><td>`<simd>`</td><td>`<utility>`</td></tr>
<tr><td>`<filesystem>`</td><td>`<map>`</td><td>`<source_location>`</td><td>`<valarray>`</td></tr>
<tr><td>`<flat_map>`</td><td>`<mdspan>`</td><td>`<span>`</td><td>`<variant>`</td></tr>
<tr><td>`<flat_set>`</td><td>`<memory>`</td><td>`<spanstream>`</td><td>`<vector>`</td></tr>
<tr><td>`<format>`</td><td>`<memory_resource>`</td><td>`<sstream>`</td><td>`<version>`</td></tr>
</table>

:::

### [namespace.std]{.sref} Namespace std {-}

Insert before paragraph 7:

::: std

[6]{.pnum} Let F denote a standard library function ([global.functions]), a standard library static member function, or an instantiation of a standard library function template.
Unless F is designated an *addressable function*, the behavior of a C++ program is unspecified (possibly ill-formed) if it explicitly or implicitly attempts to form a pointer to F. [...]

::: addu

[6a]{.pnum}
Let F denote a standard library function or function template.
Unless F is designated an addressable function, it is unspecified if or how a reflection value designating the associated entity can be formed.
[For example, it is possible that `std::meta::members_of` will not return reflections of standard library functions that an implementation handles through an extra-linguistic mechanism.]{.note}

[6b]{.pnum}
Let `C` denote a standard library class or class template specialization. It is unspecified if or how a reflection value can be formed to any private member of `C`, or what the names of such members may be.
:::

[7]{.pnum} A translation unit shall not declare namespace std to be an inline namespace ([namespace.def]).

:::


### [meta.type.synop]{.sref} Header `<type_traits>` synopsis {-}

Add a new primary type category type trait:

::: std
**Header `<type_traits>` synopsis**

...
```diff
    // [meta.unary.cat], primary type categories
    template<class T> struct is_void;
...
    template<class T> struct is_function;
+   template<class T> struct is_reflection;

...

    // [meta.unary.prop], type properties
    template<class T> struct is_const;
...
    template<class T> struct is_aggregate;
+   template<class T> struct is_consteval_only;

...

    // [meta.unary.cat], primary type categories
    template<class T>
      constexpr bool is_void_v = is_void<T>::value;
...
    template<class T>
      constexpr bool is_function_v = is_function<T>::value;
+   template<class T>
+     constexpr bool is_reflection_v = is_reflection<T>::value;

...

    // [meta.unary.prop], type properties
    template<class T>
      constexpr bool is_const_v = is_const<T>::value;
...
    template<class T>
      constexpr bool is_aggregate_v = is_aggregate<T>::value;
+   template<class T>
+     constexpr bool is_consteval_only_v = is_consteval_only<T>::value;
    template<class T>
      constexpr bool is_signed_v = is_signed<T>::value;
...
```
:::

### [meta.unary.cat]{.sref} Primary type categories {-}

Add the `is_reflection` primary type category to the table in paragraph 3:

::: std
<table>
<tr style="text-align:center"><th>Template</th><th>Condition</th><th>Comments</th></tr>
<tr><td>
```cpp
template<class T>
struct is_void;
```
</td><td style="text-align:center; vertical-align: middle">`T` is `void`</td><td></td></tr>
<tr style="text-align:center"><td>...</td><td>...</td><td>...</td></tr>
<tr><td>
::: addu
```cpp
template<class T>
struct is_reflection;
```
:::
</td><td style="text-align:center; vertical-align: middle">
::: addu
`T` is `std::meta::info`
:::
</td><td>
::: addu
<br>
:::
</td></tr>
</table>
:::

### [meta.unary.prop]{.sref} Type properties {-}

Add the `is_consteval_only` type trait to table 51 following paragraph 5:

::: std
<table>
<tr style="text-align:center"><th>Template</th><th>Condition</th><th>Preconditions</th></tr>
<tr><td>
```cpp
template<class T>
struct is_const;
```
</td><td style="text-align:center; vertical-align: middle">`T` is const-qualified ([basic.type.qualifier])</td><td></td></tr>
<tr style="text-align:center"><td>...</td><td>...</td><td>...</td></tr>
<tr><td>
::: addu
```cpp
template<class T>
struct is_consteval_only;
```
:::
</td><td style="text-align:center; vertical-align: middle">
::: addu
`T` is consteval-only ([basic.types.general])
:::
</td><td>
::: addu
`remove_all_extents_t<T>` shall be a complete type or `$cv$ void`.
:::
</td></tr>
</table>
:::

### [meta.reflection.synop] Header `<meta>` synopsis {-}

Add a new subsection in [meta]{.sref} after [type.traits]{.sref}:

::: std
::: addu
**Header `<meta>` synopsis**

```
#include <initializer_list>

namespace std::meta {
  using info = decltype(^^::);

  // [meta.reflection.operators], operator representations
  enum class operators {
    $see below$;
  };
  using enum operators;
  consteval operators operator_of(info r);
  consteval string_view symbol_of(operators op);
  consteval u8string_view u8symbol_of(operators op);

  // [meta.reflection.names], reflection names and locations
  consteval bool has_identifier(info r);

  consteval string_view identifier_of(info r);
  consteval u8string_view u8identifier_of(info r);

  consteval string_view display_string_of(info r);
  consteval u8string_view u8display_string_of(info r);

  consteval source_location source_location_of(info r);

  // [meta.reflection.queries], reflection queries
  consteval info type_of(info r);
  consteval info object_of(info r);
  consteval info constant_of(info r);

  consteval bool is_public(info r);
  consteval bool is_protected(info r);
  consteval bool is_private(info r);

  consteval bool is_virtual(info r);
  consteval bool is_pure_virtual(info r);
  consteval bool is_override(info r);
  consteval bool is_final(info r);

  consteval bool is_deleted(info r);
  consteval bool is_defaulted(info r);
  consteval bool is_user_provided(info r);
  consteval bool is_user_declared(info r);
  consteval bool is_explicit(info r);
  consteval bool is_noexcept(info r);

  consteval bool is_bit_field(info r);
  consteval bool is_enumerator(info r);

  consteval bool is_const(info r);
  consteval bool is_volatile(info r);
  consteval bool is_mutable_member(info r);
  consteval bool is_lvalue_reference_qualified(info r);
  consteval bool is_rvalue_reference_qualified(info r);

  consteval bool has_static_storage_duration(info r);
  consteval bool has_thread_storage_duration(info r);
  consteval bool has_automatic_storage_duration(info r);

  consteval bool has_internal_linkage(info r);
  consteval bool has_module_linkage(info r);
  consteval bool has_external_linkage(info r);
  consteval bool has_c_language_linkage(info r);
  consteval bool has_linkage(info r);

  consteval bool is_complete_type(info r);
  consteval bool is_enumerable_type(info r);

  consteval bool is_variable(info r);
  consteval bool is_type(info r);
  consteval bool is_namespace(info r);
  consteval bool is_type_alias(info r);
  consteval bool is_namespace_alias(info r);

  consteval bool is_function(info r);
  consteval bool is_conversion_function(info r);
  consteval bool is_operator_function(info r);
  consteval bool is_literal_operator(info r);
  consteval bool is_special_member_function(info r);
  consteval bool is_constructor(info r);
  consteval bool is_default_constructor(info r);
  consteval bool is_copy_constructor(info r);
  consteval bool is_move_constructor(info r);
  consteval bool is_assignment(info r);
  consteval bool is_copy_assignment(info r);
  consteval bool is_move_assignment(info r);
  consteval bool is_destructor(info r);

  consteval bool is_template(info r);
  consteval bool is_function_template(info r);
  consteval bool is_variable_template(info r);
  consteval bool is_class_template(info r);
  consteval bool is_alias_template(info r);
  consteval bool is_conversion_function_template(info r);
  consteval bool is_operator_function_template(info r);
  consteval bool is_literal_operator_template(info r);
  consteval bool is_constructor_template(info r);
  consteval bool is_concept(info r);

  consteval bool is_value(info r);
  consteval bool is_object(info r);

  consteval bool is_structured_binding(info r);

  consteval bool is_class_member(info r);
  consteval bool is_namespace_member(info r);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);

  consteval bool has_default_member_initializer(info r);

  consteval bool has_parent(info r);
  consteval info parent_of(info r);

  consteval info dealias(info r);

  consteval bool has_template_arguments(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);

  // [meta.reflection.access.context], access control context
  struct access_context;

  // [meta.reflection.access.queries], member accessessibility queries
  consteval bool is_accessible(info r, access_context ctx);
  consteval bool has_inaccessible_nonstatic_data_members(
      info r,
      access_context ctx);
  consteval bool has_inaccessible_bases(info r, access_context ctx);

  // [meta.reflection.member.queries], reflection member queries
  consteval vector<info> members_of(info r, access_context ctx);
  consteval vector<info> bases_of(info type, access_context ctx);
  consteval vector<info> static_data_members_of(info type, access_context ctx);
  consteval vector<info> nonstatic_data_members_of(info type, access_context ctx);
  consteval vector<info> enumerators_of(info type_enum);

  // [meta.reflection.layout], reflection layout queries
  struct member_offset;
  consteval member_offset offset_of(info r);
  consteval size_t size_of(info r);
  consteval size_t alignment_of(info r);
  consteval size_t bit_size_of(info r);

  // [meta.reflection.extract], value extraction
  template<class T>
    consteval T extract(info);

  // [meta.reflection.substitute], reflection substitution
  template <class R>
    concept reflection_range = $see below$;

  template <reflection_range R = initializer_list<info>>
    consteval bool can_substitute(info templ, R&& arguments);
  template <reflection_range R = initializer_list<info>>
    consteval info substitute(info templ, R&& arguments);

  // [meta.reflection.result], expression result reflection
  template<class T>
    consteval info reflect_constant(const T& value);
  template<class T>
    consteval info reflect_object(T& object);
  template<class T>
    consteval info reflect_function(T& fn);

  // [meta.reflection.define.aggregate], class definition generation
  struct data_member_options;
  consteval info data_member_spec(info type,
                                  data_member_options options);
  consteval bool is_data_member_spec(info r);
  template <reflection_range R = initializer_list<info>>
  consteval info define_aggregate(info type_class, R&&);

  // associated with [meta.unary.cat], primary type categories
  consteval bool is_void_type(info type);
  consteval bool is_null_pointer_type(info type);
  consteval bool is_integral_type(info type);
  consteval bool is_floating_point_type(info type);
  consteval bool is_array_type(info type);
  consteval bool is_pointer_type(info type);
  consteval bool is_lvalue_reference_type(info type);
  consteval bool is_rvalue_reference_type(info type);
  consteval bool is_member_object_pointer_type(info type);
  consteval bool is_member_function_pointer_type(info type);
  consteval bool is_enum_type(info type);
  consteval bool is_union_type(info type);
  consteval bool is_class_type(info type);
  consteval bool is_function_type(info type);
  consteval bool is_reflection_type(info type);

  // associated with [meta.unary.comp], composite type categories
  consteval bool is_reference_type(info type);
  consteval bool is_arithmetic_type(info type);
  consteval bool is_fundamental_type(info type);
  consteval bool is_object_type(info type);
  consteval bool is_scalar_type(info type);
  consteval bool is_compound_type(info type);
  consteval bool is_member_pointer_type(info type);

  // associated with [meta.unary.prop], type properties
  consteval bool is_const_type(info type);
  consteval bool is_volatile_type(info type);
  consteval bool is_trivially_copyable_type(info type);
  consteval bool is_trivially_relocatable_type(info type);
  consteval bool is_replaceable_type(info type);
  consteval bool is_standard_layout_type(info type);
  consteval bool is_empty_type(info type);
  consteval bool is_polymorphic_type(info type);
  consteval bool is_abstract_type(info type);
  consteval bool is_final_type(info type);
  consteval bool is_aggregate_type(info type);
  consteval bool is_consteval_only_type(info type);
  consteval bool is_signed_type(info type);
  consteval bool is_unsigned_type(info type);
  consteval bool is_bounded_array_type(info type);
  consteval bool is_unbounded_array_type(info type);
  consteval bool is_scoped_enum_type(info type);

  template <reflection_range R = initializer_list<info>>
    consteval bool is_constructible_type(info type, R&& type_args);
  consteval bool is_default_constructible_type(info type);
  consteval bool is_copy_constructible_type(info type);
  consteval bool is_move_constructible_type(info type);

  consteval bool is_assignable_type(info type_dst, info type_src);
  consteval bool is_copy_assignable_type(info type);
  consteval bool is_move_assignable_type(info type);

  consteval bool is_swappable_with_type(info type_dst, info type_src);
  consteval bool is_swappable_type(info type);

  consteval bool is_destructible_type(info type);

  template <reflection_range R = initializer_list<info>>
    consteval bool is_trivially_constructible_type(info type, R&& type_args);
  consteval bool is_trivially_default_constructible_type(info type);
  consteval bool is_trivially_copy_constructible_type(info type);
  consteval bool is_trivially_move_constructible_type(info type);

  consteval bool is_trivially_assignable_type(info type_dst, info type_src);
  consteval bool is_trivially_copy_assignable_type(info type);
  consteval bool is_trivially_move_assignable_type(info type);
  consteval bool is_trivially_destructible_type(info type);

  template <reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_constructible_type(info type, R&& type_args);
  consteval bool is_nothrow_default_constructible_type(info type);
  consteval bool is_nothrow_copy_constructible_type(info type);
  consteval bool is_nothrow_move_constructible_type(info type);

  consteval bool is_nothrow_assignable_type(info type_dst, info type_src);
  consteval bool is_nothrow_copy_assignable_type(info type);
  consteval bool is_nothrow_move_assignable_type(info type);

  consteval bool is_nothrow_swappable_with_type(info type_dst, info type_src);
  consteval bool is_nothrow_swappable_type(info type);

  consteval bool is_nothrow_destructible_type(info type);
  consteval bool is_nothrow_relocatable_type(info type);

  consteval bool is_implicit_lifetime_type(info type);

  consteval bool has_virtual_destructor(info type);

  consteval bool has_unique_object_representations(info type);

  consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
  consteval bool reference_converts_from_temporary(info type_dst, info type_src);

  // associated with [meta.unary.prop.query], type property queries
  consteval size_t rank(info type);
  consteval size_t extent(info type, unsigned i = 0);

  // associated with [meta.rel], type relations
  consteval bool is_same_type(info type1, info type2);
  consteval bool is_base_of_type(info type_base, info type_derived);
  consteval bool is_virtual_base_of_type(info type_base, info type_derived);
  consteval bool is_convertible_type(info type_src, info type_dst);
  consteval bool is_nothrow_convertible_type(info type_src, info type_dst);
  consteval bool is_layout_compatible_type(info type1, info type2);
  consteval bool is_pointer_interconvertible_base_of_type(info type_base, info type_derived);

  template <reflection_range R = initializer_list<info>>
    consteval bool is_invocable_type(info type, R&& type_args);
  template <reflection_range R = initializer_list<info>>
    consteval bool is_invocable_r_type(info type_result, info type, R&& type_args);

  template <reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_type(info type, R&& type_args);
  template <reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_r_type(info type_result, info type, R&& type_args);

  // associated with [meta.trans.cv], const-volatile modifications
  consteval info remove_const(info type);
  consteval info remove_volatile(info type);
  consteval info remove_cv(info type);
  consteval info add_const(info type);
  consteval info add_volatile(info type);
  consteval info add_cv(info type);

  // associated with [meta.trans.ref], reference modifications
  consteval info remove_reference(info type);
  consteval info add_lvalue_reference(info type);
  consteval info add_rvalue_reference(info type);

  // associated with [meta.trans.sign], sign modifications
  consteval info make_signed(info type);
  consteval info make_unsigned(info type);

  // associated with [meta.trans.arr], array modifications
  consteval info remove_extent(info type);
  consteval info remove_all_extents(info type);

  // associated with [meta.trans.ptr], pointer modifications
  consteval info remove_pointer(info type);
  consteval info add_pointer(info type);

  // associated with [meta.trans.other], other transformations
  consteval info remove_cvref(info type);
  consteval info decay(info type);
  template <reflection_range R = initializer_list<info>>
    consteval info common_type(R&& type_args);
  template <reflection_range R = initializer_list<info>>
    consteval info common_reference(R&& type_args);
  consteval info type_underlying_type(info type);
  template <reflection_range R = initializer_list<info>>
    consteval info invoke_result(info type, R&& type_args);
  consteval info unwrap_reference(info type);
  consteval info unwrap_ref_decay(info type);

  consteval size_t tuple_size(info type);
  consteval info tuple_element(size_t index, info type);

  consteval size_t variant_size(info type);
  consteval info variant_alternative(size_t index, info type);

  consteval strong_ordering type_order(info type_a, info type_b);
}
```

[1]{.pnum} Unless otherwise specified, each function, and each specialization of any function template, specified in this header is a designated addressable function ([namespace.std]).

[2]{.pnum} The behavior of any function specified in namespace `std::meta` is implementation-defined when a reflection of a construct not otherwise specified by this document is provided as an argument.

[Values of type `std::meta::info` can represent implementation-specific constructs ([basic.fundamental]).]{.note}

::: note
 The behavior of many of the functions specified in namespace `std::meta` have semantics that can be affected by the completeness of class types represented by reflection values. For such functions, for any reflection `r` such that `dealias(r)` represents a specialization of a templated class with a reachable definition, the specialization is implicitly instantiated ([temp.inst]).

::: example
```cpp
template <class T>
struct X {
  T mem;
};

static_assert(size_of(^^X<int>) == sizeof(int)); // instantiates X<int>
```
:::
:::

[3]{.pnum} Any function in namespace `std::meta` whose return type is `string_view` or `u8string_view` returns an object `$V$` such that `$V$.data()[$V$.size()]` equals `'\0'`.

::: example
```cpp
struct C { };

constexpr string_view sv = identifier_of(^^C);
static_assert(sv == "C");
static_assert(sv.data()[0] == 'C');
static_assert(sv.data()[1] == '\0');
```
:::

[4]{.pnum} For the purpose of exposition, throughout this clause `^^$E$` is used to indicate a reflection representing source construct `$E$`.
:::
:::

### [meta.reflection.operators] Operator representations {-}

::: std
::: addu
```cpp
enum class operators {
  $see below$;
};
using enum operators;
```

[#]{.pnum} The enumeration type `operators` specifies constants used to identify operators that can be overloaded, with the meanings listed in Table 1. The values of the constants are distinct.

[The names here are chosen after the punctuation marks, not the semantic operation, and we are sticking with the Unicode names — or resorting to the secondary name when the primary name is not well known (e.g. `solidus` -> `slash`)]{.draftnote}

<center>Table 1: Enum class `operators` [meta.reflection.operators]</center>

|Constant|Corresponding `$operator-function-id$`|Operator symbol name|
|:-|:-|:-|
|`op_new`|`operator new`|`new`|
|`op_delete`|`operator delete`|`delete`|
|`op_array_new`|`operator new[]`|`new[]`|
|`op_array_delete`|`operator delete[]`|`delete[]`|
|`op_co_await`|`operator co_await`|`co_await`|
|`op_parentheses`|`operator()`|`()`|
|`op_square_brackets`|`operator[]`|`[]`|
|`op_arrow`|`operator->`|`->`|
|`op_arrow_star`|`operator->*`|`->*`|
|`op_tilde`|`operator~`|`~`|
|`op_exclamation`|`operator!`|`!`|
|`op_plus`|`operator+`|`+`|
|`op_minus`|`operator-`|`-`|
|`op_star`|`operator*`|`*`|
|`op_slash`|`operator/`|`/`|
|`op_percent`|`operator%`|`%`|
|`op_caret`|`operator^`|`^`|
|`op_ampersand`|`operator&`|`&`|
|`op_pipe`|`operator|`|`|`|
|`op_equals`|`operator=`|`=`|
|`op_plus_equals`|`operator+=`|`+=`|
|`op_minus_equals`|`operator-=`|`-=`|
|`op_star_equals`|`operator*=`|`*=`|
|`op_slash_equals`|`operator/=`|`/=`|
|`op_percent_equals`|`operator%=`|`%=`|
|`op_caret_equals`|`operator^=`|`^=`|
|`op_ampersand_equals`|`operator&=`|`&=`|
|`op_pipe_equals`|`operator|=`|`|=`|
|`op_equals_equals`|`operator==`|`==`|
|`op_exclamation_equals`|`operator!=`|`!=`|
|`op_less`|`operator<`|`<`|
|`op_greater`|`operator>`|`>`|
|`op_less_equals`|`operator<=`|`<=`|
|`op_greater_equals`|`operator>=`|`>=`|
|`op_spaceship`|`operator<=>`|`<=>`|
|`op_ampersand_ampersand`|`operator&&`|`&&`|
|`op_pipe_pipe`|`operator||`|`||`|
|`op_less_less`|`operator<<`|`<<`|
|`op_greater_greater`|`operator>>`|`>>`|
|`op_less_less_equals`|`operator<<=`|`<<=`|
|`op_greater_greater_equals`|`operator>>=`|`>>=`|
|`op_plus_plus`|`operator++`|`++`|
|`op_minus_minus`|`operator--`|`--`|
|`op_comma`|`operator,`|`,`{.op}|

```cpp
consteval operators operator_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents an operator function or operator function template.

[#]{.pnum} *Returns*: The value of the enumerator from `operators` whose corresponding `$operator-function-id$` is the unqualified name of the entity represented by `r`.

```cpp
consteval string_view symbol_of(operators op);
consteval u8string_view u8symbol_of(operators op);
```

[#]{.pnum} *Constant When*: The value of `op` corresponds to one of the enumerators in `operators`.

[#]{.pnum} *Returns*: A `string_view` or `u8string_view` containing the characters of the operator symbol name corresponding to `op`, respectively encoded with the ordinary literal encoding or with UTF-8.
:::
:::

### [meta.reflection.names] Reflection names and locations {-}

::: std
::: addu
```cpp
consteval bool has_identifier(info r);
```

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents an entity that has a typedef name for linkage purposes ([dcl.typedef]), then `true`.
* [#.#]{.pnum} Otherwise, if `r` represents an unnamed entity, then `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a class type, then `!has_template_arguments(r)`.
* [#.#]{.pnum} Otherwise, if `r` represents a function, then `true` if `has_template_arguments(r)` is `false` and the function is not a constructor, destructor, operator function, or conversion function. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a template, then `true` if `r` does not represent a constructor template, operator function template, or conversion function template. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a variable, then `false` if the declaration of that variable was instantiated from a function parameter pack. Otherwise, `!has_template_arguments(r)`.
* [#.#]{.pnum} Otherwise, if `r` represents a structured binding, then `false` if the declaration of that structured binding was instantiated from a structured binding pack. Otherwise, `true`.
* [#.#]{.pnum} Otherwise, if `r` represents a type alias, then `!has_template_arguments(r)`.
* [#.#]{.pnum} Otherwise, if `r` represents an enumerator, non-static data member, namespace, or namespace alias, then `true`.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `has_identifier(type_of(r))`.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]); `true` if `$N$` is not ⊥. Otherwise, `false`.

```cpp
consteval string_view identifier_of(info r);
consteval u8string_view u8identifier_of(info r);
```

[#]{.pnum} Let *E* be UTF-8 for `u8identifier_of`, and otherwise the ordinary literal encoding.

[#]{.pnum} *Constant When*: `has_identifier(r)` is `true` and the identifier that would be returned (see below) is representable by `$E$`.

[#]{.pnum} *Returns*: An NTMBS, encoded with `$E$`, determined as follows:

* [#.#]{.pnum} If `r` represents an entity with a typedef name for linkage purposes, then that name.
* [#.#]{.pnum} Otherwise, if `r` represents a literal operator or literal operator template, then the `$ud-suffix$` of the operator or operator template.
* [#.#]{.pnum} Otherwise, if `r` represents an entity, then the identifier introduced by the declaration of that entity.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `identifier_of(type_of(r))` or `u8identifier_of(type_of(r))`, respectively.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]); a `string_view` or `u8string_view`, respectively, containing the identifier `$N$`.

```cpp
consteval string_view display_string_of(info r);
consteval u8string_view u8display_string_of(info r);
```

[#]{.pnum} *Returns*: An implementation-defined `string_view` or `u8string_view`, respectively.

[#]{.pnum} *Recommended practice*: Where possible, implementations should return a string suitable for identifying the represented construct.

```cpp
consteval source_location source_location_of(info r);
```

[#]{.pnum} *Returns*: If `r` represents a value, a type other than a class type or an enumeration type, the global namespace, or a data member description, then `source_location{}`. Otherwise, an implementation-defined `source_location` value.

[#]{.pnum} *Recommended practice*: If `r` represents an entity with a definition that is reachable from the evaluation context, a value corresponding to a definition should be returned.
:::
:::

### [meta.reflection.queries] Reflection queries {-}

::: std
::: addu
```cpp
consteval bool $has-type$(info r); // exposition only
```

[#]{.pnum} *Returns*: `true` if  `r` represents a value, object, variable, function whose type does not contain an undeduced placeholder type and that is not a constructor or destructor, enumerator, non-static data member, unnamed bit-field, direct base class relationship, or data member description. Otherwise, `false`.

```cpp
consteval info type_of(info r);
```

[#]{.pnum} *Constant When*: `$has-type$(r)` is `true`.

[#]{.pnum} *Returns*:

- [#.#]{.pnum} If `r` represents a value, object, variable, function, non-static data member, or unnamed bit-field, then the type of what is represented by `r`.
- [#.#]{.pnum} Otherwise, if `r` represents an enumerator `$N$` of an enumeration `$E$`, then:
  - [#.#.#]{.pnum} If `$E$` is defined by a declaration `$D$` that precedes a point `$P$` in the evaluation context and `$P$` does not occur within an `$enum-specifier$` of `$D$`, then a reflection of `$E$`.
  - [#.#.#]{.pnum} Otherwise, a reflection of the type of `$N$` prior to the closing brace of the `$enum-specifier$` as specified in [dcl.enum].
- [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship (`$D$`, `$B$`), then a reflection of `$B$`.
- [#.#]{.pnum} Otherwise, for a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]), a reflection of the type `$T$`.

```cpp
consteval info object_of(info r);
```

[#]{.pnum} *Constant When*: `r` is a reflection representing either

- [#.#]{.pnum} an object with static storage duration ([basic.stc.general]), or
- [#.#]{.pnum} a variable that either declares or refers to such an object, and if that variable is a reference `$R$` then either
  - [#.#.#]{.pnum} `$R$` is usable in constant expressions ([expr.const]), or
  - [#.#.#]{.pnum} the lifetime of `$R$` began within the core constant expression currently under evaluation.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents an object, then `r`.
* [#.#]{.pnum} Otherwise, if `r` represents a reference, then a reflection of the object referred to by that reference.
* [#.#]{.pnum} Otherwise, `r` represents a variable; a reflection of the object declared by that variable.

::: example
```cpp
int x;
int& y = x;

static_assert(^^x != ^^y);                       // OK, x and y are different variables so their
                                                 // reflections compare different
static_assert(object_of(^^x) == object_of(^^y)); // OK, because y is a reference
                                                 // to x, their underlying objects are the same
```
:::

```cpp
consteval info constant_of(info r);
```

[#]{.pnum} Let `$R$` be a constant expression of type `info` such that `$R$ == r` is `true`.

[#]{.pnum} *Constant When*: `[: $R$ :]` is a valid `$splice-expression$` ([expr.prim.splice]).

[#]{.pnum} *Effects*:  Equivalent to:

```cpp
return reflect_constant([: $R$ :]);
```


::: example
```cpp
constexpr int x = 0;
constexpr int y = 0;

static_assert(^^x != ^^y);                              // OK, x and y are different variables so their
                                                        // reflections compare different
static_assert(constant_of(^^x) == constant_of(^^y));    // OK, both constant_of(^^x) and constant_of(^^y)
                                                        // represent the value 0
static_assert(constant_of(^^x) == reflect_constant(0)); // OK, likewise

struct S { int m; };
constexpr S s {42};
static_assert(is_object(constant_of(^^s)) &&
              is_object(reflect_object(s)));
static_assert(constant_of(^^s) != reflect_object(s));   // OK, template parameter object that is
                                                        // template-argument-equivalent to s is a different
                                                        // object than s
static_assert(constant_of(^^s) ==
              constant_of(reflect_object(s)));          // OK

consteval info fn() {
  constexpr int x = 42;
  return ^^x;
}
constexpr info r = constant_of(fn());  // error: x is outside its lifetime
```
:::

```cpp
consteval bool is_public(info r);
consteval bool is_protected(info r);
consteval bool is_private(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents either

- [#.#]{.pnum} a class member or unnamed bit-field that is public, protected, or private, respectively, or
- [#.#]{.pnum} a direct base class relationship (`$D$`, `$B$`) for which `$B$` is, respectively, a public, protected, or private base class of `$D$`.

Otherwise, `false`.

```cpp
consteval bool is_virtual(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents either a virtual member function or a direct base class relationship (`$D$`, `$B$`) for which `$B$` is a virtual base class of `$D$`. Otherwise, `false`.

```cpp
consteval bool is_pure_virtual(info r);
consteval bool is_override(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a member function that is pure virtual or overrides another member function, respectively. Otherwise, `false`.

```cpp
consteval bool is_final(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a final class or a final member function. Otherwise, `false`.

```cpp
consteval bool is_deleted(info r);
consteval bool is_defaulted(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function that is a deleted function ([dcl.fct.def.delete]) or defaulted function ([dcl.fct.def.default]), respectively. Otherwise, `false`.

```cpp
consteval bool is_user_provided(info r);
consteval bool is_user_declared(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function that is user-provided or user-declared ([dcl.fct.def.default]), respectively. Otherwise, `false`.


```cpp
consteval bool is_explicit(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a member function that is declared explicit. Otherwise, `false`. [If `r` represents a member function template that is declared `explicit`, `is_explicit(r)` is still `false` because in general such queries for templates cannot be answered.]{.note}

```cpp
consteval bool is_noexcept(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a `noexcept` function type or a function with a non-throwing exception specification ([except.spec]). Otherwise, `false`. [If `r` represents a function template that is declared `noexcept`, `is_noexcept(r)` is still `false` because in general such queries for templates cannot be answered.]{.note}

```cpp
consteval bool is_bit_field(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a bit-field, or if `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) for which `$W$` is not ⊥. Otherwise, `false`.

```cpp
consteval bool is_enumerator(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an enumerator. Otherwise, `false`.

```cpp
consteval bool is_const(info r);
consteval bool is_volatile(info r);
```

[#]{.pnum} Let `$T$` be `type_of(r)` if `$has-type$(r)` is `true`. Otherwise, let `$T$` be `dealias(r)`.

[#]{.pnum} *Returns*: `true` if `$T$` represents a const or volatile type, respectively, or a const- or volatile-qualified function type, respectively. Otherwise, `false`.

```cpp
consteval bool is_mutable_member(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a `mutable` non-static data member. Otherwise, `false`.

```cpp
consteval bool is_lvalue_reference_qualified(info r);
consteval bool is_rvalue_reference_qualified(info r);
```

[#]{.pnum} Let `$T$` be `type_of(r)` if `$has-type$(r)` is `true`. Otherwise, let `$T$` be `dealias(r)`.

[#]{.pnum} *Returns*: `true` if `$T$` represents a lvalue- or rvalue-reference qualified function type, respectively. Otherwise, `false`.

```cpp
consteval bool has_static_storage_duration(info r);
consteval bool has_thread_storage_duration(info r);
consteval bool has_automatic_storage_duration(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an object or variable that has static, thread, or automatic storage duration, respectively ([basic.stc]). Otherwise, `false`. [It is not possible to have a reflection representing an object or variable having dynamic storage duration.]{.note}

```cpp
consteval bool has_internal_linkage(info r);
consteval bool has_module_linkage(info r);
consteval bool has_external_linkage(info r);
consteval bool has_c_language_linkage(info r);
consteval bool has_linkage(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a variable, function, type, template, or namespace whose name has internal linkage, module linkage, external linkage, C language linkage, or any linkage, respectively ([basic.link]). Otherwise, `false`.

```cpp
consteval bool is_complete_type(info r);
```

[#]{.pnum} *Returns*: `true` if `is_type(r)` is `true` and there is some point in the evaluation context from which the type represented by `dealias(r)` is not an incomplete type ([basic.types]). Otherwise, `false`.

```cpp
consteval bool is_enumerable_type(info r);
```

[#]{.pnum} A type `$T$` is _enumerable_ from a point `$P$` if either

  - [#.#]{.pnum} `$T$` is a class type complete at `$P$` or
  - [#.#]{.pnum} `$T$` is an enumeration type defined by a declaration `$D$` such that `$D$` is reachable from `$P$` but `$P$` does not occur within an `$enum-specifier$` of `$D$` ([dcl.enum]).

[#]{.pnum} *Returns*: `true` if `dealias(r)` represents a type that is enumerable from some point in the evaluation context. Otherwise, `false`.

::: example
```cpp
class S;
enum class E;
static_assert(!is_enumerable_type(^^S));
static_assert(!is_enumerable_type(^^E));

class S {
  void mfn() {
    static_assert(is_enumerable_type(^^S));
  }
  static_assert(!is_enumerable_type(^^S));
};
static_assert(is_enumerable_type(^^S));

enum class E {
  A = is_enumerable_type(^^E) ? 1 : 2
};
static_assert(is_enumerable_type(^^E));
static_assert(static_cast<int>(E::A) == 2);
```
:::

```cpp
consteval bool is_variable(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a variable. Otherwise, `false`.

```cpp
consteval bool is_type(info r);
consteval bool is_namespace(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an entity whose underlying entity is a type or namespace, respectively. Otherwise, `false`.

```cpp
consteval bool is_type_alias(info r);
consteval bool is_namespace_alias(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a type alias or namespace alias, respectively [A specialization of an alias template is a type alias]{.note}. Otherwise, `false`.

```cpp
consteval bool is_function(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a function. Otherwise, `false`.

```cpp
consteval bool is_conversion_function(info r);
consteval bool is_operator_function(info r);
consteval bool is_literal_operator(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function that is a conversion function ([class.conv.fct]), operator function ([over.oper]), or literal operator ([over.literal]), respectively. Otherwise, `false`.

```cpp
consteval bool is_special_member_function(info r);
consteval bool is_constructor(info r);
consteval bool is_default_constructor(info r);
consteval bool is_copy_constructor(info r);
consteval bool is_move_constructor(info r);
consteval bool is_assignment(info r);
consteval bool is_copy_assignment(info r);
consteval bool is_move_assignment(info r);
consteval bool is_destructor(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function that is a special member function ([special]), a constructor, a default constructor, a copy constructor, a move constructor, an assignment operator, a copy assignment operator, a move assignment operator, or a destructor, respectively. Otherwise, `false`.

```cpp
consteval bool is_template(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a function template, class template, variable template, alias template, or concept. Otherwise, `false`.

[#]{.pnum} [A template specialization is not a template. `is_template(^^std::vector)` is `true` but `is_template(^^std::vector<int>)` is `false`.]{.note}

```cpp
consteval bool is_function_template(info r);
consteval bool is_variable_template(info r);
consteval bool is_class_template(info r);
consteval bool is_alias_template(info r);
consteval bool is_conversion_function_template(info r);
consteval bool is_operator_function_template(info r);
consteval bool is_literal_operator_template(info r);
consteval bool is_constructor_template(info r);
consteval bool is_concept(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function template, variable template, class template, alias template, conversion function template, operator function template, literal operator template, constructor template, or concept respectively. Otherwise, `false`.

```cpp
consteval bool is_value(info r);
consteval bool is_object(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a value or object, respectively. Otherwise, `false`.

```cpp
consteval bool is_structured_binding(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a structured binding. Otherwise, `false`.



```cpp
consteval bool is_class_member(info r);
consteval bool is_namespace_member(info r);
consteval bool is_nonstatic_data_member(info r);
consteval bool is_static_member(info r);
consteval bool is_base(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a class member, namespace member, non-static data member, static member, or direct base class relationship, respectively. Otherwise, `false`.

```cpp
consteval bool has_default_member_initializer(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a non-static data member that has a default member initializer. Otherwise, `false`.

```cpp
consteval bool has_parent(info r);
```

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents the global namespace, then `false`.
* [#.#]{.pnum} Otherwise, if `r` represents an entity that has C language linkage ([dcl.link]), then `false`.
* [#.#]{.pnum} Otherwise, if `r` represents an entity that has a language linkage other than C++ language linkage, then an implementation-defined value.
* [#.#]{.pnum} Otherwise, if `r` represents a type that is neither a class nor enumeration type, then `false`.
* [#.#]{.pnum} Otherwise, if `r` represents an entity or direct base class relationship, then `true`.
* [#.#]{.pnum} Otherwise, `false`.

```cpp
consteval info parent_of(info r);
```

[#]{.pnum} *Constant When*: `has_parent(r)` is `true`.

[#]{.pnum} *Returns*:

- [#.#]{.pnum} If `r` represents a non-static data member that is a direct member of an anonymous union, or an unnamed bit-field declared within the `$member-specification$` of such a union, then a reflection representing the innermost enclosing anonymous union.
- [#.#]{.pnum} Otherwise, if `r` represents an enumerator, then a reflection representing the corresponding enumeration type.
- [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship (`$D$`, `$B$`), then a reflection representing `$D$`.
- [#.#]{.pnum} Otherwise, let `$E$` be the class, function, or namespace whose class scope, function parameter scope, or namespace scope, respectively, is the innermost such scope that either is, or encloses, the target scope of a declaration of what is represented by `r`.
  - [#.#]{.pnum} If `$E$` is the function call operator of a closure type for a `$consteval-block-declaration$` ([dcl.pre]), then `parent_of(parent_of(^^$E$))`. [In this case, the first `parent_of` will be the closure type, so the second `parent_of` is necessary to give the parent of that closure type.]{.note}
  - [#.#]{.pnum} Otherwise, `^^$E$`.

::: example
```cpp
struct I { };

struct F : I {
  union {
    int o;
  };

  enum N {
    A
  };
};

constexpr auto ctx = std::meta::access_context::current();

static_assert(parent_of(^^F) == ^^::);
static_assert(parent_of(bases_of(^^F, ctx)[0]) == ^^F);
static_assert(is_union_type(parent_of(^^F::o)));
static_assert(parent_of(^^F::N) == ^^F);
static_assert(parent_of(^^F::A) == ^^F::N);
```
:::

```cpp
consteval info dealias(info r);
```

[#]{.pnum} *Constant When*: `r` represents an entity.

[#]{.pnum} *Returns*: A reflection representing the underlying entity of what `r` represents.

[#]{.pnum}

::: example
```
using X = int;
using Y = X;
static_assert(dealias(^^int) == ^^int);
static_assert(dealias(^^X) == ^^int);
static_assert(dealias(^^Y) == ^^int);
```
:::

```cpp
consteval bool has_template_arguments(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a specialization of a function template, variable template, class template, or an alias template. Otherwise, `false`.

```cpp
consteval info template_of(info r);
```

[#]{.pnum} *Constant When*: `has_template_arguments(r)` is `true`.

[#]{.pnum} *Returns*: A reflection of the template of the specialization represented by `r`.

```cpp
consteval vector<info> template_arguments_of(info r);
```
[#]{.pnum} *Constant When*: `has_template_arguments(r)` is `true`.

[#]{.pnum} *Returns*: A `vector` containing reflections of the template arguments of the template specialization represented by `r`, in the order they appear in the corresponding template argument list. For a given template argument `$A$`, its corresponding reflection `$R$` is determined as follows:

* [#.#]{.pnum} If `$A$` denotes a type or a type alias, then `$R$` is a reflection representing the underlying entity of `$A$`. [`$R$` always represents a type, never a type alias.]{.note}
* [#.#]{.pnum} Otherwise, if `$A$` denotes a class template, variable template, concept, or alias template, then `$R$` is a reflection representing `$A$`.
* [#.#]{.pnum} Otherwise, `$A$` is a constant template argument ([temp.arg.nontype]). Let `$P$` be the corresponding template parameter.

  * [#.#.#]{.pnum} If `$P$` has reference type, then `$R$` is a reflection representing the object or function referred to by `$A$`.
  * [#.#.#]{.pnum} Otherwise, if `$P$` has class type, then `$R$` represents the corresponding template parameter object.
  * [#.#.#]{.pnum} Otherwise, `$R$` is a reflection representing the value computed by `$A$`.

::: example
```
template <class T, class U=T> struct Pair { };
template <class T> struct Pair<char, T> { };
template <class T> using PairPtr = Pair<T*>;

static_assert(template_of(^^Pair<int>) == ^^Pair);
static_assert(template_of(^^Pair<char, char>) == ^^Pair);
static_assert(template_arguments_of(^^Pair<int>).size() == 2);
static_assert(template_arguments_of(^^Pair<int>)[0] == ^^int);

static_assert(template_of(^^PairPtr<int>) == ^^PairPtr);
static_assert(template_arguments_of(^^PairPtr<int>).size() == 1);

struct S { };
int i;
template <int, int&, S, template <class> class>
struct X { };
constexpr auto T = ^^X<1, i, S{}, PairPtr>;
static_assert(is_value(template_arguments_of(T)[0]));
static_assert(is_object(template_arguments_of(T)[1]));
static_assert(is_object(template_arguments_of(T)[2]));
static_assert(template_arguments_of(T)[3] == ^^PairPtr);
```
:::
:::
:::


### [meta.reflection.access.context] Access control context {-}

::: std
::: addu
[1]{.pnum} The `access_context` class is a non-aggregate type that represents a namespace, class, or function from which queries pertaining to access rules may be performed, as well as the designating class ([class.access.base]), if any.

[#]{.pnum} An `access_context` has an associated scope and designating class.

```cpp
struct access_context {
   access_context() = delete;

   consteval info scope() const;
   consteval info designating_class() const;

   static consteval access_context current() noexcept;
   static consteval access_context unprivileged() noexcept;
   static consteval access_context unchecked() noexcept;
   consteval access_context via(info cls) const;
};
```

[#]{.pnum} `access_context` is a structural type. Two values `ac1` and `ac2` of type `access_context` are template-argument-equivalent ([temp.type]) if `ac1.scope()` and `ac2.scope()` are template-argument-equivalent and `ac1.designating_class()` and `ac2.designating_class()` are template-argument-equivalent.

```cpp
consteval info scope() const;
consteval info designating_class() const;
```

[#]{.pnum} *Returns*: The `access_context`'s associated scope and designating class, respectively.

```cpp
static consteval access_context current() noexcept;
```

[#]{.pnum} `current` is not an addressable function ([namespace.std]).

[#]{.pnum} Given a program point `$P$`, let `$eval-point$($P$)` be the following program point:

* [#.#]{.pnum} If a potentially-evaluated subexpression ([intro.execution]) of a default member initializer `$I$` for a member of a class `$C$` ([class.mem.general]) appears at `$P$`, then a point determined as follows:
  * [#.#.#]{.pnum} If an aggregate initialization is using `$I$`, `$eval-point$($Q$)`, where `$Q$` is the point at which that aggregate initialization appears.
  * [#.#.#]{.pnum} Otherwise, if an initialization by an inherited constructor ([class.inhctor.init]) is using `$I$`, a point whose immediate scope is the class scope corresponding to `$C$`.
  * [#.#.#]{.pnum} Otherwise, a point whose immediate scope is the function parameter scope corresponding to the constructor definition that is using `$I$`.
* [#.#]{.pnum} Otherwise, if a potentially-evaluated subexpression of a default argument ([dcl.fct.default]) appears at `$P$`, `$eval-point$($Q$)`, where `$Q$` is the point at which the invocation of the function ([expr.call]) using that default argument appears.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a declaration `$D$`, and `$P$` appears either before the locus of `$D$` or within the trailing `$requires-clause$` of `$D$`, a point whose immediate scope is the innermost scope enclosing the locus of `$D$` that is not a template parameter scope.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a `$lambda-expression$` `$L$` whose `$lambda-introducer$` appears at point `$Q$`, and `$P$` appears either within the `$trailing-return-type$` or the trailing `$requires-clause$` of `$L$`, `$eval-point$($Q$)`.
* [#.#]{.pnum} Otherwise, if the innermost non-block scope enclosing `$P$` is the function parameter scope introduced by a `$consteval-block-declaration$` ([dcl.pre]), a point whose immediate scope is that inhabited by the outermost `$consteval-block-declaration$` `$D$` containing `$P$` such that each scope (if any) that intervenes between `$P$` and the function parameter scope introduced by `$D$` is either
  * [#.#.#]{.pnum} a block scope or
  * [#.#.#]{.pnum} a function parameter scope or lambda scope introduced by a `$consteval-block-declaration$`.
* [#.#]{.pnum} Otherwise, `$P$`.

[#]{.pnum} Given a scope `$S$`, let `$ctx-scope$($S$)` be the following scope:

* [#.#]{.pnum} If `$S$` is a class scope or a namespace scope, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a function parameter scope introduced by the declaration of a function, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a lambda scope introduced by a `$lambda-expression$` `$L$`, the function parameter scope corresponding to the call operator of the closure type for `$L$`.
* [#.#]{.pnum} Otherwise, `$ctx-scope$($S$')` where `$S$'` is the parent scope of `$S$`.

[#]{.pnum} An invocation of `current` that appears at a program point `$P$` is value-dependent ([temp.dep.contexpr]) if `$eval-point$($P$)` is enclosed by a scope corresponding to a templated entity.

[#]{.pnum} *Returns*: An `access_context` whose designating class is the null reflection and whose scope represents the function, class, or namespace whose corresponding function parameter scope, class scope, or namespace scope is `$ctx-scope$($S$)`, where `$S$` is the immediate scope of `$eval-point$($P$)` and `$P$` is the point at which the invocation of `current` lexically appears.


::: example
```cpp
struct A {
  int a = 0;
  consteval A(int p) : a(p) {}
};
struct B : A {
  using A::A;
  consteval B(int p, int q) : A(p * q) {}
  info s = access_context::current().scope();
};
struct C : B { using B::B; };

struct Agg {
  consteval bool eq(info rhs = access_context::current().scope()) {
    return s == rhs;
  }
  info s = access_context::current().scope();
};

namespace NS {
static_assert(Agg{}.s == access_context::current().scope());  // OK
static_assert(Agg{}.eq());  // OK
static_assert(B(1).s == ^^B);  // OK
static_assert(is_constructor(B{1, 2}.s) && parent_of(B{1, 2}.s) == ^^B);  // OK
static_assert(is_constructor(C{1, 2}.s) && parent_of(C{1, 2}.s) == ^^B);  // OK

auto fn() -> [:is_namespace(access_context::current().scope()) ? ^^int : ^^bool:];
static_assert(type_of(^^fn) == ^^auto()->int);  // OK

template <auto R>
struct TCls {
  consteval bool fn()
    requires (is_type(access_context::current().scope())) {
      // OK, scope is 'TCls<R>'.
      return true;
    }
};
static_assert(TCls<0>{}.fn());  // OK
}
```
:::

```cpp
static consteval access_context unprivileged() noexcept;
```

[#]{.pnum} *Returns*: An `access_context` whose designating class is the null reflection and whose scope is the global namespace.

```cpp
static consteval access_context unchecked() noexcept;
```

[#]{.pnum} *Returns*: An `access_context` whose designating class and scope are both the null reflection.

```cpp
consteval access_context via(info cls) const;
```
[#]{.pnum} *Constant When*: `cls` is either the null reflection or a reflection of a complete class type.

[#]{.pnum} *Returns*: An `access_context` whose scope is `this->scope()` and whose designating class is `cls`.

:::
:::

### [meta.reflection.access.queries] Member accessibility queries {-}

::: std
::: addu
```cpp
consteval bool is_accessible(info r, access_context ctx);
```

[#]{.pnum} Let `$PARENT-CLS$(r)` be:

- [#.#]{.pnum} If `parent_of(r)` represents a class `$C$`, then `$C$`.
- [#.#]{.pnum} Otherwise, `$PARENT-CLS$(parent_of(r))`.

[#]{.pnum} *Constant When*:

* [#.#]{.pnum} `r` does not represent a class member for which `$PARENT-CLS$(r)` is an incomplete class and
* [#.#]{.pnum} `r` does not represent a direct base class relationship (`$D$`, `$B$`) for which `$D$` is incomplete.

[#]{.pnum} Let `$DESIGNATING-CLS$(r, ctx)` be:

* [#.#]{.pnum} If `ctx.designating_class()` represents a class `$C$`, then `$C$`.
* [#.#]{.pnum} Otherwise, `$PARENT-CLS$(r)`.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents an unnamed bit-field `$F$`, then `is_accessible(r@~$H$~@, ctx)` where `r@~$H$~@` represents a hypothetical non-static data member of the class represented by `$PARENT-CLS$(r)` with the same access as `$F$`. [Unnamed bit-fields are treated as class members for the purpose of `is_accessible`.]{.note}
* [#.#]{.pnum} Otherwise, if `r` does not represent a class member or a direct base class relationship, then `true`.
* [#.#]{.pnum} Otherwise, if `r` represents
  * [#.#.#]{.pnum} a class member that is not a (possibly indirect or variant) member of `$DESIGNATING-CLS$(r, ctx)` or
  * [#.#.#]{.pnum} a direct base class relationship such that `parent_of(r)` does not represent `$DESIGNATING-CLS$(r, ctx)` or a (direct or indirect) base class thereof,

  then `false`.
* [#.#]{.pnum} Otherwise, if `ctx.scope()` is the null reflection, then `true`.

* [#.#]{.pnum} Otherwise, letting `$P$` be a program point whose immediate scope is the function parameter scope, class scope, or namespace scope corresponding to the function, class, or namespace represented by `ctx.scope()`:
  * [#.#.#]{.pnum} If `r` represents a direct base class relationship (`$D$`, `$B$`), then `true` if base class `$B$` of `$DESIGNATING-CLS$(r, ctx)` is accessible at `$P$` ([class.access.base]); otherwise, `false`.
  * [#.#.#]{.pnum} Otherwise, `r` represents a class member `$M$`; `true` if `$M$` would be accessible at `$P$` with the designating class ([class.access.base]) as `$DESIGNATING-CLS$(r, ctx)` if the effect of any `$using-declaration$`s ([namespace.udecl]) were ignored. Otherwise, `false`.

::: note
The definitions of when a class member or base class is accessible from a point `$P$` do not consider whether a declaration of that entity is reachable from `$P$`.
:::

::: example
```cpp
consteval access_context fn() {
  return access_context::current();
}

class Cls {
    int mem;
    friend consteval access_context fn();
public:
    static constexpr auto r = ^^mem;
};

static_assert(is_accessible(Cls::r, fn()));                        // OK
static_assert(!is_accessible(Cls::r, access_context::current()));  // OK
static_assert(is_accessible(Cls::r, access_context::unchecked())); // OK
```
:::

```cpp
consteval bool has_inaccessible_nonstatic_data_members(
      info r,
      access_context ctx);
```

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `nonstatic_data_members_of(r, access_context::unchecked())` is a constant subexpression and
- [#.#]{.pnum} `r` does not represent a closure type.

[#]{.pnum} *Returns*: `true` if `is_accessible($R$, ctx)` is `false` for any `$R$` in `nonstatic_data_members_of(r, access_context::unchecked())`. Otherwise, `false`.

```cpp
consteval bool has_inaccessible_bases(info r, access_context ctx);
```

[#]{.pnum} *Constant When*: `bases_of(r, access_context::unchecked())` is a constant subexpression.

[#]{.pnum} *Returns*: `true` if `is_accessible($R$, ctx)` is `false` for any `$R$` in `bases_of(r, access_context::unchecked())`. Otherwise, `false`.

:::
:::

### [meta.reflection.member.queries] Reflection member queries  {-}

::: std
::: addu
```cpp
consteval vector<info> members_of(info r, access_context ctx);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection representing either a class type that is complete from some point in the evaluation context or a namespace.

[#]{.pnum} A declaration `$D$` _members-of-precedes_ a point `$P$` if `$D$` precedes either `$P$` or the point immediately following the `$class-specifier$` of the outermost class for which `$P$` is in a complete-class context.

[#]{.pnum} A declaration `$D$` of a member `$M$` of a class or namespace `$Q$` is _`$Q$`-members-of-eligible_ if

* [#.#]{.pnum} the host scope of `$D$` ([basic.scope.scope]) is the class scope or namespace scope associated with `$Q$`,
* [#.#]{.pnum} `$D$` is not a friend declaration,
* [#.#]{.pnum} `$M$` is not a closure type ([expr.prim.lambda.closure]),
* [#.#]{.pnum} `$M$` is not a specialization of a template ([temp.pre]),
* [#.#]{.pnum} if `$Q$` is a class that is not a closure type, then `$M$` is a direct member of `$Q$` ([class.mem.general]) that is not a variant member of a nested anonymous union of `$Q$` ([class.union.anon]), and
* [#.#]{.pnum} if `$Q$` is a closure type, then `$M$` is a function call operator or function call operator template.

It is implementation-defined whether declarations of other members of a closure type `$Q$` are `$Q$`-members-of-eligible.

[#]{.pnum} A member `$M$` of a class or namespace `$Q$` is _`$Q$`-members-of-representable_ from a point `$P$` if a `$Q$`-members-of-eligible declaration of `$M$` members-of-precedes `$P$` and `$M$` is

* [#.#]{.pnum} a class or enumeration type,
* [#.#]{.pnum} a type alias,
* [#.#]{.pnum} a class template, function template, variable template, alias template, or concept,
* [#.#]{.pnum} a variable or reference `$V$` for which the type of `$V$` does not contain an undeduced placeholder type,
* [#.#]{.pnum} a function `$F$` for which
  * [#.#]{.pnum} the type of `$F$` does not contain an undeduced placeholder type,
  * [#.#]{.pnum} the constraints (if any) of `$F$` are satisfied, and
  * [#.#]{.pnum} if `$F$` is a prospective destructor, `$F$` is the selected destructor ([class.dtor]),
* [#.#]{.pnum} a non-static data member,
* [#.#]{.pnum} a namespace, or
* [#.#]{.pnum} a namespace alias.

[Examples of direct members that are not `$Q$`-members-of-representable for any entity `$Q$` include: unscoped enumerators ([enum]), partial specializations of templates ([temp.spec.partial]), and closure types ([expr.prim.lambda.closure]).]{.note}

[#]{.pnum} *Returns*: A `vector` containing reflections of all members `$M$` of the entity `$Q$` represented by `dealias(r)` for which

* [#.#]{.pnum} `$M$` is `$Q$`-members-of-representable from some point in the evaluation context and
* [#.#]{.pnum} `is_accessible(^^$M$, ctx)` is `true`.

If `dealias(r)` represents a class `$C$`, then the `vector` also contains reflections representing all unnamed bit-fields `$B$` whose declarations inhabit the class scope corresponding to `$C$` for which `is_accessible(^^$B$, ctx)` is `true`. Reflections of class members and unnamed bit-fields that are declared appear in the order in which they are declared. [Base classes are not members. Implicitly-declared special members appear after any user-declared members ([special]).]{.note}

::: example
```cpp
// TU1
export module M;
namespace NS {
  export int m;
  static int l;
}
static_assert(members_of(^^NS, access_context::current()).size() == 2);

// TU2
import M;

static_assert(members_of(^^NS, access_context::current()).size() == 1);
  // NS::l does not precede the constant-expression ([basic.lookup])

class B {};

struct S : B {
private:
  class I;
public:
  int m;
};

static_assert(members_of(^^S, access_context::current()).size() == 7);    // 6 special members, 1 public member, does not include base
static_assert(members_of(^^S, access_context::unchecked()).size() == 8);  // all of the above, as well a reflection representing S::I
```
:::

```cpp
consteval vector<info> bases_of(info type, access_context ctx);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a class type that is complete from some point in the evaluation context.

[#]{.pnum} *Returns*: Let `$C$` be the class represented by `dealias(type)`. A `vector` containing the reflections of all the direct base class relationships `$B$`, if any, of `$C$` such that `is_accessible(^^$B$, ctx)` is `true`.
The direct base class relationships appear in the order in which the corresponding base classes appear in the `$base-specifier-list$` of `$C$`.

```cpp
consteval vector<info> static_data_members_of(info type, access_context ctx);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a class type that is complete from some point in the evaluation context.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `members_of(type, ctx)` such that `is_variable(e)` is `true`, preserving their order.

```cpp
consteval vector<info> nonstatic_data_members_of(info type, access_context ctx);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a class type that is complete from some point in the evaluation context.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `members_of(type, ctx)` such that `is_nonstatic_data_member(e)` is `true`, preserving their order.

```cpp
consteval vector<info> enumerators_of(info type_enum);
```

[#]{.pnum} *Constant When*: `dealias(type_enum)` represents an enumeration type and `is_enumerable_type(type_enum)` is `true`.

[#]{.pnum} *Returns*: A `vector` containing the reflections of each enumerator of the enumeration represented by `dealias(type_enum)`, in the order in which they are declared.

:::
:::


### [meta.reflection.layout] Reflection layout queries {-}

::: std
::: addu
```cpp
struct member_offset {
  ptrdiff_t bytes;
  ptrdiff_t bits;
  constexpr ptrdiff_t total_bits() const;
  auto operator<=>(const member_offset&) const = default;
};

constexpr ptrdiff_t member_offset::total_bits() const;
```
[#]{.pnum} *Returns*: `bytes * CHAR_BIT + bits`.

```cpp
consteval member_offset offset_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents a non-static data member, unnamed bit-field, or direct base class relationship (`$D$`, `$B$`) for which either `$B$` is not a virtual base class or `$D$` is not an abstract class.

[#]{.pnum} Let `$V$` be the offset in bits from the beginning of a complete object of type `parent_of(r)` to the subobject associated with the entity represented by `r`.

[#]{.pnum} *Returns*: `{$V$ / CHAR_BIT, $V$ % CHAR_BIT}`.

```cpp
consteval size_t size_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member that is not a bit-field, direct base class relationship, or data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) where `$W$` is not ⊥. If `dealias(r)` represents a type, then `is_complete_type(r)` is `true`.

[#]{.pnum} *Returns*: If `r` represents

- [#.#]{.pnum} a non-static data member of type `$T$`,
- [#.#]{.pnum} a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`), or
- [#.#]{.pnum} `dealias(r)` represents a type `$T$`,

then `sizeof($T$)` if `$T$` is not a reference type and `size_of(add_pointer(^^$T$))` otherwise.  Otherwise, `size_of(type_of(r))`.

[It is possible that while `sizeof(char) == size_of(^^char)` that `sizeof(char&) != size_of(^^char&)`. If `b` represents a direct base class relationship (`$D$`, `$B$`) for which `$B$` is an empty class type, then `size_of(b) > 0`.]{.note}

```cpp
consteval size_t alignment_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection of a type, object, variable of non-reference type, non-static data member that is not a bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type, then `is_complete_type(r)` is `true`.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `dealias(r)` represents a type `$T$`, then `alignment_of(add_pointer(r))` if `$T$` is a reference type and the alignment requirement of `$T$` otherwise.
* [#.#]{.pnum} Otherwise, if `dealias(r)` represents a variable or object, then the alignment requirement of the variable or object.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `alignment_of(type_of(r))`.
* [#.#]{.pnum} Otherwise, if `r` represents a non-static data member `$M$` of a class `$C$`, then the alignment of the direct member subobject corresponding to `$M$` of a complete object of type `$C$`.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]). If `$A$` is not ⊥, then the value `$A$`. Otherwise, `alignment_of(^^$T$)`.

```cpp
consteval size_t bit_size_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member, unnamed bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents a non-static data member that is a bit-field or an unnamed bit-field with width `$W$`, then `$W$`.
* [#.#]{.pnum} Otherwise, if `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) and `$W$` is not ⊥, then `$W$`.
* [#.#]{.pnum} Otherwise, `CHAR_BIT * size_of(r)`.
:::
:::


### [meta.reflection.extract] Value extraction {-}

::: std
::: addu
[1]{.pnum} The `extract` function template may be used to extract a value out of a reflection when its type is known.

[#]{.pnum} The following are defined for exposition only to aid in the specification of `extract`:
```cpp
template <class T>
  consteval T $extract-ref$(info r); // exposition only
```

[#]{.pnum} [`T` is a reference type.]{.note}

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `r` represents a variable or object of type `U`,
- [#.#]{.pnum} `is_convertible_v<remove_reference_t<U>(*)[], remove_reference_t<T>(*)[]>` is `true`, and [The intent is to allow only qualification conversions from `U` to `T`.]{.note}
- [#.#]{.pnum} if `r` represents a variable, then either that variable is usable in constant expressions or its lifetime began within the core constant expression currently under evaluation.

[#]{.pnum} *Returns*: If `r` represents an object `$O$`, then a reference to `$O$`. Otherwise, a reference to the object declared, or referred to, by the variable represented by `r`.

```cpp
template <class T>
  consteval T $extract-member-or-function$(info r); // exposition only
```

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `r` represents a non-static data member with type `X`, that is not a bit-field, that is a direct member of a class `C`, `T` and `X C::*` are similar types ([conv.qual]), and `is_convertible_v<X C::*, T>` is `true`;
- [#.#]{.pnum} `r` represents an implicit object member function with type `F` or `F noexcept` that is a direct member of a class `C` and `T` is `F C::*`; or
- [#.#]{.pnum} `r` represents a non-member function, static member function, or explicit object member function of function type `F` or `F noexcept` and `T` is `F*`.

[#]{.pnum} *Returns*:

- [#.#]{.pnum} If `T` is a pointer type, then a pointer value pointing to the function represented by `r`.
- [#.#]{.pnum} Otherwise, a pointer-to-member value designating the non-static data member or function represented by `r`.

```cpp
template <class T>
  consteval T $extract-value$(info r); // exposition only
```

[#]{.pnum} Let `U` be the type of the value or object that `r` represents.

[#]{.pnum} *Constant When*:

  - [#.#]{.pnum} `U` is a pointer type, `T` and `U` are either similar or both function pointer types, and `is_convertible_v<U, T>` is `true`,
  - [#.#]{.pnum} `U` is not a pointer type and the cv-unqualified types of `T` and `U` are the same,
  - [#.#]{.pnum} `U` is an array type, `T` is a pointer type, and the value that `r` represents is convertible to `T`, or
  - [#.#]{.pnum} `U` is a closure type, `T` is a function pointer type, and the value that `r` represents is convertible to `T`.

[#]{.pnum} *Returns*: `static_cast<T>([:$R$:])`, where `$R$` is a constant expression of type `info` such that `$R$ == r` is `true`.

```cpp
template <class T>
  consteval T extract(info r);
```

[#]{.pnum} *Effects*: Let `U` be `remove_cv_t<T>`. Equivalent to:

```cpp
if constexpr (is_reference_type(^^T)) {
  return $extract-ref$<T>(r);
} else if constexpr (is_nonstatic_data_member(r) || is_function(r)) {
  return $extract-member-or-function$<U>(r);
} else {
  return $extract-value$<U>(constant_of(r));
}
```

:::
:::

### [meta.reflection.substitute] Reflection substitution  {-}

::: std
::: addu
```cpp
template <class R>
concept reflection_range =
  ranges::input_range<R> &&
  same_as<ranges::range_value_t<R>, info> &&
  same_as<remove_cvref_t<ranges::range_reference_t<R>>, info>;
```

```cpp
template <reflection_range R = initializer_list<info>>
consteval bool can_substitute(info templ, R&& arguments);
```
[1]{.pnum} *Constant When*: `templ` represents a template and every reflection in `arguments` represents a construct usable as a template argument ([temp.arg]).

[#]{.pnum} Let `Z` be the template represented by `templ` and let `Args...` be a sequence of prvalue constant expressions that compute the reflections held by the elements of `arguments`, in order.

[#]{.pnum} *Returns*: `true` if `Z<[:Args:]...>` is a valid `$template-id$` ([temp.names]) that does not name a function whose type contains an undeduced placeholder type. Otherwise, `false`.

[#]{.pnum} [If forming `Z<[:Args:]...>` leads to a failure outside of the immediate context, the program is ill-formed.]{.note}

```cpp
template <reflection_range R = initializer_list<info>>
consteval info substitute(info templ, R&& arguments);
```

[#]{.pnum} *Constant When*: `can_substitute(templ, arguments)` is `true`.

[#]{.pnum} Let `Z` be the template represented by `templ` and let `Args...` be a sequence of prvalue constant expressions that compute the reflections held by the elements of `arguments`, in order.

[#]{.pnum} *Returns*: `^^Z<[:Args:]...>`.

[#]{.pnum} [If forming `Z<[:Args:]...>` leads to a failure outside of the immediate context, the program is ill-formed.]{.note}

[#]{.pnum}

::: example
```cpp
template <typename T>
auto fn1();

static_assert(!can_substitute(^^fn1, {^^int}));  // OK
constexpr info r1 = substitute(^^fn1, {^^int});
  // error: fn<int> contains an undeduced placeholder type

template <typename T>
auto fn2() {
  static_assert(^^T != ^^int);
    // static assertion failed during instantiation of fn<int>
  return 0;
}

constexpr bool r2 = can_substitute(^^fn2, {^^int});
  // error: instantiation of body of fn<int> is needed to deduce return type
```
:::

[#]{.pnum}

::: example
```cpp
consteval info to_integral_constant(unsigned i) {
  return substitute(^^integral_constant, {^^unsigned, reflect_constant(i)});
}
constexpr info r = to_integral_constant(2);
  // OK, r represents the type integral_constant<unsigned, 2>
```
:::
:::
:::

### [meta.reflection.result] Expression result reflection {-}

::: std
::: addu
```cpp
template <typename T>
  consteval info reflect_constant(T expr);
```

[#]{.pnum} *Mandates*: `is_copy_constructible_v<T>` is `true` and `T` is a cv-unqualified structural type ([temp.param]) that is not a reference type.

[#]{.pnum} Let `$V$` be:

* [#.#]{.pnum} if `T` is a class type, then an object that is template-argument-equivalent to the value of `expr`;
* [#.#]{.pnum} otherwise, the value of `expr`.

[#]{.pnum} *Constant When*: Given the invented template

```cpp
template <T P> struct TCls;
```

the `$template-id$` `TCls<$V$>` would be valid.

[#]{.pnum} *Returns*: `template_arguments_of(^^TCls<$V$>)[0]`. [This is a reflection of an object for class types and a reflection of a value otherwise.]{.note}

::: example
```cpp
template <auto D>
struct A { };

struct N { int x; };
struct K { char const* p; };

constexpr info r1 = reflect_constant(42);
static_assert(is_value(r1));
static_assert(r1 == template_arguments_of(^^A<42>)[0]);

constexpr info r2 = reflect_constant(N{42});
static_assert(is_object(r2));
static_assert(r2 == template_arguments_of(^^A<N{42}>)[0]);

constexpr info r3 = reflect_constant(K{nullptr}); // ok
constexpr info r4 = reflect_constant(K{"ebab"});  // error: constituent pointer points to string literal
```
:::

```cpp
template <typename T>
  consteval info reflect_object(T& expr);
```

[#]{.pnum} *Mandates*: `T` is an object type.

[#]{.pnum} *Constant When*: `expr` is suitable for use as a constant template argument for a constant template parameter of type `T&` ([temp.arg.nontype]).

[#]{.pnum} *Returns*: A reflection of the object designated by `expr`.

```cpp
template <typename T>
  consteval info reflect_function(T& fn);
```

[#]{.pnum} *Mandates*: `T` is a function type.

[#]{.pnum} *Constant When*: `fn` is suitable for use as a constant template argument for a constant template parameter of type `T&` ([temp.arg.nontype]).

[#]{.pnum} *Returns*: A reflection of the function designated by `fn`.
:::
:::

### [meta.reflection.define.aggregate] Reflection class definition generation  {-}

::: std
::: addu

```cpp
struct data_member_options {
  struct $name-type$ { // exposition only
    template<class T> requires constructible_from<u8string, T>
      consteval $name-type$(T &&);

    template<class T> requires constructible_from<string, T>
      consteval $name-type$(T &&);

  private:
    variant<u8string, string> $contents$;    // exposition only
  };

  optional<$name-type$> name;
  optional<int> alignment;
  optional<int> bit_width;
  bool no_unique_address = false;
};
```

[1]{.pnum} The classes `data_member_options` and `data_member_options::$name-type$` are consteval-only types ([basic.types.general]), and are not structural types ([temp.param]).

```cpp
template <class T> requires constructible_from<u8string, T>
consteval data_member_options::$name-type$(T&& value);
```

[#]{.pnum} *Effects*: Initializes `$contents$` with `u8string(std::forward<T>(value))`.

```cpp
template<class T> requires constructible_from<string, T>
consteval data_member_options::$name-type$(T&& value);
```
[#]{.pnum} *Effects*: Initializes `$contents$` with `string(std::forward<T>(value))`.

::: note
The class `$name-type$` allows the function `data_member_spec` to accept an ordinary string literal (or `string_view`, `string`, etc.) or a UTF-8 string literal (or `u8string_view`, `u8string`, etc.) equally well.

::: example
```cpp
consteval void fn() {
  data_member_options o1 = {.name="ordinary_literal_encoding"};
  data_member_options o2 = {.name=u8"utf8_encoding"};
}
```
:::

:::

```cpp
consteval info data_member_spec(info type,
                                data_member_options options);
```
[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `dealias(type)` represents either an object type or a reference type;
- [#.#]{.pnum} if `options.name` contains a value, then:
  - [#.#.#]{.pnum} `holds_alternative<u8string>(options.name->$contents$)` is `true` and `get<u8string>(options.name->$contents$)` contains a valid identifier ([lex.name]) that is not a keyword ([lex.key]) when interpreted with UTF-8, or
  - [#.#.#]{.pnum} `holds_alternative<string>(options.name->$contents$)` is `true` and `get<string>(options.name->$contents$)` contains a valid identifier that is not a keyword when interpreted with the ordinary literal encoding;

  [The name corresponds to the spelling of an identifier token after phase 6 of translation ([lex.phases]). Lexical constructs like `$universal-character-name$`s [lex.universal.char] are not processed and will cause evaluation to fail. For example, `R"(\u03B1)"` is an invalid identifier and is not interpreted as `"α"`.]{.note}
- [#.#]{.pnum} if `options.name` does not contain a value, then `options.bit_width` contains a value;
- [#.#]{.pnum} if `options.bit_width` contains a value `$V$`, then
  - [#.#.#]{.pnum} `is_integral_type(type) || is_enumeration_type(type)` is `true`,
  - [#.#.#]{.pnum} `options.alignment` does not contain a value,
  - [#.#.#]{.pnum} `options.no_unique_address` is `false`, and
  - [#.#.#]{.pnum} if `$V$` equals `0` then `options.name` does not contain a value; and
- [#.#]{.pnum} if `options.alignment` contains a value, it is an alignment value ([basic.align]) not less than `alignment_of(type)`.

[#]{.pnum} *Returns*: A reflection of a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]) where

- [#.#]{.pnum} `$T$` is the type represented by `dealias(type)`,
- [#.#]{.pnum} `$N$` is either the identifier encoded by `options.name` or ⊥ if `options.name` does not contain a value,
- [#.#]{.pnum} `$A$` is either the alignment value held by `options.alignment` or ⊥ if `options.alignment` does not contain a value,
- [#.#]{.pnum} `$W$` is either the value held by `options.bit_width` or ⊥ if `options.bit_width` does not contain a value, and
- [#.#]{.pnum} `$NUA$` is the value held by `options.no_unique_address`.

[#]{.pnum} [The returned reflection value is primarily useful in conjunction with `define_aggregate`; it can also be queried by certain other functions in `std::meta` (e.g., `type_of`, `identifier_of`).]{.note}

```cpp
consteval bool is_data_member_spec(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a data member description. Otherwise, `false`.

```c++
  template <reflection_range R = initializer_list<info>>
  consteval info define_aggregate(info class_type, R&& mdescrs);
```

[#]{.pnum} Let `$C$` be the class represented by `class_type` and `@$r$~$K$~@` be the `$K$`^th^ reflection value in `mdescrs`. For every `@$r$~$K$~@` in `mdescrs`, let (`@$T$~$K$~@`, `@$N$~$K$~@`, `@$A$~$K$~@`, `@$W$~$K$~@`, `@$NUA$~$K$~@`) be the corresponding data member description represented by `@$r$~$K$~@`.

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `$C$` is incomplete from every point in the evaluation context; [`$C$` can be a class template specialization for which there is a reachable definition of the class template. In this case, the injected declaration is an explicit specialization.]{.note}
- [#.#]{.pnum} `is_data_member_spec(@$r$~$K$~@)` is `true` for every `@$r$~$K$~@`;
- [#.#]{.pnum} `is_complete_type(@$T$~$K$~@)` is `true` for every `@$r$~$K$~@`; and
- [#.#]{.pnum} for every pair (`@$r$~$K$~@`, `@$r$~$L$~@`) where `K < L`,  if `@$N$~$K$~@` is not ⊥ and `@$N$~$L$~@` is not ⊥, then either:

  - [#.#.#]{.pnum} `@$N$~$K$~@ != @$N$~$L$~@` is `true` or
  - [#.#.#]{.pnum} `@$N$~$K$~@ == u8"_"` is `true`. [Every provided identifier is unique or `"_"`.]{.note}

[#]{.pnum} *Effects*:
Produces an injected declaration `$D$` ([expr.const]) that defines `$C$` and has properties as follows:

- [#.1]{.pnum} The target scope of `$D$` is the scope to which `$C$` belongs ([basic.scope.scope]).
- [#.#]{.pnum} The locus of `$D$` follows immediately after the core constant expression currently under evaluation.
- [#.#]{.pnum} The characteristic sequence of `$D$` ([expr.const]) is the sequence of reflection values `@$r$~$K$~@`.
- [#.#]{.pnum} If `$C$` is a specialization of a templated class `$T$`, and `$C$` is not a local class, then `$D$` is an explicit specialization of `$T$`.
- [#.#]{.pnum} For each `@$r$~$K$~@`, there is a corresponding entity `@$M$~$K$~@` belonging to the class scope of `$D$` with the following properties:

  - [#.#.#]{.pnum} If `@$N$~$K$~@` is ⊥, `@$M$~$K$~@` is an unnamed bit-field. Otherwise, `@$M$~$K$~@` is a non-static data member whose name is the identifier determined by the character sequence encoded by `@$N$~$K$~@` in UTF-8.
  - [#.#.#]{.pnum} The type of `@$M$~$K$~@` is `@$T$~$K$~@`.
  - [#.#.#]{.pnum} `@$M$~$K$~@` is declared with the attribute `[[no_unique_address]]` if and only if `@$NUA$~$K$~@` is `true`.
  - [#.#.#]{.pnum} If `@$W$~$K$~@` is not ⊥, `@$M$~$K$~@` is a bit-field whose width is that value. Otherwise, `@$M$~$K$~@` is not a bit-field.
  - [#.#.#]{.pnum} If `@$A$~$K$~@` is not ⊥, `@$M$~$K$~@` has the `$alignment-specifier$` `alignas(@$A$~$K$~@)`. Otherwise, `@$M$~$K$~@` has no `$alignment-specifier$`.

- For every `@$r$~$L$~@` in `mdescrs` such that `$K$ < $L$`, the declaration corresponding to `@$r$~$K$~@` precedes the declaration corresponding to `@$r$~$L$~@`.

[#]{.pnum} *Returns*: `class_type`.

:::
:::

### [meta.reflection.traits] Reflection type traits  {-}

::: std
::: addu
[1]{.pnum} Subclause [meta.reflection.traits] specifies consteval functions to query the properties of types ([meta.unary]), query the relationships between types ([meta.rel]), or transform types ([meta.trans]) at compile time. Each consteval function declared in this class has an associated class template declared elsewhere in this document.

[#]{.pnum} Every function and function template declared in this subclause has the following conditions required for a call to that function or function template to be a constant subexpression ([defns.const.subexpr]):

* [#.#]{.pnum} For every parameter `p` of type `info`, `is_type(p)` is `true`.
* [#.#]{.pnum} For every parameter `r` whose type is constrained on `reflection_range`, `ranges::all_of(r, is_type)` is `true`.

```cpp
// associated with [meta.unary.cat], primary type categories
consteval bool is_void_type(info type);
consteval bool is_null_pointer_type(info type);
consteval bool is_integral_type(info type);
consteval bool is_floating_point_type(info type);
consteval bool is_array_type(info type);
consteval bool is_pointer_type(info type);
consteval bool is_lvalue_reference_type(info type);
consteval bool is_rvalue_reference_type(info type);
consteval bool is_member_object_pointer_type(info type);
consteval bool is_member_function_pointer_type(info type);
consteval bool is_enum_type(info type);
consteval bool is_union_type(info type);
consteval bool is_class_type(info type);
consteval bool is_function_type(info type);
consteval bool is_reflection_type(info type);

  // associated with [meta.unary.comp], composite type categories
consteval bool is_reference_type(info type);
consteval bool is_arithmetic_type(info type);
consteval bool is_fundamental_type(info type);
consteval bool is_object_type(info type);
consteval bool is_scalar_type(info type);
consteval bool is_compound_type(info type);
consteval bool is_member_pointer_type(info type);

// associated with [meta.unary.prop], type properties
consteval bool is_const_type(info type);
consteval bool is_volatile_type(info type);
consteval bool is_trivially_copyable_type(info type);
consteval bool is_trivially_relocatable_type(info type);
consteval bool is_replaceable_type(info type);
consteval bool is_standard_layout_type(info type);
consteval bool is_empty_type(info type);
consteval bool is_polymorphic_type(info type);
consteval bool is_abstract_type(info type);
consteval bool is_final_type(info type);
consteval bool is_aggregate_type(info type);
consteval bool is_consteval_only_type(info type);
consteval bool is_signed_type(info type);
consteval bool is_unsigned_type(info type);
consteval bool is_bounded_array_type(info type);
consteval bool is_unbounded_array_type(info type);
consteval bool is_scoped_enum_type(info type);

template <reflection_range R = initializer_list<info>>
consteval bool is_constructible_type(info type, R&& type_args);
consteval bool is_default_constructible_type(info type);
consteval bool is_copy_constructible_type(info type);
consteval bool is_move_constructible_type(info type);

consteval bool is_assignable_type(info type_dst, info type_src);
consteval bool is_copy_assignable_type(info type);
consteval bool is_move_assignable_type(info type);

consteval bool is_swappable_with_type(info type_dst, info type_src);
consteval bool is_swappable_type(info type);

consteval bool is_destructible_type(info type);

template <reflection_range R = initializer_list<info>>
consteval bool is_trivially_constructible_type(info type, R&& type_args);
consteval bool is_trivially_default_constructible_type(info type);
consteval bool is_trivially_copy_constructible_type(info type);
consteval bool is_trivially_move_constructible_type(info type);

consteval bool is_trivially_assignable_type(info type_dst, info type_src);
consteval bool is_trivially_copy_assignable_type(info type);
consteval bool is_trivially_move_assignable_type(info type);
consteval bool is_trivially_destructible_type(info type);

template <reflection_range R = initializer_list<info>>
consteval bool is_nothrow_constructible_type(info type, R&& type_args);
consteval bool is_nothrow_default_constructible_type(info type);
consteval bool is_nothrow_copy_constructible_type(info type);
consteval bool is_nothrow_move_constructible_type(info type);

consteval bool is_nothrow_assignable_type(info type_dst, info type_src);
consteval bool is_nothrow_copy_assignable_type(info type);
consteval bool is_nothrow_move_assignable_type(info type);

consteval bool is_nothrow_swappable_with_type(info type_dst, info type_src);
consteval bool is_nothrow_swappable_type(info type);

consteval bool is_nothrow_destructible_type(info type);
consteval bool is_nothrow_relocatable_type(info type);

consteval bool is_implicit_lifetime_type(info type);

consteval bool has_virtual_destructor(info type);

consteval bool has_unique_object_representations(info type);

consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
consteval bool reference_converts_from_temporary(info type_dst, info type_src);

// associated with [meta.rel], type relations
consteval bool is_same_type(info type1, info type2);
consteval bool is_base_of_type(info type_base, info type_derived);
consteval bool is_virtual_base_of_type(info type_base, info type_derived);
consteval bool is_convertible_type(info type_src, info type_dst);
consteval bool is_nothrow_convertible_type(info type_src, info type_dst);
consteval bool is_layout_compatible_type(info type1, info type2);
consteval bool is_pointer_interconvertible_base_of_type(info type_base, info type_derived);

template <reflection_range R = initializer_list<info>>
consteval bool is_invocable_type(info type, R&& type_args);
template <reflection_range R = initializer_list<info>>
consteval bool is_invocable_r_type(info type_result, info type, R&& type_args);

template <reflection_range R = initializer_list<info>>
consteval bool is_nothrow_invocable_type(info type, R&& type_args);
template <reflection_range R = initializer_list<info>>
consteval bool is_nothrow_invocable_r_type(info type_result, info type, R&& type_args);

// associated with [meta.trans.cv], const-volatile modifications
consteval info remove_const(info type);
consteval info remove_volatile(info type);
consteval info remove_cv(info type);
consteval info add_const(info type);
consteval info add_volatile(info type);
consteval info add_cv(info type);

// associated with [meta.trans.ref], reference modifications
consteval info remove_reference(info type);
consteval info add_lvalue_reference(info type);
consteval info add_rvalue_reference(info type);

// associated with [meta.trans.sign], sign modifications
consteval info make_signed(info type);
consteval info make_unsigned(info type);

// associated with [meta.trans.arr], array modifications
consteval info remove_extent(info type);
consteval info remove_all_extents(info type);

// associated with [meta.trans.ptr], pointer modifications
consteval info remove_pointer(info type);
consteval info add_pointer(info type);

// associated with [meta.trans.other], other transformations
consteval info remove_cvref(info type);
consteval info decay(info type);
template <reflection_range R = initializer_list<info>>
consteval info common_type(R&& type_args);
template <reflection_range R = initializer_list<info>>
consteval info common_reference(R&& type_args);
consteval info underlying_type(info type);
template <reflection_range R = initializer_list<info>>
consteval info invoke_result(info type, R&& type_args);
consteval info unwrap_reference(info type);
consteval info unwrap_ref_decay(info type);
```

[#]{.pnum} Each function or function template declared above has the following behavior based on the signature and return type of that function or function template. [The associated class template need not be instantiated.]{.note}

<table>
<tr><th>Signature and Return Type</th><th>*Returns*</th></tr>
<tr><td>
```cpp
bool meta::$UNARY$(info type);
bool meta::$UNARY$_type(info type);
```
</td><td>`std::$UNARY$_v<$T$>`, where `$T$` is the type or type alias represented by `type`</td></tr>
<tr><td>
```cpp
bool meta::$BINARY$(info t1, info t2);
bool meta::$BINARY$_type(info t1, info t2);
```
</td><td>`std::$BINARY$_v<$T1$, $T2$>`, where `$T1$` and `$T2$` are the types or type aliases represented by `t1` and `t2`, respectively</td></tr>
<tr><td>
```cpp
template <reflection_range R>
bool meta::$VARIADIC$_type(info type, R&& args);
```
</td>
<td>`std::$VARIADIC$_v<$T$, $U$...>` where `$T$` is the type or type alias represented by `type` and `$U$...` is the pack of types or type aliases whose elements are represented by the corresponding elements of `args`</td></tr>
<tr><td>
```cpp
template <reflection_range R>
bool meta::$VARIADIC$_type(info t1, info t2, R&& args);
```
</td>
<td>`std::$VARIADIC$_v<$T1$, $T2$, $U$...>` where `$T1$` and `$T2$` are the types or type aliases represented by `t1` and `t2`, respectively, and `$U$...` is the pack of types or type aliases whose elements are represented by the corresponding elements of `args`</td></tr>
<tr><td>
```cpp
info meta::$UNARY$(info type);
```
</td><td>A reflection representing the type denoted by `std::$UNARY$_t<$T$>`, where `$T$` is the type or type alias represented by `type`</td></tr>
<tr><td>
```cpp
template <reflection_range R>
info meta::$VARIADIC$(R&& args);
```
</td>
<td>A reflection representing the  type denoted by `std::$VARIADIC$_t<$T$...>` where `$T$...` is the pack of types or type aliases whose elements are represented by the corresponding elements of `args`</td></tr>
<tr><td>
```cpp
template <reflection_range R>
info meta::$VARIADIC$(info type, R&& args);
```
</td>
<td>A reflection representing the  type denoted by `std::$VARIADIC$_t<$T$, $U$...>` where `$T$` is the type or type alias represented by `type` and `$U$...` is the pack of types or type aliases whose elements are represented by the corresponding elements of `args`</td></tr>
</table>

[#]{.pnum} [For those functions or function templates which return a reflection, that reflection always represents a type and never a type alias.]{.note}

[#]{.pnum} [If `t` is a reflection of the type `int` and `u` is a reflection of an alias to the type `int`, then `t == u` is `false` but `is_same_type(t, u)` is `true`. Also, `t == dealias(u)` is `true`.]{.note}.


```cpp
consteval size_t rank(info type);
```

[#]{.pnum} *Returns*: `rank_v<T>`, where `T` is the type represented by `dealias(type)`.

```cpp
consteval size_t extent(info type, unsigned i = 0);
```

[#]{.pnum} *Returns*: `extent_v<T, I>`, where `T` is the type represented by `dealias(type)` and `I` is a constant equal to `i`.
:::
:::

[The below inclusion of `meta::type_order` assumes the acceptance of [@P2830R10].]{.ednote}

::: std
::: addu
```cpp
consteval size_t tuple_size(info type);
```

[#]{.pnum} *Returns*: `tuple_size_v<$T$>` where `$T$` is the type represented by `dealias(type)`.

```cpp
consteval info tuple_element(size_t index, info type);
```

[#]{.pnum} *Returns*: A reflection representing the type denoted by `tuple_element_t<$I$, $T$>` where `$T$` is the type represented by `dealias(type)` and `$I$` is a constant equal to `index`.

```cpp
consteval size_t variant_size(info type);
```

[#]{.pnum} *Returns*: `variant_size_v<$T$>` where `$T$` is the type represented by `dealias(type)`.

```cpp
consteval info variant_alternative(size_t index, info type);
```

[#]{.pnum} *Returns*: A reflection representing the type denoted by `variant_alternative_t<$I$, $T$>` where `$T$` is the type represented by `dealias(type)` and `$I$` is a constant equal to `index`.

```cpp
consteval strong_ordering type_order(info t1, info t2);
```

[#]{.pnum} *Returns*: `type_order_v<$T1$, $T2$>`, where `$T1$` and `$T2$` are the types represented by `dealias(t1)` and `dealias(t2)`, respectively.
:::
:::

### [bit.cast]{.sref} Function template `bit_cast` {-}

And we have adjust the requirements of `bit_cast` to not allow casting to or from `meta::info`, in [bit.cast]{.sref}/3, which we add as a mandates (and then *Constant When* has to be before *Returns*, but the *Returns* remains unchanged):

::: std
```cpp
template<class To, class From>
  constexpr To bit_cast(const From& from) noexcept;
```
[1]{.pnum} *Constraints*:

* [1.1]{.pnum} `sizeof(To) == sizeof(From)` is `true`;
* [1.2]{.pnum} `is_trivially_copyable_v<To>` is `true`; and
* [1.3]{.pnum} `is_trivially_copyable_v<From>` is `true`.

::: addu
[*]{.pnum} *Mandates*: Neither `To` nor `From` are consteval-only types ([expr.const]).
:::

::: rm
[2]{.pnum} *Returns*: [...]
:::

[3]{.pnum} [*Remarks*]{.rm} [*Constant When*]{.addu}: [This function is constexpr if and only if]{.rm} `To`, `From`, and the types of all subobjects of `To` and `From` are types `T` such that:

* [#.1]{.pnum} `is_union_v<T>` is `false`;
* [#.2]{.pnum} `is_pointer_v<T>` is `false`;
* [#.3]{.pnum} `is_member_pointer_v<T>` is `false`;
* [#.4]{.pnum} `is_volatile_v<T>` is `false`; and
* [#.5]{.pnum} `T` has no non-static data members of reference type.

::: addu
[4]{.pnum} *Returns*: [...]
:::
:::

### [diff.cpp23]{.sref} Annex C (informative) Compatibility {-}

Add two new Annex C entries:

::: std
::: addu
**Affected subclause**: [lex.operators]

**Change**: New operator `^^`.

**Rationale**: Required for new features.

**Effect on original feature**: Valid C++23 code that contains two consecutive `^` tokens can be ill-formed in this revision of C++.

::: example
```cpp
struct C { int operator^(int); };
int operator^(int (C::*p)(int), C);
int i = &C::operator^^C{}; // ill-formed; previously well-formed
```
:::

**Affected subclause**: [dcl.attr.grammar]

**Change**: New token `:]`.

**Rationale**: Required for new features.

**Effect on original feature**: Valid C++23 code that contained an *attribute-specifier* with an *attribute-using-prefix* but no attributes and no whitespace is ill-formed in this revision of C++.

::: example
```cpp
struct [[using CC:]] C;   // ill-formed; previously well-formed
struct [[using DD: ]] D;  // OK
```
:::
:::
:::

Modify [diff.cpp23.library]:

::: std
**Affected subclause**: [headers]

**Change**: New headers.

**Rationale**: New functionality.

**Effect on original feature**: The folowing C++ headers are new: `<debugging>`, `<hazard_pointer>`, `<inplace_vector>`, `<linalg>`, [`<meta>`, ]{.addu} `<rcu>`, `<simd>`, and `<text_encoding>`. Valid C++ 2023 code that `#include`s headers with these names may be invalid in this revision of C++.

:::

## Feature-Test Macros

This is a feature with both a language and library component. Our usual practice is to provide something like `__cpp_impl_reflection` and `__cpp_lib_reflection` for this. But since the two pieces are so closely tied together, maybe it really only makes sense to provide one?

For now, we'll add both.

To [cpp.predefined]{.sref}:

::: std
```diff
  __cpp_impl_coroutine 201902L
  __cpp_impl_destroying_delete 201806L
  __cpp_impl_three_way_comparison 201907L
+ __cpp_impl_reflection 2025XXL
```
:::

and [version.syn]{.sref}:

::: std
```diff
+ #define __cpp_lib_reflection 2025XXL // also in <meta>
```
:::

# Appendix: Design changes approved in Hagenberg

[@P2996R4] was forwarded to CWG in St. Louis (June 2024). In the time after, some minor design changes were shown to be necessary. The following changes were confirmed by EWG during the Hagenberg 2025 meeting.

One small change was needed to the reflection operator.

- [1]{.pnum} Application of the `^^` operator to a non-type template parameter
  - **P2996R4**: Applying `^^` to a non-type template parameter, or to a `$pack-index-expression$`, gave a reflection of the value or object computed or designated by the operand ([expr.reflect]/10).
  - **D2996R10**: Applying `^^` to such expressions is ill-formed ([expr.reflect]/6.4).
  - **Rationale**: The operand following the `^^` of a `$reflect-expression$` is an unevaluated operand ([expr.context/1], [expr.reflect]/6.4). Supporting this necessarily requires that we evaluate said operand, which is at odds with its specification. Introducing some sort of "conditionally evaluated" operand machinery would be novel and unnecessary.
  - **Instead**: Just use `std::meta::reflect_value` or `std::meta::reflect_object`, as appropriate.

A few changes were needed to "consteval-only types" to ensure that objects of such types cannot reach runtime.

- [#]{.pnum} Relaxed linkage restrictions on objects of consteval-only types
  - **P2996R4**: Rigid rules prevented objects of consteval-only type from having module or external linkage ([basic.link]/4.9, [basic.types.general]).
    - Included static data members, etc. Quite strict.
  - **D2996R10**: All such restrictions have been removed.
  - **Rationale**: Implementation experience at the intersection of modules and reflection proved that reflections can be imported across TUs and modules without issue.
    - Try it with [Clang](https://godbolt.org/z/Y8cdd9sGo).

- [#]{.pnum} Immediate-escalation of expressions of consteval-only type
  - **D2996R10**: Every expression of consteval-only type is _immediate-escalating_ ([expr.const]/25).
  - **Rationale**: Prevent any need for `std::meta::info` to persist to runtime (e.g., passing a reference to a `constexpr std::meta::info` to a runtime function).
  - Fully implemented; try it with Clang [here](https://godbolt.org/z/5sfe7vdzE) and [here](https://godbolt.org/z/T3MY1Yqo4).

- [#]{.pnum} Immediate-escalation of non-constexpr variables of consteval-only type
  - **D2996R10**: Immediate-escalating functions containing non-constexpr variables of consteval-only type are immediate functions ([expr.const]/27.2.2).
  - **Rationale**: Prevents default-constructed variables of consteval-only type (for which an expression does not necessarily appear) from reaching runtime.
  - Fully implemented; try it with [Clang](https://godbolt.org/z/3asrnK13G).

- [#]{.pnum} No "erasure" of consteval-only-ness from results of constant expressions.
  - **D2996R10**: Pointer or reference results of constant expressions must have consteval-only type whenever the object that they point or reference into does.
    - e.g., A `void *` pointer to a `constexpr std::meta::info` cannot be a result of a constant expression.
  - Fully implemented; try it with [Clang](https://godbolt.org/z/n47ona1db).
  - Still fine to type erase _within_ a constant expression (as seen [here](https://godbolt.org/z/E4faezfr3)).

Two changes were needed for splicers.

- [#]{.pnum} A slight syntactic change is needed for template splicers.
  - **P2994R4**: `$splice-template-name$` handled template splicers.
    - Not the best distinction between "names" and "entities" (naturally, CWG set us straight ❤️).
    - Wasn't entirely clear how some cases (e.g., CTAD, placeholder types) were supposed to work.
  - **D2996R10**: Type template splicers are folded into `$splice-type-specifier$` ([dcl.type.splice]). Simple rule:
    - Splicing a template as a type is spelled `typename [:R:]`.
    - Splicing a template as an expression is spelled `template [:R:]`.
  - Try it on godbolt with [Clang](https://godbolt.org/z/GKj5of839).

- [#]{.pnum} Reflections of concepts cannot be spliced.
  - **P2996R4**: Concepts could be spliced within both `$type-constraint$`s and `$concept-id$`s.
  - **D2996R10**: Splicing a concept is ill-formed.
  - **Rationale**: [@P2841R5] has already done the work to figure out dependent concepts. CWG requested that we wait for that to land first, and revisit concept splicers in a future paper.
  - **Instead**: For the case of a `$concept-id$`, `substitute` can still check whether a concept is satisfied by a template argument list.

Our framework for code injection as performed by `define_aggregate` evolved quite a bit after P2996R4. When the evaluation of an expression calls `define_aggregate`, we say that the evaluation produces an _injected declaration_ of the completed type (try it on [godbolt](https://godbolt.org/z/PTeb9qqcW)).

- [1]{.pnum} Recent revisions lock down the context from which `define_aggregate` can be called.
  - **P2996R4**: `constexpr` variable initializers, immediate invocations, `$constant-expression$`s, `if constexpr` conditions ([expr.const]/21).
  - **D2996R10**: Only from `consteval` blocks ([dcl.pre]). No other expressions that would evaluate `define_aggregate` can qualify as core constant expressions ([expr.const/10.27+]).
  - **Rationale**: Other constructs have proven unsuitable for code injection due to e.g., template instantiation behavior, immediate-escalating expression behavior, etc.
  - Fully implemented with Clang.

- [#]{.pnum} The scope that a given expression can inject a declaration _into_ has been constrained.
  - **P2996R4**: The wild west: No restrictions.
  - **D2996R10**: No intervening function or class scope is allowed between the `consteval` block and the target scope of the injected declaration ([expr.const]/29).
  - **Rationale**: Prevents the program from being able to use `define_aggregate` to observe failed substitutions, overload resolution order, etc.
  - Fully implemented in Clang.

- [#]{.pnum} Strengthening order of evaluation for core constant expressions removes the need for more IFNDR.
  - **D2996R10**: During the evaluation of an expression as a core constant expresison, suboperands and subexpressions that are otherwise unsequenced or indeterminately sequenced are evaluated in lexical order.
    - All four major implementations already conform to this rule, and representatives of each have expressed that they have no concerns.

---
references:

---
