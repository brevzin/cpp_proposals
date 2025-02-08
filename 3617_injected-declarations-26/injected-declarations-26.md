---
title: "Injected Declarations for C++26"
document: P3617R0
date: today
audience: EWG, CWG, LWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
tag: reflection
---

# Introduction

This paper is exists as a response to [@P3569R0]{.title}, and exists to take portions of three different papers:

* from [@P2996R9]{.title}, the language rules around injected declarations and the `define_aggregate()` facility,
* from [@P3289R1]{.title}, `consteval` blocks (which are the only place where we want to allow injection to happen from), and
* from [@P3394R1]{.title}, the `annotate()` function (which is the only part of that paper that does injection).

The goal of this paper is to, basically, streamline the process of getting reflection into C++26. The parts of P2996, without injection, are fairly well understood at this point and the wording is in good shape (both on the language and library side) to be able to make it to plenary in Hagenberg. This would be a massive achievement.

The injection part of reflection, even the minimal part being proposed as part of these papers, is still very valuable. But the rules we are trying to construct around it — to ensure that you can only inject declarations in sensible places and not allow you to observe aspects of compilation that there is no value to allow you to observe — are subtle, require thought, and have taken time to get right.

It's possible that we have those rules right already, and we can get both the reduced versions of p2996 and p3394 and also this paper into plenary and Hagenberg. But that's not strictly necessary from a timeline perspective. We would rather not delay p2996 and p3394 any further — let's try to get those into plenary and work on injection separately.

The rest of this paper will take examples, prose, and wording pulled from the other papers listed above.

# Examples

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

# Proposal Details

## Constant evaluation order

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

## Reachability and injected declarations

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


## Restrictions on injected declarations

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

## `data_member_spec` and `define_aggregate`

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

# Wording

The wording here is presented as a diff on top of [@P2996R10]{.title}.

## Language

### [lex.phases]{.sref} Phases of translation {-}

Modify the wording for phases 7-8 of [lex.phases]{.sref} as follows:

::: std
[7-8]{.pnum} [...]


  [The program is ill-formed if any instantiation fails.]{.note}

  [Constructs that are separately subject to instantiation are specified in [temp.inst].]{.note}

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

  [8]{.pnum} Translated translation units are combined and all external entity references are resolved. Library components are linked to satisfy external references to entities not defined in the current translation. All such translator output is collected into a program image which contains information needed for execution in its execution environment.
:::

### [basic.def]{.sref} Declarations and definitions {-}

Replace `$static_assert-declaration$` and `$empty-declaration$` with `$vacant-decalaration$`, which also encompasses consteval blocks:

::: std
[2]{.pnum} Each entity declared by a `$declaration$` is also _defined_ by that declaration unless:

* [#.#]{.pnum} it declares a function without specifying the function's body ([dcl.fct.def]),

[...]

* [2.10]{.pnum} it is an `$alias-declaration$` ([dcl.typedef]),
* [2.11-]{.pnum} it is a `$namespace-alias-definition$` ([namespace.alias])
* [2.11]{.pnum} it is a `$using-declaration$` ([namespace.udecl]),
* [2.12]{.pnum} it is a `$deduction-guide$` ([temp.deduct.guide]),
* [2.13]{.pnum} it is a [`$static_assert-declaration$`]{.rm} [`$vacant-declaration$`]{.addu} ([dcl.pre]),
* [2.14]{.pnum} it is an `$attribute-declaration$` ([dcl.pre]),
* [[2.15]{.pnum} it is an `$empty-declaration$` ([dcl.pre])]{.rm},
:::

### [basic.def.odr]{.sref} One-definition rule {-}

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

### [intro.execution]{.sref} Sequential execution {-}

Introduce a new kind of side effect in paragraph 7 (i.e., injecting a declaration).

::: std
[7]{.pnum} Reading an object designated by a `volatile` glvalue ([basic.lval]), modifying an object, [producing an injected declaration ([expr.const]),]{.addu} calling a library I/O function, or calling a function that does any of those operations are all _side effects_, which are changes in the state of the execution [or translation]{.addu} environment. _Evaluation_ of an expression (or a subexpression) in general includes both value computations (including determining the identity of an object for glvalue evaluation and fetching a value previously assigned to an object for prvalue evaluation) and initiation of side effects. When a call to a library I/O function returns or an access through a volatile glvalue is evaluated, the side effect is considered complete, even though some external actions implied by the call (such as the I/O itself) or by the `volatile` access may not have completed yet.

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

Add consteval block scopes to the scopes that introduce an immediate function context:

::: std
[24]{.pnum} An expression or conversion is in an _immediate function context_ if it is potentially evaluated and either:

- [#.#]{.pnum} its innermost enclosing non-block scope is [either]{.addu} a function parameter scope of an immediate function [or a consteval block scope]{.addu},
- [#.#]{.pnum} [...]

:::

After the example following the definition of _manifestly constant-evaluated_, introduce new terminology and rules for injecting declarations and renumber accordingly:

::: std
::: addu

[30]{.pnum} The evaluation of an expression can introduce one or more _injected declarations_. Each such declaration has an associated _synthesized point_ which follows the last non-synthesized program point in the translation unit containing that declaration. The evaluation is said to _produce_ the declaration.

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
:::

[#]{.pnum} The _evaluation context_ is a set of points within the program that determines the behavior of certain functions used for reflection ([meta.reflection]). During the evaluation of an expression `$C$` as a core constant expression, the evaluation context of an evaluation `$E$` comprises [the union of]{.addu}

- [#.#]{.pnum} the instantiation context of `$C$` ([module.context]{.sref})[, and]{.addu}
- [#.#]{.pnum} [the synthesized points corresponding to any injected declarations produced by evaluations sequenced before `$E$` ([intro.execution]{.sref})]{.addu}.

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


## Library

---
references:
  - id: P2996R10
    citation-label: P2996R10
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
        month: 2
        day: 8
    URL: https://wg21.link/p2996r10
---
