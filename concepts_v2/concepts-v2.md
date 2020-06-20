---
title: "Concepts, v2"
document: DxxxxR0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction and Motivation

See [@P1900R0], which was presented in Prague and received favorably.

::: bq
These problems are worth solving: 14-14-0-0-0
:::
 
This paper attempts to present a new concepts design to address the problems
presented there. I'll go through the design here based on three example concepts
in C++20: `range`, `view`, and `invocable`.

# The `Range` concept

`Range` makes for a great litmus test here, since there's so much we need to
be able to handle here, a lot of which is currently has to be manually implemented
(in a way that is not reusable for other concepts that might conceptually require
similar machinery). 

Let's start with just a small part of `Range` and build up from there: we have
a function `begin()` which returns an `iterator`, which has to satisfy the
`input_or_output_iterator` concept:

```cpp
template <typename R>
concept struct Range {
    typename iterator;
    requires std::input_or_output_iterator<iterator>;
    
    virtual auto begin(R&) -> iterator = 0;
};
```

We have a new notation, `concept struct`, to differentiate from C++20 concepts -
we need a lot more structure here, and because we're basically defining an
interface, treating this as a class declaration has a lot of benefits. The
declarations here are basically like member functions and member typedefs, and will
be used as such.

In our new concept class, we have three declarations:

1. `typename iterator` introduces an _associated type_ named `iterator` with
no definition. The type that this refers to will have to be introduced later.
2. `requires std::input_or_output_iterator<iterator>` introduces a constraint on
the associated type `iterator`.
3. `virtual auto begin(R&) -> iterator = 0;` introduces an _associated function_,
whose presence is mandatory in order to satisfy the concept. See [@P1292R0] for 
motivation for the `virtual` keyword here.

Because `begin()` is used unqualified here, this function can be satisfied by
either member or non-member syntax, preferring member. When a candidate is
considered, its return type will be used as the type `iterator` refers to, whose
constraints will then be checked.

In other words, these declarations are equivalent (at this point) in what
constraints they impose on a given type and how a type would satisfy `Range`:

::: cmptable

### C++20 Concepts
```cpp
template <typename R>
concept Range =
    requires(R& r) {
        { begin(r) } -> std::input_or_output_iterator;
    }
    or
    requires(R& r) {
        { r.begin() } -> std::input_or_output_iterator;
    };
```

### This design
```cpp
template <typename R>
concept struct Range {
    typename iterator;
    requires std::input_or_output_iterator<iterator>;
    
    virtual auto begin(R&) -> iterator = 0;
};
```
:::

Now, the C++20 concept isn't actually written like the above, for a few
important reasons. One reason is: C arrays satisfy neither formulation (neither
in C++20 concepts nor in the design proposed). Either way, this has to be handled
separately.

With C++20, any kind of other customization must be handled manually by the 
concept author, who has to come up with their own customization mechanism and
figure out how arrays play into it. 

With the design presented here, because `begin()` is declared as a function, a
`virtual` function no less, we have a more direct avenue for customization. In
this design, that is:

```cpp
template <Range R> concept struct Range<R&>  : Range<R> { };
template <Range R> concept struct Range<R&&> : Range<R> { };

template <typename T, size_t N>
concept struct Range<T[N]> {
    auto begin(T(&arr)[N]) -> T* override { return arr; }
};
```

Several things going on here. First, a type satisfies `Range` regardless of its
value category. If `R` is a range, `R&` is a range and `R&&` is a range as well
(although `R const` may not be). So as the author of the `Range` concept, we want
to make it easy to opt-in to being a `Range` and we wouldn't want to have to
make class authors "specialize" for each reference type. The first two partial
specializations just strip off the reference so that only non-reference types
need to be explicitly provided.

The third declaration is the key one. This is how we _implement_ `Range` for `T[N]`.
Similarly to how we implement a polymorphic interface with a derived type, we have
to `override` all of the pure `virtual` functions. With this additional 
implementation, arrays now satisfy `Range` because we implemented `begin()`
(matching the signature!) and the return type of `begin()`, `T*`, satisfies
`input_or_output_iterator`.

## Concept Class Satisfaction

Let's back up and talk about how a `concept class` is satisfied, since it's
slightly different to how a C++20 `concept` would be. Copying our whole
declaration for `Range` here again for clarity:

```cpp
template <typename R>
concept struct Range {
    typename iterator;
    requires std::input_or_output_iterator<iterator>;
    
    virtual auto begin(R&) -> iterator = 0;
};

template <Range R> concept struct Range<R&>  : Range<R> { };
template <Range R> concept struct Range<R&&> : Range<R> { };

template <typename T, size_t N>
concept struct Range<T[N]> {
    auto begin(T(&arr)[N]) -> T* override { return arr; }
};
```

In order to check if a type `T` satisfies `Range`, we go through several steps.

1. Pick the most specialized implementation of `Range` for `T`.
2. For each `virtual` function:
    a. Find an implementation. The candidate set here is first, the set of
    specialized implementations, then the set of member functions,
    then the set of non-member functions. We stop once we find a candidate.
    b. A candidate is viable if it matches the signature and, if the function's
    return type is an associated type, the return type meets all that associated
    type's requirements.

    If we cannot find a valid implementation for a `virtual` function, the concept is
not satisfied. 
3. If there is inconsistent determination of associated types or values, the
concept is not satisfied.

Let's consider three types (`int`, `std::vector<int> const&`, and `int[20]`) and
see how those steps work out.

For `int`, the most specialized implementation is the primary. We then look for
an implementation of `begin`. We have no specialized implementation, nor do we
have any member functions, and non-member lookup for `begin` (in the context of
the declaration of `Range`) finds nothing. We fail to find a valid
implementation and so `int` does not satisfy `Range`.

For `std::vector<int> const&`, we go through a longer process. To pick the
most specialized implementation, we have to see if `std::vector<int> const`
satisfies `Range` (if so, the `R&` one is the most specialized). So we look
for an implementation of `begin`. We have no specialized implementation, but
we do have a member function `begin` that we can invoke on an lvalue of type
`std::vector<int> const`. That function returns a `std::vector<int>::const_iterator`,
which does satisfy `std::input_or_output_iterator`. We have thus satisfied all
of our requirements, so `std::vector<int> const` indeed satisfies `Range`. And
then `std::vector<int> const&` satisfies `Range` following those same steps again 
(although an implementation could probably just determine from the empty body
that once `R` is a `Range`, `R&` trivially follows).

For `int[20]`, the most specialized implementation is `Range<T[N]>`, which
does contain an implementation of `begin`. This satisfies all of our requirements,
so `int[20]` satisfies `Range`.

## Invoking Concept Class Associated Functions

It's all well and good that we have all these ways of satisfies the `Range`
concept's `begin()` function - but how do we actually _invoke_ the thing? A lot
of the preexisting implementation complexity is to actually call the right
function.

And here, again the fact that `begin()` is declared as a function helps. We can
simply treat it as a function:

```cpp
Range<R>::iterator it = Range<R>::begin(rng);
```

The above is ill-formed if `R` does not satisfy `Range`, otherwise we already
went through the process of verifying that there is a valid `begin()`
implementation - this just invokes the correct one. Statically. Despite the use
of `virtual` and `override`, there is no dynamic dispatch here. Likewise, we
already went through the process of verifying the `iterator` associated type,
so we can just use it.

We can add a further simplification by following the _type-constraint_ syntax
idea and allow dropping the first type when invoking associated functions and
letting it be deduced from context (sort of like a CTAD for static member
functions, except still allowing a partial template list):

```cpp
Range<R>::iterator it = Range::begin(rng);
```

`Range<R>::begin` is not quite a function - it's a niebloid. It's an object, it
cannot be found by ADL (and stops ADL if it is found), it can be passed as an
argument to another function template. `Range::begin` is likewise not quite
a function template - you cannot provide direct template arguments to it, and you
can pass it as an argument.

And at this point we can do a full comparison between a C++20 implementation of
the half of the `Range` concept presented so far and the design presented here.
Note that `end` is missing so far - and requires twice as much code in C++20 to
support while only a few extra lines with this design (still roughly twice as much
code in the new design, but we're only doubling like 5 lines of code rather than
about 50).

::: cmptable
### C++20
```cpp
namespace __begin {
  template<class T> void begin(T&&) = delete;

  template<class T>
  void begin(std::initializer_list<T>) = delete;

  template<class R>
  concept has_member = std::is_lvalue_reference_v<R> &&
    requires(R& r) {
      r.begin();
      { __decay_copy(r.begin()) }
        -> input_or_output_iterator;
    };

  template<class R>
  concept has_non_member = requires(R&& r) {
    begin(static_cast<R&&>(r));
    { __decay_copy(begin(static_cast<R&&>(r))) }
      -> input_or_output_iterator;
  };

  template <class>
  inline constexpr bool nothrow = false;
  template <has_member R>
  inline constexpr bool nothrow<R> =
    noexcept(std::declval<R&>().begin());
  template <class R>
  requires (!has_member<R> && has_non_member<R>)
  inline constexpr bool nothrow<R> =
    noexcept(begin(std::declval<R>()));

  struct __fn {
    template<class R, std::size_t N>
    constexpr R* operator()(R (&array)[N]) const noexcept {
      return array;
    }

    template<class R>
      requires has_member<R> || has_non_member<R>
    constexpr auto operator()(R&& r) const noexcept(nothrow<R>) {
      if constexpr (has_member<R>) {
        return r.begin();
      } else {
        return begin(static_cast<R&&>(r));
      }
    }
  };
}

inline namespace __cpos {
    inline constexpr __begin::__fn begin{};
}

template <class T>
concept range = requires(T& t) {
    ranges::begin(t);
};

template <class T>
using iterator_t = decltype(ranges::begin(declval<T&>()));

iterator_t<R> b = ranges::begin(r);
```

### This Design
```cpp
template <typename R>
concept struct Range {
    typename iterator;
    requires std::input_or_output_iterator<iterator>;
    
    virtual auto begin(R&) -> iterator = 0;
};

template <Range R> concept struct Range<R&>  : Range<R> { };
template <Range R> concept struct Range<R&&> : Range<R> { };

template <typename T, size_t N>
concept struct Range<T[N]> {
    constexpr auto begin(T(&arr)[N]) -> T* noexcept override {
        return arr;
    }
};

Range<R>::iterator b = Range::begin(r);
```
:::

## Using Concept

Since associated functions and associated types are really just function and
type declarations, we can think of the concept class they're associated with
as something of a namespace. To that end, we can bring that namespace into
scope with a new kind of _using-directive_:

::: cmptable
### Longer Form
```cpp
template <Range R>
auto distance(R&& r) -> int {
    Range<R>::iterator b = Range::begin(r);
    Range<R>::sentinel e = Range::end(r);
    
    int cnt = 0;
    for (; b != e; ++b) {
        ++cnt;
    }
    return cnt;
}
```

### Shorter Form
```cpp
template <Range R>
auto distance(R&& r) -> int {
    using concept Range;
    iterator b = begin(r);
    sentinel e = end(r);
    
    int cnt = 0;
    for (; b != e; ++b) {
        ++cnt;
    }
    return cnt;
}
```
:::

Name lookup for `begin(r)`, for instance, will for look through the _using-directive_
and find the associated function `begin()` and stop there (this does _not_ do
ADL, because `Range<R>::begin()` is not a function in the usual sense, it's a
niebloid).

Maybe we even make such a _using-directive_ implicit based on the constraints of
the function template or class template we're inside of, or maybe that's too much
implicitness. 

# The `View` concept

A `View` is very similar to a `Range`. It has a few extra requirements, the most
important of which is explicit opt-in. We cannot _infer_ whether a type is a
`View`. The class author must tell this to us.

In the C++20 design, this is implemented as:

```cpp
template<class T>
inline constexpr bool enable_view = derived_from<T, view_base>;

template<class T>
concept view =
    range<T> && movable<T> && default_initializable<T> && enable_view<T>;
```

The simplest way of becoming a `view` is just to inherit from the empty type
`view_base`. Barring that, you can specialize the variable template
`enable_view` for your type.

But explicit opt-in is a fairly significant feature of a concept. Rather than
relying on the concept author to come up with such a facility, we can build
it into the language. In this design:

```cpp
template <typename R>
explicit concept struct View
    : Range<R>, movable<R>, default_initializable<R>
{ };

template <derived_from<view_base> V> concept struct View<V> : View<V> { };
```

Here, the concept class `View` "inherits" the requirements from `Range`, `movable`,
and `default_initializable` (even though the latter two are not concept classes,
this is fine). But it also adds the `explicit` keyword. This means that a type
is not a `View`, regardless of all the other requirements, unless there is an
explicit implementation of `View` for that type.

Similar to C++20's `view`, we provide a default opt-in for all types inheriting
from `view_base`. This doesn't automatically make them a `View`, they still have
to satisfy all the other requirements - this only satisfies the `explicit`
requirement.

While C++20 Ranges has concepts that require explicit opt-in, it also has concepts
that require explicit opt-out (like `sized_sentinel_for`). I am not sure yet
how to fit opt-out here.

Note that because `View` "inherits" from `Range`, it also inherits the associated
types and functions. That is:

```cpp
template <View V>
void foo(V v)
{
    using concept View;
    iterator i = begin(v);
}
```

This works, `iterator` would be `View<V>::iterator` which is `Range<V>::iterator`
and likewise `begin()` finds `Range<V>::begin`.

# The `invocable` concept family

Let's turn now to invocation, and the family of concepts that can be found there.
In particular, I'll focus on just `invocable` and `predicate` which are currently
defined as (for the purposes of this paper, I'm simplifying to assume equality-
preservation and am skipping `regular_invocable`. It doesn't change anything here
and just adds noise):

```cpp
template<class F, class... Args>
concept invocable = requires (F&& f, Args&&... args) {
    invoke(std::forward<F>(f), std::forward<Args>(args)...);
  };
  
template<class F, class... Args>
concept predicate =
    invocable<F, Args...> && @_boolean-testable_@<invoke_result_t<F, Args...>>;
```

There are several interesting questions these concepts bring up for the design.
How could we declare these things differently? How do we specify the associated
type here (the result of the invocation)? Can we do better than _`boolean-testable`_?

Let's start with `invocable`. Invocation is fundamentally the call operator, so
we would want to specify it as such:

```cpp
template <typename F, typename... Args>
concept struct Invocable {
    typename result_type;
    
    virtual auto operator()(F, Args...) -> result_type = 0;
};

// bunch of specializations follow for invocables that aren't ()-able
// this one is very incomplete, but just intended to be illustrative and
// I didn't want to clutter this with too much noise:
template <typename C, typename R, typename... Args>
concept struct Invocable<R (C::*)(Args...), C*, Args...> {
    constexpr auto operator()(R (C::*pmf)(Args...), C* c, Args... args) -> R override {
        return (c->*pmf)(std::forward<Args>(args)...);
    }
};

// our favorite std::invoke
constexpr auto invoke =
    []<typename F, typename... Args>(F&& f, Args&&... args)
        requires Invocable<F, Args...>
    {
        return Invocable<Args...>::operator()(std::forward<F>(f), std::forward<Args>(args)...);
    };
```

There's an interesting turnaround here, in that the callable object `std::invoke`
that actually does the invocation is implemented in terms of the concept,
rather than the concept being implementation in terms of the function. This just
seems like the correct direction.

But we can even do a little bit better. First, let's extend our notion of what
it means to bring in a "concept namespace" with a _using-directive_ to be 
more expansive. Perhaps we can make the concept the driver of lookup _even in
member contexts_?

```cpp
// our favorite std::invoke
constexpr auto invoke =
    []<typename F, typename... Args>(F&& f, Args&&... args)
        requires Invocable<F, Args...>
    {
        using concept Invocable<Args...>;
        return std::forward<F>(f)(std::forward<Args>(args)...);
    };
```

That is, before even looking inside of `F` for `operator()`, we start by
looking inside of `Invocable<Args...>`. This now works for pointers to members
as well. 

Although really, this is going to be such a common pattern, we may as well go
all the way and allow:

```cpp
constexpr auto invoke = Invocable::operator();
```

## The `predicate` concept

All `predicate` does is refine `invocable` such that it returns `bool`. But we
can't just say that, for complicated C++ reasons and having to deal with types
that are convertible to `bool` but don't actually behave like `bool` when it
comes to the operators `&&`, `||`, and `!`. Rather than pushing the burden
of dealing with this onto generic programming authors, let's embrace the issue
and deal with it entirely in the concept:

```cpp
template <typename F, typename... Args>
concept struct Predicate : Invocable<F, Args...>
{
    requires convertible_to<result_type, bool>;
    
    auto operator()(F f, Args... args) -> bool {
        using concept Invocable;
        return std::forward<F>(f)(std::forward<Args>(args)...);
    }
};
```

What's going on here? We have a concept class declaration, but with no new
`virtual` functions or associated types. We have a non-`virtual` function and
a new constraint. The constraint part is straightforward, a `Predicate` is
an `Invocable` whose `result_type` is `convertible_to<bool>` (we don't need to use
any qualification because all `Invocable`s will have a `result_type`).

The new `operator()` is there solely to coerce the result of the invocation to
`bool`, which is an entirely different way of dealing with the problem thoroughly
described in [@P1964R0]. Consider an implementation of `count_if` that instead
takes two predicates instead of one:

```cpp
template <Range R, typename T = Range<R>::reference
          Predicate<T> Pred1, Predicate<T> Pred2>
auto count_if(R&& r, Pred1 p1, Pred2 p2) -> int
{
    using concept Range, Predicate;

    R::iterator b = r.begin();
    R::sentinel e = r.end();
    int n = 0;
    for (; b != e; ++b) {
        if (p1(*b) && p2(*b)) {
            ++n;
        }
    }
    return n;
}
```

First, I wanted to point out the declartions of the iterator and sentinel here.
Because of the _using-directive_ bringing in the concept `Range`, we look in 
the concept for `R::iterator` and `r.begin()` first and find those there. This
syntax works for all types that satsify `Range` (including C arrays!). Really, we would basically always
want this (and there's no way around `Range<R>::reference`), so maybe this is an
extra argument in favor of implicitly bringing in concept associations from
constrained declarations. 

Second, this implementation is _guaranteed_ to be valid because the type of `p1(*b)` _is_
`bool`, regardless of what the type of `invoke(p1, *b)` is. We already know
from the constraints that the type of `invoke(p1, *b)` is convertible to `bool`,
and what `p1(*b)` does is invoke `Predicate::operator()(p1, *b)` - which 
invokes `Invocable::operator()(p1, *b)` and converts its result to `bool`.

We don't need _`boolean-testable`_ here as such. Even with a type like:

```cpp
struct Evil {
    operator bool() const;
    friend auto operator&&(Evil, Evil) -> void;
};

auto evil_pred(int) -> Evil;

auto count_evil(std::vector<int> v) -> int {
    return count_if(v, evil_pred, evil_pred);
}
```

The above implementation compiles, because the `operator&&` here is never used. 

## Terser associated type access syntax

Consider the desire to implement the function `fmap` for `optional<T>`. This is
a function that takes two arguments:

1. an `optional<T>`
2. a function that when invoked with a `T` yields a `U`

and returns an `optional<U>`. Consider also the desire to implement a very similar
function `bind` for `optional<T>`, whose second argument is a function that takes
a `T` and returns an `optional<U>` (for some `U`). 

Implementing the first in C++20 is... ok. Implementing the second is less so:

```cpp
template <typename T, invocable<T> F, typename U = invoke_result_t<F, T>>
auto fmap(optional<T>, F) -> optional<U>;

template <typename T, invocable<T> F, typename Z = invoke_result_t<F, T>>
    requires is_specialization_of<Z, optional>
auto bind(optional<T>, F) -> Z;
```

These declarations have some notable issues. First, there's just the API surface
area to deal with that you just have to know about `invoke_result_t`. Second,
what is `Z`? We talk about this function as returning an `optional<U>`, but instead
for specification convenience, we just return `Z`. 

I mean, you could return an `optional<U>` too:

```cpp
template <typename T, invocable<T> F,
          specializes<optional> Z = invoke_result_t<F, T>,
          typename U = Z::value_type>
auto bind(optional<T>, F) -> optional<U>;
```

Maybe that's better? This relies on just knowing that the `U` in `optional<U>`
can be retrieved via the `value_type` specialization. You could instead just
treat `optional` as a typelist and use `mp_first<Z>`? Neither of these provide
a lot of clarity.

Instead, what this design suggests is a new syntax for introducing names based on
the associated types of concepts. We just need some separator between the arguments
for the concept and the new kind of introducer, for which this paper just picks `/`.

```cpp
template <typename U, typename T, Invocable<T / result_type=U> F>
auto fmap(optional<T>, F f) -> optional<U>;
template <typename U, typename T, Invocable<T / result_type=optional<U>> F>
auto bind(optional<T>, F f) -> optional<U>;
```

What this means is that first we introduce an unbound template parameter, `U`.
But then, in `fmap`, we add the introducer `result_type=U`. This
pattern matches the type `Invocable<F, T>::result_type` against `U` (which would
always match because it's a type), and assigns `U` to that result.

The more  complicated version is in `bind`, where the pattern is `result_type=optional<U>`. If the `result_type` does _not_ match the pattern (say
if we provided a function whose result type was `int`), then this constraint is
not satisfied and this function template is removed from overload resolution.
But if it _is_ satisfied, then we pull out the the type and assign it to the name
`U`. 

While this seems initially more complex, this is a staggeringly simpler way of
expressing the constraint and reads precisely like what we're going for. We need
an `Invocable` on `T` whose `result_type` is `optional<U>`, for some `U`.

Consider a different algorithm which is even harder to properly declare in C++20
(although, like the above algorithms, very easy to implement once we get past the
declaration). In Haskell, there is a function `sequence` that takes (in C++ terms)
a range of `expected<T, E>` and returns a `expected<vector<T>, E>`. That is, if
all of the `expected`s hold a value, return a new `expected` whose value is the
list of the success results. Otherwise, return the first failure. 

::: cmptable
### C++20
```cpp
template <Range R,
    Specializes<expected> V = range_value_t<R>>
auto sequence(R) ->
    expected<vector<V::value_type>, V::error_type>;
```

### This Design
```cpp
template <class T, class E, Range</value=expected<T, E>> R>
auto sequence(R r) -> expected<vector<T>, E>;
```
:::

It's not like the C++20 solution is especially long. It's just ...
what is even going on here and what does any of it mean?

With this design, it's pretty obvious at a glance what the parameter to
`sequence` has to satisfy and what the result type is.

# Design Summary

In short, this paper proposes a new kind of concept (a concept class) which
uses pseudo-signatures instead of expression and has support for declaring
associated types. A concept class can be specialized (mapped) to provide
custom support for a particular associated function.

A concept class' associated functions can be used externally as niebloids, and
both associated functions and types can be brought into scope with a new kind of
_using-directive_, which also affects lookup for members. Concepts classes can
have non-customizable functions as well, which can be used to just provided bonus
functionality or to do things like coerce types. 

Lastly, this paper proposes a convenient syntax for accessing associated types
in template declarations, to make certain kinds of constraints vastly more
expressible.