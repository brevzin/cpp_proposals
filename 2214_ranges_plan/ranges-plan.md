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

This paper provides our opinion for how to categorize Ranges functionality into those three buckets. We go through, in turn: [views](#views), [algorithms](#algorithms), and [actions](#actions) (which do not exist in C++20).

# Views

C++20 included a bunch of views, but range-v3 has a whole lot more. Views, much like algorithms, are the kind of thing where it's just generally good to have more of them. The C++20 standard library has over 100 algorithms, but only 17 range adapters (excluding `all` and `ref`). We want more.

One critical piece of missing functionality is [`ranges::to` [@P1206R1]]{.addu}. It's not a view, but it is often used as the terminal component of a view pipeline to create a new trailing range - to finally collect the results of the computation being constructed. This is a top priority.

We'll start this section by enumerating all the adapters in range-v3 (and a few that aren't), noting their current status, and ranking them according to our view of their priority for C++23, before describing how we came up with such a ranking.

| View | Current Status | Priority |
|---------------|----------------|----------|
| `addressof` | range-v3 | Not proposed |
| `adjacent_filter` | range-v3 | [Tier 3]{.diffdel} |
| `adjacent_remove_if` | range-v3 | [Tier 3]{.diffdel} |
| `all` | C++20 | C++20 |
| `any_view<T>` | range-v3 | Not proposed |
| `c_str` | range-v3 | [Tier 3]{.diffdel} |
| `cache1` | range-v3 | [Top, largely for `flat_map`]{.addu} |
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
| `enumerate` | range-v3 | [Top]{.addu} |
| `filter` | C++20 | C++20 |
| `filter_map` | (not in range-v3) | [Top, as a more ergonomic `maybe`]{.addu} |
| `for_each` | range-v3 | [Top, except named `flat_map` like everyone else calls it and allow for non-views]{.addu} |
| `generate` | range-v3 | [Tier 2]{.yellow} |
| `generate_n` | range-v3 | [Tier 2]{.yellow} |
| `group_by` | range-v3 | [Top (but not how range-v3 does it)]{.addu} |
| `group_by_key` | (not in range-v3) | [Top]{.addu} |
| `indirect` | range-v3 | Not proposed |
| `intersperse` | range-v3 | [Tier 2]{.yellow} |
| `ints` | range-v3 | Unnecessary unless people really hate `iota`. |
| `iota` | C++20 | C++20 |
| `join` | partially C++20, lacks delimiter ability | [Top (adding delimiter ability)]{.addu} |
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
| `zip` | range-v3 | [Top]{.addu} |
| `zip_with` | range-v3 | [Top]{.addu} |

## The `zip` family

TODO

## The windowing family

TODO

## Derivates of `transform`

Several of the above views that are labeled "not proposed" are variations on a common theme: `addressof`, `const_`, `indirect`, and `move` are all basically wrappers around `transform` that take `std::addressof`, `std::as_const`, `std::dereference` (a function object we do not have at the moment), and `std::move`, respectively. Basically, but not exactly, since one of those functions doesn't exist yet and the other three we can't pass as an argument anyway.

But some sort of broader ability to pass functions into functions would mostly alleviate the need for these. `views::addressof` is shorter than `views::transform(LIFT(std::addressof))` (assuming a `LIFT` macro that wraps a name and emits a lambda), but we're not sure that we necessarily need to add special cases of `transform` for every useful function.


# Algorithms

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
