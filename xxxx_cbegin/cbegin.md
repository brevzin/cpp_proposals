---
title: "`cbegin` should always return a constant iterator"
document: DxxxxR0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# How we got to here

A tale in many acts.

## Prologue: Terminology

[iterator.requirements.general]{.sref}/5 states:

::: bq
Iterators that further meet the requirements of output iterators are called _mutable iterators_.
Nonmutable iterators are referred to as _constant iterators_.
:::

This paper uses those terms with those meanings: a mutable iterator is one that is writable to, a constant iterator is one that is not writable to. 


## Act I: Introduction of Member `cbegin`

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
for (auto it = v.cbegin(), end = v.cend(); it != end; ++it)  {
    //use *it ...
}
```

`c.cbegin()` was specified in all of these containers to perform `as_const(c).begin()`. Although `std::as_const` itself was not added until much later - it is a C++17 feature, first proposed in [@N4380].

## Act II: Rise of Non-Member `cbegin`

C++11 thus added the free functions `std::begin` and `std::end`, and member functions `c.cbegin()` and `c.cend()`. But it did not yet have free functions to fetch constant iterators: those were added in 2013 by way of [@LWG2128].

While, `std::begin(c)` always calls `c.begin()` (except for C arrays), `std::cbegin(c)` was not specified to call `c.cbegin()`. Instead it, too, called `std::begin(c)` (not even `c.begin()`):

::: quote
Implement `std::cbegin`/`cend()` by calling `std::begin`/`end()`. This has numerous advantages:

1. It automatically works with arrays, which is the whole point of these non-member functions.
2. It works with C++98/03-era user containers, written before `cbegin`/`cend()` members were invented.
3. It works with `initializer_list`, which is extremely minimal and lacks `cbegin`/`cend()` members.
4. 22.2.1 [container.requirements.general] guarantees that this is equivalent to calling `cbegin`/`cend()` members.
:::

There are two important goals here to highlight.

First, the goal is still to provide constant iterators, not just call `begin() const`. The latter is an implementation strategy for the former.

Second, the goal is to avoid boilerplate. An implementation where `std::cbegin(c)` called `c.cbegin()` would require `c.cbegin()` to exist, which, as is clear from the list above, is not the case for a lot of useful types. 

As a result, `std::cbegin(c)` is basically specified to be `std::begin(as_const(c))` (although, again, predating `std::as_const`) which is basically `as_const(c).begin()`. 

The status quo at this point is that `c.cbegin()`, `as_const(c).begin()`, and `std::cbegin(c)` are all equivalent (where they are all valid) and all yield constant iterators.

## Act III: Climax of the Views

Before 2018, the standard library had two non-owning range types: `std::initializer_list<T>` (since C++11) and `std::string_view` (since C++17). Non-owning ranges are shallow-`const`, but both of these types are _always_-`const` so that distinction was insignificant.

That soon changed. 2018 opened with the addition of `std::span` [@P0122R7] and closed with the adoption of Ranges [@P0896R4], with a few more views added the subsequent year by way of [@P1035R7]. Now, for the first time, the C++ standard library had non-owning ranges that were nevertheless mutable. Ranges itself was but a small part of the range-v3 library, so there is a promise of many more views to come.

These types really throw a wrench in the `cbegin` design: because now `begin() const` does not necessarily yield a constant iterator, whereas this had previously always been the case.

It's important to note that while it had previously always been the case _in the standard library_, that is not true for the broad C++ community. In particular, Boost.Range (which begat range-v3 which begat C++20 Ranges) has for a very long time had a type named `boost::iterator_range<It>` (the predecessor to `std::ranges::subrange<It>`). This is a view, although that term hadn't existed yet, and so it had a `begin() const` member function that just returned an `It`. Which means that `std::cbegin` on an `iterator_range<int*>` gives you an `int*` - a mutable iterator.  

Where this discrepancy became most apparently visible was the specification of `std::span` during the ballot resolution (by way of [@LWG3320]). For the sake of simplicity, I am going to assume that the iterator types of `span<T>` and `span<T const>` are just `T*` and `T const*`, respectively.

* `span<T>::begin() const`, like all the other views, is shallow `const`, and so returns `T*`.
* `span<T>::cbegin() const`, like the other standard library containers, was provided for convenient access to a constant iterator. This returned `T const*`. Unlike the other standard library containers, this did not simply defer to `begin() const`. 

So far so good. But because `std::cbegin(s)` is specified to do `std::begin(as_const(s))`, we end up having different behavior between `s.cbegin()` and `std::cbegin(s)`. This is the first (and, thus far, only) type in the standard library for which this is the case - and while `s.cbegin()` would have yielded a constant iterator, `std::cbegin(s)` does not. 

As a result of NB comment resolution, to ship a coherent C++20, `span`'s `cbegin()` and `cend()` members were removed, for consistency. 

## Intermezzo: Examining the C++20 Status Quo

This leaves us in a state where:

- for all the standard library containers, `r.cbegin()` and `std::cbegin(r)` are equivalent, both meaning `as_const(r).begin()`, and both yielding a constant iterator. This is likely true for many containers defined outside of the standard library as well.

- for most of the standard library views, `r.cbegin()` does not exist and `std::cbegin(r)` is a valid expression that could yield a mutable iterator (e.g. `std::span<T>`). There are three different kinds of exceptions:

    1. `std::string_view::cbegin()` exists and is a constant iterator (since it is `const`-only). `std::initializer_list<T>::cbegin()` does _not_ exist, but `std::cbegin(il)` also yields a constant iterator.
    2. `std::ranges::single_view<T>` is an owning view and is actually thus deep `const`. While it does not have a `cbegin()` member function, `std::cbegin(v)` nevertheless yields a constant iterator (the proposed `views::maybe` in [@P1255R6] would also fit into this category).
    3. `std::ranges::filter_view<V, F>` is not actually `const`-iterable at all, so it is neither the case that `filt.cbegin()` exists as a member function nor that `std::cbegin(filt)` (nor `std::ranges::cbegin(filt)`) is well-formed. Many other views fit this category as well (`drop_view` being the most obvious, but `drop`, `reverse`, and `join` may not be either, etc.). Other future views may fit into this category as well (e.g. my proposed improvement to `views::split` in [@P2210R0]).

Put differently, the C++20 status quo is that `std::cbegin` on an owning range always provides a constant iterator while `std::cbegin` on a non-owning view could provide a mutable iterator or not compile at all. 

The original desire of Walter's paper from more than 15 years ago (which, in 2020 terms, may as well have happened at the last Jupiter/Saturn conjunction) still holds today: 

::: quote
However, when a container traversal is intended for inspection only, it is a generally preferred practice to use a `const_iterator` in order to permit the compiler to diagnose `const`-correctness violations
:::

How could we add `const`-correctness to views?

## A Non-Solution: Member `cbegin()`

One approach we could take to provide reliable `const`-traversal of unknown ranges is to push the problem onto the ranges:

1. We could say that `std::cbegin(c)` (and `std::ranges::cbegin(c)` as well) first tries to call `c.cbegin()` if that exists and only if it doesn't to fall-back to its present behavior of `std::begin(as_const(c))`.
2. We could then pair such a change with going through the standard library and ensuring that all views have a member `cbegin() const` that yields a constant iterator. Even the ones like `std::initializer_list<T>` that don't currently have such a member? 

Such a design would ensure that for all standard library ranges, `r.cbegin()` and `std::cbegin(r)` are equivalent and yield a constant iterator. Except for `filter_view`, for which `std::cbegin(filt)` would continue to not compile as it takes a `C const&`. 

What does this do for all the views outside of the standard library? It does nothing. `std::cbegin(v)` on such views would continue to yield a mutable iterator, as it does today with `boost::iterator_range`. That, in of itself, makes this change somewhat unsatisfactory.

But what would it actually mean to add a member `cbegin() const` to every view type? What would such a member function do? What it _should_ do is the exact same thing for every view &mdash; the same exact same thing that all views external to the standard library would have to do in order to opt in to `const`-traversal-on-demand. 

But if every type needs to do the same thing, that's an algorithm. The standard library should provide it once rather than having every view re-implement it. Or, more likely, have every view delegate to the algorithm and just have boilerplate member function implementations. A substantial amount of view implementations are already boilerplate, we do not need more. 

# Act IV: `std::const_iterator`

The problem we actually have is this: given an iterator, how do I create an iterator that is identical in all respects except for top-level mutability? This is, ultimately, the problem that from the very beginning `vector<T>::const_iterator` is intending to solve. It is a `vector<T>::iterator` in all respects (it's contiguous, its value type is `T`, it would have the same bounds coming from the same container) except that dereferencing such an iterator would give a `T const&` instead of a `T&`. 

## A Reverse Digression

We're already used to the fact that some iterators are generic wrappers over other iterators. `vector<T>` is already specified as:

```cpp
namespace std {
  template<class T, class Allocator = allocator<T>>
  class vector {
  public:
    // types
    
    using iterator               = @_implementation-defined_@; // see [container.requirements]
    using const_iterator         = @_implementation-defined_@; // see [container.requirements]
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;
```

Nobody is especially surprised by the fact that every container isn't manually implementing its own bespoke reverse iterators. `std::reverse_iterator<It>` does the job. Yet, `std::rbegin(c)` always calls `c.rbegin()` (except for arrays). Even though we've had this perfectly generic solution for a long time, if you wanted your container to support reverse-iteration, you just had to write these boilerplate `rbegin()`/`rend()` member functions that wrapped your iterators. 

Ranges improved this situation. `std::ranges::rbegin(E)` is a much more complicated algorithm that takes many steps (see [range.access.rbegin]{.sref} for complete description), but a key aspect of the design there is that if `std::ranges::begin(E)` and `std::ranges::end(E)` give you `bidirectional_iterator`s, then `std::ranges::rbegin(E)` itself does the wrapping and provides you `make_reverse_iterator(ranges​::​end(E))`. No more pushing work onto the containers. That means that it works even in this case:

```cpp
struct simple_span {
    int* begin() const;
    int* end() const;
};

void algo(simple_span ss) {
    auto rit = std::ranges::rbegin(ss); // ok
    auto rend = std::ranges::rend(ss);  // ok
    // ...
}
```

`std::rbegin` would've failed in this case, because we don't have the boilerplate necessary to make it work. But instead of pushing that boilerplate onto `simple_span`, we consigned it into `std::ranges::rbegin`. A much better solution.

## Const Is No Different

A generic `reverse_iterator<It>` is not that complicated. We're basically inverting operations. But the crux of the iterator remains the same: `*it` passes through to its underlying iterator.

A generic `const_iterator<It>` at first seems much less complicated. _Every_ operation is passthrough, except for one. We are _only_ modifying the behavior of the dereference operator. Yet, doing the right thing for dereference is decidedly non-trivial. Let's go through some cases. We're going to look at both the value type and reference type of several ranges and say what we want the desired `const_iterator<iterator_t<R>>` to dereference into: 

||`range_value_t<R>`|`range_reference_t<R>`|desired result type|
|-|-|-|-|
|`vector<int>`|`int`|`int&`|`int const&`|
|`vector<int> const`|`int`|`int const&`|`int const&`|
|`array<int const, N>`|`int`|`int const&`|`int const&`|
|a range of prvalue `int`|`int`|`int`|`int`|
|`vector<bool> const`|`bool`|`bool`|`bool`|
|`vector<bool>`|`bool`|`vector<bool>::reference`|`bool`|
|zipping a `vector<T>` and `vector<U>`|`tuple<T, U>`|`tuple<T&, U&>`|`tuple<T const&, U const&>`|

This table points out a few things:

- A first thought might be that we need to return a `range_value_t<R> const&`, but while that works in some cases, it would lead to every element dangling in other cases. 
- Sometimes, `It` is already a constant iterator, so we would want to actively avoid wrapping in such a case. 
- The last couple rows are hard. 

Thankfully, this is a solved problem. The `views::const_` [in range-v3](https://github.com/ericniebler/range-v3/blob/d098b9610ac2f182f667ae9274ac2fac7f1327f5/include/range/v3/view/const.hpp) has for many years used a formula that works for all of these cases. In C++20 Ranges terms, I would spell it this way:

```cpp
template <std::input_iterator It>
using const_ref_for = std::common_reference_t<
    std::iter_value_t<It> const&&,
    std::iter_reference_t<It>>;
```

This does not yield the correct result for the last row in my table at the moment, but if we make the changes to `std::tuple` prescribed in [@P2214R0], then it would. 

Avoiding unnecessary wrapping can be achieved through a factory function that checks to see if such wrapping would change type:

```cpp
// a type is a constant iterator if its an iterator whose reference type is
// the same as the type that const_ref_for would pick for it
template <typename It>
concept @_constant-iterator_@ = std::input_iterator<It>
                         && std::same_as<const_ref_for<It>, std::iter_reference_t<It>>;

// a type is a constant range if it is a range whose iterator is a constant iterator
template <typename R>
concept @_constant-range_@ = std::ranges::range<R> && @_constant-iterator_@<std::ranges::iterator_t<R>>;

template <std::input_iterator It>
constexpr auto make_const_iterator(It it) {
    if constexpr (@_constant-iterator_@<It>) {
        // already a constant iterator
        return it;
    } else {
        return const_iterator<It>(it);
    }
}
```

## Implementing `std::const_iterator<It>`

There's a lot of boilerplate in implementing a C++20 iterator. And especially for `const_iterator` where every single operation but one is simply pass-through to the underlying iterator. Every function here is a one-liner, there are really only three interesting things in this whole implementation:

1. Providing `iterator_concept = contiguous_iterator_tag;` ensures that wrapping a contiguous mutable iterator produces a contiguous constant iterator.
2. Only providing `iterator_category` for `forward_iterator`s ensures that we correctly handle C++20 input iterators (more on this later, and see also [@P2259R0]).
3. The spelling of the `reference` type for this iterator, described above.

```cpp
template <typename It> struct iterator_concept_for { };
template <typename It> requires std::contiguous_iterator<It>
struct iterator_concept_for<It> {
    using iterator_concept = std::contiguous_iterator_tag;
};

template <typename It> struct iterator_category_for { };
template <std::forward_iterator It>
struct iterator_category_for<It> {
    using iterator_category = typename std::iterator_traits<It>::iterator_category;
};

template <std::input_iterator It>
class const_iterator : public iterator_concept_for<It>
                     , public iterator_category_for<It>
{
    It it;

public:
    using value_type = std::iter_value_t<It>;
    using difference_type = std::iter_difference_t<It>;
    using reference = const_ref_for<It>;

    const_iterator() = default;
    const_iterator(It it) : it(std::move(it)) { }

    auto operator++() -> const_iterator& { ++it; return *this; }
    auto operator++(int) -> const_iterator requires std::forward_iterator<It> { auto cpy = *this; ++*this; return cpy; }        
    void operator++(int) { ++*this; }

    auto operator--() -> const_iterator& requires std::bidirectional_iterator<It> { --it; return *this; }
    auto operator--(int) -> const_iterator requires std::bidirectional_iterator<It> { auto cpy = *this; --*this; return cpy; }        

    auto operator+(std::ptrdiff_t n) const -> const_iterator requires std::random_access_iterator<It> { return const_iterator(it + n); }
    auto operator-(std::ptrdiff_t n) const -> const_iterator requires std::random_access_iterator<It> { return const_iterator(it - n); }
    friend auto operator+(std::ptrdiff_t n, const_iterator const& rhs) -> const_iterator { return rhs + n; }
    auto operator+=(std::ptrdiff_t n) -> const_iterator& requires std::random_access_iterator<It> { it += n; return *this; }
    auto operator-=(std::ptrdiff_t n) -> const_iterator& requires std::random_access_iterator<It> { it -= n; return *this; }        
    auto operator-(const_iterator const& rhs) const -> difference_type requires std::random_access_iterator<It> { return it - rhs.it; }
    auto operator[](std::ptrdiff_t n) const -> reference requires std::random_access_iterator<It> { return it[n]; }

    auto operator*() const -> reference { return *it; }
    auto operator->() const -> value_type const* requires std::contiguous_iterator<It> { return std::to_address(it); }

    auto operator==(const_iterator const& rhs) const  -> bool requires std::equality_comparable<It> {  return it == rhs.it; }
    auto operator<=>(const_iterator const& rhs) const requires std::random_access_iterator<It> { return it <=> rhs.it; }

    auto base() -> It& { return it; }
    auto base() const -> It const& { return it; }
};
```

## Implementing `const_sentinel<I, S>`

One of the changes in C++20 Ranges as compared to C++17 Ranges is the introduction of the Sentinel concept. A range is no longer a pair of two `iterator`s, it is an `iterator`/`sentinel` pair. The `sentinel` could be the same type as the `iterator`, such a range is known as a `common_range` but it need not be. 

We would need to handle producing a wrapped `sentinel` in addition to producing a wrapped `iterator`. Thankfully, `sentinel` only has two associated operation: `==` with its iterator (unconditionaly) and `-` for computing a difference from its iterator (conditionally). As a result, while there are more cases to consider, the implementation is much less code:

```cpp
template <std::input_iterator It, std::sentinel_for<It> S>
class const_sentinel {
    S s;
public:
    const_sentinel(S s) : s(std::move(s)) { }
    
    bool operator==(const_iterator<It> const& rhs) const {
        return rhs.base() == s;
    }
    
    auto operator-(const_iterator<It> const& rhs) const -> std::iter_difference_t<It>
            requires std::sized_sentinel_for<S, It>
    {
        return s - rhs.base();
    }

    friend auto operator-(const_iterator<It> const& lhs, const_sentinel const& rhs) -> std::iter_difference_t<It>
            requires std::sized_sentinel_for<S, It>
    {
        return -(rhs - lhs);
    }    
};

template <std::input_iterator It, std::sentinel_for<It> S>
constexpr auto make_const_sentinel(S s) {
    if constexpr (@_constant-iterator_@<It>) {
        // if the iterator is already a constant iterator, pass it through
        return s;
    } else if constexpr (std::same_as<It, S>) {
        // we have an iterator pair, so we wrap it the same way
        return const_iterator<It>(s);
    } else {
        return const_sentinel<It, S>{s};
    }
}
```

Note that we need the iterator type as a template parameter here.

There's a question as to whether we need to wrap the sentinel at all (in the case where it is distinct from the iterator type). It's possible that we do not, but following [@LWG3386], I don't have much confidence that we won't come up with an example later that demonstrates the need for such in the future. Are we especially worried about the wrapping cost? 

## Better Algorithms for `std::ranges::cbegin` and `std::ranges::end`

`std::ranges::cbegin` today ([range.access.cbegin]{.sref}), similar `std::begin`, unconditionally calls `ranges::begin`. While `std::ranges::rbegin(E)` does conditionally call `E.rbegin()`, I wonder to what extent this facility actually needs to be customizeable. The goal is to provide a constant iterator version of `begin()`.

With the above pieces, we can do precisely that (see full implementation [@const-impl]):

```cpp
inline constexpr auto possibly_const = []<std::ranges::range R>(R& r) -> auto& {
    if constexpr (std::ranges::range<R const> and not std::ranges::view<std::remove_const_t<R>>) {
        return const_cast<R const&>(r);
    } else {
        return r;
    }
};

inline constexpr auto cbegin = first_of(
    // 1. non-borrowed rvalue
    delete_if_nonborrowed_rvalue,
    // 2. possibly-wrapped begin of possibly-const r
    [](std::ranges::range auto&& r) 
        RETURNS(make_const_iterator(std::ranges::begin(possibly_const(r))))
);

inline constexpr auto cend = first_of(
    // 1. non-borrowed rvalue
    delete_if_nonborrowed_rvalue,
    // 2. possibly-wrapped end of possibly-const r
    [](std::ranges::range auto&& r) 
        RETURNS(make_const_sentinel<decltype(std::ranges::begin(possibly_const(r)))>(
            std::ranges::end(possibly_const(r))))
);
```

Here, `cbegin(r)` and `cend(r)` produce a range that is top-level const over any underlying range, without having to modify any of those underlying ranges to opt in to this behavior. This works for `std::vector<int>` and `std::span<int>` and `boost::iterator_range<int*>` and even views like `std::ranges::filter_view` (`possibly_const` ensures that if get passed a non-`const` `vector<int>`, we treat it as `const` first &mdash; which is both valid and necessary &mdash; while `filter_view const` isn't a `range` so we cannot treat it as `const` first).

In addition to simply working across all ranges, it has a few other features worth noting:

* For a `vector<int> v`, `cbegin(v)` gives precisely the type `vector<int>::const_iterator` (not `const_iterator<vector<int>::iterator>`).
* For a `span<int> s`, `cbegin(s)` provides a contiguous iterator over `int const&`, not just any kind of iterator.

## A `views::const_`

A whole const-view can be implemented on top of these pieces, in the same way that `views::reverse` is implemented on top of `reverse_iterator` (see the full implementation [@const-impl]):

```cpp
template <std::ranges::input_range V>
    requires std::ranges::view<V>
class const_view : public std::ranges::view_interface<const_view<V>> {
    V base = V();
public:
    constexpr const_view() = default;
    constexpr const_view(V base) : base(std::move(base)) { }

    constexpr auto begin() requires (!@_simple-view_@<V>) { return cbegin(base); }
    constexpr auto end() requires (!@_simple-view_@<V>) { return cend(base); }
    constexpr auto size() requires std::ranges::sized_range<V> { return std::ranges::size(base); }

    constexpr auto begin() const requires std::ranges::range<V const> { return cbegin(base); }
    constexpr auto end() const requires std::ranges::range<V const> { return cend(base); }
    constexpr auto size() const requires std::ranges::sized_range<V const> { return std::ranges::size(base); }
};

// libstdc++ specific (hopefully standard version coming soon!)
inline constexpr std::views::__adaptor::_RangeAdaptorClosure const_ =
    []<std::ranges::viewable_range R>(R&& r)
    {
        if constexpr (@_constant-range_@<R>) {
            return std::views::all(r);
        } else if constexpr (@_constant-range_@<std::remove_reference_t<R> const>) {
            return std::views::all(std::as_const(r));
        } else if constexpr (std::ranges::enable_borrowed_range<std::remove_cvref_t<R>>) {
            return std::ranges::subrange(cbegin(r), cend(r));
        } else {
            return const_view<std::views::all_t<R>>(std::views::all(r));
        }
    };
```

The four cases here are:

1. `r` is already a constant range, no need to do anything, pass it through. Examples are `std::span<T const>` or `std::vector<T> const` or `std::set<T>`.
2. `r` is not a constant range but `std::as_const(r)` would be. Rather than do any wrapping ourselves, we defer to `std::as_const`. Example is `std::vector<T>`.
3. `r` is a borrowed range, so we can rip away its iterator/sentinel and wrap them independently. Examples are `std::span<T>` or a `views::reverse`-ed `std::vector<T>` or taking the `views::values` of a `std::map<K, V>`.
4. `r` is neither a constant range nor a borrowed range, so we need to preserve it along with the iterators. Examples are `views::transform(f)`-ed `std::vector<T>` (for an `f` that doesn't produce constants).

To me, being able to provide a `views::const_` is the ultimate solution for this problem, since it's the one that guarantees correctness even in the presence of a range-based for statement:

```cpp
for (auto const& e : r | views::const_) { ... }
```

Or passing a constant range to an algorithm:

```cpp
dont_touch_me(views::const_(r));
```

As far as naming goes, obviously we can't just call it `views::const`. range-v3 calls it `const_`, but there's a few other good name options here like `views::as_const` or `views::to_const`.

## What About `std::cbegin` and `std::cend`?

The above presents an implementation strategy for `std::ranges::cbegin` and `std::ranges::cend`.

But what about `std::cbegin` and `std::cend`? The problem is, while the former is C++20 technology and so can make use of the C++20 iterator model, `std::cbegin` is C++11 technology and so has to remain fixed with the C++11 iterator model. The biggest difference for these purposes has to do with input iterators.

In C++11, even for an input iterator, this is valid code:

```cpp
auto val = *it++;
```

But in C++20, an input iterator's postfix increment operator need not return a copy of itself. All the ones in the standard library return `void`. This is the safer design, since any use of postfix increment that isn't either ignoring the result or exactly the above expression are simply wrong for input iterators. 

Trying to be backwards compatible with pre-C++20 iterators is quite hard (again, see [@P2259R0]), and the `const_iterator<It>` implementation provided in this paper would _not_ be a valid C++17 input iterator. Additionally, C++20 input iterators are required to be default constructible while C++17 input iterators were not. 

On the other hand, is it critically important to have a constant iterator for a C++17 input iterator? You're not going to mutate anything meaningful anyway.

A simple solution could have `std::cbegin(c)` pass through C++17 input iterators unconditionally, and otherwise do `make_const_iterator(as_const(c).begin())` (i.e. the `std::ranges::cbegin` described above) for all iterators that are either C++20 iterators or C++17 forward iterators. This probably addresses the majority of the use-cases with minimal fuss. 

Moreover, would we want to extend `std::cbegin` to also handle non-`const`-iterable ranges? This is technically doable:

```cpp
template <typename C>
concept can_begin = requires (C& c) { std::begin(c); }

template <class C>
    requires can_begin<const C>
constexpr auto cbegin(const C& c) {
    // today's overload, but also conditionally wrap
}

template <class C>
    requires (can_begin<C> && !can_begin<const C>)
constexpr auto cbegin(C& c) {
    // fallback for non-const-iterable ranges
}
```

I negated the constraint to prefer the `const C&` overload even for non-const arguments. 

But we didn't introduce a C++20 iteration model to then go back and give us more work to keep two models in lock-step. The C++20 one on its own is hard enough, and is a better model. We should just commit to it.

It'd probably just be good enough to do something like:

```cpp
template <class C>
constexpr auto cbegin(C const& c)
    requires requires { std::begin(c); }
{
    if consetxpr (std::forward_iterator<decltype(std::begin(c))>) {
        return make_const_iterator(std::begin(c));
    } else {
        // leave input iterators (or... whatever) alone
        return std::begin(c);
    }
}
```

And similarly for `std::cend`. This isn't entirely without peril: currently we say nothing about the constraints for `std::cbegin(r)`; just that it calls `r.begin()`, _whatever that is_. There isn't a requirement that this ends up giving an iterator and there's no requirement that any of the operations that `std::forward_iterator` would check are SFINAE-friendly. I don't know if we necessarily care about such (mis)uses of `std::cbegin`, but it is worth nothing.

## Now Reverse It

Now that we can produce a constant iterator for a range, producing a reversed constant iterator is a matter of composing operations. We don't have to worry about sentinels in this case:

```cpp
inline constexpr auto crbegin = first_of(
    // 1. non-borrowed rvalue
    delete_if_nonborrowed_rvalue,
    // 2. possibly-wrapped reversed begin of possibly-const range
    [](std::ranges::range auto&& r) RETURNS(make_const_iterator(std::ranges::rbegin(possibly_const(r))))
);

inline constexpr auto crend = first_of(
    // 1. non-borrowed rvalue
    delete_if_nonborrowed_rvalue,
    // 2. possibly-wrapped reversed end of possibly-const range
    [](std::ranges::range auto&& r) RETURNS(make_const_iterator(std::ranges::rend(possibly_const(r))))
);
```

Notably here, `ranges::rbegin` and `ranges::rend` already themselves will try to produce a `std::reverse_iterator` if the underlying range is bidirectional. Similarly, we're ourselves trying to produce a `std::const_iterator`. 

The extension to `std::crbegin` and `std::crend` mirrors the similar extension to `std::cbegin` and `std::cend`:

```cpp
template <class C>
constexpr auto crbegin(C const& c)
    requires requires { std::rbegin(c); }
{
    if consetxpr (std::forward_iterator<decltype(std::rbegin(c))>) {
        return make_const_iterator(std::rbegin(c));
    } else {
        // leave input iterators (or... whatever) alone
        return std::rbegin(c);
    }
}
```

# Act V: A Concluding Proposal

The status quo is that we have an algorithm named `cbegin` whose job is to provide a constant iterator, but it does not always do that, and sometimes it doesn't even provide a mutable iterator. This is an unfortunate situation. 

We can resolve this by extending `std::ranges::cbegin` and `std::ranges::cend` to conditionally wrap their provided range's `iterator`/`sentinel` pairs to ensure that the result is a constant iterator, and use these tools to build up a `views::const_` range adapter. This completely solves the problem without any imposed boilerplate per range. 

However, `std::cbegin` and `std::cend` are harder to extend. If we changed them at all, we would probably punt on handling C++17 input iterators and non-`const`-iterable ranges. This means that `std::cbegin` and `std::ranges::cbegin` do different things, but `std::rbegin` and `std::ranges::rbegin` _already_ do different things. `std::ranges::rbegin` is already a superior `std::rbegin`, so having `std::ranges::cbegin` be a superior `std::cbegin` only follows from that. In other words, `std::cbegin` is constrained to not deviate too much from its current behavior, whereas `std::ranges::cbegin` is new and Can Do Better.

Would it be worth making such a change to `std::cbegin`? 

Ultimately, the question is where in the Ranges Plan for C++23 [@P2214R0] such an improvement would fit in? That paper is focused exclusively on providing a large amount of new functionality to users. The facility proposed in this paper, while an improvement over the status quo, does not seem more important than any of that paper. I just want us to keep that in mind - I do not consider this problem in the top tier of ranges-related problems that need solving. 

# Epilogue

Thanks to Tim Song for helping me work through the design and implementation details of this paper. Thanks to Peter Dimov and Tomasz Kamiński for insisting on design sanity (even as they insisted on different designs) and providing feedback. Thanks to Eric Niebler for having already solved the problem of how to come up with the right reference type for a `const_iterator<It>` in range-v3.

---
references:
  - id: const-impl
    citation-label: const-impl
    title: "Implementing `cbegin`, `cend`, and `const_view`"
    author:
      - family: Barry Revzin
    issued:
      - year: 2020
    URL: https://godbolt.org/z/avbPG8
---