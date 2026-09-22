---
title: "Token Sequence Injection"
document: P4380R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
    - name: Andrei Alexandrescu, NVIDIA
      email: <andrei@nvidia.com>
toc: true
status: progress
highlighting:
  keywords:
    cpp:
      - match
      - do_return
      - __macro
---

# Introduction

This is a follow-up to [@P3294R2]{.title}. In that paper, we argued that the right model for code generation in C++ is through token sequences by comparing to other potential models. In short, we believe that code generation in C++ must be C++-shaped, must be able to generate all of C++, and must allow for the full use of C++ in that generation. Hence, token sequences.

In this paper, we will demonstrate the power of token sequence-based injection by showing a number of examples of what we can do with it. All have been implemented in [Clang](https://github.com/brevzin/llvm-project/tree/compiler-explorer/barry) and are available on Compiler Explorer with the "barry prototypes" compiler. There are two categories of examples here: [direct injection](#direct-injection-of-token-sequences) and [token sequence macros](#token-sequence-macros).

# Direct Injection of Token Sequences

A token sequence literal is introduced via `^^{ ... }`. This has type `std::meta::token_sequence`. It, along with all other members mentioned in this paper, is intended to be declared in `<meta>`. The contents between the braces are lexed — no parsing happens until injection. This may not be valid C++, but it is a [valid token sequence](https://x.com/ridiculous_fish/status/1001681073917620224):

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

The sole requirement on the contents of a token sequence is that the `{` and `}` pairs are balanced. Parentheses and square brackets may be unbalanced. Token sequences operate after preprocessing, so token pasting a la `##` is not possible.

A token sequence can be explicitly injected via `std::meta::queue_injection` or implicitly injected through a number of hooks that we will walk through.

Two `token_sequence`s can be concatenated via `+` or `+=`. A `token_sequence` is a random access range of `token_sequence` and can be directly indexed. Two objects of type `token_sequence` can be compared for equality:

::: std
```cpp
static_assert(poem[0] == ^^{ if });
static_assert(std::ranges::size(poem) == 16); // 15 keywords and a comma
```
:::

In order to add external content into a token sequence, interpolation is done via the `\(e)` operator for some expression `e`. The parentheses are mandatory (otherwise `\u` could begin a UCN, this way it's always unambiguous). The meaning of interpolation depends on the type of `e` (done to minimize interpolation kinds):

* If `e` is (or is convertible to) `token_sequence`, the tokens of `e` are directly inserted in place.
* Otherwise, if `e` is (or is convertible to) `info`, then a single artificial token is inserted whose meaning is what `e` represents. For instance, `\(^^int)` interpolates a token which is the type `int` (note: it is not the keyword `int`) and `\(^^std::vector<int>)` interpolates a token which is the type `std::vector<int>` (note: it does not interpolate the 6 tokens that make up that type).
* Otherwise, if `e` is a `std::meta::operators`, then `\(e)` interpolates into the operator token (e.g. `\(std::meta::operators::op_equals_equals)` yields the token `==`).
* Otherwise, a token is inserted whose meaning is the _value_ of `e`. `\(std::ranges::size(poem))` would be the value `16` (note: not an integer literal).

Some tokens are very important to be able to add into a token sequence, but cannot actually be produced without help. The two most significant of these are identifiers and string literals. In order to do so, the library will provide the functions `std::meta::id` and `std::meta::str_lit`, respectively. We will see examples of these shortly.

That's probably enough to dive into the examples.

## Type Erasure

Given a type, whose declaration only contains member functions that aren’t templates, it is possible to mechanically produce a type-erased version of that interface. That implementation (for a non-owning version) can look as follows. Note that there are ways to do this more directly, and we can always provide better library utilities, but we wanted to show that even with just the basics, we can achieve a lot, even if it's mildly tedious.

This example can be viewed on [compiler explorer](https://compiler-explorer.com/z/xxbxMo7je), which is basically an implementation of [@P4148R2]{.title}'s `protocol_view`. An owning version is easily supportable, just with some more boilerplate work. Note that the compiler explorer link contains two panes: the normal execution pane that shows that it works, and an AST printer. The AST printer is a useful way to see what code is actually injected. More on this shortly.

We'll start with the usage side, and the obligatory `draw` example:

::: std
```cpp
#include <iostream>

template <class I>
class DynRef {
    // see below
};

struct Interface {
    auto draw(std::ostream&) const -> void;
};

// We actually do validate that the types conform to the interface
// int isn't even a class type, Wrong::draw is non-const
struct Wrong { auto draw(std::ostream&) -> void; };
static_assert(!std::constructible_from<DynRef<Interface>, int>);
static_assert(!std::constructible_from<DynRef<Interface>, Wrong>);

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

    auto stuff = std::vector<DynRef<Interface>>{c, v1, v2};
    for (auto& d : stuff) {
        std::cout << "* ";
        d.draw(std::cout);
        std::cout << '\n';
    }
}
```
:::

The type `DynRef<Interface>` is code-generated such that it has the interface from `Interface` and forwards calls through a manually-constructed vtable. The implementation is:

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
    /// Injects e.g.
    /// ```
    /// struct VTable {
    ///   auto (*f0)(void const*, std::ostream&);
    /// } const *vtable;
    /// ```
    ///
    /// The function is named f0, not draw, in order to handle overloads
    auto vtable_members = std::meta::list_builder();
    for (int k = 0; std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info  r = return_type_of(mem);
        auto params = std::meta::list_builder(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* } : ^^{ void* };
        params += param_tokens(parameters_of(mem));
        vtable_members += ^^{
            \(r) (*\(id("f", k++)))(\(params));
        };
    }

    queue_injection(^^{
        struct VTable {
            \(vtable_members)
        } const *vtable;
    });
}

consteval auto inject_vtable_for(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// template <class T>
    /// static inline constexpr VTable vtable_for = {
    ///   +[](void const* obj, std::ostream& p0) {
    ///     return static_cast<T const*>(obj)->draw(static_cast<std::ostream&>(p0));
    ///   }
    /// };
    /// ```
    /// The seemingly-unnecessary static_cast is to forward the parameters.
    auto inits = std::meta::list_builder(^^{ , });
    for (std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = identifier_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* obj } : ^^{ void* obj };
        params += param_tokens(parameters_of(mem), "p");
        std::meta::token_sequence cast_type = is_const(type_of(mem)) ? ^^{ T const* } : ^^{ T* };
        for (int k = 0; std::meta::info _ : parameters_of(mem)) {
            auto pN = id("p", k++);
            args += ^^{ static_cast<decltype(\(pN))&&>(\(pN)) };
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
    /// Injects e.g.
    /// ```
    /// auto draw(std::ostream& p0) const -> void {
    ///     return vtable->f0(data, static_cast<std::ostream&>(p0));
    /// }
    /// ```
    auto forwarders = std::meta::list_builder();
    for (int idx = 0; std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = id(identifier_of(mem));
        auto vtable_func = id("f", idx++);
        auto param_list = parameters_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += param_tokens(param_list, "p");
        args += ^^{ data };
        for (int k = 0; k < param_list.size(); ++k) {
            auto pN = id("p", k);
            args += ^^{ static_cast<decltype(\(pN))&&>(\(pN)) };
        }
        auto suffix = is_const(type_of(mem)) ? ^^{ const } : ^^{ };

        forwarders += ^^{
            auto \(name)(\(params)) \(suffix) -> \(r) {
                return vtable->\(vtable_func)(\(args));
            }
        };
    }

    queue_injection(forwarders);
}

consteval auto inject_satisfies_for(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// template <class T>
    /// static consteval auto satisfies_interface() -> bool {
    ///     return requires {
    ///         { std::declval<T const&>().draw(std::declval<std::ostream&>()) }
    ///             -> std::convertible_to<void>;
    ///     };
    /// }
    /// ```
    auto constraints = std::meta::list_builder();
    for (std::meta::info mem : interface_functions_of(interface)) {
        auto obj_type = is_const(type_of(mem)) ? ^^{ T const } : ^^{ T };
        auto args = std::meta::list_builder(^^{ , });
        for (std::meta::info param : parameters_of(mem)) {
            args += ^^{ std::declval<\(type_of(param))>() };
        }
        constraints += ^^{
            { std::declval<\(obj_type)&>().\(id(identifier_of(mem)))(\(args)) }
                -> std::convertible_to<\(return_type_of(mem))>;
        };
    }

    queue_injection(^^{
        template <class T>
        static consteval auto satisfies_interface() -> bool {
            return requires {
                \(constraints)
            };
        }
    });
}

consteval auto inject_erasing_ctor() -> void {
    queue_injection(^^{
        template <class T>
            requires (!std::same_as<std::remove_cvref_t<T>, DynRef>)
                 and (satisfies_interface<std::remove_reference_t<T>>())
        DynRef(T&& t [[clang::lifetimebound]])
            : data((void*)(&t))
            , vtable(&vtable_for<std::remove_cvref_t<T>>)
        {}
    });
}

template<class Iface> class DynRef {
    void *data;
    consteval {
        inject_Vtable(^^Iface);
        inject_vtable_for(^^Iface);
        inject_satisfies_for(^^Iface);
    }

public:
    consteval {
        inject_interface(^^Iface);
        inject_erasing_ctor();
    }

    DynRef(DynRef&) = default;
    DynRef(DynRef const&) = default;
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

Now, the Clang AST printer for `DynRef<Interface>` prints this (starting on line 49,094):

::: std
```cpp
template <class Iface> class DynRef {
    void *data;
public:
    DynRef<Iface>(DynRef<Iface> &) = default;
    DynRef<Iface>(const DynRef<Iface> &) = default;
};
template<> class DynRef<Interface> {
    void *data;
    const struct VTable {
        void (*f0)(const void *, std::ostream &);
    } *vtable;
    template <class T> static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const T *>(obj)->draw(static_cast<std::ostream &>(p0));
    }};
    template <class T> static consteval auto satisfies_interface() -> bool {
        return requires { { std::declval<const T &>().draw(std::declval<std::ostream &>()) } -> std::convertible_to<void>; };
    }
    template<> static consteval auto satisfies_interface<Interface>() -> bool {
        return requires { { std::declval<const Interface &>().draw(std::declval<std::ostream &>()) } -> std::convertible_to<void>; };
    }
public:
    auto draw(std::ostream &p0) const -> void {
        return this->vtable->f0(this->data, static_cast<std::ostream &>(p0));
    }
    template <class T> requires (!std::same_as<std::remove_cvref_t<T>, DynRef<Interface>>) && (satisfies_interface<std::remove_reference_t<T>>()) DynRef(T &&t) : data((void *)(&t)), vtable(&vtable_for<std::remove_cvref_t<T>>) {
    }
    template<> DynRef<Constant &>(Constant &t) : data((void *)(&t)), vtable(&vtable_for<std::remove_cvref_t<Constant &>>) {
    }
    template<> DynRef<Variable &>(Variable &t) : data((void *)(&t)), vtable(&vtable_for<std::remove_cvref_t<Variable &>>) {
    }
    template<> DynRef<const Variable &>(const Variable &t) : data((void *)(&t)), vtable(&vtable_for<std::remove_cvref_t<const Variable &>>) {
    }
    template<> DynRef<DynRef<Interface>>(DynRef<Interface> &&t)    DynRef(DynRef<Interface> &) = default;
    DynRef(const DynRef<Interface> &) noexcept = default;    static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const Constant *>(obj)->draw(static_cast<std::ostream &>(p0));
    }};
    static constexpr VTable vtable_for = {+[](const void *obj, std::ostream &p0) -> void {
        return static_cast<const Variable *>(obj)->draw(static_cast<std::ostream &>(p0));
    }};
};
```
:::

There is one particularly notable aspect to the implementation. Zooming in on this part of the implementation:

::: std
```cpp
consteval auto inject_erasing_ctor() -> void {
    queue_injection(^^{
        template <class T>
            requires (!std::same_as<std::remove_cvref_t<T>, DynRef>)
                 and (satisfies_interface<std::remove_reference_t<T>>())
        DynRef(T&& t [[clang::lifetimebound]])
            : data((void*)(&t))
            , vtable(&vtable_for<std::remove_cvref_t<T>>)
        {}
    });
}

template<class Iface> class DynRef {
    void *data;
    consteval {
        inject_Vtable(^^Iface);
        inject_vtable_for(^^Iface);     // <== vtable_for<T> injected here
        inject_satisfies_for(^^Iface);  // <== satisfies_interface<T> injected here
    }

public:
    consteval {
        inject_interface(^^Iface);
        inject_erasing_ctor();          // <== why do we need this
    }

    DynRef(DynRef&) = default;
    DynRef(DynRef const&) = default;
};
```
:::

The tokens injected by `inject_erasing_ctor` are just fixed tokens — there is no interpolation here. Why can't we write that code directly? The problem is that name lookup for `vtable_for` and `satisfies_interface` during initial template parsing would fail, because `vtable_for` is only injected by `inject_vtable_for(^^Iface)` and `satisfies_interface` is only injected by `inject_satisfies_for(^^Iface)`, neither of which will be run until instantiation. The compiler doesn't know that it's going to inject these names yet, so we need to _defer_ this lookup too. Hence, injecting pure, fixed tokens.

For `satisfies_interface`, we can work around this easily enough because we can just forward-declare the function and inject its definition later. But `vtable_for` is more difficult. There are two ways that we can avoid this issue:

1. Somehow declare that the first `consteval` block is introducing the name `vtable_for` _and_ that that name represents a variable template. That would allow the parse of `vtable_for<` to properly both find the name and treat the `<` as the beginning of a template argument list.
2. Allow us to forward-declare that variable template.

For now, we simply note this problem.

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
            auto t1 = std::meta::tokenize( v, "__uwb" ); // Standardized as "uwb", but implemented as "__uwb"
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

As you can see, that program fails as expected — `popcount<N>` deliberately uses 32-bit `__builtin_popcount` on 64-bit chunks and the generated tests catch it.

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
            ts += ^^{ constexpr auto operator++() -> \(cls)& { ++this->\(cfg.adaptor); return *this; } };
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

This is definitely an incomplete implementation still, as we're not really branching off of the `iterator_concept` as we should be. But it's demonstrating that the direction is possible. It probably also reveals the need to have more/better library machinery for doing name lookup, but this paper isn't proposing that. Also we don't have a way of observing hidden friend definitions at the moment (as the comment indicates).

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

The first two callbacks are `void`, their job is to perform work outside of the class — either injecting code into some namespace or by validating properties. But the third returns a `token_sequence`, its job is to report what tokens to inject into the class represented by `r`.

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

Starting there, that lets us implement `LoggingVector<T>` [like this](https://compiler-explorer.com/z/Mv4onhT94).

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

The `reflect_constant` check above is to skip the customization point from [@P4340R0]{.title}.

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

## Debugging

One important question to ask is: how debuggable is token sequence injection for a person? It might initially seem to be a daunting task, since the token sequences we're defining in this paper do not have any checking performed at the point of use, only at the point of injection. But, perhaps surprisingly, it's not so bad — precisely because they're just token sequences, the compiler already points us to _the_ tokens that are problematic.

Consider the type erasure example and problems we could make. Let's say we couldn't decide between writing `auto` and writing `VTable` for our variable and just wrote both:

::: std
```cpp
consteval auto inject_vtable_for(std::meta::info interface) -> void {
    // ...

    queue_injection(^^{
        template <class T>
        static inline constexpr VTable auto vtable_for = {
            \(inits)
        };
    });
}
```
:::

The [compiler error](https://compiler-explorer.com/z/bnYr19ovj) is:

::: std
```
<source>:90:40: error: cannot combine with previous 'type-name' declaration specifier
   90 |         static inline constexpr VTable auto vtable_for = {
      |                                        ^
```
:::


The same idea holds for any error that is a syntax error within a single token sequence. The compiler error points you to the right spot within that token sequence.

The harder cases are going to be cases where the syntax is initially correct but is wrong for a more complicated reason. Sticking with the same `inject_for_vtable` function, let's say I got one of the identifiers wrong and wrote `"q"` instead of `"p"` on [line 77](https://compiler-explorer.com/z/hKE8fbnrr):

::: std
```
<source>:77:23: error: use of undeclared identifier 'q0'
   77 |             auto pN = id("q", k++);
      |                       ^~
```
:::

The error still points to the correct location. It is that identifier that's incorrect. But why? Well, we can change our implementation to print (using [@P2758R5]) the tokens:

::: std
```cpp
auto next_fn = ^^{
    +[](\(params))-> \(r) {
        return static_cast<\(cast_type)>(obj)->\(id(name))( \(args) );
    }
};

std::constexpr_print_str(stringize(next_fn));

inits += next_fn;
```
:::

And when we do that, we will [see this](https://compiler-explorer.com/z/YG61Gda9x):

::: std
```
<source>:87:9: note: constexpr message: +[]( void const* obj , std::ostream & p0)-> void { return static_cast< T const*>(obj)->draw( static_cast<decltype(q0)&&>(q0) ); }
   87 |         std::constexpr_print_str(stringize(next_fn));
      |         ^
<source>:77:23: error: use of undeclared identifier 'q0'
   77 |             auto pN = id("q", k++);
      |                       ^~
```
:::

There is probably a better way to present that token stream, but it's already a good starting point, and if we stare at it we can spot the issue: the param is named `p0` but the argument we're using is `q0`. We suspect between the direct errors, the indirect errors that require utilities like `constexpr_print_str`, and the more involved issues that compile but are still wrong that push us to look at the AST printer, the debuggability isn't bad at all. But of course, the current state is the floor — we can always come up with better methods.

# Token Sequence Macros

We are proposing well-behaved language macros to give us a lot more power and flexibility than preprocessor macros, avoiding all issues with the latter. The macros described here are heavily inspired by [Swift's Expression Macros](https://github.com/swiftlang/swift-evolution/blob/main/proposals/0382-expression-macros.md).

A macro declaration has the same shape as a C++ function or function template. A simple integer identity macro might look like this:

::: std
```cpp
__macro id(int x) {
    return ^^{ \(x) };
}

static_assert(id!(1 + 2) * 3 == 9);
static_assert(std::same_as<decltype(id!(1)), int>);
static_assert(std::same_as<decltype(id!(2L)), int>);
```
:::

`__macro` is just a placeholder for now.

From the call side, this macro looks like a function call (except with the `!` suffix indicating that it is a macro invocation). However, inside of the macro definition, `x` is not an `int`. It is actually a reflection representing an expression. Properties of this expression can be observed, or even decomposed. A macro returns a token sequence, which is what the macro invocation is replaced with.

Importantly, expression identity is preserved. In the above examples, we are interpolating `x`, which gives us a token whose meaning is the expression that the macro was invoked with. In the first check, `id!(1 + 2) * 3`, we are not producing the _tokens_ `1 + 2` but rather the _expression_ `1 + 2`. Thus, the result of that full expression is `9`, not `7`. This avoids the need to constantly parenthesize that we're familiar with from C macros. Also, because the macro parameter has type `int`, the expression `x` will also always have type `int` — conversions happen on the way in. So `id!(2L)` is still an `int`, even though the argument has type `long`.

Macros can have two argument forms:

1. regular C++ functions or function templates — with real parameter types.
2. pure token sequence macros — where the parameter has explicit type `std::meta::token_sequence`.

In the latter case, no attempt at parsing happens at the call site — the tokens are just slurped up and passed through. Both forms are valuable, so we want to support both.

## fwd

Perhaps the most obvious macro is simple forwarding. This is one that already works fine as a C macro, which probably exists in lots and lots of code bases already. There are two ways to implement it — typed and untyped:

::: std
```cpp
// typed
template <class T>
__macro fwd(T&& t) {
    return ^^{ static_cast<\(type_of(t))&&>(\(t)) };
}

// untyped
__macro fwd(std::meta::token_sequence toks) {
    return ^^{ static_cast<decltype(\(toks))&&>(\(toks)) };
}
```
:::

Note that we need to be careful that in the typed case that we don't try to return `^^{ static_cast<T&&>(\(t)) }`. That would produce the literal tokens `static_cast<T&&>(e)`, where the type `T&&` very likely isn't the intended type (although it might happen to be right a decent amount of the time). We specifically need to cast to the type of `t`. This macro will show up in a bunch of our examples.

## check

The popular [Catch2](https://github.com/catchorg/catch2) test framework has a `CHECK` macro that allows for nice, fluent syntax that decomposes comparisons:

::: std
```cpp
CHECK(f() == g());
```
:::

might fail with:

::: std
```cpp
/app/example.cpp:8: FAILED:
  CHECK( f() == g() )
with expansion:
  1 == 2
```
:::

Catch2 can only decompose one layer of top-level comparison, and tries to catch misuses if users try to do more than that. But with better macros, we can produce exactly the same output while allowing much more flexibility. For now, this implementation just illustrates the same thing that Catch2 does, but hopefully illustrates what else is possible. Note that this implementation also relies on [@P2806R5]{.title}.

[Demo](https://compiler-explorer.com/z/qj46xT3Yq):

::: std
```cpp
consteval auto is_comparison(std::meta::operators op) -> bool {
    switch (op) {
        using enum std::meta::operators;
        case op_equals_equals:
        case op_exclamation_equals:
        case op_less:
        case op_greater:
        case op_less_equals:
        case op_greater_equals:
            return true;
        default:
            return false;
    }
}

template <class T, class U>
auto check_fail(char const* text, std::source_location sloc, T const& lhs, char const* op, U const& rhs) -> void {
    std::println("{}:{}: FAILED:", sloc.file_name(), sloc.line());
    std::println("  CHECK( {} )", text);
    std::println("with expansion:");
    std::println("  {} {} {}", lhs, op, rhs);
}

template <class T>
auto check_fail(char const* text, std::source_location sloc, T const& expr) -> void {
    std::println("{}:{}: FAILED:", sloc.file_name(), sloc.line());
    std::println("  CHECK( {} )", text);
    std::println("with expansion:");
    std::println("  {}", expr);
}

template <class T>
__macro check(T&& cond) {
    auto const text = std::meta::str_lit(source_text_of(cond));
    auto const sloc = source_location_of(cond);

    if (is_binary_operation(cond) and is_comparison(operator_of(cond))) {
        auto ops = operands_of(cond);
        char const* op = stringize(^^{ \(operator_of(cond)) });

        return ^^{
            do {
                auto&& lhs = \(ops[0]);
                auto&& rhs = \(ops[1]);
                if (not (fwd!(lhs) \(operator_of(cond)) fwd!(rhs))) {
                    ::check_fail(\(text), \(sloc), lhs, \(op), rhs);
                }
            }
        };
    }

    return ^^{
        do {
            auto&& expr = \(cond);
            if (!static_cast<bool>(expr)) {
                ::check_fail(\(text), \(sloc), expr);
            }
        }
    };
}
```
:::

Note that `check` takes `T&&` and not `bool` both because we want to support explicit conversions to bool (like `std::optional<T>`) and also because we don't want to have to walk up through that boolean conversion in the expression.

Now, one important consequence of the macro model here interpolating expressions rather than raw tokens is that they are partially _hygienic_. Meaning that the above implementation, which introduces local variables `lhs`, `rhs`, and `expr` in the two branches, is perfectly fine — those will never conflict with names that might appear in the expression passed into `check!`.

## Double Evaluation

The canonical C preprocessor macro disaster is attempting to write `MIN(a++, b)` and erroneously implementing it in such a way that `a++` evaluates twice. Note that in the above example, when we decomposed the comparison, we wrote:

::: std
```cpp
auto&& lhs = \(ops[0]);
auto&& rhs = \(ops[1]);
if (not (fwd!(lhs) \(operator_of(cond)) fwd!(rhs))) {
    ::check_fail(\(text), \(sloc), lhs, \(op), rhs);
}
```
:::

We evaluated the operands and then used `lhs` and `rhs`, we didn't write `\(ops[0])` twice. Well... what would happen if we did? As in:

::: std
```cpp
if (not (\(ops[0]) \(operator_of(cond)) \(ops[1]))) {
    ::check_fail(\(text), \(sloc), \(ops[0]), \(op), \(ops[1]));
}
```
:::

That would actually [fail to compile](https://compiler-explorer.com/z/eqnjara95). We diagnose if an expression is evaluated multiple times:

::: std
```
<source>:80:12: error: expansion of expression macro would evaluate this argument more than once
   80 |     check!(f() == g());
      |            ^
```
:::

That's a big footgun, entirely avoided.

## Abbreviated Lambdas

One of the things Barry has wanted for some time are abbreviated lambdas, which he attempted back in [@P0573R2]{.title}. As a proper macro that can actually look at its own input, this is actually very easy to implement: walk the provided token sequence looking for identifiers of the form `_X` and use that to determine the arity. Then simply paste the body in with the right number of parameters.

This macro is called an anaphoric macro — it is necessarily unhygienic.

::: std
```cpp
consteval auto placeholder_index(std::string_view id) -> std::optional<int> {
    if (id.size() == 2 and id[0] == '_' and '1' <= id[1] and id[1] <= '9') {
        return id[1] - '0';
    }
    return std::nullopt;
}

__macro λ(std::meta::token_sequence body) {
    auto arity = std::optional<int>();
    for (std::meta::token_sequence tok : body) {
        if (token_kind_of(tok) == std::meta::token_kind::identifier) {
            arity = std::max(arity, placeholder_index(identifier_of(tok)));
        }
    }

    auto params = std::meta::list_builder(^^{ , });
    for (int i = 1; i <= arity.value_or(0); ++i) {
        params += ^^{ auto&& \(std::meta::id("_", i)) };
    }

    return ^^{ [&](\(params)) -> decltype(auto) { return \(body); } };
}
```
:::

Coupled with some of our other utilities, that gives us the ability to write [this](https://compiler-explorer.com/z/PnGTPjr1d):

::: std
```cpp
struct [[=derive_debug]] Point {
    int x;
    int y;
    auto operator==(Point const&) const -> bool = default;
};

auto main() -> int {
    std::vector<Point> points = {{1, 2}, {3, 1}};
    std::ranges::sort(points, λ!(_1.y < _2.y));
    check!(points == std::vector<Point>{{3, 2}, {1, 2}});
}
```
:::

Which is an incredible way to write that predicate and check the result. Which of course fails, since that's not how sorting works, and gives us the nice output we want:

::: std
```
/app/example.cpp:154: FAILED:
  CHECK( points == std::vector<Point>{{3, 2}, {1, 2}} )
with expansion:
  [Point{.x=3, .y=1}, Point{.x=1, .y=2}] == [Point{.x=3, .y=2}, Point{.x=1, .y=2}]
```
:::

Note we used `_1`, `_2`, etc., as the placeholders here. But could just as easily be `$1`. Or, if you really like Swift, `$0`.

## try_

[@P2561R3]{.title} proposes a dedicated language operator for control flow operations. Well, in Rust, this operator originated as a macro. The proposed design in that paper would look like this:

::: std
```cpp
template <class T>
__macro try_(T&& e) {
    // `try_` only means anything inside a function: it early-returns from one.
    std::meta::info where = std::meta::macro_expansion_context();
    if (not is_function(where)) {
        std::constexpr_error_str("bad-try-context",
                                 "try_ must be invoked inside a function");
    }

    // Computed in the macro body -- NOT injected as `using` aliases.
    std::meta::info CT = substitute(^^try_traits, {remove_cvref(type_of(e))});
    std::meta::info RT = substitute(^^try_traits, {return_type_of(where)});

    return ^^{
        do [__r=\(e)] -> decltype(auto) {
            if (not \(CT)::should_continue(__r)) [[unlikely]] {
                return \(RT)::from_break(\(CT)::extract_break(fwd!(__r)));
            }
            do_return \(CT)::extract_continue(fwd!(__r));
        }
    };
}
```
:::

This uses the awkward init-hoist feature of `do` expressions rather than pattern matching, because that's what we have available. In any case, here we're using a new function `macro_expansion_context()` to get the top-level entry point into this macro. We need that to get the return type of the function we're in, which is the P2561 design.

And that gives us the behavior [we want](https://compiler-explorer.com/z/8aqrvPzxe):

::: std
```cpp
enum class E { };
template <class T>
auto get_data() -> std::expected<T, E>;

auto f1() -> std::expected<int, E> {
    auto&& data = try_!(get_data<int>()); // <== warning about dangling reference on this line
    return data;
}

auto consume(int x) -> int { return x; }

auto f2() -> std::expected<int, E> {
    auto&& data = consume(try_!(get_data<int>())); // <== no warning here
    return data;
}
```
:::

## define_op

[Boost.Lambda2](https://www.boost.org/doc/libs/latest/libs/lambda2/doc/html/lambda2.html) is a small library that, among other things, declares a whole bunch of function objects for the operators that don't have standard library equivalents. This is pure boilerplate, since this is very straightforward repetitive code, so the implementation uses macros.

We can make a much nicer macro for this, which takes two parameters: the identifier for the type we're introducing, and a mini-DSL actually showing the operator being used:

::: std
```cpp
define_op!(left_shift, x << y);
define_op!(negate, -x);
define_op!(post_inc, x++);

static_assert(left_shift{}(1, 4) == 16);
static_assert(negate{}(1) == -1);
```
:::

Just a single macro defines all three forms: binary, prefix, and postfix. Note that none of those parameters are valid expressions in this context — the names aren't declared yet, neither are `x` nor `y`. So these are `token_sequence` parameters. We just allow you to provide more than one of them, automatically parsing at the comma.

The implementation is [quite straightforward](https://compiler-explorer.com/z/T53xMj614):

::: std
```cpp
__macro define_op(std::meta::token_sequence name,
                  std::meta::token_sequence pattern) {
    // id op id
    if (size(pattern) == 3) {
        return ^^{
            struct \(name) {
                template <class L, class R>
                constexpr decltype(auto) operator()(L&& l, R&& r) const {
                    return fwd!(l) \(pattern[1]) fwd!(r);
                }
            };
        };
    }

    // id op or op id
    auto body = token_kind_of(pattern[0]) == std::meta::token_kind::identifier
        ? ^^{ fwd!(x) \(pattern[1]) }
        : ^^{ \(pattern[0]) fwd!(x) };

    return ^^{
        struct \(name) {
            template <class T>
            constexpr decltype(auto) operator()(T&& x) const {
                return \(body);
            }
        };
    };
}
```
:::

Note that in this case, the macro is invoked at declaration scope and returns a token sequence which is a declaration. That's novel — but it's not quite having arbitrary expressions at class or namespace scope, only macro invocations. This saves otherwise having to implement macros like this as a `consteval` block where the macro returns an expression which itself invokes `queue_injection`. Just a lot of unnecessary extra ceremony.

## Tuple Indexing and Short Circuiting

`std::tuple` does not currently have an index operator, as in `elems[0]`. Of course, it couldn't have a _normal_ index operator, since the type of the result would depend on which index is being acccessed. This would call for `constexpr` function parameters, or something like it. However, we don't really need that — all we want is the ability to rewrite the call `elems[0]` into the call `std::get<0>(elems)`. That's just [a macro](https://compiler-explorer.com/z/x199rPPzW):

::: std
```cpp
template <class... Ts>
struct my_tuple : std::tuple<Ts...> {
    using std::tuple<Ts...>::tuple;

    template <class Self>
    __macro operator[](this Self&& self, size_t idx) {
        return ^^{
            std::get<\(idx)>(\(self))
        };
    }
};

constexpr auto elems = my_tuple<int, std::string>(1, std::string("hello"));
static_assert(elems[0] == 1);
static_assert(elems[1] == "hello");
```
:::

For member function macros, we require an explicit object parameter so that there is actually an object parameter to interpolate. For operators though, we don't require the extra `!` syntax, since there's not really anywhere to put it.

Similarly, macro operators allow for the ability to have real short-circuiting, so that you could properly declare `operator or()` and `operator and()`:

::: std
```cpp
struct Bool {
    bool b;

    __macro operator or(this Bool self, bool rhs) {
        return ^^{ \(self).b or \(rhs) };
    }

    __macro operator and(this Bool self, bool rhs) {
        return ^^{ \(self).b and \(rhs) };
    }
};
```
:::

Which, as you can see, [does short circuit](https://compiler-explorer.com/z/YK16Pxs9Y):

::: std
```cpp
auto call() -> bool {
    std::println("call()");
    return true;
}

auto main() -> int {
    bool const a = Bool{true} or call();   // no print
    std::println("a = {}", a);
    bool const b = Bool{false} or call();  // prints "call()"
    std::println("b = {}", b);
    bool const c = Bool{true} and call();  // prints "call()"
    std::println("c = {}", c);
    bool const d = Bool{false} and call(); // no print
    std::println("d = {}", d);
}
```
:::

## `ranges::begin`

`ranges::begin` is [specified](https://eel.is/c++draft/range.access.begin) as a sequence of potential operations linearly, like so:

::: {.std .wording}
[2]{.pnum} Given a subexpression `E` with type `T`, let `t` be an lvalue that denotes the reified object for `E`.
Then:

* [#.#]{.pnum} If `E` is an rvalue and `enable_borrowed_range<remove_cv_t<T>>` is `false`, `ranges​::​begin(E)` is ill-formed.
* [#.#]{.pnum} Otherwise, if `T` is an array type ([dcl.array]) and `remove_all_extents_t<T>` is an incomplete type, `ranges​::​begin(E)` is ill-formed with no diagnostic required.
* [#.#]{.pnum} Otherwise, if `T` is an array type, `ranges​::​begin(E)` is expression-equivalent to `t + 0`.
* [#.#]{.pnum} Otherwise, if `auto(t.begin())` is a valid expression whose type models `input_or_output_iterator`, `ranges​::​begin(E)` is expression-equivalent to `auto(t.begin())`.
* [#.#]{.pnum} Otherwise, if `T` is a class or enumeration type and `auto(begin(t))` is a valid expression whose type models `input_or_output_iterator` where the meaning of begin is established as-if by performing argument-dependent lookup only ([basic.lookup.argdep]), then `ranges​::​begin(E)` is expression-equivalent to that expression.
* [#.#]{.pnum} Otherwise, `ranges​::​begin(E)` is ill-formed.
:::

So it would be pretty nice if we could implement it with the [same linear sequence](https://compiler-explorer.com/z/MWhneP666):

::: std
```cpp
namespace my {
namespace impl {
    void begin() = delete;

    template <class R>
    constexpr auto adl_begin(R&& r) noexcept(noexcept(auto(begin(r)))) -> decltype(auto(begin(r))) {
        return auto(begin(r));
    }
}

inline constexpr struct begin_fn {
    template <class R>
    __macro operator()(this begin_fn, R&& r) {
        if (not std::is_lvalue_reference_v<R>
            and not std::ranges::enable_borrowed_range<std::remove_cv_t<R>>) {
            std::constexpr_error_str("no-begin", "rvalue range is not borrowed");
        }
        else if (std::is_array_v<std::remove_reference_t<R>>) {
            if (not is_complete_type(remove_all_extents(^^std::remove_reference_t<R>))) {
                std::constexpr_error_str("no-begin", "T is an array with incomplete element type");
            } else {
                return ^^{ (\(as_lvalue(r)) + 0) };
            }
        }
        else if (requires(R&& t) { { auto(t.begin()) } -> std::input_or_output_iterator; }) {
            return ^^{ auto(\(as_lvalue(r)).begin()) };
        }
        else if (requires(R&& t) { { impl::adl_begin(t) } -> std::input_or_output_iterator; }) {
            return ^^{ ::my::impl::adl_begin(\(as_lvalue(r))) };
        }
        else {
            std::constexpr_error_str("no-begin", "no viable begin for this type");
        }

        return ^^{};  // unreachable: the error produces no expansion
    }
} begin{};
}
```
:::

The above implementation relies on [@P2758R5]{.title} to have some of those paths fail, and [meets the requirements](https://compiler-explorer.com/z/MWhneP666).

## `vec`

Rust doesn't have `initializer_list`, it instead has a macro `vec!` that can be used. We can do the same — implementing it in such a way that noncopyable types are supported as well:

::: std
```cpp
template <class... Args>
__macro vec(Args&&... args) {
    using T = std::remove_cvref_t<Args...[0]>;

    auto emplace_back_loop = std::meta::token_sequence();
    for (std::meta::info expr : {args...}) {
        emplace_back_loop += ^^{
            res.__emplace_back_assume_capacity(\(expr));
        };
    }

    return ^^{
        do {
            auto res = ::std::vector<\(^^T)>();
            res.reserve(\(sizeof...(Args)));
            \(emplace_back_loop);
            do_return res;
        }
    };
}
```
:::

We're using `__emplace_back_assume_capacity()`, which is a public helper in libc++'s `std::vector` implementation, because we know we have the capacity here, to avoid the extra checks. This macro finally supports what people have wanted for a while, and [it works](https://compiler-explorer.com/z/Kxr7aWEzq):

::: std
```cpp
auto v = vec!{
    std::make_unique<int>(1),
    std::make_unique<int>(2),
};
```
:::

Note also that macro invocation can use any bracket: `()`, `[]`, or `{}`. In this case, `{}` seems most appropriate on the call site, so that's what we do. For macro invocations with braces (but not parentheses or square brackets), a trailing comma can be supplied (as in the above), as is typical with braced lists.

## Logging

By now, it should be pretty clear how to write a basic logging macro. So we're going to take it up a notch or two. We're going to have a `Logger` type that is going to automatically define a logging *macro* for every `LogLevel` enum. That is, we're going to have a loop that itself injects macros:

::: std
```cpp
enum class LogLevel { debug, info, error };

struct Logger {
    LogLevel min_level;

    explicit Logger(LogLevel min_level) : min_level(min_level) { }

    auto do_log(LogLevel level, std::string_view fmt, auto&&... args) -> void {
        std::println("[{}] {}", level, std::vformat(fmt, std::make_format_args(args...)));
    }

    consteval {
        for (std::meta::info e : enumerators_of(^^LogLevel)) {
            auto name = std::meta::id(identifier_of(e));
            queue_injection(^^{
                template <class... Args>
                __macro \(name)(this Logger& self, std::string_view raw, Args&&... args) {
                    auto fs = substitute(^^::std::basic_format_string, {^^char, ^^Args...});
                    constexpr auto level = ::LogLevel::\(name);

                    auto call_args = std::meta::list_builder(^^{ , });
                    call_args += ^^{ fmt };
                    ((call_args += ^^{ \(args) }), ...);
                    return ^^{
                        do {
                            constexpr std::string_view fmt = \(fs)(\(raw)).get();
                            auto& self = \(self);
                            if (\(level) >= self.min_level) {
                                self.do_log(\(level), \(call_args));
                            }
                        }
                    };
                }
            });
        }
    }
};
```
:::

In the above, we actually have nested token sequences. An interpolation in a token sequence is actually associated with the _innermost_ token sequence that it's found in. Thus, the `\(level)` interpolation above isn't trying to immediately interpolate (which would fail, as there is no `level` variable). The macro body is merely parsed, `\(level)` binds to the inner literal and inteprolates each time the injected macro is _invoked_, when the body executes and `level` is in scope.

Now, we have a member macro `debug!`, `info!`, and `error!`. If we do something [like this](https://compiler-explorer.com/z/aPKhGz6sz), we'll see that `get()` is not even invoked, but the other macros do exist and we get our logs. All the format strings are still type checked:

::: std
```cpp
auto get() -> int {
    std::println("** called get **");
    return 6;
}

int main(int, char**) {
    auto log = Logger(LogLevel::info);
    int x = 5;
    log.debug!("x={} y={}", x, get());
    log.info!("{:>3}|{}", "ab", 2.5);
    log.error!("plain");
}
```
:::

# Summary

We are proposing a new fundamental kind in the language, `std::meta::token_sequence`. Unlike our suggestion in [@P3294R2], we think this merits being a distinct type (from `std::meta::info`) because we found that in practice the usage patterns are always disjoint, and a distinct type allows us to give it a suitable API shape — including indexing, concatenation, and iteration. We're proposing a single interpolator `\(e)` (see [syntax discussion](#choice-of-interpolator)). And then on top of that we're proposing expression/declaration macros which interact with the compiler by returning a token sequence that the input expression is replaced with.

As with Reflection, it comes with a decently sized library surface to help facilitate all the desired behavior:

## Library API

::: std
```cpp
namespace std::meta {

  // ── Token sequences ────────────────────────────────────────────────────
  using token_sequence = decltype(^^{ });
  struct token_iterator;
  consteval auto begin(token_sequence) -> token_iterator;
  consteval auto end  (token_sequence) -> token_iterator;
  consteval auto size (token_sequence) -> size_t;
  consteval auto empty(token_sequence) -> bool;

  template <class... Ts> consteval auto id      (Ts const&...) -> token_sequence;  // one identifier token
  template <class... Ts> consteval auto str_lit (Ts const&...) -> token_sequence;  // one string literal
  template <class... Ts> consteval auto tokenize(Ts const&...) -> token_sequence;  // lex text
  consteval auto stringize(token_sequence) -> char const*;

  class list_builder {                               // delimited concatenation
    consteval explicit list_builder(token_sequence delim = ^^{ });
    consteval auto operator+=(token_sequence) -> void;    // empty pieces are skipped
    consteval operator token_sequence() const;
  };

  // ── Token classification ───────────────────────────────────────────────
  enum class token_kind { identifier, keyword, literal, punctuator, annotation, unknown };
  consteval auto token_kind_of (token_sequence tok) -> token_kind;   // unknown: empty or >1 token
  consteval auto operator_of   (token_sequence tok) -> operators;    // a complete operator token
  consteval auto identifier_of (token_sequence tok) -> string_view;  // tok must be a single identifier token (else non-constant)

  // ── Injection ──────────────────────────────────────────────────────────
  consteval auto queue_injection(token_sequence) -> void;                  // into the current context
  consteval auto queue_injection(info target_ns, token_sequence) -> void;  // into target_ns

  // ── Expression macros: queries on a typed parameter (a reflection of the
  //    bound argument expression) ───────────────────────────────────────────
  consteval auto type_of            (info e) -> info;        // == decltype(arg as written)
  consteval auto source_text_of     (info e) -> string_view; // the argument's spelling
  consteval auto source_location_of (info e) -> source_location;
  consteval auto is_binary_operation(info e) -> bool;
  consteval auto operator_of        (info e) -> operators;
  consteval auto operands_of        (info e) -> vector<info>; // as written, each interpolable
  consteval auto is_constant_expression(info e) -> bool;      // constant_of(e) would succeed
  consteval auto as_lvalue          (info e) -> info;         // view as if bound to auto&& and named
  consteval auto macro_expansion_context() -> info;   // the function / class / namespace the
                                                      //   expansion lands in (macro bodies only)

  // ── Declaration cloning ────────────────────────────────────────────────
  struct clone_naming {
    token_sequence name = ^^{ };                    // replacement name; empty keeps the source's
    string_view template_parameter_prefix = "T";    // T0, T1, ...
    string_view parameter_prefix          = "p";    // p0, p1, ...
  };
  consteval auto declaration_of(info fn, clone_naming = {}) -> info;
  consteval auto is_declaration_spec(info r) -> bool;

  // Transformations: description in, new description out (composable).
  consteval auto make_override(info d) -> info;    // clone declared 'override' (not for templates)
  consteval auto make_noexcept(info d) -> info;    // clone declared noexcept

  // Fragments (each usable only in the position it names).
  struct template_list_options { bool defaults = true; };
  consteval auto template_parameter_list_for(info d, template_list_options = {})
      -> token_sequence;                           // 'class T0, size_t T1, class... T2'
                                                   //   (inside a written template<...>)
  consteval auto template_argument_list_for(info d) -> token_sequence;
                                                   // 'T0, T1, T2...' (refused: nonterminal pack)

  struct argument_list_options { bool forward = true; };
  consteval auto argument_list_for(info d, argument_list_options = {})
      -> token_sequence;                           // 'static_cast<decltype(p0)&&>(p0), p1...'
                                                   //   or 'p0, p1...' with {.forward = false}
  consteval auto forwarding_call_for(info d,
                                     token_sequence receiver,
                                     argument_list_options = {}) -> token_sequence;
                                                   // '(receiver).name<T0...>(args...)', receiver
                                                   //   cast to the member's cv/ref-qualifiers
}
```
:::


## Choice of Interpolator

One of the things that becomes clear when working through macro examples is that it does come up that we want to interpolate an argument into a function call, which ends up looking like `foo(\(arg))`. Those are two very different kinds of parentheses — the outer pair is a token in our output, while the inner pair is part of the interpolation operator. The stacking of parentheses makes the code a little challenging to parse. For a human that is, machines don't care.

However, there really aren't many options available to us for interpolation, since we _must_ support all C++ syntax. Other potential options here are:

|Syntax|Notes|
|-|----|
|`\{e}`|Braces instead of Parentheses.  This would help for the expression case, but there are plenty of examples where we're building up things next to real braces, so not sure this moves the needle much.|
|`${e}`|This would help a little because `$` is actually a frequent choice for interpolator in several other programming languages, so is somewhat familiar. Unlike `\(e)` which is only used by Swift. But `$` is a valid character to use in an identifier. So we would have to special case single `$` in the lexer when inside of a token sequence? That might also prevent `$e` without braces.|
|`@e` or `@{e}`|Unlike `$`, `@` isn't really a frequent choice for interpolator, but it does stand out. And single `@` is probably something we could take, although allowing `@e` without the braces runs the risk of clashing with Objective C.|

## Comparison with P2826

We've gone this far without mentioning [@P2826R4]{.title}. That proposal can handle some of the [macro use-cases](#token-sequence-macros) presented, but none of the raw token sequence inputs, and it's unclear if that direction can handle macros that want to observe the incoming expression and output different expressions based on input properties — like [`check!`](#check). Since that paper supports a strict subset of what we're proposing here, we think our approach is superior. Several examples in that paper require _less typing_ than they would in the model that we're proposing, but we don't actually think that matters at all.

---
references:
    - id: P2561R3
      citation-label: P2561R3
      title: A control flow operator
      author:
        - family: Barry Revzin
      issued:
        - year: 2026
          month: 09
          day: 12
      URL: https://isocpp.org/files/papers/P2561R3.html
---