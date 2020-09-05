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

Another important piece of functionality is simply the ability to print views. In range-v3, views were printable, which made it easy to debug programs or to provide meaningful output. For instnace, the following program using range-v3 happily compiles:

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
| `ints` | range-v3 | Unnecessary unless people really hate `iota`. |
| `iota` | C++20 | C++20 |
| `join` | partially C++20, lacks delimiter ability | [Tier 1 (adding delimiter ability)]{.addu} |
| `keys` | C++20 | C++20 |
| `linear_distribute` | range-v3 | [Tier 3]{.diffdel} |
| `maybe` | proposed in [@P1255R6] | ??? |
| `move` | range-v3 | Not proposed |
| `partial_sum` | range-v3 | [Tier 2]{.yellow} |
| `remove` | range-v3 | [Tier 2]{.yellow} |
| `remove_if` | range-v3 | [Tier 2]{.yellow} |
| `repeat` | range-v3 | [Tier 2]{.yellow} |
| `repeat_n` | range-v3 | [Tier 2]{.yellow} |
| `replace` | range-v3 | [Tier 2]{.yellow} |
| `replace_if` | range-v3 | [Tier 2]{.yellow} |
| `reverse` | C++20 | C++20 |
| `sample` | range-v3 | [Tier 3]{.diffdel} |
| `scan` | (not in range-v3) | [Tier 2]{.yellow} |
| `single` | C++20 | C++20 |
| `slice` | range-v3 | [Tier 2]{.yellow} |
| `sliding` | range-v3 | [Tier 2]{.yellow} |
| `split` | C++20, but unergonomic | See [@P2210R0]. |
| `split_when` | range-v3 | [Tier 2]{.yellow} |
| `stride` | range-v3 | [Tier 2]{.yellow} |
| `tail` | range-v3 | [Tier 2]{.yellow} |
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

## The `zip` family

The `zip` family of range adapters (`enumerate`, `zip`, `zip_with`, and `zip_tail` -- the latter also known as `pairwise`) is an extremely useful set of adapters, with broad applicability. 

Indeed, we have many algorithms that exist largely because we don't have `zip` (and prior to Ranges, didn't have the ability to compose algorithms). We have several algorithms that have single-range-unary-function and a two-range-binary-function flavors. What if you wanted to `ranges::transform` 3 ranges? Out of luck. But in all of these cases, the two-range-binary-function flavor could be written as a single-range that is a `zip` of the two ranges, `adjacent_difference` is `zip_tail` followed by `transform`, `inner_product` is `zip` followed by `accumulate`, and so on and so on. This is why we think `zip` is the top priority view.

The most generic of this group is actually `iter_zip_with`. We're not proposing this for inclusion as it doesn't come up much in user facing code, but it is the core functionality for the family. `iter_zip_with(f, rs...)` takes an `n`-ary invocable an `n` ranges and combines those into a single range whose values are the result of `f(is...)`, where the `is` are the iterators into those ranges (note: iterators, not what they refer to). The size of an `iter_zip_with` is the minimum size of its ranges. 

We can implement the adapters we want on top of `iter_zip_with` easily (there's a minor detail about how we want `value_type` and `reference` to behave, but beyond that, these are basically straightforward equivalences):

- `zip_with` is `iter_zip_with` except first dereferencing all the iterators into the provided callable.
- `zip(rs...)` is `iter_zip_with([](Is... is) { return tuple<range_reference_t<Rs>...>(*is...); }, rs...)`. Can't implement `zip` in terms of `zip_with` because zipping a range that produces prvalues should yield a tuple of values, not a tuple of references, and `zip_with` would lose that distinction. But you can think of `zip(rs...)` as `zip_with(make_tuple, rs...)`, except in a way that properly preserves references. 
- `enumerate(r)` is `zip(iota(range_size_t<R>(0)), r)`.
- `zip_tail(r)` is `zip(r, r | tail)`, which is `zip(r, r | drop(1))`

Which is why we think they should be considered and adopted as a group. 

But in order to actually adopt `zip` and friends into C++23, we need to resolve several problems.

### A `tuple` that is `indirectly_writable`

TODO

### A `tuple` whose `common_reference` has reference semantics

TODO

### `enumerate`'s first range

TODO

## The windowing family

TODO

## Derivates of `transform`

Several of the above views that are labeled "not proposed" are variations on a common theme: `addressof`, `const_`, `indirect`, and `move` are all basically wrappers around `transform` that take `std::addressof`, `std::as_const`, `std::dereference` (a function object we do not have at the moment), and `std::move`, respectively. Basically, but not exactly, since one of those functions doesn't exist yet and the other three we can't pass as an argument anyway.

But some sort of broader ability to pass functions into functions would mostly alleviate the need for these. `views::addressof` is shorter than `views::transform(LIFT(std::addressof))` (assuming a `LIFT` macro that wraps a name and emits a lambda), but we're not sure that we necessarily need to add special cases of `transform` for every useful function.


# Algorithms

The largest chunk of C++20 Ranges were the algorithms, and the work here has been very thorough. All the `<algorithm>`s have been rangified, which has been fantastic.

But there are a few algorithms that aren't in `<algorithm>` that do not have range-based versions: those that are in `<numeric>`. These are often the last algorithms considered for anything, they were the last algorithms that were made `constexpr` ([@P1645R1]) and now are the last to become range-based. They are:

- `iota`
- `accumulate`
- `reduce`
- `transform_reduce`
- `inner_product`
- `adjacent_difference`
- `partial_sum`
- `inclusive_scan`
- `exclusive_scan`
- `transform_inclusive_scan`
- `transform_exclusive_scan`

What to do about these algorithms? Well, one of the big motivations for Ranges was the ability to actually compose algorithms. This severely reduces the need for the combinatorial explosion of algorithms - all the `transform_meow` algorithms are `transform` followed by `meow`, so we probably don't need separate range-based algorithms for those. 

Four of these (`accumulate`, `reduce`, `transform_reduce`, and `inner_product`) return a value, while the other seven output a range (one through a pair of writable iterators and the other six through an output iterator). We'll consider these separately.

## Algorithms that Output a Range

`iota` is the easiest one to consider here. We already have `views::iota` in C++20, which importantly means that we already have all the correct constraints in place. In that sense, it almost takes less time to [adopt `ranges::iota`]{.addu} than it would take to discuss whether or not it's worth spending time adopting it.

## Algorithms that Output a Value

TODO

# Actions

TODO

---
references:
    - id: range-v3
      citation-label: range-v3
      title: range-v3
      author:
        - family: Eric Niebler
      issued: 2014
      URL: https://github.com/ericniebler/range-v3/
---
