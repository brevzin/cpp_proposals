Title: A type trait for `std::compare_3way()`'s type
Document-Number: D1187R1
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: LEWG

# Motivation

For some types, in order to implement `operator<=>()` you have to defer to the implementation of the 
comparison operators of other types. And more than that, you need to know what the comparison category is for those types you defer to in order to know what comparison category to return. When I went through the exercise of trying to [implement `operator<=>` for `optional<T>`][revzin.impl], the declarations of the three functions ended up looking like this (rewritten here as non-member operator templates just for clarity):

    :::cpp
    template <typename T, typename U>
    constexpr auto operator<=>(optional<T> const& lhs, optional<U> const& rhs)
        -> decltype(compare_3way(*lhs, *rhs));
        
    template <typename T, typename U>
    constexpr auto operator<=>(optional<T> const& lhs, U const& rhs)
        -> decltype(compare_3way(*lhs, rhs));
        
    template <typename T>
    constexpr auto operator<=>(optional<T> const&, nullopt_t)
        -> strong_ordering;

We need to use `std::compare_3way(*lhs, *rhs)` instead of writing `lhs <=> rhs` to also handle those types which do not yet implement `<=>`, and that function is [specified][alg.3way] in a way to correctly address all the possible situations.

Let's throw in a few more examples for implementations for `vector` and `expected`:

    :::cpp
    template <typename T, typename U>
    auto operator<=>(vector<T> const& lhs, vector<U> const& rhs)
        -> decltype(compare_3way(lhs[0], rhs[0]));
        
    template <typename T1, typename E1, typename T2, typename E2>
    auto operator<=>(expected<T1,E1> const& lhs, expected<T2,E2> const& rhs)
        -> common_comparison_category_t<
                decltype(compare_3way(lhs.value(), rhs.value())),
                decltype(compare_3way(lhs.error(), rhs.error()))>;
                
We see the same thing over and over and over again. We need to know what comparison category to return, and this comparison category is a property of the types that we're comparing. 

That's a type trait. A type trait that's currently missing from the standard library.

# Proposal

This paper proposes the addition of a new type trait based on the preexisting rules in [\[alg.3way\]][alg.3way] and respecifying `std::compare_3way()` to use that trait in its return type instead of `auto`. This mirrors the usage of `std::invoke_result_t` as the return type of `std::invoke()`, and so should be named either `compare_3way_result` or `compare_3way_type`.

The trait should be a binary type trait, since ultimately we're comparing two things, but for convenience for common cases, the second type parameter should be defaulted to the first, so that users can simply write `compare_3way_type<T>` instead of `compare_3way_type<T,T>`.

Because comparison should not modify its arguments and `compare_3way()` takes its arguments by reference to const anyway, `compare_3way_type` is specified in such a way as to not require adding `const&` to every argument all the time.

The trait would allow for less cumbersome declarations of all of these operator templates:

<table style="width:100%">
<tr>
<th style="width:50%">
Today
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename T, typename U>
    constexpr auto operator<=>(optional<T> const& lhs,
            optional<U> const& rhs)
        -> decltype(compare_3way(*lhs, *rhs));
</td>
<td>
    :::cpp
    template <typename T, typename U>
    constexpr auto operator<=>(optional<T> const& lhs,
            optional<U> const& rhs)
        -> compare_3way_type_t<T, U>;
</td>
</tr>
<tr>
<td>
    :::cpp
    template <typename T, typename U>
    auto operator<=>(vector<T> const& lhs,
            vector<U> const& rhs)
        -> decltype(compare_3way(lhs[0], rhs[0]));
</td>
<td>
    :::cpp
    template <typename T, typename U>
    auto operator<=>(vector<T> const& lhs,
            vector<U> const& rhs)
        -> compare_3way_type_t<T, U>;
</td>
</tr>
<tr>
<td>
    :::cpp
    template <typename T1, typename E1,
        typename T2, typename E2>
    auto operator<=>(expected<T1,E1> const& lhs,
            expected<T2,E2> const& rhs)
        -> common_comparison_category_t<
                decltype(compare_3way(
                    lhs.value(), rhs.value())),
                decltype(compare_3way(
                    lhs.error(), rhs.error()))>;
</td>
<td>
    :::cpp
    template <typename T1, typename E1,
        typename T2, typename E2>
    auto operator<=>(expected<T1,E1> const& lhs,
            expected<T2,E2> const& rhs)
        -> common_comparison_category_t<
                compare_3way_type_t<T1, T2>,
                compare_3way_type_t<E1, E2>>;
</td>
</tr>    
</table>

## Wording

Add the new trait and its use into the `<compare>` synopsis in 23.4 [algorithm.sym]:

<blockquote><pre class="codehilite"><code class="language-cpp">#include &lt;initializer_list>

namespace std {
  // ...
  
  // [alg.3way], three-way comparison algorithms
  </code><code><ins>template&lt;class T, class U = T> struct compare_3way_type;
  
  template&lt;class T, class U = T>
    using compare_3way_type_t = typename compare_3way_type&lt;T, U>::type;</ins></code><code class="language-cpp">
  
  template&lt;class T, class U>
    constexpr </code><code><del>auto</del> <ins>compare_3way_type_t&lt;T, U></ins></code><code class="language-cpp"> compare_3way(const T& a, const U& b);
    
  // ...
}</code></pre></blockquote>

Add a new specification for `compare_3way_type` at the beginning of 23.7.11 \[alg.3way\]:

>  The behavior of a program that adds specializations for the `compare_3way_type` template defined in this subclause is undefined.

> For the `compare_3way_type` type trait applied to the types `T` and `U`, the member typedef `type` shall be either defined or not present as follows. Let `t` and `u` denote lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`.
>
 - If the expression `t <=> u` is well formed, the member *typedef-name* `type` shall equal `decltype(t <=> u)`.
 - Otherwise, if the expressions `t == u` and `t < u` are each well-formed and convertible to `bool`, the member *typedef-name* `type` shall equal `strong_ordering`.
 - Otherwise, if the expression `t == u` is well-formed and convertible to `bool`, the member *typedef-name* `type` shall equal `strong_equality`.
 - Otherwise, there shall be no member `type`.
 
 > A program shall not specialize `compare_3way_type`. 
 
Change the return type of `compare_3way` in 23.7.11 \[alg.3way\]. Its specification is largely repeated from the above, but the repetition is necessary:

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T, class U> constexpr </code><code><del>auto</del> <ins>compare_3way_type_t&lt;T, U></ins></code><code class="language-cpp"> compare_3way(const T& a, const U& b);</code></pre></blockquote>

## Interaction with [P1186](https://wg21.link/p1186r0)

P1186 proposed to obviate `std::compare_3way()` in favor of having `<=>` itself perform all of this logic. That paper getting adopted does not change the need for having a type trait like this, though it would make the specification much simpler as the library wording could simply defer to the core wording. 

Add a new specification for `compare_3way_type` at the beginning of 23.7.11 \[alg.3way\]:

> The behavior of a program that adds specializations for the `compare_3way_type` template defined in this subclause is undefined.

> For the `compare_3way_type` type trait applied to the types `T` and `U`, let `t` and `u` denote lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`. If the expression `t <=> u` is well formed, the member *typedef-name* type shall equal `decltype(t <=> u)`. Otherwise, there shall be no member `type`.


[revzin.impl]: https://medium.com/@barryrevzin/implementing-the-spaceship-operator-for-optional-4de89fc6d5ec "Implementing the spaceship operator for optional||Barry Revzin||2017-11-16"
[alg.3way]: http://eel.is/c++draft/alg.3way "[alg.3way]"
