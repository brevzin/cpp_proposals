---
title: Annotations for Reflection
tag: reflection
document: P3394R4
date: today
audience: CWG, LWG
hackmd: true
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
---

# Revision History

Since [@P3394R3]: wording. Removed ability to put attributes and annotations into the same `$attribute-specifier$`.

Since [@P3394R2]: wording, including rebasing off of [@P2996R13]. Also removed `annotate` to streamline the process, it can always be added later.

Since [@P3394R1]:

- loosened the P2996 scope restrictions around injected declarations: allow the definition of an entity to produce injected declaration of itself
- reduced library API down to just `is_annotation`, `annotations_of`, `annotations_of_with_type` (a binary function that returns a `vector<info>`), and `annotate`.

Since [@P3394R0]:

- added wording

# Introduction

Ever since writing [@P1240R0]{.title}, but more so since [@P2996R0]{.title}, we have been requested to add a capability to annotate declarations in a way that reflection can observe. For example, Jeremy Ong presented compelling arguments in a post to the [SG7 reflector](https://lists.isocpp.org/sg7/2023/10/0450.php). Corentin Jabot also noticed the need while P1240 was evolving and wrote [@P1887R0]{.title}, which proposes syntax not entirely unlike what we present here.


In early versions of P2996 (and P1240 before that), a workaround was to encode properties in the template arguments of alias template specializations:

```cpp
template <typename T, auto... Annotations>
using Noted = T;

struct C {
    Noted<int, 1> a;
    Noted<int*, some, thing> b;
};
```

It was expected that something like `type_of(^^C::a)` would produce a reflection of `Noted<int, 1>` and that can be taken apart with metafunctions like `template_arguments_of` — which both preserves the type as desired (`a` is still an `int`) and allows reflection queries to get at the desired annotations (the `1`, `some`, and `thing` in this case).

There are problems with this approach, unfortunately:

* It doesn't work for all contexts where we might want to annotate declarations (e.g., enumerators).
* It doesn't directly express the intent.
* It turns out that providing access to aliases used in the declaration of reflected entities raises difficult questions and P2996 is therefore likely going to drop the ability to access that information (with the intention of resurrecting the capability with a later paper once specification and implementation challenges have been addressed).

In this paper, we propose simple mechanisms that more directly support the ability to annotate C++ constructs.

# Motivating Examples

We'll start with a few motivating examples for the feature. We'll describe the details of the feature in the subsequent section.

These examples are inspired from libraries in other programming languages that provide some mechanism to annotate declarations.

## Command-Line Argument Parsing

Rust's [clap](https://docs.rs/clap/latest/clap/) library provides a way to add annotations to declarations to help drive how the parser is declared. We can now [do the same](https://godbolt.org/z/YTWPfnn4n):

```cpp
struct Args {
    [[=clap::Help("Name of the person to greet")]]
    [[=clap::Short, =clap::Long]]
    std::string name;

    [[=clap::Help("Number of times to greet")]]
    [[=clap::Short, =clap::Long]]
    int count = 1;
};


int main(int argc, char** argv) {
    Args args = clap::parse<Args>(argc, argv);

    for (int i = 0; i < args.count; ++i) {
        std::cout << "Hello " << args.name << '\n';
    }
}
```

Here, we provide three types (`Short`, `Long`, and `Help`) which help define how these member variables are intended to be used on the command-line. This is implemented on top of [Lyra](https://github.com/bfgroup/Lyra).

When run:

```
$ demo -h
USAGE:
  demo [-?|-h|--help] [-n|--name <name>] [-c|--count <count>]

Display usage information.

OPTIONS, ARGUMENTS:
  -?, -h, --help
  -n, --name <name>       Name of the person to greet
  -c, --count <count>     Number of times to greet

$ demo -n wg21 --count 3
Hello wg21
Hello wg21
Hello wg21
```

While `Short` and `Long` can take explicit values, by default they use the first letter and whole name of the member that they annotate.

The core of the implementation is that `parse<Args>` loops over all the non-static data members of `Args`, then finds all the `clap`-related annotations and invokes them:

```cpp
template <typename Args>
auto parse(int argc, char** argv) -> Args {
    Args args;
    auto cli = lyra::cli();

    // ...

    template for (constexpr info M : nonstatic_data_members_of(^^Args)) {
        auto id = std::string id(identifier_of(mem));
        auto opt = lyra::opt(args.[:M:], id);

        template for (constexpr info A : annotations_of(M)) {
            if constexpr (parent_of(type_of(A)) == ^^clap) {
                // for those annotions that are in the clap namespace
                // invoke them on our option
                extract<[:type_of(A):]>(A).apply_annotation(opt, id);
            }
        }

        cli.add_argument(opt);
    }

    // ...
};
```

So, for instance, `Short` would be implemented like this:

```cpp
namespace clap {
    struct ShortArg {
        // optional isn't structural yet but let's pretend
        optional<char> value;

        constexpr auto operator()(char c) const -> ShortArg {
            return {.value=c};
        };

        auto apply_annotation(lyra::opt& opt, std::string_view id) const -> void {
            char first = value.value_or(id[0]);
            opt[std::string("-") + first];
        }
    };

    inline constexpr auto Short = ShortArg();
}
```

Overall, a fairly concise implementation for an extremely user-friendly approach to command-line argument parsing.

## Test Parametrization

The pytest framework comes with a decorator to [parametrize](https://docs.pytest.org/en/7.1.x/how-to/parametrize.html) test functions. We can now do [the same](https://godbolt.org/z/7aK54f1sd):

```cpp
namespace N {
    [[=parametrize({
        Tuple{1, 1, 2},
        Tuple{1, 2, 3}
        })]]
    void test_sum(int x, int y, int z) {
        std::println("Called test_sum(x={}, y={}, z={})", x, y, z);
    }

    struct Fixture {
        Fixture() {
            std::println("setup fixture");
        }

        ~Fixture() {
            std::println("teardown fixture");
        }

        [[=parametrize({Tuple{1}, Tuple{2}})]]
        void test_one(int x) {
            std::println("test one({})", x);
        }

        void test_two() {
            std::println("test two");
        }
    };
}

int main() {
    invoke_all<^^N>();
}
```

When run, this prints:

```
Called test_sum(x=1, y=1, z=2)
Called test_sum(x=1, y=2, z=3)
setup fixture
test one(1)
teardown fixture
setup fixture
test one(2)
teardown fixture
setup fixture
test two
teardown fixture
```

Here, `parametrize` returns a value that is some specialization of `Parametrize` (which is basically an array of tuples, except that `std::tuple` isn't structural so the implementation rolls its own).

The rest of the implementation looks for all the free functions named `test_*` or nonstatic member functions of class types that start with `test_*` and invokes them once, or with each parameter, depending on the presence of the annotation. That looks like this:

```cpp
consteval auto parametrization_of(std::meta::info M) -> std::meta::info {
    for (auto a : annotations_of(M)) {
        auto t = type_of(a);
        if (has_template_arguments(t) and template_of(t) == ^^Parametrize) {
            return a;
        }
    }
    return std::meta::info();
}

template <std::meta::info M, class F>
void invoke_single_test(F f) {
    constexpr auto A = parametrization_of(M);

    if constexpr (A != std::meta::info()) {
        // if we are parametrized, pull out that value
        // and for each tuple, invoke the function
        // this is basically calling std::apply on an array of tuples
        constexpr auto Params = extract<[:type_of(A):]>(A);
        for (auto P : Params) {
            P.apply(f);
        }
    } else {
        f();
    }
}

template <std::meta::info Namespace>
void invoke_all() {
    template for (constexpr std::meta::info M : members_of(Namespace)) {
        if constexpr (is_function(M) and identifier_of(M).starts_with("test_")) {
            invoke_single_test<M>([:M:]);
        } else if constexpr (is_type(M)) {
            template for (constexpr std::meta::info F : nonstatic_member_functions_of(M)) {
                if constexpr (identifier_of(F).starts_with("test_")) {
                    invoke_single_test<F>([&](auto... args){
                        typename [:M:] fixture;
                        fixture.[:F:](args...);
                    });
                }
            }
        }
    }
}
```

## Serialization

Rust's [serde](https://serde.rs/) library is a framework for serialization and deserialization. It is easy enough with reflection to do member-wise serialization. But how do you opt into that? An annotation provides a cheap mechanism of [doing just that](https://godbolt.org/z/oT9cYz9sj) (built on top of [Boost.Json](https://www.boost.org/doc/libs/1_85_0/libs/json/doc/html/index.html)):

```cpp
struct [[=serde::derive]] Point {
    int x, y;
};
```

Allowing:

```cpp
// prints {"x":1,"y":2}
std::cout << boost::json::value_from(Point{.x=1, .y=2});
```

But opting in is just the first thing you might want to do with serialization. You might also, for instance, want to change how fields are serialized. `serde` provides a lot of attributes to do so. The easiest to look at is `rename`, which uses the provided string instead of the name of the non-static data member:

```cpp
struct [[=serde::derive]] Person {
    [[=serde::rename("first name")]] std::string first;
    [[=serde::rename("last name")]] std::string last;
};
```

Which leads to:

```cpp
// prints {"first name":"Peter","last name":"Dimov"}
std::cout << boost::json::value_from(Person{.first="Peter", .last="Dimov"});
```

The implementation for these pieces is fairly straightforward. We provide an opt-in for the value conversion function when the `serde::derive` annotation is present. In that case, we walk all the non-static data members and write them into the `boost::json::value` output. If a `serde::Rename` annotation is present, we use that instead of the data member's name:

```cpp
namespace serde {
    inline constexpr struct{} derive{};
    struct rename { char const* field; };
}

namespace boost::json {
    template <class T>
        requires (has_annotation(^^T, serde::derive))
    void tag_invoke(value_from_tag const&, value& v, T const& t) {
        auto& obj = v.emplace_object();
        template for (constexpr auto M : nonstatic_data_members_of(^^T)) {
            constexpr auto field = []{
                std::optional<std::meta::info> res;
                for (std::meta::info r : annotations_of_with_type(M, ^^serde::rename)) {
                    if (res and *res != r) {
                        throw "oops";
                    }
                    res = r;
                }

                return res
                    .transform([](std::meta::info r){
                        return std::string_view(extract<serde::rename>(r).field);
                    })
                    .value_or(identifier_of(M));
            }();

            obj[field] = boost::json::value_from(t.[:M:]);
        }
    }
}
```

You can imagine extending this out to support a wide variety of other serialization-specific attributes that shouldn't otherwise affect the C++ usage of the type. For instance, a [more complex approach](https://godbolt.org/z/oT9cYz9sj) additionally supports the `skip_serializing_if` annotation while first collecting all `serde` annotations into a struct.

# Proposal

The core idea is that an *annotation* is a compile-time value that can be associated with a construct to which attributes can appertain. Annotation and attributes are somewhat related ideas, and we therefore propose a syntax for the former that builds on the existing syntax for the latter.

At its simplest:

```cpp
struct C {
    [[=1]] int a;
};
```

Syntactically, an annotation is an attribute of the form `= expr` where `expr` is a _`constant-expression`_ (which syntactically excludes, e.g., _`comma-expression`_) to which the glvalue-to-prvalue conversion has been applied if the expression wasn't a prvalue to start with.

Currently, we require that an annotation has structural type because we're going to return annotations through `std::meta::info`, and currently all reflection values must be structural.

## Why not Attributes?

Attributes are very close in spirit to annotations.  So it made sense to piggy-back on the attribute syntax to add annotations. Existing attributes are designed with fairly open grammar and they can be ignored by implementations, which makes it difficult to connect them to user code. Given a declarations like:

```cpp
[[nodiscard, gnu::always_inline]]
[[deprecated("don't use me")]]
void f();
```

What could reflecting on `f` return? Because attributes are ignorable, an implementation might simply ignore them. Additionally, there is no particular value associated with any of these attributes that would be sensible to return. We're limited to returning either a sequence of strings. Or, with [P3294](https://wg21.link/p3294), token sequences.

But it turns out to be quite helpful to preserve the actual values without requiring libraries to do additional parsing work. Thus, we need to distinguish annotations (whose values we need to preserve and return back to the user) from attributes (whose values we do not). Thus, we looked for a sigil introducing a general expression.

Originally, the plus sign (`+`) was considered (as in P1887), but it is not ideal because a prefix `+` has a meaning for some expressions and not for others, and that would not carry over to the attribute notation.  A prefix `=` was found to be reasonably meaningful in the sense that the annotation "equals" the value on the right, while also being syntactically unambiguous. We also discussed using the reflection operator (`^^`) as an introducer (which is attractive because the annotation ultimately comes back to the programmer as a reflection value), but that raised questions about an annotation itself being a reflection value (which is not entirely improbable).

As such, this paper proposes annotations as distinct from attributes, introduced with a prefix `=`.

## Library Queries

We propose the following set of library functions to work with annotations:

```cpp
namespace std::meta {
  consteval bool is_annotation(info);

  consteval vector<info> annotations_of(info item);                      // (1)
  consteval vector<info> annotations_of_with_type(info item, info type); // (2)
}
```

`is_annotation` checks whether a particular reflection represents an annotation.

We provide two functions to retrieve all the annotations of a particular item:

1. `annotations_of(item)` returns all the annotations on `item`.
2. `annotations_of_item_with_type(item, type)` returns all the annotations `a` on `item` such that `type_of(a) == type`.

In earlier revisions, these were overloads (both spelled `annotations_of`), but starting in R2 the second function was renamed to add clarity on what the second parameter actually means. There were previously additional functions proposed for ergonomic reasons, but those were removed during the [Hagenberg meeting](https://wiki.edg.com/bin/view/Wg21hagenberg2025/P3394).

## Additional Syntactic Constraints

Annotations can be repeated:

```cpp
[[=42, =42]] int x;
static_assert(annotations_of(^^x).size() == 2);
```

Annotations spread over multiple declarations of the same entity accumulate:

```cpp
[[=42]] int f();
[[=24]] int f();
static_assert(annotations_of(^^f).size() == 2);
```

Annotations follow appertainance rules like attributes, but shall not appear in the *attribute-specifier-seq* of a *type-specifier-seq* or an *empty-declaration*:

```cpp
struct [[=0]] S {};  // Okay: Appertains to S.
[[=42]] int f();     // Okay: Appertains to f.
int f[[=0]] ();      // Ditto.
int [[=24]] f();     // Error: Cannot appertain to int.
[[=123]];            // Error: No applicable construct.
```

Earlier revisions of the paper allowed annotations and attributes to appear within the same `$attribute-specifier$`:

::: std
```cpp
[[nodiscard, =42]] int f();
```
:::

However, in Sofia, it was pointed out that for parsing purposes, allowing intermixing of annotations and attributes is challenging because it makes it harder to determine if `= T < x, y` means that `y` is the a new attribute or part of a template argument list. Since there was no motivation to allow intermixing them and clear benefits to the separation, we're mandating that a given `$attribute-specifier$` is either all attributes or all annotations. This also simplifies the grammar and the wording. The above, if desired, can be written as:

::: std
```cpp
[[nodiscard]] [[=42]] int f();
```
:::

## Implementation Experience

The core language feature and the basic query functions have been implemented in the EDG front end and in Bloomberg's P2996 Clang fork (with option `-freflection-latest`), both available on Compiler Explorer.

## Other Directions We Are Exploring

As evidenced in the motivating examples earlier, there is a lot of value in this proposal even in this simple form. However, there is more to consider when it comes to annotations.

This proposal right now lets us unconditionally add an annotation to a type:

```cpp
struct [[=X]] Always;
```

But it does not let us conditionally add an annotation to a type:

```cpp
template <class T>
struct /* X only for some T */ Sometimes;
```

Or to really generalize annotations. For instance, in the clap example earlier, our example showed usage with `clap::Short` and `clap::Long`. What if somebody wants to compose these into their own annotation that attaches both `clap::Short` and `clap::Long` to a declaration?

More broadly, there is clear value in having an annotation be able to be invoked by the declaration itself. Doing so allows the two uses above easily enough. An interesting question, though, is whether this callback (syntax to be determined) is invoked at the _beginning_ of the declaration or at the _end_ of the declaration. For annotations on classes, this would be before the class is complete or after the class is complete. Before completeness allows the class to observe the annotation during instantiation. After completeness allows the annotation callback to observe properties of the type. In some sense, Herb Sutter's [@P0707R4]{.title} was adding annotations on classes, invoked on class completeness, that allow mutation of the class.

One concrete, simpler example. We can, with this proposal as-is, create a `Debug` annotation that a user can add to their type and a specialization of `std::formatter` for all types that have a `Debug` annotation [as follows](https://godbolt.org/z/bcYE7nY4s):

```cpp
template <auto V> struct Derive { };
template <auto V> inline constexpr Derive<V> derive;

inline constexpr struct{} Debug;

template <class T> requires (has_annotation(^^T, derive<Debug>))
struct std::formatter<T> {
    // ...
};

struct [[=derive<Debug>]] Point {
    int x;
    int y;
};

int main() {
    auto p = Point{.x=1, .y=2};
    // prints p=Point{.x=1, .y=2}
    std::println("p={}", p);
}
```

This *works*, but it's not really the ideal way of doing it. This could still run into potential issues with ambiguous specialization of `std::formatter`. Better would be to allow the `Debug` annotation to, at the point of completion of `Point`, inject an explicit specialization of `std::formatter`. This would rely both on the ability for the annotation to be called back and language support for such injection (see [@P3294R2]{.title}).

There are still open questions as to how to handle such callbacks. Does an annotation that gets called back merit different syntax from an annotation that doesn't? Can it mutate the entity that it is attached to? How do we name the potential callbacks? Should the callback be registered implicitly (e.g., if C of type `X` with member `X::annotate_declaration(...)` appears, that member is automatically a callback invoked when an entity is first declared with an annotation of type `X`) or explicitly (e.g., calling `annotated_declaration_callback(^^X, X_handler)` would cause `X_handler(...)` to be invoked when an entity is first declared with an annotation of type `X`).

# Wording

The wording is relative to [@P2996R13].

## Language

Change [basic.fundamental]{.sref} to add "annotation" to the list of reflection kinds:

::: std
[x]{.pnum} A value of type `std::meta::info` is called a _reflection_. There exists a unique _null reflection_; every other reflection is a representation of

* [x.#]{.pnum} a value of scalar type ([temp.param]),
* [x.#]{.pnum} an object with static storage duration ([basic.stc]),
* [x.#]{.pnum} a variable ([basic.pre]),
* [x.#]{.pnum} a structured binding ([dcl.struct.bind]),
* [x.#]{.pnum} a function ([dcl.fct]),
* [x.#]{.pnum} an enumerator ([dcl.enum]),

::: addu
* [x.#]{.pnum} an annotation ([dcl.attr.grammar]),
:::

* [x.#]{.pnum} a type alias ([dcl.typedef]),
* [x.#]{.pnum} a type ([basic.types]),
* [x.#]{.pnum} a class member ([class.mem]),
* [x.#]{.pnum} an unnamed bit-field ([class.bit]),
* [x.#]{.pnum} a primary class template ([temp.pre]),
* [x.#]{.pnum} a function template ([temp.pre]),
* [x.#]{.pnum} a primary variable template ([temp.pre]),
* [x.#]{.pnum} an alias template ([temp.alias]),
* [x.#]{.pnum} a concept ([temp.concept]),
* [x.#]{.pnum} a namespace alias ([namespace.alias]),
* [x.#]{.pnum} a namespace ([basic.namespace.general]),
* [x.#]{.pnum} a direct base class relationship ([class.derived.general]), or
* [x.#]{.pnum} a data member description ([class.mem.general]).
:::

Update annotation equality in [expr.eq]{.sref}/5+:

::: std
[5+]{.pnum} If both operands are of type `std::meta::info`, they compare equal if both operands

* [5+.#]{.pnum} are null reflection values,
* [5+.#]{.pnum} represent values that are template-argument-equivalent ([temp.type]),
* [5+.#]{.pnum} represent the same object,
* [5+.#]{.pnum} represent the same entity,

::: addu
* [5+.#]{.pnum} represent the same annotation ([dcl.attr.annotation]),
:::

* [5+.#]{.pnum} represent the same direct base class relationship, or
* [5+.#]{.pnum} represent equal data member descriptions ([class.mem.general]),

and they compare unequal otherwise.
:::

Extend the grammar in [dcl.attr.grammar]{.sref}:

::: std
[1]{.pnum} Attributes [and annotations]{.addu} specify additional information for various source constructs such as types, variables, names, blocks, or translation units.

```diff
  $attribute-specifier-seq$:
    $attribute-specifier-seq$@~opt~@ $attribute-specifier$

  $attribute-specifier$:
    [ [ $attribute-using-prefix$@~opt~@ $attribute-list$ ] ]
+   [ [ $annotation-list$ ] ]
    $alignment-specifier$

  $alignment-specifier$:
    alignas ( $type-id$ ...@~opt~@ )
    alignas ( $constant-expression$ ...@~opt~@ )

  $attribute-using-prefix$:
    using $attribute-namespace$ :

  $attribute-list$:
    $attribute$@~opt~@
    $attribute-list$ , $attribute$@~opt~@
    $attribute$ ...
    $attribute-list$ , $attribute$ ...

+ $annotation-list$:
+   $annotation$ ...@~opt~@
+   $annotation-list$ , $annotation$ ...@~opt~@

  $attribute$:
    $attribute-token$ $attribute-argument-clause$@~opt~@

+ $annotation$:
+   = $constant-expression$
```
:::

Clarify the restriction on no attributes in [dcl.attr.grammar]{.sref}/4:

::: std
[4]{.pnum} In an `$attribute-list$`, an ellipsis may appear only following an `$attribute$` if that `$attribute$`'s specification permits it. An `$attribute$` followed by an ellipsis is a pack expansion. An `$attribute-specifier$` that contains [an `$attribute-list$` with]{.addu} no `$attribute$`s has no effect.
The order in which the `$attribute-tokens$` appear in an `$attribute-list$` is not significant. [...]

::: addu
[*]{.pnum} [An `$annotation$` followed by an ellipsis is a pack expansion ([temp.variadic]).]{.addu}
:::
:::

Add a new subclause under [dcl.attr]{.sref} called [dcl.attr.annotation] "Annotations":

::: std
::: addu
[1]{.pnum} An annotation may be applied to any declaration of a type, type alias, variable, function, namespace, enumerator, `$base-specifier$`, or non-static data member.

[#]{.pnum} Let `$E$` be the expression `std::meta::reflect_constant($constant-expression$)`. `$E$` shall be a constant expression; the result of `$E$` is the _underlying constant_ of the annotation.

[#]{.pnum} Each `$annotation$` produces a unique annotation.

[#]{.pnum} Substituting into an `$annotation$` is not in the immediate context.

::: example
```cpp
[[=1]] void f();
[[=2, =3, =2]] void g();
void g [[=4, =2]] ();
```
`f` has one annotation and `g` has five annotations. These can be queried with metafunctions such as `std::meta::annotations_of` ([meta.reflect.annotation]).
:::

::: example
```cpp
template <class T>
[[=T::type()]] void f(T t);

void f(int);

void g() {
  f(0);   // OK
  f('0'); // error, substituting into the annotation results in an invalid expression
}
```
:::
:::
:::

Change the pack expansion rule in [temp.variadic]{.sref}/5.9:

::: std
[5]{.pnum} [...] Pack expansions can occur in the following contexts:

* [5.9]{.pnum} In an `$attribute-list$` ([dcl.attr.grammar]); the pattern is an `$attribute$`. [In an `$annotation-list$`; the pattern is an `$annotation$`.]{.addu}
:::

## Library

Add to the `<meta>` synopsis:

::: std
```diff
namespace std::meta {
  // ...
  consteval bool is_bit_field(info r);
  consteval bool is_enumerator(info r);
+ consteval bool is_annotation(info r);
  // ..

  // [meta.reflection.annotation], annotation reflection
+ consteval vector<info> annotations_of(info item);
+ consteval vector<info> annotations_of_with_type(info item, info type);

}
```
:::

Add `is_annotation` to [meta.reflection.queries]:

::: std
```diff
  consteval bool is_enumerator(info r);
+ consteval bool is_annotation(info r);
```

[#]{.pnum} *Returns*: `true` if `r` represents an enumerator [or annotation, respectively]{.addu}. Otherwise, `false`.

:::

Update the meanings of `$has-type$`, `type_of` and `value_of` in [meta.reflection.queries]:

::: std
```cpp
consteval bool $has-type$(info r); // exposition only
```

[1]{.pnum} *Returns*: `true` if  `r` represents a value, [annotation,]{.addu} object, variable, function whose type does not contain an undeduced placeholder type and that is not a constructor or destructor, enumerator, non-static data member, unnamed bit-field, direct base class relationship, or data member description. Otherwise, `false`.

```cpp
consteval info type_of(info r);
```

[#]{.pnum} *Constant When*: `$has-type$(r)` is `true`.

[#]{.pnum} *Returns*:

- [#.#]{.pnum} If `r` represents a value, object, variable, function, non-static data member, or unnamed bit-field, then the type of what is represented by `r`.

::: addu
- [#.*]{.pnum} Otherwise, if `r` represents an annotation, then `type_of(constant_of(r))`.
:::

- [#.2]{.pnum} Otherwise, if `r` represents an enumerator `$N$` of an enumeration `$E$`, then:
  - [#.#.#]{.pnum} If `$E$` is defined by a declaration `$D$` that precedes a point `$P$` in the evaluation context and `$P$` does not occur within an `$enum-specifier$` of `$D$`, then a reflection of `$E$`.
  - [#.#.#]{.pnum} Otherwise, a reflection of the type of `$N$` prior to the closing brace of the `$enum-specifier$` as specified in [dcl.enum].
- [#.#]{.pnum} Otherwise, if `r` represents a direct base class relationship, then a reflection of the type of the direct base class.
- [#.#]{.pnum} Otherwise, for a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`) ([class.mem.general]), a reflection of the type `$T$`.

```cpp
consteval info object_of(info r);
```

[#]{.pnum} *Constant When*: [...]

[#]{.pnum} *Returns*: [...]

```cpp
consteval info constant_of(info r);
```

[6]{.pnum} Let `$R$` be a constant expression of type `info` such that `$R$ == r` is `true`. [If `r` represents an annotation, then let `$C$` be its underlying constant.]{.addu}

[#]{.pnum} *Constant When*: [Either `r` represents an annotation or]{.addu} `[: $R$ :]` is a valid `$splice-expression$` ([expr.prim.splice]).

[#]{.pnum} *Effects*: Equivalent to:

```diff
+ if constexpr (is_annotation(R)) {
+   return $C$;
+ } else {
    return reflect_constant([: $R$ :]);
+ }
```
:::



Add the new section [meta.reflection.annotation]:

::: std
::: addu
```cpp
consteval vector<info> annotations_of(info item);
```

[1]{.pnum} *Constant When*: `item` represents a type, type alias, variable, function, namespace, enumerator, direct base class relationship, or non-static data member.

[#]{.pnum} Let `$E$` be

* [#.#]{.pnum} the corresponding `$base-specifier$` if `item` represents a direct base class relationship,
* [#.#]{.pnum} otherwise, the entity represented by `item`.

[#]{.pnum} *Returns*: A `vector` containing all of the reflections `$R$` representing each annotation applying to each declaration of `$E$` that precedes either some point in the evaluation context ([expr.const]) or a point immediately following the `$class-specifier$` of the outermost class for which such a point is in a complete-class context.

For any two reflections `@*R*~1~@` and `@*R*~2~@` in the returned `vector`, if the annotation represented by `@*R*~1~@` precedes the annotation represented by `@*R*~2~@`, then `@*R*~1~@` appears before `@*R*~2~@`. If `@*R*~1~@` and `@*R*~2~@` represent annotations from the same translation unit `T`, any element in the returned `vector` between `@*R*~1~@` and `@*R*~2~@` represents an annotation from `T`. [The order in which two annotations appear is otherwise unspecified.]{.note}

::: example
```cpp
[[=1]] void f();
[[=2, =3]] void g();
void g [[=4]] ();

static_assert(annotations_of(^^f).size() == 1);
static_assert(annotations_of(^^g).size() == 3);
static_assert([: constant_of(annotations_of(^^g)[0]) :] == 2);
static_assert(extract<int>(annotations_of(^^g)[1]) == 3);
static_assert(extract<int>(annotations_of(^^g)[2]) == 4);

struct Option { bool value; };

struct C {
    [[=Option{true}]] int a;
    [[=Option{false}]] int b;
};

static_assert(extract<Option>(annotations_of(^^C::a)[0]).value);
static_assert(!extract<Option>(annotations_of(^^C::b)[0]).value);

template <class T>
struct [[=42]] D { };

constexpr std::meta::info a1 = annotations_of(^^D<int>)[0];
constexpr std::meta::info a2 = annotations_of(^^D<char>)[0];
static_assert(a1 != a2);
static_assert(constant_of(a1) == constant_of(a2));

[[=1]] int x, y;
static_assert(annotations_of(^^x)[0] == annotations_of(^^y)[0]);
```
:::

```cpp
consteval vector<info> annotations_of_with_type(info item, info type);
```

[#]{.pnum} *Constant When*:

- [#.#]{.pnum} `annotations_of(item)` is a constant subexpression and
- [#.#]{.pnum} `dealias(type)` represents a type that is complete from some point in the evaluation context.

[#]{.pnum} *Returns*: A `vector` containing each element `e` of `annotations_of(item)` where `remove_const(type_of(e)) == remove_const(type)` is `true`, preserving their order.

:::
:::