---
title: "Reflection for C++26"
document: P2996R2
date: today
audience: EWG
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

Since [@P2996R1], several changes to the overall library API:

* added `qualified_name_of` (to partner with `name_of`)
* removed `is_static` for being ambiguous, added `has_internal_linkage` (and `has_linkage` and `has_external_linkage`) and `is_static_member` instead
* added `is_class_member` and `is_namespace_member`

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
Nearly all of the examples below have links to compiler explorer demonstrating them.

The implementation is not complete (notably, for debugging purposes, `name_of(^int)` yields an empty string and `name_of(^std::optional<std::string>)` yields `"optional"`, neither of which are what we want).
The implementation will evolve along with this paper.
The EDG implementation also lacks some of the other language features we would like to be able to take advantage of.
In particular, it does not support expansion statements.
A workaround that will be used in the linked implementations of examples is the following facility:

::: bq
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
  template for (constexpr auto e : std::meta::members_of(^E)) {
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

::: bq
```c++
constexpr auto r = ^int;
typename[:r:] x = 42;       // Same as: int x = 42;
typename[:^char:] c = '*';  // Same as: char c = '*';
```
:::

The `typename` prefix can be omitted in the same contexts as with dependent qualified names.  For example:

:::bq
```c++
using MyType = [:sizeof(int)<sizeof(long)? ^long : ^int:];  // Implicit "typename" prefix.
```
:::

[On Compiler Explorer](https://godbolt.org/z/13anqE1Pa).


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

[On Compiler Explorer](https://godbolt.org/z/vT4rbva7M)

## List of Types to List of Sizes

Here, `sizes` will be a `std::array<std::size_t, 3>` initialized with `{sizeof(int), sizeof(float), sizeof(double)}`:

::: bq
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

::: bq
```c++
template<class...> struct list {};

using types = list<int, float, double>;

constexpr auto sizes = []<template<class...> class L, class... T>(L<T...>) {
    return std::array<std::size_t, sizeof...(T)>{{ sizeof(T)... }};
}(types{});
```
:::

[On Compiler Explorer](https://godbolt.org/z/4xz9Wsa8f).

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

[On Compiler Explorer](https://godbolt.org/z/bvPeqvaK5).

## Getting Class Layout

::: bq
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

[On Compiler Explorer](https://godbolt.org/z/rbbWY99TM).

## Enum to String

One of the most commonly requested facilities is to convert an enum value to a string (this example relies on expansion statements):

::: bq
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  template for (constexpr auto e : std::meta::members_of(^E)) {
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

::: bq
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::optional<E> string_to_enum(std::string_view name) {
  template for (constexpr auto e : std::meta::members_of(^E)) {
    if (name == std::meta::name_of(e)) {
      return [:e:];
    }
  }

  return std::nullopt;
}
```
:::

But we don't have to use expansion statements - we can also use algorithms. For instance, `enum_to_string` can also be implemented this way (this example relies on non-transient constexpr allocation):

::: bq
```c++
template <typename E>
  requires std::is_enum_v<E>
constexpr std::string enum_to_string(E value) {
  constexpr auto enumerators =
    std::meta::members_of(^E)
    | std::views::transform([](std::meta::info e){
        return std::pair<E, std::string>(std::meta::value_of<E>(e), std::meta::name_of(e));
      })
    | std::ranges::to<std::map>();

  auto it = enumerators.find(value);
  if (it != enumerators.end()) {
    return it->second;
  } else {
    return "<unnamed>";
  }
}
```
:::

Note that this last version has lower complexity: While the versions using an expansion statement use an expected O(N) number of comparisons to find the matching entry, a `std::map` achieves the same with O(log(N)) complexity (where N is the number of enumerator constants).

[On Compiler Explorer](https://godbolt.org/z/Y5va8MqzG).

## Parsing Command-Line Options

Our next example shows how a command-line option parser could work by automatically inferring flags based on member names. A real command-line parser would of course be more complex, this is just the beginning.

::: bq
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

[On Compiler Explorer](https://godbolt.org/z/G4dh3jq8a).

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

[On Compiler Explorer](https://godbolt.org/z/4P15rnbxh).

## A Simple Variant Type

Similarly to how we can implement a tuple using `define_class` to create on the fly a type with one member for each `Ts...`, we can implement a variant that simply defines a `union` instead of a `struct`.
One difference here is how the destructor of a `union` is currently defined:

::: bq
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

::: bq
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

::: bq
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

::: bq
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

::: bq
```cpp
: storage{.[: get_nth_field(0) :]={}}
```
:::

Arguably, the answer should be yes - this would be consistent with how other accesses work.

[On Compiler Explorer](https://godbolt.org/z/Efz5vsjaa).

## Struct to Struct of Arrays

::: bq
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

::: bq
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

[On Compiler Explorer](https://godbolt.org/z/8rT77KxjP).


## Parsing Command-Line Options II

Now that we've seen a couple examples of using `std::meta::define_class` to create a type, we can create a more sophisticated command-line parser example.

This is the opening example for [clap](https://docs.rs/clap/latest/clap/) (Rust's **C**ommand **L**ine **A**rgument **P**arser):

::: bq
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

::: bq
```c++
struct Flags {
  bool use_short;
  bool use_long;
};

// type that has a member optional<T> with some suitable constructors and members
template <typename T, Flags flags>
struct Option;

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

[On Compiler Explorer](https://godbolt.org/z/1esbcq4jq).

## A Universal Formatter

This example is taken from Boost.Describe:

::: bq
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
      out = std::format_to(out, "{}", static_cast<[:base:] const&>(t));
    }

    template for (constexpr auto mem : nonstatic_data_members_of(^T)) {
      delim();
      out = std::format_to(out, ".{}={}", name_of(mem), t.[:mem:]);
    }

    *out++ = '}';
    return out;
  }
};

struct X { int m1 = 1; };
struct Y { int m2 = 2; };
class Z : public X, private Y { int m3 = 3; int m4 = 4; };

template <> struct std::formatter<X> : universal_formatter { };
template <> struct std::formatter<Y> : universal_formatter { };
template <> struct std::formatter<Z> : universal_formatter { };

int main() {
    std::println("{}", Z()); // Z{X{.m1 = 1}, Y{.m2 = 2}, .m3 = 3, .m4 = 4}
}
```
:::

This example is not implemented on compiler explorer at this time, but only because of issues compiling both `std::format` and `fmt::format.`

## Implementing member-wise `hash_append`

Based on the [@N3980] API:

::: bq
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

::: bq
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

::: bq
```cpp
consteval auto struct_to_tuple_type(info type) -> info {
  return substitute(^std::tuple,
                    nonstatic_data_members_of(type)
                    | std::ranges::transform(std::meta::type_of)
                    | std::ranges::transform(std::meta::remove_cvref)
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

[On Compiler Explorer](https://godbolt.org/z/Moqf84nc1), with a different implementation than either of the above.

## Compile-Time Ticket Counter

The features proposed here make it a little easier to update a ticket counter at compile time.
This is not an ideal implementation (we'd prefer direct support for compile-time —-- i.e., `consteval` --- variables), but it shows how compile-time mutable state surfaces in new ways.

::: bq
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

[On Compiler Explorer](https://godbolt.org/z/1vEjW4sTr).

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

:::bq
```c++
typename[: ^:: :] x = 0;  // Error.
```
:::

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

::: bq
```c++
make_tuple(t.[:members[0]:], t.[:members[1]:], ..., t.[:members[$N-1$]:])
```
:::

This is a very useful facility indeed!

However, range splicing of dependent arguments is at least an order of magnitude harder to implement than ordinary splicing. We think that not including range splicing gives us a better chance of having reflection in C++26.
Especially since, as this paper's examples demonstrate, a lot can be done without them.

Another way to work around a lack of range splicing would be to implement `with_size<N>(f)`, which would behave like `f(integral_constant<size_t, 0>{}, integral_constant<size_t, 0>{}, ..., integral_constant<size_t, N-1>{})`.
Which is enough for a tolerable implementation:

::: bq
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
On the flip side, it requires a special rule like the one that was made to handle `<::` to leave the meaning of `arr[::N]` unchanged.

A syntax that is delimited on the left and right is useful here because spliced expressions may involve lower-precedence operators.
However, there are other possibilities.
For example, now that `$`{.op} is available in the basic source character set, we might consider `@[$]{.op}@<$expr$>`.
This is somewhat natural to those of us that have used systems where `$`{.op} is used to expand placeholders in document templates.  For example:

::: bq
```c++
@[$]{.op}@select_type(3) *ptr = nullptr;
```
:::

The prefixes `typename` and `template` are only strictly needed in some cases where the operand of the splice is a dependent expression.
In our proposal, however, we only make `typename` optional in the same contexts where it would be optional for qualified names with dependent name qualifiers.
That has the advantage to catch unfortunate errors while keeping a single rule and helping human readers parse the intended meaning of otherwise ambiguous constructs.


## `std::meta::info`

The type `std::meta::info` can be defined as follows:

::: bq
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

Notably absent at this time are general non-constant expressions (that aren't *expression-id*s referring to functions, variables or structured bindings).  For example:

::: bq
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




## Metafunctions

We propose a number of metafunctions declared in namespace `std::meta` to operator on reflection values.
Adding metafunctions to an implementation is expected to be relatively "easy" compared to implementing the core language features described previously.
However, despite offering a normal consteval C++ function interface, each on of these relies on "compiler magic" to a significant extent.

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

::: bq
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

::: bq
```cpp
using A = int;
```
:::

In C++ today, `A` and `int` can be used interchangeably and there is no distinction between the two types.
With reflection as proposed in this paper, that will no longer be the case.
`^A` yields a reflection of an alias to `int`, while `^int` yields a reflection of `int`.
`^A == ^int` evaluates to `false`, but there will be a way to strip aliases - so `dealias(^A) == ^int` evaluates to `true`.

This opens up the question of how various other metafunctions handle aliases and it is worth going over a few examples:

::: bq
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

### Synopsis

Here is a synopsis for the proposed library API. The functions will be explained below.

::: bq
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

  // @[substitute](#substitute)@
  consteval auto substitute(info templ, span<info const> args) -> info;

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

::: bq
```cpp
consteval auto do_typeof(std::meta::info r) -> std::meta::info {
  return remove_cvref(is_type(r) ? r : type_of(r));
}

#define typeof(e) [: do_typeof(^e) :]
```
:::

If `r` designates a member of a class or namespace, `parent_of(r)` is a reflection designating its immediately enclosing class or namespace.

If `r` designates an alias, `dealias(r)` designates the underlying entity.
Otherwise, `dealias(r)` produces `r`.
`dealias` is recursive - it strips all aliases:

::: bq
```cpp
using X = int;
using Y = X;
static_assert(dealias(^int) == ^int);
static_assert(dealias(^X) == ^int);
static_assert(dealias(^Y) == ^int);
```
:::

### `template_of`, `template_arguments_of`

::: bq
```c++
namespace std::meta {
  consteval auto template_of(info r) -> info;
  consteval auto template_arguments_of(info r) -> vector<info>;
}
```
:::

If `r` is a reflection designated a type that is a specialization of some template, then `template_of(r)` is a reflection of that template and `template_arguments_of(r)` is a vector of the reflections of the template arguments. In other words, the preconditions on both is that `has_template_arguments(r)` is `true`.

For example:

::: bq
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


### `substitute`

:::bq
```c++
namespace std::meta {
  consteval auto substitute(info templ, span<info const> args) -> info;
}
```
:::

Given a reflection for a template and reflections for template arguments that match that template, `substitute` returns a reflection for the entity obtained by substituting the given arguments in the template.
If the template is a concept template, the result is a reflection of a constant of type `bool`.

For example:

::: bq
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

If `r` is a reflection of an enumerator constant of type `E`, `value_of<E>(r)` evaluates to the value of that enumerator.

If `r` is a reflection of a non-bit-field non-reference non-static member of type `M` in a class `C`, `value_of<M C::*>(r)` is the pointer-to-member value for that nonstatic member.

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

::: bq
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
For instance, a few examples in this paper use `std::meta::remove_cvref(t)` as if that exists.
Technically, the functionality isn't strictly necessary - since it can be provided indirectly:

::: cmptable
### Direct
```cpp
std::meta::remove_cvref(type)
```

### Indirect
```cpp
std::meta::substitute(^std::remove_cvref_t, {type})
```

---

```cpp
std::meta::is_const(type)
```

```cpp
std::meta::value_of<bool>(std::meta::substitute(^std::is_const_v, {type}))
std::meta::test_type(^std::is_const_v, type)
```
:::

Having `std::meta::meow` for every trait `std::meow` is more straightforward and will likely be faster to compile, though means we will have a much larger library API.
There are quite a few traits in [meta]{.sref} - but it should be easy enough to specify all of them.
So we're doing it.


# Proposed Wording

## Language

### [lex.phases] Phases of translation

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

:::bq

[7]{.pnum} Whitespace characters separating tokens are no longer significant. Each preprocessing token is converted into a token (5.6). The resulting tokens constitute a translation unit and are syntactically and semantically analyzed and translated.
[ Plainly constant-evaluated expressions ([expr.const]) appearing outside template declarations are evaluated in lexical order.]{.addu}
[...]

[8]{.pnum} [...]
All the required instantiations are performed to produce instantiation units.
[ Plainly constant-evaluated expressions ([expr.const]) appearing in those instantiation units are evaluated in lexical order as part of the instantion process.]{.addu}
[...]

:::

### [lex.pptoken] Preprocessing tokens

Add a bullet after [lex.pptoken]{.sref} bullet (3.2):

::: bq
  ...

  --- Otherwise, if the next three characters are `<::` and the subsequent character is neither `:` nor `>`, the `<` is treated as a preprocessing token by itself and not as the first character of the alternative token `<:`.

:::addu
  --- Otherwise, if the next three characters are `[::` and the subsequent character is not `:`, the `[` is treated as a preprocessing token by itself and not as the first character of the preprocessing token `[:`.
:::
  ...
:::

### [lex.operators] Operators and punctuators

Change the grammar for `$operator-or-punctuator$` in paragraph 1 of [lex.operators]{.sref} to include splicer delimiters:

::: bq
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

### [basic.types.general]

Change the first sentence in paragraph 9 of [basic.types.general]{.sref} as follows:

::: bq
[9]{.pnum} Arithmetic types (6.8.2), enumeration types, pointer types, pointer-to-member types (6.8.4),[ `std::meta::info`,]{.addu} `std::nullptr_t`, and cv-qualified (6.8.5) versions of these types are collectively called scalar types.
:::

Add a new paragraph at the end of [basic.types.general]{.sref} as follows:

::: bq
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

### [basic.fundamental] Fundamental types

Add a new paragraph before the last paragraph of [basic.fundamental]{.sref} as follows:

::: bq
::: addu

[*]{.pnum} A value of type `std::meta::info` is called a _reflection_ and represents a language element such as a type, a constant value, a non-static data member, etc.
`sizeof(std::meta::info)` shall be equal to `sizeof(void*)`.
[*Note*:
Reflections are only meaningful during translation.
The notion of *consteval-only* types (see [basic.types.general]{.sref}) exists to diagnose attempts at using such values outside the translation process.]

:::
:::

### [basic.lookup.argdep] Argument-dependent name lookup

Add a bullet after the first in paragraph 3 of [basic.lookup.argdep] as follows:
::: bq
[3]{.pnum} ... Any `$typedef-name$`s and `$using-declaration$`s used to specify the types do not contribute to this set. The set of entities is determined in the following way:

- [3.1]{.pnum} If `T` is a fundamental type, its associated set of entities is empty.
::: addu
- [3.2]{.pnum} If `T` is `std::meta::info`, its associated set of entities is the singleton containing function `std::meta::is_type`.
:::
- [3.3]{.pnum} If `T` is a class type ...

:::

### [expr.prim] Primary expressions

Change the grammar for `$primary-expression$` in [expr.prim]{.sref} as follows:

::: bq
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

### [expr.prim.splice] Expression splicing

Add a new subsection of [expr.prim]{.sref} following [expr.prim.req]{.sref}

::: bq
::: addu
**Expression Splicing   [expr.prim.splice]**

[#]{.pnum} For a `$primary-expression$` of the form `[: $constant-expression$ :]` or `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` the `$constant-expression$` shall be a converted constant expression ([expr.const]{.sref}) of type `std::meta::info`.

[#]{.pnum} For a `$primary-expression$` of the form `template[: $constant-expression$ :]  < $template-argument-list$@~_opt_~@ >` the converted `$constant-expression$` shall evaluate to a reflection for a concept, variable template, class template, alias template, or function template.
The meaning of such a construct is identical to that of a `$primary-expression$` of the form `$template-name$ < $template-argument-list$@~_opt_~@ >` where `$template-name$` denotes the reflected template or concept (ignoring access checking on the `$template-name$`).

[#]{.pnum} For a `$primary-expression$` of the form `[: $constant-expression$ :]` where the converted `$constant-expression$` evaluates to a reflection for a variable, a function, an enumerator, or a structured binding, the meaning of the expression is identical to that of a `$primary-expression$` of the form `$id-expression$` that would denote the reflected entity (ignoring access checking).

[#]{.pnum} Otherwise, for a `$primary-expression$` of the form `[: $constant-expression$ :]` the converted `$constant-expression$` shall evaluate to a reflection for a constant value and the expression shall evaluate to that value.
:::
:::


### [expr.unary.general]

Change [expr.unary.general]{.sref} paragraph 1 to add productions for the new operator:

::: bq
[1]{.pnum} Expressions with unary operators group right-to-left.
```diff
  $unary-expression$:
     ...
     $delete-expression$
+    ^ ::
+    ^ $namespace-name$
+    ^ $nested-name-specifier$@~_opt_~@ $template-name$
+    ^ $nested-name-specifier$@~_opt_~@ $concept-name$
+    ^ $type-id$
+    ^ $cast-expression$
```
:::

### [expr.reflect] The reflection operator

Add a new subsection of [expr.unary]{.sref} following [expr.delete]{.sref}

::: bq
::: addu
**The Reflection Operator   [expr.reflect]**

[#]{.pnum} The unary `^` operator (called _the reflection operator_) produces a prvalue --- called _reflection_ --- whose type is the reflection type (i.e., `std::meta::info`).
That reflection represents its operand.

[#]{.pnum} An ambiguity can arise between the interpretation of the operand of the reflection operator as a `$type-id$` or a `$cast-expression$`; in such cases, the `$type-id$` treatment is chosen.
Parentheses can be introduced to force the `$cast-expression$` interpretation.


[#]{.pnum} [*Example*
```
static_assert(is_type(^int()));    // ^ applies to the type-id "int()"; not the cast "int()"
static_assert(!is_type(^(int()))); // ^ applies to the the cast-expression "(int())"

template<bool> struct X;
consteval void g(std::meta::info r) {
  if (r == ^int && true);    // error: ^ applies to the type-id "int&&"
  if (r == (^int) && true);  // OK
  if (r == ^X < true);       // error: "<" is an angle bracket
  if (r == (^X) < true);     // OK
}


```
-*end example*]

[#]{.pnum} When applied to `::`, the reflection operator produces a reflection for the global namespace.
When applied to a `$namespace-name$`, the reflection produces a reflection for the indicated namespace or namespace alias.

[#]{.pnum} When applied to a `$template-name$`, the reflection produces a reflection for the indicated template.

[#]{.pnum} When applied to a `$concept-name$`, the reflection produces a reflection for the indicated concept.

[#]{.pnum} When applied to a `$type-id$`, the reflection produces a reflection for the indicated type or type alias.

[#]{.pnum} When applied to a `$cast-expression$`, the `$cast-expression$` shall be a constant expression ([expr.const]{.sref}) or an `$id-expression$` ([expr.prim.id]{.sref}) designating a variable, a function, an enumerator constant, or a nonstatic member.
The `$cast-expression$` is not evaluated.
If the operand of the reflection operator is an `$id-expression$`, the result is a reflection for the indicated entity.
If the operand is a constant expression, the result is a reflection for the resulting value.
If the operand is both an `$id-expression$` and a constant expression, the result is a reflection for both the indicated entity and the expression's (constant) value.

[ *Example*:
```
constexpr auto r = ^std::vector;
```
— *end example* ]
:::
:::

### [expr.eq] Equality Operators

Extend [expr.eq]{.sref}/2 to also handle `std::meta::info:

::: bq
[2]{.pnum} The converted operands shall have arithmetic, enumeration, pointer, or pointer-to-member type, or [type]{.rm} [types `std::meta::info` or ]{.addu} `std​::​nullptr_t`. The operators `==` and `!=` both yield `true` or `false`, i.e., a result of type `bool`. In each case below, the operands shall have the same type after the specified conversions have been applied.

:::

Add a new paragraph between [expr.eq]{.sref}/5 and /6:

::: bq
[5]{.pnum} Two operands of type `std​::​nullptr_t` or one operand of type `std​::​nullptr_t` and the other a null pointer constant compare equal.

::: addu
[*]{.pnum} If both operands are of type `std::meta::info`, comparison is defined as follows:

* [*.#]{.pnum} If one operand is a reflection of a namespace alias, alias template, or type alias and the other operand is not a reflection of the same kind of alias, they compare unequal. [A reflection of a type and a reflection of an alias to that same type do not compare equal.]{.note}
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


### [expr.const] Constant Expressions

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

### [dcl.typedef] The `typedef` specifier

Introduce the term "type alias" to [dcl.typedef]{.sref}:

::: bq
[1]{.pnum} [...] A name declared with the `typedef` specifier becomes a typedef-name. A typedef-name names the type associated with the identifier ([dcl.decl]) or simple-template-id ([temp.pre]); a typedef-name is thus a synonym for another type. A typedef-name does not introduce a new type the way a class declaration ([class.name]) or enum declaration ([dcl.enum]) does.

[2]{.pnum} A *typedef-name* can also be introduced by an alias-declaration. The identifier following the using keyword is not looked up; it becomes a typedef-name and the optional attribute-specifier-seq following the identifier appertains to that typedef-name. Such a typedef-name has the same semantics as if it were introduced by the typedef specifier. In particular, it does not define a new type.

::: addu
[*]{.pnum} A *type alias* is either a name declared with the `typedef` specifier or a name introduced by an *alias-declaration*.
:::
:::

## Library

### [over.built] Built-in operators

Add built-in operator candidates for `std::meta::info` to [over.built]{.sref}:

::: bq
[16]{.pnum} For every `T`, where `T` is a pointer-to-member type[, `std::meta::info`,]{.addu} or `std​::​nullptr_t`, there exist candidate operator functions of the form
```cpp
bool operator==(T, T);
bool operator!=(T, T);
```
:::

### Header `<meta>` synopsis

Add a new subsection in [meta]{.sref} after [type.traits]{.sref}:

::: bq
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
  consteval auto is_class_member(info entity) -> bool;
  consteval auto is_namespace_member(info entity) -> bool;
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
    consteval vector<info> bases_of(info type, Fs... filters);
  consteval vector<info> static_data_members_of(info type);
  consteval vector<info> nonstatic_data_members_of(info type);
  consteval vector<info> subobjects_of(info type);
  consteval vector<info> enumerators_of(info enum_type);

  // [meta.reflection.unary.cat], primary type categories
  consteval bool is_void(info type);
  consteval bool is_null_pointer(info type);
  consteval bool is_integral(info type);
  consteval bool is_floating_point(info type);
  consteval bool is_array(info type);
  consteval bool is_pointer(info type);
  consteval bool is_lvalue_reference(info type);
  consteval bool is_rvalue_reference(info type);
  consteval bool is_member_object_pointer(info type);
  consteval bool is_member_function_pointer(info type);
  consteval bool is_enum(info type);
  consteval bool is_union(info type);
  consteval bool is_class(info type);
  consteval bool is_function(info type);

  // [meta.reflection.unary.comp], composite type categories
  consteval bool is_reference(info type);
  consteval bool is_arithmetic(info type);
  consteval bool is_fundamental(info type);
  consteval bool is_object(info type);
  consteval bool is_scalar(info type);
  consteval bool is_compound(info type);
  consteval bool is_member_pointer(info type);

  // [meta.reflection unary.prop], type properties
  consteval bool is_const(info type);
  consteval bool is_volatile(info type);
  consteval bool is_trivial(info type);
  consteval bool is_trivially_copyable(info type);
  consteval bool is_standard_layout(info type);
  consteval bool is_empty(info type);
  consteval bool is_polymorphic(info type);
  consteval bool is_abstract(info type);
  consteval bool is_final(info type);
  consteval bool is_aggregate(info type);
  consteval bool is_signed(info type);
  consteval bool is_unsigned(info type);
  consteval bool is_bounded_array(info type);
  consteval bool is_unbounded_array(info type);
  consteval bool is_scoped_enum(info type);

  consteval bool is_constructible(info type, span<info const> type_args);
  consteval bool is_default_constructible(info type);
  consteval bool is_copy_constructible(info type);
  consteval bool is_move_constructible(info type);

  consteval bool is_assignable(info dst_type, info src_type);
  consteval bool is_copy_assignable(info type);
  consteval bool is_move_assignable(info type);

  consteval bool is_swappable_with(info dst_type, info src_type);
  consteval bool is_swappable(info type);

  consteval bool is_destructible(info type);

  consteval bool is_trivially_constructible(info type, span<info const> type_args);
  consteval bool is_trivially_default_constructible(info type);
  consteval bool is_trivially_copy_constructible(info type);
  consteval bool is_trivially_move_constructible(info type);

  consteval bool is_trivially_assignable(info dst_type, info src_type);
  consteval bool is_trivially_copy_assignable(info type);
  consteval bool is_trivially_move_assignable(info type);
  consteval bool is_trivially_destructible(info type);

  consteval bool is_nothrow_constructible(info type, span<info const> type_args);
  consteval bool is_nothrow_default_constructible(info type);
  consteval bool is_nothrow_copy_constructible(info type);
  consteval bool is_nothrow_move_constructible(info type);

  consteval bool is_nothrow_assignable(info dst_type, info src_type);
  consteval bool is_nothrow_copy_assignable(info type);
  consteval bool is_nothrow_move_assignable(info type);

  consteval bool is_nothrow_swappable_with(info dst_type, info src_type);
  consteval bool is_nothrow_swappable(info type);

  consteval bool is_nothrow_destructible(info type);

  consteval bool is_implicit_lifetime(info type);

  consteval bool has_virtual_destructor(info type);

  consteval bool has_unique_object_representations(info type);

  consteval bool reference_constructs_from_temporary(info dst_type, info src_type);
  consteval bool reference_converts_from_temporary(info dst_type, info src_type);

  // [meta.reflection.unary.prop.query], type property queries
  consteval size_t alignment_of(info type);
  consteval size_t rank(info type);
  consteval size_t extent(info type, unsigned i = 0);

  // [meta.reflection.rel], type relations
  consteval bool is_same(info type1, info type2);
  consteval bool is_base_of(info base_type, info derived_type);
  consteval bool is_convertible(info src_type, info dst_type);
  consteval bool is_nothrow_convertible(info src_type, info dst_type);
  consteval bool is_layout_compatible(info type1, info type2);
  consteval bool is_pointer_interconvertible_base_of(info base_type, info derived_type);

  consteval bool is_invocable(info type, span<const info> type_args);
  consteval bool is_invocable_r(info result_type, info type, span<const info> type_args);

  consteval bool is_nothrow_invocable(info type, span<const info> type_args);
  consteval bool is_nothrow_invocable_r(info result_type, info type, span<const info> type_args);

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
  consteval info common_type(span<const info> type_args);
  consteval info common_reference(span<const info> type_args);
  consteval info underlying_type(info type);
  consteval info invoke_result(info type, span<const info> type_args);
  consteval info unwrap_reference(info type);
  consteval info unwrap_ref_decay(info type);
}
```
:::
:::

### [meta.reflection.info] Reflections

::: bq
::: addu

[#]{.pnum} The type `std::meta::info` is a synonym for the type produced by the reflection operator ([expr.reflect]{.sref}), and it has the characteristics describes in [basic.types.general]{.sref} and [expr.eq]{.sref}.
:::
:::

### [meta.reflection.names] Reflection names and locations

::: bq
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

### [meta.reflection.queries] Reflection queries

::: bq
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
[#]{.pnum} *Returns*: TODO

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

[#]{.pnum} *Returns*: `true` if `r` designates a member function that is declared explicit. Otherwise, `false`.

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
consteval auto is_class_member(info entity) -> bool;
consteval auto is_namespace_member(info entity) -> bool;
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

[#]{.pnum} *Mandates*: `r` designates a typed entity.

[#]{.pnum} *Returns*: A reflection of the type of that entity.

```cpp
consteval info parent_of(info r);
```

[#]{.pnum} *Mandates*: `r` designates a member of a class or a namespace.

[#]{.pnum} *Returns*: A reflection of the that entity's immediately enclosing class or namespace.

```cpp
consteval info dealias(info r);
```

[#]{.pnum} *Returns*: If `r` designates a type alias or a namespace alias, a reflection designating the underlying entity. Otherwise, `r`.

[#]{.pnum} [*Example*
```
using X = int;
using Y = X;
static_assert(dealias(^int) == ^int);
static_assert(dealias(^X) == ^int);
static_assert(dealias(^Y) == ^int);
```
-*end example*]

```cpp
consteval info template_of(info r);
consteval vector<info> template_arguments_of(info r);
```
[#]{.pnum} *Mandates*: `has_template_arguments(r)` is `true`.

[#]{.pnum} *Returns*: A reflection of the template of `r`, and the reflections of the template arguments of, the specialization designated by `r`, respectively.

[#]{.pnum} [*Example*:
```
template <class T, class U=T> struct Pair { };
template <class T> using PairPtr = Pair<T*>;

static_assert(template_of(^Pair<int>) == ^Pair);
static_assert(template_arguments_of(^Pair<int>).size() == 2);

static_assert(template_of(^PairPtr<int>) == ^PairPtr);
static_assert(template_arguments_of(^PairPtr<int>).size() == 1);
```
-*end example*]
:::
:::

### [meta.reflection.member.queries], Reflection member queries

::: bq
::: addu
```cpp
template<class... Fs>
  consteval vector<info> members_of(info r, Fs... filters);
```
[#]{.pnum} *Mandates*: `r` is a reflection designating either a class type or a namespace and `(std::predicate<Fs, info> && ...)` is `true`.

[#]{.pnum} *Returns*: A `vector` containing the reflections of all the direct members `m` of the entity designated by `r` such that `(filters(m) && ...)` is `true`.
Data members are returned in the order in which they are declared, but the order of member functions and member types is unspecified. [Base classes are not members.]{.note}

```cpp
template<class... Fs>
  consteval vector<info> bases_of(info type, Fs... filters);
```

[#]{.pnum} *Mandates*: `type` designates a type and `(std::predicate<Fs, info> && ...)` is `true`.

[#]{.pnum} *Returns*: Let `C` be the type designated by `type`. A `vector` containing the reflections of all the direct base classes, if any, of `C` such that `(filters(class_type) && ...)` is `true`.
The base classes are returned in the order in which they appear the *base-specifier-list* of `C`.

```cpp
consteval vector<info> static_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_variable);`

```cpp
consteval vector<info> nonstatic_data_members_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Effects*: Equivalent to: `return members_of(type, is_nonstatic_data_member);`

```cpp
consteval vector<info> subobjects_of(info type);
```

[#]{.pnum} *Mandates*: `type` designates a type.

[#]{.pnum} *Returns*: A `vector` containing all the reflections in `bases_of(type)` followed by all the reflections in `nonstatic_data_members_of(type)`.

```cpp
consteval vector<info> enumerators_of(info enum_type);
```

[#]{.pnum} *Mandates*: `enum_type` designates an enumeration.

[#]{.pnum} *Returns*: A `vector` containing the reflections of each enumerator of the enumeration designated by `enum_type`, in the order in which they are declared.
:::
:::

### [meta.reflection.unary] Unary type traits

::: bq
::: addu
[1]{.pnum} Subclause [meta.reflection.unary] contains consteval functions that may be used to query the properties of a type at compile time.

[2]{.pnum} For each function taking an argument of type `meta::info` whose name contains `type`, that argument shall be a reflection of a type or type alias. For each function taking an argument of type `span<const meta::info>` named `type_args`, each `meta::info` in that `span` shall be a reflection of a type or a type alias.
:::
:::

#### [meta.reflection.unary.cat] Primary type categories

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$TRAIT$` defined in this clause, `std::meta::$TRAIT$(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.cat]{.sref}.

```cpp
consteval bool is_void(info type);
consteval bool is_null_pointer(info type);
consteval bool is_integral(info type);
consteval bool is_floating_point(info type);
consteval bool is_array(info type);
consteval bool is_pointer(info type);
consteval bool is_lvalue_reference(info type);
consteval bool is_rvalue_reference(info type);
consteval bool is_member_object_pointer(info type);
consteval bool is_member_function_pointer(info type);
consteval bool is_enum(info type);
consteval bool is_union(info type);
consteval bool is_class(info type);
consteval bool is_function(info type);
```

[2]{.pnum} [*Example*
```
// an example implementation
namespace std::meta {
  consteval bool is_void(info type) {
    return value_of<bool>(substitute(^is_void_v, {type}));
  }
}
```
*-end example*]
:::
:::

#### [meta.reflection.unary.comp] Composite type categories

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$TRAIT$` defined in this clause, `std::meta::$TRAIT$(^T)` equals the value of the corresponding unary type trait `std::$TRAIT$_v<T>` as specified in [meta.unary.comp]{.sref}.

```cpp
consteval bool is_reference(info type);
consteval bool is_arithmetic(info type);
consteval bool is_fundamental(info type);
consteval bool is_object(info type);
consteval bool is_scalar(info type);
consteval bool is_compound(info type);
consteval bool is_member_pointer(info type);
```
:::
:::

#### [meta.reflection.unary.prop] Type properties

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$UNARY-TRAIT$` defined in this clause with signature `bool(std::meta::info)`, `std::meta::$UNARY-TRAIT$(^T)` equals the value of the corresponding type property `std::$UNARY-TRAIT$_v<T>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any types `T` and `U`, for each function `std::meta::$BINARY-TRAIT$` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::$BINARY-TRAIT$(^T, ^U)` equals the value of the corresponding type property `std::$BINARY-TRAIT$_v<T, U>` as specified in [meta.unary.prop]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, for each function `std::meta::$VARIADIC-TRAIT$` defined in this clause with signature `bool(std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-TRAIT$(^T, {^U...})` equals the value of the corresponding type property `std::$VARIADIC-TRAIT$_v<T, U...>` as specified in [meta.unary.prop]{.sref}.

```cpp
consteval bool is_const(info type);
consteval bool is_volatile(info type);
consteval bool is_trivial(info type);
consteval bool is_trivially_copyable(info type);
consteval bool is_standard_layout(info type);
consteval bool is_empty(info type);
consteval bool is_polymorphic(info type);
consteval bool is_abstract(info type);
consteval bool is_final(info type);
consteval bool is_aggregate(info type);
consteval bool is_signed(info type);
consteval bool is_unsigned(info type);
consteval bool is_bounded_array(info type);
consteval bool is_unbounded_array(info type);
consteval bool is_scoped_enum(info type);

consteval bool is_constructible(info type, span<info const> type_args);
consteval bool is_default_constructible(info type);
consteval bool is_copy_constructible(info type);
consteval bool is_move_constructible(info type);

consteval bool is_assignable(info dst_type, info src_type);
consteval bool is_copy_assignable(info type);
consteval bool is_move_assignable(info type);

consteval bool is_swappable_with(info dst_type, info src_type);
consteval bool is_swappable(info type);

consteval bool is_destructible(info type);

consteval bool is_trivially_constructible(info type, span<info const> type_args);
consteval bool is_trivially_default_constructible(info type);
consteval bool is_trivially_copy_constructible(info type);
consteval bool is_trivially_move_constructible(info type);

consteval bool is_trivially_assignable(info dst_type, info src_type);
consteval bool is_trivially_copy_assignable(info type);
consteval bool is_trivially_move_assignable(info type);
consteval bool is_trivially_destructible(info type);

consteval bool is_nothrow_constructible(info type, span<info const> type_args);
consteval bool is_nothrow_default_constructible(info type);
consteval bool is_nothrow_copy_constructible(info type);
consteval bool is_nothrow_move_constructible(info type);

consteval bool is_nothrow_assignable(info dst_type, info src_type);
consteval bool is_nothrow_copy_assignable(info type);
consteval bool is_nothrow_move_assignable(info type);

consteval bool is_nothrow_swappable_with(info dst_type, info src_type);
consteval bool is_nothrow_swappable(info type);

consteval bool is_nothrow_destructible(info type);

consteval bool is_implicit_lifetime(info type);

consteval bool has_virtual_destructor(info type);

consteval bool has_unique_object_representations(info type);

consteval bool reference_constructs_from_temporary(info dst_type, info src_type);
consteval bool reference_converts_from_temporary(info dst_type, info src_type);
```
:::
:::

#### [meta.reflection.unary.prop.query] Type property queries

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$PROP$` defined in this clause with signature `size_t(std::meta::info)`, `std::meta::$PROP$(^T)` equals the value of the corresponding type property `std::$PROP$_v<T>` as specified in [meta.unary.prop.query]{.sref}.

[#]{.pnum} For any type `T` and unsigned integer value `I`, `std::meta::extent(^T, I)` equals `std::extent_v<T, I>` ([meta.unary.prop.query]).

```cpp
consteval size_t alignment_of(info type);
consteval size_t rank(info type);
consteval size_t extent(info type, unsigned i = 0);
```
:::
:::

### [meta.reflection.rel], Type relations

::: bq
::: addu
[1]{.pnum} The consteval functions specified in this clause may be used to query relationships between types at compile time.

[#]{.pnum} For any types `T` and `U`, for each function `std::meta::$REL$` defined in this clause with signature `bool(std::meta::info, std::meta::info)`, `std::meta::$REL$(^T, ^U)` equals the value of the corresponding type relation `std::$REL$_v<T, U>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, for each function `std::meta::$VARIADIC-REL$` defined in this clause with signature `bool(std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-REL$(^T, {^U...})` equals the value of the corresponding type relation `std::$VARIADIC-REL$_v<T, U...>` as specified in [meta.rel]{.sref}.

[#]{.pnum} For any types `T` and `R` and pack of types `U...`, for each function `std::meta::$VARIADIC-REL-R$` defined in this clause with signature `bool(std::meta::info, std::meta::info, std::span<const std::meta::info>)`, `std::meta::$VARIADIC-REL-R$(^R, ^T, {^U...})` equals the value of the corresponding type relation `std::$VARIADIC-REL-R$_v<R, T, U...>` as specified in [meta.rel]{.sref}.

```cpp
consteval bool is_same(info type1, info type2);
consteval bool is_base_of(info base_type, info derived_type);
consteval bool is_convertible(info src_type, info dst_type);
consteval bool is_nothrow_convertible(info src_type, info dst_type);
consteval bool is_layout_compatible(info type1, info type2);
consteval bool is_pointer_interconvertible_base_of(info base_type, info derived_type);

consteval bool is_invocable(info type, span<const info> type_args);
consteval bool is_invocable_r(info result_type, info type, span<const info> type_args);

consteval bool is_nothrow_invocable(info type, span<const info> type_args);
consteval bool is_nothrow_invocable_r(info result_type, info type, span<const info> type_args);
```

[#]{.pnum} [If `t` is a reflection of the type `int` and `u` is a reflection of an alias to the type `int`, then `t == u` is `false` but `is_same(t, u)` is `true`. `t == dealias(u)` is also `true`.]{.note}.
:::
:::


### [meta.reflection.trans], Transformations between types

::: bq
::: addu
[1]{.pnum} Subclause [meta.reflection.trans] contains consteval functions that may be used to transform one type to another following some predefined rule.
:::
:::

#### [meta.reflection.trans.cv], Const-volatile modifications
::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.cv]{.sref}.

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

#### [meta.reflection.trans.ref], Reference modifications

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ref]{.sref}.

```cpp
consteval info remove_reference(info type);
consteval info add_lvalue_reference(info type);
consteval info add_rvalue_reference(info type);
```
:::
:::

#### [meta.reflection.trans.sign], Sign modifications

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.sign]{.sref}.
```cpp
consteval info make_signed(info type);
consteval info make_unsigned(info type);
```
:::
:::

#### [meta.reflection.trans.arr], Array modifications

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.arr]{.sref}.
```cpp
consteval info remove_extent(info type);
consteval info remove_all_extents(info type);
```
:::
:::

#### [meta.reflection.trans.ptr], Pointer modifications
::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.ptr]{.sref}.
```cpp
consteval info remove_pointer(info type);
consteval info add_pointer(info type);
```
:::
:::

#### [meta.reflection.trans.other], Other transformations

[There are four transformations that are deliberately omitted here. `type_identity` and `enable_if` are not useful, `conditional(cond, t, f)` would just be a long way of writing `cond ? t : f`, and `basic_common_reference` is a class template intended to be specialized and not directly invoked.]{.ednote}

::: bq
::: addu
[1]{.pnum} For any type `T`, for each function `std::meta::$MOD$` defined in this clause with signature `std::meta::info(std::meta::info)`, `std::meta::$MOD$(^T)` returns the reflection of the corresponding type `std::$MOD$_t<T>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any pack of types `T...`, for each function `std::meta::$VARIADIC-MOD$` defined in this clause with signature `std::meta::info(std::span<const std::meta::info>)`, `std::meta::$VARIADIC-MOD$({^T...})` returns the reflection of the corresponding type `std::$VARIADIC-MOD$_t<T...>` as specified in [meta.trans.other]{.sref}.

[#]{.pnum} For any type `T` and pack of types `U...`, `std::meta::invoke_result(^T, {^u...})` returns the reflection of the corresponding type `std::invoke_result_t<T, U...>` ([meta.trans.other]{.sref}).

```cpp
consteval info remove_cvref(info type);
consteval info decay(info type);
consteval info common_type(span<const info> type_args);
consteval info common_reference(span<const info> type_args);
consteval info underlying_type(info type);
consteval info invoke_result(info type, span<const info> type_args);
consteval info unwrap_reference(info type);
consteval info unwrap_ref_decay(info type);
```

[#]{.pnum} [*Example*:

```cpp
// example implementation
consteval info unwrap_reference(info type) {
  if (has_template_arguments(type) && template_of(type) == ^reference_wrapper) {
    return add_lvalue_reference(template_arguments_of(type)[0]);
  } else {
    return type;
  }
}
```

*-end example*]
:::
:::
