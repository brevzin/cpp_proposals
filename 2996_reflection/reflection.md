---
title: "Reflection for C++26"
document: P2996R0
date: today
audience: EWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Andrew Sutton
      email: <andrew.n.sutton@gmail.com>
    - name: Faisal Vali
      email: <faisalv@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
---

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

We start with a number of examples that show off what is possible with the proposed set of features.
It is expected that these are mostly self-explanatory.
Read ahead to the next sections for a more systematic description of each element of this proposal.

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


## Selecting Members

Our second example enables selecting a member "by number" for a specific type.  It also shows the use of a metafunction dealing with diagnostics:

:::bq
```c++
struct S { unsigned i:2, j:6; };

consteval auto member_number(int n) {
  if (n == 0) return ^S::i;
  else if (n == 1) return ^S::j;
  else return std::meta::invalid_reflection("Only field numbers 0 and 1 permitted");
}

int main() {
  S s{0, 0};
  s.[:member_number(1):] = 42;  // Same as: s.j = 42;
  s.[:member_number(5):] = 0;   // Error (likely with "Only field numbers 0 and 1 permitted" in text).
}
```
:::

This example also illustrates that bit fields are not beyond the reach of this proposal.


## Fast Generation of Integer Sequence

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


## Enum to String

One of the most commonly requested facilities is to convert an enum value to a string (this example relies on expansion statements [@P1306R1]):

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

But we don't have to use expansion statements - we can also use algorithms. For instance, `enum_to_string` can also be implemented this way (this example relies on non-transient constexpr allocation [@P0784R7] [@P1974R0] [@P2670R1]):

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

## Parsing Command-Line Options

Our next example shows how a command-line option parser could work by automatically inferring flags based on member names. A real command-line parser would of course be more complex, this is just the beginning:

::: bq
```c++
template<typename Opts>
auto parse_options(std::span<std::string_view const> args) -> Opts {
  Opts opts;
  template for (constexpr auto dm : members_of(^Opts, std::meta::is_nonstatic_data_member)) {
    auto it = std::ranges::find_if(args,
      [](std::string_view arg){
        return args.starts_with("--") && args.substr(2) == name_of(dm);
      });

    if (it == args.end()) {
      // no option provided, use default
      continue;
    } else if (it + 1 == args.end()) {
      std::print(stderr, "Option {} is missing a value\n", *it);
      std::exit(EXIT_FAILURE);
    }

    using T = typename[:type_of(dm):];
    auto iss = ispanstream(it[1]);
    if (iss >> opts.[:dm:]; !iss) {
      std::print(stderr, "Failed to parse option {} into a {}\n", *it, display_name_of(^T));
      std::exit(EXIT_FAILURE);
    }
  }
  return opts;
}

struct MyOpts {
  string file_name = "input.txt";  // Option "--file_name <string>"
  int    count = 1;                // Option "--count <int>"
};

int main(int argc, char *argv[]) {
  MyOpts opts = parse_options<MyOpts>(std::vector<std::string_view>(argv+1, argv+argc));
  // ...
}
```
:::

(This example is based on a presentation by Matúš Chochlík.)


## A Simple Tuple Type

:::bq
```c++
#include <meta>

template<typename... Ts> struct Tuple {
  using storage = typename[:std::meta::synth_struct({nsdm_description(^T)...}):];
  storage data;

  Tuple(): data{} {}
  Tuple(Ts const& ...vs): data{ vs... } {}
};

template<typename... Ts>
  struct std::tuple_size<Tuple<Ts...>>: public integral_constant<size_t, sizeof...(Ts)> {};

template<typename I, typename... Ts>
  struct std::tuple_element<I, Tuple<Ts...>> {
    using type = [: template_arguments_of(^Tuple<Ts...>)[I] :];
  };

template<typename I, typename... Ts>
  constexpr auto get(Tuple<Ts...> &t) noexcept -> std::tuple_element_t<I, Tuple<Ts...>>& {
    return t.data.[:members_of(^decltype(t.data), is_nonstatic_data_member)[I]:];
  }

// Similarly for other value categories...
```
:::

This example uses a "magic" `std::meta::synth_struct` template along with member reflection through the `members_of` metafunction to implement a `std::tuple`-like type without the usual complex and costly template metaprogramming tricks that that involves when these facilities are not available.

## Struct to Struct of Arrays

::: bq
```c++
consteval auto make_struct_of_arrays(std::meta::info type, size_t n) -> std::meta::info {
  std::vector<info> new_members;
  for (std::meta::info member : members_of(type, std::meta::is_nonstatic_data_member)) {
    auto array_type = substitute(^std::array, {type_of(member), reflect_value(n)});
    new_members.push_back(nsdm_description(array_type, {.name = name_of(member)}));
  }
  return std::meta::synth_struct(new_members);
}

template <typename T, size_t N>
using struct_of_arrays = [: make_struct_of_arrays(^T, N) :];
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

## A Universal Print Function

This example is taken from Boost.Describe, translated to using `std::format` instead of iostreams:

::: bq
```cpp
struct universal_formatter {
  constexpr auto parse(auto& ctx) { return ctx.begin(); }

  template <typename T>
  auto format(T const& t, auto& ctx) const {
    auto out = ctx.out();
    *out++ = '{';

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

    template for (constexpr auto mem : members_of(^T, std::meta::is_nonstatic_data_member)) {
      delim();
      out = std::format_to(out, ".{}={}", name_of(mem), t.[:mem:]);
    }

    *out++ = '}';
    return out;
  }
};

struct X { int m1 = 1; };
struct Y { int m2 = 2; };
class Z : public X, private Y { int m1 = 3; int m2 = 4; };

template <> struct std::formatter<X> : universal_formatter { };
template <> struct std::formatter<Y> : universal_formatter { };
template <> struct std::formatter<Z> : universal_formatter { };

int main() {
    std::println("{}", Z()); // {@{@.m1 = 1}, {.m2 = 2}, .m1 = 3, .m2 = 4}
}
```
:::

## Implementing hash_append

Based on the [@N3980] API:

::: bq
```cpp
template <typename H, typename T> requires std::is_standard_layout_v<T>
void hash_append(H& algo, T const& t) {
    template for (constexpr auto mem : members_of(^T, std::meta::is_nonstatic_data_member)) {
        hash_append(algo, t.[:mem:]);
    }
}
```
:::

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

In a SFINAE context, a failure to substitute the operand of a reflection operator construct causes that construct to evaluate to an invalid reflection.

## Splicers (`[:`...`:]`)

A reflection that is not an invalid reflection can be "spliced" into source code using one of several _splicer_ forms:

 - `[: r :]` produces an _expression_ evaluating to the entity or constant value represented by `r`.
 - `typename[: r :]` produces a _simple-type-specifier_ corresponding to the type represented by `r`.
 - `template[: r :]` produces a _template-name_ corresponding to the template represented by `r`.
 - `namespace[: r :]` produces a _namespace-name_ corresponding to the namespace represented by `r`.
 - `[:r:]::` produces a _nested-name-specifier_ corresponding to the namespace, enumeration type, or class type represented by `r`.

Attempting to splice a reflection value that does not meet the requirement of the splice (including "invalid reflections") is ill-formed.
For example:

:::bq
```c++
typename[: ^:: :] x = 0;  // Error.
```
:::

A quality implementation should emit the diagnostic text associated with an invalid reflection when attempting to splice that invalid reflection.

(This proposal does not at this time propose range-based splicers as described in P1240.
We still believe that those are desirable.
However, they are more complex to implement and they involve syntactic choices that benefit from being considered along with other proposals that introduce pack-like constructs in non-template contexts.
Meanwhile, we found that many very useful techniques are enabled with just the basic splicers presented here.)


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

  - an error (corresponding to an "invalid reflection")
  - any (C++) type and type-alias
  - any function or member function
  - any variable, static data member, or structured binding
  - any non-static data member
  - any constant value
  - any template
  - any namespace

Notably absent at this time are general non-constant expressions (that aren't *expression-id*s referring to variables or structured bindings).  For example:

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

### `invalid_reflection`, `is_invalid`, `diagnose_error`

:::bq
```c++
namespace std::meta {
  consteval auto invalid_reflection(string_view msg, source_location loc = source_location::current()) -> info;
  consteval auto is_invalid(info) -> bool;
  consteval auto diagnose_error(info) -> void;
}
```
:::

An invalid reflection represents a potential diagnostic for an erroneous construct.
Some standard metafunctions will generate such invalid reflections, but user programs can also create them with the `invalid_reflection` metafunction.
`is_invalid` returns true if it is given an invalid reflection.
Evaluating `diagnose_error` renders a program ill-formed.
If the given reflection is for an invalid reflection, an implementation is encouraged to render the encapsulated message and source position as part of the diagnostic indicating that the program is ill-formed.


### `name_of`, `display_name_of`, `source_location_of`

:::bq
```c++
namespace std::meta {
  consteval auto name_of(info r) -> string_view;
  consteval auto display_name_of(info r) -> string_view;
}
```
:::

Given a reflection `r` that designates a declared entity `X`, `name_of(r)` returns a `string_view` holding the unqualified name of `X`.
For all other reflections, an empty `string_view` is produced.
For template instances, the name does not include the template argument list.
The contents of the `string_view` consist of characters of the basic source character set only (an implementation can map other characters using universal character names).

Given a reflection `r`, `display_name_of(r)` returns a unspecified non-empty `string_view`.
Implementations are encouraged to produce text that is helpful in identifying the reflected construct.

Given a reflection `r`, `source_location_of(r)` returns an unspecified `source_location`.
Implementations are encouraged to produce the correct source location of the item designated by the reflection.

### `type_of`, `parent_of`, `entity_of`

:::bq
```c++
namespace std::meta {
  consteval auto type_of(info r) -> info;
  consteval auto parent_of(info r) -> info;
  consteval auto entity_of(info r) -> info;
}
```
:::

If `r` is a reflection designating a typed entity, `type_of(r)` is a reflection designating its type.
Otherwise, `type_of(r)` produces an invalid reflection.

If `r` designates a member of a class or namespace, `parent_of(r)` is a reflection designating its immediately enclosing class or namespace.
Otherwise, `parent_of(r)` produces an invalid reflection.

If `r` designates an alias, `entity_of(r)` designates the underlying entity.
Otherwise, `entity_of(r)` produces `r`.


### `members_of`, `enumerators_of`, `subobjects_of`

:::bq
```c++
namespace std::meta {
  template<typename ...Fs>
    consteval auto members_of(info class_type, Fs ...filters) -> vector<info>;
  template<typename ...Fs>
    consteval auto enumerators_of(info class_type, Fs ...filters) -> vector<info>;
  template<typename ...Fs>
    consteval auto subobjects_of(info class_type, Fs ...filters) -> vector<info>;
}
```
:::


### `substitute`

:::bq
```c++
namespace std::meta {
  consteval auto substitute(info templ, span<info const> args) -> info;
}
```
:::

Given a reflection for a template and reflections for template arguments that match that template, `substitute` returns a reflection for the entity obtains by substituting the given arguments in the template.

For example:

::: bq
```c++
constexpr auto r = substitute(^std::vector, std::vector{^int});
using T = [:r:]; // Ok, T is std::vector<int>
```
:::

This process might kick off instantiations outside the immediate context, which can lead to the program being ill-formed.
Substitution errors in the immediate context of the template result in an invalid reflection being returned.

Note that the template is only substituted, not instantiated.  For example:

:::bq
```c++
template<typename T> struct S { typename T::X x; };

constexpr auto r = substitute(^S, std::vector{^int});  // Okay.
typename[:r:] si;  // Error: T::X is invalid for T = int.
```
:::

### `entity_ref<T>`, `value_of<T>`, `ptr_to_member<T>`

:::bq
```c++
namespace std::meta {
  template<typename T> auto entity_ref<T>(info var_or_func) -> T&;
  template<typename T> auto value_of<T>(info constant_expr) -> T;
  template<typename T> auto pointer_to_member<T>(info member) -> T;
}
```
:::

If `r` is a reflection of a variable or function of type `T`, `entity_ref<T>(r)` evaluates to a reference to that variable or function.
Otherwise, `entity_ref<T>(r)` is ill-formed.

If `r` is a reflection for a constant-expression or a constant-valued entity of type `T`, `value_of(r)` evaluates to that constant value.
Otherwise, `value_of<T>(r)` is ill-formed.

If `r` is a reflection for a non-static member or for a constant pointer-to-member value matching type `T`, `pointer_to_member<T>(r)` evaluates to a corresonding pointer-to-member value.
Otherwise, `value_of<T>(r)` is ill-formed.

These function may feel similar to splicers, but unlike splicers they do not require their operand to be a constant-expression itself.
Also unlike splicers, they require knowledge of the type associated with the entity reflected by their operand.

### `test_type<Pred>`

:::bq
```c++
namespace std::meta {
  auto test_type(info templ, info type) -> bool {
    return test_types(templ, vector{type});
  }

  auto test_types(info templ, span<info const> types) -> bool {
    return value_of<bool>(substitute(templ, types));
  }
}
```
:::

This utility translates existing metaprogramming predicates (expressed as constexpr variable templates) to the reflection domain.
For example:

:::bq
```c++
struct S {};
static_assert(test_type(^std::is_class_v, ^S));
```
:::

An implementation is permitted to recognize standard predicate templates and implement `test_type` without actually instantiating the predicate template.
In fact, that is recommended practice.

### Other Singular Reflection Predicates

:::bq
```c++
namespace std::meta {
  consteval auto is_public(info r) -> bool;
  consteval auto is_protected(info r) -> bool;
  consteval auto is_private(info r) -> bool;
  consteval auto is_accessible(info r) -> bool;
  consteval auto is_virtual(info r) -> bool;
  consteval auto is_deleted(info entity) -> bool;
  consteval auto is_defaulted(info entity) -> bool;
  consteval auto is_explicit(info entity) -> bool;
  consteval auto is_override(info entity) -> bool;
  consteval auto is_pure_virtual(info entity) -> bool;
  consteval auto has_static_storage_duration(info r) -> bool;

  consteval auto is_nsdm(info entity) -> bool;
  consteval auto is_base(info entity) -> bool;
  consteval auto is_namespace(info entity) -> bool;
  consteval auto is_function(info entity) -> bool;
  consteval auto is_static(info entity) -> bool;
  consteval auto is_variable(info entity) -> bool;
  consteval auto is_type(info entity) -> bool;
  consteval auto is_alias(info entity) -> bool;
  consteval auto is_incomplete_type(info entity) -> bool;
  consteval auto is_template(info entity) -> bool;
  consteval auto is_function_template(info entity) -> bool;
  consteval auto is_variable_template(info entity) -> bool;
  consteval auto is_class_template(info entity) -> bool;
  consteval auto is_alias_template(info entity) -> bool;
  consteval auto has_template_arguments(info r) -> bool;
}
```
:::


### `reflect_value`

:::bq
```c++
namespace std::meta {
  template<typename T> consteval auto reflect_value(T value) -> info;
}
```
:::

This metafunction produces a reflection representing the constant value of the operand.


### `nsdm_description`, `synth_struct`, `synth_union`

:::bq
```c++
namespace std::meta {
  struct nsdm_field_args {
    optional<string_view> name;
    optional<int> alignment;
    optional<int> width;
  };

  consteval auto nsdm_description(info type, nsdm_field_args args = {}) -> info;

  consteval auto synth_struct(span<info const>) -> info;
  consteval auto synth_union(span<info const>) -> info;
}
```
:::

`nsdm_description` creates a reflection that describes a non-static data member of given type. Optional alignment, bit-field-width, and name can be provided as well.
If `type` does not designated a valid data member type, an invalid reflection is produced.
If no `name` is provided, the name of the non-static data member is unspecified.
Note that the reflection obtained from `nsdm_description` is _not_ the reflection of a non-static data member itself; it only encapsulates the information needed to synthesize such a data member.
In particular, metafunctions like `name_of`, `type_of`, and `parent_of` are not applicable to the result of an `nsdm_description`.

`synth_struct` and `synth_union` take a range of NSDM descriptions and return a reflection that denotes a struct and union, respectively, comprised of corresponding non-static data members.

For example:

::: bq
```c++
constexpr auto T = std::meta::synth_struct({
  nsdm_description(^int),
  nsdm_description(^char),
  nsdm_description(^double),
});

// T is a reflection of the type
// struct {
//   int $_0$;
//   char $_1$;
//   double $_2$;
// }

constexpr auto U = std::meta::synth_struct({
  nsdm_description(^int, {.name="i", .align=64}),
  nsdm_description(^int, {.name="j", .align=64}),
});

// U is a reflection of the type
// struct {
//   alignas(64) int i;
//   alignas(64) int j;
// }
```
:::

### Data Layout Reflection
:::bq
```c++
namespace std::meta {
  consteval auto byte_offset_of(info entity) -> std::size_t;
  consteval auto bit_offset_of(info entity) -> std::size_t;
  consteval auto byte_size_of(info entity) -> std::size_t;
  consteval auto bit_size_of(info entity) -> std::size_t;
}
```
:::

