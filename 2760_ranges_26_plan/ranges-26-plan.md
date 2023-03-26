---
title: "A Plan for C++26 Ranges"
document: P2214R2
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Conor Hoekstra
      email: <conorhoekstra@gmail.com>
    - name: Tim Song
      email: <t.canens.cpp@gmail.com>
toc: true
tag: ranges
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

For the C++23 cycle, we set out to create a plan to prioritize what additions we wanted to make for Ranges [@P2214R2]. We ended up adopting all of the proposals we originally labelled as Tier 1 (with the exception of some we deliberately dropped, see later), as well as some from Tier 2. Moreover, based on the questions we've seen in various contexts about how to solve certain problems with Ranges - a significant percentage of them can be answered with some new C++23 facility, which suggests that we prioritized the right tools.

To summarize, in C++23 we adopted the following facilities:

* General additions to ranges:
  * the ability to define first-class user-defined range adaptors ([@P2387R3])
  * the ability to collect a range into a container, `ranges::to` ([@P1206R7])
  * the ability to format ranges ([@P2286R8])
* New range adaptors:
  * `views::adjacent` and `views::adjacent_transform` ([@P2321R2])
  * `views::as_const` ([@P2278R4])
  * `views::as_rvalue` ([@P2446R2])
  * `views::cartesian_product` ([@P2374R4])
  * `views::chunk` ([@P2442R1])
  * `views::chunk_by` ([@P2443R1])
  * `views::enumerate` ([@P2164R9])
  * `views::join_with` ([@P2441R2])
  * `views::repeat` ([@P2474R2])
  * `views::slide` ([@P2442R1])
  * `views::stride` ([@P1899R3])
  * `views::zip` and `views::zip_transform` ([@P2321R2])
* New (or improved) range algorithms:
  * allowing C++20 iterators to be used in C++17 algorithms ([@P2408R5])
  * `ranges::contains` ([@P2302R4])
  * `ranges::fold` and family ([@P2322R6])
  * `ranges::iota` ([@P2440R1])
  * `ranges::shift_left` and `ranges::shift_right` ([@P2440R1])

There were also a bunch of smaller improvements that are not listed here.

But there's still plenty more work to be done - both on the range adaptor and the range algorithm front. The goal of this paper is to do for the C++26 timeframe what our previous plan did for the C++23 one: express what we think is the right prioritization of work, while describing what some of the outstanding issues are so that we can start tackling them.

# Views

As before, we'll start by enumerating all the adapters in range-v3 (and a few that aren't), noting their status updated by C++23. Note that many of the adaptors here labelled C++20 or C++23 are in range-v3 also, we're just using the status "range-v3" to indicate that an adaptor is in range-v3 *only*:

| View | Current Status | Proposed Priority |
|---------------|----------------|----------|
| `addressof` | range-v3 | Not proposed |
| `adjacent` | C++23 | -- |
| `adjacent_transform` | C++23 | -- |
| `adjacent_filter` | range-v3 | [Tier 2]{.yellow} |
| `adjacent_remove_if` | range-v3 | [Tier 2]{.yellow} |
| `all` | C++20 | -- |
| `any_view<T>` | range-v3 | Not proposed |
| `c_str` | range-v3 | [Tier 2]{.yellow} |
| `cache1` | range-v3 | [Tier 1. Possibly renamed as `cache_last` or `cache_latest`]{.addu} |
| `cartesian_product` | C++23 | -- |
| `chunk` | C++23 | -- |
| `chunk_by` | C++23 | -- |
| `chunk_on` | (not in range-v3) | [Tier 1]{.addu} |
| `common` | C++20 | -- |
| `concat` | range-v3 | [Tier 1 [@P2542R2]]{.addu} |
| `const_` | C++23 (as `as_const`) | -- |
| `counted` | C++20 | -- |
| `cycle` | range-v3 | [Tier 1]{.addu} |
| `delimit` | range-v3 | [Tier 1]{.addu} |
| `drop` | C++20 | -- |
| `drop_last` | range-v3 | [Tier 1]{.addu} |
| `drop_last_while` | (not in range-v3) | [Tier 1]{.addu} |
| `drop_exactly` | range-v3 | [Tier 2]{.yellow} |
| `drop_while` | C++20 | -- |
| `empty` | C++20 | -- |
| `enumerate` | C++23 | -- |
| `filter` | C++20 | -- |
| `for_each` | range-v3 | [Tier 1. Most languages call this `flat_map`, but we probably need to call it `transform_join`.]{.addu} |
| `generate` | range-v3 | [Tier 1]{.addu} |
| `generate_n` | range-v3 | [Tier 1]{.addu} |
| `group_by` | range-v3 | Not proposed. Subsumed by `chunk_by`. |
| `head` | (not in range-v3) | [Tier 2]{.yellow} |
| `indirect` | range-v3 | Not proposed |
| `intersperse` | range-v3 | [Tier 2]{.yellow} |
| `ints` | range-v3 | Unnecessary unless people really hate `iota`. |
| `iota` | C++20 | -- |
| `join` | C++20 and C++23 | -- |
| `keys` | C++20 | -- |
| `linear_distribute` | range-v3 | [Tier 3]{.diffdel} |
| `maybe` | proposed in [@P1255R9] | ??? |
| `move` | C++23 (as `as_rvalue`) | -- |
| `partial_sum` | range-v3 | [Tier 1, but not taking a callable (solely as a specialized form of `scan`)]{.addu} |
| `remove` | range-v3 | [Tier 1]{.addu} |
| `remove_if` | range-v3 | [Tier 1]{.addu} |
| `repeat` | C++23 | -- |
| `repeat_n` | C++23 (under the name `repeat`) | -- |
| `replace` | range-v3 | [Tier 1]{.addu} |
| `replace_if` | range-v3 | [Tier 1]{.addu} |
| `reverse` | C++20 | -- |
| `sample` | range-v3 | [Tier 3]{.diffdel} |
| `scan` | (not in range-v3) | [Tier 1, as a rename of what is `partial_sum` in range-v3]{.addu} |
| `set_difference` | range-v3 | [Tier 3]{.diffdel} |
| `set_intersection` | range-v3 | [Tier 3]{.diffdel} |
| `set_union` | range-v3 | [Tier 3]{.diffdel} |
| `set_symmetric_difference` | range-v3 | [Tier 3]{.diffdel} |
| `single` | C++20 | -- |
| `slice` | range-v3 | [Tier 1]{.addu} |
| `sliding` | C++23 (as `slide`) | -- |
| `split` | C++20 (improved) | -- |
| `split_when` | range-v3 | [Tier 2]{.yellow} |
| `stride` | C++23 | -- |
| `tail` | range-v3 | [Tier 2]{.yellow} |
| `take` | C++20 | -- |
| `take_exactly` | range-v3 | [Tier 2]{.yellow} |
| `take_last` | range-v3 | [Tier 1]{.addu} |
| `take_last_while` | (not in range-v3) | [Tier 1]{.addu} |
| `take_while` | C++20 | -- |
| `tokenize` | range-v3 | Not proposed |
| `transform_filter` | (not in range-v3) | [Tier 1, related to `views::maybe` [@P1255R9] ]{.addu} |
| `trim` | range-v3 | [Tier 1]{.addu} |
| `unbounded` | range-v3 | Not proposed |
| `unique` | range-v3 | [Tier 1]{.addu} |
| `values` | C++20 | -- |
| `zip` | C++23 | -- |
| `zip_with` | C++23 | -- |
