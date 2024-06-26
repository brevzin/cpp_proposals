---
title: "Reflection for C++26"
document: P2996R5
date: today
audience: EWG, LEWG
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
tag: constexpr
---

# Revision History

Since [@P2996R4]:

* removed filters from query functions
* removed `test_trait`

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
  - a _reflection operator_ (prefix `^`) that produces a reflection value for its operand construct,
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
  return substitute(^__impl::replicator, args);
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
  template for (constexpr auto e : std::meta::enumerators_of(^E)) {
    if (value == [:e:]) {
      return std::string(std::meta::name_of(e));
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
  [:expand(std::meta::enumerators_of(^E)):] >> [&]<auto e>{
    if (value == [:e:]) {
      result = std::meta::name_of(e);
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

* expansion statements [@P1306R2]
* non-transient constexpr allocation [@P0784R7] [@P1974R0] [@P2670R1]

## Back-And-Forth

Our first example is not meant to be compelling but to show how to go back and forth between the reflection domain and the grammatical domain:

::: std
```c++
constexpr auto r = ^int;
typename[:r:] x = 42;       // Same as: int x = 42;
typename[:^char:] c = '*';  // Same as: char c = '*';
```
:::

The `typename` prefix can be omitted in the same contexts as with dependent qualified names (i.e., in what the standard calls _type-only contexts_).
For example:

::: std
```c++
using MyType = [:sizeof(int)<sizeof(long)? ^long : ^int:];  // Implicit "typename" prefix.
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/13anqE1Pa), [Clang](https://godbolt.org/z/zn4vnjqzb).


## Selecting Members

Our second example enables selecting a member "by number" for a specific type:

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_number(int n) {
  if (n == 0) return ^S::i;
  else if (n == 1) return ^S::j;
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (member_number(5) is not a constant).
}
```
:::

This example also illustrates that bit fields are not beyond the reach of this proposal.

On Compiler Explorer: [EDG](https://godbolt.org/z/WEYae451z), [Clang](https://godbolt.org/z/dYGaMKEx5).

Note that a "member access splice" like `s.[:member_number(1):]` is a more direct member access mechanism than the traditional syntax.
It doesn't involve member name lookup, access checking, or --- if the spliced reflection value denotes a member function --- overload resolution.

This proposal includes a number of consteval "metafunctions" that enable the introspection of various language constructs.
Among those metafunctions is `std::meta::nonstatic_data_members_of` which returns a vector of reflection values that describe the non-static members of a given type.
We could thus rewrite the above example as:

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_number(int n) {
  return std::meta::nonstatic_data_members_of(^S)[n];
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (member_number(5) is not a constant).
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/Wb1vx7jqb), [Clang](https://godbolt.org/z/TeGrhv7nz).

This proposal specifies that namespace `std::meta` is associated with the reflection type (`std::meta::info`); the `std::meta::` qualification can therefore be omitted in the example above.

Another frequently-useful metafunction is `std::meta::name_of`, which returns a `std::string_view` describing the unqualified name of an entity denoted by a given reflection value.
With such a facility, we could conceivably access non-static data members "by string":

::: std
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_named(std::string_view name) {
  for (std::meta::info field : nonstatic_data_members_of(^S)) {
    if (name_of(field) == name) return field;
  }
}

int main() {
  S s{0, 0};
  s.[:member_named("j"):] = 42;  // Same as: s.j = 42;
  s.[:member_named("x"):] = 0;   // Error (member_named("x") is not a constant).
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/Yhh5hbcrn), [Clang](https://godbolt.org/z/vM46x4abW).


## List of Types to List of Sizes

Here, `sizes` will be a `std::array<std::size_t, 3>` initialized with `{sizeof(int), sizeof(float), sizeof(double)}`:

::: std
```c++
constexpr std::array types = {^int, ^float, ^double};
constexpr std::array sizes = []{
  std::array<std::size_t, types.size()> r;
  std::views::transform(types, r.begin(), std::meta::size_of);
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

On Compiler Explorer: [EDG](https://godbolt.org/z/4xz9Wsa8f), [Clang](https://godbolt.org/z/EPY93bTxv).

## Implementing `make_integer_sequence`

We can provide a better implementation of `make_integer_sequence` than a hand-rolled approach using regular template metaprogramming (although standard libraries today rely on an intrinsic for this):

::: std
```c++
#include <utility>
#include <vector>

template<typename T>
consteval std::meta::info make_integer_seq_refl(T N) {
  std::vector args{^T};
  for (T k = 0; k < N; ++k) {
    args.push_back(std::meta::reflect_value(k));
  }
  return substitute(^std::integer_sequence, args);
}

template<typename T, T N>
  using make_integer_sequence = [:make_integer_seq_refl<T>(N):];
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/bvPeqvaK5), [Clang](https://godbolt.org/z/ae3n8Phnn).

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
  constexpr auto members = nonstatic_data_members_of(^S);
  std::array<member_descriptor, members.size()> layout;
  for (int i = 0; i < members.size(); ++i) {
      layout[i] = {.offset=offset_of(members[i]), .size=size_of(members[i])};
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

On Compiler Explorer: [EDG](https://godbolt.org/z/rbbWY99TM), [Clang](https://godbolt.org/z/YEn3ojjWq).

## Enum to String

One of the most commonly requested facilities is to convert an enum value to a string (this example relies on expansion statements):

::: std
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  template for (constexpr auto e : std::meta::enumerators_of(^E)) {
    if (value == [:e:]) {
      return std::string(std::meta::name_of(e));
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
  template for (constexpr auto e : std::meta::enumerators_of(^E)) {
    if (name == std::meta::name_of(e)) {
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
    return std::meta::enumerators_of(^E)
      | std::views::transform([](std::meta::info e){
          return std::pair<E, std::string>(std::meta::extract<E>(e), std::meta::name_of(e));
        })
  };

  constexpr auto get_name = [](E value) -> std::optional<std::string> {
    if constexpr (enumerators_of(^E).size() <= 7) {
      // if there aren't many enumerators, use a vector with find_if()
      constexpr auto enumerators = get_pairs() | std::ranges::to<std::vector>();
      auto it = std::ranges::find_if(enumerators, [value](auto const& pr){
        return pr.first == value;
      };
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

On Compiler Explorer: [EDG](https://godbolt.org/z/Y5va8MqzG), [Clang](https://godbolt.org/z/3doherKx8).


Many many variations of these functions are possible and beneficial depending on the needs of the client code.
For example:

  - the "\<unnamed>" case could instead output a valid cast expression like "E(5)"
  - a more sophisticated lookup algorithm could be selected at compile time depending on the length of `enumerators_of(^E)`
  - a compact two-way persistent data structure could be generated to support both `enum_to_string` and `string_to_enum` with a minimal footprint
  - etc.


## Parsing Command-Line Options

Our next example shows how a command-line option parser could work by automatically inferring flags based on member names. A real command-line parser would of course be more complex, this is just the beginning.

::: std
```c++
template<typename Opts>
auto parse_options(std::span<std::string_view const> args) -> Opts {
  Opts opts;
  template for (constexpr auto dm : nonstatic_data_members_of(^Opts)) {
    auto it = std::ranges::find_if(args,
      [](std::string_view arg){
        return arg.starts_with("--") && arg.substr(2) == name_of(dm);
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
      std::print(stderr, "Failed to parse option {} into a {}\n", *it, display_name_of(^T));
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

On Compiler Explorer: [EDG](https://godbolt.org/z/G4dh3jq8a), [Clang](https://godbolt.org/z/v1PvGnafx).


## A Simple Tuple Type

::: std
```c++
#include <meta>

template<typename... Ts> struct Tuple {
  struct storage;

  static_assert(is_type(define_class(^storage, {data_member_spec(^Ts)...})));
  storage data;

  Tuple(): data{} {}
  Tuple(Ts const& ...vs): data{ vs... } {}
};

template<typename... Ts>
  struct std::tuple_size<Tuple<Ts...>>: public integral_constant<size_t, sizeof...(Ts)> {};

template<std::size_t I, typename... Ts>
  struct std::tuple_element<I, Tuple<Ts...>> {
    static constexpr std::array types = {^Ts...};
    using type = [: types[I] :];
  };

consteval std::meta::info get_nth_field(std::meta::info r, std::size_t n) {
  return nonstatic_data_members_of(r)[n];
}

template<std::size_t I, typename... Ts>
  constexpr auto get(Tuple<Ts...> &t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>>& {
    return t.data.[:get_nth_field(^decltype(t.data), I):];
  }
// Similarly for other value categories...
```
:::

This example uses a "magic" `std::meta::define_class` template along with member reflection through the `nonstatic_data_members_of` metafunction to implement a `std::tuple`-like type without the usual complex and costly template metaprogramming tricks that that involves when these facilities are not available.
`define_class` takes a reflection for an incomplete class or union plus a vector of non-static data member descriptions, and completes the give class or union type to have the described members.

On Compiler Explorer: [EDG](https://godbolt.org/z/YK35d8MMx), [Clang](https://godbolt.org/z/cT116Wb31).

## A Simple Variant Type

Similarly to how we can implement a tuple using `define_class` to create on the fly a type with one member for each `Ts...`, we can implement a variant that simply defines a `union` instead of a `struct`.
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
However, for the purposes of `define_class`, there really is only one reasonable option to choose here:

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

If we make [`define_class`](#data_member_spec-define_class) for a `union` have this behavior, then we can implement a `variant` in a much more straightforward way than in current implementations.
This is not a complete implementation of `std::variant` (and cheats using libstdc++ internals, and also uses Boost.Mp11's `mp_with_index`) but should demonstrate the idea:

::: std
```cpp
template <typename... Ts>
class Variant {
    union Storage;
    struct Empty { };

    static_assert(is_type(define_class(^Storage, {
        data_member_spec(^Empty, {.name="empty"}),
        data_member_spec(^Ts)...
    })));

    static consteval std::meta::info get_nth_field(std::size_t n) {
        return nonstatic_data_members_of(^Storage)[n+1];
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

Arguably, the answer should be yes - this would be consistent with how other accesses work. This is instead proposed in [@P3293R0].

On Compiler Explorer: [EDG](https://godbolt.org/z/Efz5vsjaa), [Clang](https://godbolt.org/z/faEaq16Kh).

## Struct to Struct of Arrays

::: std
```c++
#include <meta>
#include <array>

template <typename T, std::size_t N>
struct struct_of_arrays_impl;

consteval auto make_struct_of_arrays(std::meta::info type,
                                     std::meta::info N) -> std::meta::info {
  std::vector<std::meta::info> old_members = nonstatic_data_members_of(type);
  std::vector<std::meta::info> new_members = {};
  for (std::meta::info member : old_members) {
    auto type_array = substitute(^std::array, {type_of(member), N });
    auto mem_descr = data_member_spec(type_array, {.name = name_of(member)});
    new_members.push_back(mem_descr);
  }
  return std::meta::define_class(
    substitute(^struct_of_arrays_impl, {type, N}),
    new_members);
}

template <typename T, size_t N>
using struct_of_arrays = [: make_struct_of_arrays(^T, ^N) :];
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

Again, the combination of `nonstatic_data_members_of` and `define_class` is put to good use.

On Compiler Explorer: [EDG](https://godbolt.org/z/Whdvs3j1n), [Clang](https://godbolt.org/z/senWPW3eY).


## Parsing Command-Line Options II

Now that we've seen a couple examples of using `std::meta::define_class` to create a type, we can create a more sophisticated command-line parser example.

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
    new_members.push_back(data_member_spec(type_new, {.name=name_of(member)}));
  }
  return define_class(opts, new_members);
}

struct Clap {
  template <typename Spec>
  auto parse(this Spec const& spec, int argc, char** argv) {
    std::vector<std::string_view> cmdline(argv+1, argv+argc)

    // check if cmdline contains --help, etc.

    struct Opts;
    static_assert(is_type(spec_to_opts(^Opts, ^Spec)));
    Opts opts;

    template for (constexpr auto [sm, om] : std::views::zip(nonstatic_data_members_of(^Spec),
                                                            nonstatic_data_members_of(^Opts))) {
      auto const& cur = spec.[:sm:];
      constexpr auto type = type_of(om);

      // find the argument associated with this option
      auto it = std::ranges::find_if(cmdline,
        [&](std::string_view arg){
          return (cur.use_short && arg.size() == 2 && arg[0] == '-' && arg[1] == name_of(sm)[0])
              || (cur.use_long && arg.starts_with("--") && arg.substr(2) == name_of(sm));
        });

      // no such argument
      if (it == cmdline.end()) {
        if constexpr (has_template_arguments(type) and template_of(type) == ^std::optional) {
          // the type is optional, so the argument is too
          continue;
        } else if (cur.initializer) {
          // the type isn't optional, but an initializer is provided, use that
          opts.[:om:] = *cur.initializer;
          continue;
        } else {
          std::print(stderr, "Missing required option {}\n", name_of(sm));
          std::exit(EXIT_FAILURE);
        }
      } else if (it + 1 == cmdline.end()) {
        std::print(stderr, "Option {} for {} is missing a value\n", *it, name_of(sm));
        std::exit(EXIT_FAILURE);
      }

      // found our argument, try to parse it
      auto iss = ispanstream(it[1]);
      if (iss >> opts.[:om:]; !iss) {
        std::print(stderr, "Failed to parse {:?} into option {} of type {}\n",
          it[1], name_of(sm), display_name_of(type));
        std::exit(EXIT_FAILURE);
      }
    }
    return opts;
  }
};
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/MWfqvMeTx), [Clang](https://godbolt.org/z/79MrYvPP3).

## A Universal Formatter

This example is taken from Boost.Describe:

::: std
```cpp
struct universal_formatter {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }

  template <typename T>
  auto format(T const& t, auto& ctx) const {
    auto out = std::format_to(ctx.out(), "{}@{@{", name_of(^T));

    auto delim = [first=true]() mutable {
      if (!first) {
        *out++ = ',';
        *out++ = ' ';
      }
      first = false;
    };

    template for (constexpr auto base : bases_of(^T)) {
      delim();
      out = std::format_to(out, "{}", (typename [: type_of(base) :] const&)(t));
    }

    template for (constexpr auto mem : nonstatic_data_members_of(^T)) {
      delim();
      out = std::format_to(out, ".{}={}", name_of(mem), t.[:mem:]);
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

On Compiler Explorer: [Clang](https://godbolt.org/z/r7f8h38fq).

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
    template for (constexpr auto mem : nonstatic_data_members_of(^T)) {
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
  constexpr auto members = nonstatic_data_members_of(^T);

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
  return substitute(^std::tuple,
                    nonstatic_data_members_of(type)
                    | std::views::transform(std::meta::type_of)
                    | std::views::transform(std::meta::type_remove_cvref)
                    | std::ranges::to<std::vector>());
}

template <typename To, typename From, std::meta::info ... members>
constexpr auto struct_to_tuple_helper(From const& from) -> To {
  return To(from.[:members:]...);
}

template<typename From>
consteval auto get_struct_to_tuple_helper() {
  using To = [: type_struct_to_tuple(^From): ];

  std::vector args = {^To, ^From};
  for (auto mem : nonstatic_data_members_of(^From)) {
    args.push_back(reflect_value(mem));
  }

  /*
  Alternatively, with Ranges:
  args.append_range(
    nonstatic_data_members_of(^From)
    | std::views::transform(std::meta::reflect_value)
    );
  */

  return extract<To(*)(From const&)>(
    substitute(^struct_to_tuple_helper, args));
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

On Compiler Explorer (with a different implementation than either of the above): [EDG](https://godbolt.org/z/Moqf84nc1), [Clang](https://godbolt.org/z/1s7aj5r69).

## Implementing `tuple_cat`

Courtesy of Tomasz Kaminski, [on compiler explorer](https://godbolt.org/z/EajGPdf9q):

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

    return subst_by_value(^Indexer, args);
}

template<typename... Tuples>
auto my_tuple_cat(Tuples&&... tuples) {
    constexpr typename [: make_indexer({type_tuple_size(type_remove_cvref(^Tuples))...}) :] indexer;
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
make_named_tuple<^int, std::meta::reflect_value("x"),
                 ^double, std::meta::reflect_value("y")>()
```

We do not currently support splicing string literals, and the `pair` approach follows the similar pattern already shown with `define_class` (given a suitable `fixed_string` type):

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
            dealias(^typename Tag::type),
            {.name=Tag::name()}));

    };
    (f(tags), ...);
    return define_class(type, nsdms);
}

struct R;
static_assert(is_type(make_named_tuple(^R, pair<int, "x">{}, pair<double, "y">{})));

static_assert(type_of(nonstatic_data_members_of(^R)[0]) == ^int);
static_assert(type_of(nonstatic_data_members_of(^R)[1]) == ^double);

int main() {
    [[maybe_unused]] auto r = R{.x=1, .y=2.0};
}
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/nMx4M9sdT), [Clang](https://godbolt.org/z/TK71ThhM5).

Alternatively, can side-step the question of non-type template parameters entirely by keeping everything in the value domain:

::: std
```cpp
consteval auto make_named_tuple(std::meta::info type,
                                std::initializer_list<std::pair<std::meta::info, std::string_view>> members) {
    std::vector<std::meta::data_member_spec> nsdms;
    for (auto [type, name] : members) {
        nsdms.push_back(data_member_spec(type, {.name=name}));
    }
    return define_class(type, nsdms);
}

struct R;
static_assert(is_type(make_named_tuple(^R, {{^int, "x"}, {^double, "y"}})));

static_assert(type_of(nonstatic_data_members_of(^R)[0]) == ^int);
static_assert(type_of(nonstatic_data_members_of(^R)[1]) == ^double);

int main() {
    [[maybe_unused]] auto r = R{.x=1, .y=2.0};
}
```
:::

On Compiler Explorer: [EDG and Clang](https://godbolt.org/z/dPcsaTEv6) (the EDG and Clang implementations differ only in Clang having the updated `data_member_spec` API that returns an `info`).


## Compile-Time Ticket Counter

The features proposed here make it a little easier to update a ticket counter at compile time.
This is not an ideal implementation (we'd prefer direct support for compile-time —-- i.e., `consteval` --- variables), but it shows how compile-time mutable state surfaces in new ways.

::: std
```cpp
class TU_Ticket {
  template<int N> struct Helper;
public:
  static consteval int next() {
    int k = 0;

    // Search for the next incomplete 'Helper<k>'.
    std::meta::info r;
    while (!is_incomplete_type(r = substitute(^Helper,
                                             { std::meta::reflect_value(k) })))
      ++k;

    // Define 'Helper<k>' and return its index.
    define_class(r, {});
    return k;
  }
};

constexpr int x = TU_Ticket::next();
static_assert(x == 0);

constexpr int y = TU_Ticket::next();
static_assert(y == 1);

constexpr int z = TU_Ticket::next();
static_assert(z == 2);
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/MEYd3771Y), [Clang](https://godbolt.org/z/K4KWEqevv).

## Emulating typeful reflection
Although we believe a single opaque `std::meta::info` type to be the best and most scalable foundation for reflection, we acknowledge the desire expressed by SG7 for future support for "typeful reflection". The following demonstrates one possible means of assembling a typeful reflection library, in which different classes of reflections are represented by distinct types, on top of the facilities proposed here.

::: std
```cpp
// Represents a 'std::meta::info' constrained by a predicate.
template <std::meta::info Pred>
  requires (extract<bool>(substitute(^std::predicate, {type_of(Pred), ^std::meta::info})))
struct metatype {
  std::meta::info value;

  // Construction is ill-formed unless predicate is satisfied.
  consteval metatype(std::meta::info r) : value(r) {
    if (![:Pred:](r))
      throw "Reflection is not a member of this metatype";
  }

  // Cast to 'std::meta::info' allows values of this type to be spliced.
  consteval operator std::meta::info() const { return value; }

  static consteval bool check(std::meta::info r) { return [:Pred:](r); }
};

// Type representing a "failure to match" any known metatypes.
struct unmatched {
  consteval unmatched(std::meta::info) {}
  static consteval bool check(std::meta::info) { return true; }
};

// Returns the given reflection "enriched" with a more descriptive type.
template <typename... Choices>
consteval std::meta::info enrich(std::meta::info r) {
  // Because we control the type, we know that the constructor taking info is
  // the first constructor. The copy/move constructors are added at the }, so
  // will be the last ones in the list.
  std::array ctors = {members_of(^Choices, std::meta::is_constructor)[0]...,
                      members_of(^unmatched, std::meta::is_constructor)[0]};
  std::array checks = {^Choices::check..., ^unmatched::check};

  for (auto [check, ctor] : std::views::zip(checks, ctors))
    if (extract<bool>(reflect_invoke(check, {reflect_value(r)})))
      return reflect_invoke(ctor, {reflect_value(r)});

  std::unreachable();
}
```
:::

We can leverage this machinery to select different function overloads based on the "type" of reflection provided as an argument.

::: std
```cpp
using type_t = metatype<^std::meta::is_type>;
using template_t = metatype<^std::meta::is_template>;

// Example of a function overloaded for different "types" of reflections.
void PrintKind(type_t) { std::println("type"); }
void PrintKind(template_t) { std::println("template"); }
void PrintKind(unmatched) { std::println("unknown kind"); }

int main() {
  // Classifies any reflection as one of: Type, Function, or Unmatched.
  auto enrich = [](std::meta::info r) { return ::enrich<type_t,
                                                        template_t>(r); };

  // Demonstration of using 'enrich' to select an overload.
  PrintKind([:enrich(^metatype):]);                   // "template"
  PrintKind([:enrich(^type_t):]);                     // "type"
  PrintKind([:enrich(std::meta::reflect_value(3):]);  // "unknown kind"
}
```
:::

Note that the `metatype` class can be generalized to wrap values of any literal type, or to wrap multiple values of possibly different types. This has been used, for instance, to select compile-time overloads based on: whether two integers share the same parity, the presence or absence of a value in an `optional`, the type of the value held by a `variant` or an `any`, or the syntactic form of a compile-time string.

Achieving the same in C++23, with the same generality, would require spelling the argument(s) twice: first to obtain a "classification tag" to use as a template argument, and again to call the function, i.e.,

::: std
```cpp
Printer::PrintKind<classify(^int)>(^int).
// or worse...
fn<classify(Arg1, Arg2, Arg3)>(Arg1, Arg2, Arg3).
```
:::

On Compiler Explorer: [Clang](https://godbolt.org/z/q88dWYq8v).

# Proposed Features

## The Reflection Operator (`^`)

The reflection operator produces a reflection value from a grammatical construct (its operand):

> | _unary-expression_:
> |       ...
> |       `^` `::`
> |       `^` _namespace-name_
> |       `^` _type-id_
> |       `^` _id-expression_

The expression `^::` evaluates to a reflection of the global namespace. When the operand is a _namespace-name_ or _type-id_, the resulting value is a reflection of the designated namespace or type.

When the operand is an _id-expression_, the resulting value is a reflection of the designated entity found by lookup. This might be any of:

- a variable, static data member, or structured binding
- a function or member function
- a non-static data member
- a template or member template
- an enumerator

For all other operands, the expression is ill-formed. In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to not evaluate to constant.

Earlier revisions of this paper allowed for taking the reflection of any _cast-expression_ that could be evaluated as a constant expression, as we believed that a constant expression could be internally "represented" by just capturing the value to which it evaluated. However, the possibility of side effects from constant evaluation (introduced by this very paper) renders this approach infeasible: even a constant expression would have to be evaluated every time it's spliced. It was ultimately decided to defer all support for expression reflection, but we intend to introduce it through a future paper using the syntax `^(expr)`.

This paper does, however, support reflections of _values_ and of _objects_ (including subobjects). Such reflections arise naturally when iterating over template arguments.

```cpp
template <int P1, const int &P2> void fn() {}

static constexpr int p[2] = {1, 2};
constexpr auto spec = ^fn<p[0], p[1]>;

static_assert(is_value(template_arguments_of(spec)[0]));
static_assert(is_object(template_arguments_of(spec)[1]));
static_assert(!is_variable(template_arguments_of(spec)[1]));

static_assert([:template_arguments_of(spec)[0]:] == 1);
static_assert(&[:template_arguments_of(spec)[1]:] == &p[1]);
```

Such reflections cannot generally be obtained using the `^`-operator, but the `std::meta::reflect_value` and `std::meta::reflect_object` functions make it easy to reflect particular values or objects. The `std::meta::value_of` metafunction can also be used to map a reflection of an object to a reflection of its value.

### Syntax discussion

The original TS landed on `@[reflexpr]{.cf}@(...)` as the syntax to reflect source constructs and [@P1240R0] adopted that syntax as well.
As more examples were discussed, it became clear that that syntax was both (a) too "heavy" and (b) insufficiently distinct from a function call.
SG7 eventually agreed upon the prefix `^` operator. The "upward arrow" interpretation of the caret matches the "lift" or "raise" verbs that are sometimes used to describe the reflection operation in other contexts.

The caret already has a meaning as a binary operator in C++ ("exclusive OR"), but that is clearly not conflicting with a prefix operator.
In C++/CLI (a Microsoft C++ dialect) the caret is also used as a new kind of `$ptr-operator$` ([dcl.decl.general]{.sref}) to declare ["handles"](https://learn.microsoft.com/en-us/cpp/extensions/handle-to-object-operator-hat-cpp-component-extensions?view=msvc-170).
That is also not conflicting with the use of the caret as a unary operator because C++/CLI uses the usual prefix `*` operator to dereference handles.

Apple also uses the caret in [syntax "blocks"](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/WorkingwithBlocks/WorkingwithBlocks.html) and unfortunately we believe that does conflict with our proposed use of the caret.

Since the syntax discussions in SG7 landed on the use of the caret, new basic source characters have become available: `@`, `` ` ``{.op}, and `$`{.op}. While we have since discussed some alternatives (e.g., `@` for lifting, `\` and `/` for "raising" and "lowering"), we have grown quite fond of the existing syntax.


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
typename[: ^:: :] x = 0;  // Error.
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

When `r` is a reflection of a function or function template that is part of an overload set, overload resolution will not consider the whole overload set, just the specific function or function template that `r` reflects:

::: std
```cpp
struct C {
    template <class T> void f(T); // #1
    void f(int); // #2
};

void (C::*p1)(int) = &C::f;  // error: ambiguous

constexpr auto f1 = members_of(^C, /* function templates named f */)[0];
constexpr auto f2 = members_of(^C, /* functions named f */)[0];
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
constexpr std::meta::info NS_A = ^A;

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

What kind of parameter is `S`? If `R` reflects a class template, then it is a non-type template parameter of deduced type, but if `R` reflects a concept, it is a type template parameter. There is no other circumstance in the language for which it is not possible to decide at parse time whether a template parameter is a type or a non-type, and we don't wish to introduce one for this use case.

The most obvious solution would be to introduce a `concept [:R:]` syntax that requires that `R` reflect a concept, and while this could be added going forward, we weren't convinced of its value at this time - especially since the above can easily be rewritten:

```cpp
template <std::meta::info R> struct Outer {
  template <typename T> requires template [:R:]<T> { /* ... */ };
};
```

We are resolving this ambiguity by simply disallowing a reflection of a concept, whether dependent or otherwise, from being spliced in the declaration of a template parameter (thus in the above example, the parser can assume that `S` is a non-type parameter).

#### Splicing class members as designators in designated-initializer-lists

```cpp
struct S { int a; };

constexpr S s = {.[:^S::a:] = 2};
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
  constexpr auto members = nonstatic_data_members_of(^T);

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
  constexpr auto members = nonstatic_data_members_of(^T);
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
  constexpr auto members = nonstatic_data_members_of(^T);
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
    using info = decltype(^::);
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

Values of structural types can already be used as template arguments (so implementations must already know how to mangle them), and the notion of _template-argument-equivalent_ values defined on the class of structural types helps guarantee that `&fn<^value1> == &fn<^value2>` if and only if `&fn<value1> == &fn<value2>`.

Notably absent at this time are reflections of expressions. For example, one might wish to walk over the subexpressions of a function call:

::: std
```c++
template <typename T> void fn(T) {}

void g() {
  constexpr auto call = ^(fn(42));
  static_assert(
      template_arguments_of(function_of(call))[0] ==
      ^int);
}
```
:::

Previous revisions of this proposal suggested limited support for reflections of constant expressions. The introduction of side effects from constant evaluations (by this very paper), however, renders this roughly as difficult for constant expressions as it is for non-constant expressions. We instead defer all expression reflection to a future paper, and only present value and object reflection in the present proposal.

### Comparing reflections

The type `std::meta::info` is a _scalar_ type for which equality and inequality are meaningful, but for which no ordering relation is defined.

::: std
```c++
static_assert(^int == ^int);
static_assert(^int != ^const int);
static_assert(^int != ^int &);

using Alias = int;
static_assert(^int != ^Alias);
static_assert(^int == dealias(^Alias));

namespace AliasNS = ::std;
static_assert(^::std != ^AliasNS);
static_assert(^:: == parent_of(^::std));
```
:::

When the `^` operator is followed by an _id-expression_, the resulting `std::meta::info` reflects the entity named by the expression. Such reflections are equivalent only if they reflect the same entity.

::: std
```c++
int x;
struct S { static int y; };
static_assert(^x == ^x);
static_assert(^x != ^S::y);
static_assert(^S::y == static_data_members_of(^S)[0]);
```
:::

Special rules apply when comparing certain kinds of reflections. A reflection of an alias compares equal to another reflection if and only if they are both aliases, alias the same type, and share the same name and scope. In particular, these rules allow e.g., `fn<^std::string>` to refer to the same instantiation across translation units.

::: std
```c++
using Alias1 = int;
using Alias2 = int;
consteval std::meta::info fn() {
  using Alias1 = int;
  return ^Alias;
}
static_assert(^Alias1 == ^Alias1);
static_assert(^Alias1 != ^int);
static_assert(^Alias1 != ^Alias2);
static_assert(^Alias1 != fn());
}
```
:::

A reflection of an object (including variables) does not compare equally to a reflection of its value. Two values of different types never compare equally.

::: std
```c++
constexpr int i = 42, j = 42;

constexpr std::meta::info r = ^i, s = ^i;
static_assert(r == r && r == s);

static_assert(^i != ^j);  // 'i' and 'j' are different entities.
static_assert(value_of(^i) == value_of(^j));  // Two equivalent values.
static_assert(^i != std::meta::reflect_object(i))  // A variable is distinct from the
                                                   // object it designates.
static_assert(^i != std::meta::reflect_value(42));  // A reflection of an object
                                                    // is not the same as its value.
```
:::

### Linkage of reflections and templates specialized by reflections

Nontype template arguments of type `std::meta::info` are permitted (and frequently useful!), but since reflections represent internal compiler state while processing a single translation unit, they cannot be allowed to leak across TUs. Therefore both variables of _consteval-only type_, and entities specialized by a non-type template argument of _consteval-only type_, cannot have module or external linkage (i.e., they must have either internal or no linkage). While this can lead to some code bloat, we aren't aware of any organic use cases for reflection that are harmed by this limitation.

A corollary of this rule is that static data members of a class cannot have consteval-only types - such members always have external linkage, and to do otherwise would be an ODR violation. Again, we aren't aware of any affected use-cases that absolutely require this.

::: std
```c++
template<auto R> struct S {};
int x;
auto fn() { int k; return ^k; }

static auto r = ^int;  // r has internal name linkage.
S<^x> sx;  // S<^x> has internal name linkage.
S<fn()> sy;  // S<^y> has internal name linkage.
```
:::

### The associated `std::meta` namespace

The namespace `std::meta` is an associated type of `std::meta::info`, which allows standard library meta functions to be invoked without explicit qualification. For example:

::: std
```c++
#include <meta>
struct S {};
std::string name2 = std::meta::name_of(^S);  // Okay.
std::string name1 = name_of(^S);             // Also okay.
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
static_assert(std::meta::info() != ^S);
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
For example, we provide a `define_class` metafunction that provides a definition for a given class.
Clearly, we want the effect of calling that metafunction to be "prompt" in a lexical-order sense.
For example:

::: std
```c++
#include <meta>
struct S;

void g() {
  static_assert(is_type(define_class(^S, {})));
  S s;  // S should be defined at this point.
}
```
:::


Hence this proposal also introduces constraints on constant evaluation as follows...

First, we identify a subset of manifestly constant-evaluated expressions and conversions characterized by the fact that their evaluation must occur and must succeed in a valid C++ program: We call these _plainly constant-evaluated_.
We require that a programmer can count on those evaluations occurring exactly once and completing at translation time.

Second, we sequence plainly constant-evaluated expressions and conversions within the lexical order.
Specifically, we require that the evaluation of a plainly constant-evaluated expression or conversion occurs before the implementation checks the validity of source constructs lexically following that expression or conversion.

Those constraints are mostly intuitive, but they are a significant change to the underlying principles of the current standard in this respect.

[@P2758R1] ("Emitting messages at compile time") also has to deal with side effects during constant evaluation.
However, those effects ("output") are of a slightly different nature in the sense that they can be buffered until a manifestly constant-evaluated expression/conversion has completed.
"Buffering" a class type completion is not practical (e.g., because other metafunctions may well depend on the completed class type).
Still, we are not aware of incompatibilities between our proposal and [@P2758R1].


### Error-Handling in Reflection

Earlier revisions of this proposal suggested several possible approaches to handling errors in reflection metafunctions. This question arises naturally when considering, for instance, examples like `template_of(^int)`: the argument is a reflection of a type, but that type is not a specialization of a template, so there is no valid reflected template for us to return.

Some of the possibilities that we have considered include:

1. Returning an invalid reflection (similar to `NaN` for floating point) which carries source location info and some useful message (i.e., the approach suggested by P1240)
2. Returning a `std::expected<std::meta::info, E>` for some reflection-specific error type `E`, which carries source location info and some useful message
3. Failing to be a constant expression
4. Throwing an exception of type `E`, which requires a language extension for such exceptions to be catchable during `constexpr` evaluation

We found that we disliked (1) since there is no satisfying value that can be returned for a call like `template_arguments_of(^int)`: We could return a `std::vector<std::meta::info>` having a single invalid reflection, but this makes for awkward error handling. The experience offered by (3) is at least consistent, but provides no immediate means for a user to "recover" from an error.

Either `std::expected` or constexpr exceptions would allow for a consistent and straightforward interface. Deciding between the two, we noticed that many of usual concerns about exceptions do not apply during translation:

* concerns about runtime performance, object file size, etc. do not exist, and
* concerns about code evolving to add new uncaught exception types do not apply

An interesting example illustrates one reason for our preference for exceptions over `std::expected`:

::: std
```cpp
template <typename T>
  requires (template_of(^T) == ^std::optional)
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

* `template_arguments_of(^std::tuple<int>)` is `{^int}`
* `substitute(^std::tuple, {^int})` is `^std::tuple<int>`

This requires us to answer the question: how do we accept a range parameter and how do we provide a range return.

For return, we intend on returning `std::vector<std::meta::info>` from all such APIs. This is by far the easiest for users to deal with. We definitely don't want to return a `std::span<std::meta::info const>`, since this requires keeping all the information in the compiler memory forever (unlike `std::vector` which could free its allocation). The only other option would be a custom container type which is optimized for compile-time by being able to produce elements lazily on demand - i.e. so that `nonstatic_data_members_of(^T)[3]` wouldn't have to populate _all_ the data members, just do enough work to be able to return the 4th one. But that adds a lot of complexity that's probably not worth the effort.

For parameters, there are basically three options:

1. Accept `std::span<std::meta::info const>`, which now accepts braced-init-list arguments so it's pretty convenient in this regard.
2. Accept `std::vector<std::meta::info>`
3. Accept _any_ range whose `type_value` is `std::meta::info`.

Now, for compiler efficiency reasons, it's definitely better to have all the arguments contiguously. So the compiler wants `span`. There's really no reason to prefer `vector` over `span`. Accepting any range would look something like this:

::: std
```cpp
namespace std::meta {
    template <typename R>
    concept reflection_range = ranges::input_range<R>
                            && same_as<ranges::range_value_t<R>, info>;

    template <reflection_range R = span<info const>>
    consteval auto substitute(info tmpl, R&& args) -> info;
}
```
:::

This API is more user friendly than accepting `span<info const>` by virtue of simply accepting more kinds of ranges. The default template argument allows for braced-init-lists to still work. [Example](https://godbolt.org/z/vnzWv6vG3).

Specifically, if the user is doing anything with range adaptors, they will either end up with a non-contiguous or non-sized range, which will no longer be convertible to `span` - so they will have to manually convert their range to a `vector<info>` in order to pass it to the algorithm. Because the implementation wants contiguity anyway, that conversion to `vector` will happen either way - so it's just a matter of whether every call needs to do it manually or the implementation can just do it once.

For example, converting a struct to a tuple type:

::: cmptable
### `span` only
```cpp
consteval auto type_struct_to_tuple(info type) -> meta::info {
    return substitute(
        ^tuple,
        nonstatic_data_members_of(type)
        | views::transform(meta::type_of)
        | views::transform(meta::type_remove_cvref)
        | ranges::to<vector>());
}
```

### any range
```cpp
consteval auto type_struct_to_tuple(info type) -> meta::info {
    return substitute(
        ^tuple,
        nonstatic_data_members_of(type)
        | views::transform(meta::type_of)
        | views::transform(meta::type_remove_cvref)
        );
}
```
:::

This shouldn't cause much compilation overhead. Checking convertibility to `span` _already_ uses Ranges machinery. And implementations can just do the right thing interally:

::: std
```cpp
consteval auto __builtin_substitute(info tmpl, info const* arg, size_t num_args) -> info;

template <reflection_range R = span<info const>>
consteval auto substitute(info tmpl, R&& args) -> info {
    if constexpr (ranges::sized_range<R> && ranges::contiguous_range<R>) {
        auto as_span = span<info const>(args);
        return __builtin_substitute(tmpl, as_span.data(), as_span.size());
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
`^A` yields a reflection of an alias to `int`, while `^int` yields a reflection of `int`.
`^A == ^int` evaluates to `false`, but there will be a way to strip aliases - so `dealias(^A) == ^int` evaluates to `true`.

This opens up the question of how various other metafunctions handle aliases and it is worth going over a few examples:

::: std
```cpp
using A = int;
using B = std::unique_ptr<int>;
template <class T> using C = std::unique_ptr<T>;
```
:::

This paper is proposing that:

* `is_type(^A)` is `true`.
   `^A` is an alias, but it's an alias to a type, and if this evaluated as `false` then everyone would have to `dealias` everything all the time.
* `has_template_arguments(^B)` is `false` while `has_template_arguments(^C<int>)` is `true`.
  Even though `B` is an alias to a type that itself has template arguments (`unique_ptr<int>`), `B` itself is simply a type alias and does not.
  This reflects the actual usage.
* Meanwhile, `template_arguments_of(^C<int>)` yields `{^int}` while `template_arguments_of(^std::unique_ptr<int>)` yields `{^int, ^std::default_deleter<int>}`.
  This is because `C` has its own template arguments that can be reflected on.


### Reflecting source text

One of the most "obvious" abilities of reflection --- retrieving the name of an entity --- turns out to raise
issues that aren't obvious at all: How do we represent source text in a C++ program.

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

In practice ordinary strings encoded in the "ordinary string literal encoding" (which may or may not be UTF-8)
are often used.  We therefore need mechanisms to produce the corresponding ordinary string types as well.

Orthogonal to the character representation is the data structure used to traffic in source text.  An
implementation can easily have at least three potential representations of reflected source text:

  a) the internal representation used, e.g., in the compiler front end's AST-like structures (persistent)

  b) the representation of string literals in the AST (persistent)

  c) the representation of array of character values during constant-evaluation (transient)

(some compilers might share some of those representations).  For transient text during constant evaluation we'd
like to use `string`/`u8string` values, but because of the limitations on non-transient allocation during
constant evaluation we cannot easily transfer such types to the non-constant (i.e., run-time) environment.
E.g., if `name_of` were a (consteval) metafunction returning a `std::string` value, the following simple
example would not work:

::: std
```cpp
#include <iostream>
#include <meta>
int main() {
  int hello_world = 42;
  std::cout << name_of(^hello_world) << "\n";  // Doesn't work if name_of produces a std::string.
}
```
:::

We can instead return a `std::string_view` or `std::u8string_view`, but that has the downside
that it effectively makes all results of querying source text persistent for the compilation.

For now, however, we propose that queries like `name_of` do produce "string view" results.
For example:

::: std
```cpp
consteval std::string_view name_of(info);
consteval std::u8string_view name_of(info);
```
:::


An alternative strategy that we considered is the introduction of a "proxy type" for source text:

::: std
```cpp
namespace std::meta {
  struct source_text_info {
    ...
    template<typename T>
      requires (^T == dealias(^std::string_view) || ^T == dealias(^std::u8string_view) ||
                ^T == dealias(^std::string) || ^T == dealias(^std::u8string))
      consteval T as();
    ...
  };
}
```
:::

where the `as<...>()` member function produces a string-like type as desired.  That idea was dropped,
however, because it became unwieldy in actual use cases.

With a source text query like `name_of(refl)` it is possible that the some source
characters of the result are not representable.  We can then consider multiple options, including:

  1) the query fails to evaluate,

  2) any unrepresentable source characters are translated to a different presentation,
     such as universal-character-names of the form `\u{ $hex-number$ }`,

  3) any source characters not in the basic source character set are translated to a different
     presentation (as in (2)).

Following much discussion with SG16, we propose #1: The query fails to evaluate if the identifier cannot be represented in the ordinary string literal encoding.


### Freestanding implementations

Several important metafunctions, such as `std::meta::nonstatic_data_members_of`, return a `std::vector` value.
Unfortunately, that means that they are currently not usable in a freestanding environment, but [@P3295R0] currently proposes freestanding `std::vector`, `std::string`, and `std::allocator` in constant evaluated contexts, explicitly to make the facilities proposed by this paper work in freestanding.


### Synopsis

Here is a synopsis for the proposed library API. The functions will be explained below.

::: std
```c++
namespace std::meta {
  using info = decltype(^::);

  template <typename R>
  concept reflection_range = /* @*see [above](#range-based-metafunctions)*@ */;

  // @[name and location](#name-loc)@
  consteval auto name_of(info r) -> string_view;
  consteval auto qualified_name_of(info r) -> string_view;
  consteval auto display_name_of(info r) -> string_view;

  consteval auto u8name_of(info r) -> u8string_view;
  consteval auto u8display_name_of(info r) -> u8string_view;
  consteval auto u8qualified_name_of(info r) -> u8string_view;

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
  consteval auto members_of(info type_class) -> vector<info>;
  consteval auto bases_of(info type_class) -> vector<info>;
  consteval auto static_data_members_of(info type_class) -> vector<info>;
  consteval auto nonstatic_data_members_of(info type_class) -> vector<info>;
  consteval auto subobjects_of(info type_class) -> vector<info>;
  consteval auto enumerators_of(info type_enum) -> vector<info>;

  // @[member access](#member-access)@
  consteval auto access_context() -> info;

  struct access_pair {
    consteval access_pair(info target, info from = access_context());
  };

  consteval auto is_accessible(access_pair p) -> bool;
  consteval auto is_accessible(info r, info from);

  consteval auto accessible_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_members_of(info target, info from) -> vector<info>;

  consteval auto accessible_bases_of(access_pair p) -> vector<info>;
  consteval auto accessible_bases_of(info target, info from) -> vector<info>;

  consteval auto accessible_nonstatic_data_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_nonstatic_data_members_of(info target,
                                                      info from) -> vector<info>;
  consteval auto accessible_static_data_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_static_data_members_of(info target,
                                                   info from) -> vector<info>;
  consteval auto accessible_subobjects_of(access_pair p) -> vector<info>;
  consteval auto accessible_subobjects_of(info target,
                                         info from) -> vector<info>;

  // @[substitute](#substitute)@
  template <reflection_range R = span<info const>>
  consteval auto can_substitute(info templ, R&& args) -> bool;
  template <reflection_range R = span<info const>>
  consteval auto substitute(info templ, R&& args) -> info;

  // @[reflect_invoke](#reflect_invoke)@
  template <reflection_range R = span<info const>>
  consteval auto reflect_invoke(info target, R&& args) -> info;
  template <reflection_range R1 = span<info const>, reflection_range R2 = span<info const>>
  consteval auto reflect_invoke(info target, R1&& tmpl_args, R2&& args) -> info;

  // @[reflect expression results](#reflect-expression-results)@
  template <typename T>
    consteval auto reflect_value(T value) -> info;
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
  consteval auto is_pure_virtual(info entity) -> bool;
  consteval auto is_override(info entity) -> bool;
  consteval auto is_deleted(info entity) -> bool;
  consteval auto is_defaulted(info entity) -> bool;
  consteval auto is_explicit(info entity) -> bool;
  consteval auto is_noexcept(info entity) -> bool;
  consteval auto is_bit_field(info entity) -> bool;
  consteval auto is_const(info r) -> bool;
  consteval auto is_volatile(info r) -> bool;
  consteval auto is_final(info r) -> bool;
  consteval auto has_static_storage_duration(info r) -> bool;
  consteval auto has_internal_linkage(info r) -> bool;
  consteval auto has_module_linkage(info r) -> bool;
  consteval auto has_external_linkage(info r) -> bool;
  consteval auto has_linkage(info r) -> bool;
  consteval auto is_class_member(info entity) -> bool;
  consteval auto is_namespace_member(info entity) -> bool;
  consteval auto is_nonstatic_data_member(info entity) -> bool;
  consteval auto is_static_member(info entity) -> bool;
  consteval auto is_base(info entity) -> bool;
  consteval auto is_namespace(info entity) -> bool;
  consteval auto is_function(info entity) -> bool;
  consteval auto is_variable(info entity) -> bool;
  consteval auto is_type(info entity) -> bool;
  consteval auto is_alias(info entity) -> bool;
  consteval auto is_incomplete_type(info entity) -> bool;
  consteval auto is_template(info entity) -> bool;
  consteval auto is_function_template(info entity) -> bool;
  consteval auto is_variable_template(info entity) -> bool;
  consteval auto is_class_template(info entity) -> bool;
  consteval auto is_alias_template(info entity) -> bool;
  consteval auto is_concept(info entity) -> bool;
  consteval auto is_structured_binding(info entity) -> bool;
  consteval auto is_value(info entity) -> bool;
  consteval auto is_object(info entity) -> bool;
  consteval auto has_template_arguments(info r) -> bool;
  consteval auto is_constructor(info r) -> bool;
  consteval auto is_destructor(info r) -> bool;
  consteval auto is_special_member(info r) -> bool;
  consteval auto is_user_provided(info r) -> bool;

  // @[define_class](#data_member_spec-define_class)@
  struct data_member_options_t;
  consteval auto data_member_spec(info type_class,
                                  data_member_options_t options = {}) -> info;
  template <reflection_range R = span<info const>>
  consteval auto define_class(info type_class, R&&) -> info;

  // @[data layout](#data-layout-reflection)@
  consteval auto offset_of(info entity) -> size_t;
  consteval auto size_of(info entity) -> size_t;
  consteval auto alignment_of(info entity) -> size_t;
  consteval auto bit_offset_of(info entity) -> size_t;
  consteval auto bit_size_of(info entity) -> size_t;

}
```
:::

### `name_of`, `display_name_of`, `source_location_of` {#name-loc}

::: std
```c++
namespace std::meta {
  consteval auto name_of(info) -> string_view;
  consteval auto qualified_name_of(info) -> string_view;
  consteval auto display_name_of(info) -> string_view;

  consteval auto u8name_of(info) -> u8string_view;
  consteval auto u8qualified_name_of(info) -> u8string_view;
  consteval auto u8display_name_of(info) -> u8string_view;

  consteval auto source_location_of(info r) -> source_location;
}
```
:::

If a `string_view` is returned, its contents consist of characters representable by the ordinary string literal encoding only; if any character cannot be represented, it is not a constant expression.

Given a reflection `r` that designates a declared entity `X`, `name_of(r)` and `qualified_name_of(r)` return a `string_view` holding the unqualified and qualified name of `X`, respectively. `u8name_of(r)` and `qualified_name_of(r)` return the same, respectively, as a `u8string_view`.
For all other reflections, an empty string view is produced.
For template instances, the name does not include the template argument list.

Given a reflection `r`, `display_name_of(r)` and `u8display_name_of(r)` return an unspecified non-empty `string_view` and `u8string_view`, respectively.
Implementations are encouraged to produce text that is helpful in identifying the reflected construct.

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
  return type_remove_cvref(is_type(r) ? r : type_of(r));
}

#define typeof(e) [: type_doof(^e) :]
```
:::

If `r` designates a member of a class or namespace, `parent_of(r)` is a reflection designating its immediately enclosing class or (possibly inline or anonymous) namespace.

If `r` designates an alias, `dealias(r)` designates the underlying entity.
Otherwise, `dealias(r)` produces `r`.
`dealias` is recursive - it strips all aliases:

::: std
```cpp
using X = int;
using Y = X;
static_assert(dealias(^int) == ^int);
static_assert(dealias(^X) == ^int);
static_assert(dealias(^Y) == ^int);
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

static_assert(^x != ^y);
static_assert(object_of(^x) == object_of(^y));
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
static_assert(template_of(type_of(^v)) == ^std::vector);
static_assert(template_arguments_of(type_of(^v))[0] == ^int);
```
:::



### `members_of`, `static_data_members_of`, `nonstatic_data_members_of`, `bases_of`, `enumerators_of`, `subobjects_of` {#member-queries}

::: std
```c++
namespace std::meta {
  consteval auto members_of(info type_class) -> vector<info>;
  consteval auto bases_of(info type_class) -> vector<info>;

  consteval auto static_data_members_of(info type_class) -> vector<info>;
  consteval auto nonstatic_data_members_of(info type_class) -> vector<info>;

  consteval auto subobjects_of(info type_class) -> vector<info> {
    auto subobjects = bases_of(type_class);
    subobjects.append_range(nonstatic_data_members_of(type_class));
    return subobjects;
  }

  consteval auto enumerators_of(info type_enum) -> vector<info>;
}
```
:::

The template `members_of` returns a vector of reflections representing the direct members of the class type represented by its first argument.
Any non-static data members appear in declaration order within that vector.
Anonymous unions appear as a non-static data member of corresponding union type.
Reflections of structured bindings shall not appear in the returned vector.

The template `bases_of` returns the direct base classes of the class type represented by its first argument, in declaration order.

`static_data_members_of` and `nonstatic_data_members_of` return reflections of the static and non-static data members, in order, respectively. 

`subobjects_of` returns the base class subobjects and the non-static data members of a type, in declaration order. Note that the term [subobject](https://eel.is/c++draft/intro.object#def:subobject) also includes _array elements_, which we are excluding here. Such reflections would currently be of minimal use since you could not splice them with access (e.g. `arr.[:elem:]` is not supported), so would need some more thought first.

`enumerators_of` returns the enumerator constants of the indicated enumeration type in declaration order.

### Member Access Reflection {#member-access}

::: std
```c++
namespace std::meta {
  consteval auto access_context() -> info;

  struct access_pair {
    consteval access_pair(info target, info from = access_context());
  };

  consteval auto is_accessible(access_pair p) -> bool;

  consteval auto accessible_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_members_of(info target, info from) -> vector<info>;

  consteval auto accessible_bases_of(access_pair p) -> vector<info>;
  consteval auto accessible_bases_of(info target, info from) -> vector<info>;

  consteval auto accessible_static_data_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_static_data_members_of(info target, info from) -> vector<info>;

  consteval auto accessible_nonstatic_data_members_of(access_pair p) -> vector<info>;
  consteval auto accessible_nonstatic_data_members_of(info target, info from) -> vector<info>;

  consteval auto accessible_subobjects_of(access_pair p) -> vector<info>;
  consteval auto accessible_subobjects_of(info target, info from) -> vector<info>;
}
```
:::

The `access_context()` function returns a reflection of the function, class, or namespace whose scope encloses the function call.

The type `access_pair` represents the operands of a check for access to `target` from the scope introduced by the function, class, or namespace reflected by `from`. If `from` is not specified, the `access_pair` constructor captures the current access context of the caller via the default argument. Each function also provides an overload whereby `target` and `from` may be specified as distinct arguments.

Each function named `accessible_meow_of` returns the result of `meow_of` filtered on `is_accessible`.

For example:

::: std
```cpp
class C {
  int k;
  static_assert(is_accessible(^C::k));  // ok: context is 'C'.

  friend void fn();
}

static_assert(accessible_subobjects_of(^C).size() == 0);
static_assert(accessible_subobjects_of(^C, ^fn).size() == 1);
```
:::

### `substitute`

::: std
```c++
namespace std::meta {
  template <reflection_range R = span<info const>>
  consteval auto can_substitute(info templ, R&& args) -> bool;
  template <reflection_range R = span<info const>>
  consteval auto substitute(info templ, R&& args) -> info;
}
```
:::

Given a reflection for a template and reflections for template arguments that match that template, `substitute` returns a reflection for the entity obtained by substituting the given arguments in the template.
If the template is a concept template, the result is a reflection of a constant of type `bool`.

For example:

::: std
```c++
constexpr auto r = substitute(^std::vector, std::vector{^int});
using T = [:r:]; // Ok, T is std::vector<int>
```
:::

This process might kick off instantiations outside the immediate context, which can lead to the program being ill-formed.

Note that the template is only substituted, not instantiated.  For example:

::: std
```c++
template<typename T> struct S { typename T::X x; };

constexpr auto r = substitute(^S, std::vector{^int});  // Okay.
typename[:r:] si;  // Error: T::X is invalid for T = int.
```
:::

`can_substitute(templ, args)` simply checks if the substitution can succeed (with the same caveat about instantiations outside of the immediate context).
If `can_substitute(templ, args)` is `false`, then `substitute(templ, args)` will be ill-formed.

### `reflect_invoke`

::: std
```c++
namespace std::meta {
  template <reflection_range R = span<info const>>
  consteval auto reflect_invoke(info target, R&& args) -> info;
  template <reflection_range R1 = span<info const>, reflection_range R2 = span<info const>>
  consteval auto reflect_invoke(info target, R1&& tmpl_args, R2&& args) -> info;
}
```
:::

These metafunctions produce a reflection of the result of a call expression.

For the first overload: Letting `F` be the entity reflected by `target`, and `A@~0~@, A@~1~@, ..., A@~N~@` be the sequence of entities reflected by the values held by `args`: if the expression `F(A@~0~@, A@~1~@, ..., A@~N~@)` is a well-formed constant expression evaluating to a structural type that is not `void`, and if every value in `args` is a reflection of a value or object usable in constant expressions, then `reflect_invoke(target, args)` evaluates to a reflection of the result of `F(A@~0~@, A@~1~@, ..., A@~N~@)`. For all other invocations, `reflect_invoke(target, args)` is not a constant expression.

The second overload behaves the same as the first overload, except instead of evaluating `F(A@~0~@, A@~1~@, ..., A@~N~@)`, we require that `F` be a reflection of a template and evaluate `F<T@~0~@, T@~1~@, ..., T@~M~@>(A@~0~@, A@~1~@, ..., A@~N~@)`. This allows evaluating `reflect_invoke(^std::get, {std::meta::reflect_value(0)}, {e})` to evaluate to, approximately, `^std::get<0>([: e :])`.

If the returned reflection is of a value (rather than an object), the type of the reflected value is the cv-qualified (de-aliased) type of what's returned by the function.

A few possible extensions for `reflect_invoke` have been discussed among the authors. Given the advent of constant evaluations with side-effects, it may be worth allowing `void`-returning functions, but this would require some representation of "a returned value of type `void`". Construction of runtime call expressions is another exciting possibility. Both extensions require more thought and implementation experience, and we are not proposing either at this time.

### `reflect_value`, `reflect_object`, `reflect_function` {#reflect-expression-results}

::: std
```c++
namespace std::meta {
  template<typename T> consteval auto reflect_value(T expr) -> info;
  template<typename T> consteval auto reflect_object(T& expr) -> info;
  template<typename T> consteval auto reflect_function(T& expr) -> info;
}
```
:::

These metafunctions produce a reflection of the _result_ from evaluating the provided expression. One of the most common use-cases for such reflections is to specify the template arguments with which to build a specialization using `std::meta::substitute`.

`reflect_value(expr)` produces a reflection of the value computed by an lvalue-to-rvalue conversion on `expr`. The type of the reflected value is the cv-unqualified (de-aliased) type of `expr`. The result needs to be a permitted result of a constant expression, and `T` cannot be of reference type.

```cpp
static_assert(substitute(^std::array, {^int, std::meta::reflect_value(5)}) ==
              ^std::array<int, 5>);
```

`reflect_object(expr)` produces a reflection of the object designated by `expr`. This is frequently used to obtain a reflection of a subobject, which might then be used as a template argument for a non-type template parameter of reference type.

```cpp
template <int &> void fn();

int p[2];
constexpr auto r = substitute(^fn, {std::meta::reflect_object(p[1])});
```

`reflect_function(expr)` produces a reflection of the function designated by `expr`. It can be useful for reflecting on the properties of a function for which only a reference is available.

```cpp
consteval bool is_global_with_external_linkage(void(*fn)()) {
  std::meta::info rfn = std::meta::reflect_function(*fn);

  return (has_external_linkage(rfn) && parent_of(rfn) == ^::);
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
Also unlike splicers, it requires knowledge of the type associated with the entity reflected by its operand.

### `data_member_spec`, `define_class`

::: std
```c++
namespace std::meta {
  struct data_member_options_t {
    struct name_type {
      template <typename T> requires constructible_from<u8string, T>
        consteval name_type(T &&);

      template <typename T> requires constructible_from<string, T>
        consteval name_type(T &&);
    };

    optional<name_type> name;
    optional<int> alignment;
    optional<int> width;
    bool no_unique_address = false;    
  };
  consteval auto data_member_spec(info type,
                                  data_member_options_t options = {}) -> info;
  template <reflection_range R = span<info const>>
  consteval auto define_class(info type_class, R&&) -> info;
}
```
:::

`data_member_spec` returns a reflection of a description of a data member of given type. Optional alignment, bit-field-width, static-ness, and name can be provided as well. An inner class `name_type`, which may be implicitly constructed from any of several "string-like" types (e.g., `string_view`, `u8string_view`, `char8_t[]`, `char_t[]`), is used to represent the name. If a `name` is provided, it must be a valid identifier when interpreted as a sequence of UTF-8 code-units (after converting any contained UCNs to UTF-8). Otherwise, the name of the data member is unspecified.

`define_class` takes the reflection of an incomplete class/struct/union type and a range of reflections of data member descriptions and completes the given class type with data members as described (in the given order).
The given reflection is returned. For now, only data member reflections are supported (via `data_member_spec`) but the API takes in a range of `info` anticipating expanding this in the near future.

For example:

::: std
```c++
union U;
static_assert(is_type(define_class(^U, {
  data_member_spec(^int),
  data_member_spec(^char),
  data_member_spec(^double),
})));

// U is now defined to the equivalent of
// union U {
//   int $_0$;
//   char $_1$;
//   double $_2$;
// };

template<typename T> struct S;
constexpr auto s_int_refl = define_class(^S<int>, {
  data_member_spec(^int, {.name="i", .alignment=64}),
  data_member_spec(^int, {.name=u8"こんにち", .alignment=64}),
  data_member_spec(^int, {.name="v\\N{LATIN SMALL LETTER AE}rs\\u{e5}god"})
});

// S<int> is now defined to the equivalent of
// template<> struct S<int> {
//   alignas(64) int i;
//   alignas(64) int こんにち;
//               int værsågod;
// };
```
:::

When defining a `union`, if one of the alternatives has a non-trivial destructor, the defined union will _still_ have a destructor provided - that simply does nothing.
This allows implementing [variant](#a-simple-variant-type) without having to further extend support in `define_class` for member functions.

If `type_class` is a reflection of a type that already has a definition, or which is in the process of being defined, the call to `define_class` is not a constant expression.

### Data Layout Reflection
::: std
```c++
namespace std::meta {
  consteval auto offset_of(info entity) -> size_t;
  consteval auto size_of(info entity) -> size_t;
  consteval auto alignment_of(info entity) -> size_t;

  consteval auto bit_offset_of(info entity) -> size_t;
  consteval auto bit_size_of(info entity) -> size_t;

}
```
:::

These are generalized versions of some facilities we already have in the language.

* `offset_of` takes a reflection of a non-static data member or a base class subobject and returns the offset of it.
* `size_of` takes the reflection of a type, object, variable, non-static data member, or base class subobject and returns its size.
* `alignment_of` takes the reflection of a type, non-static data member, or base class subobject and returns its alignment.
* `bit_size_of` and `bit_offset_of` give the size and offset of a base class subobject or non-static data member, except in bits. Note that the `bit_offset_of` is a value between `0` and `7`, inclusive:

::: std
```cpp
struct Msg {
    uint64_t a : 10;
    uint64_t b :  8;
    uint64_t c : 25;
    uint64_t d : 21;
};

static_assert(bit_offset_of(^Msg::a) == 0);
static_assert(bit_offset_of(^Msg::b) == 2);
static_assert(bit_offset_of(^Msg::c) == 2);
static_assert(bit_offset_of(^Msg::d) == 3);

static_assert(bit_size_of(^Msg::a) == 10);
static_assert(bit_size_of(^Msg::b) == 8);
static_assert(bit_size_of(^Msg::c) == 25);
static_assert(bit_size_of(^Msg::d) == 21);

consteval auto total_bit_offset_of(std::meta::info m) -> size_t {
    return offset_of(m) * 8 + bit_offset_of(m);
}

static_assert(total_bit_offset_of(^Msg::a) == 0);
static_assert(total_bit_offset_of(^Msg::b) == 10);
static_assert(total_bit_offset_of(^Msg::c) == 18);
static_assert(total_bit_offset_of(^Msg::d) == 43);

```
:::


### Other Type Traits

There is a question of whether all the type traits should be provided in `std::meta`.
For instance, a few examples in this paper use `std::meta::type_remove_cvref(t)` as if that exists.
Technically, the functionality isn't strictly necessary - since it can be provided indirectly:

::: cmptable
### Direct
```cpp
std::meta::type_remove_cvref(type)
```

### Indirect
```cpp
std::meta::substitute(^std::remove_cvref_t, {type})
```

---

```cpp
std::meta::type_is_const(type)
```

```cpp
std::meta::extract<bool>(std::meta::substitute(^std::is_const_v, {type}))
```
:::

Having `std::meta::meow` for every trait `std::meow` is more straightforward and will likely be faster to compile, though means we will have a much larger library API.
There are quite a few traits in [meta]{.sref} - but it should be easy enough to specify all of them.
So we're doing it.

Now, one thing that came up is that the straightforward thing we want to do is to simply add a `std::meta::meow` for every trait `std::meow` and word it appropriately. That's what the current wording in this revision does.
However, we've run into a conflict.
The standard library type traits are all *type* traits - they only accept types.
As such, their names are simply things like `std::is_pointer`, `std::is_const`, `std::is_lvalue_reference`, and so forth.
Renaming it to `std::type_is_pointer`, for instance, would be a waste of characters since there's nothing else the argument could be save for a type.
But this is no longer the case.
Consider `std::meta::is_function(e)`, which is currently actually specified twice in our wording having two different meanings:

1. A consteval function equivalent of the type trait `std::is_function<T>`, such that `std::meta::is_function(e)` mandates that `e` reflect a type and checks if that type is a function type.
  This is the same category of type trait as the ones mentioned above.
2. A new kind of reflection query `std::meta::is_function(e)` which asks if `e` is the reflection of a function (as opposed to a type or a namespace or a template, etc.).
  This is the same category of query as `std::meta::is_template` or `std::meta::is_concept` or `std::meta::is_namespace`.

Both of these are useful, yet they mean different things entirely - the first is ill-formed when passed a reflection of a function (as opposed to a function type), and the second would simply answer `false` for the reflection of _any_ type (function type or otherwise).
So what do we do?

Probably the most straightforward choice would be to either prefix or suffix all of the type traits with `_type`.
We think prefix is a little bit better because it groups all the type traits together and perhaps make it clearer that the argument(s) must be types.
That is: `std::is_pointer<T>` because `std::meta::type_is_pointer(^T)`, `std::is_arithmetic<T>` becomes `std::meta::type_is_arithmetic(^T)`, and so forth.
The advantage of this approach is that it very likely just works, also opening the door to making a more general `std::meta::is_const(e)` that checks not just if `e` is a `const`-qualified type but also if it's a `const`-qualified object or a `const`-qualified member, etc.
The disadvantage is that the suffixed names would not be familiar - we're much more familiar with the name `is_copy_constructible` than we would be with `type_is_copy_constructible`.

That said, it's not too much added mental overhead to remember `type_is_copy_constructible` and this avoids have to remember which type traits have the suffix and which don't. Not to mention that _many_ of the type traits read as if they would accept objects just fine (e.g. `is_trivially_copyable`). So we propose that simply all the type traits be suffixed with `*_type`.

## ODR Concerns

Static reflection invariably brings new ways to violate ODR.

```cpp
// File 'cls.h'
struct Cls {
  void odr_violator() {
    if constexpr (members_of(parent_of(^std::size_t)).size() % 2 == 0)
      branch_1();
    else
      branch_2();
  }
};
```

Two translation units including `cls.h` can generate different definitions of `Cls::odr_violator()` based on whether an odd or even number of declarations have been imported from `std`. Branching on the members of a namespace is dangerous because namespaces may be redeclared and reopened: the set of contained declarations can differ between program points.

The creative programmer will find no difficulty coming up with other predicates which would be similarly dangerous if substituted into the same `if constexpr` condition: for instance, given a branch on `is_incomplete_type(^T)`, if one translation unit `#include`s a forward declaration of `T`, another `#include`s a complete definition of `T`, and they both afterwards `#include "cls.h"`, the result will be an ODR violation.

Additional papers are already in flight proposing additional metafunctions that pose similar dangers. For instance, [@P3096R1] proposes the `parameters_of` metafunction. This feature is important for generating language bindings (e.g., Python, JavaScript), but since parameter names can differ between declarations, it would be dangerous for a member function defined in a header file to branch on the name of a parameter.

These cases are not difficult to identify: Given an entity `E` and two program points `P1` and `P2` from which a reflection of `E` may be optained, it is unsafe to branch runtime code generation on any property of `E` (e.g., namespace members, parameter names, completeness of a class) that can be modified between `P1` and `P2`. Worth noting as well, these sharp edges are not unique (or new) to reflection: It is already possible to build an ODR trap based on the completeness of a class using C++23.

Education and training are important to help C++ users avoid such sharp edges, but we do not find them sufficiently concerning to give pause to our enthusiasm for the features proposed by this paper.

# Proposed Wording

## Language

### [lex.phases]{.sref} Phases of translation {-}

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

::: std

[7]{.pnum} Whitespace characters separating tokens are no longer significant. Each preprocessing token is converted into a token (5.6). The resulting tokens constitute a translation unit and are syntactically and semantically analyzed and translated.
[ Plainly constant-evaluated expressions ([expr.const]) appearing outside template declarations are evaluated in lexical order.
  Diagnosable rules ([intro.compliance.general]{.sref}) that apply to constructs whose syntactic end point occurs lexically after the syntactic end point of a plainly constant-evaluated expression X are considered in a context where X has been evaluated.]{.addu}
[...]

[8]{.pnum} [...]
All the required instantiations are performed to produce instantiation units.
[ Plainly constant-evaluated expressions ([expr.const]) appearing in those instantiation units are evaluated in lexical order as part of the instantiation process.
  Diagnosable rules ([intro.compliance.general]{.sref}) that apply to constructs whose syntactic end point occurs lexically after the syntactic end point of a plainly constant-evaluated expression X are considered in a context where X has been evaluated.]{.addu}
[...]

:::

### [lex.pptoken]{.sref} Preprocessing tokens {-}

Add a bullet after [lex.pptoken]{.sref} bullet (3.2):

::: std
  ...

  --- Otherwise, if the next three characters are `<::` and the subsequent character is neither `:` nor `>`, the `<` is treated as a preprocessing token by itself and not as the first character of the alternative token `<:`.

::: addu
  --- Otherwise, if the next three characters are `[::` and the subsequent character is not `:` or if the next three characters are `[:>`, the `[` is treated as a preprocessing token by itself and not as the first character of the preprocessing token `[:`.
:::
  ...
:::

### [lex.operators]{.sref} Operators and punctuators {-}

Change the grammar for `$operator-or-punctuator$` in paragraph 1 of [lex.operators]{.sref} to include splicer delimiters:

::: std
```
  $operator-or-punctuator$: @_one of_@
         {        }        [        ]        (        )        @[`[:        :]`]{.addu}@
         <:       :>       <%       %>       ;        :        ...
         ?        ::       .       .*        ->       ->*      ~
         !        +        -        *        /        %        ^        &        |
         =        +=       -=       *=       /=       %=       ^=       &=       |=
         ==       !=       <        >        <=       >=       <=>      &&       ||
         <<       >>       <<=      >>=      ++       --       ,
         and      or       xor      not      bitand   bitor    compl
         and_eq   or_eq    xor_eq   not_eq
```
:::

### [basic.def.odr]{.sref} One-definition rule {-}

Modify paragraph 4.1 to cover splicing of functions:

::: std
- [4.1]{.pnum} A function is named by an expression or conversion if it is the selected member of an overload set ([basic.lookup], [over.match], [over.over]) in an overload resolution performed as part of forming that expression or conversion, [or if it is denoted by a _splice-expression_ ([expr.prim.splice]),]{.addu} unless it is a pure virtual function and either the expression is not an _id-expression_ naming the function with an explicitly qualified name or the expression forms a pointer to member ([expr.unary.op]).
:::

Modify the first sentence of paragraph 5 to cover splicing of variables:

::: std
- [5]{.pnum} A variable is named by an expression if the expression is an _id-expression_ [or _splice-expression_ ([expr.prim.splice])]{.addu} that denotes it.
:::

Modify paragraph 6 to cover splicing of structured bindings:

::: std
- [6]{.pnum} A structured binding is odr-used if it appears as a potentially-evaluated expression[, or if a reflection of it is the operand of a potentially-evaluated _splice-expression_ ([expr.prim.splice])]{.addu}.
:::

Prepend before paragraph 15 of [basic.def.odr]{.sref}:

::: std

::: addu

[15pre]{.pnum} If a class `C` is defined in a translation unit with a call to `std::meta::define_class`, every definition of that class shall be the result of a call to `std::meta::define_class` such that its respective members are equal in number and have respectively the same types, alignments, `[[no_unique_address]]` attributes (if any), bit-field widths (if any), and specified names (if any).

:::


[15]{.pnum} [Otherwise, for]{.addu} [For]{.rm} any definable item D with definitions ...
:::

### [basic.lookup.argdep]{.sref} Argument-dependent name lookup {-}

Add a bullet to paragraph 3 of [basic.lookup.argdep]{.sref} as follows [this must precede the fundamental type bullet, because `meta::info` is a fundamental type]{.ednote}:

::: std

[3]{.pnum} ... Any `$typedef-name$`s and `$using-declaration$`s used to specify the types do not contribute to this set. The set of entities is determined in the following way:

::: addu

- [3.0]{.pnum} If `T` is `std::meta::info`, its associated set of entities is the singleton containing the function `std::meta::is_type`.

:::
- [3.1]{.pnum} If `T` is a fundamental type, its associated set of entities is empty.
- [3.2]{.pnum} If `T` is a class type ...

:::

### [basic.lookup.qual.general]{.sref} General {-}

Extend [basic.lookup.qual.general]{.sref}/1-2 to cover `$splice-name-qualifer$`:

::: std
[1]{.pnum} Lookup of an *identifier* followed by a ​`::`​ scope resolution operator considers only namespaces, types, and templates whose specializations are types. If a name, `$template-id$`, [or]{.rm} `$computed-type-specifier$`[, or `$splice-name-qualifier$`]{.addu} is followed by a ​`::`​, it shall designate a namespace, class, enumeration, or dependent type, and the ​::​ is never interpreted as a complete nested-name-specifier.

[2]{.pnum} A member-qualified name is the (unique) component name ([expr.prim.id.unqual]), if any, of

* [2.1]{.pnum} an *unqualified-id* or
* [2.2]{.pnum} a `$nested-name-specifier$` of the form `$type-name$ ::` [or]{.rm}[,]{.addu} `$namespace-name$ ::`[, or `$splice-name-qualifier$ ::`]{.addu}

in the *id-expression* of a class member access expression ([expr.ref]). [...]
:::

### [basic.link] Program and Linkage {-}

Add a bullet to paragraph 4, and renumber accordingly:

::: std

[4]{.pnum} An unnamed namespace or a namespace declared directly or indirectly within an unnamed namespace has internal linkage. All other namespaces have external linkage. The name of an entity that belongs to a namespace scope that has not been given internal linkage above and that is the name of
* [4.1]{.pnum} a variable; or

...

has its linkage determined as follows:

* [4.7]{.pnum} if the enclosing namespace has internal linkage, the name has internal linkage;
* [4.8]{.pnum} otherwise, if the declaration of the name is attached to a named module ([module.unit]) and is not exported ([module.interface]), the name has module linkage;
* [4.9]{.pnum} [otherwise, if the declaration is a variable having _consteval-only type_ ([basic.types.general]), or is of a class template specialization type having a _consteval-only type_ as a non-type template argument, the name has internal linkage.]{.addu}
* [4.10]{.pnum} otherwise, the name has external linkage.

:::

### [basic.types.general]{.sref} General {-}

Change the first sentence in paragraph 9 of [basic.types.general]{.sref} as follows:

::: std
[9]{.pnum} Arithmetic types (6.8.2), enumeration types, pointer types, pointer-to-member types (6.8.4), [`std::meta::info`,]{.addu} `std::nullptr_t`, and cv-qualified (6.8.5) versions of these types are collectively called scalar types.
:::

Add a new paragraph at the end of [basic.types.general]{.sref} as follows:

::: std
::: addu

[*]{.pnum} A *consteval-only type* is one of the following:

  - `std::meta::info`, or
  - a pointer or reference to a consteval-only type, or
  - an (possibly multi-dimensional) array of a consteval-only type, or
  - a pointer-to-member type to a class `C` of type `M` where either `C` or `M` is a consteval-only type, or
  - a function type with a consteval-only return type or a consteval-only parameter type, or
  - a class type with a consteval-only base class type or consteval-only non-static data member type.

An object of consteval-only type shall either end its lifetime during the evaluation of a manifestly constant-evaluated expression or conversion ([expr.const]{.sref}), or be a constexpr variable that is not odr-used ([basic.def.odr]{.sref}).

[*]{.pnum} Consteval-only types may not be used to declare a static data member of a class having module or external linkage. Furthermore, specializations of a class template having a non-type template argument of consteval-only type may not be used to declare a static data member of a class having module or external linkage.

:::
:::

### [basic.fundamental]{.sref} Fundamental types {-}

Add a new paragraph before the last paragraph of [basic.fundamental]{.sref} as follows:

::: std
::: addu

[*]{.pnum} A value of type `std::meta::info` is called a _reflection_ and represents a language element such as a type, a value, an object, a non-static data member, etc. An expression convertible to `std::meta::info` is said to _reflect_ the language element represented by the resulting value; the language element is said to be _reflected by_ the expression.
`sizeof(std::meta::info)` shall be equal to `sizeof(void*)`.
[Reflections are only meaningful during translation.
The notion of consteval-only types (see [basic.types.general]{.sref}) exists to diagnose attempts at using such values outside the translation process.]{.note}

:::
:::

### [expr.prim]{.sref} Primary expressions {-}

Change the grammar for `$primary-expression$` in [expr.prim]{.sref} as follows:

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
+
+ $splice-expression$
+    [: $constant-expression$ :]
+    template[: $constant-expression$ :] < $template-argument-list$@~_opt_~@ >
```
:::

### [expr.prim.id.qual]{.sref} Qualified names {-}

Add a production to the grammar for `$nested-name-specifier$` as follows:

::: std
```diff
  $nested-name-specifier$:
      ::
      $type-name$ ::
      $namespace-name$ ::
      $computed-type-specifier$ ::
+     $splice-name-qualifier$ ::
      $nested-name-specifier$ $identifier$ ::
      $nested-name-specifier$ template@~_opt_~@ $simple-template-id$ ::
+
+ $splice-name-qualifier$:
+     [: $constant-expression$ :]
```
:::

Extend [expr.prim.id.qual]{.sref}/1 to also cover splices:

::: std
[1]{.pnum} The component names of a `$qualified-id$` are those of its `$nested-name-specifier$` and `$unqualified-id$`. The component names of a `$nested-name-specifier$` are its `$identifier$` (if any) and those of its `$type-name$`, `$namespace-name$`, `$simple-template-id$`, [and/or]{.rm} `$nested-name-specifier$`[, and/or the `$type-name$` or `$namespace-name$` of the entity reflected by the `$constant-expression$` of its `$splice-name-qualifier$`. For a `$nested-name-specifier$` having a `$splice-name-qualifier$` with a `$constant-expression$` that reflects the global namespace, the component names are the same as for `::`. The `$constant-expression$` of a `$splice-name-qualifier$` shall be a reflection of either a `$type-name$`, `$namespace-name$`, or the global namespace]{.addu}.

:::

Extend [expr.prim.id.qual]{.sref}/3 to also cover splices:

::: std
[3]{.pnum} The `$nested-name-specifier$` `​::`​ nominates the global namespace. A `$nested-name-specifier$` with a `$computed-type-specifier$` nominates the type denoted by the `$computed-type-specifier$`, which shall be a class or enumeration type. [A `$nested-name-specifier$` with a `$splice-name-qualifier$` nominates the entity reflected by the `$constant-expression$` of the `$splice-name-qualifier$`.]{.addu} If a nested-name-specifier N is declarative and has a simple-template-id with a template argument list A that involves a template parameter, let T be the template nominated by N without A. T shall be a class template.

...

:::

### 7.5.8* [expr.prim.splice] Expression splicing {-}

Add a new subsection of [expr.prim]{.sref} following [expr.prim.req]{.sref}

::: std
::: addu
**Expression Splicing   [expr.prim.splice]**

[#]{.pnum} For a `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` the `$constant-expression$` shall be a converted constant expression ([expr.const]{.sref}) of type `std::meta::info`.

[#]{.pnum} For a `$splice-expression$` of the form `[: $constant-expression$ :]` where the converted `$constant-expression$` evaluates to a reflection for an object, a function which is not a constructor or destructor, a non-static data member, or an enumerator, or a structured binding, the expression is an lvalue denoting the reflected entity. If the converted `$constant-expression$` evaluates to a reflection for a variable or a structured binding, the expression is an lvalue denoting the object designated by the reflected entity. [Access checking of class members occurs during name lookup, and therefore does not pertain to splicing.]{.note}

[#]{.pnum} Otherwise, for a `$splice-expression$` of the form `[: $constant-expression$ :]` the converted `$constant-expression$` shall evaluate to a reflection of a value, and the expression shall be a prvalue whose evaluation computes the reflected value.
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
+   $postfix-expression$ . $template$@~_opt_~@ $splice-expression$
    $postfix-expression$ -> $template$@~_opt_~@ $id-expression$
+   $postfix-expression$ -> $template$@~_opt_~@ $splice-expression$
```
:::

### [expr.ref] Class member access {-}

Modify paragraph 1 to account for splices in member access expressions:

::: std
[1]{.pnum} A postfix expression followed by a dot `.` or an arrow `->`, optionally followed by the keyword template, and then followed by an _id-expression_ [or a _splice-expression_]{.addu}, is a postfix expression. [If the keyword `template` is used, the following unqualified name is considered to refer to a template ([temp.names]). If a `$simple-template-id$` results and is followed by a `​::`​, the _id-expression_ [or _splice-expression_]{.addu} is a qualified-id.]{.note}

:::

Modify paragraph 2 to account for splices in member access expressions:

::: std
[2]{.pnum} For the first option, if the [dot is followed by an]{.addu} `$id-expression$` [names]{.rm} [ or `$splice-expression$` naming]{.addu} a static member or an enumerator, the first expression is a discarded-value expression ([expr.context]); if the `$id-expression$` [or `$splice-expression$`]{.addu} names a non-static data member, the first expression shall be a glvalue. For the second option (arrow), the first expression shall be a prvalue having pointer type. The expression E1->E2 is converted to the equivalent form (*(E1)).E2; the remainder of [expr.ref] will address only the first option (dot).
:::

Modify paragraph 3 to account for splices in member access expressions:

::: std
[3]{.pnum} The postfix expression before the dot is evaluated the result of that evaluation, together with the `$id-expression$` [or `$splice-expression$`]{.addu}, determines the result of the entire postfix expression.
:::

Modify paragraph 4 to account for splices in member access expressions:

::: std
[4]{.pnum} Abbreviating [`$postfix-expression$`.`$id-expression$`]{.rm} [`$postfix-expression$.EXPR`, where `EXPR` is the `$id-expression$` or `$splice-expression$` following the dot,]{.addu} as `E1.E2`, `E1` is called the `$object expression$`. If the object expression is of scalar type, `E2` shall name the pseudo-destructor of that same type (ignoring cv-qualifications) and `E1.E2` is a prvalue of type “function of () returning `void`”.

:::

### [expr.unary.general]{.sref} General {-}

Change [expr.unary.general]{.sref} paragraph 1 to add productions for the new operator:

::: std
[1]{.pnum} Expressions with unary operators group right-to-left.
```diff
  $unary-expression$:
     ...
     $delete-expression$
+    $reflect-expression$

+ $reflect-expression$:
+    ^ ::
+    ^ $namespace-name$
+    ^ $nested-name-specifier$@~_opt_~@ $template-name$
+    ^ $nested-name-specifier$@~_opt_~@ $concept-name$
+    ^ $type-id$
+    ^ $id-expression$
```
:::

### 7.6.2.10* [expr.reflect] The reflection operator {-}

Add a new subsection of [expr.unary]{.sref} following [expr.delete]{.sref}

::: std
::: addu
**The Reflection Operator   [expr.reflect]**

[#]{.pnum} The unary `^` operator (called _the reflection operator_) produces a prvalue --- called a _reflection_ --- whose type is the reflection type (i.e., `std::meta::info`).
That reflection represents its operand.

[#]{.pnum} Every value of type `std::meta::info` is either a reflection of some entity (or description thereof) or a *null reflection value*.

[#]{.pnum} A _reflect-expression_ is parsed as the longest possible sequence of tokens that could syntactically form a _reflect-expression_.

[#]{.pnum}

::: example
```
static_assert(is_type(^int()));    // ^ applies to the type-id "int()"

template<bool> struct X {};
bool operator<(std::meta::info, X<false>);
consteval void g(std::meta::info r, X<false> xv) {
  if (r == ^int && true);    // error: ^ applies to the type-id "int&&"
  if (r == ^int & true);     // error: ^ applies to the type-id "int&"
  if (r == (^int) && true);  // OK
  if (^X < xv);       // error: < starts template argument list
  if ((^X) < xv);     // OK
}


```
:::

[#]{.pnum} When applied to `::`, the reflection operator produces a reflection for the global namespace.
When applied to a `$namespace-name$`, the reflection operator produces a reflection for the indicated namespace or namespace alias.

[#]{.pnum} When applied to a `$template-name$`, the reflection operator produces a reflection for the indicated template.

[#]{.pnum} When applied to a `$concept-name$`, the reflection operator produces a reflection for the indicated concept.

[#]{.pnum} When applied to a `$type-id$`, the reflection operator produces a reflection for the indicated type or type alias.

[#]{.pnum} When applied to an lvalue `$id-expression$` ([expr.prim.id]{.sref}), the reflection operator produces a reflection of the variable, function, enumerator constant, or non-static member designated by the operand.
The `$id-expression$` is not evaluated.

* [#.#]{.pnum} If this `$id-expression$` names an overload set `S`, and if the assignment of `S` to an invented variable of type `const auto` ([dcl.type.auto.deduct]{.sref}) would select a unique candidate function `F` from `S`, the result is a reflection of `F`. Otherwise, the expression `^S` is ill-formed.

[#]{.pnum} When applied to a prvalue `$id-expression$`, the reflection operator produces a reflection of the value computed by the operand [An `$id-expression$` naming a non-type template parameter of non-class and non-reference type is a prvalue.]{.note}

::: example
```cpp
template <typename T> void fn() requires (^T != ^int);
template <typename T> void fn() requires (^T == ^int);
template <typename T> void fn() requires (sizeof(T) == sizeof(int));

constexpr auto R = ^fn<char>;     // OK
constexpr auto S = ^fn<int>;      // error: cannot reflect an overload set

constexpr auto r = ^std::vector;  // OK
```
:::

:::

:::

### [expr.eq]{.sref} Equality Operators {-}

Extend [expr.eq]{.sref}/2 to also handle `std::meta::info:

::: std
[2]{.pnum} The converted operands shall have arithmetic, enumeration, pointer, or pointer-to-member type, or [type]{.rm} [types `std::meta::info` or ]{.addu} `std​::​nullptr_t`. The operators `==` and `!=` both yield `true` or `false`, i.e., a result of type `bool`. In each case below, the operands shall have the same type after the specified conversions have been applied.

:::

Add a new paragraph between [expr.eq]{.sref}/5 and /6:

::: std
[5]{.pnum} Two operands of type `std​::​nullptr_t` or one operand of type `std​::​nullptr_t` and the other a null pointer constant compare equal.

::: addu
[*]{.pnum} If both operands are of type `std::meta::info`, comparison is defined as follows:

* [*.#]{.pnum} If both operands are null reflection values, then they compare equal.
* [*.#]{.pnum} Otherwise, if one operand is a null reflection value, then they compare unequal.
* [*.#]{.pnum} Otherwise, if one operand is a reflection of a namespace alias, alias template, or type alias and the other operand is not a reflection of the same kind of alias, they compare unequal. [A reflection of a type and a reflection of an alias to that same type do not compare equal.]{.note}
* [*.#]{.pnum} Otherwise, if both operands are reflections of a namespace alias, alias template, or type alias, then they compare equal if their reflected aliases share the same name, are declared within the same enclosing scope, and alias the same underlying entity.
* [*.#]{.pnum} Otherwise, if neither operand is a reflection of a value, then they compare equal if they are reflections of the same entity.
* [*.#]{.pnum} Otherwise, if one operand is a reflection of a value and the other is not, then they compare unequal.
* [*.#]{.pnum} Otherwise, if both operands are reflections of values, then they compare equal if and only if the reflected values are _template-argument-equivalent_ ([temp.type]{.sref}).
* [*.#]{.pnum} Otherwise the result is unspecified.
:::

[6]{.pnum} If two operands compare equal, the result is `true` for the `==` operator and `false` for the `!=` operator. If two operands compare unequal, the result is `false` for the `==` operator and `true` for the `!=` operator. Otherwise, the result of each of the operators is unspecified.
:::


### [expr.const]{.sref} Constant Expressions {-}

Add a new paragraph after the definition of _manifestly constant-evaluated_ [expr.const]{.sref}/20:

::: std
::: addu

[21]{.pnum} An expression or conversion is _plainly constant-evaluated_ if it is:

* [#.#]{.pnum} a `$constant-expression$`, or
* [#.#]{.pnum} the condition of a constexpr if statement ([stmt.if]{.sref}),
* [#.#]{.pnum} the initializer of a `constexpr` ([dcl.constexpr]{.sref}) or `constinit` ([dcl.constinit]{.sref}) variable, or
* [#.#]{.pnum} an immediate invocation, unless it

  * [#.#.#]{.pnum} results from the substitution of template parameters in a concept-id ([temp.names]{.sref}), a `$requires-expression$` ([expr.prim.req]{.sref}), or during template argument deduction ([temp.deduct]{.sref}), or
  * [#.#.#]{.pnum} is a manifestly constant-evaluated initializer of a variable that is neither  `constexpr` ([dcl.constexpr]{.sref}) nor `constinit` ([dcl.constinit]{.sref}).


:::
:::

### [dcl.typedef]{.sref} The `typedef` specifier {-}

Introduce the term "type alias" to [dcl.typedef]{.sref}:

::: std
[1]{.pnum} [...] A name declared with the `typedef` specifier becomes a typedef-name. A typedef-name names the type associated with the identifier ([dcl.decl]) or simple-template-id ([temp.pre]); a typedef-name is thus a synonym for another type. A typedef-name does not introduce a new type the way a class declaration ([class.name]) or enum declaration ([dcl.enum]) does.

[2]{.pnum} A *typedef-name* can also be introduced by an alias-declaration. The identifier following the using keyword is not looked up; it becomes a typedef-name and the optional attribute-specifier-seq following the identifier appertains to that typedef-name. Such a typedef-name has the same semantics as if it were introduced by the typedef specifier. In particular, it does not define a new type.

::: addu
[*]{.pnum} A *type alias* is either a name declared with the `typedef` specifier or a name introduced by an *alias-declaration*.
:::
:::

### [dcl.type.simple]{.sref} Simple type specifiers {-}

Extend the grammar for `$computed-type-specifier$` as follows:

::: std
```diff
  $computed-type-specifier$:
     $decltype-specifier$
     $pack-index-specifier$
+    $splice-type-specifier$


+ $splice-enum-name$:
+    [: $constant-expression$ :]
+
  $using-enum-declarator$:
     $nested-name-specifier$@~_opt_~@ $identifier$
     $nested-name-specifier$@~_opt_~@ $simple-template-id$
+    $splice-enum-name$
```
:::

### 9.2.9* [dcl.type.splice] Type splicing {-}

Add a new subsection of [dcl.type]{.sref} following [dcl.type.class.deduct]{.sref}.

::: std
::: addu
```diff
+  $splice-type-specifier$
+      typename [: $constant-expression$ :]
+      [: $constant-expression$ :]
```

[#]{.pnum} The `$constant-expression$` shall be a converted constant expression ([expr.const]{.sref}) of type `std::meta::info`.

[#]{.pnum} The form `[: $constant-expression$ :]` shall only be parsed as a `$splice-type-specifier$` within a _type-only context_ ([temp.res.general]{.sref}).

[#]{.pnum} The `$constant-expression$` shall evaluate to a reflection of a type, and the type designated by the `$splice-type-specifier$` is the same as the type reflected by the `$constant-expression$`.
:::
:::

### [dcl.init.general]{.sref} Initializers (General) {-}

Change paragraphs 6-9 of [dcl.init.general]{.sref} [No changes are necessary for value-initialization, which already forwards to zero-initialization for scalar types]{.ednote}:

::: std
[6]{.pnum} To *zero-initialize* an object or reference of type `T` means:

* [6.0]{.pnum} [if `T` is `std::meta::info`, the object is initialied to a null reflection value;]{.addu}
* [6.1]{.pnum} if `T` is a scalar type ([basic.types.general]), the object is initialized to the value obtained by converting the integer literal `0` (zero) to `T`;
* [6.2]{.pnum} [...]

[7]{.pnum} To *default-initialize* an object of type `T` means:

* [7.1]{.pnum} If `T` is a (possibly cv-qualified) class type ([class]), [...]
* [7.2]{.pnum} If T is an array type, [...]
* [7.*]{.pnum} [If `T` is `std::meta::info`, the object is zero-initialized.]{.addu}
* [7.3]{.pnum} Otherwise, no initialization is performed.

[8]{.pnum} A class type `T` is *const-default-constructible* if [`T` is `std::meta::info`,]{.addu} default-initialization of `T` would invoke a user-provided constructor of T (not inherited from a base class)[,]{.addu} or if

* [8.1]{.pnum} [...]

[9]{.pnum} To value-initialize an object of type T means: [...]
:::

### [dcl.fct]{.sref} Functions {-}

Add a bullet to paragraph 9 of [dcl.fct]{.sref} to allow for reflections of abominable function types:

::: std
[9]{.pnum} A function type with a _cv-qualifier-seq_ or a _ref-qualifier_ (including a type named by _typedef-name_ ([dcl.typedef], [temp.param])) shall appear only as:

* [9.1]{.pnum} the function type for a non-static member function,
* [9.2]{.pnum} ...
* [9.5]{.pnum} the _type-id_ of a _template-argument_ for a _type-parameter_ ([temp.arg.type])[.]{.rm}[,]{.addu}

::: addu
* [9.6]{.pnum} the operand of a _reflect-expression_ ([expr.reflect]).
:::

:::

### [dcl.fct.def.delete]{.sref} Deleted definitions {-}

Change paragraph 2 of [dcl.fct.def.delete]{.sref} to allow for reflections of deleted functions:

::: std

[2]{.pnum} A program that refers to a deleted function implicitly or explicitly, other than to declare it [or to use as the operand of the reflection operator]{.addu}, is ill-formed.
:::

### [enum.udecl]{.sref} The `using enum` declaration {-}

Extend the grammar for `$using-enum-declarator$` as follows:

::: std
```diff
  $using-enum-declaration$:
     using enum $using-enum-declarator$ ;

+ $splice-enum-name$:
+    [: $constant-expression$ :]
+
  $using-enum-declarator$:
     $nested-name-specifier$@~_opt_~@ $identifier$
     $nested-name-specifier$@~_opt_~@ $simple-template-id$
+    $splice-enum-name$
```
:::

Modify paragraph 1 of [enum.udecl]{.sref} as follows:

::: std

[1]{.pnum} A `$using-enum-declarator$` [not consisting of a `$splice-enum-name$`]{.addu} names the set of declarations found by lookup ([basic.lookup.unqual]{.sref}, [basic.lookup.qual]{.sref}) for the `$using-enum-declarator$`. [A `$using-enum-declarator$` containing a `$splice-enum-name$` names the entity reflected by the `$constant-expression$`. ]{.addu}  The `$using-enum-declarator$` shall designate a non-dependent type with a reachable `$enum-specifier$`.
:::

### [namespace.udir]{.sref} Using namespace directive {-}

Modify the grammar for `$using-directive$` as follows:

::: std
```diff
+ $splice-namespace-name$:
+    [: $constant-expression$ :]
+
+ $namespace-declarator$:
+    $nested-name-specifier$@~_opt_~@ $namespace-name$
+    $splice-namespace-name$
+
  $using-directive$:
-    $attribute-specifier-seq$@~_opt_~@ using namespace $nested-name-specifier@~_opt_~@ $namespace-name$
+    $attribute-specifier-seq$@~_opt_~@ using namespace $namespace-declarator$
```
:::

Add the following to paragraph 1 of [namespace.udir]{.sref}, prior to the note:

::: std
[1]{.pnum} A `$using-directive$` shall not appear in class scope, but may appear in namespace scope or in block scope. [A `$namespace-declarator$` not consisting of a `$splice-namespace-name$` nominates the namespace found by lookup ([basic.lookup.unqual]{.sref}, [basic.lookup.qual]{.sref}) and shall not contain a dependent `$nested-name-specifier$`. A `$namespace-declarator$` consisting of a `$splice-namespace-name$` shall contain a non-dependent `$constant-expression$` that reflects a namespace or namespace alias, and nominates the entity reflected by the `$constant-expression$`.]{.addu}
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

### [over.built]{.sref} Built-in operators {-}

Add built-in operator candidates for `std::meta::info` to [over.built]{.sref}:

::: std
[16]{.pnum} For every `T`, where `T` is a pointer-to-member type[, `std::meta::info`,]{.addu} or `std​::​nullptr_t`, there exist candidate operator functions of the form
```cpp
bool operator==(T, T);
bool operator!=(T, T);
```
:::

### [temp.param]{.sref} Template parameters {-}

Extend the last sentence of paragraph 4 to disallow splicing concepts in template parameter declarations.

::: std
[4]{.pnum} ... The concept designated by a type-constraint shall be a type concept ([temp.concept]) [that does not consist of a `$splice-template-name$`]{.addu}.
:::

### [temp.names]{.sref} Names of template specializations {-}

Modify the grammars for `$template-id$` and `$template-argument$` as follows:

::: std
```diff
+ $splice-template-name$:
+     template [: constant-expression :]
+
+ $splice-template-argument$:
+     [: constant-expression :]
+
  $template-name$:
      identifier
+     $splice-template-name$

  $template-argument$:
      $constant-expression$
      $type-id$
      $id-expression$
      $braced-init-list$
+     $splice-template-argument$
```
:::

Extend paragraph 1 to cover template splicers:

::: std
The component name of a `$simple-template-id$`, `$template-id$`, or `$template-name$` is the first name in it. [If the `$template-name$` is a `$splice-template-name$`, the converted `$constant-expression$` shall evaluate to a reflection for a concept, variable template, class template, alias template, or function template which is not a constructor template or destructor template; the `$splice-template-name$` names the entity reflected by the `$constant-expression$`.]{.addu}
:::

Add a paragraph after paragraph 3 of [temp.names]{.sref}:

::: std
::: addu

[*]{.pnum} A `<` is also interpreted as the delimiter of a `$template-argument-list$` if it follows a `$template-name$` consisting of a `$splice-template-name$`.

:::
:::


### [temp.arg.general]{.sref} General {-}

Adjust paragraph 3 of [temp.arg.general] to not apply to splice template arguments:

::: std

[3]{.pnum} In a `$template-argument$` [which does not contain a `$splice-template-argument$`]{.addu}, an ambiguity between a `$type-id$` and an expression is resolved to a `$type-id$`, regardless of the form of the corresponding `$template-parameter$`. [In a `$template-argument$` containing a `$splice-template-argument$`, an ambiguity between a `$splice-template-argument$` and an expression is resolved to a `$splice-template-argument$`.]{.addu}

:::

### [temp.arg.type]{.sref} Template type arguments {-}

Extend [temp.arg.type]{.sref}/1 to cover splice template arguments:

::: std
[1]{.pnum} A `$template-argument$` for a `$template-parameter$` which is a type shall [either]{.addu} be a `$type-id$` [or a `$splice-template-argument$`. A `$template-argument$` having a `$splice-template-argument$` for such a `$template-parameter$` is treated as if it were a `$type-id$` nominating the type reflected by the `$constant-expression$` of the `$splice-template-argument$`.]{.addu}
:::

### [temp.arg.nontype]{.sref} Template non-type arguments {-}

Extend [temp.arg.nontype]{.sref}/2 to cover splice template arguments:

::: std
[2]{.pnum} The value of a non-type `$template-parameter$` _P_ of (possibly deduced) type `T` is determined from its template argument _A_ as follows. If `T` is not a class type and _A_ is [not]{.rm}[neither]{.addu} a `$braced-init-list$` [nor a `$splice-template-argument$`]{.addu}, _A_ shall be a converted constant expression ([expr.const]) of type `T`; the value of _P_ is _A_ (as converted).

:::

### [temp.arg.template]{.sref} Template template arguments {-}

Extend [temp.arg.template]{.sref}/1 to cover splice template arguments:

::: std
[1]{.pnum} A `$template-argument$` for a template `$template-parameter$` shall be the name of a class template or an alias template, expressed as `$id-expression$`[, or a `$splice-template-argument$`. A `$template-argument$` for a template `$template-parameter$` having a `$splice-template-argument$` is treated as an `$id-expression$` nominating the class template or alias template reflected by the `$constant-expression$` of the `$splice-template-argument$`.]{.addu}
:::

### [temp.type]{.sref} Type equivalence {-}

Extend *template-argument-equivalent* to handle `std::meta::info`:

::: std
[2]{.pnum} Two values are *template-argument-equivalent* if they are of the same type and

* [2.1]{.pnum} they are of integral type and their values are the same, or
* [2.2]{.pnum} they are of floating-point type and their values are identical, or
* [2.3]{.pnum} they are of type `std​::​nullptr_t`, or
* [2.*]{.pnum} [they are of type `std::meta::info` and they compare equal, or]{.addu}
* [2.4]{.pnum} they are of enumeration type and their values are the same, or
* [2.5]{.pnum} [...]
:::

### [temp.concept]{.sref} Concept definitions {-}

Extend the grammar of `$concept-name$` to allow for splicing reflections of concepts:

::: std
```diff
  $concept-name$:
    $identifier$
+   $splice-template-name$
```
:::

Modify paragraph 2 to account for splicing reflections of concepts:

::: std
A `$concept-definition$` declares a concept. Its [`$concept-name$` shall consist of an `$identifier$`, and the]{.addu} `$identifier$` becomes a _concept-name_ referring to that concept within its scope. The optional _attribute-specifier-seq_ appertains to the concept.

:::

### [temp.dep.expr]{.sref} Type-dependent expressions {-}

Add to the list of never-type-dependent expression forms in [temp.dep.expr]{.sref}/4:

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

[9]{.pnum} A `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` is type-dependent if the `$constant-expression$` is value-dependent or if the optional `$template-argument-list$` contains a value-dependent nontype or template argument, or a dependent type argument.

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

[A `$reflect-expression$` is value dependent if the operand of the reflection operator is a type-dependent or value-dependent expression or if that operand is a dependent `$type-id$`, a dependent `$namespace-name$`, or a dependent `$template-name$`.]{.addu}
:::


Add a new paragraph after [temp.dep.constexpr]{.sref}/4:

::: std
::: addu

[6]{.pnum} A `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` is value-dependent if the `$constant-expression$` is value-dependent or if the optional `$template-argument-list$` contains a value-dependent nontype or template argument, or a dependent type argument.

:::
:::



## Library

### [namespace.std]{.sref} Namespace std

Insert before paragraph 7:

::: std

[6]{.pnum} Let F denote a standard library function ([global.functions]), a standard library static member function, or an instantiation of a standard library function template.
Unless F is designated an *addressable function*, the behavior of a C++ program is unspecified (possibly ill-formed) if it explicitly or implicitly attempts to form a pointer to F. [...]

::: addu

[7pre]{.pnum}
Let F denote a standard library function, member function, or function template.
If F does not designate an addressable function, it is unspecified if or how a reflection value designating the associated entity can be formed.
[ E.g., `std::meta::members_of` might not produce reflections of standard functions that an implementation handles through an extra-linguistic mechanism.]{.note}

:::

[7]{.pnum} A translation unit shall not declare namespace std to be an inline namespace ([namespace.def]).

:::


### [meta.type.synop]{.sref} Header `<type_traits>` synopsis

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
      constexpr bool is_reflection_v = is_function<T>::value;
+   template<class T>
+     constexpr bool is_reflection_v = is_function<T>::value;
```
:::

### [meta.unary.cat]{.sref} Primary type categories

Add the `is_reflection` primary type category to the table in paragraph 3:

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

### [meta.synop] Header `<meta>` synopsis {-}

Add a new subsection in [meta]{.sref} after [type.traits]{.sref}:

::: std
::: addu
**Header `<meta>` synopsis**

```
#include <span>
#include <string_view>
#include <vector>

namespace std::meta {
  using info = decltype(^::);

  // [meta.reflection.names], reflection names and locations
  consteval string_view name_of(info r);
  consteval string_view qualified_name_of(info r);
  consteval string_view display_name_of(info r);

  consteval u8string_view u8name_of(info r);
  consteval u8string_view u8qualified_name_of(info r);
  consteval u8string_view u8display_name_of(info r);

  consteval source_location source_location_of(info r);

  // [meta.reflection.queries], reflection queries
  consteval bool is_public(info r);
  consteval bool is_protected(info r);
  consteval bool is_private(info r);
  consteval bool is_virtual(info r);
  consteval bool is_pure_virtual(info r);
  consteval bool is_override(info r);
  consteval bool is_deleted(info r);
  consteval bool is_defaulted(info r);
  consteval bool is_explicit(info r);
  consteval bool is_noexcept(info r);
  consteval bool is_bit_field(info r);
  consteval bool is_const(info r);
  consteval bool is_volatile(info r);
  consteval bool is_final(info r);
  consteval bool has_static_storage_duration(info r);
  consteval bool has_internal_linkage(info r);
  consteval bool has_module_linkage(info r);
  consteval bool has_external_linkage(info r);
  consteval bool has_linkage(info r);

  consteval bool is_namespace(info r);
  consteval bool is_function(info r);
  consteval bool is_variable(info r);
  consteval bool is_type(info r);
  consteval bool is_alias(info r);
  consteval bool is_incomplete_type(info r);
  consteval bool is_template(info r);
  consteval bool is_function_template(info r);
  consteval bool is_variable_template(info r);
  consteval bool is_class_template(info r);
  consteval bool is_alias_template(info r);
  consteval bool is_concept(info r);
  consteval bool is_value(info r);
  consteval bool is_object(info r);
  consteval bool is_structured_binding(info r);
  consteval bool has_template_arguments(info r);
  consteval bool is_class_member(info entity);
  consteval bool is_namespace_member(info entity);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);
  consteval bool is_constructor(info r);
  consteval bool is_destructor(info r);
  consteval bool is_special_member(info r);
  consteval bool is_user_provided(info r);

  consteval info type_of(info r);
  consteval info object_of(info r);
  consteval info value_of(info r);
  consteval info parent_of(info r);
  consteval info dealias(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);

  // [meta.reflection.member.queries], reflection member queries
  consteval vector<info> members_of(info type);
  consteval vector<info> bases_of(info type);
  consteval vector<info> static_data_members_of(info type);
  consteval vector<info> nonstatic_data_members_of(info type);
  consteval vector<info> subobjects_of(info type);
  consteval vector<info> enumerators_of(info type_enum);

  // [meta.reflection.member.access], reflection member access queries
  consteval info access_context();

  struct access_pair {
    consteval access_pair(info target, info from = access_context());
  };

  consteval bool is_accessible(access_pair p);
  consteval bool is_accessible(info r, info from);

  consteval vector<info> accessible_members_of(access_pair p);
  consteval vector<info> accessible_members_of(info target, info from);

  consteval vector<info> accessible_bases_of(access_pair p);
  consteval vector<info> accessible_bases_of(info target, info from);

  consteval vector<info> accessible_nonstatic_data_members_of(access_pair p);
  consteval vector<info> accessible_nonstatic_data_members_of(info target, info from);
  consteval vector<info> accessible_static_data_members_of(access_pair p);
  consteval vector<info> accessible_static_data_members_of(info target, info from);
  consteval vector<info> accessible_subobjects_of(access_pair p);
  consteval vector<info> accessible_subobjects_of(info target, info from);

  // [meta.reflection.layout], reflection layout queries
  consteval size_t offset_of(info entity);
  consteval size_t size_of(info entity);
  consteval size_t alignment_of(info entity);
  consteval size_t bit_offset_of(info entity);
  consteval size_t bit_size_of(info entity);

  // [meta.reflection.extract], value extraction
  template <typename T>
    consteval T extract(info);

  // [meta.reflection.substitute], reflection substitution
  template <reflection_range R = span<info const>>
    consteval bool can_substitute(info templ, R&& arguments);
  template <reflection_range R = span<info const>>
    consteval info substitute(info templ, R&& arguments);

  // [meta.reflection.result], expression result reflection
  template <class R>
    concept reflection_range = $see below$;

  template <typename T>
    consteval info reflect_value(T value);
  template <typename T>
    consteval info reflect_object(T& object);
  template <typename T>
    consteval info reflect_function(T& fn);

  template <reflection_range R = span<info const>>
    consteval info reflect_invoke(info target, R&& args);
  template <reflection_range R1 = span<info const>, reflection_range R2 = span<info const>>
    consteval info reflect_invoke(info target, R1&& tmpl_args, R2&& args);

  // [meta.reflection.define_class], class definition generation
  struct data_member_options_t {
    struct name_type {
      template <typename T> requires constructible_from<u8string, T>
        consteval name_type(T &&);

      template <typename T> requires constructible_from<string, T>
        consteval name_type(T &&);
    };

    optional<name_type> name;
    optional<int> alignment;
    optional<int> width;
    bool no_unique_address = false;
  };
  consteval info data_member_spec(info type,
                                  data_member_options_t options = {});
  template <reflection_range R = span<info const>>
  consteval info define_class(info type_class, R&&);

  // [meta.reflection.unary.cat], primary type categories
  consteval bool type_is_void(info type);
  consteval bool type_is_null_pointer(info type);
  consteval bool type_is_integral(info type);
  consteval bool type_is_floating_point(info type);
  consteval bool type_is_array(info type);
  consteval bool type_is_pointer(info type);
  consteval bool type_is_lvalue_reference(info type);
  consteval bool type_is_rvalue_reference(info type);
  consteval bool type_is_member_object_pointer(info type);
  consteval bool type_is_member_function_pointer(info type);
  consteval bool type_is_enum(info type);
  consteval bool type_is_union(info type);
  consteval bool type_is_class(info type);
  consteval bool type_is_function(info type);
  consteval bool type_is_reflection(info type);

  // [meta.reflection.unary.comp], composite type categories
  consteval bool type_is_reference(info type);
  consteval bool type_is_arithmetic(info type);
  consteval bool type_is_fundamental(info type);
  consteval bool type_is_object(info type);
  consteval bool type_is_scalar(info type);
  consteval bool type_is_compound(info type);
  consteval bool type_is_member_pointer(info type);

  // [meta.reflection unary.prop], type properties
  consteval bool type_is_const(info type);
  consteval bool type_is_volatile(info type);
  consteval bool type_is_trivial(info type);
  consteval bool type_is_trivially_copyable(info type);
  consteval bool type_is_standard_layout(info type);
  consteval bool type_is_empty(info type);
  consteval bool type_is_polymorphic(info type);
  consteval bool type_is_abstract(info type);
  consteval bool type_is_final(info type);
  consteval bool type_is_aggregate(info type);
  consteval bool type_is_signed(info type);
  consteval bool type_is_unsigned(info type);
  consteval bool type_is_bounded_array(info type);
  consteval bool type_is_unbounded_array(info type);
  consteval bool type_is_scoped_enum(info type);

  template <reflection_range R = span<info const>>
    consteval bool type_is_constructible(info type, R&& type_args);
  consteval bool type_is_default_constructible(info type);
  consteval bool type_is_copy_constructible(info type);
  consteval bool type_is_move_constructible(info type);

  consteval bool type_is_assignable(info type_dst, info type_src);
  consteval bool type_is_copy_assignable(info type);
  consteval bool type_is_move_assignable(info type);

  consteval bool type_is_swappable_with(info type_dst, info type_src);
  consteval bool type_is_swappable(info type);

  consteval bool type_is_destructible(info type);

  template <reflection_range R = span<info const>>
    consteval bool type_is_trivially_constructible(info type, R&& type_args);
  consteval bool type_is_trivially_default_constructible(info type);
  consteval bool type_is_trivially_copy_constructible(info type);
  consteval bool type_is_trivially_move_constructible(info type);

  consteval bool type_is_trivially_assignable(info type_dst, info type_src);
  consteval bool type_is_trivially_copy_assignable(info type);
  consteval bool type_is_trivially_move_assignable(info type);
  consteval bool type_is_trivially_destructible(info type);

  template <reflection_range R = span<info const>>
    consteval bool type_is_nothrow_constructible(info type, R&& type_args);
  consteval bool type_is_nothrow_default_constructible(info type);
  consteval bool type_is_nothrow_copy_constructible(info type);
  consteval bool type_is_nothrow_move_constructible(info type);

  consteval bool type_is_nothrow_assignable(info type_dst, info type_src);
  consteval bool type_is_nothrow_copy_assignable(info type);
  consteval bool type_is_nothrow_move_assignable(info type);

  consteval bool type_is_nothrow_swappable_with(info type_dst, info type_src);
  consteval bool type_is_nothrow_swappable(info type);

  consteval bool type_is_nothrow_destructible(info type);

  consteval bool type_is_implicit_lifetime(info type);

  consteval bool type_has_virtual_destructor(info type);

  consteval bool type_has_unique_object_representations(info type);

  consteval bool type_reference_constructs_from_temporary(info type_dst, info type_src);
  consteval bool type_reference_converts_from_temporary(info type_dst, info type_src);

  // [meta.reflection.unary.prop.query], type property queries
  consteval size_t type_alignment_of(info type);
  consteval size_t type_rank(info type);
  consteval size_t type_extent(info type, unsigned i = 0);

  // [meta.reflection.rel], type relations
  consteval bool type_is_same(info type1, info type2);
  consteval bool type_is_base_of(info type_base, info type_derived);
  consteval bool type_is_convertible(info type_src, info type_dst);
  consteval bool type_is_nothrow_convertible(info type_src, info type_dst);
  consteval bool type_is_layout_compatible(info type1, info type2);
  consteval bool type_is_pointer_interconvertible_base_of(info type_base, info type_derived);

  template <reflection_range R = span<info const>>
    consteval bool type_is_invocable(info type, R&& type_args);
  template <reflection_range R = span<info const>>
    consteval bool type_is_invocable_r(info type_result, info type, R&& type_args);

  template <reflection_range R = span<info const>>
    consteval bool type_is_nothrow_invocable(info type, R&& type_args);
  template <reflection_range R = span<info const>>
    consteval bool type_is_nothrow_invocable_r(info type_result, info type, R&& type_args);

  // [meta.reflection.trans.cv], const-volatile modifications
  consteval info type_remove_const(info type);
  consteval info type_remove_volatile(info type);
  consteval info type_remove_cv(info type);
  consteval info type_add_const(info type);
  consteval info type_add_volatile(info type);
  consteval info type_add_cv(info type);

  // [meta.reflection.trans.ref], reference modifications
  consteval info type_remove_reference(info type);
  consteval info type_add_lvalue_reference(info type);
  consteval info type_add_rvalue_reference(info type);

  // [meta.reflection.trans.sign], sign modifications
  consteval info type_make_signed(info type);
  consteval info type_make_unsigned(info type);

  // [meta.reflection.trans.arr], array modifications
  consteval info type_remove_extent(info type);
  consteval info type_remove_all_extents(info type);

  // [meta.reflection.trans.ptr], pointer modifications
  consteval info type_remove_pointer(info type);
  consteval info type_add_pointer(info type);

  // [meta.reflection.trans.other], other transformations
  consteval info type_remove_cvref(info type);
  consteval info type_decay(info type);
  template <reflection_range R = span<info const>>
    consteval info type_common_type(R&& type_args);
  template <reflection_range R = span<info const>>
    consteval info type_common_reference(R&& type_args);
  consteval info type_underlying_type(info type);
  template <reflection_range R = span<info const>>
    `consteval info type_invoke_result(info type, R&& type_args);
  consteval info type_unwrap_reference(info type);
  consteval info type_unwrap_ref_decay(info type);
}
```
:::
:::

### [meta.reflection.names] Reflection names and locations {-}

::: std
::: addu
```cpp
consteval string_view name_of(info r);
consteval u8string_view u8name_of(info r);
```

[#]{.pnum} *Mandates*: If returning `string_view`, the unqualified name is representable using the ordinary string literal encoding.

[#]{.pnum} *Returns*: If `r` designates a declared entity `X`, then the unqualified name of `X`. Otherwise, an empty `string_view` or `u8string_view`, respectively.

```cpp
consteval string_view qualified_name_of(info r);
consteval u8string_view u8qualified_name_of(info r);
```

[#]{.pnum} *Mandates*: If returning `string_view`, the qualified name is representable using the ordinary string literal encoding.

[#]{.pnum} *Returns*: If `r` designates a declared entity `X`, then the qualified name of `X`. Otherwise, an empty `string_view` or `u8string_view`, respectively.

```cpp
consteval string_view display_name_of(info r);
consteval u8string_view u8display_name_of(info r);
```

[#]{.pnum} *Mandates*: If returning `string_view`, the implementation-defined name is representable using the ordinary string literal encoding.

[#]{.pnum} *Returns*: An implementation-defined `string_view` or `u8string_view`, respectively, suitable for identifying the reflected construct.

```cpp
consteval source_location source_location_of(info r);
```

[#]{.pnum} *Returns*: An implementation-defined `source_location` corresponding to the reflected construct.
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

[#]{.pnum} *Returns*: `true` if `r` designates a class member or base class that is public, protected, or private, respectively. Otherwise, `false`.

```cpp
consteval bool is_virtual(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a either a virtual member function or a virtual base class. Otherwise, `false`.

```cpp
consteval bool is_pure_virtual(info r);
consteval bool is_override(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a member function that is pure virtual or overrides another member function, respectively. Otherwise, `false`.

```cpp
consteval bool is_deleted(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a function that is defined as deleted. Otherwise, `false`.

```cpp
consteval bool is_defaulted(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a function that is defined as defaulted. Otherwise, `false`.

```cpp
consteval bool is_explicit(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a member function that is declared explicit. Otherwise, `false`.

```cpp
consteval bool is_noexcept(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a `noexcept` function type, a pointer to `noexcept` function or member function type, a closure type of a non-generic lambda whose call operator is declared `noexcept`, a value of any of the previously mentioned types, or a function that is declared `noexcept`. Otherwise, `false`.

```cpp
consteval bool is_bit_field(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a bit-field. Otherwise, `false`.

```cpp
consteval bool is_const(info r);
consteval bool is_volatile(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a const or volatile type (respectively), a const- or volatile-qualified function type (respectively), or an object, variable, non-static data member, or function with such a type. Otherwise, `false`.

```cpp
consteval bool is_final(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a final class or a final member function. Otherwise, `false`.

```cpp
consteval bool has_static_storage_duration(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates an object or variable that has static storage duration. Otherwise, `false`.

```cpp
consteval bool has_internal_linkage(info r);
consteval bool has_module_linkage(info r);
consteval bool has_external_linkage(info r);
consteval bool has_linkage(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates an entity that has internal linkage, module linkage, external linkage, or any linkage, respectively ([basic.link]). Otherwise, `false`.


```cpp
consteval bool is_namespace(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a namespace or namespace alias. Otherwise, `false`.

```cpp
consteval bool is_function(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a function or member function. Otherwise, `false`.

```cpp
consteval bool is_variable(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a variable. Otherwise, `false`.

```cpp
consteval bool is_type(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a type or a type alias. Otherwise, `false`.

```cpp
consteval bool is_alias(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a type alias, alias template, or namespace alias. Otherwise, `false`.

```cpp
consteval bool is_incomplete_type(info r);
```
[#]{.pnum} *Mandates*: `r` is a reflection designating a type.

[#]{.pnum} *Effects*: If `dealias(r)` designates a class template specialization with a reachable definition, the specialization is instantiated.

[#]{.pnum} *Returns*: `true` if the type designated by `dealias(r)` is an incomplete type ([basic.types]). Otherwise, `false`.

```cpp
consteval bool is_template(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a function template, class template, variable template, or alias template. Otherwise, `false`.

[#]{.pnum} [A template specialization is not a template. `is_template(^std::vector)` is `true` but `is_template(^std::vector<int>)` is `false`.]{.note}

```cpp
consteval bool is_function_template(info r);
consteval bool is_variable_template(info r);
consteval bool is_class_template(info r);
consteval bool is_alias_template(info r);
consteval bool is_concept(info r);
consteval bool is_structured_binding(info r);
consteval bool is_value(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a function template, class template, variable template, alias template, concept, structured binding, or value respectively. Otherwise, `false`.

```cpp
consteval bool is_object(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates an object. Otherwise, `false`.

```cpp
consteval bool has_template_arguments(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates an instantiation of a function template, variable template, class template, or an alias template. Otherwise, `false`.


```cpp
consteval bool is_class_member(info r);
consteval bool is_namespace_member(info r);
consteval bool is_nonstatic_data_member(info r);
consteval bool is_static_member(info r);
consteval bool is_base(info r);
consteval bool is_constructor(info r);
consteval bool is_destructor(info r);
consteval bool is_special_member(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a class member, namespace member, non-static data member, static member, base class member, constructor, destructor, or special member, respectively. Otherwise, `false`.

```cpp
consteval bool is_user_provided(info r);
```

[#]{.pnum} *Mandates*: `r` designates a function.

[#]{.pnum} *Returns*: `true` if `r` designates a user-provided ([dcl.fct.def.default]{.sref}) function. Otherwise, `false`.

```cpp
consteval info type_of(info r);
```

[#]{.pnum} *Mandates*: `r` designates a typed entity. `r` does not designate a constructor, destructor, or structured binding.

[#]{.pnum} *Returns*: A reflection of the type of that entity.  If every declaration of that entity was declared with the same type alias (but not a template parameter substituted by a type alias), the reflection returned is for that alias.  Otherwise, if some declaration of that entity was declared with an alias it is unspecified whether the reflection returned is for that alias or for the type underlying that alias. Otherwise, the reflection returned shall not be a type alias reflection.

```cpp
consteval info object_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating either an object or a variable denoting an object with static storage duration ([expr.const]).

[#]{.pnum} *Returns*: If `r` is a reflection of a variable, then a reflection of the object denoted by the variable. Otherwise, `r`.

```cpp
consteval info value_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating either an object or variable usable in constant expressions ([expr.const]), an enumerator, or a value.

[#]{.pnum} *Returns*: If `r` is a reflection of an object `o`, or a reflection of a variable which designates an object `o`, then a reflection of the value held by `o`. The reflected value has type `dealias(type_of(o))`, with the cv-qualifiers removed if this is a scalar type. Otherwise, if `r` is a reflection of an enumerator, then a reflection of the value of the enumerator. Otherwise, `r`.

```cpp
consteval info parent_of(info r);
```

[#]{.pnum} *Mandates*: `r` designates a member of either a class or a namespace.

[#]{.pnum} *Returns*: A reflection of that entity's immediately enclosing class or namespace.

```cpp
consteval info dealias(info r);
```

[#]{.pnum} *Returns*: If `r` designates a type alias or a namespace alias, a reflection designating the underlying entity. Otherwise, `r`.

[#]{.pnum}

::: example
```
using X = int;
using Y = X;
static_assert(dealias(^int) == ^int);
static_assert(dealias(^X) == ^int);
static_assert(dealias(^Y) == ^int);
```
:::

```cpp
consteval info template_of(info r);
consteval vector<info> template_arguments_of(info r);
```
[#]{.pnum} *Mandates*: `has_template_arguments(r)` is `true`.

[#]{.pnum} *Returns*: A reflection of the template of `r`, and the reflections of the template arguments of the specialization designated by `r`, respectively.

[#]{.pnum}

::: example
```
template <class T, class U=T> struct Pair { };
template <class T> using PairPtr = Pair<T*>;

static_assert(template_of(^Pair<int>) == ^Pair);
static_assert(template_arguments_of(^Pair<int>).size() == 2);

static_assert(template_of(^PairPtr<int>) == ^PairPtr);
static_assert(template_arguments_of(^PairPtr<int>).size() == 1);
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

[#]{.pnum} *Mandates*: `r` is a reflection designating either a complete class type or a namespace.

[#]{.pnum} *Effects*: If `dealias(r)` designates a class template specialization with a reachable definition, the specialization is instantiated.

[#]{.pnum} *Returns*: A `vector` containing the reflections of all the direct members `m` of the entity, excluding any structured bindings, designated by `r`.
Non-static data members are indexed in the order in which they are declared, but the order of other kinds of members is unspecified. [Base classes are not members.]{.note}

```cpp
consteval vector<info> bases_of(info type);
```

[#]{.pnum} *Mandates*: `type` is a reflection designating a complete class type.

[#]{.pnum} *Effects*: If `dealias(type)` designates a class template specialization with a reachable definition, the specialization is instantiated.

[#]{.pnum} *Returns*: Let `C` be the type designated by `type`. A `vector` containing the reflections of all the direct base classes `b`, if any, of `C`.
The base classes are indexed in the order in which they appear in the *base-specifier-list* of `C`.

```cpp
consteval vector<info> static_data_members_of(info type);
```

[#]{.pnum} *Returns*: `members_of(type) | views::filter(is_variable) | ranges::to<vector>()`

```cpp
consteval vector<info> nonstatic_data_members_of(info type);
```

[#]{.pnum} *Returns*: `members_of(type) | views::filter(is_nonstatic_data_member) | ranges::to<vector>()`

```cpp
consteval vector<info> subobjects_of(info type);
```

[#]{.pnum} *Mandates*: `type` is a reflection designating a complete class type.

[#]{.pnum} *Effects*: If `dealias(type)` designates a class template specialization with a reachable definition, the specialization is instantiated.

[#]{.pnum} *Returns*: A `vector` containing all the reflections in `bases_of(type)` followed by all the reflections in `nonstatic_data_members_of(type)`.

```cpp
consteval vector<info> enumerators_of(info type_enum);
```

[#]{.pnum} *Mandates*: `type_enum` is a reflection designating an enumeration.

[#]{.pnum} *Returns*: A `vector` containing the reflections of each enumerator of the enumeration designated by `type_enum`, in the order in which they are declared.
:::
:::

### [meta.reflection.member.access], Reflection member access queries

::: std
::: addu
```cpp
consteval info access_context();
```

[#]{.pnum} *Returns*: A reflection of the function, class, or namespace scope most nearly enclosing the function call.

```cpp
consteval bool is_accessible(access_pair p);
```

[#]{.pnum} *Mandates*: `p.target` is a reflection designating a member of a class. `p.from` designates a function, class, or namespace.

[#]{.pnum} *Returns*: `true` if the class member designated by `p.target` can be named within the scope of `p.from`. Otherwise, `false`.

```cpp
consteval bool is_accessible(info target, info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return is_accessible({target, from});`

```cpp
consteval vector<info> accessible_members_of(access_pair p);
```

[#]{.pnum} *Mandates*: `p.target` is a reflection designating a complete class type. `p.from` designates a function, class, or namespace.

[#]{.pnum} *Returns*: `
```cpp
members_of(p.target)
| views::filter([&](info r) { return is_accessible({r, p.from}); })
| ranges::to<vector>()
```

```cpp
consteval vector<info> accessible_members_of(info target,
                                             info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return accessible_members_of({target, from});`

```cpp
consteval vector<info> accessible_bases_of(access_pair p);
```

[#]{.pnum} *Mandates*: `p.target` is a reflection designating a complete class type. `p.from` designates a function, class, or namespace.

[#]{.pnum} *Returns*:
```cpp
bases_of(p.target)
| views::filter([&](info r) { return is_accessible({r, p.from}); })
| ranges::to<vector>()
```

```cpp
consteval vector<info> accessible_bases_of(info target,
                                           info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return accessible_bases_of({target, from});`

```cpp
consteval vector<info> accessible_nonstatic_data_members_of(access_pair p);
```

[#]{.pnum} *Returns*: Equivalent to:
```cpp
return accessible_members_of(p)
| views::filter(is_nonstatic_data_member)
| ranges::to<vector>()
```

```cpp
consteval vector<info> accessible_nonstatic_data_members_of(info target,
                                                            info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return accessible_nonstatic_data_members_of({target, from});`

```cpp
consteval vector<info> accessible_static_data_members_of(access_pair p);
```

[#]{.pnum} *Returns*:
```cpp
accessible_members_of(p)
| views::filter(is_static_data_member)
| ranges::to<vector>()
```

```cpp
consteval vector<info> accessible_static_data_members_of(info target,
                                                         info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return accessible_static_data_members_of({target, from});`

```cpp
consteval vector<info> accessible_subobjects_of(access_pair p);
```

[#]{.pnum} *Returns*: A `vector` containing all the reflections in `accessible_bases_of(p)` followed by all the reflections in `accessible_nonstatic_data_members_of(p)`.

```cpp
consteval vector<info> accessible_subobjects_of(info target, info from);
```

[#]{.pnum} *Effects*: Equivalent to: `return accessible_subobjects_data_members_of({target, from});`

:::
:::

### [meta.reflection.layout] Reflection layout queries {-}

::: std
::: addu
```cpp
consteval size_t offset_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating a non-static data member or non-virtual base class.

[#]{.pnum} *Returns*: The offset in bytes from the beginning of an object of type `parent_of(r)` to the subobject associated with the entity reflected by `r`.

```cpp
consteval size_t size_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection of a type, non-static data member, base class, object, value, or variable.

[#]{.pnum} *Returns* If `r` designates a type `T`, then `sizeof(T)`. Otherwise, `size_of(type_of(r))`.

```cpp
consteval size_t alignment_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating an object, variable, type, non-static data member, or base class.

[#]{.pnum} *Returns*: If `r` designates a type, object, or variable, then the alignment requirement of the entity. Otherwise, if `r` designates a base class, then `alignment_of(type_of(r))`. Otherwise, the alignment requirement of the subobject associated with the reflected non-static data member within any object of type `parent_of(r)`.

```cpp
consteval size_t bit_offset_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating a non-static data member or a non-virtual base class.

[#]{.pnum} Let `V` be the offset in bits from the beginning of an object of type `parent_of(r)` to the subobject associated with the entity reflected by `r`.

[#]{.pnum} *Returns*: `V - offset_of(r)`.

```cpp
consteval size_t bit_size_of(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection of a type, non-static data member, base class, object, value, or variable.

[#]{.pnum} *Returns* If `r` designates a type, then the size in bits of any object having the reflected type. Otherwise, if `r` reflects a non-static data member that is a bit-field, then the width of the reflected bit-field. Otherwise, `bit_size_of(type_of(r))`.
:::
:::


### [meta.reflection.extract] Value extraction {-}

::: std
::: addu
```cpp
template <class T>
  consteval T extract(info r);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating a value, object, variable, function, enumerator, or non-static data member that is not a bit-field. If `r` reflects a value or enumerator, then `T` is not a reference type. If `r` reflects a value or enumerator of type `U`, or if `r` reflects a variable or object of non-reference type `U`, then the cv-unqualified types of `T` and `U` are the same. If `r` reflects a variable, object, or function with type `U`, and `T` is a reference type, then the cv-unqualified types of `T` and `U` are the same, and `T` is either `U` or more cv-qualified than `U`. If `r` reflects a non-static data member, or if `r` reflects a function and `T` is a reference type, then the statement `T v = &expr`, where `expr` is an lvalue naming the entity designated by `r`, is well-formed.

[#]{.pnum} *Returns*: If `r` designates a value or enumerator, then the entity reflected by `r`. Otherwise, if `r` reflects an object, variable, or enumerator and `T` is not a reference type, then the result of an lvalue-to-rvalue conversion applied to an expression naming the entity reflected by `r`. Otherwise, if `r` reflects an object, variable, or function and `T` is a reference type, then the result of an lvalue naming the entity reflected by `r`. Otherwise, if `r` reflects a function or non-static data member, then a pointer value designating the entity reflected by `r`.
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
template <reflection_range R = span<info const>>
consteval bool can_substitute(info templ, R&& arguments);
```
[1]{.pnum} *Mandates*: `templ` designates a template.

[#]{.pnum} Let `Z` be the template designated by `templ` and let `Args...` be the sequence of entities or aliases designated by the elements of `arguments`.

[#]{.pnum} *Returns*: `true` if `Z<Args...>` is a valid *template-id* ([temp.names]). Otherwise, `false`.

[#]{.pnum} *Remarks*: If attempting to substitute leads to a failure outside of the immediate context, the program is ill-formed.

```cpp
template <reflection_range R = span<info const>>
consteval info substitute(info templ, R&& arguments);
```

[#]{.pnum} *Mandates*: `can_substitute(templ, arguments)` is `true`.

[#]{.pnum} Let `Z` be the template designated by `templ` and let `Args...` be the sequence of entities or aliases designated by the elements of `arguments`.

[#]{.pnum} *Returns*: `^Z<Args...>`.

:::
:::

### [meta.reflection.result] Expression result reflection {-}

::: std
::: addu
```cpp
template <typename T>
  consteval info reflect_value(T expr);
```

[#]{.pnum} *Mandates*: `T` is a structural type that is not a reference type. Any subobject of the value computed by `expr` having reference or pointer type designates an entity that is a permitted result of a constant expression ([expr.const]).

[#]{.pnum} *Returns*: A reflection of the value computed by an lvalue-to-rvalue conversion applied to `expr`. The type of the reflected value is the cv-unqualified version of `T`.

```cpp
template <typename T>
  consteval info reflect_object(T& expr);
```

[#]{.pnum} *Mandates*: `T` is not a function type. `expr` designates an entity that is a permitted result of a constant expression.

[#]{.pnum} *Returns*: A reflection of the object referenced by `expr`.

```cpp
template <typename T>
  consteval info reflect_function(T& expr);
```

[#]{.pnum} *Mandates*: `T` is a function type.

[#]{.pnum} *Returns*: `^fn`, where `fn` is the function referenced by `expr`.

```cpp
template <reflection_range R = span<info const>>
  consteval info reflect_invoke(info target, R&& args);
template <reflection_range R1 = span<info const>, reflection_range R2 = span<info const>>
  consteval info reflect_invoke(info target, R1&& tmpl_args, R2&& args);
```

[#]{.pnum} Let `F` be the entity reflected by `target`, let `Arg0` be the entity reflected by the first element of `args` (if any), let `Args...` be the sequence of entities reflected by the elements of `args` excluding the first, and let `TArgs...` be the sequence of entities or aliases designated by the elements of `tmpl_args`.

[#]{.pnum} If `F` is a non-member function, a value of pointer to function type, a value of pointer to member type, or a value of closure type, then let `INVOKE-EXPR` be the expression `INVOKE(F, Arg0, Args...)`. Otherwise, if `F` is a member function, then let `INVOKE-EXPR` be the expression `Arg0.F(Args...)`. Otherwise, if `F` is a constructor for a class `C`, then let `INVOKE-EXPR` be the expression `C(Arg0, Args...)` for which only the constructor `F` is considered by overload resolution. Otherwise, if `F` is a non-member function template or a member function template, then let `INVOKE-EXPR` be the expression `F<TArgs...>(Arg0, Args...)` or `Arg0.template F<TArgs...>(Args...)` respectively. Otherwise, if `F` is a constructor template, then let `INVOKE-EXPR` be the expression `C(Arg0, Args...)` for which only the constructor `F` is considered by overload resolution, and `TArgs...` are inferred as explicit template arguments for `F`.

[#]{.pnum} *Mandates*: `target` designates a reflection of a function, a constructor, a constructor template, a value, or a function template. If `target` reflects a value of type `T`, then `T` is a pointer to function type, pointer to member type, or closure type. The expression `INVOKE-EXPR` is a well-formed constant expression of structural type.

[#]{.pnum} *Returns*: A reflection of the result of the expression `INVOKE-EXPR`.

:::
:::

### [meta.reflection.define_class] Reflection class definition generation  {-}

::: std
::: addu

```cpp
consteval info data_member_spec(info type,
                                data_member_options_t options = {});
```
[1]{.pnum} *Mandates*:
`type` designates a type.
If `options.name` contains a value, the `string` or `u8string` value that was used to initialize `options.name` contains a valid identifier ([lex.name]{.sref}).


[#]{.pnum} *Returns*: A reflection of a description of the declaration of non-static data member with a type designated by `type` and optional characteristics designated by `options`.

[#]{.pnum} *Remarks*: The reflection value being returned is only useful for consumption by `define_class`.  No other function in `std::meta` recognizes such a value.


```c++
  template <reflection_range R = span<info const>>
  consteval info define_class(info class_type, R&&  mdescrs);
```

[#]{.pnum} Let `@*d*~1~@`, `@*d*~2~@`, ..., `@*d*~N~@` denote the reflection values of the range `mdescrs` obtained by calling `data_member_spec` with `type` values `@*t*~1~@`, `@*t*~2~@`, ... `@*t*~N~@` and `option` values `@*o*~1~@`, `@*o*~2~@`, ... `@*o*~N~@` respectively.  

[#]{.pnum} *Mandates*:
`class_type` designates an incomplete class type.  `mdescrs` is a (possibly empty) range of reflection values obtained by calls to `data_member_spec`.
[For example, `class_type` could be a specialization of a class template that has not been instantiated or explicitly specialized.]{.note}
Each `@*t*~i~@` designates a type that is valid types for data members.
If `@*o*~K~@.width` (for some `$K$`) contains a value `$w$`, the corresponding type `@*t*~K~@` is a valid type for bit field of width `$w$`.
If `@*o*~K~@.alignment` (for some `$K$`) contains a value `$a$`, `alignas($a$)` is a valid `$alignment-specifier$` for a non-static data member of type `@*t*~K~@`.


[#]{.pnum} *Effects*:
Defines `class_type` with properties as follows:

* [#.1]{.pnum} If `class_type` designates a specialization of a class template, the specialization is explicitly specialized.
* [#.#]{.pnum} Non-static data members are declared in the definition of `class_type` according to `@*d*~1~@`, `@*d*~2~@`, ..., `@*d*~N~@`, in that order.
* [#.#]{.pnum} The type of the respective members are the types denoted by the reflection values `@*t*~1~@`, `@*t*~2~@`, ... `@*t*~N~@`.
* [#.#]{.pnum} If `@*o*~K~@.no_unique_address` (for some `$K$`) is `true`, the corresponding member is declared with attribute `[[no_unique_address]]`.
* [#.#]{.pnum} If `@*o*~K~@.width` (for some `$K$`) contains a value, the corresponding member is declared as a bit field with that value as its width.
* [#.#]{.pnum} If `@*o*~K~@.alignment` (for some `$K$`) contains a value `$a$`, the corresponding member is aligned as if declared with `alignas($a$)`.
* [#.#]{.pnum} If `@*o*~K~@.name` (for some `$K$`) does not contain a value, the corresponding member is declared with an implementation-defined name.
  Otherwise, the corresponding member is declared with a name corresponding to the `string` or `u8string` value that was used to initialize `@*o*~K~@.name`.
* [#.#]{.pnum} If `class_type` is a union type and any of its members is not trivially default constructible, then it has a default constructor that is user-provided and has no effect.
  If `class_type` is a union type and any of its members is not trivially default destructible, then it has a default destructor that is user-provided and has no effect.


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
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$TRAIT$` defined in this clause, `std::meta::type_$TRAIT$(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.cat]{.sref}.

```cpp
consteval bool type_is_void(info type);
consteval bool type_is_null_pointer(info type);
consteval bool type_is_integral(info type);
consteval bool type_is_floating_point(info type);
consteval bool type_is_array(info type);
consteval bool type_is_pointer(info type);
consteval bool type_is_lvalue_reference(info type);
consteval bool type_is_rvalue_reference(info type);
consteval bool type_is_member_object_pointer(info type);
consteval bool type_is_member_function_pointer(info type);
consteval bool type_is_enum(info type);
consteval bool type_is_union(info type);
consteval bool type_is_class(info type);
consteval bool type_is_function(info type);
consteval bool type_is_reflection(info type);
```

[2]{.pnum}

::: example
```
namespace std::meta {
  consteval bool type_is_void(info type) {
    // one example implementation
    return extract<bool>(substitute(^is_void_v, {type}));

    // another example implementation
    type = dealias(type);
    return type == ^void
        || type == ^const void
        || type == ^volatile void
        || type == ^const volatile void;
  }
}
```
:::
:::
:::

#### [meta.reflection.unary.comp] Composite type categories  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$TRAIT$` defined in this clause, `std::meta::type_$TRAIT$(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.comp]{.sref}.

```cpp
consteval bool type_is_reference(info type);
consteval bool type_is_arithmetic(info type);
consteval bool type_is_fundamental(info type);
consteval bool type_is_object(info type);
consteval bool type_is_scalar(info type);
consteval bool type_is_compound(info type);
consteval bool type_is_member_pointer(info type);
```
:::
:::

#### [meta.reflection.unary.prop] Type properties  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$UNARY-TRAIT$` defined in this clause with signature `bool(std::meta::info)`, `std::meta::type_$UNARY-TRAIT$(^T)` equals the value of the corresponding type property `std::$UNARY-TRAIT$_v<T>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any types or type aliases `T` and `U`, for each function `std::meta::type_$BINARY-TRAIT$` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::type_$BINARY-TRAIT$(^T, ^U)` equals the value of the corresponding type property `std::$BINARY-TRAIT$_v<T, U>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::to<vector>(r) == vector{^U...}` is `true`, for each function template `std::meta::type_$VARIADIC-TRAIT$` defined in this clause, `std::meta::type_$VARIADIC-TRAIT$(^T, r)` equals the value of the corresponding type property `std::$VARIADIC-TRAIT$_v<T, U...>` as specified in [meta.unary.prop]{.sref}.

```cpp
consteval bool type_is_const(info type);
consteval bool type_is_volatile(info type);
consteval bool type_is_trivial(info type);
consteval bool type_is_trivially_copyable(info type);
consteval bool type_is_standard_layout(info type);
consteval bool type_is_empty(info type);
consteval bool type_is_polymorphic(info type);
consteval bool type_is_abstract(info type);
consteval bool type_is_final(info type);
consteval bool type_is_aggregate(info type);
consteval bool type_is_signed(info type);
consteval bool type_is_unsigned(info type);
consteval bool type_is_bounded_array(info type);
consteval bool type_is_unbounded_array(info type);
consteval bool type_is_scoped_enum(info type);

template <reflection_range R = span<info const>>
consteval bool type_is_constructible(info type, R&& type_args);
consteval bool type_is_default_constructible(info type);
consteval bool type_is_copy_constructible(info type);
consteval bool type_is_move_constructible(info type);

consteval bool type_is_assignable(info type_dst, info type_src);
consteval bool type_is_copy_assignable(info type);
consteval bool type_is_move_assignable(info type);

consteval bool type_is_swappable_with(info type_dst, info type_src);
consteval bool type_is_swappable(info type);

consteval bool type_is_destructible(info type);

template <reflection_range R = span<info const>>
consteval bool type_is_trivially_constructible(info type, R&& type_args);
consteval bool type_is_trivially_default_constructible(info type);
consteval bool type_is_trivially_copy_constructible(info type);
consteval bool type_is_trivially_move_constructible(info type);

consteval bool type_is_trivially_assignable(info type_dst, info type_src);
consteval bool type_is_trivially_copy_assignable(info type);
consteval bool type_is_trivially_move_assignable(info type);
consteval bool type_is_trivially_destructible(info type);

template <reflection_range R = span<info const>>
consteval bool type_is_nothrow_constructible(info type, R&& type_args);
consteval bool type_is_nothrow_default_constructible(info type);
consteval bool type_is_nothrow_copy_constructible(info type);
consteval bool type_is_nothrow_move_constructible(info type);

consteval bool type_is_nothrow_assignable(info type_dst, info type_src);
consteval bool type_is_nothrow_copy_assignable(info type);
consteval bool type_is_nothrow_move_assignable(info type);

consteval bool type_is_nothrow_swappable_with(info type_dst, info type_src);
consteval bool type_is_nothrow_swappable(info type);

consteval bool type_is_nothrow_destructible(info type);

consteval bool type_is_implicit_lifetime(info type);

consteval bool type_has_virtual_destructor(info type);

consteval bool type_has_unique_object_representations(info type);

consteval bool type_reference_constructs_from_temporary(info type_dst, info type_src);
consteval bool type_reference_converts_from_temporary(info type_dst, info type_src);
```
:::
:::

#### [meta.reflection.unary.prop.query] Type property queries  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$PROP$` defined in this clause with signature `size_t(std::meta::info)`, `std::meta::type_$PROP$(^T)` equals the value of the corresponding type property `std::$PROP$_v<T>` as specified in [meta.unary.prop.query]{.sref}.

[#]{.pnum} For any type or type alias `T` and unsigned integer value `I`, `std::meta::type_extent(^T, I)` equals `std::extent_v<T, I>` ([meta.unary.prop.query]).

```cpp
consteval size_t type_alignment_of(info type);
consteval size_t type_rank(info type);
consteval size_t type_extent(info type, unsigned i = 0);
```
:::
:::

### [meta.reflection.rel], Type relations  {-}

::: std
::: addu
[1]{.pnum} The consteval functions specified in this clause may be used to query relationships between types at compile time.

[#]{.pnum} For any types or type aliases `T` and `U`, for each function `std::meta::type_$REL$` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::type_$REL$(^T, ^U)` equals the value of the corresponding type relation `std::$REL$_v<T, U>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::to<vector>(r) == vector{^U...}` is `true`, for each binary function template `std::meta::type_$VARIADIC-REL$`, `std::meta::type_$VARIADIC-REL$(^T, r)` equals the value of the corresponding type relation `std::$VARIADIC-REL$_v<T, U...>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any types or type aliases `T` and `R`, pack of types or type aliases `U...`, and range `r` such that `ranges::to<vector>(r) == vector{^U...}` is `true`, for each ternary function template `std::meta::type_$VARIADIC-REL-R$` defined in this clause, `std::meta::type_$VARIADIC-REL-R$(^R, ^T, r)` equals the value of the corresponding type relation `std::$VARIADIC-REL-R$_v<R, T, U...>` as specified in [meta.rel]{.sref}.

```cpp
consteval bool type_is_same(info type1, info type2);
consteval bool type_is_base_of(info type_base, info type_derived);
consteval bool type_is_convertible(info type_src, info type_dst);
consteval bool type_is_nothrow_convertible(info type_src, info type_dst);
consteval bool type_is_layout_compatible(info type1, info type2);
consteval bool type_is_pointer_interconvertible_base_of(info type_base, info type_derived);

template <reflection_range R = span<info const>>
consteval bool type_is_invocable(info type, R&& type_args);
template <reflection_range R = span<info const>>
consteval bool type_is_invocable_r(info type_result, info type, R&& type_args);

template <reflection_range R = span<info const>>
consteval bool type_is_nothrow_invocable(info type, R&& type_args);
template <reflection_range R = span<info const>>
consteval bool type_is_nothrow_invocable_r(info type_result, info type, R&& type_args);
```

[#]{.pnum} [If `t` is a reflection of the type `int` and `u` is a reflection of an alias to the type `int`, then `t == u` is `false` but `type_is_same(t, u)` is `true`. `t == dealias(u)` is also `true`.]{.note}.
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
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.cv]{.sref}.

```cpp
consteval info type_remove_const(info type);
consteval info type_remove_volatile(info type);
consteval info type_remove_cv(info type);
consteval info type_add_const(info type);
consteval info type_add_volatile(info type);
consteval info type_add_cv(info type);
```
:::
:::

#### [meta.reflection.trans.ref], Reference modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ref]{.sref}.

```cpp
consteval info type_remove_reference(info type);
consteval info type_add_lvalue_reference(info type);
consteval info type_add_rvalue_reference(info type);
```
:::
:::

#### [meta.reflection.trans.sign], Sign modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.sign]{.sref}.
```cpp
consteval info type_make_signed(info type);
consteval info type_make_unsigned(info type);
```
:::
:::

#### [meta.reflection.trans.arr], Array modifications  {-}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.arr]{.sref}.
```cpp
consteval info type_remove_extent(info type);
consteval info type_remove_all_extents(info type);
```
:::
:::

#### [meta.reflection.trans.ptr], Pointer modifications  {-}
::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ptr]{.sref}.
```cpp
consteval info type_remove_pointer(info type);
consteval info type_add_pointer(info type);
```
:::
:::

#### [meta.reflection.trans.other], Other transformations  {-}

[There are four transformations that are deliberately omitted here. `type_identity` and `enable_if` are not useful, `conditional(cond, t, f)` would just be a long way of writing `cond ? t : f`, and `basic_common_reference` is a class template intended to be specialized and not directly invoked.]{.ednote}

::: std
::: addu
[1]{.pnum} For any type or type alias `T`, for each function `std::meta::type_$MOD$` defined in this clause with signature `std::meta::info(std::meta::info)`, `std::meta::type_$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any pack of types or type aliases `T...` and range `r` such that `ranges::to<vector>(r) == vector{^T...}` is `true`, for each unary function template `std::meta::type_$VARIADIC-MOD$` defined in this clause, `std::meta::type_$VARIADIC-MOD$(r)` returns the reflection of the corresponding type `std::$VARIADIC-MOD$_t<T...>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any type or type alias `T`, pack of types or type aliases `U...`, and range `r` such that `ranges::to<vector>(r) == vector{^U...}` is `true`, `std::meta::type_invoke_result(^T, r)` returns the reflection of the corresponding type `std::invoke_result_t<T, U...>` ([meta.trans.other]{.sref}).

```cpp
consteval info type_remove_cvref(info type);
consteval info type_decay(info type);
template <reflection_range R = span<info const>>
consteval info type_common_type(R&& type_args);
template <reflection_range R = span<info const>>
consteval info type_common_reference(R&& type_args);
consteval info type_underlying_type(info type);
template <reflection_range R = span<info const>>
consteval info type_invoke_result(info type, R&& type_args);
consteval info type_unwrap_reference(info type);
consteval info type_unwrap_ref_decay(info type);
```

[#]{.pnum}

::: example
```cpp
// example implementation
consteval info type_unwrap_reference(info type) {
  type = dealias(type);
  if (has_template_arguments(type) && template_of(type) == ^reference_wrapper) {
    return type_add_lvalue_reference(template_arguments_of(type)[0]);
  } else {
    return type;
  }
}
```
:::

:::
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
+ __cpp_impl_reflection 2024XXL
```
:::

and [version.syn]{.sref}:

::: std
```diff
+ #define __cpp_lib_reflection 2024XXL // also in <meta>
```
:::

---
references:
  - id: P1306R2
    citation-label: P1306R2
    title: "Expansion statements"
    author:
      - family: Andrew Sutton
      - family: Sam Goodrick
      - family: Daveed Vandevoorde
      - family: Dan Katz
    issued:
      - year: 2024
        month: 05
        day: 07
    URL: https://wg21.link/p1306r2
  - id: P3096R1
    citation-label: P3096R1
    title: "Function Parameter Reflection in Reflection for C++26"
    author:
      - family: Adam Lach
      - family: Walter Genovese
    issued:
      - year: 2024
        month: 04
        day: 29
    URL: https://wg21.link/p3096r1
  - id: P3068R1
    citation-label: P3068R1
    title: "Allowing exception throwing in constant-evaluation"
    author:
      - family: Hana Dusíková
    issued:
      - year: 2024
        month: 03
        day: 18
    URL: https://wg21.link/p3293r0
  - id: P3293R0
    citation-label: P3293R0
    title: "Splicing a base class subobject"
    author:
      - family: Peter Dimov
      - family: Dan Katz
      - family: Barry Revzin
      - family: Daveed Vandevoorde
    issued:
      - year: 2024
        month: 05
        day: 19
    URL: https://wg21.link/p3293r0
  - id: P3295R0
    citation-label: P3295R0
    title: "Freestanding constexpr containers and constexpr exception types"
    author:
      - family: Ben Craig
    issued:
      - year: 2024
        month: 05
        day: 18
    URL: https://wg21.link/p3295r0
---
