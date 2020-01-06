---
title: Generalized pack declaration and usage
document: D1858R1
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

R0 [@P1858R0] was presented in EWGI in Belfast [@EWGI.Belfast], where further
work was encouraged (9-4-1-0-0). Since then, several substantial changes have
been made to this paper.

- The overloadable `operator...()` and `using ...` were removed. Pack expansion
is now driven through structured bindings, which are extended on the library
side, rather than introducing an extra language feature.
- Introduction of pack literals for types and values.
- Discussion of functions returning packs - no longer being proposed due to
ambiguity of what it actually could mean.

# Introduction and Motivation

C++11 introduced variadic templates, one of the truly transformational language
features introduced that standard. Despite pretty tight restrictions on where
packs could be declared and how they could be used, this feature has proven
incredibly successful. Three standards later, there hasn't even been much change.
C++17 added a couple new ways to use packs (fold expressions [@N4191] and
using-declarations [@P0195R2]), and C++20 will add a new way to introduce them
(in lambda capture [@P0780R2]). A proposal to iterate over them (expansion statements 
[@P1306R1]) didn't quite make it. That's it.

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

Or, in C++20:

```cpp
xstd::tuple y{1, 2, 3};
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
        constexpr tuple() requires (std::default_constructible<Ts> && ...)
            : elems()...
        { }
        
        constexpr tuple(Ts const&... args)
                requires (std::copy_constructible<Ts> && ...)
            : elems(args)...
        { }

    private:
        Ts... elems;
    };
}
```

Note the new pack expansion in the _mem-initializer_. This is a new ability this
paper is proposing. It wouldn't have made sense to have 
if you could not declare a member
variable pack.

Let's pick a more complex constructor. A `std::tuple<Ts...>` can be constructed
from a `std::pair<T, U>` if `sizeof...(Ts) == 2` and the two corresponding types
are convertible. How would we implement that? To do that check, we need to get
the corresponding types. How do we get the first and second types from `Ts`?

## Pack Indexing

This paper proposes a "simple selection" facility similar to the one initially
introduced in
[@N4235] (and favorably received in Urbana 2014): `T...[I]` is the `I`th element
of the pack `T`, which is a type or value or template based on what kind of pack
`T` is.
[Later sections](#disambiguating-packs)
 of this paper will discuss why this paper diverges from that
original proposal in choice of syntax.

Such indexing allows for implementing the pair converting constructor:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        template <std::convertible_to<Ts...[0]> T,
                  std::convertible_to<Ts...[1]> U>
            requires sizeof...(Ts) == 2
        constexpr tuple(std::pair<T, U> const& p)
            : elems...[0](p.first)
            , elems...[1](p.second)
        { }
    private:
        Ts... elems;
    };
}
```

Notably, in an earlier example we constructed the pack as a single entity and
here we are constructing each element of the pack separately. Both are fine.

A properly constrained converting constructor from a pack:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
    public:
        template <std::constructible<Ts>... Us>   // everything is convertible
            requires (sizeof...(Us) > 1 ||        // exclude the copy ctor match
                    !std::derived_from<std::remove_cvref_t<Us...[0]>, tuple>)
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
namespace std {
    template <size_t I, typename... Ts>
    struct tuple_element<I, xstd::tuple<Ts...>>
        requires (I < sizeof...(Ts))
    {
        using type = Ts...[I];
    };
}
```

This is a lot nicer than status quo.

## Extending Structured Bindings

If we implement `tuple` as:

```cpp
namespace xstd {
    template <typename... Ts>
    struct tuple {
        Ts... elems;
    };
}
```

then structured bindings [@P0144R2] works out of the box. And maybe there's an argument to
be made that with this proposal, `tuple`'s members _should_ be public in the 
same way that `pair`'s are. But how do we opt in to structured bindings if
`elems` were private? We need three things: `tuple_size`, `tuple_element`, and
`get`. `tuple_element` was just presented, `tuple_size` is straightforward,
and we could add `get` as a member:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
        Ts... elems;
    public:
        template <size_t I> requires I < sizeof...(Ts)
        auto get() & -> Ts...[I]& { return elems...[I]; }
        
        // + overloads for const&, &&, const&&
    };
}
```

From here on out, I will only provide the `const&` overload of `get()` in all
examples.

## Pack aliases and a smaller opt-in to structured bindings

Currently, there are three kinds of types that can be used with structured
bindings:

1. Arrays (specifically `T[N]` and not `std::array<T, N>`). Note that here we
have `arr[i]` and `arr.[i]` mean the same thing, except that the latter requires
`i` to be a constant expression.

2. Tuple-like: those tupes that specialize `std::tuple_size`, `std::tuple_element`,
and either provide a member or non-member `get()`. This paper already suggests
extending the two type traits to look for member pack alias named `tuple_element`.

3. Types where all of there members are public members of the same class
(approximately).

The problem with the tuple-like protocol here is that we need to instantiate
a lot of templates. Assuming [@P1061R1] gets approved, a declaration like:

```cpp
auto [...elems] = tuple;
```

requires `2N+1` template instantiations: one for `std::tuple_size`, `N` for
`std::tuple_element`, and another `N` for all the `get`s). That seems wasteful, especially when the
structured binding rules already tie into these library type traits.

There was a proposal to reduce the customization mechanism by
dropping `std::tuple_element` [@P1096R0], which was... close. 13-7 in San Diego.

This proposal reduces the customization surface in a different way
that does not even require including `<tuple>` for user-defined types.
In the same way that this paper proposes defining member variable packs, it also
allows member type alias packs. We can have our `tuple` declare a pack of its
element types:

```cpp
namespace xstd {
    template <typename... Ts>
    struct tuple {
        using ...tuple_element = Ts;
    };
}
```

The advantage here is that the `tuple_element` pack provides both the size
_and_ all the types. We can change add specializations to `std::tuple_size` and
`std::tuple_element` to understand this:

```cpp
template <typename T>
    requires (sizeof...(T::...tuple_element))
struct tuple_size<T>
    : integral_constant<size_t, sizeof...(T::...tuple_element)>
{ };

template <size_t I, typename T>
    requires (sizeof...(T::...tuple_element))
struct tuple_element<I, T>
{
    using type = T::...tuple_element...[I];
};
```

The extra preceding `...`s in the above code block are not typos, and will be
explained in a [later section](#disambiguating-dependent).

At this point, we've reduced the structured bindings protocol surface (`xstd::tuple`
did not have to specialize anything), but we still require all those extra
instantiations. A better way would be to have the language rules for structured
bindings themselves look directly for a member pack alias `tuple_element`.
That is, a type now is tuple-like if it _either_:

a. Has member pack alias `tuple_element`, which case its size is the size of that
pack and the element types are the constituents of that pack.
b. Has specialized `std::tuple_size` and `std::tuple_element`, as status quo.

This library API change would allow for a complete structured bindings opt-in
for our new `tuple` to be:

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
        Ts... elems;
    public:
        using ...tuple_element = Ts;
    
        template <size_t I> requires I < sizeof...(Ts)
        auto get() const& -> Ts...[I] const& {
            return elems...[I];
        }
    };
}
```

Which now requires `N+1` fewer template instantiations to unpack.


# Expanding a type into a pack

At this point, this paper has introduced:

* the ability to declare a member variable pack and a member alias pack
* the ability to index into a pack
* an extension of structured bindings to look for a specific member alias pack
named `tuple_element`

This paper defines the syntax `T...[I]` to be indexing into a pack, `T`.
But there's a lot of similarity between a pack of types and a tuple - which is
a pack of types in single type form. And while we can index into a tuple (that's
what `tuple_element` does), the syntax difference between the two is rather large:

<table>
<tr><th></th><th>Pack</th><th>Tuple</th></tr>
<tr><td style="vertical-align:middle">
Types
</td><td>
```cpp
Types...[I]
```
</td>
<td>
```cpp
tuple_element_t<I, Tuple>
```
</td>
</tr>
<tr><td style="vertical-align:middle">Values</td><td>
```cpp
vals...[I]
```
</td>
<td>
```cpp
std::get<I>(tuple)
```
</td>
</tr>
</table>

The problem is that we're used to having the thing we're indexing on the left
and the index on the right, and in the tuple case - this is inverted. If we
could turn the `tuple` into a pack of its constituents, we could use the pack
indexing operation. [@P1061R1], coupled with this paper thus far, would allow:

```cpp
// structured bindings can now introduce a pack, so this is a
// tuple --> pack conversion
auto [...elems] = tuple;

// pack indexing as usual
auto first = elems...[0];
```

This paper proposes a more direct mechanism to accomplish this.

## Generalized unpacking with `[:]`

This paper proposes a non-overloadable `[:]` operator which can be
thought of as the "add a layer of packness" operator. The semantics of this
operator are to unpack an object into the elements that would make up its
respective structured binding declaration. For a `std::tuple`, this would mean 
calls to `std::get<0>`, `std::get<1>`, ... For an aggregate, this would mean
aliases into its members. For the `xstd::tuple` we defined earlier, this would
be calls into the members `get<0>()`, `get<1>(), ...`. For arrays, this would
be the elements of the array.

Once we add a layer of
packness, the entity becomes a pack - and so all the usual
rules around interacting with packs apply. 


<table>
<tr><th></th><th>Pack</th><th>Tuple</th></tr>
<tr><td style="vertical-align:middle">
Types
</td><td>
```cpp
Types...[I]
```
</td>
<td>
```cpp
Tuple::[:]...[I]
```
</td>
</tr>
<tr><td style="vertical-align:middle">Values</td><td>
```cpp
vals...[I]
```
</td>
<td>
```cpp
tuple.[:]...[I]
```
</td>
</tr>
</table>

That's a fairly involved syntax for a simple and common operation, so this
paper proposes the shorthand `Tuple::[I]` for that syntax. This is not a typo.
The syntax for indexing into a pack (`Ts...[I]`) and the syntax for indexing
into a type (`Ts::[I]`) are different and it's important that they are different.
A proper discussion of the motivation for the differing syntax will
[follow](#disambiguating-packs), but the key is that these syntaxes be unambiguous.

<table>
<tr><th></th><th>Pack</th><th>Tuple</th><th>Tuple, sugared</th></tr>
<tr><td style="vertical-align:middle">
Types
</td><td>
```cpp
Types...[I]
```
</td>
<td>
```cpp
Tuple::[:]...[I]
```
</td>
<td>
```cpp
Tuple::[I]
```
</td>
</tr>
<tr><td style="vertical-align:middle">Values</td><td>
```cpp
vals...[I]
```
</td>
<td>
```cpp
tuple.[:]...[I]
```
</td>
<td>
```cpp
tuple.[I]
```
</td>
</tr>
</table>

This means we can take our tuple implementation from the previous section (with
an added constructor):

```cpp
namespace xstd {
    template <typename... Ts>
    class tuple {
        Ts... elems;
    public:
        tuple(Ts... ts) : elems(ts)... { }
    
        using ...tuple_element = Ts;
    
        template <size_t I> requires I < sizeof...(Ts)
        auto get() const& -> Ts...[I]& { return elems...[I]; }
    };
}
```

And that is already sufficient to write:

```cpp
xstd::tuple vals(1, 2, 3);
// direct indexing
assert(vals.[0] + vals.[1] + vals.[2] == 6);

// direct unpacking
auto sum = (vals.[:] + ...);
assert(sum == 6);
```

Here `vals.[0]` is the first structured binding, which for a tuple-like type
means `vals.get<0>()`. `vals.[:]` is the pack consisting of all of the
structured bindings, which means the pack `{vals.get<0>(), vals.get<1>(), vals.get<2>()}`.

## Syntax-free unpacking?

The last example demonstrates the ability to turn the tuple `vals` into a pack
by way of `vals.[:]`, at which point we can do any of the nice pack things with
that entity (e.g. use a _fold-expression_, invoke a function, etc). Why do we
need that though? Can't we just have:

```cpp
auto sum = (vals + ...);
```

The problem is this direction would lead to ambiguities (as almost everything
in this space inherently does):

```cpp
template <typename T, typename... U>
void call_f(T t, U... us)
{
    // Is this intended to add 't' to each element in 'us'
    // or is intended to pairwise sum the tuple 't' and
    // the pack 'us'?
    f((t + us)...);
}
```

Both meanings are reasonable - so it's left up to the programmer to express
their intent:

```cpp
// This adds 't' to each element in 'us'
f((t + us)...);

// This adds each element in 't' to each element in 'us',
// if the two packs have different sizes, ill-formed
f((t.[:] + us)...);
```

## Other examples of unpacking

Arrays are the easiest kind of structured binding. With `[:]`, they would unpack
into all of their elements - which have the same type. Note that `arr[i]` and
`arr.[i]` mean the same thing, except that in the latter case the index must
be a constant expression.

```cpp
void bar(int, int);

int values[] = {42, 17};
bar(values.[:]...); // equivalent to bar(42, 17)
```

This directly allows unpacking types with all-public members as well:

```cpp
struct X { int i, j; };
struct Y { int k, m; };

int sum(X x, Y y) {
    // equivalent to: return x.i + x.j + y.k * y.k + y.m * y.m
    return (x.[:] + ...) + ((y.[:] * y.[:]) + ...);
}
```

And because for such types, structured binding calls the bindings themselves
aliases rather than new variables, this works with bitfields:

```cpp
struct B {
    uint8_t i : 4;
    uint8_t j: 4;
};
B b{.i = 2, .j = 4};
assert(b.[0] + b.[1] == 6);
```

This also provides a direct solution to the fixed-size pack problem [@N4072]:

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
class Vector2 {
public:
    // independently deduces each ts and requires that
    // they all deduce to T. As opposed to the previous
    // implementation which behaves as if the constructor
    // were not a template, just that it took N T's.
    Vector2(T... ts) requires (sizeof...(ts) == N);
};
```

For instance:

```cpp
Vector<int, 2>  x('a', 2); // ok
Vector2<int, 2> y('a', 2); // ill-formed, deduction failure
```

## Generalizing slicing further

The reason the syntax `[:]` is chosen as the "add a layer of packness" operator
is because it allows a further generlization. Similar to Python's syntax for
slicing, this paper proposes to provide indexes on one side or the other of
the `:` to take just sections of the pack. 

For instance, `T::[1:]` is all but the first element of the type. `T::[:-1]`
is all but the last element of the type. `T::[2:3]` is a pack consisting of
only the third element.

Such a feature would provide an easy way to write a `std::visit` that takes
the variants first and the function last:

```cpp
template <typename... Args>
constexpr decltype(auto) better_visit(Args&&... args) {
    return std::visit(
        // the function first
        std::forward<Args...[-1]>(args...[-1]),
        // all the variants next
        // note that both slices on both Args and args are necessary, otherwise
        // we end up with two packs of different sizes that need to get expanded
        std::forward<Args...[:-1]>(args...[:-1])...);
}
```

It would also allow for a single-overload variadic fold:

```cpp
template <typename F, typename Z, typename... Ts>
constexpr Z fold(F f, Z z, Ts... rest)
{
    if constexpr (sizeof...(rest) == 0) {
        return z;
    } else {
        // we need to invoke f on z and the first elem in rest...
        // and recurse, passing the rest of rest...
        return fold(f,
            f(z, rest...[0]),
            rest...[1:]...);

        // alternate formulation
        auto head = rest...[0];
        auto ...tail = rest...[1:];
        return fold(f, f(z, head), tail...);
    }
}
```

## Examples with Boost.Mp11

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
    using ...tuple_element = Ts;
    
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
using mp_front = pack_impl<L>::[0];
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
    L, pack_impl<L>::[1:]...>;
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
using mp_second = pack_impl<L>::[1];

template <class L>
using mp_third = pack_impl<L>::[2];

template <class L, class... Ts>
using mp_push_front = apply_pack_impl<
    L, Ts..., pack_impl<L>::[:]...>;

template <class L, class... Ts>
using mp_push_back = apply_pack_impl<
    L, pack_impl<L>::[:]..., Ts...>;

template <class L, class T>
using mp_replace_front = apply_pack_impl<
    L, T, pack_impl<L>::[1:]...>;
```
:::

# Can functions return a pack?

The previous draft of this paper [@P1858R0] also proposed the ability to have
a function return a pack. This revision removes that part of the proposal. 
Consider:

```cpp
void observe();
void consume(auto...);

template <typename... Ts>
struct copy_tuple {
    Ts... elems;
    
    Ts... get() const {
        observe();
        return elems;
    }
};

copy_tuple<X, Y, Z> tuple(x, y, z);

// #1
consume(tuple.get()...);

// #2
consume(tuple.get()...[0]);

// #3
consume(tuple.get()...[0], tuple.get()...[1]);
```

There are two questions we have to answer for each call to `consume` there:

1. How many times is `observe()` invoked?
2. How many times are the `X`, `Y`, and `Z` copied?

There are two models we can think about for how the `get()` function might work:

### Language Tuple {-}

`get()` behaves as if it returns a language tuple. That is, it's syntax sugar
for:

```cpp
tuple<Ts...> get() const {
    observe();
    return {elems...};
}
```

Except that it's already a pack. With this model, every call to `get()` calls
`observe()` a single time and copies every element.

### Pack of functions {-}

`get()` behaves as if it declares a pack of functions. That is, it's syntax sugar
for:

```cpp
Ts...[0] get<0>() const {
    observe();
    return elems...[0];
}

Ts...[1] get<1>() const {
    observe();
    return elems...[1];
}

// etc.
```

With this model, each individual element access incurs a call to `observe()`,
but only the desired element is copied.

### Which is better? {-}

The problem is, both models are reasonable mental models and yet both models
lead to some jarringly strange results:

- If we consider the language tuple model, then the example `#3` -
`consume(tuple.get()...[0], tuple.get()...[1])` - would end up copying every
element in the tuple, twice. Six copies, to get two elements?

- If we consider the pack of functions mode, then the example `#1` - 
`consume(tuple.get()...)` - would end up calling `observe()` three times. But
it only looks like we're invoking a single function?

I don't think either model is better than the other, so this paper punts on
this problem entirely. There is no longer a proposal to allow functions returning
packs.

# Disambiguation

There are many aspects of this proposal that require careful disambiguation.
This section goes through those cases in turn. 

## Disambiguating Dependent Packs {#disambiguating-dependent}

If we have a dependnet name that's a pack, we need a way to disambiguate that
it's actually a pack. From [@Smith.Pack]:

::: quote
```cpp
template<typename T> void call_f(T t) {
  f(t.x ...)
}
```
Right now, this is ill-formed (no diagnostic required) because "t.x" does not 
contain an unexpanded parameter pack. But if we allow class members to be pack 
expansions, this code could be valid -- we'd lose any syntactic mechanism to 
determine whether an expression contains an unexpanded pack. This is fatal to at 
least one implementation strategy for variadic templates.
:::

We need some kind of disambiguation. Perhaps some sort of
context-sensitive keyword like `pack`?

```cpp
template <typename T>
    requires (sizeof...(T::pack tuple_element))
//                      ~~~^^^^^~~~~~~~~~~~~~
struct tuple_size<T>
    : integral_constant<size_t, sizeof...(T::pack tuple_element)>
    //                                    ~~~^^^^^~~~~~~~~~~~~~
{ };
```

Unfortunately that doesn't work because of the possibility of something
like:

```cpp
template<typename ...Ts> struct X { using ...types = Ts; };
template<typename MyX> void f() {
  using Fn = void(typename MyX::pack types ...);
}
```

Today that's declaring a function type that takes one argument of type
`typename MyX::pack` named `types` and then varargs with the comma elided. If
we make the comma mandatory (as [@P1219R1] proposes to do), then that would open
up our ability to use a context-sensitive `pack` here. 

Otherwise, we would need a new keyword to make this happen, and `pack` seems entirely too
pretty to make this work. One thing we could consider is <code>[packname]{.kw}</code> - which
has the pleasant feature of having the same number of letters in it as both
`template` and `typename`. But this paper suggests going a different direction
instead.

### Pack introducers {#introducers}

As a brief aside, it's worth enumerating the places in the language where we
can introduce a pack today - because they all have an important feature in common:

```cpp
template <typename ...T> // template parameter pack
void f(T ...t)           // function parameter pack
{
   [...u=t]{};           // init-capture pack
}
```

What these have in common is that is that the `...` always _precedes_ the name
that it introduces as a pack. 

### Proposed disambigutation

To that end, an appropriate and consistent mechanism to disambiguate a dependent
member pack might also be to use _preceding_ ellipses (which still need to be
separated by a space). That is:

```cpp
template <typename T>
    requires (sizeof...(T::...tuple_element))
struct tuple_size<T>
    : integral_constant<size_t, sizeof...(T::...tuple_element)>
{ };
```

And once we disambiguated that `T::tuple_element` is a pack, we still need
to expand it - which may also require trailing ellipses:

```
template <size_t I, typename T>
    requires (sizeof...(T::...tuple_element))
struct tuple_element<I, T>
{
    using type = T::...tuple_element...[I];
};
```

That's admittedly a lot of dots fairly close together. If that isn't acceptable,
then we can introduce a new disambiguating keyword:

```
template <size_t I, typename T>
    requires (sizeof...(T::packname tuple_element))
struct tuple_element<I, T>
{
    using type = T::packname tuple_element...[I];
};
```

## Disambiguating packs of tuples {#disambiguating-packs}

The previous section showed how to write `apply` taking a single function and a
single tuple. What if we generalized it to taking multiple tuples? How do we
handle a pack of tuples?

It's at this point that it's worth taking a step back and talking about
disambiguation and why this paper makes the syntax choices that it makes. We need
to be able to differentiate between packs and tuples. The two concepts are very
similar, and this paper seeks to make them much more similar, but we still need
to differentiate between them. It's the pack of tuples case that really brings
the ambiguity to light. 

The rules this paper proposes, which have all been introduced at this point, 
are:

- `e.[:]` takes a [_pack-like type_](#pack-like-type)
(or object of such) and adds a layer of packness
to it, by way of either `operator...()` or `using ...`. It never is applied to
an existing pack, and it is never used to disambiguate dependent member access.

- `e.[I]` never removes a layer of packness. It is picking the `I`th element of
a pack-like type.

- `e...[I]` always removes a layer of packness. It is picking the `I`th element
of a pack.

- `e. ...f` disambiguates dependent member access and identifies `f` as a pack
The space between the `.` and `...` is required. 

That is, `pack...[I]` and `tuple.[I]` are valid, `tuple...[I]` is an error, and
`pack.[I]` would be applying `.[I]` to each element of the pack (and is itself
still an unexpanded pack expression). Rule of thumb: you need `...`s if and only
if you have a pack. 

`e.[I]` is an equivalent shorthand for `e.[:]...[I]`.

This leads to clear meanings of each of the following. If we have a function
template taking an argument `e` which has a member `f`, where the kind of `e`
is specified by the columns of this table and the kind of `f` is specified by
the rows:

<table>
<tr><td /><th>`e` is a Pack</th><th>`e` is a Pack-like type</th><th>`e` is not expanded</th></tr>
<tr>
<th>`f` is a Pack</th>
<td>`foo(e. ...f... ...);`</td>
<td>`foo(e.[:]. ...f... ...);`</td>
<td>`foo(e. ...f...);`</td>
</tr>
<tr>
<th>`f` is a Pack-like type</th>
<td>`foo(e.f.[:]... ...);`</td>
<td>`foo(e.[:].f.[:]... ...);`</td>
<td>`foo(e.f.[:]...);`</td>
</tr>
<tr>
<th>`f` is not expanded</th>
<td>`foo(e.f...);`</td>
<td>`foo(e.[:].f...);`</td>
<td>`foo(e.f);`</td>
</table>

The only two valid cells in that table in C++20 are the bottom-left and bottom-right
ones. Note that every cell has different syntax, by design.

## Nested pack expansions

In order for the above table to work at all, we also need a new kind of pack
expansion. When C++11 introduced pack expansion, the rules were very simple:
The expression in `expr...` must contain at least one unexpanded pack expression
and every unexpanded pack expression must have the same length.

But with the concepts introduced in this proposal, we have the ability to
introduce new things that behave like unexpanded pack expressions within an 
unexpanded pack expression and we need to define rules for that. Consider:

```cpp
template <typename... Ts>
void foo(Ts... e) {
    bar(e.[:]... ...);
}

// what does this do?
foo(xstd::tuple{1}, xstd::tuple{2, 3});
```

Following the rules presented above, `e.[:]` adds a layer of packness to each
element in the pack (which is fine because `xstd::tuple`s are pack-like types
which define an `operator...()`). But what then do the `...`s refer to?

We say that adding layer of packness in the middle of an existing unexpanded
pack expression will hang a new, nested unexpanded pack expression onto that.

In the above example, `e` is an unexpanded pack expression. `e.[:]` is a nested
unexpanded pack expression underneath `e`.

When we encounter the the first `...`, we say that it expands the most nested
unexpanded pack expression that of the expression that it refers to. The most
nested unexpanded pack expression here is `e.[:]`,
which transforms the expression into:

```cpp
bar((e.[0], e.[1], e.[2], /* etc. */, e.[M-1])...);
```

This isn't really valid C++ code (or, worse, it actually is valid but would use
the comma operator rather than having `M` arguments). But the idea is we now have
one more `...` which now has a single unexpanded pack expression to be expanded,
which is the unexpanded pack expression that expands each element in a pack-like
type. 

A different way of looking at is the outer-most `...` expands the outer-most
unexpanded pack expression, keeping the inner ones in tact. If we only touch
the outer-most `...`, we end up with the following transformation:

```cpp
bar(e@~0~@.[:]..., e@~1~@.[:]..., /* etc. */, e@~N-1~@.[:]...);
```

The two interpretations are isomorphic, though the latter is likely easier to
understand.

Either way, the full answer to what does `foo(xstd::tuple{1}, xstd::tuple{2, 3})`
do in this example is that it calls `bar(1, 2, 3)`.

For more concrete examples, here is a generalized `apply()` which can take
many tuples and expand them in order:

```cpp
template <typename F, typename... Tuples>
constexpr decltype(auto) apply(F&& f, Tuples&&... tuples) {
    return std::invoke(
        std::forward<F>(f),
        std::forward<Tuples>(tuples).[:]... ...);
}
```

which, again more concretely, expands into something like:

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

And itself leads to a different implementation of generalized `apply`:

```cpp
template <typename F, typename... Tuples>
constexpr decltype(auto) apply(F&& f, Tuples&&... tuples) {
    return std::invoke(
        std::forward<F>(f),
        tuple_cat(std::forward<Tuples>(tuples)...).[:]...
}
```

Admittedly, six `.`s is a little cryptic. But is it any worse than the current
implementation?


# Introducing packs in more contexts

[@P0780R2] allowed pack expansion in lambda init-capture:

```cpp
template <typename... T>
void f(T... t)
{
    [...u=t]{};
}
```

If you think of lambda init-capture as basically a variable declaration with
implicit `auto`, then expanding out that capture gets us to a variable
declaration pack:

```cpp
auto ...u = t;
```

This paper proposes actually allowing that declaration form.

That is, a variable pack declaration takes as its initializer an unexpanded pack.
Which could be a normal pack, or it could be a pack-like type with packness added to it:

```cpp
xstd::tuple x{1, 2, 3};

// ill-formed, x is not an unexpanded pack
auto ...bad = x;

// proposed ok
auto ...good = x.[:]; // a pack of {1, 2, 3}

// proposed ok
auto ...doubles = x.[:] * 2; // a pack of {2, 4, 6}
```

The same idea can be used for declaring a pack alias (as was already shown earlier in this paper):

```cpp
template <typename... T>
struct X {
    // proposed ok
    using ...pointers = T*;
};

// ill-formed, not an unexpanded pack
using ...bad = xstd::tuple<int, double>;

// proposed ok
using ...good = xstd::tuple<int, double>::[:]; // a pack of {int, double}
```

## Pack literals with types

As shocking as it might be to hear, there are in fact other types in the standard
library that are not `std::tuple<Ts...>`. We should probably consider how to fit
those other types into this new world.

It might seem strange, in a paper proposing language features to make it easier
to manipulate packs, to start with `tuple` (the quintessential pack example) and
then transition to `pair` (a type that has no packs). But `pair` is in many
ways just another `tuple`, so it should be usable in the same ways. If we will
be able to inline unpack a tuple and call a function with its arguments, it would
be somewhat jarring if we couldn't do the same thing with a `pair`. So how do we?

The previous sections built up how we can implement `tuple` and opt it into
structured bindings with less code and more efficiently. We could do that with
`pair` as well, on top of `tuple`:

```cpp
namespace xstd {
    template <typename T, typename U>
    struct pair {
        T first;
        U second;
        
        using ...tuple_element = tuple<T, U>::[:];
        
        template <size_t I>
        auto get() const& -> tuple_element...[I] const&
        {
            if constexpr (I == 0) return first;
            else if constepxr (I == 1) return second;
        }
    };
}
```

The `get` implementation is a little awkward, but the way we're declaring
`tuple_element` is even more awkward. We have to instantiate a tuple just
to unpack it to get a type? Well, we don't need full blown tuple, we just
need a type list:

```cpp
template <typename... Ts>
struct typelist {
    using ...types = Ts;
};

namespace xstd {
    template <typename T, typename U>
    struct pair {
        using ...tuple_element = typelist<T, U>::...types;
    };
}
```

Is that better? I don't think so. Really, we just want to be able to introduce
a pack on the fly. A pack consisting of specific types.

Following the insight into how [pack introducers](#introducers) are used in
the language, this paper proposes allowing the introduction of a pack of types
as:

```cpp
namespace xstd {
    template <typename T, typename U>
    struct pair {
        using ...tuple_element = ...<T, U>;
    };
}
```

[@P0341R0] used the same syntax. 

Such a pack literal could also be used to provide a default template argument
for a template type parameter pack.

```cpp
template <typename... Ts = ...<int>>
struct foo();

foo(); // calls foo<int>.
```

## Pack literals with values

Where types use `<>`s, it makes sense for values to use `{}`s. That is:

```cpp
int... squares = ...{1, 4, 9};
```

Note that there is no `std::initializer_list<int>` here, the expression on the
right is much closer to a `tuple<int, int, int>` than a `std::initializer_list`.

This does introduce some more added subtletly with initialization:

```cpp
auto    a = {1, 2, 3}; // a is a std::initializer_list<int>
auto... b = {1, 2, 3}; // b is a pack of int's
```

Those two declarations are very different. But also, they look different -
one has `...` and the other does not. One looks like it is declaring an object
and the other looks like it is declaring a pack. This doesn't seem inherently
problematic.


## Implementing variant

While most of this paper has dealt specifically with making a better `tuple`,
the features proposed in this paper would also make it much easier to implement
`variant` as well. One of the difficulties with `variant` implementations is that
you need to have a `union`. With this proposal, we can declare a variant pack
too. 

Here are some parts of a variant implementation, to demonstrate what that might
look like. Still need _some_ metaprogramming facilities, but it's certainly a
a lot easier.

```cpp
template <typename... Ts>
class variant {
    int index_;
    union {
        Ts... alts_;
    };
public:
    constexpr variant() requires std::default_constructible<Ts...[0]>
      : index_(0)
      , alts_...[0]()
    { }

    ~variant() requires (std::is_trivially_destructible<Ts> && ...) = default;
    ~variant() {
        mp_with_index<sizeof...(Ts)>(index_,
            [](auto I){ destroy_at(&alts_...[I]); });
    }
};

template <size_t I, typename T>
struct variant_alternative;

template <size_t I, typename... Ts>
    requires (I < sizeof...(Ts))
struct variant_alternative<I, variant<Ts...>> {
    using type = Ts...[I];
};

template <size_t I, typename... Types>
constexpr variant_alternative_t<I, variant<Types...>>*
get_if(variant<Types...>* v) noexcept {
    if (v.index_ == I) {
        return &v.alts_...[I];
    } else {
        return nullptr;
    }
}
```

Directly indexing into the union variant members makes the implementation much
easier to write and read. Not needing a recursive union template is a nice bonus.

## Enumerating over a pack

As another example, let's say we want to take a parameter pack and print its
contents along with an index. Here are some ways we could do that with this
proposal (assuming expansion statements):

```cpp
// iterate over the indices, and index into the pack
template <typename... Ts>
void enumerate1(Ts... ts)
{
    for ... (constexpr auto I : view::iota(0u, sizeof...(Ts))) {
        cout << I << ' ' << ts...[I] << '\n';
    }
}

// construct a new pack of tuples
template <typename... Ts>
void enumerate2(Ts... ts)
{
    constexpr auto indices = view::iota(0u, sizeof...(Ts));
    auto enumerated = tuple{tuple{indices.[:], ts}...};
    for ... (auto [i, t] : enumerated) {
        cout << i << ' ' << t << '\n';
    }
}
```



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
using at = Ts...[I];
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
        tuple<T&, U&> operator ... () & { return {first, second}; }
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
f(p.[:]..);
```

`p.[:]...` will invoke `p.operator...()`, which gives a `tuple<int&, char&>`. That
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
        
        consteval auto operator...() const {
            return std::vector{
                reflexpr(first),
                reflexpr(second)
            };
        }
}
```

In the above example, `f(p.[:]...)` would evaluate as 
`f(p.unreflexpr(p.operator...())...)`.

It's not clear if this direction will actually work, since you would have to
disambiguate between the case where you want the identifiers and the case where
actually you want the `meta::info` objects themselves. Let's call it an open
question.

Of course, for this particular example, both the pack alias and operator could be
defaulted.

## `std::integer_sequence` and Ranges

There is a paper in the pre-Cologne mailing specifially wanting to opt
`std::integer_sequence` into expansion statements [@P1789R0]. We could instead
be able to opt it into the new pack protocol:

```cpp
template <class T, T... Ints>
struct integer_sequence {
    std::integral_constant<T, Ints> operator ...() const {
        return {};
    }
};
```

One of the things we could have built on top of expansion statements was to
implement tuple swap like so [@Stone.Swap]:

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
auto [a, b, c] = view::iota(0, 3); // maybe this should work
foo(view::iota(0, 3).[:]...);      // ... and this too
```

# Proposal

All the separate bits and pieces of this proposal have been presented one step
at a time during the course of this paper. This section will formalize all
the important notions. 

## Pack declarations

You can declare member variable packs, namespace-scope variable packs, and
block-scope variable packs. You can declare alias packs. The initializer for a
pack declaration has to be either an unexpanded pack or a _braced-init-list_.

These can be directly unpacked when in non-dependent contexts. 

You can declare packs within structured binding declarations (this paper may
we well just subsume [@P1061R0]). 

## Pack-like type

To start with, structured bindings today works on three kinds of types:

1. Arrays
2. Tuple-Like types -- defined as types that satisfy `std::tuple_size<E>`,
`std::tuple_element<i, E>`, and `get<i>()`. Note that this is a language feature
that nevertheless has a library hook.
3. Types that have all of their non-static data members in the same class. This
one is a little fuzzy because it's based on accessibility.

This paper proposes the notion of a _pack-like type_. A pack-like type:

1. Is an array type
2. Has one of:
    a. an unnamed pack alias type that names a pack-like type or a pack
    b. a pack operator that returns a pack-like type or a pack
    
   If any of these new special named members yields a reflection range, that range
will be reified as appropriate before further consideration. If a type provides
only a pack alias, it can be indexed into/unpacked as a type but not as a value.
 If a type provides only a pack operator, it can be indexed/unpacked as a value
but not as a type.
3. Is a Tuple-Like type (as per structured bindings)
4. Is a constexpr range (the constexpr-ness is important because of the fixed
size)
5. Is a type that has all of its non-static data members in the same class (as
per structured bindings)

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

Member packs and block scope packs can be directly unpacked
when in non-dependent contexts. We know what they are.

In dependent contexts, anything that is not a pack must be explicitly identified
as a pack in order to be treated as one. Similar to how we need the `typename`
and `template` keyword in many places to identify that such and such an expression
is a type or a template, a preceding `...` (or whatever alternate spelling)
will identify the expression that
follows it as a pack. If that entity is _not_ a pack, then
the indexing or unpacking expression is ill-formed.

# Acknowledgments

This paper would not exist without many thorough conversations with
Agustín Bergé, Matt Calabrese, and Richard Smith. Thank you.

Thank you to David Stone for pointing out many issues.

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
  - id: Sutton
    citation-label: Sutton
    title: "Meta++: Language Support for Advanced Generative Programming"
    author:
        - family: Andrew Sutton
    issued:
        - year: 2019
    URL: https://youtu.be/kjQXhuPX-Ac?t=389
  - id: EWGI.Belfast
    citation-label: EWGI.Belfast
    title: "EWGI Discussion of P1858R0"
    author:
        - family: EWGI
    issued:
        - year: 2019
    URL: http://wiki.edg.com/bin/view/Wg21belfast/P1858
---
