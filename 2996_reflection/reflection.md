---
title: "Reflection for C++26"
document: D2996R10
date: today
audience: CWG, LEWG, LWG
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
* library wording updates
  * add `<meta>` to [headers]
  * fix definition of "Constant When" (use "constant subexpression" in lieu of "core constant expression")
  * avoid referring to "permitted results of constant expressions" in wording for `reflect_value` and `reflect_object` (term was retired by [@P2686R5])
  * template specializations and non-static data members of closure types are not _members-of-representable_

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
* `type_of` no longer returns reflections of `$typedef-names$`; added elaboration of reasoning to the ["Handling Aliases"](#handling-aliases) section.
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
    args.push_back(reflect_value(r));
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
  return std::meta::nonstatic_data_members_of(^^S)[n];
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (member_number(5) is not a constant).
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/7P3ax5K16), [Clang](https://godbolt.org/z/8M5jP9d9E).

This proposal specifies that namespace `std::meta` is associated with the reflection type (`std::meta::info`); the `std::meta::` qualification can therefore be omitted in the example above.

Another frequently-useful metafunction is `std::meta::identifier_of`, which returns a `std::string_view` describing the identifier with which an entity represented by a given reflection value was declared.
With such a facility, we could conceivably access non-static data members "by string":

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_named(std::string_view name) {
  for (std::meta::info field : nonstatic_data_members_of(^^S)) {
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

On Compiler Explorer: [EDG](https://godbolt.org/z/hhd9vePW7), [Clang](https://godbolt.org/z/1hP77jbsd).


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
    args.push_back(std::meta::reflect_value(k));
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
  constexpr auto members = nonstatic_data_members_of(^^S);
  std::array<member_descriptor, members.size()> layout;
  for (int i = 0; i < members.size(); ++i) {
      layout[i] = {.offset=offset_of(members[i]).bytes, .size=size_of(members[i])};
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

On Compiler Explorer: [EDG](https://godbolt.org/z/ss9hfaMKT), [Clang](https://godbolt.org/z/Wq13c7Gsv).

## Enum to String

One of the most commonly requested facilities is to convert an enum value to a string (this example relies on expansion statements):

::: std
```c++
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

enum Color { red, green, blue };
static_assert(enum_to_string(Color::red) == "red");
static_assert(enum_to_string(Color(42)) == "<unnamed>");
```
:::

We can also do the reverse in pretty much the same way:

::: std
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::optional<E> string_to_enum(std::string_view name) {
  template for (constexpr auto e : std::meta::enumerators_of(^^E)) {
    if (name == std::meta::identifier_of(e)) {
      return [:e:];
    }
  }

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

On Compiler Explorer: [EDG](https://godbolt.org/z/hf777PfGo), [Clang](https://godbolt.org/z/h5rTnWrKW).


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
  template for (constexpr auto dm : nonstatic_data_members_of(^^Opts)) {
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

On Compiler Explorer: [EDG](https://godbolt.org/z/jGfGv84oh), [Clang](https://godbolt.org/z/hfoP5P3sd).


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
  return nonstatic_data_members_of(r)[n];
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

On Compiler Explorer: [EDG](https://godbolt.org/z/76EojjcEe), [Clang](https://godbolt.org/z/cx8cr53q7).

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
        return nonstatic_data_members_of(^^Storage)[n+1];
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

On Compiler Explorer: [EDG](https://godbolt.org/z/W74qxqnhf), [Clang](https://godbolt.org/z/h13oh4s6e).

## Struct to Struct of Arrays

::: std
```c++
#include <meta>
#include <array>

template <typename T, size_t N>
struct struct_of_arrays_impl {
  struct impl;

  consteval {
    std::vector<std::meta::info> old_members = nonstatic_data_members_of(^^T);
    std::vector<std::meta::info> new_members = {};
    for (std::meta::info member : old_members) {
        auto array_type = substitute(^^std::array, {
            type_of(member),
            std::meta::reflect_value(N),
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

On Compiler Explorer: [EDG](https://godbolt.org/z/jWrPGhn5s), [Clang](https://godbolt.org/z/a1sTxnW4o).


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
  std::vector<std::meta::info> new_members;
  for (std::meta::info member : nonstatic_data_members_of(spec)) {
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

    template for (constexpr auto [sm, om] : std::views::zip(nonstatic_data_members_of(^^Spec),
                                                            nonstatic_data_members_of(^^Opts))) {
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

On Compiler Explorer: [EDG](https://godbolt.org/z/4aseo5eGq), [Clang](https://godbolt.org/z/3qG5roer4).

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

    template for (constexpr auto base : bases_of(^^T)) {
      delim();
      out = std::format_to(out, "{}", (typename [: type_of(base) :] const&)(t));
    }

    template for (constexpr auto mem : nonstatic_data_members_of(^^T)) {
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

On Compiler Explorer: [Clang](https://godbolt.org/z/drv1nbsf3).

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
  template for (constexpr auto mem : nonstatic_data_members_of(^^T)) {
      hash_append(algo, t.[:mem:]);
  }
}
```
:::

## Converting a Struct to a Tuple

This approach requires allowing packs in structured bindings [@P1061R5], but can also be written using `std::make_index_sequence`:

::: std
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
:::

An alternative approach is:

::: std
```cpp
consteval auto type_struct_to_tuple(info type) -> info {
  return substitute(^^std::tuple,
                    nonstatic_data_members_of(type)
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

  std::vector args = {^^To, ^^From};
  for (auto mem : nonstatic_data_members_of(^^From)) {
    args.push_back(reflect_value(mem));
  }

  /*
  Alternatively, with Ranges:
  args.append_range(
    nonstatic_data_members_of(^^From)
    | std::views::transform(std::meta::reflect_value)
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

On Compiler Explorer (with a different implementation than either of the above): [EDG](https://godbolt.org/z/1Tffn4vzn), [Clang](https://godbolt.org/z/9WzGM93dM).

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
        a2.push_back(std::meta::reflect_value(x));
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
make_named_tuple<^^int, std::meta::reflect_value("x"),
                 ^^double, std::meta::reflect_value("y")>()
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

static_assert(type_of(nonstatic_data_members_of(^^R)[0]) == ^^int);
static_assert(type_of(nonstatic_data_members_of(^^R)[1]) == ^^double);

int main() {
    [[maybe_unused]] auto r = R{.x=1, .y=2.0};
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/64qTe4KG1), [Clang](https://godbolt.org/z/76qM1xqvn).

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

static_assert(type_of(nonstatic_data_members_of(^^R)[0]) == ^^int);
static_assert(type_of(nonstatic_data_members_of(^^R)[1]) == ^^double);

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
                                       { std::meta::reflect_value(k) })))
      ++k;
    return k;
  }

  static consteval void increment() {
    define_aggregate(substitute(^^Helper,
                                { std::meta::reflect_value(latest())}),
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
- a function or member function
- a non-static data member
- a primary template or primary member template
- an enumerator

For all other operands, the expression is ill-formed. In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to not evaluate to constant.

Earlier revisions of this paper allowed for taking the reflection of any `$cast-expression$` that could be evaluated as a constant expression, as we believed that a constant expression could be internally "represented" by just capturing the value to which it evaluated. However, the possibility of side effects from constant evaluation (introduced by this very paper) renders this approach infeasible: even a constant expression would have to be evaluated every time it's spliced. It was ultimately decided to defer all support for expression reflection, but we intend to introduce it through a future paper using the syntax `^^(expr)`.

This paper does, however, support reflections of _values_ and of _objects_ (including subobjects). Such reflections arise naturally when iterating over template arguments.

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

Such reflections cannot generally be obtained using the `^^`-operator, but the `std::meta::reflect_value` and `std::meta::reflect_object` functions make it easy to reflect particular values or objects. The `std::meta::value_of` metafunction can also be used to map a reflection of an object to a reflection of its value.

### Syntax discussion

The original TS landed on `@[reflexpr]{.cf}@(...)` as the syntax to reflect source constructs and [@P1240R0] adopted that syntax as well.
As more examples were discussed, it became clear that that syntax was both (a) too "heavy" and (b) insufficiently distinct from a function call.
SG7 eventually agreed upon the prefix `^` operator. The "upward arrow" interpretation of the caret matches the "lift" or "raise" verbs that are sometimes used to describe the reflection operation in other contexts.

The caret already has a meaning as a binary operator in C++ ("exclusive OR"), but that is clearly not conflicting with a prefix operator.
In C++/CLI (a Microsoft C++ dialect) the caret is also used as a new kind of `$ptr-operator$` ([dcl.decl.general]{.sref}) to declare ["handles"](https://learn.microsoft.com/en-us/cpp/extensions/handle-to-object-operator-hat-cpp-component-extensions?view=msvc-170).
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
*  Otherwise, if `r` is a reflection of a function template or member function template, `&[:r:]` is the address of that overload set - which would then require external context to resolve as usual.

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

```cpp
namespace A {}
constexpr std::meta::info NS_A = ^^A;

namespace B {
  namespace [:NS_A:] {
    void fn();  // Is this '::A::fn' or '::B::A::fn' ?
  }
}
```

We found no satisfying answer as to how to interpret examples like the one given above. Neither did we find motivating use cases: many of the "interesting" uses for reflections of namespaces are either to introspect their members, or to pass them as template arguments - but the above example does nothing to help with introspection, and neither can namespaces be reopened within any dependent context. Rather than choose between unintuitive options for a syntax without a motivating use case, we are disallowing splicers from appearing in the opening of a namespace.

#### Splicing namespaces in using-directives and using-enum-declarators

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

C++20 already disallowed dependent enumeration types from appearing in _using-enum-declarators_ (as in #1), as it would otherwise force the parser to consider every subsequent identifier as possibly a member of the substituted enumeration type. We extend this limitation to splices of dependent reflections of enumeration types, and further disallow the use of dependent reflections of namespaces in _using-directives_ (as in #2) following the same principle.

#### Splicing concepts in declarations of template parameters

```cpp
template <typename T> concept C = requires { requires true; };

template <std::meta::info R> struct Outer {
  template <template [:R:] S> struct Inner { /* ... */ };
};
```

What kind of parameter is `S`? If `R` represents a class template, then it is a non-type template parameter of deduced type, but if `R` represents a concept, it is a type template parameter. There is no other circumstance in the language for which it is not possible to decide at parse time whether a template parameter is a type or a non-type, and we don't wish to introduce one for this use case.

The most obvious solution would be to introduce a `concept [:R:]` syntax that requires that `R` reflect a concept, and while this could be added going forward, we weren't convinced of its value at this time - especially since the above can easily be rewritten:

```cpp
template <std::meta::info R> struct Outer {
  template <typename T> requires template [:R:]<T>
  struct Inner { /* ... */ };
};
```

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
  - any function or member function
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
static_assert(value_of(^^i) == value_of(^^j));  // Two equivalent values.
static_assert(^^i != std::meta::reflect_object(i))  // A variable is distinct from the
                                                    // object it designates.
static_assert(^^i != std::meta::reflect_value(42));  // A reflection of an object
                                                     // is not the same as its value.
```
:::


### The associated `std::meta` namespace

The namespace `std::meta` is an associated type of `std::meta::info`, which allows standard library meta functions to be invoked without explicit qualification. For example:

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

First, we identify a subset of manifestly constant-evaluated expressions and conversions characterized by the fact that their evaluation must occur and must succeed in a valid C++ program: We call these _plainly constant-evaluated_.
We require that a programmer can count on those evaluations occurring exactly once and completing at translation time.

Second, we sequence plainly constant-evaluated expressions and conversions within the lexical order.
Specifically, we require that the evaluation of a non-dependent plainly constant-evaluated expression or conversion occurs before the implementation checks the validity of source constructs lexically following that expression or conversion.

Those constraints are mostly intuitive, but they are a significant change to the underlying principles of the current standard in this respect.

[@P2758R1] ("Emitting messages at compile time") also has to deal with side effects during constant evaluation.
However, those effects ("output") are of a slightly different nature in the sense that they can be buffered until a manifestly constant-evaluated expression/conversion has completed.
"Buffering" a class type completion is not practical (e.g., because other metafunctions may well depend on the completed class type).
Still, we are not aware of incompatibilities between our proposal and [@P2758R1].


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

The above example observes the effects of the failed substitution of `#1` by way of the completeness of `S`. Such tricks can be used to observe implementation details, like the order in which overloads are checked, that may be unportable (and which implementations might desire to change over time).

Our proposed solution, specified in [expr.const]/23.2, is to make it ill-formed to produce an injected declaration from a manifestly constant-evaluated expression _inside of_ an instantiation to _outside of_ that instantiation, or visa versa. Because that expression in the example above (`define_aggregate(^^S, {})`) is within the instantiation of the requires clause of `TCls<void>::sfn`, and the target scope of the injected declaration is outside of that same instantiaton, the example becomes ill-formed (diagnostic required). Note that this does not prevent writing `consteval` function templates that wrap `define_aggregate`:

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

Nor does this rule prevent a class template from producing a declaration whose target scope is the same specialization.

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

Athough the instantiation of `TCls1<void>` in the requires-clause of `#1` causes an injected declaration to be produced, it is not discernibly a side-effect of the failed substitution: Observing the side effect will first require one to write (some moral  equivalent of) `TCLs1<void>::Incomplete`, the act of which would otherwise itself trigger the same side-effect.

Although this rule constrains the manner with which `define_aggregate` can be used, we are not aware of any motivating use cases for P2996 that are harmed. Worth mentioning, however: the rule has more dire implications for other code injection papers being considered by WG21, most notably [@P3294R2] ("_Code Injection With Token Sequences_"). With this rule as it is, it becomes impossible for e.g., the instantiation of a class template specialization `TCls<Foo>` to produce an injected declaration of `std::formatter<TCls<Foo>>` (since the target scope would be the global namespace).

In this context, we do believe that relaxations of the rule can be considered: For instance, we ought to be able to say that the instantiation of `std::formatter<TCls<Foo>>` is sequenced strictly after the instantiation of `TCls<Foo>`, and observations such as these might make it possible to permit such injections without making it "discernible" whether they resulted from failed substitutions. The key to such an approach would be to define a partial order over the instantiations of a program, and to allow constructs to be injected _across_ instantiations when the relative order of their respective instantiations is defined.

All of that said, these relaxations are not needed for the code injection introduced by this proposal, and we do not seek to introduce them at this time.

### Freestanding implementations

Several important metafunctions, such as `std::meta::nonstatic_data_members_of`, return a `std::vector` value.
Unfortunately, that means that they are currently not usable in a freestanding environment, but [@P3295R0] currently proposes freestanding `std::vector`, `std::string`, and `std::allocator` in constant evaluated contexts, explicitly to make the facilities proposed by this paper work in freestanding.


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

  // @[object and value queries](#object_of-value_of)@
  consteval auto object_of(info r) -> info;
  consteval auto value_of(info r) -> info;

  // @[template queries](#template_of-template_arguments_of)@
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;

  // @[member queries](#member-queries)@
  consteval auto members_of(info r) -> vector<info>;
  consteval auto bases_of(info type_class) -> vector<info>;
  consteval auto static_data_members_of(info type_class) -> vector<info>;
  consteval auto nonstatic_data_members_of(info type_class) -> vector<info>;
  consteval auto enumerators_of(info type_enum) -> vector<info>;

  consteval auto get_public_members(info type) -> vector<info>;
  consteval auto get_public_static_data_members(info type) -> vector<info>;
  consteval auto get_public_nonstatic_data_members(info type) -> vector<info>;
  consteval auto get_public_bases(info type) -> vector<info>;

  // @[substitute](#substitute)@
  template <reflection_range R = initializer_list<info>>
    consteval auto can_substitute(info templ, R&& args) -> bool;
  template <reflection_range R = initializer_list<info>>
    consteval auto substitute(info templ, R&& args) -> info;

  // @[reflect expression results](#reflect-expression-results)@
  template <typename T>
    consteval auto reflect_value(const T& value) -> info;
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
  consteval auto has_complete_definition(info r) -> bool;
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

### `object_of`, `value_of`

::: std
```c++
namespace std::meta {
  consteval auto object_of(info r) -> info;
  consteval auto value_of(info r) -> info;
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

If `r` is a reflection of an enumerator, then `value_of(r)` is a reflection of the value of the enumerator. Otherwise, if `r` is a reflection of an object _usable in constant expressions_, then `value_of(r)` is a reflection of the value of the object. For all other inputs, `value_of(r)` is not a constant expression.

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

  consteval auto get_public_members(info type_class) -> vector<info>;
  consteval auto get_public_static_data_members(info type_class) -> vector<info>;
  consteval auto get_public_nonstatic_data_members(info type_class) -> vector<info>;
  consteval auto get_public_bases(info type_class) -> vector<info>;
}
```
:::

The template `members_of` returns a vector of reflections representing the direct members of the class type or namespace represented by its first argument.
Any non-static data members appear in declaration order within that vector.
Anonymous unions appear as a non-static data member of corresponding union type.
Reflections of structured bindings shall not appear in the returned vector.

The template `bases_of` returns the direct base classes of the class type represented by its first argument, in declaration order.

`static_data_members_of` and `nonstatic_data_members_of` return reflections of the static and non-static data members, in order, respectively.

`enumerators_of` returns the enumerator constants of the indicated enumeration type in declaration order.

The `get_public_meow` functions are equivalent to `meow_of` functions except that they additionally filter the results on those members for which `is_public(member)` is `true`.
The only other distinction is that `members_of` can be invoked on a namespace, while `get_public_members` can only be invoked on a class type (because it does not make sense to ask for the public members of a namespace).
This set of functions has a distinct API by demand for ease of grepping.

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

### `reflect_value`, `reflect_object`, `reflect_function` {#reflect-expression-results}

::: std
```c++
namespace std::meta {
  template<typename T> consteval auto reflect_value(const T& expr) -> info;
  template<typename T> consteval auto reflect_object(T& expr) -> info;
  template<typename T> consteval auto reflect_function(T& expr) -> info;
}
```
:::

These metafunctions produce a reflection of the _result_ from evaluating the provided expression. One of the most common use-cases for such reflections is to specify the template arguments with which to build a specialization using `std::meta::substitute`.

`reflect_value(expr)` produces a reflection of the value computed by an lvalue-to-rvalue conversion on `expr`. The type of the reflected value is the cv-unqualified (de-aliased) type of `expr`. The result needs to be a permitted result of a constant expression, and `T` cannot be of reference type.

```cpp
static_assert(substitute(^^std::array, {^^int, std::meta::reflect_value(5)}) ==
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

For other reflection values `r`, `extrace<T>(r)` is ill-formed.

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

### [intro.defs]{.sref} Terms and definitions {-}

Add `$splice-specifier$` to the list of template argument forms in definition 3.5.

::: std
**[3.5]{.pnum} argument**

⟨template instantiation⟩ `$constant-expression$`, `$type-id$`, [or]{.rm} `$id-expression$`[, or `$splice-specifier$`]{.addu} in the comma-separated list bounded by the angle brackets

:::

### [lex.phases]{.sref} Phases of translation {-}

[In addition to changes necessary for this proposal, we are applying the "drive-by fix" of merging phases 7/8, in order to clarify that template instantiation is interleaved with translation. In so doing, we replace the notion of "instantiation units" with a partial ordering among all program constructs in a translation unit.]{.ednote}

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

::: std

[7-8]{.pnum} Whitespace characters separating tokens are no longer significant. Each preprocessing token is converted into a token ([lex.token]{.sref}). The resulting tokens constitute a translation unit and are syntactically and semantically analyzed and translated.

  [The process of analyzing and translating the tokens can occasionally result in one token being replaced by a sequence of other tokens ([temp.names])]{.note3}

  It is implementation-defined whether the sources for module-units and header units on which the current translation unit has an interface dependency ([module.unit]{.sref}, [module.import]{.sref}) are required to be available.

  [Source files, translation units and translated translation units need not necessarily be stored as files, nor need there be any one-to-one correspondence between these entities and any external representation. The description is conceptual only, and does not specify any particular implementation.]{.note}

  [Translated translation units and instantiation units are combined as follows:]{.rm}

  [[Some or all of these can be supplied from a library.]{.note5}]{.rm}

  [Each translated translation unit is examined to produce a list of required instantiations.]{.rm}

  [While the tokens constituting translation units are being analyzed and translated, required instantiations are performed.]{.addu}

  [This can include instantiations which have been explicitly requested ([temp.explicit]).]{.note5}

  [The contexts from which instantiations may be performed are determined by their respective points of instantiation ([temp.point]{.sref}).]{.addu}

  [[Other requirements in this document can further constrain the  context from which an instantiation can be performed. For example, a constexpr function template specialization might have a point of instantation at the end of a translation unit, but its use in certain constant expressions could require that it be instantiated at an earlier point ([temp.inst]).]{.note}]{.addu}

  [The definitions of the required templates are located. It is implementation-defined whether the source of the translation units containing these definitions is required to be available.]{.rm}

  [[An implementation can choose to encode sufficient information into the translated translation unit so as to ensure the source is not required here.]{.note}]{.rm}

  [All required instantiations are perfomed to produce _instantiation units_.]{.rm}

  [[These are similar to translated translation units, but contain no references to uninstantiated templates and no template definitions.]{.note}]{.rm}

  [Each instantiation results in new program constructs.]{.addu}

  [The program is ill-formed if any instantiation fails.]{.note}

  [[Constructs that are separately subject to instantiation are specified in [temp.inst].]{.note}]{.addu}

::: addu
  During the analysis and translation of tokens, certain expressions are evaluated ([expr.const]). Constructs appearing at a program point `$P$` are analyzed in a context where each side effect of evaluating an expression `$E$` as a full-expression is complete if and only if

  - [7-8.#]{.pnum} `$E$` is the evaluating expression of a `$consteval-block-declaration$` ([dcl.pre]), and
  - [7-8.#]{.pnum} either that `$consteval-block-declaration$` or the template definition from which it is instantiated is reachable from

    - [7-8.#.#]{.pnum} `$P$`, or
    - [7-8.#.#]{.pnum} a point immediately following the `$class-specifier$` of a class for which `$P$` is in a complete-class context.

::: example
```cpp
class S {
  class Incomplete;

  class Inner {
    void fn() {
      /* p1 */ Incomplete i; // OK, constructs at P1 are analyzed in a context where the side effect of
                             // the call to define_aggregate is evaluated because:
                             // * E is the evaluating expression of a consteval block, and
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

  [8]{.pnum} [All]{.rm} [Translated translation units are combined and all]{.addu} external entity references are resolved. Library components are linked to satisfy external references to entities not defined in the current translation. All such translator output is collected into a program image which contains information needed for execution in its execution environment.

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

Add type aliases and namespace aliases to the list of entities in paragraph 3. As drive-by fixes, remove "variable", "object", "reference", and "template specialization"; replace "class member" with "non-static data member", since all other cases are subsumed by existing one. Add "template parameters" and "`$init-capture$`s", which collectively subsume "packs".

::: std
[3]{.pnum} An _entity_ is a [value, object, reference]{.rm} [variable,]{.addu} structured binding, function, enumerator, type, [type alias]{.addu}, [class]{.rm} [non-static data]{.addu} member, bit-field, template, template specialization, namespace, [namespace alias, template parameter]{.addu}, or [`$init-capture$`]{.addu} [pack]{.rm}.

:::

Introduce a notion of an "underlying entity" in paragraph 5, and utilize it for the definition of a name "denoting" an entity. Type aliases are now entities, so also modify accordingly.

::: std
[5]{.pnum} Every name is introduced by a _declaration_, which is a

* [#.#]{.pnum} `$name-declaration$`, `$block-declaration$`, or `$member-declaration$` ([dcl.pre]{.sref}, [class.mem]{.sref}),

[...]

* [#.14]{.pnum} implicit declaration of an injected-class-name ([class.pre]{.sref}).

[The _underlying entity_ of an entity is that entity unless otherwise specified. A name _denotes_ the underlying entity of the entity declared by each declaration that introduces the name.]{.addu} [An entity `$E$` is denoted by the name (if any) that is introduced by a declaration of `$E$` or by a `$typedef-name$` introduced by a declaration specifying `$E$`.]{.rm}

[[Type aliases and namespace aliases have underlying entities that are distinct from themselves.]{.note}]{.addu}
:::

Modify paragraph 6 such that denoting a variable by its name finds the variable, not the associated object.

::: std
[6]{.pnum} A _variable_ is introduced by the declaration of a reference other than a non-static data member or of an object. [The variable's name, if any, denotes the reference or object.]{.rm}
:::

### [basic.def]{.sref} Declarations and definitions {-}

Modify the third sentence of paragraph 1 to clarify that type aliases are now entities.

::: std
[1]{.pnum} [...] A declaration of an entity [or `$typedef-name$`]{.rm} `$X$` is a redeclaration of `$X$` if another declaration of `$X$` is reachable from it ([module.reach]{.sref}). [...]

:::

Since namespace aliases are now entities but their declarations are not definitions, add `$namespace-alias-definition$` to the list of declarations in paragraph 2, just before `$using-declaration$`. Also replace `$static_assert-declaration$` and `$empty-declaration$` with `$vacant-decalaration$`, which also encompasses consteval blocks:

::: std
[2]{.pnum} Each entity declared by a `$declaration$` is also _defined_ by that declaration unless:

* [#.#]{.pnum} it declares a function without specifying the function's body ([dcl.fct.def]),

[...]

* [2.10]{.pnum} it is an `$alias-declaration$` ([dcl.typedef]),
* [[2.11-]{.pnum} it is a `$namespace-alias-definition$` ([namespace.alias]),]{.addu}
* [2.11]{.pnum} it is a `$using-declaration$` ([namespace.udecl]),
* [2.12]{.pnum} it is a `$deduction-guide$` ([temp.deduct.guide]),
* [2.13]{.pnum} it is a [`$static_assert-declaration$`]{.rm} [`$vacant-declaration$`]{.addu} ([dcl.pre]),
* [2.14]{.pnum} it is an `$attribute-declaration$` ([dcl.pre]),
* [[2.15]{.pnum} it is an `$empty-declaration$` ([dcl.pre])]{.rm},

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
+ namesapce N1 = N;               // declares N1
  extern X anotherX;              // declares anotherX
  using N::d;                     // declares d
```
:::

### [basic.def.odr]{.sref} One-definition rule {-}

Add `$splice-expression$`s to the set of potential results of an expression in paragraph 3.

::: std
[3]{.pnum} An expression or conversion is _potentially evaluated_ unless it is an unevaluated operand ([expr.context]), a subexpression thereof, or a conversion in an initialization or conversion sequence in such a context. The set of _potential results_ of an expression `$E$` is defined as follows:

- [#.#]{.pnum} If `$E$` is an `$id-expression$` ([expr.prim.id]{.sref}) [or a `$splice-expression$` ([expr.prim.splice])]{.addu}, the set contains only `$E$`.
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

Break bullet 4.1 into sub-bullets, modify it to cover splicing of functions, and replace [basic.lookup] with [over.pre] since the canonical definition of "overload set" is relocated there by this proposal:

::: std
- [4.1]{.pnum} A function is named by an expression or conversion [`$E$`]{.addu} if it is the selected member of an overload set ([[basic.lookup]]{.rm} [[over.pre]]{.addu}, [over.match], [over.over]) in an overload resolution performed as part of forming that expression or conversion, unless it is a pure virtual function and [either the expression]{.rm}[\ ]{.addu}

  - [#.#.#]{.pnum} [`$E$`]{.addu} is not an `$id-expression$` naming the function with an explicitly qualified name[,]{.addu}
  - [[#.#.#]{.pnum} [`$E$`]{.addu} is a `$splice-expression$`,]{.addu} or
  - [#.#.#]{.pnum} [the expression]{.rm} [`$E$`]{.addu} forms a pointer to member ([expr.unary.op]).

:::

Modify the first sentence of paragraph 5 to cover splicing of variables:

::: std
- [5]{.pnum} A variable is named by an expression if the expression is an `$id-expression$` [or `$splice-expression$` ([expr.prim.splice])]{.addu} that designates it.
:::

Modify paragraph 6 to cover splicing of structured bindings:

::: std
- [6]{.pnum} A structured binding is [odr-used if it appears as a potentially-evaluated]{.rm} [named by an]{.addu} expression [if that expression is either an]{.addu} `$id-expression$` [or a `$splice-expression$` that designates that structured binding. A structured binding is odr-used if it is named by a potentially-evaluated expression.]{.addu}

:::

Modify paragraph 10.1 to prevent `*this` from being _odr-usable_ in a consteval block:

::: std
[10]{.pnum} A local entity ([basic.pre]) is _odr-usable_ in a scope ([basic.scope.scope]) if

- [#.#]{.pnum} either the local entity is not `*this`, or an enclosing class[, consteval block,]{.addu} or non-lambda function parameter scope exists and, if the innermost such scope is a function parameter scope, it corresponds to a non-static member function, and
- [#.#]{.pnum} for each intervening scope ([basic.scope.scope]) between the point at which the entity is introduced and the scope (where `*this` is considered to be introduced within the innermost enclosing class or non-lambda function defintion scope), either:
  - [#.#.#]{.pnum} the intervening scope is a block scope, or
  - [#.#.#]{.pnum} the intervening scope is the function parameter scope of a `$lambda-expression$` or `$requires-expression$`, or
  - [#.#.#]{.pnum} the intervening scope is the lambda scope of a `$lambda-expression$` that has a `$simple-capture$` naming the entity or has a `$capture-default$`, and the block scope of the `$lambda-expression$` is also an intervening scope.

If a local entity is odr-used in a scope in which it is not odr-usable, the program is ill-formed.

:::

Prepend before paragraph 15 of [basic.def.odr]{.sref}:

::: std

::: addu
[15pre]{.pnum} If a class `C` is defined in a translation unit as a result of a call to a specialization of `std::meta::define_aggregate` and another translation unit contains a definition of `C` that is not a result of calling the same specialization with the same function arguments, the program is ill-formed; a diagnostic is required only if `C` is attached to a named module and a prior definition is reachable at the point where a later definition occurs.

:::


[15]{.pnum} For any [other]{.addu} definable item `D` with definitions in multiple translation units,

* if `D` is a non-inline non-templated function or variable, or
* if the definitions in different translation units do not satisfy the following requirements,

the program is ill-formed; a diagnostic is required only if the definable item is attached to a named module and a prior definition is reachable at the point where a later definition occurs. [...]
:::

Prefer the verb "denote" in bullet 15.5 to emphasize that ODR "looks through" aliases, and clarify that objects are not entities in bullet 15.5.2.

::: std
- [15.5]{.pnum} In each such definition, corresponding names, looked up according to [basic.lookup]{.sref}, shall [refer to]{.rm} [denote]{.addu} the same entity, after overload resolution ([over.match]{.sref}) and after matching of partial template specialization ([temp.over]{.sref}), except that a name can refer to
  - [#.#.#]{.pnum} a non-volatile const object [...], or
  - [#.#.#]{.pnum} a reference with internal or no linkage initialized with a constant expression such that the reference refers to the same [entity]{.rm} [object or function]{.addu} in all definitions of `$D$`.

:::

Clarify in bullet 15.11 that default template-arguments in `$splice-specialization-specifier$`s also factor into ODR.

::: std
- [15.10]{.pnum} In each such definition, the overloaded operators referred to, the implicit calls to conversion functions, constructors, operator new functions and operator delete functions, shall refer to the same function.
- [15.11]{.pnum} In each such definition, a default argument used by an (implicit or explicit) function call or a default template argument used by an (implicit or explicit) `$template-id$`[,]{.addu} [or]{.rm} `$simple-template-id$`[, or `$splice-specialization-specifier$`]{.addu} is treated as if its token sequence were present in the definition of `$D$`; that is, the default argument or default template argument is subject to the requirements described in this paragraph (recursively).

:::

And add a bullet thereafter that factors the result of a `$reflect-expression$` into ODR.

::: std
::: addu
- [15.11+]{.pnum} In each such definition, corresponding `$reflect-expression$`s ([expr.reflect]) compute equivalent values ([expr.eq]{.sref}).

:::
:::

### [basic.scope.scope]{.sref} General {-}

Change bullet 4.2 to refer to the declaration of a "type alias" instead of a `$typedef-name$`.

::: std
[4.2]{.pnum} one declares a type (not a [`$typedef-name$`]{.rm} [type alias]{.addu}) and the other declares a variable, non-static data member other than an anonymous union ([class.union.anon]{.sref}), enumerator, function, or function template, or

:::

### 6.4.5+ [basic.scope.consteval] Consteval block scope {-}

Add a new section for consteval block scopes following [basic.scope.lambda]{.sref}:

::: std
::: addu
**Consteval block scope   [basic.scope.consteval]**

[#]{.pnum} A `$consteval-block-declaration$` `C` introduces a _consteval block scope_ that includes the `$compound-statement$` of `C`.

::: example
```cpp
consteval {
  int x;
  consteval {
  int x; // #1
  consteval {
    int x;  // OK, distinct variable from #1
  }
}
```
:::
:::
:::

### [basic.lookup.general]{.sref} General {-}

Modify paragraph 1 to cross-reference to the new definition of "overload set" given in [over.pre], rather than define it here.

::: std
[1]{.pnum} [...] If the declarations found by name lookup all denote functions or function templates, the declarations [are said to]{.rm} form an [_overload set_]{.rm} [overload set ([over.pre]{.sref})]{.addu}. Otherwise, [...]

:::

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

* [[#.#]{.pnum} If `T` is `std::meta::info` ([meta.reflection.synop]), its associated set of entities is the singleton containing the enumeration type `std::meta::operators` ([meta.reflection.operators]).]{.addu}

  [[The `std::meta::info` type is a type alias, so an explicit rule is needed to associate calls whose arguments are reflections with the namespace `std::meta`.]{.note}]{.addu}

* [#.#]{.pnum} If `T` is [a]{.rm} [any other]{.addu} fundamental type, its associated set of entities is empty.
* [#.#]{.pnum} If `T` is a class type ...

:::

### [basic.lookup.qual.general]{.sref} General {-}

Extend paragraph 1 to cover `$splice-specifier$`s:

::: std
[1]{.pnum} Lookup of an *identifier* followed by a `::` scope resolution operator considers only namespaces, types, and templates whose specializations are types. If a name, `$template-id$`, [`$splice-scope-specifier$`,]{.addu} or `$computed-type-specifier$` is followed by a `::`, it shall [either be a dependent `$splice-scope-specifier$` ([temp.dep.splice]) or it shall]{.addu} designate a namespace, class, enumeration, or dependent type, and the `::` is never interpreted as a complete nested-name-specifier.

:::

Add `$qualified-reflection-name$` ([expr.reflect]) to the list of non-terminals in bullet 2.4.5 that are "qualified names" in the presence of a  `$nested-name-specifier$`:

::: std
[2]{.pnum} [...] A _qualified name_ is

- [2.3]{.pnum} a member-qualified name or
- [#.#]{.pnum} the terminal name of
  - [#.#.#]{.pnum} a `$qualified-id$`,
  - [#.#.#]{.pnum} a `$using-declarator$`,
  - [#.#.#]{.pnum} a `$typename-specifier$`,
  - [#.#.#]{.pnum} a `$qualified-namespace-specifier$`, or
  - [#.#.#]{.pnum} a `$nested-name-specifier$`, [`$qualified-reflection-name$`, ]{.addu} `$elaborated-type-specifier$`, or `$class-or-decltype$` that has a `$nested-name-specifier$` ([expr.prim.id.qual]).

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

[1]{.pnum} The `$constant-expression$` of a `$splice-specifier$` shall be a converted constant expression of type `std::meta::info` ([expr.const]{.sref}). A `$splice-specifier$` whose converted `$constant-expression$` represents a construct `$X$` is said to _designate_ either

* [#.#]{.pnum} the underlying entity of `$X$` if `$X$` is an entity ([basic.pre]{.sref}), or
* [#.#]{.pnum} `$X$` otherwise.

[A `$splice-specifier$` is dependent if the converted `$constant-expression$` is value-dependent ([temp.dep.splice]).]{.note}

[#]{.pnum} The `$splice-specifier$` of a non-dependent `$splice-specialization-specifier$` shall designate a template.

[#]{.pnum} [A `<` following a `$splice-specifier$` is interpreted as the delimiter of a `$template-argument-list$` when the `$splice-specifier$` is preceded by `typename` or `template`, or when it appears in a type-only context ([temp.names]{.sref}).]{.note}

::: example
```cpp
constexpr int v = 1;
template <int V> struct TCls {
  static constexpr int s = V + 1;
};

using alias = [:^^TCls:]<[:^^v:]>;
  // OK, a splice-specialization-specifier with a splice-specifier
  // as a template argument

static_assert(alias::s == 2);
```
:::

:::
:::

### [basic.link]{.sref} Program and Linkage {-}

Add a bullet to paragraph 13 and handle `$splice-expression$`s in the existing bullets:

::: std

[13]{.pnum} A declaration `$D$` _names_ an entity `$E$` if

* [13.1]{.pnum} `$D$` contains a `$lambda-expression$` whose closure type is `$E$`,
* [13.1+]{.pnum} [`$D$` contains a manifestly constant-evaluated expression of type `std::meta::info` that represents `$E$`,]{.addu}
* [13.2]{.pnum} `$E$` is not a function or function template and `$D$` contains an `$id-expression$`, `$type-specifier$`, `$nested-name-specifier$`, `$template-name$`, or `$concept-name$` denoting `$E$` [or a `$splice-specifier$` or `$splice-expression$` designating `$E$`]{.addu}, or
* [13.#]{.pnum} `$E$` is a function or function template and `$D$` contains an expression that names `$E$` ([basic.def.odr]) or an `$id-expression$` [or `$splice-expression$`]{.addu} that refers to a set of overloads that contains `$E$`.

:::

Modify paragraph 15 to make all type aliases and namespace aliases explicitly TU-local.

::: std
An entity is _TU-local_ if it is

* [15.#]{.pnum} a type, function, variable, or template that [...]
* [[15.1+]{.pnum} a type alias or a namespace alias,]{.addu}
* [15.#]{.pnum} [...]

:::

[The below addition of "value or object of a TU-local type" is in part a drive-by fix to make sure that enumerators in a TU-local enumeration are also TU-local]{.ednote}

Extend the definition of _TU-local_ values and objects in p16 to include reflections:

::: std

[16]{.pnum} A value or object is _TU-local_ if

* [[16.0]{.pnum} it is of TU-local type,]{.addu}
* [16.1]{.pnum} it is, or is a pointer to, a TU-local function or the object associated with a TU-local variable, [or]{.rm}

:::addu
* [16.1+]{.pnum} it is a reflection representing either
  * [16.1+.#]{.pnum} an entity, value, or object that is TU-local, or
  * [16.1+.#]{.pnum} a direct base class relationship introduced by an exposure, or
:::
* [16.2]{.pnum} it is an object of class or array type and any of its subobjects or any of the objects or functions to which its non-static data members of reference type refer is TU-local and is usable in constant expressions.

:::

### [basic.types.general]{.sref} General {-}

Change the first sentence in paragraph 9 of [basic.types.general]{.sref} as follows:

::: std
[9]{.pnum} Arithmetic types ([basic.fundamental]{.sref}), enumeration types, pointer types, pointer-to-member types ([basic.compound]{.sref}), [`std::meta::info`,]{.addu} `std::nullptr_t`, and cv-qualified versions of these types are collectively called scalar types. ...
:::

Add a new paragraph at the end of [basic.types.general]{.sref} as follows:

::: std
::: addu

[12]{.pnum} A type is _consteval-only_ if it is either `std::meta::info` or a type compounded from a consteval-only type ([basic.compound]). Every object of consteval-only type shall be

  - [#.#]{.pnum} the object associated with a constexpr variable or a subobject thereof,
  - [#.#]{.pnum} a template parameter object ([temp.param]{.sref}) or a subobject thereof, or
  - [#.#]{.pnum} an object whose lifetime begins and ends during the evaluation of a core constant expression.

:::
:::

### [basic.fundamental]{.sref} Fundamental types {-}

Add new paragraphs before the last paragraph of [basic.fundamental]{.sref} as follows:

::: std
::: addu

[17 - 1]{.pnum} A value of type `std::meta::info` is called a _reflection_. There exists a unique _null reflection_; every other reflection is a representation of

* a value of structural type ([temp.param]{.sref}),
* an object with static storage duration ([basic.stc]{.sref}),
* a variable ([basic.pre]{.sref}),
* a structured binding ([dcl.struct.bind]{.sref}),
* a function ([dcl.fct]{.sref}),
* an enumerator ([dcl.enum]{.sref}),
* a type alias ([dcl.typedef]{.sref}),
* a type ([basic.types]{.sref}),
* a class member ([class.mem]{.sref}),
* an unnamed bit-field ([class.bit]{.sref}),
* a primary class template ([temp.pre]{.sref}),
* a function template ([temp.pre]{.sref}),
* a primary variable template ([temp.pre]{.sref}),
* an alias template ([temp.alias]{.sref}),
* a concept ([temp.concept]{.sref}),
* a namespace alias ([namespace.alias]{.sref}),
* a namespace ([basic.namespace.general]{.sref}),
* a direct base class relationship ([class.derived.general]{.sref}), or
* a data member description ([class.mem.general]{.sref}).

A reflection is said to _represent_ the corresponding construct.

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

constexpr auto r1 = std::meta::reflect_value(42);  // represents int value of 42

constexpr auto r2 = std::meta::reflect_object(arr[1]);  // represents int object

constexpr auto r3 = ^^arr;      // represents a variable
constexpr auto r4 = ^^a3;       // represents a structured binding
constexpr auto r5 = ^^fn;       // represents a function
constexpr auto r6 = ^^Enum::A;  // represents an enumerator
constexpr auto r7 = ^^Alias;    // represents a type alias
constexpr auto r8 = ^^S;        // represents a type
constexpr auto r9 = ^^S::mem;   // represents a class member

constexpr auto r10 = std::meta::members_of(^^S)[1];
    // represents an unnamed bit-field

constexpr auto r11 = ^^TCls;     // represents a class template
constexpr auto r12 = ^^TFn;      // represents a function template
constexpr auto r13 = ^^TVar;     // represents a variable template
constexpr auto r14 = ^^Concept;  // represents a concept
constexpr auto r15 = ^^NSAlias;  // represents a namespace alias
constexpr auto r16 = ^^NS;       // represents a namespace

constexpr auto r17 = std::meta::bases_of(^^S)[0];
    // represents a direct base class relationship

constexpr auto r18 = std::meta::data_member_spec(^^int, {.name="member"});
    // represents a data member description
```

:::

[17 - 2]{.pnum} *Recommended practice*: Implementations should not represent other constructs specified in this document, such as partial template specializations, attributes, placeholder types, statements, or expressions, as values of type `std::meta::info`.

[17 - 3]{.pnum} [Future revisions of this document can specify semantics for reflections representing any such constructs.]{.note}

:::
:::

### [intro.execution]{.sref} Sequential execution {-}

Introduce a new kind of side effect in paragraph 7 (i.e., injecting a declaration).

::: std
[7]{.pnum} Reading an object designated by a `volatile` glvalue ([basic.lval]), modifying an object, [producing an injected declaration ([expr.const]),]{.addu} calling a library I/O function, or calling a function that does any of those operations are all _side effects_, which are changes in the state of the execution [or translation]{.addu} environment. _Evaluation_ of an expression (or a subexpression) in general includes both value computations (including determining the identity of an object for glvalue evaluation and fetching a value previously assigned to an object for prvalue evaluation) and initiation of side effects. When a call to a library I/O function returns or an access through a volatile glvalue is evaluated, the side effect is considered complete, even though some external actions implied by the call (such as the I/O itself) or by the `volatile` access may not have completed yet.

:::

Add a new paragraph to the end of [intro.execution] specifying a stronger sequencing during constant evaluation.

::: std
[12]{.pnum} If a signal handler is executed as a result of a call to the `std::raise` function, then the execution of the handler is sequenced after the invocation of the `std::raise` function and before its return.

::: addu
[12+]{.pnum} During the evaluation of an expression as a core constant expression ([expr.const]), evaluations of operands of individual operators and of subexpressions of individual expresssions that are otherwise either unsequenced or indeterminately sequenced are evaluated in lexical order.
:::

:::

### [basic.start.static]{.sref} Static initialization {-}

Clarify in paragraph 1 that variables with static storage duration can be initialized during translation.

::: std
[1]{.pnum} Variables with static storage duration are initialized [either during translation or]{.addu} as a consequence of program initiation. Variables with thread storage duration are initialized as a consequence of thread execution. Within each of these phases of initiation, initialization occurs as follows.

:::

### [basic.lval]{.sref} Value category {-}

Apply a drive-by fix to bullet 1.1 clarifying that a glvalue can also determine the identity of a non-static data member.

::: std
* [1.1]{.pnum} A _glvalue_ is an expression whose evaluation determines the identity of an object[,]{.addu} [or]{.rm} function[, or non-static data member]{.addu}.

:::

Account for move-eligible `$splice-expression$`s in bullet 4.1 of Note 3.

::: std
* [4.1]{.pnum} a move-eligible `$id-expression$` [or `$splice-expression$`]{.addu} ([expr.prim.id.unqual]),

:::

### [expr.context]{.sref} Context dependence {-}

Add `$reflect-expression$`s to the list of unevaluated operands in paragraph 1.

::: std
[1]{.pnum} In some contexts, _unevaluated operands_ appear ([expr.prim.req], [expr.typeid], [expr.sizeof], [expr.unary.noexcept], [[expr.reflect],]{.addu} [temp.pre], [temp.concept]). An unevaluated operand is not evaluated.

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
[2]{.pnum} If an `$id-expression$` `$E$` denotes a non-static non-type member of some class `C` at a point where the current class ([expr.prim.this]{.sref}) is `X` and

* [#.#]{.pnum} `$E$` is potentially evaluated or `C` is `X` or a base class of `X`, and
* [#.#]{.pnum} `$E$` is not the `$id-expression$` of a class member access expression ([expr.ref]{.sref}), and
* [[#.#]{.pnum} `$E$` is not the `$id-expression$` of a `$reflect-expression$` ([expr.reflect]), and]{.addu}
* [#.#]{.pnum} if `$E$` is a `$qualified-id$`, `$E$` is not the un-parenthesized operand of the unary `&` operator ([expr.unary.op]{.sref}),

the `$id-expression$` is transformed into a class member access expression using `(*this)` as the object expression.

:::

### [expr.prim.id.unqual]{.sref} Unqualified names {-}

Modify paragraph 4 to allow `$splice-expression$`s to be move-eligible:

::: std
[4]{.pnum} An _implicitly movable entity_ is a variable of automatic storage duration that is either a non-volatile object or an rvalue reference to a non-volatile object type. In the following contexts, [an]{.rm} [a (possibly parenthesized)]{.addu} `$id-expression$` [or `$splice-expression$` ([expr.prim.splice]) `$E$`]{.addu} is _move-eligible_:

- [#.#]{.pnum} If [the `$id-expression$` (possibly parenthesized)]{.rm} [`$E$`]{.addu} is an operand of a `return` ([stmt.return]) or `co_return` ([stmt.return.coroutine]) statement, and names an implicitly movable entity declared in the body or `$parameter-declaration-clause$` of the innermost enclosing function or `$lambda-expression$`, or
- [#.#]{.pnum} if [the `$id-expression$` (possibly parenthesized)]{.rm} [`$E$`]{.addu} is the operand of a `$throw-expression$` ([expr.throw]) and names an implicitly movable entity that belongs to a scope that does not contain the `$compound-statement$` of the innermost `$lambda-expression$`, `$try-block$`, or `$function-try-block$` (if any) whose `$compound-statement$` or `$ctor-initializer$` contains the `$throw-expression$`.

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
[1+]{.pnum} A `$splice-specifier$` or `$splice-specialization-specifier$` that is not followed by `::` is never interpreted as part of a `$splice-scope-specifier$`. The `template` may only be omitted from the form `template@~_opt_~@ $splice-specialization-specifier$ ::` when the `$splice-specialization-specifier$` is preceded by `typename`.

::: example
```cpp
template <int V>
struct TCls {
  static constexpr int s = V;
  using type = int;
};

constexpr int v1 = [:^^TCls<1>:]::s;
constexpr int v2 = template [:^^TCls:]<2>::s;
  // OK, template binds to splice-scope-specifier

constexpr typename [:^^TCls:]<3>::type v3 = 3;
  // OK, typename binds to the qualified name

constexpr [:^^TCls:]<3>::type v4 = 4;
  // error: [:^^TCls:]< is parsed as a splice-expression followed
  // by a comparison operator
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

[Here and in a few other places, the wording for the entity represented by a `$splice-specialization-specifier$` is complicated. This is primarily because the possibility that such a construct might form a `$concept-id$` (which is a prvalue) precludes us from generically saying that a `$splice-specialization-specifier$` designates an entity.]{.draftnote}

::: std
[3]{.pnum} [The entity designated by a `$nested-name-specifier$` is determined as follows:]{.addu}

  - [#.#]{.pnum} The `$nested-name-specifier$` `::` [nominates]{.rm} [designates]{.addu} the global namespace.[\ ]{.addu}
  - [#.#]{.pnum} A `$nested-name-specifier$` with a `$computed-type-specifier$` [nominates]{.rm} [designates]{.addu} the [same]{.addu} type [denoted]{.rm} [designated]{.addu} by the `$computed-type-specifier$`, which shall be a class or enumeration type.[\ ]{.addu}
  - [[#.#]{.pnum} For a `$nested-name-specifier$` of the form `$splice-specifier$ ::`, the `$splice-specifier$` shall designate a class or enumeration type or a namespace. The `$nested-name-specifier$` designates the same entity as the `$splice-specifier$`.]{.addu}
  - [[#.#]{.pnum} For a `$nested-name-specifier$` of the form `template@~_opt_~@ $splice-specialization-specifier$ ::`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a primary class template or an alias template `$T$`. Letting `$S$` be the specialization of `$T$` corresponding to the `$template-argument-list$` (if any) of the `$splice-specialization-specifier$`, `$S$` shall either be a class template specialization or an alias template specialization that denotes a class or enumeration type. The `$nested-name-specifier$` designates `$S$` if `$T$` is a class template or the type denoted by `$S$` if `$T$` is an alias template.]{.addu}
  - [#.#]{.pnum} If a `$nested-name-specifier$` _N_ is declarative and has a `$simple-template-id$` with a template argument list _A_ that involves a template parameter, let _T_ be the template [nominated]{.rm} [designated]{.addu} by _N_ without _A_. _T_ shall be a class template.
    - [#.#.#]{.pnum} If `$A$` is the template argument list ([temp.arg]{.sref}) of the corresponding `$template-head$` `$H$` ([temp.mem]{.sref}), `$N$` [nominates]{.rm} [designates]{.addu} the primary template of `$T$`; `$H$` shall be equivalent to the `$template-head$` of `$T$` ([temp.over.link]{.sref}).
    - [#.#.#]{.pnum} Otherwise, `$N$` [nominates]{.rm} [designates]{.addu} the partial specialization ([temp.spec.partial]{.sref}) of `$T$` whose template argument list is equivalent to `$A$` ([temp.over.link]{.sref}); the program is ill-formed if no such partial specialization exists.

  - [#.#]{.pnum} Any other `$nested-name-specifier$` [nominates]{.rm} [designates]{.addu} the entity [denoted]{.rm} [designated]{.addu} by its `$type-name$`, `$namespace-name$`, `$identifier$`, or `$simple-template-id$`. If the `$nested-name-specifier$` is not declarative, the entity shall not be a template.

:::

### [expr.prim.lambda.capture]{.sref} Captures {-}

Modify bullet 7.1 as follows:

::: std
- [7.1]{.pnum} An `$id-expression$` [or `$splice-expression$`]{.addu} that names a local entity potentially references that entity; an `$id-expression$` that names one or more non-static class members and does not form a pointer to member ([expr.unary.op]) potentially references `*this`.

:::

And extend the example following paragraph 7 with uses of expression splices:

::: std
::: example4
```cpp
void f(int, const int (&)[2] = {});       // #1
void f(const int &, const int (&)[1]);    // #2

void test() {
  const int x = 17;
  auto g = [](auto a) {
    f(x);                    // OK, calls #1, does not capture x
@[\ ]{.addu}@
@[\ \ \ \ `constexpr auto r = ^^x;  // OK, unevaluated operand does not capture x`]{.addu}@
@[\ \ \ \ `f([:r:]);                // OK, calls #1, also does not capture x`]{.addu}@
  }

  auto g1 = [=](auto a) {
    f(x);                   // OK, calls #1, captures x
@[\ \ \ \ `f([:^^x:]);             // OK, calls #1, also captures x`]{.addu}@
  }
}
```

:::
:::

Modify paragraph 11 (and note 7 which follows):

::: std
- [11]{.pnum} An `$id-expression$` [or `$splice-expression$`]{.addu} within the `$compound-statement$` of a `$lambda-expression$` that is an odr-use of an entity captured by copy is transformed into an access to the corresponding unnamed data member of the closure type.

  [An `$id-expression$` [or `$splice-expression$`]{.addu} that is not an odr-use refers to the original entity, never to a member of the closure type. However, such an `$id-expression$` can still cause the implicit capture of the entity.]{.note7}
:::

And extend the example following paragraph 11 with uses of expression splices:

::: std
::: example8
```cpp
void f(const int *);
void g() {
  const int N = 10;
  [=] {
    int arr[N];      // OK, not an odr-use, refers to variable with automatic storage duration
    f(&N);           // OK, causes N to be captured; &N points to
                     // the corresponding member of the closure type
@[\ ]{.addu}@
@[\ \ \ \ `f(&[:^^N:])      // OK, also causes N to be captured`]{.addu}@
  }
}
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

constexpr int c = [:^^S:]::a;                   // [:^^S:] is not an expression

constexpr int d = template [:^^TCls:]<int>::b;  // template [:^^TCls:]<int> is not
                                                // an expression

template <auto V> constexpr int e = [:V:];   // splice-expression
constexpr int f = template [:^^e:]<^^S::a>;  // splice-expression

auto g = typename [:^^int:](42);
  // [:^^int:] forms part of a type, not a splice-expression
```

:::

[#]{.pnum} For a `$splice-expression$` of the form `$splice-specifier$`, let `$S$` be the construct designated by `$splice-specifier$`.

* [#]{.pnum} If `$S$` is a function, overload resolution ([over.match], [temp.over]) is performed from an initial set of candidate functions containing only that function. The expression is an lvalue referring to the selected function and has the same type as that function.

* [#.#]{.pnum} Otherwise, if `$S$` is an object or a non-static data member, the expression is an lvalue designating `$S$`. The expression has the same type as `$S$`, and is a bit-field if and only if `$S$` is a bit-field.

* [#.#]{.pnum} Otherwise, if `$S$` is a variable or a structured binding, `$S$` shall either have static or thread storage duration or shall inhabit a scope enclosing the expression. The expression is an lvalue referring to the object or function `$X$` associated with or referenced by `$S$`, has the same type as `$S$`, and is a bit-field if and only if `$X$` is a bit-field.

  [The type of a `$splice-expression$` designating a variable or structured binding of reference type will be adjusted to a non-reference type ([expr.type]{.sref}).]{.note}

* [#.#]{.pnum} Otherwise, if `$S$` is a value or an enumerator, the expression is a prvalue that computes `$S$` and whose type is the same as `$S$`.

* [#.#]{.pnum} Otherwise, the expression is ill-formed.

[#]{.pnum} For a `$splice-expression$` of the form  `template $splice-specifier$`, the `$splice-specifier$` shall designate a function template. Overload resolution is performed from an initial set of candidate functions containing only that function template. The expression is an lvalue referring to the selected function and has the same type as that function.

[#]{.pnum} For a `$splice-expression$` of the form `template $splice-specialization-specifier$`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a template. Let `$T$` be that template and let `$S$` be the specialization of `$T$` corresponding to the `$template-argument-list$` (if any) of the `$splice-specialization-specifier$`.

* [#.#]{.pnum} If `$T$` is a function template, overload resolution is performed from an initial set of candidate functions containing only the function associated with `$S$`. The expression is an lvalue referring to the selected function and has the same type as that function.

* [#.#]{.pnum} Otherwise, if `$T$` is a primary variable template, the expression is an lvalue referring to the same object associated with `$S$` and has the same type as `$S$`.

* [#.#]{.pnum} Otherwise, if `$T$` is a concept, the expression is a prvalue that computes the same boolean value as the `$concept-id$` formed by `$S$`.

* [#.#]{.pnum} Otherwise, the expression is ill-formed.

[Access checking of class members occurs during lookup, and therefore does not pertain to splicing.]{.note}

[#]{.pnum} A `$splice-expression$` that designates a non-static data member or implicit object member function of a class can only be used:

* [#.#]{.pnum} as part of a class member access in which the object expression refers to the member's class or a class derived from that class,
* [#.#]{.pnum} to form a pointer to member ([expr.unary.op]{.sref}), or
* [#.#]{.pnum} if that `$splice-expression$` designates a non-static data member and it appears in an unevaluated operand.

[The implicit transformation ([expr.prim.id]{.sref}) whereby an `$id-expression$` denoting a non-static member becomes a class member access does not apply to a `$splice-expression$`.]{.note}

[#]{.pnum} While performing overload resolution to determine the entity referred to by a `$splice-expression$`, the best viable function is _designated in a manner exempt from access rules_.

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

Modify paragraph 1 to account for splices in member access expressions:

::: std
[1]{.pnum} A postfix expression followed by a dot `.` or an arrow `->`, optionally followed by the keyword `template`, and then followed by an `$id-expression$` [or a `$splice-expression$`]{.addu}, is a postfix expression.

[If the keyword `template` is used, the following unqualified name is considered to refer to a template ([temp.names]). If a `$simple-template-id$` results and is followed by a `::`, the `$id-expression$` is a `$qualified-id$`.]{.note}

:::

Modify paragraph 2 to account for splices in member access expressions:

::: std
[2]{.pnum} For [the first option, if the]{.rm} [a dot that is followed by an]{.addu} `$id-expression$` [names]{.rm} [ or `$splice-expression$` that designates]{.addu} a static member or an enumerator, the first expression is a discarded-value expression ([expr.context]{.sref}); if the `$id-expression$` [or `$splice-expression$` designates]{.addu} [names]{.rm} a non-static data member, the first expression shall be a glvalue. [For the second option (arrow), the first expression]{.rm} [A postfix expression that is followed by an arrow]{.addu} shall be a prvalue having pointer type. The expression `E1->E2` is converted to the equivalent form `(*(E1)).E2`; the remainder of [expr.ref] will address only [the first option (dot)]{.rm} [the form using a dot]{.addu}.
:::

Modify paragraph 3 to account for splices in member access expressions:

::: std
[3]{.pnum} The postfix expression before the dot is evaluated; the result of that evaluation, together with the `$id-expression$` [or `$splice-expression$`]{.addu}, determines the result of the entire postfix expression.
:::

Modify paragraph 4 to account for splices in member access expressions:

::: std
[4]{.pnum} Abbreviating `$postfix-expression$`.`$id-expression$` [or `$postfix-expression$.$splice-expression$`]{.addu} as `E1.E2`, `E1` is called the `$object expression$`. [...]

:::

Adjust the language in paragraphs 6-9 to account for `$splice-expression$`s. Explicitly add a fallback to paragraph 7 that makes other cases ill-formed.

::: std

[6]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a bit-field, `E1.E2` is a bit-field. [...]

[7]{.pnum} If `E2` [designates an entity that]{.addu} is declared to have type "reference to `T`", then `E1.E2` is an lvalue of type `T`. [If]{.rm} [In that case, if]{.addu} `E2` [is]{.rm} [designates]{.addu} a static data member, `E1.E2` designates the object or function to which the reference is bound, otherwise `E1.E2` designates the object or function to which the corresponding reference member of `E1` is bound. Otherwise, one of the following rules applies.

* [#.#]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a static data member and the type of `E2` is `T`, then `E1.E2` is an lvalue; [...]
* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a non-static data member and the type of `E1` is "_cq1_ _vq1_ `X`", and the type of `E2` is "_cq2 vq2_ `T`", [...]. If [the entity designated by]{.addu} `E2` is declared to be a `mutable` member, then the type of `E1.E2` is "_vq12_ `T`". If [the entity designated by]{.addu} `E2` is not declared to be a `mutable` member, then the type of `E1.E2` is "_cq12_ _vq12_ `T`".

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` is an overload set, [...]

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a nested type, the expression `E1.E2` is ill-formed.

* [#.#]{.pnum} [Otherwise, if]{.addu} [If]{.rm} `E2` [is]{.rm} [designates]{.addu} a member enumerator and the type of `E2` is `T`, the expression `E1.E2` is a prvalue of type `T` whose value is the value of the enumerator.

* [[#.#]{.pnum} Otherwise, the program is ill-formed.]{.addu}

[8]{.pnum} If `E2` [is]{.rm} [designates]{.addu} a non-static member, the program is ill-formed if the class of which `E2` [is directly]{.rm} [designates]{.addu} a member is an ambiguous base ([class.member.lookup]{.sref}) of the naming class ([class.access.base]{.sref}) of `E2`.

[9]{.pnum} If [the entity designated by]{.addu} `E2` is a non-static member and the result of `E1` is an object whose type is not similar ([conv.qual]) to the type of `E1`, the behavior is undefined.

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

Modify paragraphs 3 and 4 to permit forming a pointer-to-member with a splice.

::: std
[3]{.pnum} The operand of the unary `&` operator shall be an lvalue of some type `T`.

* [#.#]{.pnum} If the operand is a `$qualified-id$` [or `$splice-expression$`]{.addu} [naming]{.rm} [designating]{.addu} a non-static or variant member of some class `C`, other than an explicit object member function, the result has type "pointer to member of class `C` of type `T`" and designates `C::m`.

* [#.#]{.pnum} Otherwise, the result has type "pointer to `T`" and points to the designated object ([intro.memory]{.sref}) or function ([basic.compound]{.sref}). If the operand designates an explicit object member function ([dcl.fct]{.sref}), the operand shall be a `$qualified-id$` [or a `$splice-expression$`]{.addu}.

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
   ^^ $qualified-reflection-name$
   ^^ $type-id$
   ^^ $id-expression$

$qualified-reflection-name$:
  $nested-name-specifier$@~_opt_~@ $identifier$
```

[#]{.pnum} The unary `^^` operator, called the _reflection operator_, yields a prvalue of type `std::meta::info` ([basic.fundamental]{.sref}).

[Constructs not described by this document can also be represented by reflections, and can appear as operands of `$reflect-expression$`s.]{.note}

[#]{.pnum} A `$reflect-expression$` is parsed as the longest possible sequence of tokens that could syntactically form a `$reflect-expression$`.

::: example
```
static_assert(std::meta::is_type(^^int()));  // ^^ applies to the type-id "int()"

template<bool> struct X {};
consteval bool operator<(std::meta::info, X<false>) { return false; }
consteval void g(std::meta::info r, X<false> xv) {
  r == ^^int && true;    // error: ^^ applies to the type-id "int&&"
  r == ^^int & true;     // error: ^^ applies to the type-id "int&"
  r == (^^int) && true;  // OK
  r == ^^int &&&& true;  // error: 'int &&&&' is not a valid type
  ^^X < xv;              // OK
  (^^X) < xv;            // OK
}

```
:::

[#]{.pnum} A `$reflect-expression$` of the form `^^ ::` represents the global namespace.

[#]{.pnum} If a `$reflect-expression$` `$R$` matches the form `^^ $qualified-reflection-name$`, it is interpreted as such and represents the entity named by the `$identifier$` of `R` as follows:

- [#.#]{.pnum} If the `$identifier$` is a `$namespace-name$` that names a namespace alias ([namespace.alias]), `$R$` represents that namespace alias. For any other `$namespace-name$`, `$R$` represents the denoted namespace.
- [#.#]{.pnum} Otherwise, if the `$identifier$` is a `$concept-name$` ([temp.concept]), `$R$` represents the denoted concept.
- [#.#]{.pnum} Otherwise, if the `$identifier$` is a `$template-name$` ([temp.names]) that is an injected-class-name ([class.pre]), the injected-class-name is instead considered a `$type-name$` ([temp.local]) and `$R$` represents the class template specialization so named. For any other `$template-name$` denoting a primary class template, function template, primary variable template, or alias template, `$R$` represents that template.

- [#.#]{.pnum} Otherwise, if the `$identifier$` names a type alias that was introduced by the declaration of a template parameter, `$R$` represents the underlying entity of that type alias. For any other `$identifier$` that names a type alias, `$R$` represents that type alias.
- [#.#]{.pnum} Otherwise, if the `$identifier$` is a `$class-name$` or an `$enum-name$`, `$R$` represents the denoted type.
- [#.#]{.pnum} Otherwise, the `$qualified-reflection-name$` shall be an `$id-expression$` `$I$` and `$R$` is `^^ $I$` (see below).

[#]{.pnum} A `$reflect-expression$` of the form `^^ $type-id$` represents an entity determined as follows:

- [#.#]{.pnum} If the `$type-id$` names a type alias that is a specialization of an alias template, `$R$` represents that type alias.
- [#.#]{.pnum} Otherwise, `$R$` represents the type denoted by the `$type-id$`.

[All other cases for `^^ $type-id$` are covered by `$qualified-reflection-name$`.]{.ednote}

[#]{.pnum} A `$reflect-expression$` `$R$` of the form `^^ $id-expression$` represents an entity determined as follows:

  * [#.#]{.pnum} If the `$id-expression$` denotes an overload set `$S$`, overload resolution for the expression `&$S$` with no target shall select a unique function ([over.over]{.sref}); `$R$` represents that function.

  * [#.#]{.pnum} Otherwise, if the `$id-expression$` denotes a local entity captured by an enclosing `$lambda-expression$`, `$R$` is ill-formed.

  * [#.#]{.pnum} Otherwise, if the `$id-expression$` denotes a variable, structured binding, enumerator, or non-static data member, `$R$` represents that entity.

  * [#.#]{.pnum} Otherwise, `$R$` is ill-formed. [This includes `$pack-index-expression$`s and non-type template parameters.]{.note}

  The `$id-expression$` of a `$reflect-expression$` is an unevaluated operand ([expr.context]{.sref}).

::: example
```cpp
template <typename T> void fn() requires (^^T != ^^int);
template <typename T> void fn() requires (^^T == ^^int);
template <typename T> void fn() requires (sizeof(T) == sizeof(int));

constexpr auto a = ^^fn<char>;     // OK
constexpr auto b = ^^fn<int>;      // error: ambiguous

constexpr auto c = ^^std::vector;  // OK

template <typename T>
struct S {
  static constexpr auto r = ^^T;
  using type = T;
}
static_assert(S<int>::r == ^^int);
static_assert(^^S<int>::type != ^^int);

typedef struct X {} Y;
typedef struct Z {} Z;
constexpr auto e = ^^Y;  // OK, represents the type alias Y
constexpr auto f = ^^Z;  // OK, represents the type Z (not the type alias)
```
:::

:::

:::

### [expr.eq]{.sref} Equality Operators {-}

Extend paragraph 2 to also handle `std::meta::info`:

::: std
[2]{.pnum} The converted operands shall have arithmetic, enumeration, pointer, or pointer-to-member type, [type `std::meta::info`,]{.addu} or type `std::nullptr_t`. The operators `==` and `!=` both yield `true` or `false`, i.e., a result of type `bool`. In each case below, the operands shall have the same type after the specified conversions have been applied.

:::

Add a new paragraph between paragraphs 5 and 6:

::: std
[5]{.pnum} Two operands of type `std::nullptr_t` or one operand of type `std::nullptr_t` and the other a null pointer constant compare equal.

::: addu
[5+]{.pnum} If both operands are of type `std::meta::info`, comparison is defined as follows:

* [5+.#]{.pnum} If one operand is a null reflection value, then they compare equal if and only if the other operand is also a null reflection value.
* [5+.#]{.pnum} Otherwise, if one operand represents a value, then they compare equal if and only if the other operand represents a value that is template-argument-equivalent ([temp.type]{.sref}).
* [5+.#]{.pnum} Otherwise, if one operand represents an object, then they compare equal if and only if the other operand represents the same object.
* [5+.#]{.pnum} Otherwise, if one operand represents an entity, then they compare equal if and only if the other operand represents the same entity.
* [5+.#]{.pnum} Otherwise, if one operand represents a direct base class relationship, then they compare equal if and only if the other operand represents the same direct base class relationship.
* [5+.#]{.pnum} Otherwise, both operands `O@~_1_~@` and `O@~_2_~@` represent data member descriptions. The operands compare equal if and only if the data member descriptions represented by `O@~_1_~@` and `O@~_2_~@` compare equal ([class.mem.general]{.sref}).
:::

[6]{.pnum} If two operands compare equal, the result is `true` for the `==` operator and `false` for the `!=` operator. If two operands compare unequal, the result is `false` for the `==` operator and `true` for the `!=` operator. Otherwise, the result of each of the operators is unspecified.
:::


### [expr.const]{.sref} Constant Expressions {-}

Modify paragraph 4 to account for local variables in consteval block scopes:

::: std
[4]{.pnum} An object `$o$` is _constexpr-referenceable_ from a point `$P$` if

- [#.#]{.pnum} `$o$` has static storage duration, or
- [#.#]{.pnum} `$o$` has automatic storage duration, and letting `v` denote
  - [#.#.#]{.pnum} the variable corresponding to `$o$`'s complete object or
  - [#.#.#]{.pnum} the variable whose lifetime that of `$o$` is extended,

  the smallest scope enclosing `v` and the smallest scope enclosing `$P$` that are neither
  - [#.#.#]{.pnum} block scopes nor
  - [#.#.#]{.pnum} function parameter scopes associated with a `$requirement-parameter-list$`

  are the same function parameter [or consteval block scope]{.addu}.

:::

Add a bullet to paragraph 10 between 10.27 and 10.28 to disallow the production of injected declarations from any core constant expression that isn't a consteval block.

::: std
[10]{.pnum} An expression `$E$` is a _core constant expression_ unless the evaluation of `$E$`, following the rules of the abstract machine ([intro.execution]), would evaluate one of the following:

[...]

- [#.27]{.pnum} a `dynamic_cast` ([expr.dynamic.cast]) expression, `typeid` ([expr.typeid]) expression, or `$new-expression$` ([expr.new]) that would throw an exception where no definition of the exception type is reachable;
- [[#.27+]{.pnum} an expression that would produce an injected declaration, unless `$E$` is the evaluating expression of a `$consteval-block-declaration$` ([dcl.pre]);]{.addu}
- [#.28]{.pnum} an `$asm-declaration$` ([dcl.asm]);
- [#.#]{.pnum} [...]

:::

Modify paragraph 17 to mention `$splice-expression$`s:

::: std
[17]{.pnum} During the evaluation of an expression `$E$` as a core constant expression, all `$id-expression$`s[, `$splice-expression$`s,]{.addu} and uses of `*this` that refer to an object or reference whose lifetime did not begin with the evaluation of `$E$` are treated as referring to a specific instance of that object or reference whose lifetime and that of all subobjects (including all union members) includes the entire constant evaluation. [...]

:::

Modify paragraph 22 to disallow returning non-consteval-only pointers and references to consteval-only objects from constant expressions.

::: std
[22]{.pnum} A _constant expression_ is either a glvalue core constant expression [`$E$`]{.addu} that [\ ]{.addu}

* [#.#]{.pnum} refers to an object or a non-immediate function[, and]{.addu}
* [[#.#]{.pnum} if `$E$` designates a function of consteval-only type ([basic.types.general]{.sref}) or an object whose complete object is of consteval-only type, then `$E$` is also of consteval-only type,]{.addu}

  ::: addu
  ::: example
  ```cpp
  struct Base { };
  struct Derived : Base { std::meta::info r; };

  consteval const Base& fn(const Derived& derived) { return derived; }

  constexpr auto obj = Derived{^^::}; // OK
  constexpr auto const& d = obj; // OK
  constexpr auto const& b = fn(obj); // error: not a constant expression
    // because Derived is a consteval-only type but Base is not.
  ```

  :::
  :::

or a prvalue core constant expression whose value satisfies the following constraints:

* [#.#]{.pnum} each constituent reference refers to an object or a non-immediate function,
* [#.#]{.pnum} no constituent value of scalar type is an indeterminate value ([basic.indet]{.sref}),
* [#.#]{.pnum} no constituent value of pointer type is a pointer to an immediate function or an invalid pointer value ([basic.compound]), [and]{.rm}
* [#.#]{.pnum} no constituent value of pointer-to-member type designates an immediate function[.]{.rm}[, and]{.addu}

::: addu
* [#.#]{.pnum} unless the value is of consteval-only type, no constituent value of pointer or pointer-to-member type is
  * a pointer to a function or member of consteval-only type, or
  * a pointer to or past an object whose complete object has consteval-only type,

  nor does any constituent reference refer to an object or function of consteval-only type.
:::

:::

Add consteval block scopes to the scopes that introduce an immediate function context:

::: std
[24]{.pnum} An expression or conversion is in an _immediate function context_ if it is potentially evaluated and either:

- [#.#]{.pnum} its innermost enclosing non-block scope is [either]{.addu} a function parameter scope of an immediate function [or a consteval block scope]{.addu},
- [#.#]{.pnum} [...]

:::

Modify (and clean up) the definition of _immediate-escalating expression_ in paragraph 25 to also apply to expressions of consteval-only type.

::: std
[25]{.pnum} A[n]{.rm} [potentially-evaluated]{.addu} expression or conversion is _immediate-escalating_ if it is [not]{.rm} [neither]{.addu} initially in an immediate function context [nor a subexpression of an immediate invocation,]{.addu} and it is [either]{.rm}

* [#.#]{.pnum} [a potentially-evaluated]{.rm} [an]{.addu} `$id-expression$` [or `$splice-expression$`]{.addu} that [denotes]{.rm} [designates]{.addu} an immediate function[,]{.addu} [that is not a subexpression of an immediate invocation, or]{.rm}
* [#.#]{.pnum} an immediate invocation that is not a constant expression[, or]{.addu} [and is not a subexpression of an immediate invocation.]{.rm}
* [[#.#]{.pnum} of consteval-only type ([basic.types.general]{.sref}).]{.addu}

:::

Extend the definition of _immediate function_ in paragraph 27 to include functions containing a declaration of a variable of consteval-only type.

::: std
[27]{.pnum} An _immediate function_ is a function or constructor that is [either]{.addu}

* [#.#]{.pnum} declared with the `consteval` specifier, or
* [#.#]{.pnum} an immediate-escalating function [`$F$`]{.rm} whose function [body contains]{.rm} [parameter scope is the innermost non-block scope enclosing either]{.addu}
  * [#.#.#]{.pnum} an immediate-escalating expression [`$E$` such that `$E$`'s innermost enclosing non-block scope is `$F$`'s function parameter scope.]{.rm}[, or]{.addu}
  * [[#.#.#]{.pnum} a definition of a non-constexpr variable with consteval-only type.]{.addu}
:::

After the example following the definition of _manifestly constant-evaluated_, introduce new terminology and rules for injecting declarations and renumber accordingly:

::: std
::: addu

[#]{.pnum} The evaluation of an expression can introduce one or more _injected declarations_. Each such declaration has an associated _synthesized point_ which follows the last non-synthesized program point in the translation unit containing that declaration. The evaluation is said to _produce_ the declaration.

[Special rules concerning reachability apply to synthesized points ([module.reach]{.sref}).]{.note13}

[#]{.pnum} Let `$C$` be a `$consteval-block-declaration$` whose evaluating expression produces an injected declaration `$D$` ([expr.const]). The program is ill-formed if a scope encloses exactly one of `$C$` or `$D$` that is either

* [#.#]{.pnum} a function parameter scope,
* [#.#]{.pnum} a class scope, or
* [#.#]{.pnum} a consteval block scope.

::: example
```cpp
consteval void complete_type(std::meta::info r) {
  std::meta::define_aggregate(r, {});
}

struct S1;
consteval { complete_type(^^S1); }  // OK

template <std::meta::info R> consteval void tfn1() {
  complete_type(R);
}

struct S2;
consteval { tfn1<^^S2>(); }
  // OK, tfn1<^^S2>() and S2 are enclosed by the same scope

template <std::meta::info R> consteval void tfn2() {
  consteval { complete_type(R); }
  return b;
}

struct S3;
consteval { tfn2<^^S3>(); }
  // error: complete_type(^^S3) is enclosed tfn2<^^S3>, but S3 is not

template <typename> struct TCls {
  struct S4;
  static void sfn() requires ([] {
    consteval { complete_type(^^S4); }
    return true;
  }) { }
};

consteval { TCls<void>::sfn(); }
  // error: TCls<void>::S4 is not enclosed by requires-clause lambda

struct S5;
struct Cls {
  consteval { complete_type(^^S5); }
    // error: S5 is not enclosed by class Cls
};

struct S6;
consteval { // #1
  struct S7;
  consteval { // #2
    define_aggregate(^^S6, {});
      // error: consteval block #1 encloses consteval block #2 but not S6
    define_aggregate(^^S7, {});  // OK, consteval block #1 encloses both #2 and S7
  }
}
```
:::

[#]{.pnum} The _evaluation context_ is a set of points within the program that determines the behavior of certain functions used for reflection ([meta.reflection]). During the evaluation of an expression `$C$` as a core constant expression, the evaluation context of an evaluation `$E$` comprises the union of

- [#.#]{.pnum} the instantiation context of `$C$` ([module.context]{.sref}), and
- [#.#]{.pnum} the synthesized points corresponding to any injected declarations produced by evaluations sequenced before `$E$` ([intro.execution]{.sref}).

:::
:::

### [dcl.pre]{.sref} Preamble {-}

Introduce the non-terminal `$vacant-declaration$` in paragraph 9.1 to encompass static assertions, empty declarations, and consteval blocks:

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

Strike the assertion that a `$typedef-name$` is synonymous with its associated type from paragraph 8 (type aliases are entities now).

::: std
[8]{.pnum} If the `$decl-specifier-seq$` contains the `typedef` specifier, the declaration is a _typedef declaration_ and each `$declarator-id$` is declared to be a `$typedef-name$`[, synonymous with its associated type]{.rm} ([dcl.typedef]{.sref}).

:::

Insert the following after paragraph 13 in relation to consteval blocks:

::: std
[13]{.pnum} *Recommended practice*: When a `$static_assert-declaration$` fails, [...]

::: addu
[*]{.pnum} The _evaluating expression_ of a `$consteval-block-declaration$` is an expression whose evaluation has the same associated side effects as the `$postfix-expression$`

```cpp
[] -> void consteval $compound-statement$ ()
```

The evaluating expression shall be a constant expression ([expr.const]).

[The evaluating expression of a `$consteval-block-declaration$` can produce injected declarations as side effects ([expr.const]).]{.note}
:::

[14]{.pnum} An `$empty-declaration$` has no effect.

:::

### [dcl.typedef]{.sref} The `typedef` specifier {-}

Modify paragraphs 1-2 to clarify that the `typedef` specifier now introduces an entity.

::: std
[1]{.pnum} Declarations containing the `$decl-specifier$` `typedef` declare [identifiers that can be used later for naming]{.rm} [type aliases whose underlying entities are]{.addu} fundamental ([basic.fundamental]{.sref}) or compound ([basic.compound]{.sref}) types. The `typedef` specifier shall not be combined in a `$decl-specifier-seq$` with any other kind of specifier except a `$defining-type-specifier$`, and it shall not be used in the `$decl-specifier-seq$` of a `$parameter-declaration$` ([dcl.fct]{.sref}) nor in the `$decl-specifier-seq$` of a `$function-definition$` ([dcl.fct.def]{.sref}). If a `$typedef-specifier$` appears in a declaration without a `$declarator$`, the program is ill-formed.

```
  $typedef-name$:
      $identifier$
      $simple-template-id$
```

A name declared with the `typedef` specifier becomes a `$typedef-name$`. [A `$typedef-name$` names]{.rm} [The underlying entity of the type alias is]{.addu} the type associated with the `$identifier$` ([dcl.decl]{.sref}) or `$simple-template-id$` ([temp.pre]{.sref}); a `$typedef-name$` [is]{.rm} thus [a synonym for]{.rm} [denotes]{.addu} another type. A `$typedef-name$` does not introduce a new type the way a class declaration ([class.name]{.sref}) or enum declaration ([dcl.enum]{.sref}) does.

[2]{.pnum} A [`$typedef-name$`]{.rm} [type alias]{.addu} can also be [introduced]{.rm} [declared]{.addu} by an `$alias-declaration$`. The `$identifier$` following the `using` keyword is not looked up; it becomes [a]{.rm} [the]{.addu} `$typedef-name$` [of a type alias]{.addu} and the optional `$attribute-specifier-seq$` following the `$identifier$` appertains to that [`$typedef-name$`]{.rm} [type alias]{.addu}. Such a [`$typedef-name$`]{.rm} [type alias]{.addu} has the same semantics as if it were introduced by the `typedef` specifier. In particular, it does not define a new type.

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
[3]{.pnum} A `$placeholder-type-specifier$` is a placeholder for a type to be deduced ([dcl.spec.auto]). A `$type-specifier$` [of the form `typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ $template-name$`]{.rm} is a placeholder for a deduced class type ([dcl.type.class.deduct]) [if it either\ ]{.addu}

- [[#.#]{.pnum} is of the form `typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ $template-name$`, or]{.addu}
- [[#.#]{.pnum} is of the form `typename@~_opt_~@ $splice-specifier$` and the `$splice-specifier$` designates a class template or alias template.]{.addu}

The `$nested-name-specifier$`, if any, shall be non-dependent and the `$template-name$` [or `$splice-specifier$`]{.addu} shall [name]{.rm} [designate]{.addu} a deducible template. A _deducible template_ is either a class template or is an alias template whose `$defining-type-id$` is of the form

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

void f() {
  [](auto ...pack) {
    decltype(pack...[0]) @[x5]{.rm} [x6]{.addu}@;    // type is int
    decltype((pack...[0])) @[x6]{.rm} [x7]{.addu}@;  // type is int&
  }
}
```
:::

:::

### 9.2.9.8+ [dcl.type.splice] Type splicing {-}

Add a new subsection of ([dcl.type]{.sref}) following ([dcl.type.class.deduct]{.sref}).

::: std
::: addu
**Type Splicing   [dcl.type.splice]**

```
$splice-type-specifier$:
   typename@~_opt_~@ $splice-specifier$
   typename@~_opt_~@ $splice-specialization-specifier$
```

[#]{.pnum} A `$splice-specifier$` or `$splice-specialization-specifier$` immediately followed by `::` is never interpreted as part of a `$splice-type-specifier$`. A `$splice-specifier$` or `$splice-specialization-specifier$` not preceded by `typename` is only interpreted as a `$splice-type-specifier$` within a type-only context ([temp.res.general]{.sref}).

::: example
```cpp
struct S { using type = int; };
template <auto R> struct TCls {
  typename [:R:]::type member;  // typename applies to the qualified name
};

int fn() {
  [:^^S::type:] *var;           // error: [:^^S::type:] is an expression
  typename [:^^S::type:] *var;  // OK, declares variable with type int*
}

using alias = [:^^S::type:];    // OK, type-only context
```
:::

[#]{.pnum} For a `$splice-type-specifier$` of the form `typename@~_opt_~@ $splice-specifier$`, the `$splice-specifier$` shall designate a type, a primary class template, an alias template, or a type concept. The `$splice-type-specifier$` designates the same entity as the `$splice-specifier$`.

[#]{.pnum} For a `$splice-type-specifier$` of the form `typename@~_opt_~@ $splice-specialization-specifier$`, the `$splice-specifier$` of the `$splice-specialization-specifier$` shall designate a primary class template, an alias template, or a type concept (call it `$T$`). If `$T$` designates a primary class template or an alias template, the `$splice-type-specifier$` designates the specialization of `$T$` corresponding to the `$template-argument-list$` (if any) of the `$splice-specialization-specifier$`. Otherwise, the `$splice-specifier$` shall appear as a `$type-constraint$`.

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
@[`constexpr std::meta::info r = ^^void(int) &;    // OK`]{.addu}@

```
:::
:::

### [dcl.fct.default]{.sref} Default arguments {-}

Modify paragraph 9 to allow reflections of non-static data members to appear in default function arguments, and extend example 8 which follows.

::: std
[9]{.pnum} A default argument is evaluated each time the function is called with no argument for the corresponding parameter.

[...]

A non-static member shall not appear in a default argument unless it appears as the `$id-expression$` of a class member access expression ([expr.ref]) [or `$reflect-expression$` ([expr.reflect])]{.addu} or unless it is used to form a pointer to member ([expr.unary.op]).

::: example8
```cpp
int b;
class X {
  int a;
  int mem1(int i = a);    // error: non-static member `a` used as default argument
  int mem2(int i = b);    // OK; use `X::b`
  @[`consteval void mem3(std::meta::info r = ^^a) {};    // OK`]{.addu}@
  static int b;
}
```

:::
:::

### [dcl.init.general]{.sref} Initializers (General) {-}

Change paragraphs 6-8 of [dcl.init.general]{.sref} [No changes are necessary for value-initialization, which already forwards to zero-initialization for scalar types]{.ednote}:

::: std
[6]{.pnum} To *zero-initialize* an object or reference of type `T` means:

* [6.0]{.pnum} [if `T` is `std::meta::info`, the object is initialized to a null reflection value;]{.addu}
* [6.1]{.pnum} if `T` is [a]{.rm} [any other]{.addu} scalar type ([basic.types.general]{.sref}), the object is initialized to the value obtained by converting the integer literal `0` (zero) to `T`;
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

[2]{.pnum} A program that refers to a deleted function implicitly or explicitly, other than to declare it [or to use as the operand of a `$reflect-expression$` ([expr.reflect])]{.addu}, is ill-formed.
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

[1]{.pnum} [A `$using-enum-declarator$` of the form `$splice-type-specifier$` designates the same construct designated by the `$splice-type-specifier$`. Any other]{.addu} [A]{.rm} `$using-enum-declarator$` names the set of declarations found by type-only lookup ([basic.lookup.general]) for the `$using-enum-declarator$` ([basic.lookup.unqual]{.sref}, [basic.lookup.qual]{.sref}). The `$using-enum-declarator$` shall designate a non-dependent type with a reachable `$enum-specifier$`.
:::

### [namespace.alias]{.sref} Namespace alias {-}

Modify the grammar for `$namespace-alias-definition$` in paragraph 1, and clarify that such declarations declare a "namespace alias" (which is now an entity as per [basic.pre]).

::: std
[1]{.pnum} A `$namespace-alias-definition$` declares [an alternative name for a namespace]{.rm} [a namespace alias]{.addu} according to the following grammar:

```diff
  $namespace-alias$:
      $identifier$

  $namespace-alias-definition$:
      namespace $identifier$ = $qualified-namespace-specifier$
+     namespace $identifier$ = $splice-specifier$

  $qualified-namespace-specifier$:
      $nested-name-specifier$@~_opt_~@ $namespace-name$
```

[The `$splice-specifier$` (if any) shall designate a namespace.]{.addu}
:::

Remove the details about what the `$namespace-alias$` denotes; this will fall out from the "underlying entity" of the namespace alias defined below:

::: std
[2]{.pnum} The `$identifier$` in a `$namespace-alias-definition$` becomes a `$namespace-alias$` [and denotes the namespace denoted by the `$qualified-namespace-specifier$`]{.rm}.

:::

Add the following paragraph after paragraph 2 and before the note:

::: std
::: addu
[2+]{.pnum} The underlying entity ([basic.pre]{.sref}) of the namespace alias is the namespace either denoted by the `$qualified-namespace-specifier$` or designated by the `$splice-specifier$`.

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

Add the following prior to the first paragraph of [namespace.udir]{.sref}, and renumber accordingly:

::: std
::: addu
[0]{.pnum} The `$splice-specifier$`, if any, designates a namespace. The `$nested-name-specifier$`, `$namespace-name$`, and `$splice-specifier$` shall not be dependent.
:::

[1]{.pnum} A `$using-directive$` shall not appear in class scope, but may appear in namespace scope or in block scope.

[...]
:::

Prefer the verb "designate" rather than "nominate" in the notes that follow:

::: std
[A `$using-directive$` makes the names in the [nominated]{.rm} [designated]{.addu} namespace usable in the scope [...]. During unqualified name lookup, the names appear as if they were declared in the nearest enclosing namespace which contains both the `$using-directive$` and the [nomindated]{.rm} [designated]{.addu} namespace.]{.note2}

[...]

[A `$using-directive$` is transitive: if a scope contains a `$using-directive$` that [nominates]{.rm} [designates]{.addu} a namespace that itself contains `$using-directives$`, the namespaces [nominated]{.rm} [designated]{.addu} by those `$using-directives$` are also eligible to be considered.]{.note4}
:::


### [dcl.attr.grammar]{.sref} Attribute syntax and semantics {-}

Add a production to the grammar for `$attribute-specifier$` as follows:

::: std
```diff
  $attribute-specifier$:
     [ [ $attribute-using-prefix$@~_opt_~@ $attribute-list$ ] ]
+    [ [ using $attribute-namespace$ :] ]
     $alignment-specifier$
```
:::

and update the grammar for balanced token as follows:

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

[4]{.pnum} [...] An `$attribute-specifier$` that contains no `$attribute$`s [and no `$alignment-specifier$`]{.addu} has no effect. [[That includes an `$attribute-specifier$` of the form `[ [ using $attribute-namespace$ :] ]` which is thus equivalent to replacing the `:]` token by the two-token sequence `:` `]`.]{.note}]{.addu} ...
:::

### [dcl.attr.deprecated]{.sref} Deprecated attribute {-}

Prefer "type alias" to "`$typedef-name$`" in paragraph 2.

::: std
[2]{.pnum} The attribute may be applied to the declaration of a class, a [`$typedef-name$`]{.rm} [type alias]{.addu}, a variable, a non-static data member, a function, a namespace, an enumeration, an enumerator, a concept, or a template specialization.

:::

### [dcl.attr.unused]{.sref} Maybe unused attribute {-}

Prefer "type alias" to "`$typedef-name$`" in paragraph 2.

::: std
[2]{.pnum} The attribute may be applied to the declaration of a class, [`$typedef-name$`]{.rm} [type alias]{.addu}, variable (including a structured binding declaration), structured binding, non-static data member, function, enumeration, or enumerator, or to an `$identifier$` label ([stmt.label]{.sref}).
:::

### [module.global.frag]{.sref} Global module fragment {-}

Specify in paragraph 3 that it is unspecified whether spliced types are replaced by their designated types, and renumber accordingly. Add an additional bullet further clarifying that it is unspecified whether any splice specifier is replaced.

::: std
In this determination, it is unspecified

- [3.6]{.pnum} whether a reference to an `$alias-declaration$`, `typedef` declaration, `$using-declaration$`, or `$namespace-alias-definition$` is replaced by the declarations they name prior to this determination,
- [#.#]{.pnum} whether a `$simple-template-id$` that does not denote a dependent type and whose `$template-name$` names an alias template is replaced by its denoted type prior to this determination,
- [#.#]{.pnum} whether a `$decltype-specifier$` [or `$splice-type-specifier$`]{.addu} that does not [denote]{.rm} [designate]{.addu} a dependent type is replaced by its [denoted]{.rm} [designated]{.addu} type prior to this determination, [and]{.rm}
- [#.#]{.pnum} whether a non-value-dependent constant expression is replaced by the result of constant evaluation prior to this determination[.]{.rm}[, and]{.addu}
- [[#.#]{.pnum} whether a `$splice-specifier$` or `$splice-expression$` that is not dependent is replaced by the construct that it designates prior to this determination.]{.addu}

:::

### [module.reach]{.sref} Reachability {-}

Modify the definition of reachability to account for injected declarations:

::: std
[3]{.pnum} A declaration `$D$` is _reachable from_ a point `$P$` if

* [#.#]{.pnum} [`$P$` is not a synthesized point and]{.addu} `$D$` appears prior to `$P$` in the same translation unit, [or]{.rm}
* [#.#]{.pnum} [`$D$` is an injected declaration for which `$P$` is the corresponding synthesized point, or]{.addu}
* [#.#]{.pnum} `$D$` is not discarded ([module.global.frag]{.sref}), appears in a translation unit that is reachable from `$P$`, and does not appear within a _private-module-fragment_.
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
-   $static_assert-declaration$
+   $vacant-declaration$
    $template-declaration$
    $explicit-specialization$
    $deduction-guide$
    $alias-declaration$
    $opaque-enum-declaration$
-   $empty-declaration$
```
:::

Update paragraph 3 accordingly:

::: std
[3]{.pnum} A `$member-declaration$` does not declare new members of the class if it is

* [#.#]{.pnum} a friend declaration ([class.friend]),
* [#.#]{.pnum} a `$deduction-guide$` ([temp.deduct.guide]),
* [#.#]{.pnum} a `$template-declaration$` whose declaration is one of the above,
* [#.#]{.pnum} a [`$static_assert-declaration$`,]{.rm}
* [#.#]{.pnum} a `$using-declaration$` ([namespace.udecl]) , or
* [#.#]{.pnum} [an `$empty-declaration$`.]{.rm} [a `$vacant-declaration$`.]{.addu}

:::

Extend paragraph 5, and modify note 3, to clarify the existence of subobjects corresponding to non-static data members of reference types.

::: std
[5]{.pnum} A data member or member function may be declared `static` in its _member-declaration_, in which case it is a _static member_ (see [class.static]{.sref}) (a _static data member_ ([class.static.data]{.sref}) or _static member function_ ([class.static.mfct]{.sref}), respectively) of the class. Any other data member or member function is a _non-static member_ (a _non-static data member_ or _non-static member function_ ([class.mfct.non.static]{.sref}), respectively). [For each non-static data member of reference type, there is a unique member subobject whose size and alignment is the same as if the data member were declared with the corresponding pointer type.]{.addu}

[[A non-static data member of non-reference type is a member subobject of a class object.]{.rm} [An object of class type has a member subobject corresponding to each non-static data member of its class.]{.addu}]{.note3}

:::

Add a new paragraph to the end of the section defining _data member description_:

::: std
::: addu
[29+]{.pnum} A _data member description_ is a quintuple (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) describing the potential declaration of a nonstatic data member where

- [29+.#]{.pnum} `$T$` is a type or type alias,
- [29+.#]{.pnum} `$N$` is an `$identifier$` or `-1`,
- [29+.#]{.pnum} `$A$` is an alignment or `-1`,
- [29+.#]{.pnum} `$W$` is a bit-field width or `-1`, and
- [29+.#]{.pnum} `$NUA$` is a boolean value.

Two data member descriptions are equal if each of their respective components are same types, same identifiers, and equal values.

::: note
The components of a data member description describe a data member such that

- [29+.#]{.pnum} its type is specified using the type or type alias given by `$T$`,
- [29+.#]{.pnum} it is declared with the name given by `$N$` if `$N$ != -1` and is otherwise unnamed,
- [29+.#]{.pnum} it is declared with the `$alignment-specifier$` ([dcl.align]{.sref}) given by `alignas($A$)` if `$A$ != -1` and is otherwise declared without an `$alignment-specifier$`,
- [29+.#]{.pnum} it is a bit-field ([class.bit]{.sref}) with the width given by `$W$` if `$W$ != -1` and is otherwise not a bit-field,
- [29+.#]{.pnum} it is declared with the attribute `[[no_unique_address]]` ([dcl.attr.nouniqueaddr]{.sref}) if `$NUA$` is `true` and is otherwise declared without that attribute.

Data member descriptions are represented by reflections ([basic.fundamental]{.sref}) returned by `std::meta::data_member_spec` ([meta.reflection.define.aggregate]) and can be reified as data members of a class using `std::meta::define_aggregate` ([meta.reflection.define.aggregate]).
:::

:::
:::

### [class.union.anon]{.sref} Anonymous unions {-}

Replace `$static_assert-declaration$` with `$vacant-declaration$` in paragraph 1. [This refactor allows putting in an `$empty-declaration$` into an anonymous union, which is kind of a consistency drive by with other classes.]{.ednote}

::: std
[1]{.pnum} [...] Each `$member-declaration$` in the `$member-specification$` of an anonymous union shall either define one or more public non-static data members or be a [`$static_assert-declaration$`]{.rm} [`$vacant-declaration$`]{.addu}.  [...]

:::

### [class.derived.general]{.sref} General {-}

Introduce the term "direct base class relationship" to paragraph 2.

::: std
[2]{.pnum} The component names of a `$class-or-decltype$` are those of its `$nested-name-specifier$`, `$type-name$`, and/or `$simple-template-id$`. A `$class-or-decltype$` shall denote a (possily cv-qualified) class type that is not an incompletely defined class ([class.mem]{.sref}); any cv-qualifiers are ignored. The class denoted by the `$class-or-decltype$` of a `$base-specifier$` is called a _direct base class_ for the class being defined[; each such `$base-specifier$` introduces a _direct base class relationship_ between the class being defined and the direct base class]{.addu}. The lookup for the component name of the `$type-name$` or `$simple-template-id$` is type-only ([basic.lookup]{.sref}). [...]
:::

### [class.access.general]{.sref} General {-}

Prefer "type alias" rather than `$typedef-name$` in the note that follows paragraph 4.

::: std
[Because access control applies to the declarations named, if access control is applied to a [`$typedef-name$`]{.rm} [type alias]{.addu}, only the accessibility of the typedef or alias declaration itself is considered. The accessibility of the [entity referred to by the `$typedef-name$`]{.rm} [underlying entity]{.addu} is not considered.]{.note3}
:::

### [over.pre]{.sref} Preamble {-}

Move the definition "overload set" from [basic.lookup]{.sref} to paragraph 2, rewrite the preamble to better describe overload resolution, and add a note explaining the expressions that form overload sets.

::: std
[2]{.pnum} [An _overload set_ is a set of declarations that each denote a function or function template. Using these declarations as a starting point, the process of _overload resolution_ attempts to determine]{.addu} [When a function is named in a call,]{.rm} which function [declaration]{.rm} is being referenced [and the validity of the call are determined]{.rm} by comparing the types of the arguments at [the]{.rm} [a]{.addu} point of use with the types of the parameters in [candidate functions]{.addu} [in the declarations in the overload set]{.rm}. [This function selection process is called _overload resolution_ and]{.rm} [Overload resolution]{.addu} is defined in [over.match].

  [[Overload sets are formed by `$id-expression$`s naming functions and function templates and by `$splice-expression$`s designating entities of the same kinds.]{.note}]{.addu}

:::

### [over.match.general]{.sref} General {-}

Modify paragraphs 3 and 4 to clarify that access rules do not apply in all contexts.

::: std
[3]{.pnum} If a best viable function exists and is unique, overload resolution succeeds and produces it as the result. Otherwise overload resolution fails and the invocation is ill-formed. When overload resolution succeeds, and the best viable function is [not]{.rm} [neither designated in a manner exempt from access rules ([expr.prim.splice]) nor]{.addu} accessible in the context in which it is used, the program is ill-formed.

[4]{.pnum} Overload resolution results in a _usable candidate_ if overload resolution succeeds and the selected candidate is either not a function ([over.built]{.sref}), or is a function that is not deleted and is [either designated in a manner exempt from access rules or is]{.addu} accessible from the context in which overload resolution was performed.

:::

### [over.call.func]{.sref} Call to named function {-}

Modify paragraph 1 to clarify that this section will also apply to splices of function templates.

::: std
[1]{.pnum} Of interest in [over.call.func] are only those function calls in which the `$posfix-expression$` ultimately contains an `$id-expression$` [or `$splice-expression$`]{.addu} that denotes one or more functions [or function templates]{.addu}. Such a `$postfix-expression$`, perhaps nested arbitrarily deep in parentheses, has one of the following forms:

```diff
  $postfix-expression$:
     $postfix-expression$ . $id-expression$
+    $postfix-expression$ . $splice-expression$
     $postfix-expression$ -> $id-expression$
+    $postfix-expression$ -> $splice-expression$
     $primary-expression$
```

These represent two syntactic subcategories of function calls: qualified function calls and unqualified function calls.

:::

Modify paragraph 2 to account for overload resolution of `$splice-expression$`s. Massage the wording to better account for member function templates.

::: std
[2]{.pnum} In qualified function calls, the function is [named]{.rm} [designated]{.addu} by an `$id-expression$` [or `$splice-expression$`]{.addu} preceded by an `->` or `.` operator. Since the construct `A->B` is generally equivalent to `(*A).B`, the rest of [over] assumes, without loss of generality, that all member function calls have been normalized to the form that uses an object and the `.` operator. Furthermore, [over] assumes that the `$postfix-expression$` that is the left operand of the `.` operator has type "_cv_ `T`" where `T` denotes a class.^102^ The function [and function template]{.addu} declarations [either]{.addu} found by name lookup [if the dot is followed by an `$id-expression$`, or as specified by [expr.prim.splice] if the dot is followed by a `$splice-expression$`, undergo the adjustments described in [over.match.funcs.general] and thereafter]{.addu} constitute the set of candidate functions. The argument list is the `$expression-list$` in the call augmented by the addition of the left operand of the `.` operator in the normalized member function call as the implied object argument ([over.match.funcs]{.sref}).

:::

Modify paragraph 3 to account for overload resolution of `$splice-expression$`s. Massage the wording to better account for member function templates.

::: std
[3]{.pnum} In unqualified function calls, the function is named by a `$primary-expression$`. The function [and function template]{.addu} declarations [either]{.addu} found by name lookup[, or as specified by [expr.prim.splice] if the `$primary-expression$` is a (possibly parenthesized) `$splice-expression$`, undergo the adjustments described in [over.match.funcs.general] and thereafter]{.addu} constitute the set of candidate functions. Because of the rules for name lookup, the set of candidate functions consists either entirely of non-member functions or entirely of member functions of some class `T`. In the former case or if the `$primary-expression$` is [a `$splice-expression$` or]{.addu} the address of an overload set, the argument list is the same as the `$expression-list$` in the call. Otherwise, the argument list is the `$expression-list$` in the call augmented by the addition of an implied function argument as in a qualified function call. If the current class is, or is derived from, `T`, and the keyword `this` ([expr.prim.this]{.sref}) refers to it, then the implied object argument is `(*this)`. Otherwise, a contrived object of type `T` becomes the implied object argument;^103^ if overload resolution selects a non-static member function, the call is ill-formed.

:::

### [over.match.class.deduct]{.sref} Class template argument deduction {-}

Extend paragraph 1 to work with `$splice-type-specifier$`s.

::: std
[1]{.pnum} When resolving a placeholder for a deduced class type ([dcl.type.class.deduct]{.sref}) where the `$template-name$` [or `$splice-type-specifier$`]{.addu} [names]{.rm} [designates]{.addu} a primary class template `C`, a set of functions and function templates, called the guides of `C`, is formed comprising:

* [#.#]{.pnum} ...

:::

Extend paragraph 3 to also cover `$splice-type-specifier$`s.

:::std
[3]{.pnum} When resolving a placeholder for a deduced class type ([dcl.type.simple]{.sref}) where the `$template-name$` [or `$splice-type-specifier$`]{.addu} [names]{.rm} [designates]{.addu} an alias template `A`, the `$defining-type-id$` of `A` must be of the form

```cpp
typename@~_opt_~@ $nested-name-specifier$@~_opt_~@ template@~_opt_~@ $simple-template-id$
```

as specified in [dcl.type.simple]{.sref}. The guides of `A` are the set of functions or function templates formed as follows. ...

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

Extend `$type-parameter$` to permit `$splice-specifier$`s as default template arguments for template template parameters. Also extend the grammar for `$type-constraint$` to include `$splice-type-specifier$`.

::: std
```diff
  $type-parameter$:
      $type-parameter-key$ ...@~_opt_~@ $identifier$@~_opt_~@
      $type-parameter-key$ $identifier$@~_opt_~@ = $type-id$
      $type-constraint$ ...@~_opt_~@ $identifier$@~_opt_~@
      $type-constraint$ $identifier$@~_opt_~@ = $type-id$
      $template-head$ $type-parameter-key$ ...@~_opt_~@ $identifier$@~_opt_~@
      $template-head$ $type-parameter-key$ $identifier$@~_opt_~@ = $id-expression$
+     $template-head$ $type-parameter-key$ $identifier$@~_opt_~@ = $splice-template-argument$

  $type-constraint$:
      $nested-name-specifier$@~_opt_~@ $concept-name$
      $nested-name-specifier$@~_opt_~@ $concept-name$ < $template-argument-list$@~_opt_~@>
+     $splice-type-specifier$
```
:::

Add a paragraph after paragraph 3 to restrict which `$splice-type-specifier$`s form `$type-constraint$`s.

::: std
::: addu
[3+]{.pnum} A `$type-constraint$` of the form `$splice-type-specifier$` shall not appear in a `$type-parameter$`. The `$splice-type-specifier$` (if any) shall contain a `$splice-specifier$` that either designates a type concept or is dependent.
:::
:::

### [temp.names]{.sref} Names of template specializations {-}

Define the term `$splice-template-argument$`, and add it as a production for `$template-argument$`.

::: std
```diff
  $template-argument$:
      $constant-expression$
      $type-id$
      $id-expression$
      $braced-init-list$
+     $splice-template-argument$

+ $splice-template-argument$:
+     $splice-specifier$
```
:::

Extend and re-format paragraph 3 of [temp.names]{.sref}:

::: std

[3]{.pnum} A `<` is interpreted as the delimiter of a `$template-argument-list$` if it follows

* [[#.#]{.pnum} a `$splice-specifier$` that either appears in a type-only context or is preceded by `template` or `typename`, or]{.addu}
* [#.#]{.pnum} a name that is not a `$conversion-function-id$` and
  * [#.#.#]{.pnum} that follows the keyword template or a ~ after a nested-name-specifier or in a class member access expression, or
  * [#.#.#]{.pnum}  for which name lookup finds the injected-class-name of a class template or finds any declaration of a template, or
  * [#.#.#]{.pnum} that is an unqualified name for which name lookup either finds one or more functions or finds nothing, or
  * [#.#.#]{.pnum} that is a terminal name in a using-declarator ([namespace.udecl]), in a declarator-id ([dcl.meaning]), or in a type-only context other than a nested-name-specifier ([temp.res]).

[If the name is an identifier, it is then interpreted as a *template-name*. The keyword template is used to indicate that a dependent qualified name ([temp.dep.type]{.sref}) denotes a template where an expression might appear.]{.note}

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

+ static constexpr auto r = ^^T::adjust;
+ T* p3 = [:r:]<200>();                 // error: < means less than
+ T* p4 = template [:r:]<200>();        // OK, < starts template argument list
}
```
:::
:::

Clarify that the `>` disambiguation in paragraph 4 also applies to the parsing of `$splice-specialization-specifier$`s:

::: std
[4]{.pnum} When parsing a `$template-argument-list$`, the first non-nested `>`^111^ is taken as the ending delimiter rather than a greater-than operator. Similarly, the first non-nested `>>` is treated as two consecutive but distinct `>` tokens, the first of which is taken as the end of the `$template-argument-list$` and completes the `$template-id$` [or `$splice-specialization-specifier$`]{.addu}.

[The second `>` token produced by this replacement rule can terminate an enclosing `$template-id$` [or `$splice-specialization-specifier$`]{.addu} construct or it can be part of a different construct (e.g., a cast).]{.note}

:::

Extend the definition of a _valid_ `$template-id$` to also cover `$splice-specialization-specifier$`s:

::: std
[7]{.pnum} A `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is _valid_ if

* [#.#]{.pnum} there are at most as many arguments as there are parameters or a parameter is a template parameter pack ([temp.variadic]{.sref}),
* [#.#]{.pnum} there is an argument for each non-deducible non-pack parameter that does not have a default `$template-argument$`,
* [#.#]{.pnum} each `$template-argument$` matches the corresponding `$template-parameter$` ([temp.arg]{.sref}),
* [#.#]{.pnum} substitution of each template argument into the following template parameters (if any) succeeds, and
* [#.#]{.pnum} if the `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is non-dependent, the associated constraints are satisfied as specified in the next paragraph.

A `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} shall be valid unless it names a function template specialization ([temp.deduct]{.sref}).

:::

Extend paragraph 8 to require constraints to also be satisfied by `$splice-specialization-specifier$`s:

::: std
[8]{.pnum} When the `$template-name$` of a `$simple-template-id$` [or the `$splice-specifier$` of a `$splice-specialization-specifier$` designates]{.addu} [names]{.rm} a constrained non-function template or a constrained template `$template-parameter$`, and all `$template-arguments$` in the `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} are non-dependent ([temp.dep.temp]{.sref}), the associated constraints ([temp.constr.decl]{.sref}) of the constrained template shall be satisfied ([temp.constr.constr]{.sref}).

:::

Modify footnote 111 to account for `$splice-specialization-specifier$`s:

::: std
[111)]{.pnum} A `>` that encloses the `$type-id$` of a `dynamic_cast`, `static_cast`, `reinterpret_cast` or `const_cast`, or which encloses the `$template-argument$`s of a subsequent `$template-id$` [or `$splice-specialization-specifier$`]{.addu}, is considered nested for the purpose of this description.

:::


### [temp.arg.general]{.sref} General {-}

Modify paragraph 1; there are now _four_ forms of `$template-argument$`.

::: std
[1]{.pnum} There are [three]{.rm} [four]{.addu} forms of `$template-argument$`, [three of which]{.addu} correspond[ing]{.rm} to the three forms of `$template-parameter$`: type, non-type and template. [The fourth argument form, _splice template argument_, is considered to match the form of any template parameter.]{.addu} The type and form of each `$template-argument$` specified in a `$template-id$` [or in a `$splice-specialization-specifier$`]{.addu} shall match the type and form specified for the corresponding parameter declared by the template in its `$template-parameter-list$`.

:::

Clarify ambiguity between `$splice-expression$`s and `$splice-template-argument$`s in paragraph 3:

::: std

[3]{.pnum} [A `$template-argument$` of the form `$splice-specifier$` is interpreted as a `$splice-template-argument$`.]{.addu} [In a]{.rm} [For any other]{.addu} `$template-argument$`, an ambiguity between a `$type-id$` and an expression is resolved to a `$type-id$`, regardless of the form of the corresponding `$template-parameter$`.

::: example2
```diff
  template<class T> void f(); @[\ \ // #1]{.addu}@
  template<int I> void f(); @[\ \ \  // #2]{.addu}@

  void g() {
    f<int()>();       // int() is a type-id: call@[s (#1)]{.addu}@ @[`the first f()`]{.rm}@

+   constexpr int x = 42;
+   f<[:^^int:]>();    // splice-template-argument: calls (#1)
+   f<[:^^x:]>();      // splice-template-argument: calls (#2)
  }
```
:::

:::

Clarify in paragraph 9 that default template arguments also apply to `$splice-specialization-specifier$`s:

::: std
[9]{.pnum} When a `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} does not [name]{.rm} [designate]{.addu} a function, a default `$template-argument$` is implicitly instantiated when the value of that default argument is needed.

:::

### [temp.arg.type]{.sref} Template type arguments {-}

Extend [temp.arg.type]{.sref}/1 to cover splice template arguments:

::: std
[1]{.pnum} A `$template-argument$` for a `$template-parameter$` which is a type shall [either]{.addu} be a `$type-id$` [or a `$splice-template-argument$` whose `$splice-specifier$` designates a type]{.addu}.
:::

### [temp.arg.nontype]{.sref} Template non-type arguments {-}

[We don't think we have to change anything here, since if `E` is a `$splice-specifier$` that can be interpreted as a `$splice-expression$`, the requirements already fall out based on how paragraphs 1 and 3 are already worded]{.draftnote}

::: std
[1]{.pnum} If the type `T` of a *template-parameter* ([temp.param]) contains a placeholder type ([dcl.spec.auto]) or a placeholder for a deduced class type ([dcl.type.class.deduct]), the type of the parameter is the type deduced for the variable x in the invented
declaration
```cpp
T x = $E$ ;
```
where `$E$` is the template argument provided for the parameter.

[2]{.pnum} The value of a non-type *template-parameter* `P` of (possibly deduced) type `T` [...]

[3]{.pnum} Otherwise, a temporary variable
```cpp
constexpr T v = $A$;
```
is introduced.
:::

### [temp.arg.template]{.sref} Template template arguments {-}

Extend [temp.arg.template]{.sref}/1 to cover splice template arguments:

::: std
[1]{.pnum} A `$template-argument$` for a template `$template-parameter$` shall be [the name of]{.rm} a class template or an alias template, expressed as [an]{.addu} `$id-expression$` [or a `$splice-template-argument$`]{.addu}. Only primary templates are considered when matching the template argument with the corresponding parameter; partial specializations are not considered even if their parameter lists match that of the template template parameter.
:::

### [temp.constr.normal]{.sref} Constraint normalization {-}

Include a reference to `$splice-expression$`s in Note 1.

::: std
[Normalization of `$constraint-expression$`s is performed when determining the associated constraints ([temp.constr.constr]) of a declaration and when evaluating the value of an `$id-expression$` [or `$splice-expression$`]{.addu} that names a concept specialization ([expr.prim.id] [, [expr.prim.splice]]{.addu})]{.note1}

:::

### [temp.type]{.sref} Type equivalence {-}

Extend paragraph 1 to also define the "sameness" of `$splice-specialization-specifier$`s:

::: std
[1]{.pnum} Two `$template-id$`s [or `$splice-specialization-specifier$`s]{.addu} are the same if

* [#.#]{.pnum} their `$template-name$`s, `$operator-function-id$`s, [or]{.rm} `$literal-operator-id$`s[, or `$splice-specifier$`s]{.addu} refer to the same template, and
* [#.#]{.pnum} their corresponding type `$template-argument$`s are the same type, and
* [#.#]{.pnum} the template parameter values determined by their corresponding non-type template arguments ([temp.arg.nontype]{.sref}) are template-argument-equivalent (see below), and
* [#.#]{.pnum} their corresponding template `$template-argument$`s refer to the same template.

Two `$template-id$`s [or `$splice-specialization-specifier$`s]{.addu} that are the same refer to the same class, function, or variable.

:::

Extend *template-argument-equivalent* in paragraph 2 to handle `std::meta::info`:

::: std
[2]{.pnum} Two values are *template-argument-equivalent* if they are of the same type and

* [2.1]{.pnum} they are of integral type and their values are the same, or
* [2.2]{.pnum} they are of floating-point type and their values are identical, or
* [2.3]{.pnum} they are of type `std::nullptr_t`, or
* [2.*]{.pnum} [they are of type `std::meta::info` and their values are the same, or]{.addu}
* [2.4]{.pnum} they are of enumeration type and their values are the same, or
* [2.5]{.pnum} [...]
:::

### [temp.deduct.guide]{.sref} Deduction guides {-}

Extend paragraph 1 to clarify that `$splice-type-specifier$`s can also leverage deduction guides.

::: std
[1]{.pnum} Deduction guides are used when a `$template-name$` [or `$splice-type-specifier$`]{.addu} appears as a type specifier for a deduced class type ([dcl.type.class.deduct]{.sref}). Deduction guides are not found by name lookup. Instead, when performing class template argument deduction ([over.match.class.deduct]{.sref}), all reachable deduction guides declared for the class template are considered.

:::

### [temp.mem]{.sref} Member templates {-}

Clarify in Note 1 that a specialization of a conversion function template can be formed through a `$splice-expression$`.

::: std
::: note
A specialization of a conversion function template is referenced in the same way as a non-template conversion function that converts to the same type ([class.conv.fct]{.sref}).

...

[An expression designating a particular specialization of a conversion function template can only be formed with a `$splice-expression$`.]{.addu} There is no [analogous]{.addu} syntax to form a `$template-id$` ([temp.names]{.sref}) [for such a function]{.addu} by providing an explicit template argument list ([temp.arg.explicit]{.sref}).

:::
:::

### [temp.alias]{.sref} Alias templates {-}

Extend paragraph 2 to enable reflection of alias template specializations.

::: std
[2]{.pnum} [When a]{.rm} [A]{.addu} `$template-id$` [that]{.addu} refers to the specialization of an alias template[, it is equivalent to]{.rm} [is a `$typedef-name$` for a type alias whose underlying entity is]{.addu} the associated type obtained by substitution of its `$template-arguments$` for the `$template-parameters$` in the `$defining-type-id$` of the alias template.

:::

### [temp.res.general]{.sref} General {-}

Extend paragraph 4 to define what it means for a `$splice-specifier$` to appear in a type-only context. Also `$using-enum-declarator$`s to the list of type-only contexts, as it allows the `typename` to be elided from a `$splice-type-specifier$` in non-dependent contexts.

::: std
[4]{.pnum} A qualified or unqualified name is said to be in a `$type-only context$` if it is the terminal name of

* [#.#]{.pnum} a `$typename-specifier$`, `$type-requirement$`, `$nested-name-specifier$`, `$elaborated-type-specifier$`, `$class-or-decltype$`, [`$using-enum-declarator$`]{.addu} or
* [#.#]{.pnum} [...]
  * [4.4.6]{.pnum} `$parameter-declaration$` of a (non-type) `$template-parameter$`.

[A `$splice-specifier$` or `$splice-specialization-specifier$` ([basic.splice]) is said to be in a _type-only context_ if a hypothetical qualified name appearing in the same position would be in a type-only context.]{.addu}

::: example5
```cpp
template<class T> T::R f();
template<class T> void f(T::R);   // ill-formed, no diagnostic required: attempt to
                                  // declare a `void` variable template
@[`enum class Enum { A, B, C };`]{.addu}@

template<class T> struct S {
  using Ptr = PtrTraits<T>::Ptr;  // OK, in a $defining-type-id$
  @[`using Alias = [:^^int];         // OK, in a $defining-type-id$`]{.addu}@
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

Apply a drive-by fix to paragraph 8 to account for placeholders for deduced class types whose template is dependent, while extending the definition to apply to `$splice-specifier$`s.

::: std
[8]{.pnum} A placeholder for a deduced class type ([dcl.type.class.deduct]{.sref}) is dependent if

* [#.#]{.pnum} it has a dependent initializer, [or]{.rm}
* [[#.#]{.pnum} it has a dependent `$template-name$` or a dependent `$splice-specifier$`, or]{.addu}
* [#.#]{.pnum} it refers to an alias template that is a member of the current instantiation and whose `$defining-type-id$` is dependent after class template argument deduction ([over.match.class.deduct]{.sref}) and substitution ([temp.alias]{.sref}).

:::

Account for dependent `$splice-type-specifier$`s in paragraph 10:

::: std
[10]{.pnum} A type is dependent if it is

* [#.#]{.pnum} a template parameter,
* [#.#]{.pnum} ...
* [#.11]{.pnum} denoted by a `$simple-template-id$` in which either the template name is a template parameter or [any of the template arguments is a dependent type or an expression that is type-dependent or value-dependent or is a pack expansion]{.rm} [any of its arguments are dependent]{.addu},^119^
* [#.#]{.pnum} a `$pack-index-specifier$`, [or]{.rm}
* [#.#]{.pnum} denoted by `decltype($expression$)`, where `$expression$` is type-dependent[.]{.rm}[, or]{.addu}
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

[9]{.pnum} A `$primary-expression$` of the form `$splice-specifier$` or `template $splice-specialization-specifier$` is type-dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent ([temp.dep.splice]).
:::
:::

### [temp.dep.constexpr]{.sref} Value-dependent expressions {-}

Add at the end of [temp.dep.constexpr]{.sref}/2 (before the note):

::: std
[2]{.pnum} An *id-expression* is value-dependent if:

* [2.1]{.pnum} [...]

Expressions of the following form are value-dependent if the *unary-expression* or *expression* is type-dependent or the *type-id* is dependent:
```
sizeof unary-expression
sizeof ( type-id )
typeid ( expression )
typeid ( type-id )
alignof ( type-id )
noexcept ( expression )
```

[A `$reflect-expression$` is value-dependent if it contains a dependent `$nested-name-specifier$`, `$type-id$`, `$namespace-name$`, or `$template-name$`, or if it contains a value-dependent or type-dependent `$id-expression$`.]{.addu}
:::


Add a new paragraph after [temp.dep.constexpr]{.sref}/5:

::: std
::: addu

[6]{.pnum} A `$primary-expression$` of the form `$splice-specifier$` or `template $splice-specialization-specifier$` is value-dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent ([temp.dep.splice]).

:::
:::

### 13.8.3.4+ [temp.dep.splice] Dependent splice specifiers {-}


Add a new subsection of [temp.dep]{.sref} following [temp.dep.constexpr]{.sref}, and renumber accordingly.

::: std
::: addu
**Dependent splice specifiers   [temp.dep.splice]**

[1]{.pnum} A `$splice-specifier$` is dependent if its converted `$constant-expression$` is value-dependent. A `$splice-specialization-specifier$` is dependent if its `$splice-specifier$` is dependent or if any of its arguments are dependent. A `$splice-scope-specifier$` is dependent if its `$splice-specifier$` or `$splice-specialization-specifier$` is dependent.

[2]{.pnum}

:::example
```cpp
template <auto T, auto NS, auto C, auto V>
void fn() {
  using a = [:T:]<0>;  // [:T:] and [:T:]<V> are dependent

  static_assert(template [:C:]<typename [:NS:]::template TCls<0>>);
    // [:C:] and [:NS:] are dependent
}

namespace NS {
template <auto V> struct TCls {};
template <typename> concept Concept = requires { requires true; };
}

int main() {
  static constexpr int v = 1;
  fn<^^NS::TCls, ^^NS, ^^NS::Concept, ^^v>();
}
```

:::
:::
:::

### [temp.dep.temp]{.sref} Dependent template arguments {-}

Add a new paragraph to cover dependent splice template arguments.

::: std
[4]{.pnum} A template `$template-parameter$` is dependent if it names a `$template-parameter$` or if its terminal name is dependent.

[[5]{.pnum} A splice template argument is dependent if its `$splice-specifier$` is dependent.]{.addu}

:::

### 13.8.3.6 [temp.dep.alias] Dependent aliases {-}

Add a new section to cover dependent type aliases and namespace aliases.

::: std
::: addu
[1]{.pnum} A type alias is dependent if its underlying entity is a dependent type.

[2]{.pnum} A namespace alias is dependent if it is introduced by a `$namespace-alias-definition$` containing a dependent `$nested-name-specifier$` or a dependent `$splice-specifier$`. A `$namespace-name$` is dependent if it names a dependent namespace alias.

::: example
```cpp
template <std::meta::info R> int fn() {
  namespace Alias = [:R:];  // [:R:] is dependent
  return Alias::v;  // Alias is dependent
}

namespace NS {
  int v = 1;
}

int a = fn<^^NS>();
```
:::

:::
:::

### [temp.expl.spec]{.sref} Explicit specialization {-}

Modify paragraph 9 to allow `$splice-specialization-specifier$`s to be used like incompletely-defined classes.

::: std
[9]{.pnum} A `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu} that [names]{.rm} [designates]{.addu} a class template explicit specialization that has been declared but not defined can be used exactly like the names of other incompletely-defined classes ([basic.types]{.sref}).

:::

### [temp.deduct.general]{.sref} General {-}

Cover `$splice-specialization-specifier$`s in paragraph 2:

::: std
[2]{.pnum} When an explicit template argument list is specified, if the given `$template-id$` [or `$splice-specialization-specifier$`]{.addu} is not valid ([temp.names]{.sref}), type deduction fails. Otherwise, the specified template argument values are substituted for the corresponding template parameters as specified below.

:::

### [temp.deduct.call]{.sref} Deducing template arguments from a function call {-}

Modify paragraph 4.3 to treat parameter types of function templates that are specified using `$splice-specialization-specifier$`s the same as parameter types that are specified using `$simple-template-id$`s.

::: std
- [4.3]{.pnum} If `P` is a class and `P` has the form `$simple-template-id$` [or `typename@~_opt_~@ $splice-specialization-specifier$`]{.addu}, then the transformed `A` can be a derived class `D` of the deduced `A`. Likewise, if `P` is a pointer to a class of the form `$simple-template-id$` [or `typename@~_opt_~@ $splice-specialization-specifier$`]{.addu}, the transformed `A` can be a pointer to a derived class `D` pointed to by the deduced `A`. However, if there is a class `C` that is a (direct or indirect) base class of `D` and derived (directly or indirectly) from a class `B` and that would be a valid deduced `A`, the deduced `A` cannot be `B1` or pointer to `B`, respectively.

:::

### [temp.deduct.type]{.sref} Deducing template arguments from a type {-}

Modify paragraph 20 to clarify that the construct enclosing a template argument might also be a `$splice-specialization-specifier$`.

::: std
[20]{.pnum} If `P` has a form that contains `<i>`, and if the type of `i` differs from the type of the corresponding template parameter of the template named by the enclosing `$simple-template-id$` [or `$splice-specialization-specifier$`]{.addu}, deduction fails. If `P` has a form that contains `[i]`, and if the type of `i` is not an integral type, deduction fails.^123^ If `P` has a form that includes `noexcept(i)` and the type of `i` is not `bool`, deduction fails.

:::

### [cpp.cond]{.sref} Conditional inclusion {-}

Extend paragraph 9 to clarify that `$splice-specifier$`s may not appear in preprocessor directives, while also applying a "drive-by fix" to disallow lambdas in the same context.

::: std

[9]{.pnum} Preprocessing directives of the forms
```cpp
     # if      $constant-expression$ $new-line$ $group$@~_opt_~@
     # elif    $constant-expression$ $new-line$ $group$@~_opt_~@
```
check whether the controlling constant expression evaluates to nonzero. [The program is ill-formed if a `$splice-specifier$` or `$lambda-expression$` appears in the controlling constant expression.]{.addu}

:::



## Library

### [structure.specifications]{.sref} Detailed specifications {-}

For convenience, we're going to add a new library element to [structure.specifications]{.sref}/3:

::: std
[3]{.pnum} Descriptions of function semantics contain the following elements (as appropriate):

* [#.1]{.pnum} *Constraints*: [...]

* [#.2]{.pnum} *Mandates*: the conditions that, if not met, render the program ill-formed. [...]

::: addu
* [#.2+1]{.pnum} *Constant When*: the conditions that are required for a call to this function to be a constant subexpression ([defns.const.subexpr]).
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
Let F denote a standard library function, member function, or function template.
If F does not designate an addressable function, it is unspecified if or how a reflection value designating the associated entity can be formed.
[For example, `std::meta::members_of` might not return reflections of standard functions that an implementation handles through an extra-linguistic mechanism.]{.note}

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

    // [meta.unary.cat], primary type categories
    template<class T>
      constexpr bool is_void_v = is_void<T>::value;
...
    template<class T>
      constexpr bool is_function_v = is_function<T>::value;
+   template<class T>
+     constexpr bool is_reflection_v = is_reflection<T>::value;
```
:::

### [meta.unary.cat]{.sref} Primary type categories {-}

Add the `is_reflection` primary type category to the table in paragraph 3:

::: std
<table>
<tr style="text-align:center"><th>Template</th><th>Condition</th><th>Comments</th></tr>
<tr><td>
```cpp
template <class T>
struct is_void;
```
</td><td style="text-align:center; vertical-align: middle">`T` is `void`</td><td></td></tr>
<tr style="text-align:center"><td>...</td><td>...</td><td>...</td></tr>
<tr><td>
::: addu
```cpp
template <class T>
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
  consteval bool has_linkage(info r);

  consteval bool is_complete_type(info r);
  consteval bool has_complete_definition(info r);

  consteval bool is_namespace(info r);
  consteval bool is_variable(info r);
  consteval bool is_type(info r);
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
  consteval bool has_template_arguments(info r);

  consteval bool is_value(info r);
  consteval bool is_object(info r);

  consteval bool is_structured_binding(info r);

  consteval bool is_class_member(info r);
  consteval bool is_namespace_member(info r);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);

  consteval bool has_default_member_initializer(info r);

  consteval info type_of(info r);
  consteval info object_of(info r);
  consteval info value_of(info r);
  consteval info parent_of(info r);
  consteval info dealias(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);

  // [meta.reflection.member.queries], reflection member queries
  consteval vector<info> members_of(info r);
  consteval vector<info> bases_of(info type);
  consteval vector<info> static_data_members_of(info type);
  consteval vector<info> nonstatic_data_members_of(info type);
  consteval vector<info> enumerators_of(info type_enum);

  consteval vector<info> get_public_members(info type);
  consteval vector<info> get_public_bases(info type);
  consteval vector<info> get_public_static_data_members(info type);
  consteval vector<info> get_public_nonstatic_data_members(info type);

  // [meta.reflection.layout], reflection layout queries
  struct member_offset {
    ptrdiff_t bytes;
    ptrdiff_t bits;
    constexpr ptrdiff_t total_bits() const;
    auto operator<=>(member_offset const&) const = default;
  };
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
    consteval info reflect_value(const T& value);
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

  // [meta.reflection.unary.cat], primary type categories
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

  // [meta.reflection.unary.comp], composite type categories
  consteval bool is_reference_type(info type);
  consteval bool is_arithmetic_type(info type);
  consteval bool is_fundamental_type(info type);
  consteval bool is_object_type(info type);
  consteval bool is_scalar_type(info type);
  consteval bool is_compound_type(info type);
  consteval bool is_member_pointer_type(info type);

  // [meta.reflection unary.prop], type properties
  consteval bool is_const_type(info type);
  consteval bool is_volatile_type(info type);
  consteval bool is_trivially_copyable_type(info type);
  consteval bool is_standard_layout_type(info type);
  consteval bool is_empty_type(info type);
  consteval bool is_polymorphic_type(info type);
  consteval bool is_abstract_type(info type);
  consteval bool is_final_type(info type);
  consteval bool is_aggregate_type(info type);
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

  consteval bool is_implicit_lifetime_type(info type);

  consteval bool has_virtual_destructor(info type);

  consteval bool has_unique_object_representations(info type);

  consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
  consteval bool reference_converts_from_temporary(info type_dst, info type_src);

  // [meta.reflection.unary.prop.query], type property queries
  consteval size_t rank(info type);
  consteval size_t extent(info type, unsigned i = 0);

  // [meta.reflection.rel], type relations
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

  // [meta.reflection.trans.cv], const-volatile modifications
  consteval info remove_const(info type);
  consteval info remove_volatile(info type);
  consteval info remove_cv(info type);
  consteval info add_const(info type);
  consteval info add_volatile(info type);
  consteval info add_cv(info type);

  // [meta.reflection.trans.ref], reference modifications
  consteval info remove_reference(info type);
  consteval info add_lvalue_reference(info type);
  consteval info add_rvalue_reference(info type);

  // [meta.reflection.trans.sign], sign modifications
  consteval info make_signed(info type);
  consteval info make_unsigned(info type);

  // [meta.reflection.trans.arr], array modifications
  consteval info remove_extent(info type);
  consteval info remove_all_extents(info type);

  // [meta.reflection.trans.ptr], pointer modifications
  consteval info remove_pointer(info type);
  consteval info add_pointer(info type);

  // [meta.reflection.trans.other], other transformations
  consteval info remove_cvref(info type);
  consteval info decay(info type);
  template <reflection_range R = initializer_list<info>>
    consteval info common_type(R&& type_args);
  template <reflection_range R = initializer_list<info>>
    consteval info common_reference(R&& type_args);
  consteval info type_underlying_type(info type);
  template <reflection_range R = initializer_list<info>>
    `consteval info invoke_result(info type, R&& type_args);
  consteval info unwrap_reference(info type);
  consteval info unwrap_ref_decay(info type);

  // [meta.reflection.tuple.variant], tuple and variant queries
  consteval size_t tuple_size(info type);
  consteval info tuple_element(size_t index, info type);

  consteval size_t variant_size(info type);
  consteval info variant_alternative(size_t index, info type);
}
```

[1]{.pnum} Each function, and each instantiation of each function template, specified in this header is a designated addressable function ([namespace.std]).

[2]{.pnum} The behavior of any function specified in namespace `std::meta` is implementation-defined when a reflection of a construct not otherwise specified by this document is provided as an argument.

[Values of type `std::meta::info` may represent implementation-defined constructs ([basic.fundamental]{.sref}).]{.note}

::: note
 The behavior of many of the functions specified in namespace `std::meta` have semantics that would be affected by the completeness of class types represented by reflection arguments ([temp.inst]). For such functions, for any reflection `r` such that `dealias(r)` represents a specialization of a templated class with a reachable definition, the specialization is implicitly instantiated.

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

[3]{.pnum} Any function in namespace `std::meta` that whose return type is `string_view` or `u8string_view` returns an object `$V$` such that `$V$.data()[$V$.size()] == '\0'`.

::: example
```cpp
struct C { };

constexpr string_view sv = identifier_of(^^C);
static_assert(sv == "C");
static_assert(sv.data()[0] == 'C');
static_assert(sv.data()[1] == '\0');
```
:::
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

[#]{.pnum} This enum class specifies constants used to identify operators that can be overloaded, with the meanings listed in Table 1. The values of the constants are distinct.

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

[#]{.pnum} *Returns*: `string_view` or `u8string_view` containing the characters of the operator symbol name corresponding to `op`, respectively encoded with the ordinary literal encoding or with UTF-8.
:::
:::

### [meta.reflection.names] Reflection names and locations {-}

::: std
::: addu
```cpp
consteval bool has_identifier(info r);
```

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` is an unnamed entity other than a class that has a typedef name for linkage purposes ([dcl.typedef]{.sref}), then `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a class type `$C$`, then `true` when either the `$class-name$` of `$C$` is an identifier or `$C$` has a typedef name for linkage purposes. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a function, then `true` if the function is not a function template specialization, constructor, destructor, operator function, or conversion function. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a function template, then `true` if `r` does not represent a constructor template, operator function template, or conversion function template. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents variable or a type alias, then `!has_template_arguments(r)`.
* [#.#]{.pnum} Otherwise, if `r` represents a structured binding, enumerator, non-static data member, template, namespace, or namespace alias, then `true`. Otherwise, `false`.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `has_identifier(type_of(r))`.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}); `$N$ != -1`.

```cpp
consteval string_view identifier_of(info r);
consteval u8string_view u8identifier_of(info r);
```

[#]{.pnum} Let *E* be UTF-8 if returning a `u8string_view`, and otherwise the ordinary literal encoding.

[#]{.pnum} *Constant When*: `has_identifier(r)` is `true` and the identifier that would be returned (see below) is representable by `$E$`.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` represents a literal operator or literal operator template, then the `$ud-suffix$` of the operator or operator template.
* [#.#]{.pnum} Otherwise, if `r` represents a class type, then either the typedef name for linkage purposes or the identifier introduced by the declaration of the represented type.
* [#.#]{.pnum} Otherwise, if `r` represents an entity, then the identifier introduced by the declaration of that entity.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `identifier_of(type_of(r))` or `u8identifier_of(type_of(r))`, respectively.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}); a `string` or `u8string` respectively containing the identifier `$N$` encoded with `$E$`.

```cpp
consteval string_view display_string_of(info r);
consteval u8string_view u8display_string_of(info r);
```

[#]{.pnum} *Returns*: An implementation-defined `string_view` or `u8string_view`, respectively.

[#]{.pnum} *Recommended practice*: Where possible, implementations should return a string suitable for identifying the represented construct.

```cpp
consteval source_location source_location_of(info r);
```

[#]{.pnum} *Returns*: If `r` represents a value, a non-class type, the global namespace, or a data member description, then `source_location{}`. Otherwise, an implementation-defined `source_location` value.

[#]{.pnum} *Recommended practice*: If `r` represents an entity, name, or direct base class relationship that was introduced by a declaration, implementations should return a value corresponding to a declaration of the represented construct that is reachable from the evaluation construct. If there are multiple such declarations and one is a definition, a value corresponding to the definition is preferred.
:::
:::

### [meta.reflection.queries] Reflection queries {-}

::: std
::: addu
```cpp
consteval bool is_public(info r);
consteval bool is_protected(info r);
consteval bool is_private(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a class member or direct base class relationship that is public, protected, or private, respectively. Otherwise, `false`.

```cpp
consteval bool is_virtual(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents either a virtual member function or a direct base class relationship that is virtual. Otherwise, `false`.

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

[#]{.pnum} *Returns*: `true` if `r` represents a function that is a deleted function ([dcl.fct.def.delete]) or defined as defaulted ([dcl.fct.def.default]), respectively. Otherwise, `false`.

```cpp
consteval bool is_user_provided(info r);
consteval bool is_user_declared(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a function that is user-provided or user-declared ([dcl.fct.def.default]{.sref}), respectively. Otherwise, `false`.


```cpp
consteval bool is_explicit(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a member function that is declared explicit. Otherwise, `false`. [If `r` represents a member function template that is declared `explicit`, `is_explicit(r)` is still `false` because in general such queries for templates cannot be answered.]{.note}

```cpp
consteval bool is_noexcept(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a `noexcept` function type or a function or member function with a non-throwing exception specification ([except.spec]). Otherwise, `false`. [If `r` represents a function template that is declared `noexcept`, `is_noexcept(r)` is still `false` because in general such queries for templates cannot be answered.]{.note}

```cpp
consteval bool is_bit_field(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a bit-field, or if `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}) for which `$W$` is not `-1`. Otherwise, `false`.

```cpp
consteval bool is_enumerator(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an enumerator. Otherwise, `false`.

```cpp
consteval bool is_const(info r);
consteval bool is_volatile(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a const or volatile type (respectively), a const- or volatile-qualified function type (respectively), or an object, variable, non-static data member, or function with such a type. Otherwise, `false`.

```cpp
consteval bool is_mutable_member(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a `mutable` non-static data member. Otherwise, `false`.

```cpp
consteval bool is_lvalue_reference_qualified(info r);
consteval bool is_rvalue_reference_qualified(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a lvalue- or rvalue-reference qualified function type (respectively), or a member function with such a type. Otherwise, `false`.

```cpp
consteval bool has_static_storage_duration(info r);
consteval bool has_thread_storage_duration(info r);
consteval bool has_automatic_storage_duration(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an object or variable that has static, thread, or automatic storage duration, respectively ([basic.stc]). Otherwise, `false`.

```cpp
consteval bool has_internal_linkage(info r);
consteval bool has_module_linkage(info r);
consteval bool has_external_linkage(info r);
consteval bool has_linkage(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a variable, function, type, template, or namespace whose name has internal linkage, module linkage, external linkage, or any linkage, respectively ([basic.link]). Otherwise, `false`.

```cpp
consteval bool is_complete_type(info r);
```

[#]{.pnum} *Returns*: `true` if `is_type(r)` is `true` and there is some point in the evaluation context from which the type represented by `dealias(r)` is not an incomplete type ([basic.types]). Otherwise, `false`.

```cpp
consteval bool has_complete_definition(info r);
```

[#]{.pnum} Returns: `true` if `r` represents a function, class type, or enumeration type, such that no entities not already declared may be introduced within the scope of the entity represented by `r`. Otherwise `false`.

```cpp
consteval bool is_namespace(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a namespace or namespace alias. Otherwise, `false`.

```cpp
consteval bool is_variable(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a variable. Otherwise, `false`.

```cpp
consteval bool is_type(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents an entity whose underlying entity is a type. Otherwise, `false`.

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

[#]{.pnum} *Returns*: `true` if `r` represents a conversion function, operator function, or literal operator, respectively. Otherwise, `false`.

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
consteval bool has_template_arguments(info r);
```
[#]{.pnum} *Returns*: `true` if `r` represents a specialization of a function template, variable template, class template, or an alias template. Otherwise, `false`.

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
consteval info type_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents a value, object, variable, function that is not a constructor or destructor, enumerator, non-static data member, bit-field, direct base class relationship, or data member description.

[#]{.pnum} *Returns*: If `r` represents an entity, object, or value, then a reflection of the type of what is represented by `r`. Otherwise, if `r` represents a direct base class relationship, then a reflection of the type of the direct base class. Otherwise, for a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}), a reflection of the type `$T$`.

```cpp
consteval info object_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents either an object with static storage duration ([basic.stc.general]), or a variable associated with, or referring to, such an object.

[#]{.pnum} *Returns*: If `r` represents a variable, then a reflection of the object associated with, or referred to by, the variable. Otherwise, `r`.

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
consteval info value_of(info r);
```

[#]{.pnum} *Constant When*: `r` is a reflection representing

* [#.#]{.pnum} a value,
* [#.#]{.pnum} an enumerator, or
* [#.#]{.pnum} an object or variable `$X$` such that the lifetime of `$X$` has not ended, the type of `$X$` is a structural type, and either `$X$` is usable in constant expressions from some point in the evaluation context or the lifetime of `$X$` began in the core constant expression currently under evaluation ([expr.const]), ([temp.type]).

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `r` is a reflection of an object `o`, or a reflection of a variable which designates an object `o`, then a reflection of the value held by `o`. The reflected value has type `type_of(o)`, with the cv-qualifiers removed if this is a scalar type
* [#.#]{.pnum} Otherwise, if `r` is a reflection of an enumerator, then a reflection of the value of the enumerator.
* [#.#]{.pnum} Otherwise, `r`.

::: example
```cpp
constexpr int x = 0;
constexpr int y = 0;

static_assert(^^x != ^^y);                         // OK, x and y are different variables so their
                                                 // reflections compare different
static_assert(value_of(^^x) == value_of(^^y));     // OK, both value_of(^^x) and value_of(^^y) represent
                                                 // the value 0
static_assert(value_of(^^x) == reflect_value(0)); // OK, likewise
```
:::

```cpp
consteval info parent_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents a variable, structured binding, function, enumerator, class, class member, bit-field, template, namespace or namespace alias (other than `::`), type alias, or direct base class relationship.

[#]{.pnum} *Returns*: If `r` represents a non-static data member that is a direct member of an anonymous union, then a reflection representing the innermost enclosing anonymous union. Otherwise, a reflection of the class, function, or namespace that is the target scope ([basic.scope.scope]) of the first declaration of what is represented by `r`.

```cpp
consteval info dealias(info r);
```

[#]{.pnum} *Returns*: A reflection representing the underlying entity of `r`.

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
consteval info template_of(info r);
consteval vector<info> template_arguments_of(info r);
```
[#]{.pnum} *Constant When*: `has_template_arguments(r)` is `true`.

[#]{.pnum} *Returns*: A reflection of the primary template of `r`, and the reflections of the template arguments of the specialization represented by `r`, respectively.

[#]{.pnum}

::: example
```
template <class T, class U=T> struct Pair { };
template <class T> struct Pair<char, T> { };
template <class T> using PairPtr = Pair<T*>;

static_assert(template_of(^^Pair<int>) == ^^Pair);
static_assert(template_of(^^Pair<char, char>) == ^^Pair);
static_assert(template_arguments_of(^^Pair<int>).size() == 2);

static_assert(template_of(^^PairPtr<int>) == ^^PairPtr);
static_assert(template_arguments_of(^^PairPtr<int>).size() == 1);
```
:::
:::
:::

### [meta.reflection.member.queries], Reflection member queries  {-}

::: std
::: addu
```cpp
consteval vector<info> members_of(info r);
```

[#]{.pnum} *Constant When*: `r` is a reflection representing either a class type that is complete from some point in the evaluation context or a namespace.

[#]{.pnum} A member of either

* [#.#]{.pnum} a class that is not a closure type, or
* [#.#]{.pnum} a namespace

is _members-of-representable_ if it is not a specialization of a template and if it is

* [#.#]{.pnum} a class that is not a closure type,
* [#.#]{.pnum} a type alias,
* [#.#]{.pnum} a primary class template, function template, primary variable template, alias template, or concept,
* [#.#]{.pnum} a variable or reference,
* [#.#]{.pnum} a function whose constraints (if any) are satisfied unless it is a prospective destructor that is not a selected destructor ([class.dtor]),
* [#.#]{.pnum} a non-static data member or unnamed bit-field, other than members of an anonymous union that is directly or indirectly members-of-representable,
* [#.#]{.pnum} a namespace, or
* [#.#]{.pnum} a namespace alias.

A member of a closure type is members-of-representable if it is

* [#.#]{.pnum} a function call operator or function call operator template, or
* [#.#]{.pnum} a conversion function or conversion function template.

It is implementation-defined whether other members of closure types are _members-of-representable_.

[Counterexamples of members-of-representable members include: injected class names, partial template specializations, friend declarations, static assertions, and non-static data members of closure types that correspond to captured entities.]{.note}

[3]{.pnum} A member `$M$` of a class or namespace _members-of-precedes_ a point `$P$` if a declaration of `$M$` precedes either `$P$` or a point immediately following the `$class-specifier$` of a class for which `$P$` is in a complete-class context.

[#]{.pnum} *Returns*: A `vector` containing reflections of all members-of-representable members of the entity represented by `r` that members-of-precede some point in the evaluation context ([expr.const]).
If `r` represents a class `$C$`, then the vector also contains reflections representing all unnamed bit-fields declared within the member-specification of `$C$`.
Class members and unnamed bit-fields are indexed in the order in which they are declared, but the order of namespace members is unspecified.
[Base classes are not members. Implicitly-declared special members appear after any user-declared members.]{.note}

::: example
```cpp
class S {
  class Incomplete;

  /* P1 */ consteval {
    // OK, n == 7. The member Incomplete::x members-of-precedes the synthesized point P2 associated
    // with the injected declaration produced by the call to define_aggregate.
    int n = members_of(
        define_aggregate(^^Incomplete, {data_member_spec(^^int, {.name="x"})})
      ).size();
  }
}; /* P2 */
```
:::

```cpp
consteval vector<info> bases_of(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` is a reflection representing a complete class type.

[#]{.pnum} *Returns*: Let `C` be the type represented by `dealias(type)`. A `vector` containing the reflections of all the direct base class relationships, if any, of `C`.
The direct base class relationships are indexed in the order in which the corresponding base classes appear in the *base-specifier-list* of `C`.

```cpp
consteval vector<info> static_data_members_of(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `members_of(type)` such that `is_variable(e)` is `true`, in order.

```cpp
consteval vector<info> nonstatic_data_members_of(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `members_of(type)` such that `is_nonstatic_data_member(e)` is `true`, in order.

```cpp
consteval vector<info> enumerators_of(info type_enum);
```

[#]{.pnum} *Constant When*: `dealias(type_enum)` represents an enumeration type and `has_complete_definition(dealias(type_enum))` is `true`.

[#]{.pnum} *Returns*: A `vector` containing the reflections of each enumerator of the enumeration represented by `dealias(type_enum)`, in the order in which they are declared.

```cpp
consteval vector<info> get_public_members(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `members_of(type)` such that `is_public(e)` is `true`, in order.

```cpp
consteval vector<info> get_public_bases(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `bases_of(type)` such that `is_public(e)` is `true`, in order.

```cpp
consteval vector<info> get_public_static_data_members(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `static_data_members_of(type)` such that `is_public(e)` is `true`, in order.

```cpp
consteval vector<info> get_public_nonstatic_data_members(info type);
```

[#]{.pnum} *Constant When*: `dealias(type)` represents a complete class type.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `nonstatic_data_members_of(type)` such that `is_public(e)` is `true`, in order.

:::
:::


### [meta.reflection.layout] Reflection layout queries {-}

::: std
::: addu
```cpp
constexpr ptrdiff_t member_offset::total_bits() const;
```
[#]{.pnum} *Returns*: `bytes * CHAR_BIT + bits`.

```cpp
consteval member_offset offset_of(info r);
```

[#]{.pnum} *Constant When*: `r` represents a non-static data member, unnamed bit-field, or direct base class relationship other than a virtual base class of an abstract class.

[#]{.pnum} Let `$V$` be the offset in bits from the beginning of a complete object of type `parent_of(r)` to the subobject associated with the entity represented by `r`.

[#]{.pnum} *Returns*: `{$V$ / CHAR_BIT, $V$ % CHAR_BIT}`.

```cpp
consteval size_t size_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.

[#]{.pnum} *Returns*: If `r` represents a non-static data member whose corresponding subobject has type `$T$`, or a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}), then `sizeof($T$)`. Otherwise, if `dealias(r)` represents a type `T`, then `sizeof(T)`. Otherwise, `size_of(type_of(r))`.

[The subobject corresponding to a non-static data member of reference type has the same size and alignment as the corresponding pointer type.]{.note}

```cpp
consteval size_t alignment_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection representing a type, object, variable, non-static data member that is not a bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.

[#]{.pnum} *Returns*:

* [#.#]{.pnum} If `dealias(r)` represents a type, variable, or object, then the alignment requirement of the entity or object.
* [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then `alignment_of(type_of(r))`.
* [#.#]{.pnum} Otherwise, if `r` represents a non-static data member, then the alignment requirement of the subobject associated with the represented entity within any object of type `parent_of(r)`.
* [#.#]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}). If `$A$ != 1`, then the value `$A$`. Otherwise `alignof($T$)`.

```cpp
consteval size_t bit_size_of(info r);
```

[#]{.pnum} *Constant When*: `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member, unnamed bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.

[#]{.pnum} *Returns*: If `r` represents a non-static data member that is a bit-field or unnamed bit-field with width `$W$`, then `$W$`. If `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}), then `$W$` if `$W$ != -1`, otherwise `sizeof($T$) * CHAR_BIT`. Otherwise, `CHAR_BIT * size_of(r)`.
:::
:::


### [meta.reflection.extract] Value extraction {-}

::: std
::: addu
[1]{.pnum} The `extract` function template may be used to extract a value out of a reflection when the type is known.

[#]{.pnum} The following are defined for exposition only to aid in the specification of `extract`:
```cpp
template <class T>
  consteval T $extract-ref$(info r); // exposition only
```

[#]{.pnum} [`T` is a reference type.]{.note}

[#]{.pnum} *Constant When*: `r` represents a variable or object of type `U` that is usable in constant expressions from some point in the evaluation context and `is_convertible_v<remove_reference_t<U>(*)[], remove_reference_t<T>(*)[]>` is `true`.

[#]{.pnum} *Returns*: the object represented by `object_of(r)`.

```cpp
template <class T>
  consteval T $extract-member-or-function$(info r); // exposition only
```

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} If `r` represents a non-static data member of a class `C` with type `X`, then when `T` is `X C::*` and `r` does not represent a bit-field.
- [#.#]{.pnum} Otherwise, if `r` represents an implicit object member function of class `C` with type `F` or `F noexcept`, then when `T` is `F C::*`.
- [#.#]{.pnum} Otherwise, `r` represents a function, static member function, or explicit object member function of function type `F` or `F noexcept`, then when `T` is `F*`.

[#]{.pnum} *Returns*:

- [#.#]{.pnum} If `T` is a pointer type, then a pointer value pointing to the entity represented by `r`.
- [#.#]{.pnum} Otherwise, a pointer-to-member value designating the entity represented by `r`.

```cpp
template <class T>
  consteval T $extract-val$(info r); // exposition only
```

[#]{.pnum} Let `U` be the type of the value that `r` represents.

[#]{.pnum} *Constant When*:

  - [#.#]{.pnum} `U` is a pointer type, `T` and `U` are similar types ([conv.qual]), and `is_convertible_v<U, T>` is `true`,
  - [#.#]{.pnum} `U` is not a pointer type and the cv-unqualified types of `T` and `U` are the same, or
  - [#.#]{.pnum} `U` is a closure type, `T` is a function pointer type, and the value that `r` represents is convertible to `T`.

[#]{.pnum} *Returns*: the value that `r` represents converted to `T`.

```cpp
template <class T>
  consteval T extract(info r);
```

[#]{.pnum} *Effects*:

- [#]{.pnum} If `T` is a reference type, then equivalent to `return $extract-ref$<T>(r);`
- [#]{.pnum} Otherwise, if `r` represents a function, non-static data member, or member function, equivalent to `return $extract-member-or-function$<T>(r);`
- [#]{.pnum} Otherwise, equivalent to `return $extract-value$<T>(value_of(r))`

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

[#]{.pnum} Let `Z` be the template represented by `templ` and let `Args...` be the sequence of entities, values, and objects represented by the elements of `arguments`.

[#]{.pnum} *Returns*: `true` if `Z<Args...>` is a valid *template-id* ([temp.names]). Otherwise, `false`.

[#]{.pnum} *Remarks*: If attempting to substitute leads to a failure outside of the immediate context, the program is ill-formed.

```cpp
template <reflection_range R = initializer_list<info>>
consteval info substitute(info templ, R&& arguments);
```

[#]{.pnum} *Constant When*: `can_substitute(templ, arguments)` is `true`.

[#]{.pnum} Let `Z` be the template represented by `templ` and let `Args...` be the sequence of entities, values, and objects represented by the elements of `arguments`.

[#]{.pnum} *Returns*: `^^Z<Args...>`.

[#]{.pnum} [`Z<Args..>` is only instantiated if the deduction of a placeholder type necessarily requires that instantiation.]{.note}

:::
:::

### [meta.reflection.result] Expression result reflection {-}

::: std
::: addu
```cpp
template <typename T>
  consteval info reflect_value(const T& expr);
```

[#]{.pnum} *Mandates*: `T` is a structural type that is neither a reference type nor an array type.

[#]{.pnum} Let `$V$` be the value computed by an lvalue-to-rvalue conversion applied to `expr`.

[#]{.pnum} *Constant When*: `$V$` satisfies the constraints for a value computed by a prvalue constant expression and no constituent reference of `$V$` refers to, or constituent value of `$V$` is a pointer to:

  - [#.#]{.pnum} a temporary object ([class.temporary]),
  - [#.#]{.pnum} a string literal object ([lex.string]),
  - [#.#]{.pnum} the result of a `typeid` expression ([expr.typeid]),
  - [#.#]{.pnum} an object associated with a predefined `__func__` variable ([dcl.fct.def.general]), or
  - [#.#]{.pnum} an object that is not constexpr-representable from a program point in a namespace scope.

[#]{.pnum} *Returns*: A reflection of `$V$`. The type of the represented value is the cv-unqualified version of `T`.

```cpp
template <typename T>
  consteval info reflect_object(T& expr);
```

[#]{.pnum} *Mandates*: `T` is not a function type.

[#]{.pnum} *Constant When*: `expr` designates an object or function that

  - [#.#]{.pnum} is constexpr-representable from a program point in a namespace scope ([expr.const]),
  - [#.#]{.pnum} is not a temporary object ([class.temporary]),
  - [#.#]{.pnum} is not a string literal object ([lex.string]),
  - [#.#]{.pnum} is not the result of a `typeid` expression ([expr.typeid]), and
  - [#.#]{.pnum} is not an object associated with a predefined `__func__` variable ([dcl.fct.def.general]).

[#]{.pnum} *Returns*: A reflection of the object designated by `expr`.

```cpp
template <typename T>
  consteval info reflect_function(T& expr);
```

[#]{.pnum} *Mandates*: `T` is a function type.

[#]{.pnum} *Returns*: `^^fn`, where `fn` is the function designated by `expr`.
:::
:::

### [meta.reflection.define.aggregate] Reflection class definition generation  {-}

::: std
::: addu

[1]{.pnum} The classes `data_member_options` and `name_type` are consteval-only types ([basic.types.general]), and are not a structural types ([temp.param]).

```cpp
struct data_member_options {
  struct name_type {
    template<class T> requires constructible_from<u8string, T>
      consteval name_type(T &&);

    template<class T> requires constructible_from<string, T>
      consteval name_type(T &&);

    variant<u8string, string> $contents$;    // $exposition only$
  };

  optional<name_type> name;
  optional<int> alignment;
  optional<int> bit_width;
  bool no_unique_address = false;
};
```

```cpp
template <class T> requires constructible_from<u8string, T>
consteval data_member_options::name_type(T&& value);
```

[#]{.pnum} *Effects*: Initializes `$contents$` with `u8string(value)`.

```cpp
template<class T> requires constructible_from<string, T>
consteval data_member_options::name_type(T&& value);
```
[#]{.pnum} *Effects*: Initializes `$contents$` with `string(value)`.

::: note
`name_type` provides a simple inner class that can be implicitly constructed from anything convertible to `string` or `u8string`. This allows a `data_member_spec` to accept an ordinary string literal (or `string_view`, `string`, etc) or a UTF-8 string literal (or `u8string_view`, `u8string`, etc) equally well.

```cpp
constexpr auto mem1 = data_member_spec(^^int, {.name="ordinary_literal_encoding"});
constexpr auto mem2 = data_member_spec(^^int, {.name=u8"utf8_encoding"});
```

:::

```cpp
consteval info data_member_spec(info type,
                                data_member_options options);
```
[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `dealias(type)` represents a type `cv $T$` where `$T$` is either an object type or a reference type;
- [#.#]{.pnum} if `options.name` contains a value, then:
  - [#.#.#]{.pnum} `holds_alternative<u8string>(options.name->$contents$)` is `true` and `get<u8string>(options.name->$contents$)` contains a valid identifier when interpreted with UTF-8, or
  - [#.#.#]{.pnum} `holds_alternative<string>(options.name->$contents$)` is `true` and `get<string>(options.name->$contents$)` contains a valid identifier when interpreted with the ordinary literal encoding;
- [#.#]{.pnum} otherwise, if `options.name` does not contain a value, then `options.bit_width` contains a value;
- [#.#]{.pnum} if `options.alignment` contains a value, it is an alignment value ([basic.align]) not less than `alignment_of(type)`; and
- [#.#]{.pnum} if `options.bit_width` contains a value `$V$`, then
  - [#.#.#]{.pnum} `is_integral_type(type) || is_enumeration_type(type)` is `true`,
  - [#.#.#]{.pnum} `options.alignment` does not contain a value,
  - [#.#.#]{.pnum} `options.no_unique_address` is `false`, and
  - [#.#.#]{.pnum} if `$V$` equals `0` then `options.name` does not contain a value.

[#]{.pnum} *Returns*: A reflection of a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]{.sref}) where

- [#.#]{.pnum} `$T$` is the type or type alias represented by `type`,
- [#.#]{.pnum} `$N$` is either the identifier encoded by `options.name` or `-1` if `options.name` is empty,
- [#.#]{.pnum} `$A$` is either the alignment value held by `options.alignment` or `-1` if `options.alignment` is empty,
- [#.#]{.pnum} `$W$` is either the value held by `options.bit_width` or `-1` if `options.bit_width` is empty, and
- [#.#]{.pnum} `$NUA$` is the value held by `options.no_unique_address`.

[#]{.pnum} [The returned reflection value is primarily useful in conjunction with `define_aggregate`. Certain other functions in `std::meta` (e.g., `type_of`, `identifier_of`) can also be used to query the characteristics indicated by the arguments provided to `data_member_spec`.]{.note}

```cpp
consteval bool is_data_member_spec(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents a data member description. Otherwise, `false`.

```c++
  template <reflection_range R = initializer_list<info>>
  consteval info define_aggregate(info class_type, R&& mdescrs);
```

[#]{.pnum} *Constant When*: Letting `$C$` be the class represented by `class_type` and `@$r$~$K$~@` be the `$K$`^th^ reflection value in `mdescrs`,

- [#.#]{.pnum} `$C$` is incomplete from every point in the evaluation context;
- [#.#]{.pnum} `$C$` is not a class being defined;
- [#.#]{.pnum} `is_data_member_spec(@$r$~$K$~@)` is `true` for every `@$r$~$K$~@` in `mdescrs`;
- [#.#]{.pnum} the type represented by `type_of(@$r$~$K$~@)` is a complete type for every `@$r$~$K$~@` in `mdescrs`; and
- [#.#]{.pnum} for every pair 0 ≤ `$K$` < `$L$` < `mdescrs.size()`,  if `has_identifier(@$r$~$K$~@) && has_identifier(@$r$~$L$~@)` is `true`, then either `u8identifier_of(@$r$~$K$~@) != u8identifier_of(@$r$~$L$~@)` is `true` or `u8identifier_of(@$r$~$K$~@) == u8"_"` is `true`. [Every provided identifier is unique or `"_"`.]{.note}

[`$C$` could be a class template specialization for which there is a reachable definition of the primary class template. In this case, an explicit specialization is injected.]{.note}

[#]{.pnum} Let {`@$t$~k~@`} be a sequence of reflections and {`@$o$~k~@`} be a sequence of `data_member_options` values such that

    data_member_spec(@$t$~$k$~@, @$o$~$k$~@) == @$r$~$k$~@

for every `@$r$~$k$~@` in `mdescrs`.

[#]{.pnum} *Effects*:
Produces an injected declaration `$D$` ([expr.const]) that provides a definition for `$C$` with properties as follows:

- [#.1]{.pnum} The target scope of `$D$` is the scope to which `$C$` belongs ([basic.scope.scope]).
- [#.#]{.pnum} The locus of `$D$` follows immediately after the core constant expression currently under evaluation.
- [#.#]{.pnum} If `$C$` is a specialization of a class template `$T$`, then `$D$` is is an explicit specialization of `$T$`.
- [#.#]{.pnum} `$D$` contains a public non-static data member or unnamed bit-field corresponding to each reflection value `@$r$~$K$~@` in `mdescrs`. For every other `@$r$~$L$~@` in `mdescrs` such that `$K$ < $L$`, the declaration of `@$r$~$K$~@` precedes the declaration of `@$r$~$L$~@`.
- [#.#]{.pnum} A non-static data member or unnamed bit-field corresponding to each `@$r$~$K$~@` is declared with the type or type alias represented by `@$t$~$K$~@`.
- [#.#]{.pnum} A non-static data member corresponding to a reflection `@$r$~$K$~@` for which `@$o$~$K$~@.no_unique_address` is `true` is declared with the attribute `[[no_unique_address]]`.
- [#.#]{.pnum} A non-static data member or unnamed bit-field corresponding to a reflection `@$r$~$K$~@` for which `@$o$~$K$~@.bit_width` contains a value is declared as a bit-field whose width is that value.
- [#.#]{.pnum} A non-static data member corresponding to a reflection `@$r$~$K$~@` for which `@$o$~$K$~@.alignment` contains a value is declared with the `$alignment-specifier$` `alignas(@$o$~$K$~@.alignment)`.
- [#.#]{.pnum} A non-static data member or unnamed bit-field corresponding to a reflection `@$r$~$K$~@` is declared with a name determined as follows:
  - If `@$o$~$K$~@.name` does not contain a value, an unnamed bit-field is declared.
  - Otherwise, the name of the non-static data member is the identifier determined by the character sequence encoded by `u8identifier_of(@$r$~$K$~@)` in UTF-8.
- [#.#]{.pnum} If `$C$` is a union type for which any of its members are not trivially default constructible, then `$D$` has a user-provided default constructor which has no effect. [If P3074 is adopted, do not include this bullet.]{.draftnote}
- [#.#]{.pnum} If `$C$` is a union type for which any of its members are not trivially destructible, then `$D$` has a user-provided destructor which has no effect. [If P3074 is adopted, do not include this bullet.]{.draftnote}

[#]{.pnum} *Returns*: `class_type`.

:::
:::

### [meta.reflection.unary] Unary type traits  {-}

::: std
::: addu
[1]{.pnum} Subclause [meta.reflection.unary] contains consteval functions that may be used to query the properties of a type at compile time.

[2]{.pnum} For each function taking an argument of type `meta::info` whose name contains `type`, a call to the function is a non-constant library call ([defns.nonconst.libcall]{.sref}) if that argument is not a reflection of a type or type alias. For each function taking an argument named `type_args`, a call to the function is a non-constant library call if any `meta::info` in that range is not a reflection of a type or a type alias.
:::
:::

#### [meta.reflection.unary.cat] Primary type categories  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$TRAIT$_type` defined in this subclause, `meta::$TRAIT$_type(^^T)` equals the value of the corresponding unary type trait `$TRAIT$_v<T>` as specified in [meta.unary.cat]{.sref}.

```cpp
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
```

[2]{.pnum}

::: example
```
namespace std::meta {
  consteval bool is_void_type(info type) {
    // one example implementation
    return extract<bool>(substitute(^^is_void_v, {type}));

    // another example implementation
    type = dealias(type);
    return type == ^^void
        || type == ^^const void
        || type == ^^volatile void
        || type == ^^const volatile void;
  }
}
```
:::
:::
:::

#### [meta.reflection.unary.comp] Composite type categories  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$TRAIT$_type` defined in this subclause, `meta::$TRAIT$_type(^^T)` equals the value of the corresponding unary type trait `$TRAIT$_v<T>` as specified in [meta.unary.comp]{.sref}.

```cpp
consteval bool is_reference_type(info type);
consteval bool is_arithmetic_type(info type);
consteval bool is_fundamental_type(info type);
consteval bool is_object_type(info type);
consteval bool is_scalar_type(info type);
consteval bool is_compound_type(info type);
consteval bool is_member_pointer_type(info type);
```
:::
:::

#### [meta.reflection.unary.prop] Type properties  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$UNARY-TRAIT$_type` or `meta::$UNARY-TRAIT$` defined in this subclause with type `bool(meta::info)`, `meta::$UNARY-TRAIT$_type(^^T)` or `meta::$UNARY-TRAIT$(^^T)` equals the value of the corresponding type property `$UNARY-TRAIT$_v<T>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any types or type aliases `T` and `U`, for each function `meta::$BINARY-TRAIT$_type` or `meta::$BINARY-TYPE$` defined in this subclause with type `bool(meta::info, meta::info)`, `meta::$BINARY-TRAIT$_type(^^T, ^^U)` or `meta::$BINARY-TRAIT$(^^T, ^^U)` equals the value of the corresponding type property `$BINARY-TRAIT$_v<T, U>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::equal(r, vector{^^U...})` is `true`, for each function template `meta::$VARIADIC-TRAIT$_type` defined in this subclause, `meta::$VARIADIC-TRAIT$_type(^^T, r)` equals the value of the corresponding type property `$VARIADIC-TRAIT$_v<T, U...>` as specified in [meta.unary.prop]{.sref}.

```cpp
consteval bool is_const_type(info type);
consteval bool is_volatile_type(info type);
consteval bool is_trivially_copyable_type(info type);
consteval bool is_standard_layout_type(info type);
consteval bool is_empty_type(info type);
consteval bool is_polymorphic_type(info type);
consteval bool is_abstract_type(info type);
consteval bool is_final_type(info type);
consteval bool is_aggregate_type(info type);
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

consteval bool is_implicit_lifetime_type(info type);

consteval bool has_virtual_destructor(info type);

consteval bool has_unique_object_representations(info type);

consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
consteval bool reference_converts_from_temporary(info type_dst, info type_src);
```
:::
:::

#### [meta.reflection.unary.prop.query] Type property queries  {-}

::: std
::: addu
```cpp
consteval size_t rank(info type);
```

[#]{.pnum} *Effects*: Equivalent to `return rank_v<T>;`, where `T` is the type represented by `dealias(type)`.

```cpp
consteval size_t extent(info type, unsigned i = 0);
```

[#]{.pnum} *Effects*: Equivalent to `return extent_v<T, I>;`, where `T` is the type represented by `dealias(type)` and `I` is a constant equal to `i`.

:::
:::

### [meta.reflection.rel], Type relations  {-}

::: std
::: addu
[1]{.pnum} The consteval functions specified in this subclause may be used to query relationships between types at compile time.

[#]{.pnum} For any types or type aliases `T` and `U`, for each function `meta::$REL$_type` defined in this subclause with type `bool(meta::info, meta::info)`, `meta::$REL$_type(^^T, ^^U)` equals the value of the corresponding type relation `$REL$_v<T, U>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::equal(r, vector{^^U...})` is `true`, for each binary function template `meta::$VARIADIC-REL$_type`, `meta::$VARIADIC-REL$_type(^^T, r)` equals the value of the corresponding type relation `$VARIADIC-REL$_v<T, U...>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any types or type aliases `T` and `R`, pack of types or type aliases `U...`, and range `r` such that `ranges::equal(r, vector{^^U...})` is `true`, for each ternary function template `meta::$VARIADIC-REL-R$_type` defined in this subclause, `meta::$VARIADIC-REL-R$(^^R, ^^T, r)_type` equals the value of the corresponding type relation `$VARIADIC-REL-R$_v<R, T, U...>` as specified in [meta.rel]{.sref}.

```cpp
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
```

[#]{.pnum} [If `t` is a reflection of the type `int` and `u` is a reflection of an alias to the type `int`, then `t == u` is `false` but `is_same_type(t, u)` is `true`. `t == dealias(u)` is also `true`.]{.note}.
:::
:::


### [meta.reflection.trans], Transformations between types  {-}

::: std
::: addu
[1]{.pnum} Subclause [meta.reflection.trans] contains consteval functions that may be used to transform one type to another following some predefined rule.
:::
:::

#### [meta.reflection.trans.cv], Const-volatile modifications  {-}
::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.cv]{.sref}.

```cpp
consteval info remove_const(info type);
consteval info remove_volatile(info type);
consteval info remove_cv(info type);
consteval info add_const(info type);
consteval info add_volatile(info type);
consteval info add_cv(info type);
```
:::
:::

#### [meta.reflection.trans.ref], Reference modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.ref]{.sref}.

```cpp
consteval info remove_reference(info type);
consteval info add_lvalue_reference(info type);
consteval info add_rvalue_reference(info type);
```
:::
:::

#### [meta.reflection.trans.sign], Sign modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.sign]{.sref}.
```cpp
consteval info make_signed(info type);
consteval info make_unsigned(info type);
```
:::
:::

#### [meta.reflection.trans.arr], Array modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.arr]{.sref}.
```cpp
consteval info remove_extent(info type);
consteval info remove_all_extents(info type);
```
:::
:::

#### [meta.reflection.trans.ptr], Pointer modifications  {-}
::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.ptr]{.sref}.
```cpp
consteval info remove_pointer(info type);
consteval info add_pointer(info type);
```
:::
:::

#### [meta.reflection.trans.other], Other transformations  {-}

[There are four transformations that are deliberately omitted here. `type_identity` and `enable_if` are not useful, `conditional(cond, t, f)` would just be a long way of writing `cond ? t : f`, and `basic_common_reference` is a class template intended to be specialized and not directly invoked.]{.ednote}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$MOD$` defined in this subclause with type `meta::info(meta::info)`, `meta::$MOD$(^^T)` returns the reflection of the corresponding type denoted by `$MOD$_t<T>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any pack of types or type aliases `T...` and range `r` such that `ranges::equal(r, vector{^^T...})` is `true`, for each unary function template `meta::$VARIADIC-MOD$` defined in this subclause, `meta::$VARIADIC-MOD$(r)` returns the reflection of the corresponding type denoted by `$VARIADIC-MOD$_t<T...>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::equal(r, vector{^^U...})` is `true`, `meta::invoke_result(^^T, r)` returns the reflection of the corresponding type denoted by `invoke_result_t<T, U...>` ([meta.trans.other]{.sref}).

```cpp
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

[#]{.pnum}

::: example
```cpp
// example implementation
consteval info unwrap_reference(info type) {
  type = dealias(type);
  if (has_template_arguments(type) && template_of(type) == ^^reference_wrapper) {
    return add_lvalue_reference(template_arguments_of(type)[0]);
  } else {
    return type;
  }
}
```
:::

:::
:::

#### [meta.reflection.tuple.variant], Tuple and Variant Queries {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `meta::$UNARY-TRAIT$` defined in this subclause with the type `size_t(meta::info)`, `meta::$UNARY-TRAIT$(^^T)` equals the value of the corresponding property `$UNARY-TRAIT$_v<T>` as defined in [tuple]{.sref} or [variant]{.sref}.

[2]{.pnum} For any type or type alias `T` and value `I`, for each function `meta::$BINARY-TRAIT$` defined in this subclause with the type `info(size_t, meta::info)`, `meta::$BINARY-TRAIT$(I, ^^T)` returns a reflection representing the type denoted by `$BINARY-TRAIT$_t<I, T>` as defined in [tuple]{.sref} or [variant]{.sref}.

```cpp
consteval size_t tuple_size(info type);
consteval info tuple_element(size_t index, info type);

consteval size_t variant_size(info type);
consteval info variant_alternative(size_t index, info type);
```
:::
:::

### [bit.cast]{.sref} Function template `bit_cast` {-}

And we have adjust the requirements of `bit_cast` to not allow casting to or from `meta::info`, in [bit.cast]{.sref}/3, which we add as a mandates:

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

[2]{.pnum} *Returns*: [...]

[3]{.pnum} [*Remarks*]{.rm} [*Constant When*]{.addu}: [This function is constexpr if and only if]{.rm} `To`, `From`, and the types of all subobjects of `To` and `From` are types `T` such that:

* [#.1]{.pnum} `is_union_v<T>` is `false`;
* [#.2]{.pnum} `is_pointer_v<T>` is `false`;
* [#.3]{.pnum} `is_member_pointer_v<T>` is `false`;
* [#.4]{.pnum} `is_volatile_v<T>` is `false`; and
* [#.5]{.pnum} `T` has no non-static data members of reference type.
:::

### [diff.cpp23]{.sref} Annex C (informative) Compatibility {-}

Add a new Annex C entry:

::: std
**Affected subclause**: [lex.operators]

**Change**: New operator `^^`.

**Rationale**: Required for new features.

**Effect on original feature**: Valid C++23 code that contains two consecutive `^` tokens may be ill-formed in this revision of C++.

::: example
```cpp
struct C { int operator^(int); };
int operator^(int (C::*p)(int), C);
int i = &C::operator^^C{}; // ill-formed; previously well-formed
```
:::
:::

Modify [diff.cpp23.library]:

::: std
**Affected subclause**: [headers]

**Change**: New headers.

**Rationale**: New functionality.

**Effect on original feature**: The folowing C++ headers are new: `<debugging>`, `<hazard_pointer>`, `<inplace_vector>`, `<linalg>`, [`<meta>`, ]{.addu} `<rcu>`, `<simd>`, and `<text_encoding>`. Valid C++ 2023 code that `#include`s headers with these names may be invalid in this revision of C++.

:::

## Feature-Test Macro

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

---
references:
  - id: P3289R1
    citation-label: P3289R1
    title: "Consteval blocks"
    author:
      - family: Daveed Vandevoorde, Wyatt Childers, Barry Revzin, Dan Katz
    issued:
      year: 2024
      month: 1
      day: 12
    URL: https://wg21.link/p3289r1
  - id: P3554R0
    citation-label: P3554R0
    title: "Non-transient allocation with std::vector and std::basic_string"
    author:
      - family: Barry Revzin, Peter Dimov
    issued:
      year: 2024
      month: 1
      day: 5
    URL: https://wg21.link/p3289r1
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
