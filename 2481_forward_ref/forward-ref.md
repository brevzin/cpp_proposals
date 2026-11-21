---
title: "Forwarding reference to specific type/template"
document: P2481R3
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
    - name: Corentin Jabot
      email: <corentin.jabot@gmail.com>
toc: true
status: progress
---

# Revision History {- .unlisted .collapsed}

Since [@P2481R2], added Corentin and Daveed to the authors. We finally found good syntax during the Búzios meeting in November 2026! And implemented it.

When [@P2481R1] was discussed in Issaquah, three polls were taken. There was weak consensus to solve the problem as a whole, but there was clear preference to see "a new forwarding reference syntax":

|SF|F|N|A|SA|
|-|-|-|-|-|
|1|7|3|0|0|

Over "additional syntax in the parameter declaration":

|SF|F|N|A|SA|
|-|-|-|-|-|
|3|2|4|1|2|

So this paper focuses on trying to find a solution that satisfies that preference.

Since [@P2481R0], added [Circle's approach](#circles-approach) and a choice implementor quote.

# Introduction

There are many situations where the goal of a function template is deduce an arbitrary type - an arbitrary range, an arbitrary value, an arbitrary predicate, and so forth. But sometimes, we need something more specific. While we still want to allow for deducing `const` vs mutable and lvalue vs rvalue, we know either what concrete type or concrete class template we need - and simply want to deduce _just_ that. With the adoption of [@P0847R7], the incidence of this will only go up.

It may help if I provide a few examples.

## `std::tuple` converting constructor

`std::tuple<T...>` is constructible from `std::tuple<U...> $cv$ $ref$` when `T...` and `U...` are the same size and each `T` is constructible from `U $cv$ $ref$` (plus another constraint to avoid clashing with other constructors that I'm just going to ignore for the purposes of this paper). The way this would be written today is:

::: std
```cpp
template <typename... Ts>
struct tuple {
    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us&> && ...)
    tuple(tuple<Us...>&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us const&> && ...)
    tuple(tuple<Us...> const&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us&&> && ...)
    tuple(tuple<Us...>&&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us const&&> && ...)
    tuple(tuple<Us...> const&&);
};
```
:::

This is pretty tedious to say the least. But it also has a subtle problem: these constructors are all _overloads_ - which means that if the one you'd think would be called is valid, it is still possible for a different one to be invoked. For instance:

::: std
```cpp
void f(tuple<int&>);
auto g() -> tuple<int&&>;

void h() {
    f(g()); // ok!
}
```
:::

Here, we're trying to construct a `tuple<int&>` from an _rvalue_ `tuple<int&&>`. The desired behavior is that the `tuple<Us...>&&` is considered and then rejected (because `int&` is not constructible from `Us&&` - `int&&`). That part indeed happens. But the `tuple<Us...> const&` constructor still exists and that one ends up being fine (because `int&` is constructible from `Us const&` - which is `int&` here). That's surprising and undesirable.

But in order to avoid this, we'd need to only have a single constructor template. What we _want_ is to have _some kind_ of `tuple<Us...>` and just deduce the `$cv$ $ref$` part. But our only choice today is either the code above (tedious, yet mostly functional, despite this problem) or to go full template:

:::bq
```cpp
template <typename... Ts>
struct tuple {
    template <typename Other>
        requires is_specialization_of<remove_cvref_t<Other>, tuple>
              && /* ???? */
    tuple(Other&& rhs);
};
```
:::

How do we write the rest of the constraint? We don't really have a good way of doing so. Besides, for types which inherit from `std::tuple`, this is now actually wrong - that derived type will not be a specialization of tuple, but rather instead inherit from one. We have to do extra work to get that part right, as we currently do in the `std::visit` specification - see [variant.visit]{.sref}/1.

## `std::get` for `std::tuple`

We run into the same thing for non-member functions, where we want to have `std::get<I>` be invocable on every kind of tuple. Which today likewise has to be written:

::: std
```cpp
template <size_t I, typename... Ts>
auto get(tuple<Ts...>&) -> tuple_element_t<I, tuple<Ts...>>&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...> const&) -> tuple_element_t<I, tuple<Ts...>> const&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...>&&) -> tuple_element_t<I, tuple<Ts...>>&&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...> const&&) -> tuple_element_t<I, tuple<Ts...>> const&&;
```
:::

This one we could try to rewrite as a single function template, but in order to do that, we need to first coerce the type down to some kind of specialization of `tuple` - which ends up requiring a lot of the same work anyway.

## `transform` for `std::optional`

The previous two examples want to deduce to some specialization of a class template, this example wants to deduce to a specific type. One of the motivation examples from "deducing `this`" was to remove the quadruplication necessary when writing a set of overloads that want to preserve `const`-ness and value category. The adoption of [@P0798R8] gives us several such examples.

In C++20, we'd write it as:

::: std
```cpp
template <typename T>
struct optional {
    template <typename F> constexpr auto transform(F&&) &;
    template <typename F> constexpr auto transform(F&&) const&;
    template <typename F> constexpr auto transform(F&&) &&;
    template <typename F> constexpr auto transform(F&&) const&&;
};
```
:::

complete with quadruplicating the body. But with deducing this, we might consider writing it as:

::: std
```cpp
template <typename T>
struct optional {
    template <typename Self, typename F> constexpr auto transform(this Self&&, F&&);
};
```
:::

But this deduces too much! We don't want to deduce derived types (which in addition to unnecessary extra template instantiations, can also run into [shadowing issues](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0847r7.html#the-shadowing-mitigation-private-inheritance-problem)). We _just_ want to know what the `const`-ness and value category of the `optional` are. But the only solution at the moment is to C-style cast your way back down to your own type. And that's not a particular popular solution, even amongst [standard library implementors](https://discourse.llvm.org/t/std-pmr-maturity/62200/17):

::: quote
FWIW, we’re no longer using explicit object member functions for `std::expected`; STL couldn’t stomach the necessary C-style cast without which the feature is not fit for purpose.
:::

## `view_interface` members

Similar to the above, but even more specific, are the members for `view_interface` (see [view.interface]{.sref}). Currently, we have a bunch of pairs of member functions:

::: std
```cpp
template <typename D>
class view_interface {
    constexpr D& derived() noexcept { return static_cast<D&>(*this); }
    constexpr D const& derived() const noexcept { return static_cast<D const&>(*this); }

public:
    constexpr bool empty() requires forward_range<D> {
        return ranges::begin(derived()) == ranges::end(derived());
    }
    constexpr bool empty() const requires forward_range<D const> {
        return ranges::begin(derived()) == ranges::end(derived());
    }
};
```
:::

With deducing this, we could write this as a single function template - deducing the self parameter such that it ends up being the derived type. But that's again deducing way too much, when all we want to do is know if we're a `D&` or a `D const&`. But we can't deduce just `const`-ness.

## The Goal

To be more concrete, the goal here is to be able to specify a particular type or a particular class template such that template deduction will _just_ deduce the `const`-ness and value category, while also (where relevant) performing a derived-to-base conversion. That is, I want to be able to implement a single function template for `optional<T>::transform` such that if I invoke it with an rvalue of type `D` that inherits publicly and unambiguously from `optional<int>`, the function template will be instantiated with a first parameter of `optional<int>&&` (not `D&&`).

Put differently, and focused more on the desired solution criteria, the goal is to be able to define this (to be clear, the syntax `$some-kind-of$` is just a placeholder):

::: std
```cpp
template <class T> struct Base { };
template <class T> struct Derived : Base<T> { };

void f($some-kind-of$(Base<int>) x) {
  std::println("Type of x is {}", name_of(type_of(^x)));
}

template <class T>
void g($some-kind-of$(Base<T>) y) {
  std::println("Type of y is {}", name_of(type_of(^y)));
}

int main() {
  Base<char> bc;
  Base<int> bi;
  Derived<char> dc;
  Derived<int> di;

  f(bi);                // prints: Type of x is Base<int>&
  f(di);                // prints: Type of x is Base<int&>
  f(std::move(di));     // prints: Type of x is Base<int>&&

  g(bc);                // deduces T=char, prints: Type of y is Base<char>&
  g(std::move(dc));     // deduces T=char, prints: Type of y is Base<char>&&
  g(std::as_const(di)); // deduces T=int, prints: Type of y is Base<int> const&
}
```
:::

Two important things to keep in mind here:

* `f` and `g` are both function templates
* The types of the parameters `x` and `y` are always some kind of `Base`, even if we pass in a `Derived`.

## Previous Ideas

In previous revisions of this paper, several syntaxes were proposed and rejected:

* `string auto&& x`
* `string&&& x`
* `forward string x`
* `T&& x : string`
* `Q<string>` (deducing a qualifier alias template)

None of these felt viable. But we finally came up with something better in Búzios.

# Proposal

We propose a new way to annotate the template parameter to drive deduction, as in:

::: std
```cpp
template <typename T : std::string>
void f(T&& s);

struct Derived : std::string { };

auto check() -> void {
    std::string s = "msg";

    f("hello"s);  // #1: calls f<std::string>
    f(msg);       // #2: calls f<std::string&>
    f("msg");     // #3: calls f<std::string>
    f(Derived{}); // #4: calls f<std::string>
}
```
:::

The way this works is that we perform deduction and then coerce the result down to the target type. Here, we have a forwarding reference to `std::string` — `s` is always some kind of `string`. For the simple calls, `#1` and `#2`, this behaves just like a regular forwarding reference. But for `#3` and `#4`, conversions are allowed to happen too. The source template parameter is not observable — `f` never sees `char const[4]` or `Derived`.

That is the most basic example — but once we establish this kind of type coercion effect during the deduction process, it gives us a way to provide very clean solutions to all of our earlier examples.

Note that this is similar to the feature in Circle, except we're annotating the template parameter instead of the function parameter.

## `transform` for `std::optional`

::: std
```cpp
template <typename T>
struct optional {
    template <typename Self : optional, typename F>
    constexpr auto transform(this Self&&, F&&);
};
```
:::

The issue with using deducing this for implementing things like `optional<T>::transform` (and the many similar overload sets in `optional` and `expected`) is that we deduce too much, including potentially derived types. This problem was discussed in [@P0847R7] [here](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0847r7.html#the-shadowing-mitigation-private-inheritance-problem), and can these days be mitigated by doing a base-class splice with reflection to avoid having to do the prior C-style cast mitigation.

But even there, we just wrote a bunch of ugly syntax to solve a problem introduced by unnecessarily deducing derived types. The above formulation simply never deduces `Self` as anything other than `optional` (possibly `const`, possibly lvalue reference). Using the concrete example from the original paper:

::: cmptable
### Original (Broken)
```cpp
class B {
    int i;
public:
    template <typename Self>
    auto&& get(this Self&& self) {
        // we need to mitigate against shadowing
        return forward<Self>(self).B::i;
    }
};

class D : private B {
    double i;
public:
    using B::get;
};

D().get(); // error
```

### Proposed (Fixed)
```cpp
class B {
    int i;
public:
    template <typename Self : B>
    auto&& get(this Self&& self) {
        // no shadowing avoidance is necessary
        return forward<Self>(self).i;
    }
};

class D : private B {
    double i;
public:
    using B::get;
};

D().get(); // ok
```
:::

## `std::get` for `std::tuple`

::: cmptable
### Stauts Quo
```cpp
template <size_t I, typename... Ts>
auto get(tuple<Ts...>&)
    -> tuple_element_t<I, tuple<Ts...>>&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...> const&)
    -> tuple_element_t<I, tuple<Ts...>> const&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...>&&)
    -> tuple_element_t<I, tuple<Ts...>>&&;

template <size_t I, typename... Ts>
auto get(tuple<Ts...> const&&)
    -> tuple_element_t<I, tuple<Ts...>> const&&;
```

### Proposed
```cpp
template <size_t I,
          typename... Ts,
          typename Tuple : tuple<Ts...>>
auto get(Tuple&& )
    -> copy_cvref_t<Tuple&&, tuple_element_t<I, tuple<Ts...>>>;
```
:::

This is a more interesting example, since instead of coercing to a concrete type, we're coercing to a deduced template. We're at the same time deducing our `Ts...` and also deducing the cv-ref-qualifiers. Now, in the same way that the status quo implementation allows derived-to-base conversions in this form, the proposal also being called with an argument that inherits from, say, `std::tuple<int, double>`, but _not_ one that has a conversion function to `std::tuple<int, double>`. That follows from the form of the coercion target.

## `std::tuple` converting constructor

::: cmptable
### Status Quo
```cpp
template <typename... Ts>
struct tuple {
    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us&> && ...)
    tuple(tuple<Us...>&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us const&> && ...)
    tuple(tuple<Us...> const&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us&&> && ...)
    tuple(tuple<Us...>&&);

    template <typename... Us>
        requires sizeof...(Ts) == sizeof...(Us)
              && (is_constructible_v<Ts, Us const&&> && ...)
    tuple(tuple<Us...> const&&);
};
```

### Proposed
```cpp
template <typename... Ts>
struct tuple {
    template <typename... Us,
              typename OtherTuple : tuple<Us...>>
        requires sizeof...(Ts) == sizeof...(Us)
            and (is_constructible_v<
                    Ts,
                    copy_cvref_t<OtherTuple&&, Us&>
                > and ...)
    tuple(OtherTuple&& );
};
```
:::

This is quite similar to `std::get`, and works the same way.

## Homogeneous Packs

[@P1219R3]{.title} proposes syntax for homogeneous packs, both deduced and fixed. Interestingly enough, the type coercion we're proposing in this paper is sufficient to cover that problem as well:

::: cmptable
### P1219
```cpp
template <class T>
void f(T... v);
```

### This Paper
```cpp
template <class T, class... Ts : T>
void f(Ts... v);
```

---
```cpp
void f(int... v);
```

```cpp
template <class... Ts : int>
void f(Ts... v);
```
:::

We could still introduce the syntax in the bottom-left as short-hand for the bottom-right.

## Implementation Experience

This is implemented in Barry's clang fork and is available on compiler explorer.