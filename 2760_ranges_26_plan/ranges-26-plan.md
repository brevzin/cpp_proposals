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

## `cache_last`

One of the adapters that we considered for C++23 but ended up not pursuing was what range-v3 calls `cache1` and what we'd instead like to call something like `cache_last`. This is an adapter which, as the name suggests, caches the last element. The reason for this is efficiency - specifically avoiding extra work that has to be done by iterator dereferencing.

The canonical example of this is `transform(f) | filter(g)`, where if you then iterate over the subsequent range, `f` will be invoked twice for every element that satisfies `g`:

::: bq
```cpp
int main()
{
    std::vector<int> v = {1, 2, 3, 4, 5};

    auto even_squares = v
        | std::views::transform([](int i){
                std::print("transform: {}\n", i);
                return i * i;
            })
        | std::views::filter([](int i){
                std::print("filter: {}\n", i);
                return i % 2 == 0;
            });

    for (int i : even_squares) {
        std::print("Got: {}\n", i);
    }
}
```
:::

prints the following (note that there are 7 invocations of `transform`):

::: bq
```
transform: 1
filter: 1
transform: 2
filter: 4
transform: 2
Got: 4
transform: 3
filter: 9
transform: 4
filter: 16
transform: 4
Got: 16
transform: 5
filter: 25
```
:::

The solution here is to add a layer of caching:

::: bq
```cpp
auto even_squares = v
    | views::transform(square)
    | views::cache_last
    | views::filter(is_even);
```
:::

Which will ensure that `square` will only be called once per element.

The tricky part here is: how do you implement `cache_last`? Specifically: in what member function do you perform the caching?

The range-v3 implementation looks roughly like this:

::: bq
```cpp
template <view V>
struct cache_last_view {
    V base_;
    bool dirty_ = true;
    $non-propagating-cache$<range_value_t<V>> cache_;

    struct $iterator$ {
        cache_last_view* parent_;
        iterator_t<V> cur_;

        auto operator*() const -> range_value_t<V>&& {
            if (parent_->dirty_) {
                parent_->cache_.emplace(iter_move(cur_));
                parent_->dirty_ = false;
            }
            return std::move(*parent_->cache_);
        }

        auto operator++() -> $iterator$& {
            ++cur_;
            parent_->dirty_ = true;
        }
    };
};
```
:::

But there's a problem here: [res.on.data.races]{.sref} says that `const` member functions are not allowed to introduce data races. While everything here is `const`-correct (there isn't even a `mutable`), iterator dereference here _does_ introduce a data race: two threads were both dereferencing an iterator into a dirty `cache_last_view`.

There are four potential solutions to this problem, presented in our order of preference:

1. We could carve out an exception to [res.on.data.races] for all input iterators. Even some standard library implementations of input iterators (like `std::istreambuf_iterator<char>`) already don't satisfy this, and using input iterators in multi-threaded contexts is already kind of interesting. This makes the above implementation valid.
2. We could require synchronization on `operator*() const`. This probably isn't terrible expensive in this context, but adding synchronization to an adaptor whose primary purpose is to improve performance seems a bit heavy-handed, especially since that synchronization will almost never be actually necessary.
3. We could move the updating of the cached value from `operator*() const` to `operator++()`, which is already a mutable member function. This has the downside of requiring calculating more elements than necessary - since `r | cache_last | stride(2)` will still have to cache every element, even if only every other one is necessary.
4. We could allow input iterators to have _mutable_ `operator*()`, since some of them clearly need it. A mutable `operator*()` makes the concepts even more awkward, and adds more work for every range adaptor. It theoretically is sensible, but seems extremely impractical.

The other issue is what the reference type of the range should be. range-v3 uses `range_value_t<V>&&`, but this somewhat defeats the purpose of caching if you can so easily invalidate it. `range_value_t<V>&` is probably a better choice.

## `scan`

If you want to take a range of elements and get a new range that is applying `f` to every element, that's `transform(f)`. But there are many cases where you need a `transform` to that is _stateful_. That is, rather than have the input to `f` be the current element (and require that `f` be `regular_invocable`), have the input to `f` be both the current element _and_ the current state.

For instance, given the range `[1, 2, 3, 4, 5]`, if you want to produce the range `[1, 3, 6, 10, 15]` - you can't get there with `transform`. Instead, you need to use `scan` using `+` as the binary operator. The special case of `scan` over `+` is `partial_sum`.

One consideration here is how to process the first element. You might want `[1, 3, 6, 10, 15]` and you might want `[0, 1, 3, 6, 10, 15]` (with one extra element), the latter could be called a `prescan`.

## `generate`

C++23 has `std::generator<T>`. There are two very closely related range factories in range-v3, which are basically:

::: bq
```cpp
template <class F>
    requires std::invocable<F&>
auto generate(F f) -> std::generator<std::invoke_result_t<F&>> {
    while (true) {
        co_yield f();
    }
}

template <class F>
    requires std::invocable<F&>
auto generate_n(F f, int n) -> std::generator<std::invoke_result_t<F&>> {
    for (int i = 0; i != n; ++i) {
        co_yield f();
    }
}
```
:::

Note that the constraint here is `invocable`, not `regular_invocable`. The latter wouldn't be very interesting - that's `views::repeat(f())`. These factories are somewhat related to `scan` (in the sense that we have a mutable function that we're repeatedly invoking) and also somewhat related to `cache_latest` (in the sense that the range-v3 implementation of both also violate [res.on.data.races]).

Since with `views::repeat`, we just used the same name for the infinite and finite versions, we should probably end up with just the one name for `views::generate`.

A similar factory in this vein is one that Haskell calls `iterate`:

::: bq
```cpp
template <class F, class T>
auto iterate(F f, T x) -> std::generator<T> {
    while (true) {
        co_yield x;
        x = f(x);
    }
}
```
:::

Whereas `generate(f)` is the sequence `[f(), f(), f(), f(), ...]`, `iterate(f, x)` is the sequence `[x, f(x), f(f(x)), f(f(f(x))), ...]`

# View Adjuncts

In the C++23 plan, we listed several facilities that would greatly improve the usability of views: the ability for users to define first class pipe support, the ability to collect into a container (`ranges::to`), and formatting.

There are some other operations that we've seen come up regularly - operations that are not themselves views or algorithms, but would improve the quality of life around using the standard library (and other) range adpators.

## More Function Objects

The standard library has a lot of function objects, but there are still plenty of common ones that are missing.

Some unary operators have no associated function object:

* indirection: `*_1`
* addressof: `&_1` (except if we add a function object for this, it should do `std::addressof`)
* prefix and postfix increment: `++_1` or `_1++`
* prefix and postfix decrement: `--_1` or `_1--`

range-v3 has `views::indirect`, for instance, which is basically an over-constrained `views::transform(*_1)`.

Some binary operators have no associated function object:

* the shifts: `_1 << _2` and `_1 >> _2`
* all the compound assignments: `_1 += _2`, etc.

The various language cases also have no associated function object. The most common of these is `static_cast<T>(_1)`.

It is also worth considering whether we should actually add function objects for these, like `std::indirect` (or `std::ranges::indirect`?) or whether we should try to bring back one of the earlier proposals that added nicer syntax for passing operators as function objects:

* [@P0119R2]: `views::transform((*))`
* [@P0834R0]: `views::transform([] *)`
* use backticks: ``views::transform(`*`)``

## More Function Adapters

The standard library doesn't have very many function adapters. There are two particularly notable ones that seem to come up frequently.

* function composition: an adapter `compose` such that `compose(f, g)(x...) == f(g(x...))`
* function projection: an adapter `proj` such that `proj(p, f)(x...) == f(p(x)...)`

If we had a `proj` adapter, people wouldn't need to ask for range adapters to support projections - they could just provide one.

The difficulty with these is that both are syntactically heavy in C++, because our lambdas are verbose and we have difficulties passing functions around (see the two papers noted in the previous section).

The other problem is that these adapters don't really have obvious ordering. Should `compose(f, g)(x)` mean `f(g(x))` or `g(f(x))`? There's good arguments for either. The same is true for `proj` (which is sometimes also called `on`).

# Algorithms

We improved the Ranges story on algorithms quite a bit in C++23 - both in terms of new and existing algorithms. But there's a few more pretty interesting ones left on the table.

## `reduce`

We talked about [`reduce`](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2214r2.html#rangesreduce) in [@P2214R2]. `ranges::reduce` is a version of `ranges::fold_left` ([@P2322R6]) that is parallelizable. It requires the binary operation to be associative (to allow chunks of the range to be reduced in praallel) and commutative (to allow those chunks to be arbitrarily combined). So we will need to figure out what constraints to add on this algorithm (see [@P1813R0]) as well as how we determine what the return type is (see [this section](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2322r6.html#return-type) discussing the same problem for `ranges::fold_left`).

One thing is clear: `ranges::reduce` should _not_ take a default binary operation _nor_ a default initial parameter. The user needs to supply both.

However, for convenience, we do propose providing `ranges::sum(r)` as `ranges::reduce(r, plus{}, range_value_t<R>())` and `ranges::product(r)` as `ranges::reduce(r, multiplies{}, range_value_t<R>(1))`.

Note that naming is a problem here: some languages (Rust, Scala, Kotlin) have an algorithm that takes an initial value named `fold` and an algorithm that takes no initial value and returns and optional `reduce`. In C++23, we called these `fold_left` and `fold_left_first` since we've already had `std::reduce` since C++17.

But since our `reduce` differs from our `fold` not based on initial element but rather on operation requirements, it also leaves open the question for whether there should be a `reduce_first`. A good example there might be using `std::max` as the reduction operator - which is both associative and commutative, but for some types may not have an obvious choice for the minimum.
