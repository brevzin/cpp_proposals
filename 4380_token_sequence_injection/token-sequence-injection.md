---
title: "Token Sequence Injection"
document: P4380R0
date: today
audience: EWG
author:
    - name: Andrei Alexandrescu, NVIDIA
      email: <andrei@nvidia.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
status: progress
---

# Introduction

This is a follow-up to [@P3294R2]{.title}. In that paper, we argued that the right model for code generation in C++ is through token sequences by comparing to other potential models. In short, we believe that code generation in C++ must be C++-shaped, must be able to generate all of C++, and must allow for the full use of C++ in that generation. Hence, token sequences.

In this paper, we will demonstrate the power of token sequence based injection by showing a number of examples of what we can do with it. All have been implemented in [Clang](https://github.com/brevzin/llvm-project/tree/compiler-explorer/barry) and are available on Compiler Explorer with the "barry prototypes" compiler. There are two categories of examples here: [direction injection](#direct-injection-of-token-sequences) and [token sequence macros](#token-sequence-macros).

# Direct Injection of Token Sequences

A token sequence literal is introduced via `^^{ ... }`. This has type `std::meta::token_sequence`. The contents between the braces are lexed — no parsing happens until injection. This may not be valid C++, but it is a [valid token sequence](https://x.com/ridiculous_fish/status/1001681073917620224):

::: std
```cpp
constexpr auto poem = ^^{
    if volatile or bitor
    try short break
    goto this private void, float
    and return new
};
```
:::

A token sequence can be explicitly via `std::meta::queue_injection` or implicitly through a number of hooks that we will walk through.

Two `token_sequence`s can be concatenated via `+` or `+=`. A `token_sequence` is a random access range of `token_sequence` and can be directly indexed. Two objects of type `token_sequence` can be compared for equality:

::: std
```cpp
static_assert(poem[0] == ^^{ if });

// whitespace doesn't count, but the comma does
static_assert(std::ranges::size(poem) == 16);
```
:::

In order to add external content into a token sequence, interpolation is done via the `\(e)` operator. The parentheses are mandatory (otherwise `\u` could begin a UCN, this way it's always unambiguous). The meaning of interpolation depends on the type of `e` (done to minimize interpolation kinds):

* If `e` is (or is convertible to) `token_sequence`, the tokens of `e` are directly concatenated in place.
* Otherwise, if `e` is (or is convertible to) `info`, then a single artificial token is inserted whose meaning is what `e` represents. For instance, `\(^^int)` interpolates a token which is the type `int` (note: it is not the keyword `int`).
* Otherwise, a token is inserted whose meaning is the _value_ of `e`. `\(std::ranges::size(poem))` would be the value `16` (note: not an integer literal).

Some tokens are very important to be able to add into a token sequence, but cannot actually be produced without help. The two most significant of these are identifiers and string literals. In order to do so, the library will provide the functions `std::meta::id` and `std::meta::str_lit`, respectively. We will see examples of these shortly.

That's probably enough to dive into the examples.

## Type Erasure

Given a type, whose declaration only contains member functions that aren’t templates, it is possible to mechanically produce a type-erased version of that interface. That implementation (for a non-owning version) can look as follows. Note that there are ways to do this more directly, and we can always provide better library utilities, but we wanted to show that even with just the basics, we can achieve a lot, even if it's mildly tedious.

This example can be viewed on [compiler explorer](https://compiler-explorer.com/z/cbfsK5e6d), which is basically an implementation of [@P4148R2]{.title}'s `protocol_view`. An owning version is easily supportable, just with some more boilerplate work. Note that the compiler explorer link contains two panes: the normal execution pane that shows that it works, and an AST printer. The AST printer is a useful way to see what code is actually injected. More on this shortly.

We'll start with the usage side, and the obligatory `draw` example:

::: std
```cpp
#include <iostream>

struct Interface {
    auto draw(std::ostream&) const -> void;
};

struct Constant {
    auto draw(std::ostream& out) const -> void {
        out << "Constant(" << 1 << ')';
    }
};

struct Variable {
    int i;

    auto draw(std::ostream& out) const -> void {
        out << "Variable(" << i << ')';
    }
};

int main() {
    auto c = Constant();
    auto v1 = Variable{10};
    auto v2 = Variable{20};

    auto stuff = std::vector<Dyn<Interface>>{c, v1, v2};
    for (auto& d : stuff) {
        std::cout << "* ";
        d.draw(std::cout);
        std::cout << '\n';
    }
}
```
:::

The type `Dyn<Interface>` is code-generated such that it has the interface from `Interface` and forwards calls through a manually-constructed vtable. The implementation is:

::: std
```cpp
#include <meta>
using std::meta::id;

consteval auto interface_functions_of(std::meta::info ty) -> std::vector<std::meta::info> {
    auto v = members_of(ty, std::meta::access_context::current());
    std::erase_if(v, [](std::meta::info m){
        return not is_function(m) or is_special_member_function(m) or is_static_member(m);
    });
    return v;
}

consteval auto param_tokens(std::vector<std::meta::info> params,
                            std::string_view  name_prefix = "")
    -> std::meta::token_sequence
{
    auto result = std::meta::list_builder(^^{ , });
    for (int k = 0; std::meta::info p : params) {
        if (is_function_parameter(p)) p = type_of(p);
        if (not name_prefix.empty()) {
            result += ^^{ \(p) \(id(name_prefix, k++)) };
        } else {
            result += ^^{ \(p) };
        }
    }

    return result;
}

consteval auto inject_Vtable(std::meta::info interface) -> void {
    auto vtable_members = std::meta::list_builder();
    for (std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info  r = return_type_of(mem);
        auto name = identifier_of(mem);
        auto params = std::meta::list_builder(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* } : ^^{ void* };
        params += param_tokens(parameters_of(mem));
        vtable_members += ^^{
            \(r) (*\(id(name)))(\(params));
        };
    }

    queue_injection(^^{
        struct VTable {
            \(vtable_members)
        } const *vtable;
    });
}

consteval auto inject_vtable_for(std::meta::info interface) -> void {
    auto inits = std::meta::list_builder(^^{ , });
    for (std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = identifier_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* obj } : ^^{ void* obj };
        params += param_tokens(parameters_of(mem), "p");
        std::meta::token_sequence cast_type = is_const(type_of(mem)) ? ^^{ T const* } : ^^{ T* };
        for (int k = 0; std::meta::info _ : parameters_of(mem)) {
            args += ^^{ \(id("p", k++)) };
        }

        inits += ^^{
            +[](\(params))-> \(r) {
                return static_cast<\(cast_type)>(obj)->\(id(name))( \(args) );
            }
        };
    }

    queue_injection(^^{
        template <class T>
        static inline constexpr VTable vtable_for = {
            \(inits)
        };
    });
}


consteval auto inject_interface(std::meta::info interface) -> void {
    auto forwarders = std::meta::list_builder();
    for (std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = id(identifier_of(mem));
        auto param_list = parameters_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += param_tokens(param_list, "p");
        args += ^^{ data };
        for (int k = 0; k < param_list.size(); ++k) {
            args += ^^{ \(id("p", k)) };
        }
        auto suffix = is_const(type_of(mem)) ? ^^{ const } : ^^{ };

        forwarders += ^^{
            auto \(name)(\(params)) \(suffix) -> \(r) {
                return vtable->\(name)(\(args));
            }
        };
    }

    queue_injection(forwarders);
}

consteval auto inject_erasing_ctor() -> void {
    queue_injection(^^{
        template <class T> Dyn(T&& t)
            : data(&t)
            , vtable(&vtable_for<std::remove_cvref_t<T>>)
        {}
    });
}

template<class Iface> class Dyn {
    void *data;
    consteval {
        inject_Vtable(^^Iface);
        inject_vtable_for(^^Iface);
    }

public:
    consteval {
        inject_interface(^^Iface);
        inject_erasing_ctor();
    }

    Dyn(Dyn&) = default;
    Dyn(Dyn const&) = default;
};
```
:::

There are many uses of `std::meta::id()` in the above example to introduce an _identifier_ token. The signature of this magic function is:

::: std
```cpp
template <class... Ts>
consteval auto id(Ts const&...) -> token_sequence;
```
:::

It takes an arbitrary sequence of integers or strings, concatenates them, and returns a `token_sequence` consisting of a single identifier token. So `std::meta::id("p", 0)` is the token sequence `^^{ p0 }`. Since this sort of concatenation is very common, having `std::meta::id` be able to do it directly instead of forcing the user to build up a `string` is both more convenient and more efficient.

The one not-completely manual aspect of this implementation is the use of `std::meta::list_builder`. It turns out we frequently need to be able to produce possibly-delimited sequences to inject. It's not a very complicated facility:

::: std
```cpp
class list_builder {
    token_sequence body = ^^{};
    token_sequence delim;
    bool first = true;

public:
    consteval explicit list_builder(token_sequence delim = ^^{ }) : delim(delim) { }

    consteval auto operator+=(token_sequence tok) -> void {
        if (tok != ^^{ }) {
            if (not first) {
                body += delim;
            }

            first = false;
            body += tok;
        }
    }

    consteval operator token_sequence() const {
        return body;
    }
};
```
:::

Now, the Clang AST printer for `Dyn<Interface>` prints this (starting on line 48,895):

::: std
```cpp
template <class Iface> class Dyn {
    void *data;
public:
    Dyn<Iface>(Dyn<Iface> &) = default;
    Dyn<Iface>(const Dyn<Iface> &) = default;
};
template<> class Dyn<Interface> {
    void *data;
    const struct VTable {
        void (*draw)(const void *, std::ostream &);
    } *vtable;
    template <class T> static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const T *>(obj)->draw(p0);
    }};
public:
    auto draw(std::ostream &p0) const -> void {
        return this->vtable->draw(this->data, p0);
    }
    template <class T> Dyn(T &&t) : data(&t), vtable(&vtable_for<std::remove_cvref_t<T>>) {
    }
    template<> Dyn<Constant &>(Constant &t) : data(&t), vtable(&vtable_for<std::remove_cvref_t<Constant &>>) {
    }
    template<> Dyn<Variable &>(Variable &t) : data(&t), vtable(&vtable_for<std::remove_cvref_t<Variable &>>) {
    }
    template<> Dyn<Dyn<Interface>>(Dyn<Interface> &&t)    Dyn(Dyn<Interface> &) = default;
    Dyn(const Dyn<Interface> &) noexcept = default;    static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const Constant *>(obj)->draw(p0);
    }};
    static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const Variable *>(obj)->draw(p0);
    }};
};
```
:::

There is one particularly notable aspect to the implementation. Zooming in on this part of the implementation:

::: std
```cpp

consteval auto inject_erasing_ctor() -> void {
    queue_injection(^^{
        template <class T> Dyn(T&& t)
            : data(&t)
            , vtable(&vtable_for<std::remove_cvref_t<T>>)
        {}
    });
}

template<class Iface> class Dyn {
    void *data;
    consteval {
        inject_Vtable(^^Iface);
        inject_vtable_for(^^Iface); // <== vtable_for injected here
    }

public:
    consteval {
        inject_interface(^^Iface);
        inject_erasing_ctor();     // <==  why do we do this
    }

    Dyn(Dyn&) = default;
    Dyn(Dyn const&) = default;
};
```
:::

The tokens injected by `inject_erasing_ctor` are just fixed tokens — there is no interpolation here. Why can't we write that code directly? The problem is that name lookup for `vtable_for` during initial template parsing would fail, because `vtable_for` is only injected by `inject_vtable_for(^^Iface)`, which won't be run until instantiation. The compiler doesn't know that it's going to inject that name yet, so we need to _defer_ this lookup too. Hence, injecting pure, fixed tokens. One way to avoid this would be able to somehow declare that the first `consteval` block is introducing the name `vtable_for` _and_ that that name represents a variable template. Another way would be to allow us to forward-declare that variable template. For now, we simply note this problem.

## Push-Based Customization I (Formatting)

Using standard C++26 with annotations, it is possible to do pull-based defaulted formatting:

::: std
```cpp
struct DeriveDebug { };
inline constexpr auto derive_debug = DeriveDebug();

template <class T>
    // this function isn't in standard C++26
    // but is easily implementable
    requires (has_annotation(^^T, derive_debug))
struct std::formatter<T> {
    // ...
};

struct [[=derive_debug]] Config {
    std::string name;
    int amount;
};
```
:::

And this is great! The problem, however, is that the specialization we're introducing is just another partial specialization. Which means it could easily be ambiguous with some other partial specialization if one exists. That inherently makes this an incomplete solution to the problem, and we'd like to do better.

One way to do better is to give annotations a completion callback. In this case, when `Config` becomes complete, the annotation can have a hook that will inject an _explicit_ specialization of `formatter`. This now becomes a complete solution to the problem. On [compiler explorer](https://compiler-explorer.com/z/crbEqje1r):

::: std
```cpp
struct DeriveDebug {
    consteval auto on_complete(std::meta::info ty) const -> void {
        auto fmt_body = std::meta::list_builder();
        fmt_body += ^^{
            auto out = std::format_to(ctx.out(), "{}{{", \(display_string_of(ty)));
        };

        auto delim = [first=true, &fmt_body]() mutable {
            if (not first) {
                fmt_body += ^^{
                    *out++ = ',';
                    *out++ = ' ';
                };
            }
            first = false;
        };

        auto unchecked = std::meta::access_context::unchecked();
        for (auto nsdm : nonstatic_data_members_of(ty, unchecked)) {
            delim();
            fmt_body += ^^{
                out = std::format_to(out,
                    \(std::meta::str_lit(".", identifier_of(nsdm), "={}")),
                    object.\(nsdm)
                );
            };
        }

        fmt_body += ^^{
            *out++ = '}';
            return out;
        };

        queue_injection(^^std, ^^{
            template <>
            struct formatter<\(ty)> {
                constexpr auto parse(auto& ctx) { return ctx.begin(); }

                auto format(\(ty) const& object, auto& ctx) const {
                    \(fmt_body);
                }
            };
        });
    }
};

inline constexpr DeriveDebug derive_debug{};
```
:::

The usage of `[[=derive_debug]]` is the same, it's just push-vs-pull.

We're also using `std::meta::str_lit` here to create a string literal. Similar to `std::meta::id`, its signature is:

::: std
```cpp
template <class... Ts>
consteval auto str_lit(Ts const&...) -> token_sequence;
```
:::

This performs concatenation of its arguments and returns a token sequence containing a single string literal. With the AST printer, we can see that we're injecting this specialization for `Config` (I removed the repetitive specializations for the particular context types):

::: std
```cpp
template<> struct formatter<Config> {
    constexpr auto parse(auto &ctx) {
        return ctx.begin();
    }

    auto format(const Config &object, auto &ctx) const {
        auto out = std::format_to(ctx.out(), "{}{{", display_string_of(ty));
        out = std::format_to(out, ".name={}", object.name);
        *out++ = ',';
        *out++ = ' ';
        out = std::format_to(out, ".amount={}", object.amount);
        *out++ = '}';
        return out;
    }
};
```
:::

## Literal Testing

Similar to producing a string literal, we also have a way to convert from a `token_sequence` to a string and from a string to a `token_sequence`:

::: std
```cpp
template <class... Ts>
  consteval auto tokenize(Ts const&...) -> token_sequence;
consteval auto stringize(token_sequence) -> char const*;
```
:::

This allows for testing, e.g., suffix literals. [This example](https://compiler-explorer.com/z/Ke6nYP5b1) comes courtesy of Peter Dimov:

::: std
```cpp
#include <meta>
#include <print>
#include <ranges>
#include <cstdint>

constexpr std::uint64_t splitmix64( std::uint64_t z )
{
    z ^= z >> 30;
    z *= 0xbf58476d1ce4e5b9ull;
    z ^= z >> 27;
    z *= 0x94d049bb133111ebull;
    z ^= z >> 31;

    return z;
}

static int s_errors;

template<class T1, class T2>
void test( char const* q1, char const* q2, T1 const& t1, T2 const& t2 )
{
    if( !(t1 == t2) )
    {
        std::print( stderr, "Test '{} == {}' failed: '{}' != '{}'\n", q1, q2, t1, t2 );
        ++s_errors;
    }
}

template<int N> int popcount( unsigned _BitInt(N) const& v )
{
    int r = 0;

    for( int i = 0; i < N; i += 64 )
    {
        r += __builtin_popcount( static_cast<std::uint64_t>( v >> i ) );
    }

    return r;
}

int popcount( std::uint64_t v )
{
    return __builtin_popcountll( v );
}

int main()
{
    constexpr auto fn = ^^{ popcount };

    consteval
    {
        for( auto v: std::views::iota( 0, 5 ) | std::views::transform( splitmix64 ) )
        {
            auto t1 = std::meta::tokenize( v, "__uwb" ); // "uwb"
            auto q1 = ^^{ \(fn)(\(t1)) };

            auto t2 = std::meta::tokenize( v, "ull" );
            auto q2 = ^^{ \(fn)(\(t2)) };

            queue_injection( ^^{ test( \(stringize(q1)), \(stringize(q2)), \(q1), \(q2) ); } );
        }
    }

    return s_errors;
}
```
:::

As you can see, that program fails:

::: std
```
Test 'popcount(6238072747940578789__uwb) == popcount(6238072747940578789ull)' failed: '11' != '25'
Test 'popcount(15839785061582574730__uwb) == popcount(15839785061582574730ull)' failed: '13' != '31'
Test 'popcount(2185194620014831856__uwb) == popcount(2185194620014831856ull)' failed: '13' != '32'
Test 'popcount(13232826040865663252__uwb) == popcount(13232826040865663252ull)' failed: '13' != '29'
```
:::

## Iterator Interface

Zach Laine is the author of [Boost.STLInterfaces](https://www.boost.org/doc/libs/latest/doc/html/stl_interfaces.html) which he had proposed for standardizing in [@P2727R0]{.title}.

We can much more easily approach this problem using code injection, because rather than have complicated and disjoint template specializations to handle all the different cases, we can just have `if` statements. In order to do this, we need to be able to extend `members_of` to observe the declared members of a class while it's still being defined. Note that we don't actually need any more than observation.

[Demo](https://compiler-explorer.com/z/PTPTE158P):

::: std
```cpp
template <class B> constexpr bool base_can_deref   = requires(B const& b) { *b; };
template <class B> constexpr bool base_can_pre_inc = requires(B& b) { ++b; };
template <class B> constexpr bool base_can_eq      =
    requires(B const& a, B const& b) { { a == b } -> std::convertible_to<bool>; };

struct IterConfig {
    std::meta::info iterator_concept;
    std::meta::info value_type;
    std::meta::info difference_type = ^^std::ptrdiff_t;
    std::meta::info adaptor = std::meta::info();
};

// Which operators has the user declared SO FAR in the class being defined?
// Caveat: We don't see friend declarations, that's still TBD for what the interface should be
consteval auto declares_op(std::meta::info cls, std::meta::operators which) -> bool {
    for (auto m : members_of(cls, std::meta::access_context::unchecked()))
        if (is_operator_function(m) && operator_of(m) == which)
            return true;
    return false;
}

consteval auto declares_fn(std::meta::info cls, std::string_view name) -> bool {
    for (auto m : members_of(cls, std::meta::access_context::unchecked()))
        if (has_identifier(m) && identifier_of(m) == name)
            return true;
    return false;
}

consteval auto iterator_interface(std::meta::info cls, IterConfig cfg) -> void {
    using namespace std::meta;
    auto ts = ^^{
        using iterator_concept = \(cfg.iterator_concept);
        using value_type = \(cfg.value_type);
        using difference_type = \(cfg.difference_type);
    };

    bool plus_eq  = declares_op(cls, op_plus_equals);
    bool minus    = declares_op(cls, op_minus);
    bool pre_inc  = declares_op(cls, op_plus_plus);
    bool pre_dec  = declares_op(cls, op_minus_minus);
    bool eq       = declares_op(cls, op_equals_equals);
    bool deref    = declares_op(cls, op_star);

    if (cfg.adaptor != std::meta::info()) {
        // Adaptor path: defer to cfg.adaptor for all operations
        info base_t = type_of(cfg.adaptor);
        bool base_deref   = extract<bool>(substitute(^^base_can_deref, {base_t}));
        bool base_pre_inc = extract<bool>(substitute(^^base_can_pre_inc, {base_t}));
        bool base_eq      = extract<bool>(substitute(^^base_can_eq, {base_t}));

        if (base_deref && !deref) {
            ts += ^^{ constexpr decltype(auto) operator*() const { return *this->\(cfg.adaptor); } };
        }
        if (base_pre_inc && !pre_inc) {
            ts += ^^{ constexpr auto operator++() -> \(cls)& { ++base_reference(); return *this; } };
        }
        if (base_pre_inc) {
            ts += ^^{ constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; } };
        }
        if (base_eq && !eq) {
            ts += ^^{
                friend constexpr bool operator==(\(cls) const& a, \(cls) const& b) {
                    return a.\(cfg.adaptor) == b.\(cfg.adaptor);
                }
            };
        }
        queue_injection(ts);
        return;
    }

    // Direct path: decide based on visible members.
    // Wrong signatures error in the injected bodies at definition time — can't check these yet
    if (plus_eq) {
        ts += ^^{
            constexpr auto operator++() -> \(cls)& { *this += 1; return *this; }
            constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; }
            constexpr auto operator--() -> \(cls)& { *this += -1; return *this; }
            constexpr auto operator--(int) -> \(cls) { auto tmp = *this; --*this; return tmp; }
            constexpr auto operator-=(std::ptrdiff_t n) -> \(cls)& { *this += -n; return *this; }
            constexpr decltype(auto) operator[](std::ptrdiff_t n) const { auto tmp = *this; tmp += n; return *tmp; }
            friend constexpr auto operator+(\(cls) it, std::ptrdiff_t n) -> \(cls) { it += n; return it; }
            friend constexpr auto operator+(std::ptrdiff_t n, \(cls) it) -> \(cls) { it += n; return it; }
            friend constexpr auto operator-(\(cls) it, std::ptrdiff_t n) -> \(cls) { it += -n; return it; }
        };
    } else {
        if (pre_inc) {
            ts += ^^{ constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; } };
        }
        if (pre_dec) {
            ts += ^^{ constexpr auto operator--(int) -> \(cls) { auto tmp = *this; --*this; return tmp; } };
        }
    }
    if (minus) {
        if (!eq) {
            ts += ^^{
                friend constexpr bool operator==(\(cls) const& a, \(cls) const& b) { return (a - b) == 0; }
            };
        }
        ts += ^^{
            friend constexpr auto operator<=>(\(cls) const& a, \(cls) const& b) { return (a - b) <=> 0; }
        };
    }
    queue_injection(ts);
}
```
:::

This is definitely an incomplete implementation still, as we're not really branching off of the `iterator_concept` as we should be. But it's demonstrating that the direction is possible. It probably also reveals the need to have more/better library machinery for doing name lookup, but this paper isn't proposing that. Also we don't have a way of observing hidden friend declarations at the moment (as the comment indicates).

## Push-Based Customization II (Structured Bindings)

That section [earlier](#push-based-customization-i-formatting) about how push-based customization was a complete solution? Well, that wasn't entirely accurate. It turns out that if we try out that approach to inject structured bindings specialization on class completion, it doesn't quite work. Because, for templates, the class template is not necessarily instantiated when we check for some properties. Barry has a [blog post](https://brevzin.github.io/c++/2026/09/14/push-vs-pull/) on this topic in more detail.

In order to do this better, what we really need to do is inject a _partial_ class specialization at the point where the _template_ becomes complete. To do that, we need a different hook. We saw `on_complete()` earlier, now we have `on_template_defined()` as well. [Demo](https://compiler-explorer.com/z/T55Tbdjr9)

::: std
```cpp
// ------------------------- the library -------------------------
namespace lib {
    template <class T, template <class...> class Z>
    concept specializes = has_template_arguments(remove_cvref(^^T))
                      and template_of(remove_cvref(^^T)) == ^^Z;

    struct inject_bindings_t {
        consteval auto on_template_defined(std::meta::info tmpl) const -> void {
            queue_injection(^^std, ^^{
                template <class... Ts>
                struct tuple_size<\(tmpl)<Ts...>>
                    : integral_constant<size_t, size(\(tmpl)<Ts...>::tuple_elements)>
                { };

                template <size_t I, class... Ts>
                struct tuple_element<I, \(tmpl)<Ts...>> {
                    using type = [: type_of(\(tmpl)<Ts...>::tuple_elements[I]) :];
                };
            });

            queue_injection(parent_of(tmpl), ^^{
                template <size_t I, ::lib::specializes<\(tmpl)> Self>
                constexpr auto get(Self&& self) -> decltype(auto) {
                    return (((Self&&)self).[: self.tuple_elements[I] :]);
                }
            });
        }
    };
    inline constexpr inject_bindings_t inject_bindings{};
}

// ------------------------- the user code -------------------------
template <class T>
class [[=lib::inject_bindings]] wide_result {
    T hi;
    T lo;

public:
    constexpr wide_result(T hi, T lo) : hi(hi), lo(lo) { }

    static constexpr std::meta::info tuple_elements[] = {^^hi, ^^lo};
};

static_assert(std::tuple_size_v<wide_result<uint64_t>> == 2); // works!

auto main() -> int {
    auto [hi, lo] = wide_result<uint64_t>(123, 456);
    std::println("hi={}, lo={}", hi, lo);
}
```
:::

Note that the above example is injecting the partial specialization:

::: std
```cpp
template <class... Ts>
struct tuple_size<wide_result<Ts...>> { ... };
```
:::

But `wide_result` isn't a variadic class template, it's unary — we pick `class... Ts` as an attempted catch-all. We don't have [@P2989R2]{.title}, so this is the best attempt. So far anyway, keep reading.

However, another aspect of the above implementation that is a little unsatisfying is the fact that we're injecting `get` into the enclosing namespace of the template. It would be better to make `get` local. Which would additional avoid the need for the `specializes` concept there. Well, we've got two annotation hooks so far, let's just add a third:

1. `annot.on_complete(r)` gets invoked when the class represented by `r` becomes complete.
2. `annot.on_template_defined(r)` gets invoked when the class template represented by `r` is defined (before any specialization happens).
3. `annot.inject_members(r)` gets invoked right before the `}` when the class represented by `r` is about to be complete. Before the special members become defined.

The first two callbacks are `void`, their job is to perform work outside of the class — either injecting code into some namespace or by validating properties. But the third returns a `token_sequence`, it's job is to report what tokens to inject into the class represented by `r`.

With that third hook, we get [this tighter implementation](https://compiler-explorer.com/z/qcsdrzqx6):

::: std
```cpp
struct inject_bindings_t {
    consteval auto on_template_defined(std::meta::info tmpl) const -> void {
        queue_injection(^^std, ^^{
            template <class... Ts>
            struct tuple_size<\(tmpl)<Ts...>>
                : integral_constant<size_t, size(\(tmpl)<Ts...>::tuple_elements)>
            { };

            template <size_t I, class... Ts>
            struct tuple_element<I, \(tmpl)<Ts...>> {
                using type = [: type_of(\(tmpl)<Ts...>::tuple_elements[I]) :];
            };
        });
    }

    consteval auto inject_members(std::meta::info) const -> std::meta::token_sequence {
        return ^^{
            public:
            template <size_t I, class Self>
            constexpr auto get(this Self&& self) -> decltype(auto) {
                return (((Self&&)self).[: tuple_elements[I] :]);
            }
        };
    }
};

inline constexpr inject_bindings_t inject_bindings{};
```
:::

And then, once we can `inject_members()`, we can...

## `Eq` as a metaclass

One of the C++20 language features was the ability to default `operator==` — which does a memberwise equality comparison of the subobjects, or produces a deleted operator if any of the subobjects are not comparable. By the time a class becomes complete, its subobject types have to be complete as well, so we could do that checking ourselves. And then produce that same language feature in about 40 lines of library code.

[Demo](https://compiler-explorer.com/z/zEeGW3scv):

::: std
```cpp
template <class T>
concept EqComparable = requires (T const& t) {
  static_cast<bool>(t == t);
};

struct Eq_t {
    consteval auto inject_members(std::meta::info r) const -> std::meta::token_sequence {
        auto subobjects = subobjects_of(r, std::meta::access_context::unchecked());
        if (not std::ranges::all_of(subobjects, [](std::meta::info o){
            return extract<bool>(substitute(^^EqComparable, {type_of(o)}));
            }))
        {
            // not all comparable? defined as deleted
            return ^^{
                friend constexpr bool operator==(\(r) const&, \(r) const&) = delete;
                friend constexpr bool operator!=(\(r) const&, \(r) const&) = delete;
            };
        }

        std::meta::list_builder cmp(^^{ && });
        for (std::meta::info s : subobjects) {
            cmp += ^^{ (__lhs.\(s) == __rhs.\(s)) };
        }

        if (subobjects.empty()) {
            cmp += ^^{ true };
        }

        return ^^{
            friend constexpr bool operator==(\(r) const& __lhs [[maybe_unused]],
                                             \(r) const& __rhs [[maybe_unused]]) {
                return \(cmp);
            }

            friend constexpr bool operator!=(\(r) const& __lhs, \(r) const& __rhs) {
                return !(__lhs == __rhs);
            }
        };
    }
};
inline constexpr Eq_t Eq{};

struct [[=Eq]] P {
  int x;
  int y;
};
static_assert(P{1, 2} == P{1, 2});
static_assert(P{1, 2} != P{1, 3});

struct [[=Eq]] D : P {
    int z;
};
static_assert(D{{1, 2}, 3} == D{{1, 2}, 3});
static_assert(D{{1, 2}, 3} != D{{1, 2}, 4});
static_assert(D{{1, 2}, 3} != D{{1, 3}, 3});


template <class T> struct [[=Eq]] Wrap { T t; };
struct NonComparable { };

static_assert(std::equality_comparable<Wrap<int>>);
static_assert(!std::equality_comparable<Wrap<NonComparable>>);
```
:::

In order to be a full C++20 replacement, this would have to handle C arrays as well. That's doable as well, with a little bit more work, but the above should demonstrate that it's clearly feasible. And indeed, being able to implement useful language features as short library features is exactly the promise of [@P0707R5]{.title}.

## Logging Vector

One of the problems we wanted to solve in [@P3294R2] was to be able to create a `LoggingVector<T>`: clone the interface of `std::vector<T>` and just log every call that happens. This is actually extremely hard to do in C++ because of the wealth of complexity of C++ function (template) declarations. This basically cannot be doable without added compiler help. Consider what we need to be able to handle:

* arbitrary new template parameters, which would need to be renamed in a way that doesn't collide
* the declaration can refer to names within the type, including private names
* there could be default arguments (which themselves could do... anything)
* etc.

Attempting to provide a low-level interface to iterate over individual parameters is going be impossible. On top of that, once we do clone the declaration, how do we forward the call from `LoggingVector<T>`'s implementation to `std::vector<T>`'s? In general, we have just as much complexity. Does the function have an explicit or implicit object parameter? But the killer is: if it's a function template, which (if any) template parameters must be provided explicitly?

Again, we don't think it's possible to even implement this part of it without compiler help. To that end, we're providing a dedicated API for declaration cloning:

::: std
```cpp
struct clone_naming {
    token_sequence name = ^^{ };                   // empty: keep the source's name
    string_view template_parameter_prefix = "T";   // -> T0, T1, ...
    string_view parameter_prefix = "p";            // -> p0, p1, ...
};

consteval auto declaration_of(info fn, clone_naming naming = {}) -> info;
consteval auto forwarding_call_for(info d, token_sequence receiver) -> token_sequence;
```
:::

Starting there, that lets us implement `LoggingVector<T>` [like this](https://compiler-explorer.com/z/Mv4onhT94):

::: std
```cpp
std::vector<std::string_view> calls;
void log_call(std::string_view name) { calls.push_back(name); }

template <class T>
class LoggingVector {
  std::vector<T> impl;

public:
  LoggingVector(std::vector<T> v) : impl(std::move(v)) {}

  consteval {
    for (std::meta::info m : members_of(^^std::vector<T>, std::meta::access_context::unprivileged())) {
      if (is_static_member(m)
          or is_special_member_function(m)
          or is_constructor(m)
          or is_constructor_template(m)
          or is_operator_function(m) or is_operator_function_template(m)
          or is_conversion_function(m) or is_conversion_function_template(m)
          or not has_identifier(m)
          or identifier_of(m) == "reflect_constant"
          or (not is_function(m) and not is_function_template(m))) {
        continue;
      }

      auto d = std::meta::declaration_of(m);
      auto call = std::meta::forwarding_call_for(d, ^^{ impl });
      queue_injection(^^{
      public:
        \(d) {
          ::log_call(\(std::meta::str_lit(identifier_of(m))));
          return \(call);
        }
      });
    }
  }
};
```
:::

Which now, because we're actually _cloning_ the declaration, all the right call shapes work. We can pass braced-init-lists into functions on `LoggingVector` too:

::: std
```cpp
auto main() -> int {
  LoggingVector<int> lv(std::vector<int>{1, 2, 3});

  // variadic member template; returns vector<int>::reference (int&)
  int& r = lv.emplace_back(4);
  assert(r == 4);
  static_assert(std::same_as<decltype(lv.emplace_back(5)), int&>);

  // mixed concrete + pack parameters: the const_iterator argument converts
  // in the inner call
  auto it = lv.emplace(lv.begin(), 0);
  assert(*it == 0);

  // a whole overload set through one forwarder
  lv.insert(lv.begin(), 7);

  // the constrained template, with a real range
  int more[] = {8, 9};
  lv.append_range(more);

  // the const overloads are their own clones: const begin() is const_iterator
  const LoggingVector<int>& clv = lv;
  static_assert(
      !std::is_same_v<decltype(lv.begin()), decltype(clv.begin())>);

  // plain members are cloned too
  assert(lv.size() == 8);
  assert(lv.front() == 7);
  assert(lv.back() == 9);
  lv.pop_back();

  assert(calls.size() >= 9);
  assert(calls[0] == "emplace_back");
  assert(calls[2] == "emplace");  // calls[1] is the begin() argument

  // A braced-init-list argument deduces against the clone's *real* parameter
  // type (initializer_list<int>) -- the per-name deducing-this forwarder
  // could not do this at all.
  lv.assign({5, 6});
  assert(calls.back() == "assign");
  assert(lv.size() == 2 && lv.front() == 5 && lv.back() == 6);
}
```
:::

A very similar problem to `LoggingVector<T>` is...

## Mocking

Mocking an interface is very similar to [type erasure](#type-erasure), but now we'll see the `declaration_of` API for a more concise implementation. The added benefit is that it also allows us to handle default arguments (which are otherwise impossible). We just need to add an API to manipulate the declaration. In this case, cloning a declaration will strip the `noexcept` by default so we can add it back (proper `noexcept` handling is still an open question):

::: std
```cpp
consteval auto make_override(info d) -> info;
consteval auto make_noexcept(info d) -> info;
```
:::

Which gives us [this implementation](https://compiler-explorer.com/z/rrnfx5fKz):

::: std
```cpp
template <class I>
struct mock : I {
    mutable std::vector<std::string_view> calls_;

    int call_count(std::string_view name) const {
        return std::ranges::count(calls_, name);
    }

    consteval {
        for (std::meta::info m : members_of(^^I, std::meta::access_context::current())) {
            if (not is_function(m)
                or not is_virtual(m)
                or is_destructor(m)
                or is_operator_function(m)
                or is_conversion_function(m)) {
                continue;
            }

            auto name = identifier_of(m);
            auto d = make_override(declaration_of(m));
            if (is_noexcept(type_of(m))) {
                d = make_noexcept(d);
            }
            std::meta::info ret = return_type_of(m);

            // The handler member's type: std::function<R(Params...)>.
            auto ptypes = std::meta::list_builder(^^{ , });
            for (std::meta::info p : parameters_of(m)) {
                ptypes += ^^{ \(type_of(p)) };
            }

            // Forward the clone's parameters (p0, p1, ...) into the handler.
            auto args = argument_list_for(d);

            // The handler named {name}_
            auto handler = std::meta::id(name, "_");
            queue_injection(^^{
                public:
                std::function<\(ret)(\(ptypes))> \(handler);
            });

            // The full override
            queue_injection(^^{
                public:
                \(d) {
                    calls_.push_back(\(std::meta::str_lit(name)));
                    if (\(handler)) {
                        return \(handler)(\(args));
                    } else {
                        return \(ret)();
                    }
                }
            });
        }
    }
};
```
:::

Unlike the `LoggingVector` example where we need to generate the call `impl.name(args...)`, here we just need `args...`, which is a subset of the functionality — hence `std::meta::argument_list_for()`.

Otherwise, everything follows. And given an interface like:

::: std
```cpp
struct Calculator {
    virtual ~Calculator() = default;
    virtual int add(int a, int b) = 0;
    virtual int negate(int x) const = 0;           // const virtual
    virtual std::string describe() const = 0;      // class-type result
    virtual void reset() = 0;                      // void
    virtual int increment(int x, int by = 1) = 0;  // default argument
    virtual int fast(int x) const noexcept = 0;    // noexcept virtual
};
```
:::

`mock<Calculator>` will expose all of those functions with the same interface — including the default arguments! The linked example tests this:

::: std
```cpp
auto run_twice(Calculator& c, int start) -> int {
    c.reset();
    return c.increment(c.increment(start));  // uses the default 'by'
}

auto main() -> int {
    mock<Calculator> m;
    m.add_ = [](int a, int b) { return a + b; };
    m.increment_ = [](int x, int by) { return x + by; };

    Calculator& c = m;

    // Virtual dispatch lands in the mock.
    assert(c.add(2, 3) == 5);

    // Uninteresting calls (no handler) return a default-constructed result.
    assert(c.negate(5) == 0);
    assert(c.describe() == "");

    // Handlers can be (re)set mid-test.
    m.negate_ = [](int x) { return -x; };
    assert(c.negate(7) == -7);

    // The cloned default argument works through both static types. (GMock's
    // MOCK_METHOD cannot mock a defaulted parameter at all.)
    assert(c.increment(41) == 42);
    assert(m.increment(10, 5) == 15);

    // Drive it through interface-only code.
    assert(run_twice(c, 0) == 2);

    // The noexcept member mocks like any other.
    m.fast_ = [](int x) { return x * 3; };
    assert(c.fast(5) == 15);

    // Expectations.
    assert(m.call_count("add") == 1);
    assert(m.call_count("negate") == 2);
    assert(m.call_count("describe") == 1);
    assert(m.call_count("reset") == 1);
    assert(m.call_count("increment") == 4);
    assert(m.calls_.front() == "add");
}
```
:::

## Push-Based Customization III

Now that we've gone through the [logging](#logging-vector) and [mocking](#mocking) examples, we've basically built up the pieces to do push-based customization completely: by simply cloning the template head in a similar way that we cloned our functions. That implementation looks [like this](https://compiler-explorer.com/z/a1xqhz6Yj):

::: std
```cpp
struct inject_bindings_t {
    consteval auto on_template_defined(std::meta::info tmpl) const -> void {
        auto d = declaration_of(tmpl);
        auto head = template_parameter_list_for(d, {.defaults = false});
        auto args = template_argument_list_for(d);
        auto self = ^^{ \(tmpl)<\(args)> };

        queue_injection(^^std, ^^{
            template <\(head)>
            struct tuple_size<\(self)>
                : integral_constant<size_t, size(\(self)::tuple_elements)>
            { };

            template <size_t I, \(head)>
            struct tuple_element<I, \(self)> {
                using type = [: type_of(\(self)::tuple_elements[I]) :];
            };
        });
    }

    consteval auto inject_members(std::meta::info) const -> std::meta::token_sequence {
        return ^^{
            public:
            template <size_t I, class Self>
            constexpr auto get(this Self&& self) -> decltype(auto) {
                return (((Self&&)self).[: tuple_elements[I] :]);
            }
        };
    }
};

inline constexpr inject_bindings_t inject_bindings{};
```
:::

For `wide_result<T>`, `template_parameter_list_for(d)` will give us precisely `class T0` and `template_argument_list_for(d)` will give us `T0`. This now will work for all template shapes, even without having universal template parameters.

# Token Sequence Macros
