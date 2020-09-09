---
title: "A Plan for C++23 Ranges"
document: P2214R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Conor Hoekstra
      email: <conorhoekstra@gmail.com>
    - name: Tim Song
      email: <t.canens.cpp@gmail.com>
toc: true
---

<style type="text/css">
span.orange {
    background-color: #ffa500;
}
span.yellow {
    background-color: #ffff00;
}
</style>

# Introduction

When Ranges was merged into C++20 [@P0896R4], it was knowingly incomplete. While it was based on the implementation experience in range-v3 [@range-v3], only a small part of that library was adopted into C++20. The Ranges proposal was big enough already, a lot of the pieces were separable and so could be included later. 

But now that the core of Ranges has been included, later has come and we have to figure out what to do for C++23. This is a particularly trying period in the committee's history with the global pandemic and lack of face-to-face meetings. But we do already have a plan for C++23 [@P0592R4] which laid out the following priorities:

::: quote
The priority order of handling material is thus:

1. Material that is mentioned in this plan.
2. Bug fixes, performance improvements, integration fixes for/between existing features, and issue processing.
3. Material that is not mentioned in this plan.
:::

and

::: quote
Where are nex-gen Ranges in this plan?

We could certainly entertain more Actions and Views in the realm of Ranges. Whether such material appears for standardization is a bit unknown at this point.
::: 

We believe that adding more functionality to Ranges is important, even if it would technically be a 3rd priority item (unless you consider the spirit of the 2nd priority to include integration with itself).

But there is a _lot_ of outstanding functionality that could be added. And while we think all of it should eventually be added (having more algorithms and more views is always a good thing), we realize that this is may be too much even in the C++23 time frame. Rather than having one-off papers that propose one bit of functionality (as in [@P1255R6], [@P1894R0], or [@P2164R1]), we think it's important to take a big picture view of what is out there and triage the various parts into three separate buckets:

1. Functionality that is really important for C++23. That which is frequently used and broadly applicable, and thus whose absence is frequently complained about.
2. Functionality that would be nice to have for C++23. Not as frequently needed as the first bucket, but still clearly want it in the standard - but more tolerable if this slips. Ideally C++23 anyway, but simply less critical.
3. Functionality that is less important or needs more design work.

This paper provides our opinion for how to categorize Ranges functionality into those three buckets. We go through, in turn: [views-adjacent functionality](#view-adjuncts), [views](#views), [algorithms](#algorithms), and [actions](#actions) (which do not exist in C++20).

# View adjuncts

C++20 Ranges, and the range-v3 that birthed it, isn't just a collection of loosely related views and algorithms. There's some important other functionality there. 

One critical piece of missing functionality is [`ranges::to` [@P1206R1]]{.addu}. It's not a view, but it is often used as the terminal component of a view pipeline to create a new trailing range - to finally collect the results of the computation being constructed. This is a top priority and is sorely missing. 

Another important piece of functionality is simply the ability to print views. In range-v3, views were printable, which made it easy to debug programs or to provide meaningful output. For instance, the following program using range-v3 happily compiles:

```cpp
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/filter.hpp>
#include <iostream>

int main() {
    namespace rv = ranges::views;
    auto r = rv::iota(0, 20)
           | rv::filter([](int i) { return i % 2 == 0; })
           | rv::transform([](int i){ return i * i; });
    std::cout << r;
}
```

and prints `[0,4,16,36,64,100,144,196,256,324]`. 

Similarly, fmtlib supports printing ranges with `fmt::join`, which is slightly more tedious but is at least still a single line:

```cpp
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>
#include <range/v3/view/filter.hpp>
#include <fmt/format.h>

int main() {
    namespace rv = ranges::views;
    auto r = rv::iota(0, 20)
           | rv::filter([](int i) { return i % 2 == 0; })
           | rv::transform([](int i){ return i * i; });
    fmt::print("[{}]", fmt::join(r, ","));
}
```

But neither the ability to stream views directly nor `fmt::join` are in C++20, so there is no direct way to print a range at all.
 

We think it's important that C++23 provides the ability to [format all the `view`s]{.addu}. Since these are all standard library types, it is difficult for the user to be able to actually do this themselves and it's frustrating to even have to.

# Views

C++20 included a bunch of views, but range-v3 has a whole lot more. Views, much like algorithms, are the kind of thing where it's just generally good to have more of them. The C++20 standard library has over 100 algorithms, but only 17 range adapters (excluding `all` and `ref`). We want more.

We'll start this section by enumerating all the adapters in range-v3 (and a few that aren't), noting their current status, and ranking them according to our view of their priority for C++23, before describing how we came up with such a ranking.

| View | Current Status | Priority |
|---------------|----------------|----------|
| `addressof` | range-v3 | Not proposed |
| `adjacent_filter` | range-v3 | [Tier 3]{.diffdel} |
| `adjacent_remove_if` | range-v3 | [Tier 3]{.diffdel} |
| `all` | C++20 | C++20 |
| `any_view<T>` | range-v3 | Not proposed |
| `c_str` | range-v3 | [Tier 3]{.diffdel} |
| `cache1` | range-v3 | [Tier 1, largely for `flat_map`]{.addu} |
| `cartesian_product` | range-v3 | [Tier 3]{.diffdel} |
| `chunk` | range-v3 | [Tier 2]{.yellow} |
| `common` | C++20 | C++20 |
| `concat` | range-v3 | [Tier 2]{.yellow} |
| `const_` | range-v3 | Not proposed |
| `counted` | C++20 | C++20 |
| `cycle` | range-v3 | [Tier 2]{.yellow} |
| `delimit` | range-v3 | [Tier 3]{.diffdel} |
| `drop` | C++20 | C++20 |
| `drop_last` | range-v3 | [Tier 2]{.yellow} |
| `drop_exactly` | range-v3 | [Tier 2]{.yellow} |
| `drop_while` | C++20 | C++20 |
| `empty` | C++20 | C++20 |
| `enumerate` | range-v3 | [Tier 1]{.addu} |
| `filter` | C++20 | C++20 |
| `filter_map` | (not in range-v3) | [Tier 1, as a more ergonomic `maybe`]{.addu} |
| `for_each` | range-v3 | [Tier 1, except named `flat_map` like everyone else calls it and allow for non-views]{.addu} |
| `generate` | range-v3 | [Tier 2]{.yellow} |
| `generate_n` | range-v3 | [Tier 2]{.yellow} |
| `group_by` | range-v3 | [Tier 1 (but not how range-v3 does it)]{.addu} |
| `group_by_key` | (not in range-v3) | [Tier 1]{.addu} |
| `indirect` | range-v3 | Not proposed |
| `intersperse` | range-v3 | [Tier 2]{.yellow} |
| `iter_zip_with` | range-v3 | [Tier 1]{.addu} |
| `iter_zip_tail_with` | (not in range-v3) | [Tier 1]{.addu} |
| `ints` | range-v3 | Unnecessary unless people really hate `iota`. |
| `iota` | C++20 | C++20 |
| `join` | partially C++20, lacks delimiter ability | [Tier 1 (adding delimiter ability)]{.addu} |
| `keys` | C++20 | C++20 |
| `linear_distribute` | range-v3 | [Tier 3]{.diffdel} |
| `maybe` | proposed in [@P1255R6] | ??? |
| `move` | range-v3 | Not proposed |
| `partial_sum` | range-v3 | [Tier 2, but not taking a callable (solely as a specialized form of `scan`)]{.yellow} |
| `remove` | range-v3 | [Tier 2]{.yellow} |
| `remove_if` | range-v3 | [Tier 2]{.yellow} |
| `repeat` | range-v3 | [Tier 2]{.yellow} |
| `repeat_n` | range-v3 | [Tier 2]{.yellow} |
| `replace` | range-v3 | [Tier 2]{.yellow} |
| `replace_if` | range-v3 | [Tier 2]{.yellow} |
| `reverse` | C++20 | C++20 |
| `sample` | range-v3 | [Tier 3]{.diffdel} |
| `scan` | (not in range-v3) | [Tier 2, as a rename of what is `partial_sum` in range-v3]{.yellow} |
| `single` | C++20 | C++20 |
| `slice` | range-v3 | [Tier 2]{.yellow} |
| `sliding` | range-v3 | [Tier 2]{.yellow} |
| `split` | C++20, but unergonomic | See [@P2210R0]. |
| `split_when` | range-v3 | [Tier 2]{.yellow} |
| `stride` | range-v3 | [Tier 2]{.yellow} |
| `tail` | range-v3 | [Tier 3]{.diffdel} |
| `take` | C++20 | C++20 |
| `take_exactly` | range-v3 | [Tier 2]{.yellow} |
| `take_last` | range-v3 | [Tier 2]{.yellow} |
| `take_while` | C++20 | C++20 |
| `tokenize` | range-v3 | Not proposed |
| `trim` | range-v3 | [Tier 2]{.yellow} |
| `unbounded` | range-v3 | Not proposed |
| `unique` | range-v3 | [Tier 2]{.yellow} |
| `values` | C++20 | C++20 |
| `zip` | range-v3 | [Tier 1]{.addu} |
| `zip_with` | range-v3 | [Tier 1]{.addu} |
| `zip_tail` | (not in range-v3) | [Tier 1]{.addu} |
| `zip_tail_with` | (not in range-v3) | [Tier 1]{.addu} |

## Full view or not full view

One question we have to ask is which of these views need to be full-blown view types and which do not. This is an important question to deal with in light of the very real limitation that is LWG's bandwidth. Views that need to be full types necessitate more longer and more complex specification which require more LWG time. Views that need _not_ be full types could save some time.

Consider `views::tail`. This is a very simple range adapter that simply drops its first element. As such, it could be specified entirely as:

```cpp
namespace std::ranges::views {
    inline constexpr auto tail = drop(1);
}
```

The above is a semantically correct implementation. However, is it the implementation we would actually want for `views::tail`? There are a few extra things `views::drop` has to do. `views::drop` must store a count and a cache for the begin iterator for forward ranges (to satisfy `O(1)`), but `views:tail` has to do none of these things since 1 is a constant and invoking `next()` one time still satisfies `O(1)`.

This puts us in an interesting position: we either adopt a known suboptimal implementation of `tail` with minimal LWG cost (such that we could likely adopt this for C++23) or we could hold off for the optimal implementation (in which case we could not in good conscience put `tail` as a Tier 1 view, as it is certainly not that important). As such, we have tentatively marked it as Tier 3.

This question is easier for some other views. As we'll see in the `zip` family shortly, there is really no issue with specifying `views::zip_with(F, Es...)` as expression-equivalent to `views::iter_zip_with(@_indirected_@(F), Es...)`. Although with the `map` family later, while `filter_map(E, F)` can be easily specified in terms of three adapters today (a `transform`, then a `filter`, then another `transform`), it could be significantly improved as a standalone view. It's a question that should guide review of this paper. 


## The `zip` family

The `zip` family of range adapters is an extremely useful set of adapters with broad applicability. It consists of two primary adapters (`iter_zip_with` and `iter_zip_tail_with`) and five range adapters that can be specified in terms of them (`enumerate`, `zip`, `zip_with`, `zip_tail`, and `zip_tail_with`).

Indeed, we have many algorithms that exist largely because we don't have `zip` (and prior to Ranges, didn't have the ability to compose algorithms). We have several algorithms that have single-range-unary-function and a two-range-binary-function flavors. What if you wanted to `ranges::transform` 3 ranges? Out of luck. But in all of these cases, the two-range-binary-function flavor could be written as a single-range that is a `zip` of the two ranges, `adjacent_difference` is `zip_tail` followed by `transform`, `inner_product` is `zip_with` followed by `accumulate`, and so on and so on. This is why we think `zip` is the top priority view.

The most generic of this group is `iter_zip_with`. While it doesn't come up much, if at all, in user facing code, it is core functionality for the family and aids significantly in the specification effort where LWG bandwidth is concerned. `iter_zip_with(f, rs...)` takes an `n`-ary invocable an `n` ranges and combines those into a single range whose values are the result of `f(is...)`, where the `is` are the iterators into those ranges (note: iterators, not what they refer to). The size of an `iter_zip_with` is the minimum size of its ranges. 

From `iter_zip_with`, we get:

- `zip_with(F, E...)` is expression-equivalent to `views::iter_zip_with(@_indirected_@(F), Es...)`, where `@_indirected_@` is a specification-only function object that dereferences all of its arguments before invoking `F`. 
- `zip(E...)` is expression-equivalent to `views::iter_zip_with(@_make-zip-tuple_@, Es...)`, where `@_make-zip-tuple_@` is basically `[]<input_iterator... Is>(Is... is) { return tuple<iter_reference_t<Is>...>(*is...); }`.
- `enumerate(E)` is expression-equivalent to `zip(_@index-view@_(E), E)`. We will discuss why `_@index-view@_(E)` instead of `iota(range_size_t<decltype(E)>(0)` [later](#enumerates-first-range).

But we do not want to define `zip_tail(E)` as `zip(E, E | drop(1))`. The primary reason to avoid doing this, and to make `zip_tail` (or rather, `iter_zip_tail_with`) a first-class view type despite the added specification effort, is that it allows supporting move-only views. On top of that, it has the benefit of not having to store the view twice or the other cruft that `drop` has to store (see also the `drop`/`tail` discussion earlier).

Once we have `iter_zip_tail_with`, `zip_tail` and `zip_tail_with` follow in the same way that we defined `zip` and `zip_with`.

In short, we get seven extremely useful ranges for the price of two. Which is why we think they should be considered and adopted as a group. 

But in order to actually adopt `zip` and friends into C++23, we need to resolve several problems.

### A `tuple` that is `writable`

Consider the following:

```cpp
std::vector<int> vi = /* ... */;
std::vector<std::string> vs = /* ... */;
ranges::sort(views::zip(vi, vs));
```

Does this operation make sense? Definitely! It would be very useful if this compiled. Unfortunately, as-is, it will not. We need to go back and understand _why_ this won't compile today and what we need to do to fix it.

In [@range-v3.573], user Jarod42 pointed out that the following program used to compile, yet provide a non-sensical answer:

```cpp
struct C
{
    explicit C(std::string a) : bar(a) {}

    std::string bar;
};

int main()
{
    std::vector<C> cs = { C("z"), C("d"), C("b"), C("c") };

    ranges::sort(cs | ranges::view::transform([](const C& x) {return x.bar;}));

    for (const auto& c : cs) {
        std::cout << c.bar << std::endl;
    }
}
```

The range was _not_ sorted, the emitted output was still in the same order as the input... because we're sorting a range of _prvalue_ `std::string`s, and trying to swap prvalue `std::string`s makes little sense because it doesn't do anything. 

But the reason it compiled was that the constraints checked if the iterators were assignable-through, which in this case was equivalent to checking if `std::string() = std::string()` is a valid expression... and it, unfortunately, is.  Assignment operators simply are not reference-qualified - they could not have been for a long time, and they are not now. The question became then - how can the library ensure that the above nonsense _does not compile_? 

As discussed in [@stl2.381]:

::: quote
One fix would be to require that `*o` return a true reference, but that breaks when `*o` returns a proxy reference. The trick is in distinguishing between a prvalue that is a proxy from a prvalue that is just a value. The trick lies in recognizing that a proxy always represents a (logical, if not physical) indirection. As such, adding a `const` to the proxy should not effect the mutability of the thing being proxied. Further, if `decltype(*o)` is a true reference, then adding `const` to it has no effect, which also does not effect the mutability. So the fix is to add `const` to `decltype(*o)`, `const_cast` `*o` to that, and then test for writability.
:::

Which is how we ended up with the `indirectly_writable` (the concept formerly known as `Writable`) requiring `const`-assignability. 

Hence, in order to make `ranges::sort(zip(vi, vs))` compile, we need to make `zip_view<R...>::iterator` model `indirectly_writable`, which means we need to make `std::tuple` const-assignable. That is, adding the following assignment operators, appropriately constrained:

```diff
  // [tuple.assign], tuple assignment
  constexpr tuple& operator=(const tuple&);
+ constexpr tuple& operator=(const tuple&) const;
  constexpr tuple& operator=(tuple&&) noexcept(@_see below_@);
+ constexpr tuple& operator=(tuple&&) const noexcept(@_see below_@);

  template<class... UTypes>
    constexpr tuple& operator=(const tuple<UTypes...>&);
+ template<class... UTypes>
+   constexpr tuple& operator=(const tuple<UTypes...>&) const;
  template<class... UTypes>
    constexpr tuple& operator=(tuple<UTypes...>&&);
+ template<class... UTypes>
+   constexpr tuple& operator=(tuple<UTypes...>&&) const;
```

_Or_, rather than change `std::tuple`, we can have `zip_view<R...>::value_type` and `zip_view<R...>::reference` be an entirely different tuple type than `std::tuple`, introducing a second tuple type into the standard library: `std2::tuple2`. This just seems like a facially terrible idea, we already have one tuple type, we should just have it solve this problem for is.

We therefore propose that `std::tuple<T...>` be const-assignable whenever all of `T...` are const-assignable. And likewise for `std::pair<T, U>`, for consistency, as well as the other proxy reference types in the standard libary: `std::vector<bool>::reference` and `std::bitset<N>::reference`.

### A `tuple` that is `readable`

We encourage everyone to review Eric Niebler's four-part series on iterators [@niebler.iter.0] [@niebler.iter.1] [@niebler.iter.2] [@niebler.iter.3], as this problem is covered in there in some depth.

There is a fundamental problem with standard library algorithms and how they interact with proxy iterators. One example used in the series is `unique_copy`:

::: quote
```{.cpp .numberLines}
// Copyright (c) 1994
// Hewlett-Packard Company
// Copyright (c) 1996
// Silicon Graphics Computer Systems, Inc.
template <class InIter, class OutIter, class Fn,
          class _Tp>
OutIter
__unique_copy(InIter first, InIter last,
              OutIter result,
              Fn binary_pred, _Tp*) {
  _Tp value = *first;
  *result = value;
  while (++first != last)
    if (!binary_pred(value, *first)) {
      value = *first;
      *++result = value;
    }
  return ++result;
}
```

[...] Note the value local variable on line 11, and especially note line 14, where it passes a value and a reference to `binary_pred`. Keep that in mind because it’s important!

[...] Why do I bring it up? Because it’s _super problematic_ when used with proxy iterators. Think about what happens when you try to pass `vector<bool>::iterator` to the above `__unique_copy` function:

```cpp
std::vector<bool> vb{true, true, false, false};
using R = std::vector<bool>::reference;
__unique_copy(
  vb.begin(), vb.end(),
  std::ostream_iterator<bool>{std::cout, " "},
  [](R b1, R b2) { return b1 == b2; }, (bool*)0 );
```

This _should_ write a “true” and a “false” to `cout`, but it doesn’t compile. Why? The lambda is expecting to be passed two objects of `vector<bool>`‘s proxy reference type, but remember how `__unique_copy` calls the predicate:
```cpp	
if (!binary_pred(value, *first)) { /*...*/
```

That’s a `bool&` and a `vector<bool>::reference`. Ouch!
:::

The blog goes on to point out that one way to resolve this is by having the lambda take both parameters as `auto&&`. But having to _require_ a generic lambda is a bit... much. This was the reason we have the idea of a `common_reference`. Rewriting the above to be:

```cpp
using R = std::iter_common_reference_t<std::vector<bool>::reference>;
__unique_copy(
  vb.begin(), vb.end(),
  std::ostream_iterator<bool>{std::cout, " "},
  [](R b1, R b2) { return b1 == b2; }, (bool*)0 );
```

This now works. And that is now the requirement that we have for iterators, that they be `indirectly_readable` - which, among other things, requires that there be a `common_reference` between the iterator's `reference` type and the iterator's `value_type&`:

```cpp
template<class In>
  concept @_indirectly-readable-impl_@ =
    requires(const In in) {
      typename iter_value_t<In>;
      typename iter_reference_t<In>;
      typename iter_rvalue_reference_t<In>;
      { *in } -> same_as<iter_reference_t<In>>;
      { ranges::iter_move(in) } -> same_as<iter_rvalue_reference_t<In>>;
    } &&
    common_reference_with<iter_reference_t<In>&&, iter_value_t<In>&> &&
    common_reference_with<iter_reference_t<In>&&, iter_rvalue_reference_t<In>&&> &&
    common_reference_with<iter_rvalue_reference_t<In>&&, const iter_value_t<In>&>;

template<class In>
  concept indirectly_readable =
    @_indirectly-readable-impl_@<remove_cvref_t<In>>;
    
template<class I>
  concept input_iterator =
    input_or_output_iterator<I> &&
    indirectly_readable<I> &&
    requires { typename @_ITER_CONCEPT_@(I); } &&
    derived_from<@_ITER_CONCEPT_@(I), input_iterator_tag>;    
```

How does this relate to `zip`?

Letting `I` be `zip_view<R...>::iterator` for some set of ranges `R...`, it just follows from first principles that `iter_reference_t<I>` should be `std::tuple<range_reference_t<R>...>` &mdash; dereferencing a zip iterator should give you a tuple of dereferencing all the underlying ranges' iterators. And then it sort of follows from symmetry that `iter_value_t<I>` should be `std::tuple<range_value_t<R>...>`. But then what does `iter_common_reference_t<I>` end up being?

Let's pick some concrete types. Taking our earlier example of:

```cpp
std::vector<int> vi = /* ... */;
std::vector<std::string> vs = /* ... */;
ranges::sort(views::zip(vi, vs));
```

We have a value type of `std::tuple<int, std::string>` and a reference type of `std::tuple<int&, std::string&>`. The common reference of those exists: the `reference` type is convertible to the value type but not in the other direction, so the common reference is just the value type of `std::tuple<int, std::string>`. This might be odd, having a common _reference_ type that has no reference semantics, but it does work. And in some cases you can't really do better (as in the `vector<bool>` example from the blog series, the common reference is `bool`).

But where we really run into problems is with non-copyable types. If instead of zipping a `std::vector<int>` and a `std::vector<std::string>`, we changed the first range to be a `std::vector<std::unique_ptr<int>>`, what happens? We have a value type of `std::tuple<std::unique_ptr<int>, std::string>` and a reference type of `std::tuple<std::unique_ptr<int>&, std::string&>`, similar to before. But now, neither is convertible to the other.

The constructor overload set of `std::tuple` is... intense to say the least. But the relevant ones are:

```{.cpp .numberLines}
constexpr explicit(@_see below_@) tuple();
constexpr explicit(@_see below_@) tuple(const Types&...);         // only if sizeof...(Types) >= 1
template<class... UTypes>
  constexpr explicit(@_see below_@) tuple(UTypes&&...);           // only if sizeof...(Types) >= 1

tuple(const tuple&) = default;
tuple(tuple&&) = default;

template<class... UTypes>
  constexpr explicit(@_see below_@) tuple(const tuple<UTypes...>&);
template<class... UTypes>
  constexpr explicit(@_see below_@) tuple(tuple<UTypes...>&&);

template<class U1, class U2>
  constexpr explicit(@_see below_@) tuple(const pair<U1, U2>&);   // only if sizeof...(Types) == 2
template<class U1, class U2>
  constexpr explicit(@_see below_@) tuple(pair<U1, U2>&&);        // only if sizeof...(Types) == 2
```

We have a converting constructor from `std::tuple<U...> const&`, viable if `(constructible_from<T, U const&> && ...)`, and a converting constructor `std::tuple<U...>&&`, viable if `(constructible_from<T, U&&> && ...)`. In both cases, we're just distributing the qualifiers. 

When trying to construct a `std::tuple<std::unique_ptr<int>, std::string>` from a `std::tuple<std::unique_ptr<int>&, std::string&>`, we reject the converting constructor of lines 9-10 because we can't construct a `std::unique_ptr<int>` from a `std::unique_ptr<int> const&`. `std::unique_ptr` isn't copyable, and that's not going to change.

When trying to construct a `std::tuple<std::unique_ptr<int>&, std::string&>` from an _lvalue_ `std::tuple<std::unique_ptr<int>, std::string>` (the fact that it's an lvalue is important), we reject that very same converting constructor because we can't construct a `std::unique_ptr<int>&` from a `std::unique_ptr<int> const&`. Can't bind a non-const lvalue reference to a const lvalue. But we can of course bind a non-const lvalue reference to a non-const lvalue.

And indeed that's how close this is to working. All we need is one more constructor (although we added two here for completeness):

```{.diff .numberLines}
  constexpr explicit(@_see below_@) tuple();
  constexpr explicit(@_see below_@) tuple(const Types&...);         // only if sizeof...(Types) >= 1
  template<class... UTypes>
    constexpr explicit(@_see below_@) tuple(UTypes&&...);           // only if sizeof...(Types) >= 1

  tuple(const tuple&) = default;
  tuple(tuple&&) = default;

+ template<class... UTypes>
+   constexpr explicit(@_see below_@) tuple(tuple<UTypes...>&);
  template<class... UTypes>
    constexpr explicit(@_see below_@) tuple(const tuple<UTypes...>&);
  template<class... UTypes>
    constexpr explicit(@_see below_@) tuple(tuple<UTypes...>&&);
+ template<class... UTypes>
+   constexpr explicit(@_see below_@) tuple(const tuple<UTypes...>&&);  

  template<class U1, class U2>
    constexpr explicit(@_see below_@) tuple(const pair<U1, U2>&);   // only if sizeof...(Types) == 2
  template<class U1, class U2>
    constexpr explicit(@_see below_@) tuple(pair<U1, U2>&&);        // only if sizeof...(Types) == 2
```

If only we had a way to express "a forwarding reference to `tuple<UTypes...>`" in the language. But if we add these constructors, then suddenly we _can_ construct a `std::tuple<std::unique_ptr<int>&, std::string&>` from an lvalue `std::tuple<std::unique_ptr<int>, std::string>`. And that would just end up binding the references as you would expect.

Such a change to the constructor set of `std::tuple` means that all of our `zip_view` iterators can actually be `indirectly_readable`, which means they can actually count as being iterators. In all cases then, the common reference type of zip iterators would become the reference type. Indeed, this even fixes the issue we mentioned earlier - where even when our underlying types were copyable, we originally ended up with a common reference type of `std::tuple<int, std::string>`, a type that does not have reference semantics. But now it would have a common reference type of `std::tuple<int&, std::string&>`, which certainly has reference semantics. 

We therefore propose that to extend the constructor overload set of `std::tuple<T...>` to add converting constructors from `std::tuple<U...>&` and `std::tuple<U...> const&&`. And likewise for `std::pair<T, U>`, for consistency. 

### `enumerate`'s first range

We wrote earlier that `enumerate(r)` can be defined as `zip(iota(range_size_t<R>(0)), r)`, but it turns out that this isn't _quite_ right, and we need to do something slightly different.

It turns out `iota` is a fairly complicated view due to wanting to avoid undefined behavior in its `difference_type` [@P1522R1]. `iota_view` has to choose a type that is wide enough to properly compute differences. One of the important points in that paper is that integer-like types are:

::: quote
Not required to be `WeaklyIncrementable`: There is no requirement that a user-defined integer-like type specify what _its_ difference type is.  (If we were to require that, then by induction implementors would be on the hook to provide an infinite-precision integer-like type.)  Therefore, `iota_view<iter_difference_t<I>>` for an arbitrary iterator `I` may in fact be ill-formed.
:::

Similarly, `iota(range_size_t<R>(0))` need not be valid [@range-v3.1141]. And, even if it were valid, it may be a wider integer type than would be strictly necessary. But `enumerate` is not as general-purpose as `iota`. From Eric Niebler:

::: quote
However, the enumerate view is special. The `iota_view<diffmax_t>` that `view::enumerate` constructs will never produce a number less than zero or greater than the maximum `diffmax_t`. So `diffmax_t` itself has enough bits to be usable as the difference type of `iota_view<diffmax_t>::iterator`.

I can solve this problem with a custom `index_view` that doesn't try to promote the index type when computing the difference type.
:::

And this is what range-v3 does: `enumerate(r)` is defined as `zip(@_index-view_@<range_size_t<R>, range_difference_t<R>>(), r)`, where `@_index-view_@<S, D>` is basically a specialized version of `iota` such that `range_size_t<@_index-view_@<S, D>>` is `S` and `range_difference_t<@_index-view_@<S, D>>` is `D`. That is, rather than trying to compute a size and difference types to fit the range, we just use the provided range's size and difference types. If it's good enough for `R`, it's good enough for `enumerate_view<R>`!

This `@_index-view_@` can be exposition-only, it's effectively an implementation detail of `enumerate`.

## The `group_by` family

TODO

## The `map` family

We added `views::transform` in C++20, but there are closely related views in that family, which several other languages also provide. 

`views::transform` takes a range of `T` and a function `T -> U` and produces a range of `U`. But we can play around with the shape of the callable to produce two other extremely useful adapters:

- we can take a function `T -> RangeOf<U>` and produce a range of `U`. That is, take the resulting range and flatten it out into a single range. 
- we can take a function `T -> optional<U>` and produce a range of `U` from those resulting optionals that are engaged. 

The former is commonly known as `flat_map` (because it's a `map` followed by a `flatten`), although C++20's version of `flatten` is named` join` and C++20's version of `map` is named `transform`. So perhaps this adapter should be named `join_transform` or `transform_join`? Eughh?

The latter is called `filter_map` in Rust and `compactMap` in Swift. Neither strike us as great names either. 

There really aren't any particular thorny library issues to resolve here, simply a question of specification. 

### `flat_map`

`flat_map(E, F)` is very nearly `E | transform(F) | join`. Very nearly, because that doesn't _quite_ work. If the callable returns a prvalue range that is not a `view` (a seemingly specific constraint that is actually a very common use-case - consider a function returning a `vector<int>`), the above doesn't work. This specific case frequently comes up on StackOverflow asking for workarounds. 

And there is one in range-v3, it's called `cache1`. As the name suggests, it caches a single element at a time from its parent view - which allows the range to be `join`ed. With the adoption of `cache1` (a view itself with other broad applicability), we could specify `views::flat_map(E, F)` as expression-equivalent to:

::: bq
- `E | views::transform(F) | views::join` if that is a valid expression.
- Otherwise, `E | views::transform(F) | views::cache1 | views::join`.
:::

range-v3 has `cache1` yet only supports the first bullet, under the name `views::for_each`.

Despite being composed of other adapters that we already have, this is sufficiently complex to implement, sufficiently important, and requires a new adapter to boot, that it merits Tier 1 inclusion.

### `filter_map`

For `filter_map`, [@P1255R6] seeks to address this problem but requires using three chained adapters:

```cpp
inline constexpr auto filter_map1 = [](auto&& f){
    return views::transform(FWD(f))
         | views::transform(views::maybe)
         | views::join;
};
```

This is an expensive implementation. Not only do we need three adapters, but `join` is a very complex adapter and we have an extremely specialized case here that is much simpler. Moreover, the result of `filter_map1` is always an input range only (we are joining a range of prvalue ranges). 

We could get the same behavior out of three simpler adapters in a different way in C++20:

```cpp
inline constexpr auto filter_map2 = [](auto&& f){
    return views::transform(FWD(f))
         | views::filter([](auto&& e) -> decltype(static_cast<bool>(e)) {
               return static_cast<bool>(e);
           })
         | views::transform([](auto&& e) -> decltype(@_decay-copy_@(*FWD(e))) {
               return @_decay-copy_@(*FWD(e));
           });
};
```

This implementation can now potentially be a bidirectional range (instead of `input`-only) thanks to avoiding `join`, but it still uses three adapters.

Moreover, both of these implementations have the issue of doing unnecessary extra copies. Let's say we have a range of `T` and a bunch of example functions, what would we want the resulting `filter_map`'s reference type to be?

| function | desired reference type | `maybe` result | `filter` result |
|-|-|-|-|
| `T -> A*` | `A&` | `A&` | `A` |
| `T -> std::optional<B> const&` | `B const&` | `B&` | `B` |
| `T -> std::optional<C>` | `C` | `C&` | `C` |
| `T -> boost::optional<D&>` | `D&` | `D&` | `D` |

The `maybe` implementation always yields lvalue references, the `transform-filter-transform` implementation always yields prvalues. The latter is predictable - we always copy out - whereas the former doesn't need to copy the `D`, although it does copy the `B` despite perhaps looking as if it does not (it does copy the `C`, and needs to, although it likewise might appear as if it does not).

We can provide the desired result with a more complex version of the final `transform` by only decaying xvalues:

```cpp
inline constexpr auto filter_map3 = [](auto&& f){
    return views::transform(FWD(f))
         | views::filter([](auto&& e) -> decltype(static_cast<bool>(e)) {
               return static_cast<bool>(e);
           })
         | views::transform([](auto&& e) requires requires { *e; } -> decltype(auto) {
               if constexpr (std::is_rvalue_reference_v(decltype(*FWD(e)))) {
                   return @_decay-copy_@(*FWD(e));
               } else {
                   return *FWD(e);
               }
           });
};
```

This is certainly quite a bit more complicated than the `views::tail` implementation suggested earlier! 

But we don't want to specify it like that either.

We think that `filter_map` merits a first-class view to accomplish this functionality not only because the above is fairly involved but also because it has has unnecessary overhead - it'd be nice to avoid instantiating three views when one is sufficient, along with all the wrapping that entails. 

## The windowing family

TODO

## Derivatives of `transform`

Several of the above views that are labeled "not proposed" are variations on a common theme: `addressof`, `const_`, `indirect`, and `move` are all basically wrappers around `transform` that take `std::addressof`, `std::as_const`, `std::dereference` (a function object we do not have at the moment), and `std::move`, respectively. Basically, but not exactly, since one of those functions doesn't exist yet and the other three we can't pass as an argument anyway.

But some sort of broader ability to pass functions into functions would mostly alleviate the need for these. `views::addressof` is shorter than `views::transform(LIFT(std::addressof))` (assuming a `LIFT` macro that wraps a name and emits a lambda), but we're not sure that we necessarily need to add special cases of `transform` for every useful function.


# Algorithms

The largest chunk of C++20 Ranges were the algorithms, and the work here has been very thorough. All the `<algorithm>`s have been range-ified, which has been fantastic.

But there are a few algorithms that aren't in `<algorithm>` that do not have range-based versions: those that are in `<numeric>`. These are often the last algorithms considered for anything, they were the last algorithms that were made `constexpr` ([@P1645R1]) and now are the last to become range-based. They are:

| Algorithm | Priority |
|-----------|----------|
| `iota` | [Tier 1]{.addu} |
| `accumulate` | [Tier 1, renamed to `fold`.]{.addu} |
| `reduce` | [Tier 2, along with `sum` and `product`.]{.yellow} |
| `transform_reduce` | Not proposed. |
| `inner_product` | Not proposed. |
| `adjacent_difference` | [Tier 3, renamed to `zip_tail_with`]{.diffdel} |
| `partial_sum` | [Tier 3, but without a binary operation parameter. Also adding `partial_fold`. ]{.diffdel} |
| `inclusive_scan` | [Tier 3]{.diffdel} |
| `exclusive_scan` | [Tier 3]{.diffdel} |
| `transform_inclusive_scan` | Not proposed. |
| `transform_exclusive_scan` | Not proposed. |

What to do about these algorithms? Well, one of the big motivations for Ranges was the ability to actually compose algorithms. This severely reduces the need for the combinatorial explosion of algorithms - all the `transform_meow` algorithms are `transform` followed by `meow`, so we probably don't need separate range-based algorithms for those. 

Four of these (`accumulate`, `reduce`, `transform_reduce`, and `inner_product`) return a value, while the other seven output a range (one through a pair of writable iterators and the other six through an output iterator). We'll consider these separately.

## Algorithms that Output a Value (Catamorphisms)

### `std::accumulate` &rarr; `ranges::fold`

We think having a range-based left-fold algorithm in the standard library is very important, since this is such a fundamental algorithm. Indeed, several of the other standard library algorithms _are_ simple folds &mdash; for instance `count`, `count_if`, `max_element`, `min_element`, `minmax_element`, and `inner_product`. We don't have a generic range-based `max` or `min` (just ones that takes an `initializer_list`), but those would also be a left-folds. As such , we think adding such a left-fold to the standard library is a top tier priority for C++23. Except that we think this algorithm should be named [`ranges::fold`]{.addu} - the problem with the name `accumulate` is that it is strongly suggestive of addition, which makes uses of it over different operations just very strange. `fold` is what the algorithm is, and has no such emphasis. It's the more generic name, for the most generic algorithm. 

[@P1813R0] goes through the work of introducing a set of constraints for these algorithms, and its suggestion for this algorithm is:

```cpp
template <input_range R, movable T, class Proj = identity,
          indirect_magma<const T*, projected<iterator_t<R>, Proj>, T*> BOp = ranges::plus>
constexpr accumulate_result<safe_iterator_t<R>, T>
    accumulate(R&& r, T init, BOp bop = {}, Proj proj = {});
```

We think this is a bad direction, for three reasons.

First, we should not default the binary operation at all. Having a default `fold` operation doesn't make much sense - it's reasonable for `ranges::sort` to default to sorting by `<`, since the entire standard library is built on `<` as the primary comparison operator, but that doesn't really hold for `+`. Instead, we should add separate named algorithms [`ranges::sum`]{.addu} and [`ranges::product`]{.addu} that just invoke `ranges::fold` with `std::plus()` and `std::multiplies()` -- or more likely that these invoke `ranges::reduce` instead as the more efficient algorithm with more constraints. 

Second, the above definition definitely follows Alexander Stepanov's law of useful return [@stepanov] (emphasis ours):

::: quote
When writing code, it’s often the case that you end up computing a value that the calling function doesn’t currently need. Later, however, this value may be important when the code is called in a different situation. In this situation, you should obey the law of useful return: *A procedure should return all the potentially useful information it computed.*
:::

But it makes the usage of the algorithm quite cumbersome. The point of a fold is to return the single value. We would just want to write:

```cpp
int total = ranges::sum(numbers);
```

Rather than:

```cpp
auto [_, total] = ranges::sum(numbers);
```

or:

```cpp
int total = ranges::sum(numbers, 0).value;
```

`ranges::fold` should just return `T`. This would be consistent with what the other range-based folds already return in C++20 (e.g. `ranges::count` returns a `range_difference_t<R>`, `ranges::any_of` - which can't quite be a `fold` due to wanting to short-circuit - just returns `bool`). 

Third, these constraints are far too restrictive. Copying the proposed definition of `magma` and `indirect_magma` here for readability:

::: quote
```cpp
// section 3.2.2 from P1813R0
template <class BOp, class T, class U>
concept magma =
    common_with<T, U> &&
    regular_invocable<BOp, T, T> &&
    regular_invocable<BOp, U, U> &&
    regular_invocable<BOp, T, U> &&
    regular_invocable<BOp, U, T> &&
    common_with<invoke_result_t<BOp&, T, U>, T> &&
    common_with<invoke_result_t<BOp&, T, U>, U> &&
    same_as<invoke_result_t<BOp&, T, U>, invoke_result_t<BOp&, U, T>>;
```

Let `bop` be an object of type `BOp`, `t` be an object of type `T`, and `u` be an object of type `U`. The value `invoke(bop, t, u)` must return a result that is representable by `common_type_t<T, U>`.

The decision to require common types for a over `magma<T, U>` is similar to the reason that `equality_comparable_with` requires `common_reference_with`: this ensures that when an algorithm requires a `magma`,we are able to _equationally reason_ about those requirements. It’s possible to overload `operator+(int,vector<int> const&)`, but that doesn’t follow the canonical usage of `+`.  Does `1 + vector{1, 2, 3}` mean "concatenate `vector{1, 2, 3}` to the end of a temporary `vector{1}`"?  Is it a shorthand for `accumulate(vector{1, 2, 3}, 1)`? The intention is unclear, and so `std::plus<>` should not model `magma<int, vector<int>>`.

```cpp
// section 3.2.11 from P1813R0
template <class BOp, class I1, class I2, class O>
concept indirect_magma =
    readable<I1> &&
    readable<I2> &&
    writable<O, indirect_result_t<BOp&, I1, I2>> &&
    magma<BOp&, iter_value_t<I1>&, iter_value_t<I2>&> &&
    magma<BOp&, iter_value_t<I1>&, iter_reference_t<I2>&> &&
    magma<BOp&, iter_reference_t<I1>, iter_value_t<I2>&> &&
    magma<BOp&, iter_reference_t<I1>, iter_reference_t<I2>> &&
    magma<BOp&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>;
```
:::

We see here again the heavy association of `plus` with `accumulate`, hence again the desire to rename the algorithm to `fold`. But the important thing to consider here is the requirement that the binary function _need_ be invokable on each type and that there _need_ be a common type for the result. We've already been through this process with the ranges comparison algorithsm in [@P1716R3] and removed those restrictions.

Consider a simple fold counting the occurences of a string (i.e. how you would implement `ranges::count` with `ranges::fold`):

```cpp
std::vector<std::string> words = /* ... */;
int n = ranges::fold(words, 0, [](int accum, std::string const& w){
    return accum + (w == "range");
});
```

Such an algorithm would not meet the requirements laid out in P1813. There's no common type between `int` and `string`, that lambda is only invocable with one of the possible four orders of arguments. But it's a perfectly reasonable fold. Instead, the only allowed implementation would be:

```cpp
int n = ranges::fold(words, 0, ranges::plus{}, [](std::string const& w) { return w == "ranges"; });
```

But we're hard-pressed to explain why would be considered better. In the general case, there may not even be an allowed implementation. Consider wanting to score a word in Scrabble. In Scrabble, each letter has a value but each tile can either multiply the score of a single letter or multiple the score of the whole word. One way to compute the score then is to use two `fold`s, one to figure out the world multiplier and another to figure out the letter sum:

```cpp
struct Square { int letter_multiplier, word_multiplier; };
vector<Square> squares = /* ... */;
vector<int> letters = /* ... */;

int score = fold(squares, 1, multiplies(), &Square::word_multiplier)
          * fold(zip_with(multiplies(),
                          squares | views::transform(&Square::letter_multiplier),
                          letters),
                 0, plus());
```

Another way is to keep a running sum of the two parts separately, and do a manual multiply:

```cpp
struct Accum {
    int result() const { return total * multiplier; };
    int multiplier, total;
};
int score = fold(zip(squares, letters), Accum(), [](Accum a, auto const& sq_let){
        auto [sq, letter] = sq_letter;
        return Accum{
            .multiplier = a.multiplier * sq.word_multiplier,
            .total = a.total + sq.letter_multiplier * letter
        };
    }).result();
```

We're not trying to argue that the second solution is necessarily better than the first - merely that it is a perfectly adequate solution, that happens to not be able to meet the constraints as laid out in P1813.

Instead, we suggest a much lighter set of restrictions on `fold`: simply that this is a binary operation:

```cpp
template <class F, class T, class U>
concept @_foldable_@ =
    regular_invocable<F&, T, U> &&
    convertible_to<invoke_result_t<F&, T, U>, T>;

template <class F, class T, class I>
concept @_indirectly-binary-foldable_@ =
    indirectly_readable<I> &&
    copy_constructible<F> &&
    @_foldable_@<F, T, iter_value_t<I>> &&
    @_foldable_@<F, T, iter_reference_t<I>> &&
    @_foldable_@<F, T, iter_common_reference_t<I>>;

template <input_range R, movable T, class Proj = identity, 
    @_indirectly-binary-foldable_@<T, projected<iterator_t<R>, Proj>> BinaryOperation>
constexpr T fold(R&& r, T init, BinaryOperation op, Proj proj = {}) {
    range_iterator_t<R> b = begin(r);
    range_sentinel_t<R> e = end(r);
    for (; b != e; ++b) {
        init = op(std::move(init), proj(*b));
    }
    return init;
}
```

### `ranges::reduce`

We have this interesting situation in the standard library today where `std::accumulate` has a name strongly suggestive of addition, yet because it's specified to invoke its binary operation serially, it has no additional requirements on that operation. But we also have `std::reduce`, which is a much more generic name with no suggested underlying operation, yet has very strong semantic constraints on its operation: it must be both associative and commutative. This comes from [@N3408], emphasis ours:

::: quote
Thrust has no `accumulate` algorithm. Instead, it introduces the analogous `thrust::reduce`, **which requires stricter semantics from its user-specified sum operator to allow a parallel implementation**. Specifically, `thrust::reduce` requires mathematical associativity and commutativity of its user-specified sum operator. This allows the algorithm implementor discretion to parallelize the sum. We chose the name `reduce` for this algorithm because we believe that most existing parallel programmers are familiar with the idea of a parallel reduction. Other names for this algorithm exist, e.g., `fold`. However,we did not select a name derived from `fold` because other languages tend to impose a non-associative directionality to the operation. [cf. Haskell’s `foldl` & `foldr`, Scala’s `foldLeft` & `foldRight`]
:::

While `ranges::fold` should have minimal constraints, that is not the case for a future `ranges::reduce`. As with `std::reduce`, we would need to enforce that the binary operation is both associative and commutative. This calls for the kinds of constrains that [@P1813R0] is proposing. As it is a more complex set of constraints, we suggest that this is a [Tier 2 algorithm]{.yellow}, with no default operation. Given the previous suggestion of `ranges::fold` not having a default operation either, we also suggest the addition of a [`ranges::sum` and a `ranges::product`]{.yellow} that simply invoke `ranges::reduce` with `std::plus()` and `std::multiplies()`, respectively, with an initial value defaulted to `range_value_t<R>()` and `range_value_t<R>{1}`, respectively.

The naming here is somewhat problematic. `reduce` is, in general, a much better name than `accumulate` as it does not have any particular operation connotation. But it has additional requirements on the operation. With the suggested change in name, we would end up having both `fold` and `reduce` &mdash; names that seem synonymous and interchangeable, though they are not. We feel that this is probably okay though, since people already frequently think `reduce` is "just" the parallel version of `accumulate` and perhaps having `fold` and `reduce` both would make users more likely to consult the documentation?


### `ranges::transform_reduce` and `ranges::inner_product`

These two algorithms are different from the previous two in that they are less fundamental. `transform_reduce` is a binary `transform` followed by `reduce` while `inner_product` is a binary `transform` followed by `accumulate`. First, `inner_product`, much like `accumulate`, is a bad name for the algorithm as it strongly prejudices `product` as the binary transform operation and as such uses of the algorithm with any other function simply look bizarre. From the [cppreference example for `inner_product`](https://en.cppreference.com/w/cpp/algorithm/inner_product):

```cpp
#include <numeric>
#include <iostream>
#include <vector>
#include <functional>
int main()
{
    std::vector<int> a{0, 1, 2, 3, 4};
    std::vector<int> b{5, 4, 2, 3, 1};
 
    int r1 = std::inner_product(a.begin(), a.end(), b.begin(), 0);
    std::cout << "Inner product of a and b: " << r1 << '\n';
 
    int r2 = std::inner_product(a.begin(), a.end(), b.begin(), 0,
                                std::plus<>(), std::equal_to<>());
    std::cout << "Number of pairwise matches between a and b: " <<  r2 << '\n';
}
```

Second, and more importantly, with Ranges allowing us to compose algorithms properly, do we even need these at all? Consider again the above example and how it might be written with and without specialized algorithms:

::: cmptable
### Specialized
```cpp
ranges::inner_product(a, b, 0,
    std::plus(), std::equal_to());
```

### Composed
```cpp
ranges::fold(views::zip_with(std::equal_to(), a, b),
    0, std::plus());
ranges::sum(views::zip_with(std::equal_to(), a, b));
```

:::

Even though the `ranges::fold` construction is more complicated, it's also easier to see the groupings and understand what's going on. The composed construction also allows for arbitrarily many ranges, not simply two. 

There is also the question of projections. With `transform_reduce` and `inner_product`, there are _three_ ranges that could be projected: each range into the binary grouping operation, and the result of that grouping. This makes it exceedingly awkward if you only want to provide exactly one of those projections:

::: cmptable
### Specialized
```cpp
ranges::inner_product(a, b, 0,
    std::plus(), std::multiplies(),
    p1);
```
    
### Composed
```cpp
ranges::fold(views::zip_with(std::multiplies(),
        a | views::transform(p1), b),
    0, std::plus());
```

---

```cpp
ranges::inner_product(a, b, 0,
    std::plus(), std::multiplies(),
    {}, p2);
```

```cpp
ranges::fold(views::zip_with(std::multiplies(),
        a, b | views::transform(p2)),
    0, std::plus());
```

---

```cpp
ranges::inner_product(a, b, 0,
    std::plus(), std::multiplies(),
    {}, {}, p3);
```

```cpp
ranges::fold(views::zip_with(std::multiplies(), a, b)
           | views::transform(p3),
    0, std::plus());
```

:::

We think that once we add [`ranges::fold` as Tier 1]{.addu} and [`ranges::reduce` as Tier 2]{.yellow}, we do not actually have a need for either a `ranges::transform_reduce` or a `ranges::inner_product` (which would also save us from having to come up with a name for the latter).
 

## Algorithms that Output a Range (Anamorphisms)

`iota` is the easiest one to consider here. We already have `views::iota` in C++20, which importantly means that we already have all the correct constraints in place. In that sense, it almost takes less time to adopt `ranges::iota` than it would take to discuss whether or not it's worth spending time adopting it. 

But that does not hold for the other algorithms.

### `std::adjacent_difference` &rarr; `ranges::zip_tail_with`

`std::adjacent_difference` joins `std::accumulate` and `std::inner_product` in the list of algorithms prejudicially named after a specific operation. We do not yet have `views::zip_tail_with` ([Tier 1]{.addu} above), and this would be the algorithm version of those views:

::: cmptable
### Specialized
```cpp
ranges::adjacent_difference(r, o);
```

### Composed
```cpp
ranges::copy(views::zip_tail_with(r, std::minus()), o);
```

---

```cpp
ranges::adjacent_difference(r, o, f);
```

```cpp
ranges::copy(views::zip_tail_with(r, f), o);
```
:::

Even though we're increasing the length of the expression as we go, and arguably increasing the complexity of the construction as well, we're also lowering the surface area of the API by taking advantage of composition. These become even better with the adoption of the pipeline operator in [@P2011R1]:

::: cmptable
### Specialized
```cpp
ranges::adjacent_difference(r, o);
```

### Composed
```cpp
views::zip_tail_with(r, std::minus()) |> ranges::copy(o);
```

---

```cpp
ranges::adjacent_difference(r, o, f);
```

```cpp
views::zip_tail_with(r, f) |> ranges::copy(o);
```
:::

This begs the question: do we actually need to have a `ranges::zip_tail_with()` at all? This question needs to be answered, and its existence lowers the priority of the range-ification of such algorithms relative to the adoption of their corresponding range adapters.

### `std::partial_sum` &rarr; `ranges::partial_fold` and `std::{in,ex}clusive_scan`

We saw in the catamorphism section that we have a pair of algorithms, `std::accumulate` and `std::reduce`, that solve basically the same problem except that one prejudices a particular operation (`std::accumulate` suggests `+`) while the other has the more generic name yet is actually more restrictive (`std::reduce` requires both the operation to be both associative and commutative, `std::accumulate` does not require either).

We have the exact same issue here, `std::partial_sum` is strongly suggestive of `+`, while `std::inclusive_scan` is the more generically-named algorithm that nevertheless imposes the stronger restriction (in this case, just associativity). 

Our suggestion for what to do with `std::partial_sum` and `std::{in,ex}clusive_scan`  thus mirrors our suggestion for what we did with `std::accumulate` and `std::reduce`:

- rename `std::partial_sum` to `ranges::partial_fold` (since it's a `fold` that also yields partial results), which will have neither a defaulted binary operation nor associativity requirements.
- introduce `ranges::{in,ex}clusive_scan`
- introduce `ranges::partial_sum` that is hard-coded to use `std::plus()` as the binary operation, which internally forwards to `ranges::inclusive_scan` (not `ranges::partial_fold`, since we know addition is associative). 

As we discussed with the question of the need for `adjacent_difference`, there would also be the question of whether we need these algorithms at all. As such, we ascribe them fairly low priority. 

### `transform_{in,ex}clusive_scan`

Similar to the question of `transform_reduce` and `inner_product`, we don't think we need a:

```cpp
ranges::transform_inclusive_scan(r, o, f, g);
```

once we can write

```cpp
ranges::inclusive_scan(r | views::transform(g), o, f);
```

or even

```cpp
ranges::copy(r | views::transform(g) | views::inclusive_scan(f), o);
```

The latter two having the nice property that you don't have to remember the order of operations of the operations. We don't think we need these at all.

# Actions

TODO

# Plan Summary

To summarize the above descriptions, we want to triage a lot of outstanding ranges algorithms, views, actions, and other utilities into three tiers based on our opinions of their importance. While ideally we could just add everything into C++23, we realize that this is not realistic with the amount of available LWG bandwidth, so our tier 1 here is trying to be as small as possible while still hitting as many major pain points as possible.

## [Tier 1]{.addu}

- `ranges::to`
- the ability to format `view`s with `std::format`
- the addition of the following first class range adapters:
    - `views::cache1`
    - `views::filter_map`    
    - `views::group_by`
    - `views::group_by_key`
    - `views::iter_zip_with`
    - `views::iter_zip_tail_with`
    - `views::join_with`
- the addition of the following range adapters specified in terms of other range adapters:
    - `views::enumerate` 
    - `views::flat_map`       
    - `views::zip`
    - `views::zip_with`
    - `views::zip_tail`
    - `views::zip_tail_with`
- the addition of the following range algorithms:
    - `ranges::fold()`
- the following other changes to standard library (necessary for the `zip` family):
    - `pair<T, U>` should be const-assignable whenever `T` and `U` are both const-assignable
    - `pair<T&, U&>` should be constructible from `pair<T, U>&`
    - `tuple<T...>` should be const-assignable whenever `T...` are const-assignable
    - `tuple<T&...>` should be constructible from `tuple<T...>&`.
    - `vector<bool>::reference` should be const-assignable
    - `bitset<N>::reference` should be const-assignable

## [Tier 2]{.yellow}

- the addition of the following range adapters:
    - `views::chunk`
    - `views::cycle`
    - `views::drop_last`
    - `views::drop_exactly`
    - `views::generate`
    - `views::generate_n`
    - `views::intersperse`
    - `views::partial_sum`
    - `views::remove`
    - `views::remove_if`
    - `views::repeat`
    - `views::repeat_n`
    - `views::replace`
    - `views::replace_if`
    - `views::scan`
    - `views::slice`
    - `views::sliding`
    - `views::split_when`
    - `views::stride`
    - `views::take_exactly`
    - `views::take_last`
    - `views::trim`
    - `views::unique`
- the addition of the following range algorithms:
    - `ranges::reduce()`
    - `ranges::sum()`
    - `ranges::product()`

## [Tier 3]{.diffdel}

- the addition of the following range adapters:
    - `views::adjacent_filter`
    - `views::adjacent_remove_if`
    - `views::cartesian_product`
    - `views::delimit`
    - `views::linear_distribute`
    - `views::sample`
    - `views::tail`    
- the addition of the following range algorithms:
    - `ranges::zip_tail_with()`
    - `ranges::partial_fold()`
    - `ranges::inclusive_scan()`
    - `ranges::exclusive_scan()`
    - `ranges::partial_sum()`
- the addition of ranges actions

---
references:
    - id: range-v3
      citation-label: range-v3
      title: range-v3
      author:
        - family: Eric Niebler
      issued:
        year: 2014
      URL: https://github.com/ericniebler/range-v3/
    - id: stepanov
      citation-label: stepanov
      title: From Mathematics to Generic Programming
      author:
        - family: Alexander A. Stepanov
      issued:
        year: 2014
    - id: range-v3.573
      citation-label: range-v3.573
      title: Readable types with prvalue reference types erroneously model IndirectlyMovable
      author:
        - family: Jarod42
      issued:
        year: 2017
      URL: https://github.com/ericniebler/range-v3/issues/573
    - id: range-v3.1141
      citation-label: range-v3.1141
      title: view::enumerate issues with latest 1.0-beta commits?
      author:
        - family: voivoid
      issued:
        year: 2019
      URL: https://github.com/ericniebler/range-v3/issues/1141
    - id: stl2.381
      citation-label: stl2.381
      title: Readable types with prvalue reference types erroneously model Writable
      author:
        - family: Eric Niebler
      issued:
        year: 2017
      URL: https://github.com/ericniebler/stl2/issues/381
    - id: niebler.iter.0
      citation-label: niebler.iter.0
      title: To Be or Not to Be (an Iterator)
      author:
        - family: Eric Niebler
      issued:
        year: 2015
      URL: http://ericniebler.com/2015/01/28/to-be-or-not-to-be-an-iterator/
    - id: niebler.iter.1
      citation-label: niebler.iter.1
      title: Iterators++, Part 1
      author:
        - family: Eric Niebler
      issued:
        year: 2015
      URL: http://ericniebler.com/2015/02/03/iterators-plus-plus-part-1/
    - id: niebler.iter.2
      citation-label: niebler.iter.2
      title: Iterators++, Part 2
      author:
        - family: Eric Niebler
      issued:
        - year: 2015
      URL: http://ericniebler.com/2015/02/13/iterators-plus-plus-part-2/
    - id: niebler.iter.3
      citation-label: niebler.iter.3
      title: Iterators++, Part 3
      author:
        - family: Eric Niebler
      issued:
        - year: 2015
      URL: http://ericniebler.com/2015/03/03/iterators-plus-plus-part-3/     
---
