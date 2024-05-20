---
title: "Reflection for C++26"
document: D2996R3
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

Since [@P2996R2]:

* many wording changes, additions, and improvements
* added `accessible_members_of` variants to restore a TS-era agreement
* expanded `value_of` to operate on functions
* added Godbolt links to Clang/P2996 implementation
* added `can_substitute`
* added explanation of a naming issue with the [type traits](#other-type-traits)
* added an alternative [named tuple](#named-tuple) implementation
* made default/value/zero-initializing a `meta::info` yield a null reflection
* added addressed splicing, which is implemented but was omitted from the paper
* added another overload to `reflect_invoke` to support template arguments
* renamed all the type traits to end in `_type` to avoid name clashes

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
For example, we added a `std::meta::test_type` interface that makes it convenient to use existing standard type predicates (such as `is_class_v`) in reflection computations.

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

Nearly all of the examples below have links to Compiler Explorer demonstrating them in both the EDG and Clang.

Neither implementation is complete (notably, splicing of templates has not yet been implemented), both have their "quirks", and both will evolve alongside this paper.
They also lack some of the other proposed language features that dovetail well with reflection; most notably, expansion statements are absent.
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

* expansion statements [@P1306R1]
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

:::bq
```c++
using MyType = [:sizeof(int)<sizeof(long)? ^long : ^int:];  // Implicit "typename" prefix.
```
:::

On Compiler Explorer: [EDG](https://godbolt.org/z/13anqE1Pa), [Clang](https://godbolt.org/z/zn4vnjqzb).


## Selecting Members

Our second example enables selecting a member "by number" for a specific type:

:::bq
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

On Compiler Explorer: [EDG](https://godbolt.org/z/WEYae451z), [Clang](https://godbolt.org/z/dhrdd14P1).

Note that a "member access splice" like `s.[:member_number(1):]` is a more direct member access mechanism than the traditional syntax.
It doesn't involve member name lookup, access checking, or --- if the spliced reflection value denotes a member function --- overload resolution.

This proposal includes a number of consteval "metafunctions" that enable the introspection of various language constructs.
Among those metafunctions is `std::meta::nonstatic_data_members_of` which returns a vector of reflection values that describe the nonstatic members of a given type.
We could thus rewrite the above example as:

:::bq
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
With such a facility, we could conceivably access nonstatic data members "by string":

:::bq
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

On Compiler Explorer: [EDG](https://godbolt.org/z/Yhh5hbcrn), [Clang](https://godbolt.org/z/nYvc9ddr1).


## List of Types to List of Sizes

Here, `sizes` will be a `std::array<std::size_t, 3>` initialized with `{sizeof(int), sizeof(float), sizeof(double)}`:

::: std
```c++
constexpr std::array types = {^int, ^float, ^double};
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

On Compiler Explorer: [EDG](https://godbolt.org/z/4xz9Wsa8f), [Clang](https://godbolt.org/z/nnrrYTTW9).

## Implementing `make_integer_sequence`

We can provide a better implementation of `make_integer_sequence` than a hand-rolled approach using regular template metaprogramming (although standard libraries today rely on an intrinsic for this):

:::bq
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
          return std::pair<E, std::string>(std::meta::value_of<E>(e), std::meta::name_of(e));
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

On Compiler Explorer: [EDG](https://godbolt.org/z/Y5va8MqzG), [Clang](https://godbolt.org/z/Kfqc77rMq).


Many many variations of these functions are possible and beneficial depending on the needs of the client code.
For example:

  - the "<unnamed>" case could instead output a valid cast expression like "E(5)"
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

On Compiler Explorer: [EDG](https://godbolt.org/z/G4dh3jq8a), [Clang](https://godbolt.org/z/xae9n6z5G).


## A Simple Tuple Type

:::bq
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
`define_class` takes a reflection for an incomplete class or union plus a vector of nonstatic data member descriptions, and completes the give class or union type to have the described members.

On Compiler Explorer: [EDG](https://godbolt.org/z/4P15rnbxh), [Clang](https://godbolt.org/z/cT116Wb31).

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

    static constexpr std::array<std::meta::info, sizeof...(Ts)> types = {^Ts...};

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
    constexpr Variant() requires std::is_default_constructible_v<[: types[0] :]>
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

Arguably, the answer should be yes - this would be consistent with how other accesses work.

On Compiler Explorer: [EDG](https://godbolt.org/z/Efz5vsjaa), [Clang](https://godbolt.org/z/9bjd6rGjT).

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
    auto array_type = substitute(^std::array, {type_of(member), N });
    auto mem_descr = data_member_spec(array_type, {.name = name_of(member)});
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

On Compiler Explorer: [EDG](https://godbolt.org/z/8rT77KxjP), [Clang](https://godbolt.org/z/senWPW3eY).


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
    auto new_type = template_arguments_of(type_of(member))[0];
    new_members.push_back(data_member_spec(new_type, {.name=name_of(member)}));
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

On Compiler Explorer: [EDG](https://godbolt.org/z/1esbcq4jq), [Clang](https://godbolt.org/z/s943aezKs).

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

On Compiler Explorer: [Clang](https://godbolt.org/z/rbs6K78WG).

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
consteval auto struct_to_tuple_type(info type) -> info {
  return substitute(^std::tuple,
                    nonstatic_data_members_of(type)
                    | std::ranges::transform(std::meta::type_of)
                    | std::ranges::transform(std::meta::remove_cvref_type)
                    | std::ranges::to<std::vector>());
}

template <typename To, typename From, std::meta::info ... members>
constexpr auto struct_to_tuple_helper(From const& from) -> To {
  return To(from.[:members:]...);
}

template<typename From>
consteval auto get_struct_to_tuple_helper() {
  using To = [: struct_to_tuple_type(^From): ];

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

  return value_of<To(*)(From const&)>(
    substitute(^struct_to_tuple_helper, args));
}

template <typename From>
constexpr auto struct_to_tuple(From const& from) {
  return get_struct_to_tuple_helper<From>()(from);
}
```
:::

Here, `struct_to_tuple_type` takes a reflection of a type like `struct { T t; U const& u; V v; }` and returns a reflection of the type `std::tuple<T, U, V>`.
That gives us the return type.
Then, `struct_to_tuple_helper` is a function template that does the actual conversion --- which it can do by having all the reflections of the members as a non-type template parameter pack.
This is a `constexpr` function and not a `consteval` function because in the general case the conversion is a run-time operation.
However, determining the instance of `struct_to_tuple_helper` that is needed is a compile-time operation and has to be performed with a `consteval` function (because the function invokes `nonstatic_data_members_of`), hence the separate function template `get_struct_to_tuple_helper()`.

Everything is put together by using `substitute` to create the instantiation of `struct_to_tuple_helper` that we need, and a compile-time reference to that instance is obtained with `value_of`.
Thus `f` is a function reference to the correct specialization of `struct_to_tuple_helper`, which we can simply invoke.

On Compiler Explorer (with a different implementation than either of the above): [EDG](https://godbolt.org/z/Moqf84nc1), [Clang](https://godbolt.org/z/1s7aj5r69).

## Named Tuple

The tricky thing with implementing a named tuple is actually strings as non-type template parameters.
Because you cannot just pass `"x"` into a non-type template parameter of the form `auto V`, that leaves us with two ways of specifying the constituents:

1. Can introduce a `pair` type so that we can write `make_named_tuple<pair<int, "x">, pair<double, "y">>()`, or
2. Can just do reflections all the way down so that we can write `make_named_tuple<^int, ^"x", ^double, ^"y">()`.

We do not currently support splicing string literals (although that may change in the next revision), and the `pair` approach follows the similar pattern already shown with `define_class` (given a suitable `fixed_string` type):

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
  template<int N> struct Helper {
    static constexpr int value = N;
  };
public:
  static consteval int next() {
    // Search for the next incomplete Helper<k>.
    std::meta::info r;
    for (int k = 0;; ++k) {
      r = substitute(^Helper, { std::meta::reflect_value(k) });
      if (is_incomplete_type(r)) break;
    }
    // Return the value of its member.  Calling static_data_members_of
    // triggers the instantiation (i.e., completion) of Helper<k>.
    return value_of<int>(static_data_members_of(r)[0]);
  }
};

int x = TU_Ticket::next();  // x initialized to 0.
int y = TU_Ticket::next();  // y initialized to 1.
int z = TU_Ticket::next();  // z initialized to 2.
```
:::

Note that this relies on the fact that a call to `substitute` returns a specialization of a template, but doesn't trigger the instantiation of that specialization.
Thus, the only instantiations of `TU_Ticket::Helper` occur because of the call to `nonstatic_data_members_of` (which is a singleton representing the lone `value` member).

On Compiler Explorer: [EDG](https://godbolt.org/z/1vEjW4sTr), [Clang](https://godbolt.org/z/3Y3T1Y7Ya).

## Emulating typeful reflection
Although we believe a single opaque `std::meta::info` type to be the best and most scalable foundation for reflection, we acknowledge the desire expressed by SG7 for future support for "typeful reflection". The following demonstrates one possible means of assembling a typeful reflection library, in which different classes of reflections are represented by distinct types, on top of the facilities proposed here.

::: std
```cpp
// Represents a 'std::meta::info' constrained by a predicate.
template <std::meta::info Pred>
  requires (type_of(^([:Pred:](^int))) == ^bool)
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

  std::meta::info choice;
  for (auto [check, ctor] : std::views::zip(checks, ctors))
    if (value_of<bool>(reflect_invoke(check, {reflect_value(r)})))
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
  PrintKind([:enrich(^metatype):]);  // "template"
  PrintKind([:enrich(^type_t):]);    // "type"
  PrintKind([:enrich(^3):]);         // "unknown kind"
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

On Compiler Explorer: [Clang](https://godbolt.org/z/Ejeh8vWYs).

# Proposed Features

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
  - a _primary-expression_ referring to a variable, static data member, or structured binding, or
  - a _primary-expression_ referring to a nonstatic data member, or
  - a _primary-expression_ referring to a template, or
  - a constant-expression.

In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to not evaluate to constant.

### Syntax discussion

The original TS landed on `@[reflexpr]{.cf}@(...)` as the syntax to reflect source constructs and [@P1240R0] adopted that syntax as well.
As more examples were discussed, it became clear that that syntax was both (a) too "heavy" and (b) insufficiently distinct from a function call.
SG7 eventually agreed upon the prefix `^` operator. The "upward arrow" interpretation of the caret matches the "lift" or "raise" verbs that are sometimes used to describe the reflection operation in other contexts.

The caret already has a meaning as a binary operator in C++ ("exclusive OR"), but that is clearly not conflicting with a prefix operator.
In C++/CLI (a Microsoft C++ dialect) the caret is also used as a new kind of `$ptr-operator$` ([dcl.decl.general]{.sref}) to declare ["handles"](https://learn.microsoft.com/en-us/cpp/extensions/handle-to-object-operator-hat-cpp-component-extensions?view=msvc-170).
That is also not conflicting with the use of the caret as a unary operator because C++/CLI uses the usual prefix `*` operator to dereference handled.

Apple also uses the caret in the [syntax "blocks"](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/ProgrammingWithObjectiveC/WorkingwithBlocks/WorkingwithBlocks.html) and unfortunately we believe that does conflict with our proposed use of the caret.

Since the syntax discussions in SG7 landed on the use of the caret, new basic source characters have become available: `@`, `` ` ``{.op}, and `$`{.op}.
Of those, `@` seems the most likely substitute for the caret, because `$`{.op} is used for splice-like operations in other languages and `` ` ``{.op} is suggestive of some kind of quoting (which may be useful in future metaprogramming syntax developments).

Another option might be the use of the backslash (`\`{.op}).
It currently has a meaning at the end of a line of source code, but we could still use it as a prefix operator with the constraint that the reflected operand has to start on the same source line.  If we were to opt for that choice, it could make sense to use the slash (`/`{.op}) as a unary operator denoting splicing (see [Splicers](#splicers) below) so that `\`{.op} would correspond to "raise" and `/`{.op} would correspond to "lower".


## Splicers (`[:`...`:]`)

A reflection can be "spliced" into source code using one of several _splicer_ forms:

 - `[: r :]` produces an _expression_ evaluating to the entity or constant value represented by `r` in grammatical contexts that permit expressions.  In type-only contexts ([temp.res.general]{.sref}/4), `[: r :]` produces a type (and `r` must be the reflection of a type). In contexts that only permit a namespace name, `[: r :]` produces a namespace (and `r` must be the reflection of a namespace or alias thereof).
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

### Range Splicers

The splicers described above all take a single object of type `std::meta::info` (described in more detail below).
However, there are many cases where we don't have a single reflection, we have a range of reflections - and we want to splice them all in one go.
For that, we need a different form of splicer: a range splicer.

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

:::bq
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

Another way to work around a lack of range splicing would be to implement `with_size<N>(f)`, which would behave like `f(integral_constant<size_t, 0>{}, integral_constant<size_t, 0>{}, ..., integral_constant<size_t, N-1>{})`.
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

(P1240 did propose range splicers.)

### Syntax discussion

Early discussions of splice-like constructs (related to the TS design) considered using `@[unreflexpr]{.cf}@(...)` for that purpose.
[@P1240R0] adopted that option for _expression_ splicing, observing that a single splicing syntax could not viably be parsed (some disambiguation is needed to distinguish types and templates).
S-7 eventually agreed with the `[: ... :]` syntax --- with disambiguating tokens such as `typename` where needed --- which is a little lighter and more distinctive.

We propose `[:` and `:]` be single tokens rather than combinations of `[`, `]`, and `:`.
Among others, it simplifies the handling of expressions like `arr[[:refl():]]`.
On the flip side, it requires a special rule like the one that was made to handle `<::` to leave the meaning of `arr[::N]` unchanged and another one to avoid breaking a (somewhat useless) attribute specifier of the form `[[using ns:]]`.

A syntax that is delimited on the left and right is useful here because spliced expressions may involve lower-precedence operators.
However, there are other possibilities.
For example, now that `$`{.op} is available in the basic source character set, we might consider `@[$]{.op}@<$expr$>`.
This is somewhat natural to those of us that have used systems where `$`{.op} is used to expand placeholders in document templates.  For example:

::: std
```c++
@[$]{.op}@select_type(3) *ptr = nullptr;
```
:::

The prefixes `typename` and `template` are only strictly needed in some cases where the operand of the splice is a dependent expression.
In our proposal, however, we only make `typename` optional in the same contexts where it would be optional for qualified names with dependent name qualifiers.
That has the advantage to catch unfortunate errors while keeping a single rule and helping human readers parse the intended meaning of otherwise ambiguous constructs.


## `std::meta::info`

The type `std::meta::info` can be defined as follows:

::: std
```c++
namespace std {
  namespace meta {
    using info = decltype(^int);
  }
}
```
:::

In our initial proposal a value of type `std::meta::info` can represent:

  - any (C++) type and type alias
  - any function or member function
  - any variable, static data member, or structured binding
  - any non-static data member
  - any constant value
  - any template
  - any namespace
  - the null reflection (when default-constructed)

Notably absent at this time are general non-constant expressions (that aren't *expression-id*s referring to functions, variables or structured bindings).  For example:

::: std
```c++
int x = 0;
void g() {
  [:^x:] = 42;     // Okay.  Same as: x = 42;
  x = [:^(2*x):];  // Error: "2*x" is a general non-constant expression.
  constexpr int N = 42;
  x = [:^(2*N):];  // Okay: "2*N" is a constant-expression.
}
```
:::
Note that for `^(2*N)` an implementation only has to capture the constant value of `2*N` and not various other properties of the underlying expression (such as any temporaries it involves, etc.).

The type `std::meta::info` is a _scalar_ type. Nontype template arguments of type `std::meta::info` are permitted.
The entity being reflected can affect the linkage of a template instance involving a reflection.  For example:

:::bq
```c++
template<auto R> struct S {};

extern int x;
static int y;

S<^x> sx;  // S<^x> has external name linkage.
S<^y> sy;  // S<^y> has internal name linkage.
```
:::

Namespace `std::meta` is associated with type `std::meta::info`: That allows the core meta functions to be invoked without explicit qualification.
For example:

:::bq
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

:::bq
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

Some of the proposed metafunctions, however, have side-effects that have an effect of the remainder of the program.
For example, we provide a `define_class` metafunction that provides a definition for a given class.
Clearly, we want the effect of calling that metafunction to be "prompt" in a lexical-order sense.
For example:

:::bq
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

One important question we have to answer is: How do we handle errors in reflection metafunctions?
For example, what does `std::meta::template_of(^int)` do?
`^int` is a reflection of a type, but that type is not a specialization of a template, so there is no valid reflected template for us to return.

There are a few options available to us today:

1. This fails to be a constant expression (unspecified mechanism).
2. This returns an invalid reflection (similar to `NaN` for floating point) which carries source location info and some useful message.  (This was the approach suggested in P1240.)
3. This returns `std::expected<std::meta::info, E>` for some reflection-specific error type `E` which carries source location info and some useful message (this could be just `info` but probably should not be).
4. This throws an exception of type `E` (which requires allowing exceptions to work during `constexpr` evaluation, such that an uncaught exception would fail to be a constant exception).

The immediate downside of (2), yielding a `NaN`-like reflection for `template_of(^int)` is what we do for those functions that need to return a range.
That is, what does `template_arguments_of(^int)` return?

1. This fails to be a constant expression (unspecified mechanism).
2. This returns a `std::vector<std::meta::info>` containing one invalid reflection.
3. This returns a `std::expected<std::vector<std::meta::info>, E>`.
4. This throws an exception of type `E`.

Having range-based functions return a single invalid reflection would make for awkward error handling code.
Using `std::expected` or exceptions for error handling allow for a consistent, more straightforward interface.

This becomes another situation where we need to decide an error handling mechanism between exceptions and not exceptions, although importantly in this context a lot of usual concerns about exceptions do not apply:

* there is no runtime (so concerns about runtime performance, object file size, etc. do not exist), and
* there is no runtime (so concerns about code evolving to add a new uncaught exception type do not apply)

There is one interesting example to consider to decide between `std::expected` and exceptions here:

::: std
```cpp
template <typename T>
  requires (template_of(^T) == ^std::optional)
void foo();
```
:::

If `template_of` returns an `excepted<info, E>`, then `foo<int>` is a substitution failure --- `expected<T, E>` is equality-comparable to `T`, that comparison would evaluate to `false` but still be a constant expression.

If `template_of` returns `info` but throws an exception, then `foo<int>` would cause that exception to be uncaught, which would make the comparison not a constant expression.
This actually makes the constraint ill-formed - not a substitution failure.
In order to have `foo<int>` be a substitution failure, either the constraint would have to first check that `T` is a template or we would have to change the language rule that requires constraints to be constant expressions (we would of course still keep the requirement that the constraint is a `bool`).

The other thing to consider are compiler modes that disable exception support (like `-fno-exceptions` in GCC and Clang).
Today, implementations reject using `try`, `catch`, or `throw` at all when such modes are enabled.
With support for `constexpr` exceptions, implementations would have to come up with a strategy for how to support compile-time exceptions --- probably by only allowing them in `consteval` functions (including `constexpr` function templates that were propagated to `consteval`).

Despite these concerns (and the requirement of a whole new language feature), we believe that exceptions will be the more user-friendly choice for error handling here, simply because exceptions are more ergonomic to use than `std::expected` (even if we adopt language features that make this type easier to use - like pattern matching and a control flow operator).

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
  This is `C` has its own template arguments that can be reflected on.

### Freestanding implementations


Several important metafunctions, such as `std::meta::_nonstatic_data_members_of`, return a `std::vector` value.
Unfortunately, that means that they are currently not usable in a freestanding environment.
That is an highly undesirable limitation that we believe should be addressed by imbuing freestanding implementations with a more restricted `std::vector` (e.g., one that can only allocate at compile time).

### Synopsis

Here is a synopsis for the proposed library API. The functions will be explained below.

::: std
```c++
namespace std::meta {
  // @[name and location](#name_of-display_name_of-source_location_of)@
  consteval auto name_of(info r) -> string_view;
  consteval auto qualified_name_of(info r) -> string_view;
  consteval auto display_name_of(info r) -> string_view;
  consteval auto source_location_of(info r) -> source_location;

  // @[type queries](#type_of-parent_of-dealias)@
  consteval auto type_of(info r) -> info;
  consteval auto parent_of(info r) -> info;
  consteval auto dealias(info r) -> info;

  // @[template queries](#template_of-template_arguments_of)@
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;

  // @[member queries](#members_of-static_data_members_of-nonstatic_data_members_of-bases_of-enumerators_of-subobjects_of)@
  template<typename ...Fs>
    consteval auto members_of(info class_type, Fs ...filters) -> vector<info>;
  template<typename ...Fs>
    consteval auto bases_of(info class_type, Fs ...filters) -> vector<info>;
  consteval auto static_data_members_of(info class_type) -> vector<info>;
  consteval auto nonstatic_data_members_of(info class_type) -> vector<info>;
  consteval auto subobjects_of(info class_type) -> vector<info>;
  consteval auto enumerators_of(info enum_type) -> vector<info>;

  template<typename ...Fs>
    consteval auto accessible_members_of(info class_type, Fs ...filters) -> vector<info>;
  template<typename ...Fs>
    consteval auto accessible_bases_of(info class_type, Fs ...filters) -> vector<info>;
  consteval auto accessible_static_data_members_of(info class_type) -> vector<info>;
  consteval auto accessible_nonstatic_data_members_of(info class_type) -> vector<info>;
  consteval auto accessible_subobjects_of(info class_type) -> vector<info>;

  // @[substitute](#substitute)@
  consteval auto can_substitute(info templ, span<info const> args) -> bool;
  consteval auto substitute(info templ, span<info const> args) -> info;

  // @[reflect_invoke](#reflect_invoke)@
  consteval auto reflect_invoke(info target, span<info const> args) -> info;
  consteval auto reflect_invoke(info target, span<info const> tmpl_args, span<info const> args) -> info;

   // @[value_of<T>](#value_oft)@
  template<typename T>
    consteval auto value_of(info) -> T;

  // @[test_type](#test_type-test_types)@
  consteval auto test_type(info templ, info type) -> bool;
  consteval auto test_types(info templ, span<info const> types) -> bool;

  // other type predicates (see @[the wording](#meta.reflection.queries-reflection-queries)@)
  consteval auto is_public(info r) -> bool;
  consteval auto is_protected(info r) -> bool;
  consteval auto is_private(info r) -> bool;
  consteval auto is_accessible(info r) -> bool;
  consteval auto is_virtual(info r) -> bool;
  consteval auto is_pure_virtual(info entity) -> bool;
  consteval auto is_override(info entity) -> bool;
  consteval auto is_deleted(info entity) -> bool;
  consteval auto is_defaulted(info entity) -> bool;
  consteval auto is_explicit(info entity) -> bool;
  consteval auto is_bit_field(info entity) -> bool;
  consteval auto has_static_storage_duration(info r) -> bool;
  consteval auto has_internal_linkage(info r) -> bool;
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
  consteval auto has_template_arguments(info r) -> bool;
  consteval auto is_constructor(info r) -> bool;
  consteval auto is_destructor(info r) -> bool;
  consteval auto is_special_member(info r) -> bool;

  // @[reflect_value](#reflect_value)@
  template<typename T>
    consteval auto reflect_value(T value) -> info;

  // @[define_class](#data_member_spec-define_class)@
  struct data_member_options_t;
  consteval auto data_member_spec(info class_type,
                                  data_member_options_t options = {}) -> info;
  consteval auto define_class(info class_type, span<info const>) -> info;

  // @[data layout](#data-layout-reflection)@
  consteval auto offset_of(info entity) -> size_t;
  consteval auto size_of(info entity) -> size_t;
  consteval auto bit_offset_of(info entity) -> size_t;
  consteval auto bit_size_of(info entity) -> size_t;
  consteval auto alignment_of(info entity) -> size_t;
}
```
:::

### `name_of`, `display_name_of`, `source_location_of`

:::bq
```c++
namespace std::meta {
  consteval auto name_of(info r) -> string_view;
  consteval auto qualified_name_of(info r) -> string_view;
  consteval auto display_name_of(info r) -> string_view;
  consteval auto source_location_of(info r) -> source_location;
}
```
:::

Given a reflection `r` that designates a declared entity `X`, `name_of(r)` and `qualified_name_of(r)` return a `string_view` holding the unqualified and qualified name of `X`, respectively.
For all other reflections, an empty `string_view` is produced.
For template instances, the name does not include the template argument list.
The contents of the `string_view` consist of characters of the basic source character set only (an implementation can map other characters using universal character names).

Given a reflection `r`, `display_name_of(r)` returns a unspecified non-empty `string_view`.
Implementations are encouraged to produce text that is helpful in identifying the reflected construct.

Given a reflection `r`, `source_location_of(r)` returns an unspecified `source_location`.
Implementations are encouraged to produce the correct source location of the item designated by the reflection.

### `type_of`, `parent_of`, `dealias`

:::bq
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
consteval auto do_typeof(std::meta::info r) -> std::meta::info {
  return remove_cvref_type(is_type(r) ? r : type_of(r));
}

#define typeof(e) [: do_typeof(^e) :]
```
:::

If `r` designates a member of a class or namespace, `parent_of(r)` is a reflection designating its immediately enclosing class or namespace.

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

### `template_of`, `template_arguments_of`

::: std
```c++
namespace std::meta {
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;
}
```
:::

If `r` is a reflection designated a specialization of some template, then `template_of(r)` is a reflection of that template and `template_arguments_of(r)` is a vector of the reflections of the template arguments. In other words, the preconditions on both is that `has_template_arguments(r)` is `true`.

For example:

::: std
```c++
std::vector<int> v = {1, 2, 3};
static_assert(template_of(type_of(^v)) == ^std::vector);
static_assert(template_arguments_of(type_of(^v))[0] == ^int);
```
:::



### `members_of`, `static_data_members_of`, `nonstatic_data_members_of`, `bases_of`, `enumerators_of`, `subobjects_of`

:::bq
```c++
namespace std::meta {
  template<typename ...Fs>
    consteval auto members_of(info class_type, Fs ...filters) -> vector<info>;

  template<typename ...Fs>
    consteval auto bases_of(info class_type, Fs ...filters) -> vector<info>;

  consteval auto static_data_members_of(info class_type) -> vector<info> {
    return members_of(class_type, is_variable);
  }

  consteval auto nonstatic_data_members_of(info class_type) -> vector<info> {
    return members_of(class_type, is_nonstatic_data_member);
  }

  consteval auto subobjects_of(info class_type) -> vector<info> {
    auto subobjects = bases_of(class_type);
    subobjects.append_range(nonstatic_data_members_of(class_type));
    return subobjects;
  }

  consteval auto enumerators_of(info enum_type) -> vector<info>;

  template<typename ...Fs>
    consteval auto accessible_members_of(info class_type, Fs ...filters) -> vector<info>;
  template<typename ...Fs>
    consteval auto accessible_bases_of(info class_type, Fs ...filters) -> vector<info>;
  consteval auto accessible_static_data_members_of(info class_type) -> vector<info>;
  consteval auto accessible_nonstatic_data_members_of(info class_type) -> vector<info>;
  consteval auto accessible_subobjects_of(info class_type) -> vector<info>;
}
```
:::

The template `members_of` returns a vector of reflections representing the direct members of the class type represented by its first argument.
Any nonstatic data members appear in declaration order within that vector.
Anonymous unions appear as a nonstatic data member of corresponding union type.
If any `Filters...` argument is specified, a member is dropped from the result if any filter applied to that members reflection returns `false`.
E.g., `members_of(^C, std::meta::is_type)` will only return types nested in the definition of `C` and `members_of(^C, std::meta::is_type, std::meta::is_variable)` will return an empty vector since a member cannot be both a type and a variable.

The template `bases_of` returns the direct base classes of the class type represented by its first argument, in declaration order.

`enumerators_of` returns the enumerator constants of the indicated enumeration type in declaration order.

Each variant named `accessible_meow_of` simply returns the result of `meow_of` filtered on `is_accessible`. Note that this might change to be `is_accessible_from(e, context)` rather than simply `is_accessible(e)`.


### `substitute`

:::bq
```c++
namespace std::meta {
  consteval auto can_substitute(info templ, span<info const> args) -> bool;
  consteval auto substitute(info templ, span<info const> args) -> info;
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

:::bq
```c++
template<typename T> struct S { typename T::X x; };

constexpr auto r = substitute(^S, std::vector{^int});  // Okay.
typename[:r:] si;  // Error: T::X is invalid for T = int.
```
:::

`can_substitute(templ, args)` simply checks if the substitution can succeed (with the same caveat about instantiations outside of the immediate context).
If `can_substitute(templ, args)` is `false`, then `substitute(templ, args)` will be ill-formed.

### `reflect_invoke`

:::bq
```c++
namespace std::meta {
  consteval auto reflect_invoke(info target, span<info const> args) -> info;
  consteval auto reflect_invoke(info target, span<info const> tmpl_args, span<info const> args) -> info;
}
```
:::

These metafunctions produces a reflection of the value returned by a call expression.

For the first overload: Letting `F` be the entity reflected by `target`, and `A@~0~@, A@~1~@, ..., A@~N~@` be the sequence of entities reflected by the values held by `args`: if the expression `F(A@~0~@, A@~1~@, ..., A@~N~@)` is a well-formed constant expression evaluating to a type that is not `void`, and if every value in `args` is a reflection of a constant value, then `reflect_invoke(target, args)` evaluates to a reflection of the constant value `F(A@~0~@, A@~1~@, ..., A@~N~@)`. For all other invocations, `reflect_invoke(target, args)` is not a constant expression.

The second overload behaves the same as the first overload, except instead of evaluating `F(A@~0~@, A@~1~@, ..., A@~N~@)`, we require that `F` be a reflection of a template and evaluate `F<T@~0~@, T@~1~@, ..., T@~M~@>(A@~0~@, A@~1~@, ..., A@~N~@)`. This allows evaluating `reflect_invoke(^std::get, {reflect_value(0)}, {e})` to evaluate to, approximately, `^std::get<0>([: e :])`.


### `value_of<T>`

:::bq
```c++
namespace std::meta {
  template<typename T> consteval auto value_of(info) -> T;
}
```
:::

If `r` is a reflection for a constant-expression or a constant-valued entity of type `T`, `value_of<T>(r)` evaluates to that constant value.

If `r` is a reflection for a variable of non-reference type `T`, `value_of<T&>(r)` and `value_of<T const&>(r)` are lvalues referring to that variable.
If the variable is usable in constant expressions [expr.const], `value_of<T>(r)` evaluates to its value.

If `r` is a reflection for a variable of reference type `T` usable in constant-expressions, `value_of<T>(r)` evaluates to that reference.

If `r` is a reflection for a function, or pointer to a function, of type `R(A_0, ... A_n)`, `value_of<R(*)(A_0, ..., A_n)>(r)` evaluates to a pointer to that function.

If `r` is a reflection for a non-static member function, or pointer to a non-static member function, and `T` is the type for a pointer to the reflected member function, `value_of<T>(r)` evaluates to a pointer to the member function.

If `r` is a reflection for an enumerator constant of type `E`, `value_of<E>(r)` evaluates to the value of that enumerator.

If `r` is a reflection for a non-bit-field non-reference non-static member of type `M` in a class `C`, `value_of<M C::*>(r)` is the pointer-to-member value for that nonstatic member.

For other reflection values `r`, `value_of<T>(r)` is ill-formed.

The function template `value_of` may feel similar to splicers, but unlike splicers it does not require its operand to be a constant-expression itself.
Also unlike splicers, it requires knowledge of the type associated with the entity reflected by its operand.

### `test_type`, `test_types`

:::bq
```c++
namespace std::meta {
  consteval auto test_type(info templ, info type) -> bool {
    return test_types(templ, {type});
  }

  consteval auto test_types(info templ, span<info const> types) -> bool {
    return value_of<bool>(substitute(templ, types));
  }
}
```
:::

This utility translates existing metaprogramming predicates (expressed as constexpr variable templates or concept templates) to the reflection domain.
For example:

:::bq
```c++
struct S {};
static_assert(test_type(^std::is_class_v, ^S));
```
:::

An implementation is permitted to recognize standard predicate templates and implement `test_type` without actually instantiating the predicate template.
In fact, that is recommended practice.

### `reflect_value`

:::bq
```c++
namespace std::meta {
  template<typename T> consteval auto reflect_value(T value) -> info;
}
```
:::

This metafunction produces a reflection representing the constant value of the operand.

### `data_member_spec`, `define_class`

:::bq
```c++
namespace std::meta {
  struct data_member_options_t {
    optional<string_view> name;
    bool is_static = false;
    optional<int> alignment;
    optional<int> width;
  };
  consteval auto data_member_spec(info type,
                                  data_member_options_t options = {}) -> info;
  consteval auto define_class(info class_type, span<info const>) -> info;
}
```
:::

`data_member_spec` returns a reflection of a description of a data member of given type. Optional alignment, bit-field-width, static-ness, and name can be provided as well. If no `name` is provided, the name of the data member is unspecified. If `is_static` is `true`, the data member is declared `static`.

`define_class` takes the reflection of an incomplete class/struct/union type and a range of reflections of data member descriptions and it completes the given class type with data members as described (in the given order).
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
constexpr auto U = define_class(^S<int>, {
  data_member_spec(^int, {.name="i", .align=64}),
  data_member_spec(^int, {.name="j", .align=64}),
});

// S<int> is now defined to the equivalent of
// template<> struct S<int> {
//   alignas(64) int i;
//   alignas(64) int j;
// };
```
:::

When defining a `union`, if one of the alternatives has a non-trivial destructor, the defined union will _still_ have a destructor provided - that simply does nothing.
This allows implementing [variant](#a-simple-variant-type) without having to further extend support in `define_class` for member functions.

If `class_type` is a reflection of a type that already has a definition, or which is in the process of being defined, the call to `define_class` is not a constant expression.

### Data Layout Reflection
:::bq
```c++
namespace std::meta {
  consteval auto offset_of(info entity) -> size_t;
  consteval auto size_of(info entity) -> size_t;

  consteval auto bit_offset_of(info entity) -> size_t;
  consteval auto bit_size_of(info entity) -> size_t;

  consteval auto alignment_of(info entity) -> size_t;
}
```
:::

### Other Type Traits

There is a question of whether all the type traits should be provided in `std::meta`.
For instance, a few examples in this paper use `std::meta::remove_cvref_type(t)` as if that exists.
Technically, the functionality isn't strictly necessary - since it can be provided indirectly:

::: cmptable
### Direct
```cpp
std::meta::remove_cvref_type(type)
```

### Indirect
```cpp
std::meta::substitute(^std::remove_cvref_t, {type})
```

---

```cpp
std::meta::is_const_type(type)
```

```cpp
std::meta::value_of<bool>(std::meta::substitute(^std::is_const_v, {type}))
std::meta::test_type(^std::is_const_v, type)
```
:::

Having `std::meta::meow` for every trait `std::meow` is more straightforward and will likely be faster to compile, though means we will have a much larger library API.
There are quite a few traits in [meta]{.sref} - but it should be easy enough to specify all of them.
So we're doing it.

Now, one thing that came up is that the straightforward thing we want to do is to simply add a `std::meta::meow` for every trait `std::meow` and word it appropriately. That's what the current wording in this revision does.
However, we've run into a conflict.
The standard library type traits are all *type* traits - they only accept types.
As such, their names are simply things like `std::is_pointer`, `std::is_const`, `std::is_lvalue_reference`, and so forth.
Renaming it to `std::is_pointer_type`, for instance, would be a waste of characters since there's nothing else the argument could be save for a type.
But this is no longer the case.
Consider `std::meta::is_function(e)`, which is currently actually specified twice in our wording having two different meanings:

1. A consteval function equivalent of the type trait `std::is_function<T>`, such that `std::meta::is_function(e)` mandates that `e` reflect a type and checks if that type is a function type.
  This is the same category of type trait as the ones mentioned above.
2. A new kind of reflection query `std::meta::is_function(e)` which asks if `e` is the reflection of a function (as opposed to a type or a namespace or a template, etc.).
  This is the same category of query as `std::meta::is_template` or `std::meta::is_concept` or `std::meta::is_namespace`.

Both of these are useful, yet they mean different things entirely - the first is ill-formed when passed a reflection of a function (as opposed to a function type), and the second would simply answer `false` for the reflection of _any_ type (function type or otherwise).
So what do we do?

Probably the most straightforward choice would be to suffix all of the type traits with `_type`.
That is: `std::is_pointer<T>` because `std::meta::is_pointer_type(^T)`, `std::is_arithmetic<T>` becomes `std::meta::is_arithmetic_type(^T)`, and so forth.
The advantage of this approach is that it very likely just works, also opening the door to making a more general `std::meta::is_const(e)` that checks not just if `e` is a `const`-qualified type but also if it's a `const`-qualified object or a `const`-qualified member, etc.
The disadvantage is that the suffixed names would not be familiar - we're much more familiar with the name `is_copy_constructible` than we would be with `is_copy_constructible_type`.

That said, it's not too much added mental overhead to remember `is_copy_constructible_type` and this avoids have to remember which type traits have the suffix and which don't. Not to mention that _many_ of the type traits read as if they would accept objects just fine (e.g. `is_trivially_copyable`). So we propose that simply all the type traits be suffixed with `*_type`.

# Proposed Wording

## Language

### [lex.phases]{.sref} Phases of translation {-}

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

:::bq

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

:::addu
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

### [basic.types.general]{.sref} General {-}

Change the first sentence in paragraph 9 of [basic.types.general]{.sref} as follows:

::: std
[9]{.pnum} Arithmetic types (6.8.2), enumeration types, pointer types, pointer-to-member types (6.8.4),[ `std::meta::info`,]{.addu} `std::nullptr_t`, and cv-qualified (6.8.5) versions of these types are collectively called scalar types.
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

:::
:::

### [basic.fundamental]{.sref} Fundamental types {-}

Add a new paragraph before the last paragraph of [basic.fundamental]{.sref} as follows:

::: std
::: addu

[*]{.pnum} A value of type `std::meta::info` is called a _reflection_ and represents a language element such as a type, a constant value, a non-static data member, etc. An expression convertible to `std::meta::info` is said to _reflect_ the language element represented by the resulting value; the language element is said to be _reflected by_ the expression.
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
+    [: $constant-expression$ :]
+    template[: $constant-expression$ :] < $template-argument-list$@~_opt_~@ >
```
:::

### [expr.prim.id.qual]{.sref} Qualified names {-}

Add a production to the grammar for `$nested-name-specifier$` as follows:

:::bq
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

[#]{.pnum} For a `$primary-expression$` of the form `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` the converted `$constant-expression$` shall evaluate to a reflection for a concept, variable template, class template, alias template, or function template.
The meaning of such a construct is identical to that of a `$primary-expression$` of the form `$template-name$ < $template-argument-list$@~_opt_~@ >` where `$template-name$` denotes the reflected template or concept (ignoring access checking on the `$template-name$`).

[#]{.pnum} For a `$primary-expression$` of the form `[: $constant-expression$ :]` where the converted `$constant-expression$` evaluates to a reflection for a variable, a function, an enumerator, or a structured binding, the meaning of the expression is identical to that of a `$primary-expression$` of the form `$id-expression$` that would denote the reflected entity (ignoring access checking).

[#]{.pnum} Otherwise, for a `$primary-expression$` of the form `[: $constant-expression$ :]` the converted `$constant-expression$` shall evaluate to a reflection for a constant value and the expression shall evaluate to that value.
:::
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
+    ^ $cast-expression$
```
:::

### 7.6.2.10* [expr.reflect] The reflection operator {-}

Add a new subsection of [expr.unary]{.sref} following [expr.delete]{.sref}

::: std
::: addu
**The Reflection Operator   [expr.reflect]**

[#]{.pnum} The unary `^` operator (called _the reflection operator_) produces a prvalue --- called _reflection_ --- whose type is the reflection type (i.e., `std::meta::info`).
That reflection represents its operand.

[#]{.pnum} Every value of type `std::meta::info` is either a reflection of some operand or a *null reflection value*.

[#]{.pnum} An ambiguity can arise between the interpretation of the operand of the reflection operator as a `$type-id$` or a `$cast-expression$`; in such cases, the `$type-id$` treatment is chosen.
Parentheses can be introduced to force the `$cast-expression$` interpretation.


[#]{.pnum}

::: example
```
static_assert(is_type(^int()));    // ^ applies to the type-id "int()"; not the cast "int()"
static_assert(!is_type(^(int()))); // ^ applies to the the cast-expression "(int())"

template<bool> struct X {};
bool operator<(std::meta::info, X<false>);
consteval void g(std::meta::info r, X<false> xv) {
  if (r == ^int && true);    // error: ^ applies to the type-id "int&&"
  if (r == (^int) && true);  // OK
  if (^X < xv);       // error: < starts template argument list
  if ((^X) < xv);     // OK
}


```
:::

[#]{.pnum} When applied to `::`, the reflection operator produces a reflection for the global namespace.
When applied to a `$namespace-name$`, the reflection produces a reflection for the indicated namespace or namespace alias.

[#]{.pnum} When applied to a `$template-name$`, the reflection produces a reflection for the indicated template.

[#]{.pnum} When applied to a `$concept-name$`, the reflection produces a reflection for the indicated concept.

[#]{.pnum} When applied to a `$type-id$`, the reflection produces a reflection for the indicated type or type alias.

[#]{.pnum} When applied to a `$cast-expression$`, the `$cast-expression$` shall be a constant expression ([expr.const]{.sref}) or an `$id-expression$` ([expr.prim.id]{.sref}) designating a variable, a function, an enumerator constant, or a nonstatic member.
The `$cast-expression$` is not evaluated.

* [#.#]{.pnum} If the operand of the reflection operator is an `$id-expression$`, the result is a reflection for the indicated entity.

  * [#.#.#]{.pnum} If this `$id-expression$` names an overload set `S`, and if the assignment of `S` to an invented variable of type `const auto` ([dcl.type.auto.deduct]{.sref}) would select a unique candidate function `F` from `S`, the result is a reflection of `F`. Otherwise, the expression `^S` is ill-formed.

* [#.#]{.pnum} If the operand is a constant expression, the result is a reflection for the resulting value.

* [#.#]{.pnum} If the operand is both an `$id-expression$` and a constant expression, the result is a reflection for both the indicated entity and the expression's (constant) value.

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
* [*.#]{.pnum} Otherwise, if both operands are reflections of a namespace alias, alias template, or type alias, then they compare equal if they are reflections of the same namespace alias, alias template, or type alias, respectively.
* [*.#]{.pnum} Otherwise, if neither operand is a reflection of an expression, then they compare equal if they are reflections of the same entity.
* [*.#]{.pnum} Otherwise, if one operand is a reflection of an expression and the other is not, then they compare unequal.
* [*.#]{.pnum} Otherwise (if both operands are reflections of expressions):
  * [*.#.#]{.pnum} If both operands designate *id-expressions*, then they compare equal if they identify the same declared entity.
  * [*.#.#]{.pnum} Otherwise, if one operand designates an *id-expression*, then they compare unequal.
  * [*.#.#]{.pnum} Otherwise, the result is unspecified.
:::

[6]{.pnum} If two operands compare equal, the result is `true` for the `==` operator and `false` for the `!=` operator. If two operands compare unequal, the result is `false` for the `==` operator and `true` for the `!=` operator. Otherwise, the result of each of the operators is unspecified.
:::


### [expr.const]{.sref} Constant Expressions {-}

Add a new paragraph after the definition of _manifestly constant-evaluated_ [expr.const]{.sref}/20:

:::bq
:::addu

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
[1]{.pnum} A `$using-directive$` shall not appear in class scope, but may appear in namespace scope or in block scope. [A `$namespace-declarator$` not consisting of a `$splice-namespace-name$` nominates the namespace found by lookup ([basic.lookup.unqual]{.sref}, [basic.lookup.qual]{.sref}) and shall not contain a dependent `$nested-name-specifier$`. A `$namespace-declarator$` consisting of a `$splice-namespace-name$` shall contain a non-dependent `$constant-expression$` that reflects a namespace, and nominates the namespace reflected by the `$constant-expression$`.]{.addu}
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

### [temp.names]{.sref} Names of template specializations {-}

Modify the grammar for `$template-argument$` as follows:

::: std
```diff
+ $splice-template-argument$:
+     [: constant-expression :]
+
  $template-argument$:
      $constant-expression$
      $type-id$
      $id-expression$
      $braced-init-list$
+     $splice-template-argument$
```
:::


Add a paragraph after paragraph 3 of [temp.names]{.sref}:

::: std
:::addu

[*]{.pnum} A `<` is also interpreted as the delimiter of a `$template-argument-list$` if it follows a splicer of the form `template[: $constant-expression$ :]`.

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
[1]{.pnum} A `$template-argument$` for a `$template-parameter$` which is a type shall [either]{.addu} be a `$type-id$` [or a `$splice-template-argument$`. A `$template-argument$` having a `$splice-template-argument$` for such a `$template-parameter$` is treated as if were a `$type-id$` nominating the type reflected by the `$constant-expression$` of the `$splice-template-argument$`.]{.addu}

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


### [temp.dep.expr]{.sref} Type-dependent expressions {-}

Add to the list of never-type-dependent expression forms in [temp.dep.expr]{.sref}/4:

:::bq
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

:::bq
:::addu

[9]{.pnum} A `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` is type-dependent if the `$constant-expression$` is value-dependent or if the optional `$template-argument-list$` contains a value-dependent nontype or template argument, or a dependent type argument.

:::
:::



### [temp.dep.constexpr]{.sref} Value-dependent expressions {-}

Add at the end of [temp.dep.constexpr]{.sref}/2 (before the note):

:::bq
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

[A `$reflect-expression$` is value dependent if the operand of the reflection operator is a type-dependent or value-dependent expression or if that operand is a dependent `$type-id$`.]{.addu}
:::


Add a new paragraph after [temp.dep.constexpr]{.sref}/4:

:::bq
:::addu

[6]{.pnum} A `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` is value-dependent if the `$constant-expression$` is value-dependent or if the optional `$template-argument-list$` contains a value-dependent nontype or template argument, or a dependent type argument.

:::
:::



## Library

### [meta] Header `<meta>` synopsis {-}

Add a new subsection in [meta]{.sref} after [type.traits]{.sref}:

::: std
::: addu
**Header `<meta>` synopsis**

```
namespace std::meta {
  using info = decltype(^::);

  // [meta.reflection.names], reflection names and locations
  consteval string_view name_of(info r);
  consteval string_view qualified_name_of(info r);
  consteval string_view display_name_of(info r);
  consteval source_location source_location_of(info r);

  // [meta.reflection.queries], reflection queries
  consteval bool is_public(info r);
  consteval bool is_protected(info r);
  consteval bool is_private(info r);
  consteval bool is_accessible(info r);
  consteval bool is_virtual(info r);
  consteval bool is_pure_virtual(info r);
  consteval bool is_override(info r);
  consteval bool is_deleted(info r);
  consteval bool is_defaulted(info r);
  consteval bool is_explicit(info r);
  consteval bool is_bit_field(info r);
  consteval bool has_static_storage_duration(info r);
  consteval bool has_internal_linkage(info r);
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
  consteval bool has_template_arguments(info r);
  consteval bool is_class_member(info entity);
  consteval bool is_namespace_member(info entity);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);
  consteval bool is_constructor(info r);
  consteval bool is_destructor(info r);
  consteval bool is_special_member(info r);

  consteval info type_of(info r);
  consteval info parent_of(info r);
  consteval info dealias(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);

  // [meta.reflection.member.queries], reflection member queries
  template<class... Fs>
    consteval vector<info> members_of(info type, Fs... filters);
  template<class... Fs>
    consteval vector<info> accessible_members_of(info type, Fs... filters);
  template<class... Fs>
    consteval vector<info> bases_of(info type, Fs... filters);
  template<class... Fs>
    consteval vector<info> accessible_bases_of(info type, Fs... filters);
  consteval vector<info> static_data_members_of(info type);
  consteval vector<info> accessible_static_data_members_of(info type);
  consteval vector<info> nonstatic_data_members_of(info type);
  consteval vector<info> accessible_nonstatic_data_members_of(info type);
  consteval vector<info> subobjects_of(info type);
  consteval vector<info> accessible_subobjects_of(info type);
  consteval vector<info> enumerators_of(info enum_type);

  // [meta.reflection.substitute], reflection substitution
  consteval bool can_substitute(info templ, span<const info> arguments);
  consteval info substitute(info templ, span<const info> arguments);

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
  consteval bool is_trivial_type(info type);
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

  consteval bool is_constructible_type(info type, span<info const> type_args);
  consteval bool is_default_constructible_type(info type);
  consteval bool is_copy_constructible_type(info type);
  consteval bool is_move_constructible_type(info type);

  consteval bool is_assignable_type(info dst_type, info src_type);
  consteval bool is_copy_assignable_type(info type);
  consteval bool is_move_assignable_type(info type);

  consteval bool is_swappable_with_type(info dst_type, info src_type);
  consteval bool is_swappable_type(info type);

  consteval bool is_destructible_type(info type);

  consteval bool is_trivially_constructible_type(info type, span<info const> type_args);
  consteval bool is_trivially_default_constructible_type(info type);
  consteval bool is_trivially_copy_constructible_type(info type);
  consteval bool is_trivially_move_constructible_type(info type);

  consteval bool is_trivially_assignable_type(info dst_type, info src_type);
  consteval bool is_trivially_copy_assignable_type(info type);
  consteval bool is_trivially_move_assignable_type(info type);
  consteval bool is_trivially_destructible_type(info type);

  consteval bool is_nothrow_constructible_type(info type, span<info const> type_args);
  consteval bool is_nothrow_default_constructible_type(info type);
  consteval bool is_nothrow_copy_constructible_type(info type);
  consteval bool is_nothrow_move_constructible_type(info type);

  consteval bool is_nothrow_assignable_type(info dst_type, info src_type);
  consteval bool is_nothrow_copy_assignable_type(info type);
  consteval bool is_nothrow_move_assignable_type(info type);

  consteval bool is_nothrow_swappable_with_type(info dst_type, info src_type);
  consteval bool is_nothrow_swappable_type(info type);

  consteval bool is_nothrow_destructible_type(info type);

  consteval bool is_implicit_lifetime_type(info type);

  consteval bool has_virtual_destructor_type(info type);

  consteval bool has_unique_object_representations_type(info type);

  consteval bool reference_constructs_from_temporary_type(info dst_type, info src_type);
  consteval bool reference_converts_from_temporary_type(info dst_type, info src_type);

  // [meta.reflection.unary.prop.query], type property queries
  consteval size_t alignment_of_type(info type);
  consteval size_t rank_type(info type);
  consteval size_t extent_type(info type, unsigned i = 0);

  // [meta.reflection.rel], type relations
  consteval bool is_same_type(info type1, info type2);
  consteval bool is_base_of_type(info base_type, info derived_type);
  consteval bool is_convertible_type(info src_type, info dst_type);
  consteval bool is_nothrow_convertible_type(info src_type, info dst_type);
  consteval bool is_layout_compatible_type(info type1, info type2);
  consteval bool is_pointer_interconvertible_base_of_type(info base_type, info derived_type);

  consteval bool is_invocable_type(info type, span<const info> type_args);
  consteval bool is_invocable_r_type(info result_type, info type, span<const info> type_args);

  consteval bool is_nothrow_invocable_type(info type, span<const info> type_args);
  consteval bool is_nothrow_invocable_r_type(info result_type, info type, span<const info> type_args);

  // [meta.reflection.trans.cv], const-volatile modifications
  consteval info remove_const_type(info type);
  consteval info remove_volatile_type(info type);
  consteval info remove_cv_type(info type);
  consteval info add_const_type(info type);
  consteval info add_volatile_type(info type);
  consteval info add_cv_type(info type);

  // [meta.reflection.trans.ref], reference modifications
  consteval info remove_reference_type(info type);
  consteval info add_lvalue_reference_type(info type);
  consteval info add_rvalue_reference_type(info type);

  // [meta.reflection.trans.sign], sign modifications
  consteval info make_signed_type(info type);
  consteval info make_unsigned_type(info type);

  // [meta.reflection.trans.arr], array modifications
  consteval info remove_extent_type(info type);
  consteval info remove_all_extents_type(info type);

  // [meta.reflection.trans.ptr], pointer modifications
  consteval info remove_pointer_type(info type);
  consteval info add_pointer_type(info type);

  // [meta.reflection.trans.other], other transformations
  consteval info remove_cvref_type(info type);
  consteval info decay_type(info type);
  consteval info common_type_type(span<const info> type_args);
  consteval info common_reference_type(span<const info> type_args);
  consteval info underlying_type_type(info type);
  consteval info invoke_result_type(info type, span<const info> type_args);
  consteval info unwrap_reference_type(info type);
  consteval info unwrap_ref_decay_type(info type);
}
```
:::
:::

### [meta.reflection.names] Reflection names and locations {-}

::: std
::: addu
```cpp
consteval string_view name_of(info r);
consteval string_view qualified_name_of(info r);
```

[#]{.pnum} *Returns*: If `r` designates a declared entity `X`, then the unqualified and qualified names of `X`, respectively. Otherwise, an empty `string_view`.

```cpp
consteval string_view display_name_of(info r);
```
[#]{.pnum} *Returns*: An implementation-defined string suitable for identifying the reflected construct.

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
consteval bool is_accessible(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates a class member or base class that is accessible at the point of the immediate invocation ([expr.const]) that resulted in the evaluation of `is_accessible(r)`.  Otherwise, `false`.

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

[#]{.pnum} *Returns*: `true` if `r` designates a function or member function that is defined as deleted. Otherwise, `false`.

```cpp
consteval bool is_defaulted(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a member function that is defined as defaulted. Otherwise, `false`.

```cpp
consteval bool is_explicit(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a member function or member function template that is declared explicit. Otherwise, `false`.

```cpp
consteval bool is_bit_field(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a bit-field. Otherwise, `false`.

```cpp
consteval bool has_static_storage_duration(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates an object that has static storage duration. Otherwise, `false`.

```cpp
consteval bool has_internal_linkage(info r);
consteval bool has_external_linkage(info r);
consteval bool has_linkage(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates an entity that has internal linkage, external linkage, or any linkage, respectively ([basic.link]). Otherwise, `false`.


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
[#]{.pnum} *Returns*: `true` if `delias(r)` designates an incomplete type. Otherwise, `false`.

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
```
[#]{.pnum} *Returns*: `true` if `r` designates a function template, class template, variable template, alias template, or concept, respectively. Otherwise, `false`.

```cpp
consteval bool has_template_arguments(info r);
```
[#]{.pnum} *Returns*: `true` if `r` designates an instantiation of a function template, variable template, class template, or an alias template. Otherwise, `false`.


```cpp
consteval bool is_class_member(info entity);
consteval bool is_namespace_member(info entity);
consteval bool is_nonstatic_data_member(info r);
consteval bool is_static_member(info r);
consteval bool is_base(info r);
consteval bool is_constructor(info r);
consteval bool is_destructor(info r);
consteval bool is_special_member(info r);
```

[#]{.pnum} *Returns*: `true` if `r` designates a class member, namespace member, non-static data member, static member, base class member, constructor, destructor, or special member, respectively. Otherwise, `false`.

```cpp
consteval info type_of(info r);
```

[#]{.pnum} *Mandates*: `r` designates a typed entity. `r` does not designate a constructor or destructor.

[#]{.pnum} *Returns*: A reflection of the type of that entity.  If every declaration of that entity was declared with the same type alias (but not a template parameter substituted by a type alias), the reflection returned is for that alias.  Otherwise, if some declaration of that entity was declared with an alias it is unspecified whether the reflection returned is for that alias or for the type underlying that alias. Otherwise, the reflection returned shall not be a type alias reflection.

```cpp
consteval info parent_of(info r);
```

[#]{.pnum} *Mandates*: `r` designates a member of a class or a namespace.

[#]{.pnum} *Returns*: A reflection of the that entity's immediately enclosing class or namespace.

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
template<class... Fs>
  consteval vector<info> members_of(info r, Fs... filters);
```

[#]{.pnum} *Mandates*: `r` is a reflection designating either a class type or a namespace and `(std::predicate<Fs, info> && ...)` is `true`.

[#]{.pnum} *Returns*: A `vector` containing the reflections of all the direct members `m` of the entity designated by `r` such that `(filters(m) && ...)` is `true`.
Non-static data members are indexed in the order in which they are declared, but the order of other kinds of members is unspecified. [Base classes are not members.]{.note}

```cpp
template<class... Fs>
  consteval vector<info> accessible_members_of(info type, Fs... filters);
```

[#]{.pnum} *Mandates*: `type` is a reflection designating a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_accessible, filters...);`

```cpp
template<class... Fs>
  consteval vector<info> bases_of(info type, Fs... filters);
```

[#]{.pnum} *Mandates*: `type` is a reflection designating a type and `(std::predicate<Fs, info> && ...)` is `true`.

[#]{.pnum} *Returns*: Let `C` be the type designated by `type`. A `vector` containing the reflections of all the direct base classes `b`, if any, of `C` such that `(filters(b) && ...)` is `true`.
The base classes are indexed in the order in which they appear in the *base-specifier-list* of `C`.

```cpp
template<class... Fs>
  consteval vector<info> accessible_bases_of(info type, Fs... filters);
```

[#]{.pnum} *Effects*: Equivalent to: `return bases_of(r, is_accessible, filters...);`

```cpp
consteval vector<info> static_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` is a reflection designating a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_variable);`

```cpp
consteval vector<info> accessible_static_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_variable, is_accessible);`

```cpp
consteval vector<info> nonstatic_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_nonstatic_data_member);`

```cpp
consteval vector<info> accessible_nonstatic_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_nonstatic_data_member, is_accessible);`

```cpp
consteval vector<info> subobjects_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Returns*: A `vector` containing all the reflections in `bases_of(type)` followed by all the reflections in `nonstatic_data_members_of(type)`.

```cpp
consteval vector<info> accessible_subobjects_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Returns*: A `vector` containing all the reflections in `accessible_bases_of(type)` followed by all the reflections in `accessible_nonstatic_data_members_of(type)`.

```cpp
consteval vector<info> enumerators_of(info enum_type);
```

[#]{.pnum} *Mandates*: `enum_type` designates an enumeration.

[#]{.pnum} *Returns*: A `vector` containing the reflections of each enumerator of the enumeration designated by `enum_type`, in the order in which they are declared.
:::
:::

### [meta.reflection.substitute] Reflection substitution  {-}

::: std
::: addu
```cpp
consteval bool can_substitute(info templ, span<const info> arguments);
```
[1]{.pnum} *Mandates*: `templ` designates a template.

[#]{.pnum} Let `Z` be the template designated by `templ` and let `Args...` be the sequence of entities or expressions designated by the elements of `arguments`.

[#]{.pnum} *Returns*: `true` if `Z<Args...>` is a valid *template-id* ([temp.names]). Otherwise, `false`.

[#]{.pnum} *Remarks*: If attempting to substitute leads to a failure outside of the immediate context, the program is ill-formed.

```cpp
consteval info substitute(info templ, span<const info> arguments);
```

[#]{.pnum} *Mandates*: `can_substitute(templ, arguments)` is `true`.

[#]{.pnum} Let `Z` be the template designated by `templ` and let `Args...` be the sequence of entities or expressions designated by the elements of `arguments`.

[#]{.pnum} *Returns*: `^Z<Args...>`.

:::
:::

### [meta.reflection.unary] Unary type traits  {-}

::: std
::: addu
[1]{.pnum} Subclause [meta.reflection.unary] contains consteval functions that may be used to query the properties of a type at compile time.

[2]{.pnum} For each function taking an argument of type `meta::info` whose name contains `type`, that argument shall be a reflection of a type or type alias. For each function taking an argument of type `span<const meta::info>` named `type_args`, each `meta::info` in that `span` shall be a reflection of a type or a type alias.
:::
:::

#### [meta.reflection.unary.cat] Primary type categories  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$TRAIT$_type` defined in this clause, `std::meta::$TRAIT$_type(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.cat]{.sref}.

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
```

[2]{.pnum}

::: example
```
// an example implementation
namespace std::meta {
  consteval bool is_void_type(info type) {
    return value_of<bool>(substitute(^is_void_v, {type}));
  }
}
```
:::
:::
:::

#### [meta.reflection.unary.comp] Composite type categories  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$TRAIT$_type` defined in this clause, `std::meta::$TRAIT$_type(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.comp]{.sref}.

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
[1]{.pnum} For any type `T`, for each function `std::meta::$UNARY-TRAIT$_type` defined in this clause with signature `bool(std::meta::info)`, `std::meta::$UNARY-TRAIT$_type(^T)` equals the value of the corresponding type property `std::$UNARY-TRAIT$_v<T>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any types `T` and `U`, for each function `std::meta::$BINARY-TRAIT$_type` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::$BINARY-TRAIT$_type(^T, ^U)` equals the value of the corresponding type property `std::$BINARY-TRAIT$_v<T, U>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, for each function `std::meta::$VARIADIC-TRAIT$_type` defined in this clause with signature `bool(std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-TRAIT$_type(^T, {^U...})` equals the value of the corresponding type property `std::$VARIADIC-TRAIT$_v<T, U...>` as specified in [meta.unary.prop]{.sref}.

```cpp
consteval bool is_const_type(info type);
consteval bool is_volatile_type(info type);
consteval bool is_trivial_type(info type);
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

consteval bool is_constructible_type(info type, span<info const> type_args);
consteval bool is_default_constructible_type(info type);
consteval bool is_copy_constructible_type(info type);
consteval bool is_move_constructible_type(info type);

consteval bool is_assignable_type(info dst_type, info src_type);
consteval bool is_copy_assignable_type(info type);
consteval bool is_move_assignable_type(info type);

consteval bool is_swappable_with_type(info dst_type, info src_type);
consteval bool is_swappable_type(info type);

consteval bool is_destructible_type(info type);

consteval bool is_trivially_constructible_type(info type, span<info const> type_args);
consteval bool is_trivially_default_constructible_type(info type);
consteval bool is_trivially_copy_constructible_type(info type);
consteval bool is_trivially_move_constructible_type(info type);

consteval bool is_trivially_assignable_type(info dst_type, info src_type);
consteval bool is_trivially_copy_assignable_type(info type);
consteval bool is_trivially_move_assignable_type(info type);
consteval bool is_trivially_destructible_type(info type);

consteval bool is_nothrow_constructible_type(info type, span<info const> type_args);
consteval bool is_nothrow_default_constructible_type(info type);
consteval bool is_nothrow_copy_constructible_type(info type);
consteval bool is_nothrow_move_constructible_type(info type);

consteval bool is_nothrow_assignable_type(info dst_type, info src_type);
consteval bool is_nothrow_copy_assignable_type(info type);
consteval bool is_nothrow_move_assignable_type(info type);

consteval bool is_nothrow_swappable_with_type(info dst_type, info src_type);
consteval bool is_nothrow_swappable_type(info type);

consteval bool is_nothrow_destructible_type(info type);

consteval bool is_implicit_lifetime_type(info type);

consteval bool has_virtual_destructor_type(info type);

consteval bool has_unique_object_representations_type(info type);

consteval bool reference_constructs_from_temporary_type(info dst_type, info src_type);
consteval bool reference_converts_from_temporary_type(info dst_type, info src_type);
```
:::
:::

#### [meta.reflection.unary.prop.query] Type property queries  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$PROP$_type` defined in this clause with signature `size_t(std::meta::info)`, `std::meta::$PROP$_type(^T)` equals the value of the corresponding type property `std::$PROP$_v<T>` as specified in [meta.unary.prop.query]{.sref}.

[#]{.pnum} For any type `T` and unsigned integer value `I`, `std::meta::extent_type(^T, I)` equals `std::extent_v<T, I>` ([meta.unary.prop.query]).

```cpp
consteval size_t alignment_of_type(info type);
consteval size_t rank_type(info type);
consteval size_t extent_type(info type, unsigned i = 0);
```
:::
:::

### [meta.reflection.rel], Type relations  {-}

::: std
::: addu
[1]{.pnum} The consteval functions specified in this clause may be used to query relationships between types at compile time.

[#]{.pnum} For any types `T` and `U`, for each function `std::meta::$REL$_type` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::$REL$_type(^T, ^U)` equals the value of the corresponding type relation `std::$REL$_v<T, U>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, for each function `std::meta::$VARIADIC-REL$_type` defined in this clause with signature `bool(std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-REL$_type(^T, {^U...})` equals the value of the corresponding type relation `std::$VARIADIC-REL$_v<T, U...>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any types `T` and `R` and pack of types `U...`, for each function `std::meta::$VARIADIC-REL-R$_type` defined in this clause with signature `bool(std::meta::info, std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-REL-R$_type(^R, ^T, {^U...})` equals the value of the corresponding type relation `std::$VARIADIC-REL-R$_v<R, T, U...>` as specified in [meta.rel]{.sref}.

```cpp
consteval bool is_same_type(info type1, info type2);
consteval bool is_base_of_type(info base_type, info derived_type);
consteval bool is_convertible_type(info src_type, info dst_type);
consteval bool is_nothrow_convertible_type(info src_type, info dst_type);
consteval bool is_layout_compatible_type(info type1, info type2);
consteval bool is_pointer_interconvertible_base_of_type(info base_type, info derived_type);

consteval bool is_invocable_type(info type, span<const info> type_args);
consteval bool is_invocable_r_type(info result_type, info type, span<const info> type_args);

consteval bool is_nothrow_invocable_type(info type, span<const info> type_args);
consteval bool is_nothrow_invocable_r_type(info result_type, info type, span<const info> type_args);
```

[#]{.pnum} [If `t` is a reflection of the type `int` and `u` is a reflection of an alias to the type `int`, then `t == u` is `false` but `is_same(t, u)` is `true`. `t == dealias(u)` is also `true`.]{.note}.
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
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.cv]{.sref}.

```cpp
consteval info remove_const_type(info type);
consteval info remove_volatile_type(info type);
consteval info remove_cv_type(info type);
consteval info add_const_type(info type);
consteval info add_volatile_type(info type);
consteval info add_cv_type(info type);
```
:::
:::

#### [meta.reflection.trans.ref], Reference modifications  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ref]{.sref}.

```cpp
consteval info remove_reference_type(info type);
consteval info add_lvalue_reference_type(info type);
consteval info add_rvalue_reference_type(info type);
```
:::
:::

#### [meta.reflection.trans.sign], Sign modifications  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.sign]{.sref}.
```cpp
consteval info make_signed_type(info type);
consteval info make_unsigned_type(info type);
```
:::
:::

#### [meta.reflection.trans.arr], Array modifications  {-}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.arr]{.sref}.
```cpp
consteval info remove_extent_type(info type);
consteval info remove_all_extents_type(info type);
```
:::
:::

#### [meta.reflection.trans.ptr], Pointer modifications  {-}
::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ptr]{.sref}.
```cpp
consteval info remove_pointer_type(info type);
consteval info add_pointer_type(info type);
```
:::
:::

#### [meta.reflection.trans.other], Other transformations  {-}

[There are four transformations that are deliberately omitted here. `type_identity` and `enable_if` are not useful, `conditional(cond, t, f)` would just be a long way of writing `cond ? t : f`, and `basic_common_reference` is a class template intended to be specialized and not directly invoked.]{.ednote}

::: std
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$_type` defined in this clause with signature `std::meta::info(std::meta::info)`, `std::meta::$MOD$_type(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any pack of types `T...`, for each function `std::meta::$VARIADIC-MOD$_type` defined in this clause with signature `std::meta::info(std::span<const std::meta::info>)`, `std::meta::$VARIADIC-MOD$_type({^T...})` returns the reflection of the corresponding type `std::$VARIADIC-MOD$_t<T...>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, `std::meta::invoke_result_type(^T, {^u...})` returns the reflection of the corresponding type `std::invoke_result_t<T, U...>` ([meta.trans.other]{.sref}).

```cpp
consteval info remove_cvref_type(info type);
consteval info decay_type(info type);
consteval info common_type_type(span<const info> type_args);
consteval info common_reference_type(span<const info> type_args);
consteval info underlying_type_type(info type);
consteval info invoke_result_type(info type, span<const info> type_args);
consteval info unwrap_reference_type(info type);
consteval info unwrap_ref_decay_type(info type);
```

[#]{.pnum}

::: example
```cpp
// example implementation
consteval info unwrap_reference_type(info type) {
  type = dealias(type);
  if (has_template_arguments(type) && template_of(type) == ^reference_wrapper) {
    return add_lvalue_reference_type(template_arguments_of(type)[0]);
  } else {
    return type;
  }
}
```
:::

:::
:::
