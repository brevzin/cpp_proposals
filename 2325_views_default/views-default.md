---
title: "Views should not be required to be default constructible"
document: P2325R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

Currently, the `view` concept is defined in [range.view]{.sref} as:

```cpp
template <class T>
concept view =
    range<T> &&
    movable<T> &&
    default_initializable<T> &&
    enable_view<T>;
```

Three of these four criteria, I understand. A `view` clearly needs to be a `range`, and it's important that they be `movable` for various operations to work. And the difference between a `view` and `range` is largely semantic, and so there needs to be an explicit opt-in in the form of `enable_view`. 

But why does a view need to be `default_initializable`? 

## History

The history of the design of Ranges is split between many papers and github issues in both the range-v3 [@range-v3] and stl2 [@stl2] libraries. However, I simply am unable to find much information that motivates this particular choice. 

In [@N4128], we have (this paper predates the term `view`, at the time the term "range" instead was used to refer to what is now called a `view`. To alleviate confusion, I have editted this paragraph accordingly):

::: bq
We’ve already decided that [Views] are copyable and assignable. They are, in the terminology of [@EoP] and [@N3351], Semiregular types. It follows that copies are independent, even though the copies are both aliases of the same underlying elements. The [views] are independent in the same way that a copy of a pointer or an iterator is independent from the original. Likewise, iterators from two [views] that are copies of each other are also independent. When the source [view] goes out of scope, it does not invalidate an iterator into the destination [view].

Semiregular also requires DefaultConstructible in [@N3351]. We follow suit and require all [Views] to be DefaultConstructible. Although this complicates the implementation of some range types, it has proven useful in practice, so we have kept this requirement.
:::

There is also [@stl2-179], titled "Consider relaxing the DefaultConstructible requirements," in which Casey Carter states (although the issue is about iterators rather than views):

::: bq
There's concern in the community that relaxing type invariants to allow for default construction of a type that would not otherwise provide it is a horrible idea.

Relaxing the default construction requirement for iterators would also remove one of the few "breaking" differences between input and output iterators in the Standard (which do not require default construction) and Ranges (which currently do require default construction).
:::

Though, importantly, Casey points out one concern:

::: bq
The recent trend of making everything in the standard library `constexpr` is in conflict with the desire to not require default construction. The traditional workaround for delayed initialization of a non-default-constructible `T` is to instead store an `optional<T>`. Changing an `optional<T>` from the empty to filled states is not possible in a constant expression 
:::

This was true at the time of the writing of the issue, but has since been resolved first at the core language level by [@P1330R0] and then at the library level by [@P2231R1]. As such, I'm simply unsure what the motivation is for requiring default construction of views.

The motivation for default construction of _iterators_ comes from [@N3644], although this doesn't really apply to _output_ iterators (which are also currently required to be default constructible).

## Uses of default construction

I couldn't find any other motivation for default construction of views from the paper trail, so I tried to discover the motivation for it in range-v3. I did this with a large hammer: I removed all the default constructors and saw what broke.

And the answer is... not much. The commit can be found here: [@range-v3-no-dflt]. The full list of breakage is:

1. `join_view` and `join_with_view` need a default-constructed inner view. This clearly breaks if that view isn't default constructible. I wrapped them in semiregular_box.

2. `views::ints` and `views::indices` are interesting in range-v3 because it's not just that `ints(0, 4)` gives you the range `[0,4)` but also that `ints` by itself is also a range (from `0` to infinity). These two inherit from iota, so once I removed the default constructor from iota, these uses break. So I added default constructors to `ints` and `indices`.

3. One of range-v3's mechanisms for easier implementation of views and iterators is called `view_facade`. I don't understand how it works, but in order for the iterator produced from `view_facade` to be default constructible (a requirement of forward iterator), the `view` itself has to be default constructible. So `linear_distribute_view` and `chunk_view` (the specialization for input ranges) kept their defaulted default constructors. This would break if you tried to chunk a non-default-constructible input view (chunk would also have to wrap its underlying view in semiregular_box or something), but there's no such test in range-v3. 

4. There's one test for `any_view` that just tests that it's default constructible. 

That's it. Broadly, just a few views that actually need default construction that can easily provide it, most simply don't need this constraint.

## Does this requirement cause harm?

Rather than providing a benefit, it seems like the default construction requirement causes harm.

If the argument for default construction is that it enables efficient deferred initialization during view composition, then I'm not sure I buy that argument. `join_view` would have to use an optional where it wouldn't have before, which makes it a little bigger. But conversely, right now, every range adaptor that takes a function has to use an optional: `transform_view`, `filter_view`, etc. all need to be default constructible so they have to wrap their callables in `@*semiregular-box*@` to make them default constructible. If views didn't have to be constructible, they wouldn't have to do this. Or rather, they would still have to do some wrapping, but we'd only need the assignment parts of `@*semiregular-box*@`, and not the default construction part, which means that `sizeof(@*copyable-box*@<T>)` would be equal to `sizeof(T)`, whereas `sizeof(@*semiregular-box*@<T>)` could be larger.

My impression right now is that the default construction requirement actually adds storage cost to range adapters on the whole rather than removing storage cost.

Furthermore, there's the question of _requiring_ a partially formed state to types even they didn't want to do that. This goes against the general advice of making bad states unrepresentable. Consider a type like `span<int, 5>`. This _should_ be a view: it's a non-owning, O(1)-everything range. But it's not default constructible, for good reason. If we were to add a default constructor that would make `span<int, 5>` partially formed, this adds an extra state that needs to be carefully checked by users, and suddenly every operation has additional preconditions that need to be documented. But this is true for every other view, too!

`ranges::ref_view` (see [range.ref.view]{.sref}) is another such view. In the same way that `std::reference_wrapper<T>` is a rebindable reference to `T`, `ref_view<R>` is a rebindable reference to the range `R`. Except `reference_wrapper<T>` isn't default constructible, but `ref_view<R>` is &mdash; it's just that as a user, I have no way to check to see if a particular `ref_view<R>` is fully formed or not. All of its member functions have this precondition that it really does refer to a range that I as the user can't check. This is broadly true of all the range adapters: you can't do _anything_ with a default constructed range adapter except assign to it.

If the default construction requirement doesn't add benefit (and I'm not sure that it does) and it causes harm (both in the sense of requiring invalid states on types and adding to the storage requirements on all range adapters and further adding to user confusion when their types fail to model `view`), maybe we should get rid of it?

# Proposal

Remove the `default_initializable` constraint from `view`, such that the concept becomes:

```cpp
template <class T>
concept view =
    range<T> &&
    movable<T> &&
    enable_view<T>;
```

Remove the `default_initializable` constraint from `weakly_incrementable`. This ends up removing the default constructible requirement from input-only and output iterators, while still keeping it on forward iterators (`forward_iterator` requires `incrementable` which requires `regular`). It also removes the default constructible requirement on the `W` parameter for `iota_view`.

Remove the default constructors from the standard library views and iterators for which they only exist to satisfy the requirement (`ref_view`, `istream_view`, `ostream_iterator`, `ostreambuf_iterator`, `back_insert_iterator`, `front_insert_iterator`, `insert_iterator`). Constrain the other standard library views' default constructors on the underlying types being default constructible.

For `join_view`, store the inner view in a `@*semiregular-box*@<views::all_t<@*InnerRng*@>>`.

Make `span<ElementType, Extent>` a `view` regardless of `Extent`. Currently, it is only a `view` when `Extent == 0 || Extent == dynamic_extent`.

We currently use `@*semiregular-box*@<T>` to make types `semiregular` (see [range.semi.wrap]{.sref}), which we use to wrap function objects throughout. We can do a little bit better by introducing a `@*copyable-box*@<T>` such that:

* If `T` is `copyable`, then `@*copyable-box*@<T>` is basically just `T`
* Otherwise, if `T` is `nothrow_copy_constructible` but not `copy_assignable`, then `@*copyable-box*@<T>` can be a thin wrapper around `T` that adds a copy assignment operator that does destroy-then-copy-construct.
* Otherwise, `@*copyable-box*@<T>` is `@*semiregular-box*@<T>` (we still need `optional<T>`'s empty state here to handle the case where copy construction can throw, to avoid double-destruction).

Replace all function object `@*semiregular-box*@<F>` wrappers throughout `<ranges>` with `@*copyable-box*@<F>` wrappers.

## Timeline

At the moment, only libstdc++ and MSVC provide an implementation of ranges (and MSVC's is incomplete). We either have to make this change now and soon, or never.

---
references:
    - id: EoP
      citation-label: EoP
      title: Elements of Programming. Addison-Wesley Professional.
      author:
        - family: Stepanov, A. and McJones, P.
      issued:
        - year: 2009
    - id: range-v3
      citation-label: range-v3
      title: "range-v3 repo"
      author:
        - family: Eric Niebler and Casey Carter
      issued:
        - year: 2013
      URL: https://github.com/ericniebler/range-v3/
    - id: stl2
      citation-label: stl2
      title: "stl2 repo"
      author:
        - family: Eric Niebler and Casey Carter
      issued:
        - year: 2014
      URL: https://github.com/ericniebler/stl2/  
    - id: stl2-179
      citation-label: stl2-179
      title: "Consider relaxing the DefaultConstructible requirements"
      author:
        - family: Casey Carter
      issued:
        - year: 2016
      URL: https://github.com/ericniebler/stl2/issues/179
    - id: P2231R1
      citation-label: P2231R1
      title: "Missing `constexpr` in `std::optional` and `std::variant`"
      author:
        - family: Barry Revzin
      issued:
        year: 2021
      URL: https://wg21.link/p2231r1
    - id: range-v3-no-dflt
      citation-label: range-v3-no-dflt
      title: "Removing default construction from range-v3 views"
      author:
        - family: Barry Revzin
      issued:
        year: 2021
      URL: https://github.com/BRevzin/range-v3/commit/2e2c9299535211bc5417f9146eaed9945e596e83
---