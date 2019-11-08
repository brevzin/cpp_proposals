---
title: "`constexpr` for `<numeric>` algorithms"
document: D1946R1
date: today
audience: LWG
author:
    - name: Ben Deane
      email: <ben@elbeno.com>
toc: true
---

<style>
	del { color: red;  text-decoration: line-through; }
	ins {
        color: #006e28;
        background-color: #e6ffed;
    }
</style>

# Motivation

We have added `constexpr` to many parts of the standard library for C++20.

Notably, [@P0879R0] *Constexpr for swap and swap-related functions* added
`constexpr` to all functions in `<algorithm>` except `shuffle`, `sample`,
`stable_sort`, `stable_partition`, `inplace_merge`, and functions accepting an
`ExecutionPolicy`.

I believe LEWG's design intent is that the non-allocating, non-parallel
algorithms be `constexpr`. However, there are algorithms in `<numeric>` that
have been overlooked.

# Assumptions

[@P0879R0] made the following assumptions:

- if an algorithm uses compiler intrinsics, then those intrinsics could be made `constexpr` by compiler vendors.
- if an algorithm uses assembly optimization, then that assembly could be turned into a `constexpr` compiler intrinsic.
- if an algorithm uses external functions, then those functions could be made `inline` and marked `constexpr` or could be replaced with intrinsics.

This proposal could make the same assumptions about implementation; however with the advent of `std::is_constant_evaluated` for distinguishing compile-time and runtime code, perhaps these assumptions are no longer necessary.

# Algorithms in `<numeric>` that were apparently overlooked 

This proposal is to add `constexpr` to the following function templates in `<numeric>`, excepting the function templates that accept an `ExecutionPolicy`.

- `accumulate`
- `reduce`
- `inner_product`
- `transform_reduce`
- `partial_sum`
- `exclusive_scan`
- `inclusive_scan`
- `transform_exclusive_scan`
- `transform_inclusive_scan`
- `adjacent_difference`
- `iota`

# Feature testing macro

I propose the feature-testing macro `__cpp_lib_constexpr_numeric` to identify the presence of these `constexpr` forms.

# Proposed wording relative to N4810

Exactly as one would expect: add `constexpr` to the function templates listed above where they do not accept an `ExecutionPolicy`.


- Change **25.8 Header** <tt>&lt;numeric&gt;</tt> **synopsis** [**numeric.ops.overview**] as indicated:

<pre>
<tt>template&lt;class InputIterator, class T&gt;
  <ins>constexpr</ins> T accumulate(InputIterator first, InputIterator last, T init);
template&lt;class InputIterator, class T, class BinaryOperation&gt;
  <ins>constexpr</ins> T accumulate(InputIterator first, InputIterator last, T init, BinaryOperation binary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator&gt;
  <ins>constexpr</ins> typename iterator_traits&lt;InputIterator>::value_type
    reduce(InputIterator first, InputIterator last);
template&lt;class InputIterator, class T&gt;
  <ins>constexpr</ins> T reduce(InputIterator first, InputIterator last, T init);
template&lt;class InputIterator, class T, class BinaryOperation&gt;
  <ins>constexpr</ins> T reduce(InputIterator first, InputIterator last, T init, BinaryOperation binary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator1, class InputIterator2, class T&gt;
  <ins>constexpr</ins> T inner_product(InputIterator1 first1, InputIterator1 last1,
                            InputIterator2 first2, T init);
template&lt;class InputIterator1, class InputIterator2, class T,
         class BinaryOperation1, class BinaryOperation2&gt;
  <ins>constexpr</ins> T inner_product(InputIterator1 first1, InputIterator1 last1,
                            InputIterator2 first2, T init,
                            BinaryOperation1 binary_op1,
                            BinaryOperation2 binary_op2);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator1, class InputIterator2, class T&gt;
  <ins>constexpr</ins> T transform_reduce(InputIterator1 first1, InputIterator1 last1,
                               InputIterator2 first2,
                               T init);
template&lt;class InputIterator1, class InputIterator2, class T,
         class BinaryOperation1, class BinaryOperation2&gt;
  <ins>constexpr</ins> T transform_reduce(InputIterator1 first1, InputIterator1 last1,
                               InputIterator2 first2,
                               T init,
                               BinaryOperation1 binary_op1,
                               BinaryOperation2 binary_op2);
template&lt;class InputIterator, class T,
         class BinaryOperation, class UnaryOperation&gt;
  <ins>constexpr</ins> T transform_reduce(InputIterator first, InputIterator last,
                               T init,
                               BinaryOperation binary_op, UnaryOperation unary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator&gt;
  <ins>constexpr</ins> OutputIterator partial_sum(InputIterator first,
                                       InputIterator last,
                                       OutputIterator result);
template&lt;class InputIterator, class OutputIterator, class BinaryOperation&gt;
  <ins>constexpr</ins> OutputIterator partial_sum(InputIterator first,
                                       InputIterator last,
                                       OutputIterator result,
                                       BinaryOperation binary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator, class T&gt;
  <ins>constexpr</ins> OutputIterator exclusive_scan(InputIterator first, InputIterator last,
                                          OutputIterator result,
                                          T init);
template&lt;class InputIterator, class OutputIterator, class T, class BinaryOperation&gt;
  <ins>constexpr</ins> OutputIterator exclusive_scan(InputIterator first, InputIterator last,
                                          OutputIterator result,
                                          T init, BinaryOperation binary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator&gt;
  <ins>constexpr</ins> OutputIterator inclusive_scan(InputIterator first, InputIterator last,
                                          OutputIterator result);
template&lt;class InputIterator, class OutputIterator, class BinaryOperation&gt;
  <ins>constexpr</ins> OutputIterator inclusive_scan(InputIterator first, InputIterator last,
                                          OutputIterator result,
                                          BinaryOperation binary_op);
template&lt;class InputIterator, class OutputIterator, class BinaryOperation, class T&gt;
  <ins>constexpr</ins> OutputIterator inclusive_scan(InputIterator first, InputIterator last,
                                          OutputIterator result,
                                          BinaryOperation binary_op, T init);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator, class T,
         class BinaryOperation, class UnaryOperation&gt;
  <ins>constexpr</ins> OutputIterator transform_exclusive_scan(InputIterator first, InputIterator last,
                                                    OutputIterator result,
                                                    T init,
                                                    BinaryOperation binary_op,
                                                    UnaryOperation unary_op);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator,
         class BinaryOperation, class UnaryOperation&gt;
  <ins>constexpr</ins> OutputIterator transform_inclusive_scan(InputIterator first, InputIterator last,
                                                    OutputIterator result,
                                                    BinaryOperation binary_op,
                                                    UnaryOperation unary_op);
template&lt;class InputIterator, class OutputIterator,
         class BinaryOperation, class UnaryOperation, class T&gt;
  <ins>constexpr</ins> OutputIterator transform_inclusive_scan(InputIterator first, InputIterator last,
                                                    OutputIterator result,
                                                    BinaryOperation binary_op,
                                                    UnaryOperation unary_op,
                                                    T init);</tt>
</pre>

<pre>
<tt>template&lt;class InputIterator, class OutputIterator&gt;
  <ins>constexpr</ins> OutputIterator adjacent_difference(InputIterator first,
                                               InputIterator last,
                                               OutputIterator result);
template&lt;class InputIterator, class OutputIterator, class BinaryOperation&gt;
  <ins>constexpr</ins> OutputIterator adjacent_difference(InputIterator first,
                                               InputIterator last,
                                               OutputIterator result,
                                               BinaryOperation binary_op);</tt>
</pre>

<pre>
<tt>template&lt;class ForwardIterator, class T&gt;
  <ins>constexpr</ins> void iota(ForwardIterator first, ForwardIterator last, T value);</tt>
</pre>

- Change **25.9.2 Accumulate** [**accumulate**] as the synopsis.
- Change **25.9.3 Reduce** [**reduce**] as the synopsis.
- Change **25.9.4 Inner product** [**inner.product**] as the synopsis.
- Change **25.9.5 Transform reduce** [**transform.reduce**] as the synopsis.
- Change **25.9.6 Partial sum** [**partial.sum**] as the synopsis.
- Change **25.9.7 Exclusive scan** [**exclusive.scan**] as the synopsis.
- Change **25.9.8 Inclusive scan** [**inclusive.scan**] as the synopsis.
- Change **25.9.9 Transform exclusive scan** [**transform.exclusive.scan**] as the synopsis.
- Change **25.9.10 Transform inclusive scan** [**transform.inclusive.scan**] as the synopsis.
- Change **25.9.11 Adjacent difference** [**adjacent.difference**] as the synopsis.
- Change **25.9.12 Iota** [**iota**] as the synopsis.

The feature-testing macro should also be updated.

- Add a line to Table 36, **17.3.1 General** [**support.limits.general**]/3 as indicated:

<table border='1'>
<tr> 
<td width='55%'><tt><ins>__cpp_lib_constexpr_numeric</ins></tt></td>
<td width='15%'><tt>\[TBD]</tt></td>
<td><tt><ins>&lt;numeric&gt;</tt></td>
</tr>
</table>

# Thanks

Thanks to Jan Wilmans, Joe Loser and Casey Carter.
