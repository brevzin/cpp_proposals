---
title: "`cbegin` on views"
document: DxxxxR0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# How we got to here

A tale in three acts.

## Prologue: Terminology

The term `const_iterator` can have two possible connotations. It can be used to refer to an iterator that is not writable (typically because `*it` is a `T const&` for some object type `T`). Or it can be used to refer to specifically the named member type `C::const_iterator`. 

Because there doesn't seem to be a good alternative term for the former, I'll do my best to make clear which usage I mean. It's nearly always the former. 


## Act I: Member `cbegin`

In 2004, C++0x had added `auto` but not yet added the range-based for statement. So there was this problem: how do you write a for loop that is immutable? The goal of the paper was quite clear:

::: quote
This paper proposes to improve user access to the `const` versions of C++ container `iterator`s and `reverse_iterator`s.
:::

and:

::: quote
However, when a container traversal is intended for inspection only, it is a generally preferred practice to use a `const_iterator` in order to permit the compiler to diagnose `const`-correctness violations
:::

The solution proposed in [@N1674] (and later adopted by way of [@N1913]) was to add members `cbegin()` and `cend()` (and `crbegin()` and `crend()`) to all the standard library containers, facilitating this code:

```cpp
for (auto it = v.cbegin(),end=v.cend(); it!=end; ++it)  {
    //use *it ...
}
```

`c.cbegin()` was specified in all of these containers perform `as_const(c).begin()`. Although `std::as_const` itself was not added until much later - it is a C++17 feature, first proposed in [@N4380].

## Act II: Non-member `cbegin`


C++11 thus added the free functions `std::begin` and `std::end`, and member functions `c.cbegin()` and `c.cend()`. But it did not yet have free functions to fetch `iterators` to `const`: those were added in 2013 by way of [@LWG2128].

While, `std::begin(c)` always calls `c.begin()` (except for C arrays), `std::cbegin(c)` was not specified to call `c.cbegin()`. Instead it, too, called `std::begin(c)` (not even `c.begin()`):

::: quote
Implement `std::cbegin`/`cend()` by calling `std::begin`/`end()`. This has numerous advantages:

1. It automatically works with arrays, which is the whole point of these non-member functions.
2. It works with C++98/03-era user containers, written before `cbegin`/`cend()` members were invented.
3. It works with `initializer_list`, which is extremely minimal and lacks `cbegin`/`cend()` members.
4. 22.2.1 [container.requirements.general] guarantees that this is equivalent to calling `cbegin`/`cend()` members.
:::

There are two important goals here to highlight.

First, the goal is still to provide `const_iterator`s, not just call `begin() const`. The latter is an implementation strategy for the former.

Second, the goal is to avoid boilerplate. An implementation where `std::cbegin(c)` called `c.cbegin()` would require `c.cbegin()` to exist, which, as is clear from the list above, is not the for a lot of useful types. 

As a result, `std::cbegin(c)` is basically specified to be `std::begin(as_const(c))` (although, again, predating `std::as_const`) which is basically `as_const(c).begin()`. 

The status quo at this point is that `c.cbegin()`, `as_const(c).begin()`, and `std::cbegin(c)` are all equivalent (where they are all valid) and all yield `const_iterator`s.

## Act III: Climax of the Views

Before 2018, the standard library had two non-owning range types: `std::initializer_list<T>` (since C++11) and `std::string_view` (since C++17). Non-owning ranges are shallow-`const`, but both of these types are _always_-`const` so that distinction was insignificant.

That soon changed. 2018 opened with the addition of `std::span` [@P0122R7] and closed with the adoption of Ranges [@P0896R4], with a few more views added the subsequent year by way of [@P1035R7]. Now, for the first time, the C++ standard library had non-owning ranges that were nevertheless mutable. Ranges itself was but a small part of the range-v3 library, so there is a promise of many more views to come.

These types really throw a wrench in the `cbegin` design: because now `begin() const` does not necessarily yield a `const_iterator` (i.e. a non-writable iterator), whereas this had previously always been the case.

Where this became most apparently visible was the specification of `std::span` during the ballot resolution by way of [@LWG3320]. For the sake of simplicity, I am going to assume that the iterator types of `span<T>` and `span<T const>` are just `T*` and `T const*`, respectively.

* `span<T>::begin() const`, like all the other views, is shallow `const`, and so returns `T*`.
* `span<T>::cbegin() const`, like the other standard library containers, was provided for convenient access to a `const_iterator`. This returned `T const*`. Unlike the other standard library containers, this does not simply defer to `begin() const`. 

So far so good. But because `std::cbegin(s)` is specified to do `std::begin(as_const(s))`, we end up having different behavior between `s.cbegin()` and `std::cbegin(s)`. This is the first (and, thus far, only) type in the standard library for which this is the case - and while `s.cbegin()` would have yielded a `const_iterator`, `std::cbegin(s)` does not. 

As a result of NB comment resolution, to ship a coherent C++20, `span`'s `cbegin()` and `cend()` members were removed, for consistency. 

## Intermezzo: Examining the C++20 Status Quo

This leaves us in a state where:

- for all the standard library containers, `r.cbegin()` and `std::cbegin(r)` are equivalent, both meaning `as_const(r).begin()`, and both yielding a `const_iterator`. This is likely true for many containers defined outside of the standard library as well.

- for most the standard library views, `r.cbegin()` does not exist and `std::cbegin(r)` is a valid expression that could yield a mutable iterator (e.g. `std::span<T>`). There are three different kinds of exceptions:

    1. `std::string_view::cbegin()` exists and is a `const_iterator` (since it is `const`-only). `std::initializer_list<T>::cbegin()` does _not_ exist, but `std::cbegin(il)` also yields a `const_iterator`.
    2. `std::ranges::single_view<T>` is an owning view and is actually thus deep `const`. While it does not have a `cbegin()` member function, `std::cbegin(v)` nevertheless yields a `const_iterator` (the proposed `views::maybe` in [@P1255R6] would also fit into this category).
    3. `std::ranges::filter_view<V, F>` is not actually `const`-iterable at all, so it is neither the case that `filt.cbegin()` exists as a member function nor that `std::cbegin(filt)` (nor `std::ranges::cbegin(filt)`) is well-formed. Other future views may fit into this category as well (e.g. my proposed improvement to `views::split` in [@P2210R0]).

Put differently, the C++20 status quo is that `std::cbegin` on an owning range always provides a `const_iterator` while `std::cbegin` on a non-owning view could provide a mutable iterator or not compile at all. 

The original desire of Walter's paper from more than 15 years ago (which, in 2020 terms, may as well have happened at the last Jupiter/Saturn conjunction) still holds today: 

::: quote
However, when a container traversal is intended for inspection only, it is a generally preferred practice to use a `const_iterator` in order to permit the compiler to diagnose `const`-correctness violations
:::

How could we add `const`-correctness to views?

## Act IV: Let's Consider Our Options

TODO