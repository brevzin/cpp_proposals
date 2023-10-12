---
title: "Removing the common reference requirement from the indirectly invocable concepts"
document: P2997R0
date: today
audience: LEWG, SG9
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Tim Song
      email: <t.canens.cpp@gmail.com>
toc: true
tag: ranges
---

# Introduction

Consider the following example ([compiler-explorer](https://godbolt.org/z/Pcb9639YG)):

::: bq
```cpp
#include <algorithm>
#include <ranges>

struct C {
    auto f() -> void;
};

struct Iterator {
    using value_type = C;
    using difference_type = std::ptrdiff_t;
    using iterator_category = std::input_iterator_tag;

    auto operator*() const -> C;
    auto operator++() -> Iterator&;
    auto operator++(int) -> void;
    auto operator==(Iterator const&) const -> bool;
};

static_assert(std::input_iterator<Iterator>);
static_assert(std::same_as<std::iter_value_t<Iterator>, C>);
static_assert(std::same_as<std::iter_reference_t<Iterator>, C>);
static_assert(std::same_as<std::iter_common_reference_t<Iterator>, C>);

struct R {
    auto begin() -> Iterator;
    auto end() -> Iterator;
};

static_assert(std::ranges::range<R>);
static_assert(std::same_as<std::ranges::range_reference_t<R>, C>);

using P = std::projected<Iterator, std::identity>;
static_assert(std::same_as<std::iter_value_t<P>, C>);
static_assert(std::same_as<std::iter_reference_t<P>, C&&>);
static_assert(std::same_as<std::iter_common_reference_t<P>, C const&>);

auto f(R r) -> void {
    std::ranges::for_each(r, [](auto&& c){
        c.f();
    });
}
```
:::

It's a bit lengthy, but the important part is that `R` is a range of prvalue `C` - `Iterator` is just a minimal iterator to achieve that goal (the fact that the iterator is just an input iterator doesn't matter, it just makes the example smaller).

The above example is trying to simply invoke `c.f()` for every `c` in `r`. When written using a `for` loop, the code compiles. But when written using `std::ranges::for_each`, it does not. The compile error on gcc 13.2 is:

::: bq
<pre><code>
/opt/compiler-explorer/gcc-13.2.0/include/c++/13.2.0/type_traits:2558:26:   required by substitution of 'template<class _Fn, class ... _Args> static std::__result_of_success<decltype (declval<_Fn>()((declval<_Args>)()...)), std::__invoke_other> std::__result_of_other_impl::_S_test(int) [with _Fn = f(R)::<lambda(auto:62&&)>&; _Args = {const C&}]'
/opt/compiler-explorer/gcc-13.2.0/include/c++/13.2.0/type_traits:2569:55:   required from 'struct std::__result_of_impl<false, false, f(R)::<lambda(auto:62&&)>&, const C&>'
/opt/compiler-explorer/gcc-13.2.0/include/c++/13.2.0/type_traits:3077:12:   recursively required by substitution of 'template<class _Result, class _Ret> struct std::__is_invocable_impl<_Result, _Ret, true, std::__void_t<typename _CTp::type> > [with _Result = std::__invoke_result<f(R)::<lambda(auto:62&&)>&, const C&>; _Ret = void]'
/opt/compiler-explorer/gcc-13.2.0/include/c++/13.2.0/type_traits:3077:12:   required from 'struct std::is_invocable<f(R)::<lambda(auto:62&&)>&, const C&>'
<source>:38:26:   recursively required by substitution of 'template<class _Range, class _Proj, class _Fun>  requires (input_range<_Range>) && (indirectly_unary_invocable<_Fun, typename std::__detail::__projected<decltype(std::ranges::__cust_access::__begin((declval<_Container&>)())), _Proj>::__type>) constexpr std::ranges::for_each_result<std::ranges::borrowed_iterator_t<_Range>, _Fun> std::ranges::__for_each_fn::operator()(_Range&&, _Fun, _Proj) const [with _Range = R&; _Proj = std::identity; _Fun = f(R)::<lambda(auto:62&&)>]'
<source>:38:26:   required from here
<source>:39:12: error: passing 'const C' as 'this' argument discards qualifiers [-fpermissive]
   39 |         c.f();
      |         ~~~^~
<source>:5:10: note:   in call to 'void C::f()'
    5 |     auto f() -> void;
      |          ^
</code></pre>
:::

If you stare at the error long enough you might eventually see a hint as to what the problem is: why, exactly, am I getting a compile error about discarding qualifiers on a `const C`  when nothing in this code example appears to be forming a `const C`?

## Wait... but why?

The problem here is coming from the algorithm. `ranges::for_each` is declared as:

::: bq
```cpp
template<input_range R, class Proj = identity,
         indirectly_unary_invocable<projected<iterator_t<R>, Proj>> Fun>
  constexpr ranges::for_each_result<borrowed_iterator_t<R>, Fun>
    ranges::for_each(R&& r, Fun f, Proj proj = {});
```
:::

To understand what's going on here, we need to look at several other pieces. `projected` is a helper utility that is only used to make it easier to specify constraints (it isn't used in the implementation):

::: bq
```cpp
template<class I, class Proj>
struct $projected-impl$ {                             // exposition only
  struct type {                                       // exposition only
    using value_type = remove_cvref_t<indirect_result_t<Proj&, I>>;
    using difference_type = iter_difference_t<I>;     // present only if I
                                                      // models weakly_incrementable
    indirect_result_t<Proj&, I> operator*() const;    // not defined
  };
};

template<indirectly_readable I, indirectly_regular_unary_invocable<I> Proj>
  using projected = $projected-impl$<I, Proj>::type;
```
:::

And the relevant concept for us in this algorithm is this one:

::: bq
```cpp
template<class F, class I>
  concept indirectly_unary_invocable =
    indirectly_readable<I> &&
    copy_constructible<F> &&
    invocable<F&, $indirect-value-t$<I>> &&
    invocable<F&, iter_reference_t<I>> &&
    invocable<F&, iter_common_reference_t<I>> &&
    common_reference_with<
      invoke_result_t<F&, $indirect-value-t$<I>>,
      invoke_result_t<F&, iter_reference_t<I>>>;
```
:::

What the code example needs to do is validate that our lambda satisfies `indirectly_unary_invocable<I>`, where `I` is `projected<Iterator, identity>`. Before we go through the constraints, it's worth first start with what all the iterator types actually are, because there's a lot and it's easy to get confused.

<table>
<tr><th/><th>`Iterator`</th><th>`projected<Iterator, identity>`</th></tr>
<tr><td>`value_type`</td><td>`C`</td><td>`C`</td></tr>
<tr><td>`$indirect-value-t$`</td><td>`C&`</td><td>`C&`</td></tr>
<tr><td>`reference`</td><td>`C`</td><td>`C&&`</td></tr>
<tr><td>`common_reference`</td><td>`C`</td><td>`C const&`</td></tr>
</table>

Now, the left column actually doesn't matter here - although those are probably the types that users expect to be relevant. After all, we are not using a projection in our algorithm call, so why do the projected types matter? But because we can't propagate prvalues, `projected<Iterator, identity>` does actually differ from `Iterator` - because the reference type becomes an rvalue reference, which causes the common reference type to become a reference to const.

As a result, if we go through our four constraints:

|Constraint|Outcome|
|-|-|
|`invocable<F&, $indirect-value-t$<I>>`|✔️|
|`invocable<F&, iter_reference_t<I>>`|✔️|
|`invocable<F&, iter_common_reference_t<I>>`|❌|
|`common_reference_with<..., ...>`|✔️|

That third one fails, because we're trying to invoke our lambda with a `C const&`, which instantiates the lambda - which doesn't have any constraints - and the instantiation of the body (to figure out the return type) causes the compiler to instantiate that `c.f()` call, which isn't valid for a `C const`. Hard error.

It's worth it to point out here that this issue wouldn't just come up for ranges of prvalues, the same would be true if `R` was a range of `C&&` (e.g. `std::generator<C>` or `std::vector<C> | views::as_rvalue`). In that case, both columns of the `Iterator` vs `projected<Iterator, identity>` table would look the same as the right column does above. Either way - we start with a range in which nothing is `const` and end up with a requirement that the callable is invocable with a `const&`.

## Ok, but... why?

We have to go back to Eric Niebler's four-part series on iterators [@niebler.iter.0] [@niebler.iter.1] [@niebler.iter.2] [@niebler.iter.3]. There, he introduced an algorithm `unique_copy` which looks like this:

::: quote
```{.cpp .numberLines}
// Copyright (c) 1994
// Hewlett-Packard Company
// Copyright (c) 1996
// Silicon Graphics Computer Systems, Inc.
template <class InIter, class OutIter, class Fn,
          class _Tp>
OutIter
__unique_copy(InIter first, InIter last,
              OutIter result,
              Fn binary_pred, _Tp*) {
  _Tp value = *first;
  *result = value;
  while (++first != last)
    if (!binary_pred(value, *first)) {
      value = *first;
      *++result = value;
    }
  return ++result;
}
```

[...] Note the value local variable on line 11, and especially note line 14, where it passes a value and a reference to `binary_pred`. Keep that in mind because it’s important!

[...] Why do I bring it up? Because it’s _super problematic_ when used with proxy iterators. Think about what happens when you try to pass `vector<bool>::iterator` to the above `__unique_copy` function:

```cpp
std::vector<bool> vb{true, true, false, false};
using R = std::vector<bool>::reference;
__unique_copy(
  vb.begin(), vb.end(),
  std::ostream_iterator<bool>{std::cout, " "},
  [](R b1, R b2) { return b1 == b2; }, (bool*)0 );
```

This _should_ write a “true” and a “false” to `cout`, but it doesn’t compile. Why? The lambda is expecting to be passed two objects of `vector<bool>`‘s proxy reference type, but remember how `__unique_copy` calls the predicate:
```cpp
if (!binary_pred(value, *first)) { /*...*/
```

That’s a `bool&` and a `vector<bool>::reference`. Ouch!
:::

The blog goes on to point out that one way to resolve this is by having the lambda take both parameters as `auto&&`. But having to _require_ a generic lambda is a bit... much. This was the reason we have the idea of a `common_reference`. Rewriting the above to be:

```cpp
using R = std::iter_common_reference_t<std::vector<bool>::reference>;
__unique_copy(
  vb.begin(), vb.end(),
  std::ostream_iterator<bool>{std::cout, " "},
  [](R b1, R b2) { return b1 == b2; }, (bool*)0 );
```

This now works. And that is now the requirement that we have for iterators, that they be `indirectly_readable` - which, among other things, requires that there be a `common_reference` between the iterator's `reference` type and the iterator's `value_type&`.

That is: the common reference is the type that you can use as the parameter of a non-generic callable that you pass into an algorithm, to ensure that both forms (`value` and `*it` above) that the implementation might use to invoke the callable work.

## Common Reference is for Parameters

The key thing to point out in the above motivation though is that the common reference type is useful _for parameters_. That is, for the user invoking the algorithm to be able to do so with a homogenous callable - a non-generic lambda.

But what the common reference type is _not_ useful for is the algorithm implementations themselves. The `unique_copy` implementation above is never going to construct a `std::iter_common_reference<InIter>`. It has no reason to do so - it's only going to traffic in `std::iter_value_t<InIter>&` and `std::iter_reference_t<InIter>`. The same is true for every algorithm.

The only places in the standard library right now where we actually do construct common references are either:

* when we actually do need the common reference of multiple ranges (e.g. `views::join_with` and the proposed `views::concat`), since we're explicitly doing merging
* when we're trying to produce a new reference type (e.g. `views::as_const`)

But internally within the algorithms? No such need.

So let's go back to the definition of `std::indirectly_unary_invocable`, which was the constraint the original example failed:

::: bq
```{.cpp .numberLines}
template<class F, class I>
  concept indirectly_unary_invocable =
    indirectly_readable<I> &&
    copy_constructible<F> &&
    invocable<F&, $indirect-value-t$<I>> &&
    invocable<F&, iter_reference_t<I>> &&
    invocable<F&, iter_common_reference_t<I>> &&
    common_reference_with<
      invoke_result_t<F&, $indirect-value-t$<I>>,
      invoke_result_t<F&, iter_reference_t<I>>>;
```
:::

Excluding the check that `I` is an iterator and `F` is copyable, we have 4 other checks here:

1. that `F&` is invocable with `iter_value_t<I&>` (`$indirect-value-t$<I>` is basically this, but in a way that will work correctly through projections, which we don't have to worry about in this paper)
2. that `F&` is invocable with `iter_reference_t<I>`
3. that `F&` is invocable with `iter_common_reference_t<I>`
4. that the results of invocations (1) and (2) have a common reference

Notably, (3) is kind of the odd man out in this concept. Algorithms certainly may need to invoke `F` with `iter_value_t<I&>` and with `iter_reference_t<I>` (as in the `unique_copy` example above, which does both). But algorithms don't need to interally construct a common reference ever.

Moreover, we check we can invoke `F` with the common reference - but while we check that value/reference invocations are compatible, we don't actually check that the common reference invocation is compatible with... either of the other two? It could just do something different entirely? It's not clear what the point of this part of the concept actually is.

# Proposal

The common reference type exists to be able to merge multiple ranges and to provide users with a way to write a non-generic callable. But the requirement that callables be invocable with the iterator's common reference type isn't a useful requirement for any algorithm. And while we require this invocability, we don't even require that this invocation (which no algorithm uses) is compatible with the invocations that the algorithms do use. This check seems to simply reject valid code (as in the original example) while providing no value.

We propose to simply remove this check from every indirect invocation concept. That would fix the above example and others like it, while also not removing a requirement that algorithms actually rely on. Those uses cited above that actually do need to produce a common reference (`views::join_with`, `views::concat`, and `views::as_const`) already have this requirement separately.

## Wording

Remove all the `iter_common_reference_t` invocation requirements in [indirectcallable.indirectinvocable]{.sref} (which frequently requires a multi-line diff just because the common reference invocation was the last one in the list):

::: bq
```diff
namespace std {
  template<class F, class I>
    concept indirectly_unary_invocable =
      indirectly_readable<I> &&
      copy_constructible<F> &&
      invocable<F&, $indirect-value-t<I>$> &&
      invocable<F&, iter_reference_t<I>> &&
-     invocable<F&, iter_common_reference_t<I>> &&
      common_reference_with<
        invoke_result_t<F&, $indirect-value-t<I>$>,
        invoke_result_t<F&, iter_reference_t<I>>>;

  template<class F, class I>
    concept indirectly_regular_unary_invocable =
      indirectly_readable<I> &&
      copy_constructible<F> &&
      regular_invocable<F&, $indirect-value-t<I>$> &&
      regular_invocable<F&, iter_reference_t<I>> &&
-     regular_invocable<F&, iter_common_reference_t<I>> &&
      common_reference_with<
        invoke_result_t<F&, $indirect-value-t<I>$>,
        invoke_result_t<F&, iter_reference_t<I>>>;

  template<class F, class I>
    concept indirect_unary_predicate =
      indirectly_readable<I> &&
      copy_constructible<F> &&
      predicate<F&, $indirect-value-t<I>$> &&
-     predicate<F&, iter_reference_t<I>>@[ &&]{.diffdel}@
-     predicate<F&, iter_common_reference_t<I>>;
+     predicate<F&, iter_reference_t<I>>@[;]{.diffins}@

  template<class F, class I1, class I2>
    concept indirect_binary_predicate =
      indirectly_readable<I1> && indirectly_readable<I2> &&
      copy_constructible<F> &&
      predicate<F&, $indirect-value-t$<I1>, $indirect-value-t$<I2>> &&
      predicate<F&, $indirect-value-t$<I1>, iter_reference_t<I2>> &&
      predicate<F&, iter_reference_t<I1>, $indirect-value-t$<I2>> &&
-     predicate<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[ &&]{.diffdel}@
-     predicate<F&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>;
+     predicate<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[;]{.diffins}@

  template<class F, class I1, class I2 = I1>
    concept indirect_equivalence_relation =
      indirectly_readable<I1> && indirectly_readable<I2> &&
      copy_constructible<F> &&
      equivalence_relation<F&, $indirect-value-t$<I1>, $indirect-value-t$<I2>> &&
      equivalence_relation<F&, $indirect-value-t$<I1>, iter_reference_t<I2>> &&
      equivalence_relation<F&, iter_reference_t<I1>, $indirect-value-t$<I2>> &&
-     equivalence_relation<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[ &&]{.diffdel}@
-     equivalence_relation<F&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>;
+     equivalence_relation<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[;]{.diffins}@

  template<class F, class I1, class I2 = I1>
    concept indirect_strict_weak_order =
      indirectly_readable<I1> && indirectly_readable<I2> &&
      copy_constructible<F> &&
      strict_weak_order<F&, $indirect-value-t$<I1>, $indirect-value-t$<I2>> &&
      strict_weak_order<F&, $indirect-value-t$<I1>, iter_reference_t<I2>> &&
      strict_weak_order<F&, iter_reference_t<I1>, $indirect-value-t$<I2>> &&
-     strict_weak_order<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[ &&]{.diffdel}@
-     strict_weak_order<F&, iter_common_reference_t<I1>, iter_common_reference_t<I2>>;
+     strict_weak_order<F&, iter_reference_t<I1>, iter_reference_t<I2>>@[;]{.diffins}@
}
```
:::

---
references:
    - id: niebler.iter.0
      citation-label: niebler.iter.0
      title: To Be or Not to Be (an Iterator)
      author:
        - family: Eric Niebler
      issued:
        year: 2015
      URL: http://ericniebler.com/2015/01/28/to-be-or-not-to-be-an-iterator/
    - id: niebler.iter.1
      citation-label: niebler.iter.1
      title: Iterators++, Part 1
      author:
        - family: Eric Niebler
      issued:
        year: 2015
      URL: http://ericniebler.com/2015/02/03/iterators-plus-plus-part-1/
    - id: niebler.iter.2
      citation-label: niebler.iter.2
      title: Iterators++, Part 2
      author:
        - family: Eric Niebler
      issued:
        - year: 2015
      URL: http://ericniebler.com/2015/02/13/iterators-plus-plus-part-2/
    - id: niebler.iter.3
      citation-label: niebler.iter.3
      title: Iterators++, Part 3
      author:
        - family: Eric Niebler
      issued:
        - year: 2015
      URL: http://ericniebler.com/2015/03/03/iterators-plus-plus-part-3/
---
