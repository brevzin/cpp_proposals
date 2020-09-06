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
| `tail` | range-v3 | [Tier 1, largely for `zip_tail`]{.addu} |
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

## The `zip` family

The `zip` family of range adapters (`enumerate`, `zip`, `zip_with`, and `zip_tail` -- the latter also known as `pairwise`) is an extremely useful set of adapters, with broad applicability. 

Indeed, we have many algorithms that exist largely because we don't have `zip` (and prior to Ranges, didn't have the ability to compose algorithms). We have several algorithms that have single-range-unary-function and a two-range-binary-function flavors. What if you wanted to `ranges::transform` 3 ranges? Out of luck. But in all of these cases, the two-range-binary-function flavor could be written as a single-range that is a `zip` of the two ranges, `adjacent_difference` is `zip_tail` followed by `transform`, `inner_product` is `zip` followed by `accumulate`, and so on and so on. This is why we think `zip` is the top priority view.

The most generic of this group is actually `iter_zip_with`. We're not proposing this for inclusion as it doesn't come up much in user facing code, but it is the core functionality for the family. `iter_zip_with(f, rs...)` takes an `n`-ary invocable an `n` ranges and combines those into a single range whose values are the result of `f(is...)`, where the `is` are the iterators into those ranges (note: iterators, not what they refer to). The size of an `iter_zip_with` is the minimum size of its ranges. 

We can implement the adapters we want on top of `iter_zip_with` easily (there's a minor detail about how we want `value_type` and `reference` to behave, but beyond that, these are basically straightforward equivalences):

- `zip_with` is `iter_zip_with` except first dereferencing all the iterators into the provided callable.
- `zip(rs...)` is `iter_zip_with([](Is... is) { return tuple<range_reference_t<Rs>...>(*is...); }, rs...)`. Can't implement `zip` in terms of `zip_with` because zipping a range that produces prvalues should yield a tuple of values, not a tuple of references, and `zip_with` would lose that distinction. But you can think of `zip(rs...)` as `zip_with(make_tuple, rs...)`, except in a way that properly preserves references. 
- `enumerate(r)` is `zip(iota(range_size_t<R>(0)), r)`.
- `zip_tail(r)` is `zip(r, r | tail)`, which is `zip(r, r | drop(1))`
- `zip_tail_with(f, r)` is `zip_with(f, r, r | tail)`, which is `zip_with(f, r, r | drop(1))`

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
- the addition of the following range adapters:
    - `views::cache1`
    - `views::enumerate`
    - `views::filter_map`
    - `views::flat_map`
    - `views::group_by`
    - `views::group_by_key`
    - `views::join_with`
    - `views::tail`
    - `views::zip`
    - `views::zip_with`
    - `views::zip_tail`
    - `views::zip_tail_with`
- the addition of the following range algorithms:
    - `ranges::fold()`

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
      issued: 2014
      URL: https://github.com/ericniebler/range-v3/
    - id: stepanov
      citation-label: stepanov
      title: From Mathematics to Generic Programming
      author:
        - family: Alexander A. Stepanov
      issued: 2014
---
