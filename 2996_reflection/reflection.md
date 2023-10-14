---
title: "Reflection for C++26"
document: P2996R0
date: today
audience: EWG
author:
    - name: Lots
    - name: Of
    - name: People
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

We start with a number of example that show off what is possible with the proposed set of features.
It is expected that these are mostly self-explanatory.
Read ahead to the next section for a more systematic description of each element of this proposal.

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
consteval info make_integer_seq_refl(T N) {
  std::vector args{^T};
  for (T k = 0; k<N; ++k)  args.push_back(std::meta::reflect_value(k));
  return substitute(^std::integer_sequence, args);
}

template<typename T>
  using make_integer_sequence<typename T, T N> = [:make_integer_seq_refl<T>(N):];
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


## Parsing Command-Line Options

Our next example shows how command-line options could be automatically mapped to an "options" structure.
For simplicity, we posit the existence of a range-like class type `ProgramArgs` that collects the traditional `(argc, argv)` parameters in a more friendly package.
This example also uses an expansion statement for simplicity.

::: bq
```c++
#include <sstream>
#include <string>
#include <meta>
#include <ProgramArgs.h>

template<typename Opts> bool parse_options(Opts *opts, ProgramArgs const &args) {
  using namespace std;
  using namespace std::meta;
  bool success = true;
  for (auto const arg = args.begin(); args != args.end(); ++args) {
    template for(constexpr auto dm: members_of(^Opts, is_nonstatic_data_member)) {
      if (arg->is_option_with_name(name_of(dm))) {
        // Arg is of the form "--word" where "word" is the name of dm.
        // Move to the option value, but remember the option tag:
        auto const opt_arg = arg++;
        if (arg == args.end()) {
          cerr << "Option " << string(opt_arg) << " is missing a value." << endl;
          success = false;
          break;
        }
        using T = typename[:type_of(dm):];
        if constexpr (requires (T d, istringstream is) { is >> d; }) {
          T val;
          istringstream is(string(*arg));
          is >> val;
          opts->[:dm:] = val;
        } else {
          cerr << "Option " << string(opt_arg) << " value \"" << string(arg)
               << "\" is no match for type << display_name_of(^T) << "." << endl;
          success = false;
        }
      }
    }
  }
  return success;
}

struct MyOpts {
   string file_name = "input.txt";  // Option "--file_name <string>"
  int    count = 1;                // Option "--count <int>"
} opts;

int main(int argc, char *argv[]) {
  ProgramArgs  cmd_line_args(argc, argv);
  if (!parse_options(&opts, cmd_line_args)) {
    return 1;
  }   ...
}

```

(This example is based on a presentation by Matúš Chochlík.)


## A Simple Tuple Type

:::bq
```c++
#include <meta>

template<typename... Ts> struct Tuple {
  std::meta::synth_struct<std::array<info, sizeof...(Ts)>{ nsdm_description(^Ts)... }> data;

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
  constexpr auto get(Tuple<Ts...> &t) noexcept -> std::tuple_element<I, Tuple<Ts...>> {
    return t.data.[:members_of(^decltype(t.data), is_nonstatic_data_member)[I]:];
  }

// Similarly for other value categories...
```
:::

This example uses a "magic" `std::meta::synth_struct` template along with member reflection (through the `members_of` metafunction to implement a `std::tuple`-like type without the usual complex and costly template metaprogramming tricks that that involves when these facilities are not available.



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

[ DV: I'm not sure that last rule is needed or even desirable.  I know at some point in the discussions it was brought up as desirable, but
      I don't think anything like it was implemented. ]


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


## `std::meta::info`

The type `std::meta::info` can be defined as follows:

```c++
namespace std {
  namespace meta {
    using info = decltype(^int);
  }
}
```

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
consteval auto invalid_reflection(
                  std::string_view message,
                  std::source_location src_loc = std::source_location::current())->info;
consteval auto is_invalid(info)->bool;
consteval auto is_invalid(std::span<info>)->bool;
consteval void diagnose_error(info);
```
:::

An invalid reflection represents a potential diagnostic for an erroneous construct.
Some standard metafunctions will generate such invalid reflections, but user programs can also create them with the `invalid_reflection` metafunction.
`is_invalid` returns true if it is given an invalid reflection or a span containing at least one invalid reflection.
Evaluating `diagnose_error` renders a program ill-formed.
If the given reflection is for an invalid reflection, an implementation is encouraged to render the encapsulated message and source position as part of the diagnostic indicating that the program is ill-formed.


### `name_of`, `display_name_of`, `source_location_of`

:::bq
```c++
consteval auto name_of(info r)->std::string_view;
consteval auto display_name_of(info r)->std::string_view;
}
```
:::

Given a reflection `r` that designates a declared entity X, `name_of(r)` returns a `string_view` holding the unqualified name of X.
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
consteval auto type_of(info r)->info;
consteval auto parent_of(info r)->info;
consteval auto entity_of(info r)->info;
}
```
:::

If `r` is a reflection designating a typed entity, `type_of(r)` is a reflection designating its type.
Otherwise, `type_of(r)` produces an invalid reflection.

If `r` designates a member of a class or namespace, `parent_of(r)` is a reflection designating its immediately enclosing class or namespace.
Otherwise, `parent_of(r)` produces an invalid reflection.

If `r` designates an alias, `entity_of(r)` designates the underlying entity.
Otherwise, `parent_of(r)` produces `r`.


### `members_of`, `enumerators_of`, `subobjects_of`

:::bq
```c++
template<typename ...Fs>
    consteval auto members_of(info class_type, Fs ...filters)->std::vector<info>;
template<typename ...Fs>
    consteval auto enumerators_of(info class_type, Fs ...filters)->std::vector<info>;
template<typename ...Fs>
    consteval auto subobjects_of(info class_type, Fs ...filters)->std::vector<info>;
```
:::




### `substitute`

:::bq
```c++
consteval auto substitute(info templ, std::span<info> args)->info;
```
:::

Given a reflection for a template and reflections for template arguments that match that template, `substitute` returns a reflection for the entity obtains by substituting the given arguments in the template.
This process might kick off instantiations outside the immediate context, which can lead to the program being ill-formed.
Substitution errors in the immediate context of the template result in an invalid reflection being returned.

Note that the template is only substituted, not instantiated.  For example:
:::bq
```c++
template<typename T> struct S { typename T::X x; };

constexpr auto r = substitute(^S, ^int);  // Okay.
typename[:r:] si;  // Error: T::X is invalid for T = int.
```
:::

### `entity_ref<T>`, `value_of<T>`, `ptr_to_member<T>`

:::bq
```c++
template<typename T> auto entity_ref<T>(info var_or_func)->T&;
template<typename T> auto value_of<T>(info constant_expr)->T;
template<typename T> auto pointer_to_member<T>(info member)->T;
```
:::

If `r` is a reflection `r` for a variable or function of type `T`, `entity_ref<T>(r)` evaluates to a reference to that variable or function.
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
auto test_type(info templ, info type)->bool {
  return value_of(substitute(templ, std::vector{type}));
}
auto test_types(info templ, std::vector<info> types)->bool {
  return value_of(substitute(templ, types));
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
consteval auto is_public(info r)->bool;
consteval auto is_protected(info r)->bool;
consteval auto is_private(info r)->bool;
consteval auto is_accessible(info r)->bool;
consteval auto is_virtual(info r)->bool;
consteval auto is_deleted(info entity)->bool;
consteval auto is_defaulted(info entity)->bool;
consteval auto is_explicit(info entity)->bool;
consteval auto is_override(info entity)->bool;
consteval auto is_pure_virtual(info entity)->bool;
consteval auto has_static_storage_duration(info r)->bool;

consteval auto is_nsdm(info entity)->bool;
consteval auto is_base(info entity)->bool;
consteval auto is_namespace(info entity)->bool;
consteval auto is_function(info entity)->bool;
consteval auto is_static(info entity)->bool;
consteval auto is_variable(info entity)->bool;
consteval auto is_type(info entity)->bool;
consteval auto is_alias(info entity)->bool;
consteval auto is_incomplete_type(info entity)->bool;
consteval auto is_template(info entity)->bool;
consteval auto is_function_template(info entity)->bool;
consteval auto is_variable_template(info entity)->bool;
consteval auto is_class_template(info entity)->bool;
consteval auto is_alias_template(info entity)->bool;
consteval auto has_template_arguments(info r)->bool;
```
:::



### `reflect_value`

:::bq
```c++
template<typename T> consteval auto reflect_value(T value)->info;
```
:::

This metafunction produces a reflection representing the constant value of the operand.


### `nsdm_description`, `synth_struct`, `synth_union`

:::bq
```c++
consteval auto nsdm_description(info  type, unsigned alignment = 0, unsigned width = 0)->info;

template<auto NDSMs> requires template_of(^NSDMs) == ^std:array
  struct synth_struct;

template<auto NDSMs> requires template_of(^NSDMs) == ^std:array
  struct synth_union;
```
:::

`nsdm_description` encapsulates the type of a nonstatic data member into a reflection.  Optional alignment and bit-field-width can be provided as well.
If the first operand does not designate a valid data member type, an invalid reflection is produced.

`std::meta::synth_struct<NSDMs>` where NSDMs is a `std::array<info, N>` containing entries generated by `nsdm_description` is a struct, which when completed contains just `N` data members corresponding to those descriptions (declared in the order of the array elements).
The names of the members are unspecified.
If `std::meta::synth_struct<NSDMs>` is completed and any of the description entries is an invalid reflection, the program is ill-formed.

`std::meta::synth_union<NSDMs>` is similar but produces a union instead of a struct.

### Data Layout Reflection
:::bq
```c++
consteval auto byte_offset_of(info entity)->std::size_t {...};
consteval auto bit_offset_of(info entity)->std::size_t {...};
consteval auto byte_size_of(info entity)->std::size_t {...};
consteval auto bit_size_of(info entity)->std::size_t {...};
```
:::

