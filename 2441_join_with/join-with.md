---
title: "`views::join_with`"
document: P2441R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

C++20 Ranges introduced `views::join`: a range adaptor for turning a range of range of `T` into a range of `T`. This adaptor, however, did not take any delimiter range. So you could not, for instance, join together a range of `string`s with a space to then convert that to a longer string. This paper remedies that by producing a `views::join_with`, as described in [@P2214R1], section 3.8.

The behavior of `views::join_with` is an inverse of `views::split`. That is, given a range `r` and a pattern `p`, `r | views::split(p) | views::join_with(p)` should yield a range consisting of the same elements as `r`.

# Design

There are several aspects to `join_with` that are more complicated than `join`, that are worth going over. For convenience, let `Rng` denote the range we're `join`ing (i.e. the range of ranges), let `Pattern` denote the delimiter pattern, and let `Inner` denote the inner range (i.e. `range_reference_t<Rng>`).

## Naming

Why `views::join_with` rather than supporting an extra argument to `views::join`? The issue here is ultimately ambiguity. In this potential design where we simply overload `join`, `views::join(x)` could mean either (a) produce a range that joins the ranges of `x`, or (b) it is a partial call that produces a range adaptor closure object to where `x` is a delimiter for some other range to be provided in the future.

This ambiguity is... rare. `x` needs to be a range of ranges (i.e. `[[T]]`) in order to be joinable, which means that in order for it to be a delimiter, the range it's doing needs to be a range of range of ranges (i.e. `[[[T]]]`). These don't come up very often. range-v3 simply treats `join(x)` if `x` is a joinable range (i.e. a range whose reference type is also a range) a request to produce a `join_view`. That's one option. 

However, eventually somebody is going to want to join a `[[[T]]]` on a `[[T]]` and it will end up just not working. 

Regardless, joining with a delimiter will produce a different view than joining without a delimiter (it's `r | views::join` does not produce a `join_with_view<all_t<R>, empty_view<T>>`, that would be needlessly inefficient... it just produces a `join_with_view<all_t<R>>`), so we don't gain anything on either the implementation front or the specification front by overloading `join`. The only difference of introducing a new name would be that users would have to write `r | views::join_with(' ')` instead of `r | views::join(' ')`. That doesn't seem like an unreasonable burden on users, at the cost of avoiding any future ambiguity.

## Category

Like `views::join`, and following [@P2328R1], `views::join_with` is input-only if the primary range (the one we're `join`ing) is a range of prvalue non-view ranges.

Otherwise, if `Rng`, `Inner`, and `Pattern` are all bidirectional and `Inner` and `Pattern` are common, then bidirectional. Otherwise, if `Rng`, `Inner`, and `Pattern` are all forward. Otherwise, input. 

## Value and Reference Type

For `views::join`, these are obvious: `Inner`'s value and reference, respectively. But now, we have two ranges that we're joining together: `Inner` and `Pattern`, an those two may have different value and reference types. range-v3's implementation allows this, using the folloiwng formation:

```cpp
using value_type = common_type_t<range_value_t<Inner>, range_value_t<Pattern>>;
using reference = common_reference_t<range_reference_t<Inner>, range_reference_t<Pattern>>;
using rvalue_reference = common_reference_t<range_rvalue_reference_t<Inner>, range_rvalue_reference_t<Pattern>>;
```

Those types all existing is an added constraint on constructing a `join_with_view`. 

I'm not sure I see a need to deviate from the implementation here. 

## Conditionally Common

Like `views::join`, `views::join_with` can be common when `Rng` is a range of glvalue ranges, `Rng` and `Inner` are both forward and common. Note that we don't need `Pattern` to be common.

## Borrowed

Never.

## Sized

Never.

## Implementation Experience

Up until recently, the `join_with_view` in range-v3 was input-only, never common, and never const-iterable. But I have since implemented this paper without issue.

# Wording

## Addition to `<ranges>`

Add the following to [ranges.syn]{.sref}, header `<ranges>` synopsis:

::: bq
```cpp
// [...]
namespace std::ranges {
  // [...]

  // [range.join.with], join with view
  template<class R, class P>
    concept $compatible-joinable-ranges$ = $see below$; // exposition only
  
  template<input_range V, forward_range Pattern>
    requires view<V>
          && input_range<range_reference_t<V>>
          && view<Pattern>
          && $compatible-joinable-ranges$<range_reference_t<V>, Pattern>
  class join_with_view;

  namespace views {
    inline constexpr $unspecified$ join_with = $unspecified$;
  }
}
```
:::

## `join_with`

Add the following subclause to [range.adaptors]{.sref}.

### 24.7.? Join with view [range.join.with]

#### 24.7.?.1  Overview [range.join.with.overview]

::: bq
[#]{.pnum} `join_with_view` takes a `view` and a delimiter, and flattens the `view`, inserting every element of the delimiter in between elements of the `view`. The delimiter can be a single element or a `view` of elements.

[#]{.pnum} The name `views::join_with` denotes a range adaptor object ([range.adaptor.object]). Given subexpressions `E` and `F`, the expression `views::join_with(E, F)` is expression-equivalent to `join_with_view{E, F}`.

[#]{.pnum} [*Example*:

```cpp
vector<string> vs = {"the", "quick", "brown", "fox"};
for (char c : vs | join_with(' ')) {
    cout << c;
}
// the above prints: the quick brown fox
```

-*end example*]
:::

---
references:
    - id: P2214R1
      citation-label: P2214R1
      title: "A Plan for C++23 Ranges"
      author:
        - family: Barry Revzin
        - family: Conor Hoekstra
        - family: Tim Song
      issued:
        year: 2021
      URL: https://wg21.link/p2214r1
---
