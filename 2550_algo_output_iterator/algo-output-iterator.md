---
title: "`ranges::copy` should say `output_iterator` somewhere"
document: P2550R0
date: today
audience: LWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

In the beginning, we had:

::: bq
```cpp
template<class InputIterator, class OutputIterator>
  constexpr OutputIterator copy(InputIterator first, InputIterator last,
                                OutputIterator result);
```
:::

And then, in C++20, we added Concepts and Ranges - which came with a whole library of `concept`s for the standard library, in particular for iterators and ragnes. This included a concept `input_iterator` and `output_iterator`. We also added new rangified versions of all the algorithms, constrained using these concepts.

The new overloads look like this:

::: bq
```cpp
template<input_iterator I, sentinel_for<I> S, weakly_incrementable O>
  requires indirectly_copyable<I, O>
  constexpr ranges::copy_result<I, O> ranges::copy(I first, S last, O result);
template<input_range R, weakly_incrementable O>
  requires indirectly_copyable<iterator_t<R>, O>
  constexpr ranges::copy_result<borrowed_iterator_t<R>, O> ranges::copy(R&& r, O result);
```
:::

The crux of this paper is that `std::copy` takes an `OutputIterator` (even if the name of this parameter does nothing), but `std::ranges::copy` does not have anything named `output_iterator` in this constraints at all. That just seems wrong. Output iterator is a thing that users understand, but "weakly incrementable" and "indirectly copyable," while reasonable names for the functionality they require, are not particularly well known and are not really useful when they show up in diagnostics.

We should do better here.

## Output concept hierarchy

Let me start with what all the relevant `concept`s actually are:

::: bq
```cpp
template<class I>
  concept weakly_incrementable =
    movable<I> &&
    requires(I i) {
      typename iter_difference_t<I>;
      requires $is-signed-integer-like$<iter_difference_t<I>>;
      { ++i } -> same_as<I&>;   // not required to be equality-preserving
      i++;                      // not required to be equality-preserving
    };

template<class I>
  concept input_or_output_iterator =
    requires(I i) {
      { *i } -> $can-reference$;
    } &&
    weakly_incrementable<I>;

template<class Out, class T>
  concept indirectly_writable =
    requires(Out&& o, T&& t) {
      *o = std::forward<T>(t);  // not required to be equality-preserving
      *std::forward<Out>(o) = std::forward<T>(t);   // not required to be equality-preserving
      const_cast<const iter_reference_t<Out>&&>(*o) =
        std::forward<T>(t);     // not required to be equality-preserving
      const_cast<const iter_reference_t<Out>&&>(*std::forward<Out>(o)) =
        std::forward<T>(t);     // not required to be equality-preserving
    };

template<class In, class Out>
  concept indirectly_copyable =
    indirectly_readable<In> &&
    indirectly_writable<Out, iter_reference_t<In>>;

template<class I, class T>
  concept output_iterator =
    input_or_output_iterator<I> &&
    indirectly_writable<I, T> &&
    requires(I i, T&& t) {
      *i++ = std::forward<T>(t);        // not required to be equality-preserving
    };
```
:::

And let me present two possible specifications for `ranges::copy`.

::: cmptable
### In C++20
```cpp
template <input_iterator I,
          sentinel_for<I> S,
          weakly_incrementable O>
  requires indirectly_copyable<I, O>
constexpr ranges::copy_result<I, O>
ranges::copy(I first, S last, O result);
```

### Hypothetical
```cpp
template <input_iterator I,
          sentinel_for<I> S,
          output_iterator<iter_reference_t<I>> O>
constexpr ranges::copy_result<I, O>
ranges::copy(I first, S last, O result);
```
:::

Now, the one on the right actually says `output_iterator`, which I think is extremely valuable. But are the requirements any different?

In C++20, we require:

* `O` is `weakly_incrementable`
* `indirectly_copyable<I, O>`, which means `I` is `indirectly_readable` (which is already required by `input_iterator`) and `O` is `indirectly_writable<iter_reference_t<I>>`

The hypothetical version requires `output_iterator<iter_reference_t<I>>`, which breaks down into:

* `input_or_output_iterator`
  * dereferencable (already required by `indirectly_writable`, since you have to have `*o = expr;` work)
  * `weakly_incrementable` (explicitly required in C++20)
* `indirectly_writable` (explicitly required in C++20)
* `*i++ = t;` (_not_ required in C++20)

Basically: the two formulations have identical requirements _except_ that today's specification of `std::ranges::copy` does not require `*out++ = t;` to work, while my hypothetical one does. Generally speaking, this is a good thing. It may be syntactically nice to write `*out++ = t;` instead of `*out = t; ++out;`, but it's not actually necessary to solve any problems. This makes `ranges::copy` more usable, but it means that our most output-y of output algorithms doesn't use `output_iterator`.

## Does anything in Ranges use `output_iterator`?

I thought it'd be useful to go through everything in `<algorithm>` that uses an output iterator (if not an `output_iterator`) and catalogue all the kinds of constraints we have on them. There are many different approaches (in the below, `X` just denotes some type, `O` is our output iterator, `I` is the corresponding input iterator):

* `weakly_incrementable<O> && indirectly_copyable<I, O>`: `ranges::copy`, `ranges::copy_n`, `ranges::copy_if`, `ranges::remove_copy`, `ranges::remove_copy_if`, `ranges::unique_copy` (although this one has a disjunction that might include other constraints), `ranges::reverse_copy`, `ranges::rotate_copy`, `ranges::partition_copy`
* `weakly_incrementable<O> && indirectly_movable<I, O>`: `ranges::move`, `ranges::move_if`
* `weakly_incrementable<O> && indirectly_writable<O, X>`: `ranges::transform`
* `output_iterator<O, X> && indirectly_copyable<I, O>`: `ranges::replace_copy`, `ranges::replace_copy_if`
* `output_iterator<O, X>`: `ranges::fill`, `ranges::fill_n`
* `input_or_output_iterator<O> && indirectly_writable<O, X>`: `ranges::generate` (see below), `ranges::generate_n`
* `weakly_incrementable<O> && mergeable<I1, I2, O, X, X>` (technically `mergeable`'s requirements on `O` are just `indirectly_copyable`, but putting it separately for completeness): `ranges::merge`, `ranges::set_union`, `ranges::set_intersection`, `ranges::set_difference`, `ranges::set_symmetric_difference`

Put differently, there are only 4 algorithms that use `output_iterator`: `ranges::replace_copy`, `ranges::replace_copy_if`, `ranges::fill`, and `ranges::fill_n`. However, there are 2 algorithms that use `output_range<R>` (which requires its iterator to be an `output_iterator`): `ranges::fill` and `ranges::generate`. The former makes sense, but the latter's iterator/sentinel overload doesn't require `output_iterator`. This means that `ranges::generate(r, f)` requires `*r.begin()++ = f();` to work, but `ranges::generate(r.begin(), r.end(), f)` does not. That just seems inconsistent.

Now, `indirectly_copyable<I, O>` is really two constraints put together. It requires that `I` is an `input_iterator` and that `O` is `indirectly_writable<iter_reference_t<I>>`. This constraint is by far the most common formulation for output ranges. But it's a bit redundant, since all of these algorithms already separately require `input_iterator<I>`. The only new requirement that `indirectly_copyable` brings in is the `indirectly_writable` one. What I mean is that instead of:

::: bq
```cpp
template<input_iterator I, sentinel_for<I> S, weakly_incrementable O>
  requires indirectly_copyable<I, O>
  constexpr ranges::copy_result<I, O> ranges::copy(I first, S last, O result);
```
:::

we could get the same exact same requirements (no more, no less) by instead writing:

::: bq
```cpp
template<input_iterator I, sentinel_for<I> S, weakly_incrementable O>
  requires indirectly_writable<O, iter_reference_t<I>>
  constexpr ranges::copy_result<I, O> ranges::copy(I first, S last, O result);
```
:::

The same idea could hold for the algorithms requiring `indirectly_movable` (replaced with a different kind of `indirectly_writable` constraint).

# Proposal: A Weaker Output Iterator

We can't remove the `*out++= r;` requirement from `output_iterator`. It's 2022, surely somebody has written some C++20 code by now, and might rely on that part of the `concept`. Similarly, we cannot add the `*out++ = r;` requirement to all the algorithms which take an output iterator, since likewise somebody could have written C++20 code that passes in a type into these algorithms that meets every requirement but that one, and this added constraint would break their code.

This paper does not propose either adding or removing any requirements to algorithms.

Let me repeat in different words: this paper does not propose anything that would change any behavior of C++20 code.

Instead, the problem this paper seeks to solve is that we have all these algorithms which require an output iterator but don't actually use those terms anywhere, which just seems wrong, and leads to diagnostics that are just worse than they could be. We can do better by introducing a new, weaker output iterator concept:

::: bq
```cpp
template<class I, class T>
  concept weak_output_iterator =
    input_or_output_iterator<I> &&
    indirectly_writable<I, T>;

template<class I, class T>
  concept output_iterator =
    weak_output_iterator<I, T> &&
    requires(I i, T&& t) {
      *i++ = std::forward<T>(t);        // not required to be equality-preserving
    };
```
:::

We can discuss whether this should be exposition-only or not, but it seems like it shouldn't be.

With such a concept, we can go through all the algorithms and respecify them to just use it. For example, `ranges::copy` becomes:

::: cmptable
### In C++20
```cpp
template <input_iterator I,
          sentinel_for<I> S,
          weakly_incrementable O>
  requires indirectly_copyable<I, O>
constexpr ranges::copy_result<I, O>
ranges::copy(I first, S last, O result);
```

### Proposed
```cpp
template <input_iterator I,
          sentinel_for<I> S,
          weak_output_iterator<iter_reference_t<I>> O>
constexpr ranges::copy_result<I, O>
ranges::copy(I first, S last, O result);
```
:::

Unlike my hypothetical spelling earlier, these two now have identical requirements.

## Wording

Change [iterator.synopsis]{.sref}:

::: bq
```diff
namespace std {
  // ...

  // [iterator.concept.output], concept output_iterator
+ template<class I, class T>
+   concept weak_output_iterator = $see below$;

  template<class I, class T>
    concept output_iterator = see below;

  // ...
}
```
:::

Change [iterator.concept.output]{.sref} [The semantic effects are specific to `output_iterator`, not `weak_output_iterator`]{.ednote}:

::: bq
[1]{.pnum} The [`weak_output_iterator` and]{.addu} `output_iterator` concept[s]{.addu} define[s]{.rm} requirements for a type that can be used to write values (from the requirement for `indirectly_writable` ([iterator.concept.writable])) and which can be both pre- and post-incremented.
[*Note 1*: Output iterators are not required to model `equality_comparable`. — *end note*]

```diff
+ template<class I, class T>
+   concept weak_output_iterator =
+     input_or_output_iterator<I> &&
+     indirectly_writable<I, T>;

  template<class I, class T>
    concept output_iterator =
-     input_or_output_iterator<I> &&
-     indirectly_writable<I, T> &&
+     weak_output_iterator<I, T> &&
      requires(I i, T&& t) {
        *i++ = std::forward<T>(t);        // not required to be equality-preserving
      };
```

[2]{.pnum} Let `E` be an expression such that `decltype((E))` is `T`, and let `i` be a dereferenceable object of type `I`.
`I` and `T` model `output_iterator<I, T>` only if `*i++ = E;` has effects equivalent to: `*i = E; ++i;`
:::

Change all the constraints to use `weak_output_iterator` in the algorithms. The wording diff here only includes the synopsis, the same changes need to be made in these algorithms' corresponding definition too. These are all in [algorithm.syn]{.sref}, broken up by algorithm for convenience:

### `ranges::copy`

::: bq
```diff
namespace std::ranges {
    template<class I, class O>
      using copy_result = in_out_result<I, O>;

-   template<input_iterator I, sentinel_for<I> S, weakly_incrementable O>
-     requires indirectly_copyable<I, O>
+   template<input_iterator I, sentinel_for<I> S, weak_output_iterator<iter_reference_t<I>> O>
      constexpr copy_result<I, O>
        copy(I first, S last, O result);

-   template<input_range R, weakly_incrementable O>
-     requires indirectly_copyable<iterator_t<R>, O>
+   template<input_range R, weak_output_iterator<range_reference_t<R>> O>
      constexpr copy_result<borrowed_iterator_t<R>, O>
        copy(R&& r, O result);
}
```
:::

### `ranges::copy_n`

::: bq
```diff
namespace std::ranges {
    template<class I, class O>
      using copy_n_result = in_out_result<I, O>;

-   template<input_iterator I, weakly_incrementable O>
-     requires indirectly_copyable<I, O>
+   template<input_iterator I, weak_output_iterator<iter_reference_t<I>> O>
      constexpr copy_n_result<I, O>
        copy_n(I first, iter_difference_t<I> n, O result);
}
```
:::

### `ranges::copy_if`

::: bq
```diff
namespace std::ranges {
    template<class I, class O>
      using copy_if_result = in_out_result<I, O>;

-   template<input_iterator I, sentinel_for<I> S, weakly_incrementable O,
+   template<input_iterator I, sentinel_for<I> S, weak_output_iterator<iter_reference_t<I>> O,
             class Proj = identity,
             indirect_unary_predicate<projected<I, Proj>> Pred>
-     requires indirectly_copyable<I, O>
      constexpr copy_if_result<I, O>
        copy_if(I first, S last, O result, Pred pred, Proj proj = {});

-   template<input_range R, weakly_incrementable O,
+   template<input_range R, weak_output_iterator<range_reference_t<R>> O,
             class Proj = identity,
             indirect_unary_predicate<projected<iterator_t<R>, Proj>> Pred>
-     requires indirectly_copyable<iterator_t<R>, O>
      constexpr copy_if_result<borrowed_iterator_t<R>, O>
        copy_if(R&& r, O result, Pred pred, Proj proj = {});
}
```
:::

### `ranges::move`

::: bq
```diff
namespace std::ranges {
    template<class I, class O>
      using move_result = in_out_result<I, O>;

-   template<input_iterator I, sentinel_for<I> S, weakly_incrementable O>
-     requires indirectly_movable<I, O>
+   template<input_iterator I, sentinel_for<I> S, weak_output_iterator<iter_rvalue_reference_t<I>> O>
      constexpr move_result<I, O>
        move(I first, S last, O result);

-   template<input_range R, weakly_incrementable O>
-     requires indirectly_movable<iterator_t<R>, O>
+   template<input_range, weak_output_iterator<range_rvalue_reference_t<R>> O>
      constexpr move_result<borrowed_iterator_t<R>, O>
        move(R&& r, O result);
}
```
:::

### `ranges::transform`

[Here, I'm using `class O` and having a trailing `requires weak_output_iterator<O, T>` because the relevant type `T` here is based on `Proj`, which is declared after `O`.]{.draftnote}

::: bq
```diff
namespace std::ranges {
    template<class I, class O>
      using unary_transform_result = in_out_result<I, O>;

-   template<input_iterator I, sentinel_for<I> S, weakly_incrementable O,
+   template<input_iterator I, sentinel_for<I> S, class O,
             copy_constructible F, class Proj = identity>
-     requires indirectly_writable<O, indirect_result_t<F&, projected<I, Proj>>>
+     requires weak_output_iterator<O, indirect_result_t<F&, projected<I, Proj>>>
      constexpr unary_transform_result<I, O>
        transform(I first1, S last1, O result, F op, Proj proj = {});

-   template<input_range R, weakly_incrementable O, copy_constructible F,
+   template<input_range R, class O, copy_constructible F,
             class Proj = identity>
-     requires indirectly_writable<O, indirect_result_t<F&, projected<iterator_t<R>, Proj>>>
+     requires weak_output_iterator<O, indirect_result_t<F&, projected<iterator_t<R>, Proj>>>
      constexpr unary_transform_result<borrowed_iterator_t<R>, O>
        transform(R&& r, O result, F op, Proj proj = {});

    template<class I1, class I2, class O>
      using binary_transform_result = in_in_out_result<I1, I2, O>;

    template<input_iterator I1, sentinel_for<I1> S1, input_iterator I2, sentinel_for<I2> S2,
-            weakly_incrementable O, copy_constructible F, class Proj1 = identity,
+            class O, copy_constructible F, class Proj1 = identity,
             class Proj2 = identity>
-     requires indirectly_writable<O, indirect_result_t<F&, projected<I1, Proj1>,
+     requires weak_output_iterator<O, indirect_result_t<F&, projected<I1, Proj1>,
                                             projected<I2, Proj2>>>
      constexpr binary_transform_result<I1, I2, O>
        transform(I1 first1, S1 last1, I2 first2, S2 last2, O result,
                  F binary_op, Proj1 proj1 = {}, Proj2 proj2 = {});

-   template<input_range R1, input_range R2, weakly_incrementable O,
+   template<input_range R1, input_range R2, class O,
             copy_constructible F, class Proj1 = identity, class Proj2 = identity>
-     requires indirectly_writable<O, indirect_result_t<F&, projected<iterator_t<R1>, Proj1>,
+     requires weak_output_iterator<O, indirect_result_t<F&, projected<iterator_t<R1>, Proj1>,
                                             projected<iterator_t<R2>, Proj2>>>
      constexpr binary_transform_result<borrowed_iterator_t<R1>, borrowed_iterator_t<R2>, O>
        transform(R1&& r1, R2&& r2, O result,
                  F binary_op, Proj1 proj1 = {}, Proj2 proj2 = {});
}
```
:::
