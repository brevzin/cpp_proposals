---
title: "`ranges::fold`"
document: P2322R1
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

[@P2322R0] used `regular_invocable` as the constraint in the `@*foldable*@` concept, but we don't need that for this algorithm and it prohibits reasonable uses like a mutating operation. `invocable` is the sufficient constraint here (in the same way that it is for `for_each`). Also restructured the API to overload on providing the initial value instead of having differently named algorithms. 

# Introduction

As described in [@P2214R0], there is one very important rangified algorithm missing from the standard library: `fold`. 

While we do have an iterator-based version of `fold` in the standard library, it is currently named `accumulate`, defaults to performing `+` on its operands, and is found in the header `<numeric>`. But `fold` is much more than addition, so as described in the linked paper, it's important to give it the more generic name and to avoid a default operator.

Also as described in the linked paper, it is important to avoid over-constraining `fold` in a way that prevents using it for heterogeneous folds. As such, the `fold` specified in this paper only requires one particular invocation of the binary operator and there is no `common_reference` requirement between any of the types involved.

Lastly, the `fold` here is proposed to go into `<algorithm>` rather than `<numeric>` since there is nothing especially numeric about it.

# Other `fold` algorithms

[@P2214R0] proposed a single fold algorithm that takes an initial value and a binary operation and performs a _left_ fold over the range. But there are a couple variants that are also quite valuable and that we should adopt as a family.

## `fold_first`

Sometimes, there is no good choice for the initial value of the fold and you want to use the first element of the range. For instance, if I want to find the smallest string in a range, I can already do that as `ranges::min(r)` but the only way to express this in terms of `fold` is to manually pull out the first element, like so:

```cpp
auto b = ranges::begin(r);
auto e = ranges::end(r);
ranges::fold(ranges::next(b), e, *b, ranges::min);
```

But this is both tedious to write, and subtly wrong for input ranges anyway since if the `next(b)` is evaluated before `*b`, we have a dangling iterator. This comes up enough that this paper proposes a version of `fold` that uses the first element in the range as the initial value (and thus has a precondition that the range is not empty).

This algorithm exists in Rust (under the name `fold_first` as a nightly-only experimental API and `fold1` in the `Itertools` crate) and Haskell (under the name `foldl1`).

The question is: should we give this algorithm a different name (e.g. `fold_first`) or provide a distinct overload of `fold`? To answer that question, we have to deal with the question of ambiguity. For two arguments, `fold(xs, a)` can only be interpreted as a `fold` with no initial value using `a` as the binary operator. For four arguments, `fold(xs, a, b, c)` can only be interpreted as a `fold` with `a` as the initial value, `b` as the binary operation that is the reduction function, and `c` as a unary projection.

What about `fold(xs, a, b)`?  It could be:

1. Using `a` as the initial value and `b` as a binary reduction of the form `(A, X) -> A`. 
2. Using `a` as a binary reduction of the form `(X, Y) -> X` and `b` as a unary projection of the form `X -> Y`.

Is it possible for these to collide? It would be an uncommon situation, since `b` would have to be both a unary and a binary function. But it is definitely _possible_:

```cpp
inline constexpr auto first = [](auto x, auto... ){ return x; };
auto r = fold(xs, first, first);
```

This works with either interpretation. It either just returns `first` (the lambda) in the first case or the first element of the range in the second case, which makes it either completely useless or just mostly useless.

There might be a situation that is actually useful in which there is an ambiguity in these cases. But if one arises, it is fairly straightforward to force the correct interpretation by coercing the last argument to be either a binary or unary function:

```cpp
#define FWD(x) static_cast<decltype(x)&&>(x)
#define RETURNS(e) -> decltype((e)) { return e; }
inline constexpr auto as_unary = [](auto f){ return [=](auto&& x) RETURNS(f(FWD(x))); };
inline constexpr auto as_binary = [](auto f){ return [=](auto&& x, auto&& y) RETURNS(f(FWD(x), FWD(y))); };

fold(xs, first, as_binary(first)); // definitely interpretation #1
fold(xs, first, as_unary(first));  // definitely interpretation #2
```

As such, this paper proposes an overload for `fold` that take no initial value (and have a precondition that the range is non-empty) rather than introducing a different name for this case.

## `fold_right`

While `ranges::fold` would be a left-fold, there is also occasionally the need for a _right_-fold. While a `fold_right` is much easier to write in code given `fold` than `fold_first`, since `fold_right(r, init, op)` is `fold(r | views::reverse, init, flip(op))`, it's sufficiently common that it may as well be in the standard library.

As with the previous section, we should also provide overloads of `fold_right` that do not take an initial value.

There are three questions that would need to be asked about `fold_right`.

First, the order of operations of to the function. Given `fold_right([1, 2, 3], z, f)`, is the evaluation `f(1, f(2, f(3, z)))` or is the evaluation `f(f(f(z, 3), 2), 1)`? Note that either way, we're choosing the `3` then `2` then `1`, both are right folds. It's a question of if the initial element is the left-hand operand (as it is in the left `fold`) or the right-hand operand (as it would be if consider the right fold as a flip of the left fold). For instance, Scheme picks the former but Haskell picks the latter.

One advantage of the former is that we can specify `fold_right(r, z, op)` as precisely `fold_left(views::reverse(r), z, op)` and leave it at that. With the latter, we would need need slightly more specification and would want to avoid saying `flip(op)` since directly invoking the operation with the arguments in the correct order is a little better in the case of ranges of prvalues. 

This paper picks the latter (that is `fold_right` as the order of arguments flipped from `fold`).

Second, supporting bidirectional ranges is straightforward. Supporting forward ranges involves recursion of the size of the range. Supporting input ranges involves recursion and also copying the whole range first. Are either of these worth supporting? The paper simply supports bidirectional ranges. 

Third, the naming question.

## Naming

There are roughly three different choices that we could make here:

1. Provide the algorithms `fold` (a left-fold) and `fold_right`.
2. Provide the algorithms `fold_left` and `fold_right`.
3. Provide the algorithms `fold_left` and `fold_right` and also provide an alias `fold` which is also `fold_left`.

Left-folds are more common than right-folds. Some languages only provide a left-fold operation (though that's because those languages don't have a concept of bidirectional iteration) and name it simply `fold` (Rust) or `reduce` (Python). But other languages that provide both do provide a suffix on both, whether Haskell/Elm (`foldl` and `foldr`) or OCaml (`fold_left` and `fold_right`). Scala provides a `foldLeft` and `foldRight` while also providing a `fold` which means `foldLeft`.

We don't have precedent in the library at this point for providing an alias for an algorithm, although we do have precedent in the library for providing an alias for a range adapter (`keys` and `values` for `elements<0>` and `elements<1>`, and [@P2321R0] proposes `pairwise` and `pairwise_transform` as aliases for `adjacent<2>` and `adjacent_transform<2>`). We also have precedent in the library for asymmetric names (`sort` vs `stable_sort` vs `partial_sort`), although those algorithms are not as symmetric as `fold_left` and `fold_right`... while we do also have `shift_left` and `shift_right`.

All of which is to say, I don't think there's a clear answer to this question. I would be quite happy with any of the three options. This paper picks (2). 

# Wording

Append to [algorithm.syn]{.sref}:

::: bq
```cpp
#include <initializer_list>

namespace std {
  // ...
    
  // [alg.fold], folds
  namespace ranges {
    template<class LHS, class RHS>
    concept @*weakly-assignable-from*@ =          // exposition only
      requires(LHS lhs, RHS&& rhs) {
        { lhs = std::forward<RHS>(rhs); } -> same_as<LHS>;
      };
      
    template<class F, class R, class... Args>
    concept @*foldable*@ =                        // exposition only
      movable<R> &&
      copy_constructible<F> &&
      invocable<F&, Args...> &&
      @*weakly-assignable-from*@<R&, invoke_result_t<F&, Args...>>;
  
    template<class F, class T, class I>
    concept @*indirectly-binary-left-foldable*@ = // exposition only
      indirectly_readable<I> &&
      @*foldable*@<F, T, T, iter_reference_t<I>>;
      
    template<class F, class T, class I>
    concept @*indirectly-binary-right-foldable*@ = // exposition only
      indirectly_readable<I> &&
      @*foldable*@<F, T, iter_reference_t<I>, T>;      

    template<input_iterator I, sentinel_for<I> S, class T, class Proj = identity,
      @*indirectly-binary-left-foldable*@<T, projected<I, Proj>> BinaryOperation>
    constexpr T fold_left(I first, S last, T init, BinaryOperation binary_op, Proj proj = {});

    template<input_range R, class T, class Proj = identity,
      @*indirectly-binary-left-foldable*@<T, projected<iterator_t<R>, Proj>> BinaryOperation>
    constexpr T fold_left(R&& r, T init, BinaryOperation binary_op, Proj proj = {});

    template <input_iterator I, sentinel_for<I> S, class Proj = identity,
      @*indirectly-binary-left-foldable*@<iter_value_t<I>, projected<I, Proj>> BinaryOperation>
      requires constructible_from<iter_value_t<I>, iter_reference_t<I>>
    constexpr iter_value_t<I> fold_left(I first, S last, BinaryOperation binary_op, Proj proj = {});

    template <input_range R, class Proj = identity,
      @*indirectly-binary-left-foldable*@<range_value_t<R>, projected<iterator_t<R>, Proj>> BinaryOperation>
      requires constructible_from<range_value_t<I>, range_reference_t<I>>
    constexpr range_value_t<R> fold_left(R&& r, BinaryOperation binary_op, Proj proj = {});
    
    template<bidirectional_iterator I, sentinel_for<I> S, class T, class Proj = identity,
      @*indirectly-binary-right-foldable*@<T, projected<I, Proj>> BinaryOperation>
    constexpr T fold_right(I first, S last, T init, BinaryOperation binary_op, Proj proj = {});

    template<bidirectional_range R, class T, class Proj = identity,
      @*indirectly-binary-right-foldable*@<T, projected<iterator_t<R>, Proj>> BinaryOperation>
    constexpr T fold_right(R&& r, T init, BinaryOperation binary_op, Proj proj = {});

    template <bidirectional_iterator I, sentinel_for<I> S, class Proj = identity,
      @*indirectly-binary-right-foldable*@<iter_value_t<I>, projected<I, Proj>> BinaryOperation>
      requires constructible_from<iter_value_t<I>, iter_reference_t<I>>
    constexpr iter_value_t<I> fold_right(I first, S last, BinaryOperation binary_op, Proj proj = {});

    template <bidirectional_range R, class Proj = identity,
      @*indirectly-binary-right-foldable*@<range_value_t<R>, projected<iterator_t<R>, Proj>> BinaryOperation>
      requires constructible_from<range_value_t<I>, range_reference_t<I>>
    constexpr range_value_t<R> fold_right(R&& r, BinaryOperation binary_op, Proj proj = {});    
  }
}
```
:::

And add a new clause, [alg.fold]:

::: bq
```cpp
template<input_iterator I, sentinel_for<I> S, class T, class Proj = identity,
  @*indirectly-binary-left-foldable*@<T, projected<I, Proj>> BinaryOperation>
constexpr T ranges::fold_left(I first, S last, T init, BinaryOperation binary_op, Proj proj = {});

template<input_range R, class T, class Proj = identity,
  @*indirectly-binary-left-foldable*@<T, projected<iterator_t<R>, Proj>> BinaryOperation>
constexpr T ranges::fold_left(R&& r, T init, BinaryOperation binary_op, Proj proj = {});
```

[1]{.pnum} *Effects*: Equivalent to:

::: bq
```cpp
while (first != last) {
    init = invoke(binary_op, std::move(init), invoke(proj, *first));
    ++first;
}
return init;
```
:::

```cpp
template <input_iterator I, sentinel_for<I> S, class Proj = identity,
  @*indirectly-binary-left-foldable*@<iter_value_t<I>, projected<I, Proj>> BinaryOperation>
  requires constructible_from<iter_value_t<I>, iter_reference_t<I>>
constexpr iter_value_t<I> ranges::fold_left(I first, S last, BinaryOperation binary_op, Proj proj = {});

template <input_range R, class Proj = identity,
  @*indirectly-binary-left-foldable*@<range_value_t<R>, projected<iterator_t<R>, Proj>> BinaryOperation>
  requires constructible_from<range_value_t<I>, range_reference_t<I>>
constexpr range_value_t<R> ranges::fold_left(R&& r, BinaryOperation binary_op, Proj proj = {});
```

[2]{.pnum} *Preconditions*: `first != last` is `true`.

[3]{.pnum} *Effects*: Equivalent to `return ranges::fold_left(ranges::next(std::move(first)), std::move(last), iter_value_t<I>(*first), binary_op, proj)` except ensuring that the initial value is constructed before the iterator is incremented if `first` is an input iterator.

```cpp
template<bidirectional_iterator I, sentinel_for<I> S, class T, class Proj = identity,
  @*indirectly-binary-right-foldable*@<T, projected<I, Proj>> BinaryOperation>
constexpr T ranges::fold_right(I first, S last, T init, BinaryOperation binary_op, Proj proj = {});

template<bidirectional_range R, class T, class Proj = identity,
  @*indirectly-binary-right-foldable*@<T, projected<iterator_t<R>, Proj>> BinaryOperation>
constexpr T ranges::fold_right(R&& r, T init, BinaryOperation binary_op, Proj proj = {});
```

[4]{.pnum} *Effects*: Equivalent to:

::: bq
```cpp
I tail = ranges::next(first, last);
while (first != tail) {
    init = invoke(binary_op, invoke(proj, *--tail), std::move(init));
}
return init;
```
:::

```cpp
template <bidirectional_iterator I, sentinel_for<I> S, class Proj = identity,
  @*indirectly-binary-right-foldable*@<iter_value_t<I>, projected<I, Proj>> BinaryOperation>
  requires constructible_from<iter_value_t<I>, iter_reference_t<I>>
constexpr iter_value_t<I> fold_right(I first, S last, BinaryOperation binary_op, Proj proj = {});

template <bidirectional_range R, class Proj = identity,
  @*indirectly-binary-right-foldable*@<range_value_t<R>, projected<iterator_t<R>, Proj>> BinaryOperation>
  requires constructible_from<range_value_t<I>, range_reference_t<I>>
constexpr range_value_t<R> fold_right(R&& r, BinaryOperation binary_op, Proj proj = {});  
```

[5]{.pnum} *Preconditions*: `first != last` is `true`.

[6]{.pnum} *Effects*: Equivalent to:

::: bq
```cpp
I tail = ranges::prev(ranges::next(first, std::move(last));
return ranges::fold_right(std::move(first), tail, iter_value_t<I>(*tail), binary_op, proj);
```
:::
:::
