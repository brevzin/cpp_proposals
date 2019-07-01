---
title: Generalized pack declaration and usage
document: DxxxxRx
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction and Motivation

C++11 introduced variadic templates, one of the truly transformational language
features introduced that standard. Despite pretty tight restrictions on where
packs could be declared and how they could be used, this feature has proven
incredibly successful. Three standards later, there hasn't even been much change.
C++17 added a couple new ways to use packs (fold expressions [@N4191] and
using-declarations [@P0195R2]), C++20 will add a new way to introduce them
(in lambda capture [@P0780R2]) and iterate over them (expansion statements 
[@P1306R1]). That's it.

There have been many papers in the interlude about trying to enhance pack
functionality: a language typelist [@N3728], fixed size and homogeneous packs
[@N4072] (and later [@P1219R1]), indexing and slicing into packs [@N4235] and
[@P0535R0], being able to declare packs in more places [@P0341R0] and other
places [@P1061R0].

In short, there's been work in this space, although not all of these papers have
been discussed by Evolution. Although, many of these have been received favorably
and then never followed up on.

Yet the features that keep getting hinted at and requested again and again
are still missing from our feature set: 

1. the ability to declare a variable pack at class, namespace, or local scope
2. the ability to index into a pack
3. the ability to unpack a tuple, or tuple-like type, inline

All efficiently, from a compile time perspective. Instead, for (1) we have to use
`std::tuple`, for (2) we have to use `std::get`, `std::tuple_element`, or, if
we're only dealing with types, something like `mp_at_c` [@Boost.Mp11], for (3)
we have to use `std::apply()`, which necessarily introduces a new scope.
`std::apply()` is actually worse than that, since it doesn't play well with
callables that aren't objects and even worse if you want to use additional
arguments on top of that:

```cpp
std::tuple args(1, 2, 3);

// I want to call f with args... and then 4
std::apply([&](auto... vs){ return f(vs..., 4); }, args);
```

Matt Calabrese is working on a library facility to help address the shortcomings
here [@Calabrese.Argot].


This paper attempts to provide a solution to these problems, building on the work
of prior paper authors. The goal of this paper is to provide a better
implementation for a library `tuple`, one that ends up being much easier to
implement, more compiler friendly, and more ergonomic. The paper will piecewise
introduce the necessary langauge features, increasing in complexity as it goes. 

# The Tuple Example

This paper proposes the ability to declare a variable pack wherever we can declare
a variable today:

```cpp
namespace xstd {
    template <typename... Ts>
    struct tuple {
        Ts... elems;
    };
}
```

That gives us all the members that we need, using a syntax that arguably has
obvious meaning to any reader familiar with C++ packs. All the usual rules
follow directly from there. That class template is an aggregate, so we can use
aggregate initialization:

```cpp
xstd::tuple<int, int, int> x{1, 2, 3};
```

## Empty variable pack

One question right off the back: what does `xstd::tuple<> t;` mean? The same way 
that an empty function parameter pack means a function taking no arguments, an
empty member variable pack means no member variables. `xstd::tuple<>` is an empty 
type. 

## Constructors

But `tuple` has constructors. `tuple` has _lots_ of constructors. We're not going
to go through all of them in this paper, just the interesting ones. But let's 
at least start with the easy ones:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        constexpr tuple() requires (DefaultConstructible<Ts> && ...)
            : elems()...
        { }
        
        constexpr tuple(Ts const&... args)
                requires (CopyConstructible<Ts> && ...)
            : elems(args)...
        { }

    private:
        Ts... elems;
    };
}
```

Note the new pack expansion in the _mem-initializer_. This is a new ability this
paper is proposing. It wouldn't have made sense if you could not declare a member
variable pack.

Let's pick a more complex constructor. A `std::tuple<Ts...>` can be constructed
from a `std::pair<T, U>` if `sizeof...(Ts) == 2` and the two corresponding types
are convertible. How would we implement that? To do that check, we need to get
the corresponding types. How do we get the first and second types from `Ts`?

## Pack Indexing

This paper proposes the "simple selection" facility initially introduced in
[@N4235] (and favorably received in Urbana 2014): `T.[I]` is the `I`th element
of the pack `T`, which is a type if `T` is a type pack and a value otherwise.

Such indexing allows for implementing the pair converting constructor:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        template <ConvertibleTo<Ts.[0]> T, ConvertibleTo<Ts.[1]> U>
            requires sizeof...(Ts) == 2
        constexpr tuple(std::pair<T, U> const& p)
            : elems.[0](p.first)
            , elems.[1](p.second)
        { }
    private:
        Ts... elems;
    };
}
```

A properly constrained converting constructor from a pack:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        template <typename... Us>
            requires (Constructible<Ts, Us> && ...) // everything is convertible
                && (sizeof...(Us) > 1 ||            // exclude the copy ctor match
                    !DerivedFrom<std::remove_cvref_t<Us.[0]>, tuple>)
        template <ConvertibleTo<Ts.[0]> T, ConvertibleTo<Ts.[1]> U>
            requires sizeof...(Ts) == 2
        constexpr tuple(Us&&... us)
            : elems(std::forward<Us>(us))...
        { }
    private:
        Ts... elems;
    };
}
```


As well as implementing `tuple_element`:

```cpp
namespace xstd {
    template <typename... > class tuple;
    
    template <size_t I, typename>
    struct tuple_element;
    template <size_t I, typename... Ts>
    struct tuple_element<I, tuple<Ts...>> requires (I < sizeof...(Ts))
    {
        using type = Ts.[I];
    };
}
```

This is nicer than status quo, but I think we can do better in this regard with
a little bit more help.

## Pack aliases and generalized indexing

The previous section defines the syntax `T.[I]` to be indexing into a pack, `T`.
Let's immediately generalize this. Let's also say that a _type_ can be pack-indexed
into is the type provides an alias named `...`

That is:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        using ... = Ts; // declares that tuple<Ts...> can be indexed just like Ts...
                        // note that the Ts on the right-hand-side is not expanded
    };
    
    template <size_t I, typename Tuple>
    struct tuple_element {
        using type = Tuple.[I]; // indexes via the pack Tuple::...
    };
}
```

This isn't quite right though, since we want to constrain `tuple_element` here.
In the wild, this is fine, since `Tuple.[I]` will be SFINAE-friendly (simply
discard the overload if the type either does not provide a pack template or
the _constant-expression_ `I` is out of bounds for the size of the pack).

We have the operator `sizeof...`, which takes a pack. We just need a way of
passing the pack itself. To do that, this paper proposes the syntax `T.[:]`:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        using ... = Ts;
    };
    
    template <size_t I, typename Tuple>
    struct tuple_element;

    template <size_t I, typename Tuple> requires (I < sizeof...(Tuple.[:]))
        using type = Tuple.[I];
    };
}
```

## Named pack aliases

Pack aliases can be named as well, it's just that the unnamed pack alias gets
special treatment as far as the language is concerned:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        using ... = Ts;
        using ...refs = Ts&;
    };
    
    using Record = tuple<int, double, std::string>;
    static_assert(std::is_same_v<Record.[0], int>);
    static_assert(std::is_same_v<Record::refs.[1], double&>);
}
```

## Unpacking

Let's go back to our initial sketch, where `xstd::tuple` was an aggregate:

```cpp
namespace xstd {
    template <typename... Ts>
    struct tuple {
        Ts... elems;
    };
}
```

Since we know `elems` is a pack, we can directly access and unpack it into
a function:

```cpp
int sum(int x, int y, int z) { return x + y + z; }

xstd::tuple<int, int, int> point{1, 2, 3};
int s = sum(point.elems...); // ok, 6
```

This can work since the compiler knows that `point.elems` is a pack, and unpacking
that is reasonable following the rules of the language. 

However, we quickly run into a problem as soon as we add more templates into
the mix:

```cpp
template <typename Tuple>
int tuple_sum(Tuple t) {
    return sum(t.elems...); // ??
}
```

This isn't going to work. From [@Smith.Pack]:

> ```cpp
> template<typename T> void call_f(T t) {
>   f(t.x ...)
> }
> ```
> Right now, this is ill-formed (no diagnostic required) because "t.x" does not 
> contain an unexpanded parameter pack. But if we allow class members to be pack 
> expansions, this code could be valid -- we'd lose any syntactic mechanism to 
> determine whether an expression contains an unexpanded pack. This is fatal to at 
> least one implementation strategy for variadic templates. It also admits the 
> possibility of pack expansions occurring outside templates, which current 
> implementations are not well-suited to handle.

We can't have _no_ syntactic mechanism, so we need one: the same one used in the
previous section to annotate a pack: `T.[:]`:

```cpp
template <typename Tuple>
int tuple_sum(Tuple t) {
    return sum(t.elems.[:]...);
}
```

Compare the above to the C++20 status quo of having to indirect via `std::apply()`:


```cpp
// status quo
template <typename Tuple>
int tuple_sum(Tuple t) {
    return std::apply([](auto... e){
        return sum(e...);
    }, t);
}
```

But this is an incomplete solution, since while showing the constructor examples,
the member `elems` was private. How are we accessing it?

## Generalized unpacking and `operator...`

In the same way that we let `xstd::tuple<Ts...>` be directly indexed into, we
can also let it be directly unpacked. We do that with the help of `operator...()`:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        operator Ts&       ...() &       { return elems; }
        operator Ts const& ...() const&  { return elems; }
        operator Ts&&      ...() &&      { return std::move(*this).elems; }
        operator Ts const&&...() const&& { return std::move(*this).elems; }
    private:
        Ts... elems;
    };
}
```

We do not need to write `elems.[:]` here because we know `elems` is a pack, it's
declared as such. Note that this form of the declaration uses an unexpanded pack
on the left (the `...` does not expand the `Ts const&`, not really anyway) and
in the body. Later sections in the paper will show other forms. Any other
declaration syntax kind of looks wrong:

```cpp
operator ...Ts&(); // but we're not introducing a pack named Ts?
Ts& operator...(); // why is ths Ts& on the wrong side?
```

Also, from here on out, this paper will only use the `const&`
overloads of relevant functions for general sanity (see also [@P0847R2]). The
above declarations allow for:

```cpp
template <typename Tuple>
constexpr auto tuple_sum(Tuple const& tuple) {
    return (tuple.[:] + ...);
}

static_assert(tuple_sum(xstd::tuple(1, 2, 3)) == 6);
```

or more generally:

```cpp
template <typename F, typename Tuple>
constexpr decltype(auto) apply(F&& f, Tuple&& tuple) {
    return std::invoke(std::forward<F>(f), std::forward<Tuple>(tuple).[:]...);
}
```

As well as writing our `get` function template:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        using ... = Ts;
        operator Ts const& ...() const& { return elems; }
    private:
        Ts... elems;
    };
    
    template <size_t I, typename... Ts>
    constexpr auto const& get(tuple<Ts...> const& v) noexcept {
        return v.[I];
    }
}
```

Although since the syntax directly allows for `v.[0]`, why would anyone write
`xstd::get<0>(v)`? And if you could write `f(t.[:]...)` or, in non-dependent
contexts, simply `f(t...)`, why would anyone call `apply()`?

## Packs of tuples

The previous section showed how to write `apply` taking a single function and a
single tuple. What if we generalized it it taking multiple tuples? We can do that:

```cpp
template <typename F, typename... Tuples>
constexpr decltype(auto) apply(F&& f, Tuples&&... tuples) {
    return std::invoke(std::forward<F>(f), std::forward<Tuples>(tuples).[:]... ...);
}
```

That's six `.`s, I added a space for clarity. This works because with pack
expansion, we go outside in. The outer-most `...` has to expand the packs in
the subexpression `std::forward<Tuples>(tuples).[:]...`, which turns the above into:

```cpp
template <typename F, typename T@~0~@, typename T@~1~@, ..., typename T@~N-1~@>
constexpr decltype(auto) apply(F&& f, T@~0~@ t@~0~@, T@~1~@ t@~1~@, ..., T@~N-1~@ t@~N-1~@) {
    return std::invoke(std::forward<F>(f),
        std::forward<T@~0~@>(t@~0~@).[:]...,
        std::forward<T@~1~@>(t@~1~@).[:]...,
        ...
        std::forward<T@~N-1~@>(t@~N-1~@).[:]...);
}
```

And then we unpack each of these `...`s through the appropriate `operator...`s.

Similarly, `tuple_cat` would be:

```cpp
template <typename... Tuples>
constexpr std::tuple<Tuples.[:]... ...> tuple_cat(Tuples&&... tuples) {
    return {std::forward<Tuples>(tuples).[:]... ...};
}
```

Admitedly, six `.`s is a little cryptic. But is it any worse than the current
implementation?

## Default unpacking

If all we want to do is unpack a type into its subobjects, it seems annoyingly
wasteful to require _four_ overloads of `operator...()` to get that done. C++20
already introduces two operators that implicitly iterate over all subobjects
(`<=>` from [@P0515R3] and `==` from [@P1185R2]), this paper proposes that both
the pack alias and the pack operator be defaultable.

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        using ... = default; // same as Ts
        
        // same as elems for the first two and move(*this).elems for the next two
        constexpr operator Ts&       ...() &       = default;
        constexpr operator Ts const& ...() const&  = default;
        constexpr operator Ts&&      ...() &&      = default;
        constexpr operator Ts const&&...() const&& = default;
        
    private:
        Ts... elems;
    };
}
```

But this situation is a little different. For comparisons, it really only makes
sense to compare constant objects. But for unpacking, we do want to unpack
const and non-const, lvalue and rvalue, differently. This paper suggests that
defaulting can thus drop the type as well, all of it will be inferred:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        // same as above
        using ... = default;
        constexpr operator ...() = default;
        
    private:
        Ts... elems;
    };
}
```

This paper suggests that this kind of default pack operator behave much like
structured bindings do today: that these are are not references, just aliases,
so that they can work with bitfields.

## Structured bindings for pack-expandable types

Structured bindings [@P0144R2] were a great usability feature introduced in
C++17, but it's quite cumbersome to opt-in to the customization mechanism: you
need to specialize `std::tuple_size`, `std::tuple_element`, and provide a `get()`
of some sort. There was a proposal to reduce the customization mechanism by
dropping `std::tuple_element` [@P1096R0], which was... close. 13-7 in San Diego.

But the mechanisms presented in this paper provide a better customization point
for structured bindings: `operator...`! This is a single function that the language
can examine to determine the arity, the types, and the values. All without even
having to include `<tuple>`:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
#ifdef ADD_ALIAS
        using ... = Ts;
#endif
        operator Ts& ...() & { return elems; }
    private:
        Ts... elems;
    };
}

int n = 0;

xstd::tuple<int, int&> tref{n, n};
auto& [i, iref] = tref;
```

This paper proposes that the above is well-formed, with or without `ADD_ALIAS`
defined. And either way, `decltype(i)` is `int`. If there is no pack alias
declared, then the type will be determined from the `operator...` result (similar
to what [@P1096R0] proposed). If there is a pack alias declared, then the type will
be determined from that pack alias. That is, `decltype(iref)` is `int&` if
`ADD_ALIAS` is defined and `int` otherwise. 

## Language arrays and types with all-public members

Structured bindings works by default with language arrays and types with all-
public members. This paper proposes that such types also have an
implicitly-defaulted pack alias and pack operator. This allows for the same
kind seamless unpacking this paper demonstrates for `xstd::tuple`:

```cpp
void bar(int, int);

int values[] = {42, 17};
bar(values...); // equivalent to bar(42, 17)
```

But also provides a direct solution to the fixed-size pack problem [@N4072]:

```cpp
template <typename T, int N>
class Vector {
public:
    // I want this to be constructible from exactly N T's. The type T[N]
    // expands directly into that
    Vector(T[N].[:]... vals);
    
    // ... which possibly reads better if you take an alias first
    using D = T[N];
    Vector(D.[:]... vals);
};
```

Note that this behaves differently from the homogenous variadic function packs
paper [@P1219R1]:

```cpp
template <typename T, int N>
class Vector {
public:
    // independently deduces each ts and requires that
    // they all deduce to T. As opposed to the previous
    // implementation which behaves as if the constructor
    // were not a template, just that it took N T's.
    Vector(T... ts) requires (sizeof...(Ts) == N);
};
```

## Generalized Slicing and a simplified Boost.Mp11

This paper proposes `T.[:]` to be a sigil to identify a pack. This isn't just
necessary for being able to unpack things in dependent contexts. It also allows
for more fine-grained control over which part of the pack is referenced. 

Similar to Python's syntax for slicing, this paper proposes to provide indexes
on one side or the other of the `:` to take just parts of the pack. For instance,
`T.[1:]` is all but the first element of the pack. `T.[:-1]` is all but the
last element of the pack. `T.[2:3]` is a pack consisting only of the third element.

Such a feature would provide an easy way to write a `std::visit` that takes
the variants first and the function last:

```cpp
template <typename... Args>
constexpr decltype(auto) better_visit(Args&&... args) {
    return std::visit(
        // the function first
        std::forward<Args.[-1]>(args.[-1]),
        // all the variants next
        // note that both slices on both Args and args are necessary, otherwise
        // we end up with two packs of different sizes that need to get expanded
        std::forward<Args.[:-1]>(args.[:-1])...);
}
```

Boost.Mp11 works by treating any variadic class template as a type list and
providing operations that just work. A common pattern in the implementation
of many of the metafunctions is to indirect to a class template specialization
to do the pattern matching on the pack. This paper provides a more direct
way to implement many of the facilities.

We just need one helper that we will reuse every time (as opposed to each
metafunction needing its own helper):

```cpp
template <class L> struct pack_impl;
template <template <class...> class L, class... Ts>
struct pack_impl<L<Ts...>> {
    // a pack alias for the template arguments
    using ... = Ts;
    
    // an alias template for the class template itself
    template <typename... Us> using apply = L<Us...>;
};

template <class L, class... Us>
using apply_pack_impl = typename pack_impl<L>::template apply<Us...>;
```

::: tonytable

### Boost.Mp11 
```cpp
template <class L> struct mp_front_impl;
template <template <class...> class L, class T, class... Ts>
struct mp_front_impl<L<T, Ts...>> {
    using type = T;
};

template <class L>
using mp_front = typename mp_front_impl<L>::type;
```

### This proposal
```cpp
template <class L>
using mp_front = pack_impl<L>.[0];
```

---

```cpp
template <class L> struct mp_pop_front_impl;
template <template <class...> class L, class T, class... Ts>
struct mp_pop_front_impl<L<T, Ts...>> {
    using type = T;
};

template <class L>
using mp_pop_front = typename mp_pop_front_impl<L>::type;
```

```cpp
template <class L>
using mp_pop_front = apply_pack_impl<
    L, pack_impl<L>.[1:]...>;
```

---

```cpp
// you get the idea
template <class L>
using mp_second = /* ... */;

template <class L>
using mp_third = /* ... */;

template <class L, class... Ts>
using mp_push_front = /* ... */;

template <class L, class... Ts>
using mp_push_back = /* ... */;

template <class L, class T>
using mp_replace_front = /* ... */;

// ...
```

```cpp
template <class L>
using mp_second = pack_impl<L>.[1];

template <class L>
using mp_third = pack_impl<L>.[2];

template <class L, class... Ts>
using mp_push_front = apply_pack_impl<
    L, Ts..., pack_impl<L>.[:]...>;

template <class L, class... Ts>
using mp_push_back = apply_pack_impl<
    L, pack_impl<L>.[:]..., Ts...>;

template <class L, class T>
using mp_replace_front = apply_pack_impl<
    L, T, pack_impl<L>.[1:]...>;
```
:::

## What about Reflection?

Two recent reflection papers ([@P1240R0] and [@P1717R0]) provide solutions for
some of the problems this paper is attempting to solve. What follows is my best
attempt to compare the reflection solutions to the generalized pack solutions
presented here. I am not entirely sure about the examples on the left, but
hopefully they are at least close enough to correct to be able to evaluate the
differences.

::: tonytable
### Reflection
```cpp
// member pack declaration (P1717)
template <typename... Types>
class tuple {
  consteval {
    int counter = 0;
    for... (meta::info type : reflexpr(Types)) {
      auto fragment = __fragment struct {
        typename(type) unqualid("element_", counter);
      };
      -> fragment;
      ++counter;
    }
  }
};
```

### This proposal
```cpp
template <typename... Types>
class tuple {
  Types... element;
};
```

---

```cpp
// pack indexing (P1240)
template <size_t I, typename... Ts>
using at = typename(std::vector{reflexpr(Ts)...}[I]);
```

```cpp
template <size_t I, typename... Ts>
using at = Ts.[0];
```

---

```cpp
// generalized pack indexing (P1240)
template <typename... Types>
struct tuple {
  consteval static auto types() {
    return std::vector{reflexpr(Types)...};
  }
};

template <size_t I, typename T>
using tuple_element_t = typename(T::types()[I]);
```

```cpp
template <typename... Types>
struct tuple {
    using ... = Types;
};

template <size_t I, typename T>
using tuple_element_t = T.[I];
```

---

```cpp
template <typename... Types>
struct tuple {
  consteval auto members() const {
    // return some range of vector<meta::info>
    // here that represents the data members.
    // I am not sure how to implement that
  }
};

template <typename Tuple>
void call_f(Tuple const& t) {
  f(t.unreflexpr(t.members())...);
}
```

```cpp
template <typename... Types>
struct tuple {
  operator Types const&...() const& { return elems; }
  // or:
  operator ...() = default;
  
  Types... elems;
};

template <typename Tuple>
void call_f(Tuple const& t) {
  return f(t.[:]...);
}
```

:::

It's not that I think that the reflection direction is bad, or isn't useful. Far
from. Indeed, this paper will build on it shortly. It's just that dealing with
`tuple` is, in no small part, and ergonomics problem and I don't think any
reflection proposal that I've seen so far can adequately address that. This is
fine - sometimes we need a specific language feature for a specific use-case.

# The Pair Example and others

As shocking as it might be to hear, there are in fact other types in the standard
library that are not `std::tuple<Ts...>`. We should probably consider how to fit
those other types into this new world.

## `std::pair`

It might seem strange, in a paper proposing language features to make it easier
to manipulate packs, to start with `tuple` (the quintessential pack example) and
then transition to `pair` (a type that has no packs). But `pair` is in many
ways just another `tuple`, so it should be usable in the same ways. If we will
be able to inline unpack a tuple and call a function with its arguments, it would
be somewhat jarring if we couldn't do the same thing with a `pair`. So how do we?

In the previous section, this paper laid out proposals to declare a variable pack,
to provide for an alias pack and `operator...`, to index into each, and connect
all of this into structured bindings. How would this work if we do _not_ have
a pack anywhere?

```cpp
namespace xstd {
    template <typename T, typename U>
    struct pair {
        T first;
        U second;
        
        using ... = ???;
        operator ??? ... () { return ???; }
    };
}
```

The direction this paper proposes is a recursive one. I'm taking two ideas that
are already present in the language and merging them:

- `operator->()` recurses down until it finds a pointer
- Expansion statements [@P1306R1] can take either an unexpanded pack, a type
that adheres to the structured bindings protocol, or a constexpr range .

In the same vein, this paper proposes that both the pack operator and pack
aliases can be defined in terms of an unexpanded pack or a type that defines
one of these aliases:


```cpp
namespace xstd {
    template <typename T, typename U>
    struct pair {
        T first;
        U second;
        
        using ... = tuple<T, U>;
        operator tuple<T&, U&>... () & { return {first, second}; }
    };
}
```

With the definition of `xstd::tuple` presented in this paper, this is now a light
type to instantiate (or at least, as light as possible), so the extra overhead
might not be a concern.

In the following example:

```cpp
void f(int&, char&);
xstd::pair<int, char> p{1, 'x'};
f(p...);
```

`p...` will invoke `p.operator...()`, which gives a `tuple<int&, char&>`.  That
is not a pack, so we invoke its `operator...()`, which gives us a pack of `int&`
and `char&`. 

A different, non-recursive approach would be to use the reflection facilities
introduced in [@P1240R0] and allow the returning of a consteval range:

```cpp
namepsace xstd {
    template <typename T, typename U>
    struct pair {
        T first;
        U second;
        
        using ... = typename(std::vector{reflexpr(T), reflexpr(U)});
        
        consteval operator auto...() const {
            return std::vector{
                reflexpr(first),
                reflexpr(second)
            };
        }
}
```

In the above example, `f(p...)` would evaluate as 
`f(p.unreflexpr(p.operator...())...)`

Of course, for this particular example, both the pack alias and operator could be
defaulted.

## `std::integer_sequence` and Ranges

There is a paper in the pre-Cologne mailing specifially wanting to opt
`std::integer_sequence` into expansion statements [@P1789R0]. We could instead
be able to opt it into the new pack protocol:

```cpp
template <class T, T... Ints>
struct integer_sequence {
    operator std::integral_constant<T, Ints> ...() const {
        return {};
    }
};
```

One of the cool things we will be able to do in C++20 is implement tuple swap
like so [@Stone.Swap]:

```cpp
template <class... TYPES>
constexpr
void tuple<TYPES...>::swap(tuple& other)
   noexcept((is_nothrow_swappable_v<TYPES> and ...))
{
   for...(constexpr size_t N : view::iota(0u, sizeof...(TYPES))) {
      swap(get<N>(*this), get<N>(other));
   }
}
```

But what if I wanted to unpack a range into a function? It's doesn't seem so far 
fetched that if you can use an expansion statement over a range (which requires
a known fixed size) that you should be able to use other language constructs that
also require a known fixed size: structured bindings and tuple unpacking:


```cpp
void foo(int...);
auto [a, b, c] = view::iota(0, 3); // maybe this should work
foo(view::iota(0, 3)...);          // ... and this too

void call_f(Range auto&& r) {
    f(r.[:]...);                   // .. and this also
}
```

# Proposal

All the separate bits and pieces of this proposal have been presented one step
at a time during the course of this paper. This section will formalize all
the important notions. 

## Pack declarations

You can declare member variable packs, namespace-scope variable packs, and
block-scope variable packs. You can declare alias packs.

These can be directly unpacked when in non-dependent contexts. 

You can declare packs within structured binding declarations (this paper may
we well just subsume [@P1061R0]). 

## Pack-like type

To start with, structured bindings today works on three kinds of types:

1. Arrays
2. Tuple-Like types -- defined as types that satisfy `std::tuple_size<E>`,
`std::tuple_element<i, E>`, and `get<i>()`.
3. Types that have all of their non-static data members in the same class. This
one is a little fuzzy because it's based on accessibility.

This paper proposes the notion of a _pack-like type_. A pack-like type:

1. Is an unexpanded pack
2. Is an array type
3. Has one of:
    a. an unnamed pack alias type that names a pack-like type
    b. a pack operator that returns a pack-like type
    
   If any of these new special named members yields a reflection range, that range
will be reified as appropriate before further consideration. If a type provides
only a pack alias, it can be indexed into/unpacked as a type but not as a value.
 If a type provides only a pack operator, it can be indexed/unpacked as a value
but not as a type.
4. Is a Tuple-Like type (as per structured bindings)
5. Is a constexpr range.
6. Is a types that has all of its non-static data members in the same class (as
per structured bindings).

This paper proposes to redefine both structured bindings and expansion statements
in terms of the pack-like type concept, unifying the two ideas. Any pack-like
type can be expanded over or used as the right-hand side of a structured binding
declaration.

Any pack-like type can be indexed into, `T.[i]` will yield the `i`th type (if `T`
is a type) or `i`th value (if `T` is a variable) of the type. Any pack like type
can be sliced and unpacked via `T.[:]...` or with specific indices. 

This also unifies the special unpacking rules in [@P1240R0]:
a reflection range is a pack-like type, therefore it can be unpacked.

## Dependent packs

Member packs, block scope packs, and pack-like types can be directly unpacked
when in non-dependent contexts. We know what they are.

In dependent contexts, anything that is not a pack must be explicitly identified
as a pack in order to be treated as one. Similar to how we need the `typename`
and `template` keyword in many places to identify that such and such an expression
is a type or a template, the `.[]` sequence will identify the expression that
precedes it as a pack-like type. If that entity is _not_ a pack-like type, then
the indexing or unpacking expression is ill-formed.

---
references:
  - id: Boost.Mp11
    citation-label: Boost.Mp11
    title: "Boost.Mp11: A C++11 metaprogramming library - 1.70.0"
    author:
      - family: Peter Dimov
    issued:
      - year: 2017
    URL: https://www.boost.org/doc/libs/1_70_0/libs/mp11/doc/html/mp11.html
  - id: Smith.Pack
    citation-label: Smith.Pack
    title: "A problem with generalized lambda captures and pack expansion"
    author:
      - family: Richard Smith
    issued:
      - year: 2013
    URL: https://groups.google.com/a/isocpp.org/d/msg/std-discussion/ePRzn4K7VcM/Cvy8M8EL3YAJ
  - id: Calabrese.Argot
    citation-label: Calabrese.Argot
    title: "C++Now 2018: Argot: Simplifying Variants, Tuples, and Futures"
    author:
      - family: Matt Calabrese
    issued:
      - year: 2018
    URL: https://www.youtube.com/watch?v=pKVCB_Bzalk
  - id: Stone.Swap
    citation-label: Stone.Swap
    title: "Library Support for Expansion Statements: P1789"
    author:
      - family: David Stone
    issued:
      - year: 2019
    URL: http://lists.isocpp.org/lib-ext/2019/06/11932.php
---