---
title: "Views should not be required to be default constructible"
document: P2325R2
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

Since [@P2325R1], added wording.

Since [@P2325R0], added discussion of the different treatments of lvalue vs rvalue fixed-extent `span` in pipelines.

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

3. One of range-v3's mechanisms for easier implementation of views and iterators is called `view_facade`. This is an implementation strategy that uses the view as part of the iterator as an implementation detail. As such, because the iterator has to be default constructible, the view must be as well. So `linear_distribute_view` and `chunk_view` (the specialization for input ranges) kept their defaulted default constructors. But this is simply an implementation strategy, there's nothing inherent to these views that requires this approach.

4. There's one test for `any_view` that just tests that it's default constructible. 

That's it. Broadly, just a few views that actually need default construction that can easily provide it, most simply don't need this constraint.

## Does this requirement cause harm?

Rather than providing a benefit, it seems like the default construction requirement causes harm.

If the argument for default construction is that it enables efficient deferred initialization during view composition, then I'm not sure I buy that argument. `join_view` would have to use an optional where it wouldn't have before, which makes it a little bigger. But conversely, right now, every range adaptor that takes a function has to use an optional: `transform_view`, `filter_view`, etc. all need to be default constructible so they have to wrap their callables in `@*semiregular-box*@` to make them default constructible. If views didn't have to be constructible, they wouldn't have to do this. Or rather, they would still have to do some wrapping, but we'd only need the assignment parts of `@*semiregular-box*@`, and not the default construction part, which means that `sizeof(@*copyable-box*@<T>)` would be equal to `sizeof(T)`, whereas `sizeof(@*semiregular-box*@<T>)` could be larger.

My impression right now is that the default construction requirement actually adds storage cost to range adapters on the whole rather than removing storage cost.

Furthermore, there's the question of _requiring_ a partially formed state to types even they didn't want to do that. This goes against the general advice of making bad states unrepresentable. Consider a type like `span<int, 5>`. This _should_ be a `view`: it's a non-owning, O(1)-everything range. But it's not default constructible, so it's not a `view`. The consequence of this choice is the difference in behavior when using fixed-extent `span` in pipelines that start with an lvalue vs an rvalue:

```cpp
std::span<int, 5> s = /* ... */;

// Because span<int, 5> is not a view, rather than copying s into
// the resulting transform_view, this instead takes a
// ref_view<span<int, 5>>. If s goes out of scope, this will dangle.
auto lvalue = s | views::transform(f);

// Because span<int, 5> is a borrowed range, this compiles. We still
// don't copy the span<int, 5> directly, instead we end up with a
// subrange<span<int, 5>::iterator>.
auto rvalue = std::move(s) | views::transform(f);
```

Both alternatives are less efficient than they could be. The lvalue case requires an extra indirection and exposes an opportunity for a dangling range. The rvalue case won't dangle, but ends up requiring storing two iterators, which requires twice the storage as storing the single `span` would have. Either case is strictly worse than the behavior that would result from `span<int, 5>` having been a `view`.

But fixed-extent `span` isn't default-constructible for good reason: if we were to add a default constructor that would make `span<int, 5>` partially formed, this adds an extra state that needs to be carefully checked by users, and suddenly every operation has additional preconditions that need to be documented. But this is true for every other view, too!

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

Remove the `default_initializable` constraint from `weakly_incrementable`. This ends up removing the default constructible requirement from input-only and output iterators, while still keeping it on forward iterators (`forward_iterator` requires `incrementable` which requires `regular`). 

For `iota_view`, replace the `semiregular<W>` constraint with `copyable<W>`, and add a constraint on `iota_view<W, Bound>::iterator`'s default constructor. This allows an input-only `iota_view` with a non-default-constructible `W` while preserving the current behavior for all forward-or-better `iota_view`s. 

Remove the default constructors from the standard library views and iterators for which they only exist to satisfy the requirement (`ref_view`, `istream_view`, `ostream_iterator`, `ostreambuf_iterator`, `back_insert_iterator`, `front_insert_iterator`, `insert_iterator`). Constrain the other standard library views' default constructors on the underlying types being default constructible.

For `join_view`, store the inner view in a `optional<views::all_t<@*InnerRng*@>>`.

Make `span<ElementType, Extent>` a `view` regardless of `Extent`. Currently, it is only a `view` when `Extent == 0 || Extent == dynamic_extent`.

We currently use `@*semiregular-box*@<T>` to make types `semiregular` (see [range.semi.wrap]{.sref}), which we use to wrap function objects throughout. We can do a little bit better by introducing a `@*copyable-box*@<T>` such that:

* If `T` is `copyable`, then `@*copyable-box*@<T>` is basically just `T`
* Otherwise, if `T` is `nothrow_copy_constructible` but not `copy_assignable`, then `@*copyable-box*@<T>` can be a thin wrapper around `T` that adds a copy assignment operator that does destroy-then-copy-construct.
* Otherwise, `@*copyable-box*@<T>` is `@*semiregular-box*@<T>` (we still need `optional<T>`'s empty state here to handle the case where copy construction can throw, to avoid double-destruction).

Replace all function object `@*semiregular-box*@<F>` wrappers throughout `<ranges>` with `@*copyable-box*@<F>` wrappers. At this point, there are no uses of `@*semiregular-box*@<F>` left, so remove it.

## Timeline

At the moment, only libstdc++ and MSVC provide an implementation of ranges (and MSVC's is incomplete). We either have to make this change now and soon, or never.

# Wording

Make `span` unconditionally a view in [span.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...
  
  // [views.span], class template span
  template<class ElementType, size_t Extent = dynamic_extent>
    class span;

  template<class ElementType, size_t Extent>
    inline constexpr bool ranges::enable_view<span<ElementType, Extent>> =
-     Extent == 0 || Extent == dynamic_extent;
+     true;

  // ...
}
```
:::

Change [iterator.concept.winc]{.sref}/1:

::: bq
```diff
template<class I>
  concept weakly_incrementable =
-   @[default_initializable&lt;I> &&]{.diffdel}@ movable<I> &&
+   movable<I> &&
    requires(I i) {
      typename iter_difference_t<I>;
      requires @*is-signed-integer-like*@<iter_difference_t<I>>;
      { ++i } -> same_as<I&>;   // not required to be equality-preserving
      i++;                      // not required to be equality-preserving
    };
```
:::

Remove the default constructor and default member initializer from [back.insert.iterator]{.sref}:

::: bq
```diff
namespace std {
  template<class Container>
  class back_insert_iterator {
  protected:
-   Container* container @[= nullptr]{.diffdel}@;
+   Container* container;

  public:
    // ...

-   constexpr back_insert_iterator() noexcept = default;
    
    // ...
  };
}
```
:::

Remove the default constructor and default member initializer from [front.insert.iterator]{.sref}:

::: bq
```diff
namespace std {
  template<class Container>
  class front_insert_iterator {
  protected:
-   Container* container @[= nullptr]{.diffdel}@;
+   Container* container;

  public:
    // ...

-   constexpr front_insert_iterator() noexcept = default;
    
    // ...
  };
}
```
:::

Remove the default constructor and default member initializers from [insert.iterator]{.sref}:

::: bq
```diff
namespace std {
  template<class Container>
  class insert_iterator {
  protected:
-   Container* container @[= nullptr]{.diffdel}@;
-   ranges::iterator_t<Container> iter @[= ranges::iterator_t<Container>()]{.diffdel}@;
+   Container* container;
+   ranges::iterator_t<Container> iter;

  public:
    // ...

-   insert_iterator() = default;
    
    // ...
  };
}
```
:::

Constrain the defaulted default constructor in [common.iterator]{.sref} [This is not strictly necessary since `variant<I, S>` is not default constructible when `I` is not and so the default constructor would already be defined as deleted, but I think it just adds clarity to do this for consistency]{.ednote}:

::: bq
```diff
namespace std {
  template<input_or_output_iterator I, sentinel_for<I> S>
    requires (!same_as<I, S> && copyable<I>)
  class common_iterator {
  public:
-   constexpr common_iterator() = default;
+   constexpr common_iterator() @[requires default_initializable&lt;I>]{.diffins}@ = default;
    
    // ...

  private:
    variant<I, S> v_;   // exposition only
  };
```
:::

Constrain the defaulted default constructor in [counted.iterator]{.sref}:

::: bq
```diff
namespace std {
  template<input_or_output_iterator I>
  class counted_iterator {
  public:
    // ...
    
-   constexpr counted_iterator() = default;
+   constexpr counted_iterator() @[requires default_initializable&lt;I>]{.diffins}@ = default;
    
    // ...

  private:
    I current = I();                    // exposition only
    iter_difference_t<I> length = 0;    // exposition only
  };

  // ...
}
```
:::

Remove the default constructor and default member initializers from [ostream.iterator.general]{.sref}:

::: bq
```diff
namespace std {
  template<class T, class charT = char, class traits = char_traits<charT>>
  class ostream_iterator {
  public:
    // ...

-   constexpr ostream_iterator() noexcept = default;
    
    // ...

  private:
-   basic_ostream<charT,traits>* out_stream @[= nullptr]{.diffdel}@;          // exposition only
-   const charT* delim @[= nullptr]{.diffdel}@;                               // exposition only
+   basic_ostream<charT,traits>* out_stream;                    // exposition only
+   const charT* delim;                                         // exposition only
  };
}
```
:::

Remove the default constructor and default member initializer from [ostreambuf.iterator.general]{.sref}:

::: bq
```diff
namespace std {
  template<class charT, class traits = char_traits<charT>>
  class ostreambuf_iterator {
  public:
    // ...

-   constexpr ostreambuf_iterator() noexcept = default;
    
    // ...

  private:
-   streambuf_type* sbuf_ @[= nullptr]{.diffdel}@;    // exposition only
+   streambuf_type* sbuf_;              // exposition only
  };
}
```
:::


Adjust the `iota_view` constraints in [ranges.syn]{.sref}:

::: bq
```diff
#include <compare>              // see [compare.syn]
#include <initializer_list>     // see [initializer.list.syn]
#include <iterator>             // see [iterator.synopsis]

namespace std::ranges {
  // ...
  
  // [range.iota], iota view
  template<weakly_incrementable W, semiregular Bound = unreachable_sentinel_t>
-   requires weakly-equality-comparable-with<W, Bound> && @[semiregular]{.diffdel}@<W>
+   requires weakly-equality-comparable-with<W, Bound> && @[copyable]{.diffins}@<W>
  class iota_view;

  template<class W, class Bound>
    inline constexpr bool enable_borrowed_range<iota_view<W, Bound>> = true;

  namespace views { inline constexpr @*unspecified*@ iota = @*unspecified*@; }

  // ...  
}
```
:::

Remove `default_initializable` from the `view` concept in [range.view]{.sref}/1:

::: bq
```diff
template<class T>
  concept view =
-   range<T> && movable<T> && @[default_initializable&lt;T> &&]{.diffdel}@ enable_view<T>;
+   range<T> && movable<T> && enable_view<T>;
```
:::

Constrain the defaulted default constructor in [range.subrange.general]{.sref}:

::: bq
```diff
namespace std::ranges {
  // ...
  
  template<input_or_output_iterator I, sentinel_for<I> S = I, subrange_kind K =
      sized_sentinel_for<S, I> ? subrange_kind::sized : subrange_kind::unsized>
    requires (K == subrange_kind::sized || !sized_sentinel_for<S, I>)
  class subrange : public view_interface<subrange<I, S, K>> {
  private:
    static constexpr bool StoreSize =                           // exposition only
      K == subrange_kind::sized && !sized_sentinel_for<S, I>;
    I begin_ = I();                                             // exposition only
    S end_ = S();                                               // exposition only
    make-unsigned-like-t<iter_difference_t<I>> size_ = 0;       // exposition only; present only
                                                                // when StoreSize is true
  public:
-   subrange() = default;
+   subrange() @[requires default_initializable&lt;I>]{.diffins}@ = default;
  
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor and switch to _`copyable-box`_ in [range.single.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<copy_constructible T>
    requires is_object_v<T>
  class single_view : public view_interface<single_view<T>> {
  private:
-   @[*semiregular-box*]{.diffdel}@<T> value_;   // exposition only (see [range.semi.wrap])
+   @[*copyable-box*]{.diffins}@<T> value_;      // exposition only (see [range.copy.wrap])
  public:
-   single_view() = default;
+   single_view() @[requires default_initializable&lt;T>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

Change the synopsis in [range.iota.view]{.sref}:

::: bq
```diff
  template<weakly_incrementable W, semiregular Bound = unreachable_sentinel_t>
-   requires weakly-equality-comparable-with<W, Bound> && @[semiregular]{.diffdel}@<W>
+   requires weakly-equality-comparable-with<W, Bound> && @[copyable]{.diffins}@<W>
  class iota_view : public view_interface<iota_view<W, Bound>> {
  private:
    // [range.iota.iterator], class iota_view​::​iterator
    struct iterator;            // exposition only
    // [range.iota.sentinel], class iota_view​::​sentinel
    struct sentinel;            // exposition only
    W value_ = W();             // exposition only
    Bound bound_ = Bound();     // exposition only
  public:
-   iota_view() = default;
+   iota_view() @[requires default_initializable&lt;W>]{.diffins}@ = default;
    constexpr explicit iota_view(W value);
    constexpr iota_view(type_identity_t<W> value,
                        type_identity_t<Bound> bound);
    constexpr iota_view(iterator first, sentinel last) : iota_view(*first, last.bound_) {}

    constexpr iterator begin() const;
    constexpr auto end() const;
    constexpr iterator end() const requires same_as<W, Bound>;

    constexpr auto size() const requires see below;
  };
```
:::

Constrain the defaulted default constructor and adjust the constraint in [range.iota.iterator]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<weakly_incrementable W, semiregular Bound>
-   requires weakly-equality-comparable-with<W, Bound> && @[semiregular]{.diffdel}@<W>
+   requires weakly-equality-comparable-with<W, Bound> && @[copyable]{.diffins}@<W>
  struct iota_view<W, Bound>::iterator {
  private:
    W value_ = W();             // exposition only
  public:
    using iterator_concept = see below;
    using iterator_category = input_iterator_tag;       // present only if W models incrementable
    using value_type = W;
    using difference_type = IOTA-DIFF-T(W);

-   iterator() = default;
+   iterator() @[requires default_initializable&lt;W>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

Adjust the constraint in [range.iota.sentinel]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<weakly_incrementable W, semiregular Bound>
-   requires weakly-equality-comparable-with<W, Bound> && @[semiregular]{.diffdel}@<W>
+   requires weakly-equality-comparable-with<W, Bound> && @[copyable]{.diffins}@<W>
  struct iota_view<W, Bound>::sentinel {
    // ...
  };
}
```
:::

Remove the default constructor and default member initializers, as well as the `if` from [range.istream.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  // ...

  template<movable Val, class CharT, class Traits>
    requires default_initializable<Val> &&
             stream-extractable<Val, CharT, Traits>
  class basic_istream_view : public view_interface<basic_istream_view<Val, CharT, Traits>> {
  public:
-   basic_istream_view() = default;
    constexpr explicit basic_istream_view(basic_istream<CharT, Traits>& stream);

    constexpr auto begin()
    {
-     if (@*stream_*@) {
        *@*stream_*@ >> @*value_*@;
-     }
      return iterator{*this};
    }

    constexpr default_sentinel_t end() const noexcept;

  private:
    struct iterator;                                    // exposition only
-   basic_istream<CharT, Traits>* @*stream_*@ @[= nullptr]{.diffdel}@;    // exposition only
-   Val @*value_*@ @[= Val()]{.diffdel}@;                                 // exposition only
+   basic_istream<CharT, Traits>* @*stream_*@;              // exposition only
+   Val @*value_*@;                                         // exposition only
  };
}
```
:::

Remove the default constructor and default member initializer from [range.istream.iterator]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<movable Val, class CharT, class Traits>
    requires default_initializable<Val> &&
             stream-extractable<Val, CharT, Traits>
  class basic_istream_view<Val, CharT, Traits>::iterator {      // exposition only
  public:
    // ...

-   iterator() = default;
    
    // ...

  private:
-   basic_istream_view* parent_ @[= nullptr]{.diffdel}@;                      // exposition only
+   basic_istream_view* parent_;                                // exposition only
  };
}
```
:::

Remove having to handle the case where `parent_ == nullptr` from all the iterator operations in [range.istream.iterator]{.sref} as this state is no longer representable:

::: bq
```
iterator& operator++();
```
::: rm
[2]{.pnum} Preconditions: `parent_->stream_ != nullptr` is `true`.
:::

[3]{.pnum} Effects: Equivalent to:
    ```
    *parent_->stream_ >> parent_->value_;
    return *this;
    ```
```
void operator++(int);
```
::: rm
[4]{.pnum} *Preconditions*: `parent_->stream_ != nullptr` is `true`.
:::

[5]{.pnum} *Effects*: Equivalent to ++*this.
```
Val& operator*() const;
```
::: rm
[6]{.pnum} *Preconditions*: `parent_->stream_ != nullptr` is `true`.
:::

[7]{.pnum} *Effects*: Equivalent to: return `parent_->value_`;
```
friend bool operator==(const iterator& x, default_sentinel_t);
```
[8]{.pnum} *Effects*: Equivalent to: `return @[x.parent_ == nullptr ||]{.diffdel}@ !*x.parent_->stream_`;
:::

Replace the subclause [range.semi.wrap] (all the uses of `semiregular-box<T>` are removed with this paper) with a new subclause "Copyable wrapper" with stable name [range.copy.wrap]. The following is presented as a diff against the current [range.semi.wrap]{.sref}, and also resolves [@LWG3479]:

::: bq
[1]{.pnum} Many types in this subclause are specified in terms of an exposition-only class template [_`semiregular-box`_]{.rm} [_`copyable-box`_]{.addu}. [`@*semiregular-box*@<T>`]{.rm} [`@*copyable-box*@<T>`]{.addu}  behaves exactly like `optional<T>` with the following differences:

* [1.1]{.pnum} [`@*semiregular-box*@<T>`]{.rm} [`@*copyable-box*@<T>`]{.addu} constrains its type parameter `T` with `copy_constructible<T>` && `is_object_v<T>`.
* [1.2]{.pnum} [If T models default_initializable, the]{.rm} [The]{.addu} default constructor of [`@*semiregular-box*@<T>`]{.rm} [`@*copyable-box*@<T>`]{.addu} is equivalent to:
```diff
- constexpr @*semiregular-box*@() noexcept(is_nothrow_default_constructible_v<T>)
-    : @*semiregular-box*@{in_place}
+ constexpr @*copyable-box*@() noexcept(is_nothrow_default_constructible_v<T>) @[requires default_initializable&lt;T>]{.diffins}@ 
+    : copyable-box{in_place}
{ }
```
* [1.3]{.pnum} If `copyable<T>` is not modeled, the copy assignment operator is equivalent to:
```diff
- @*semiregular-box*@& operator=(const @*semiregular-box*@& that)
+ @*copyable-box*@& operator=(const @*copyable-box*@& that)
    noexcept(is_nothrow_copy_constructible_v<T>)
  {
+   if (this != addressof(that)) {  
      if (that) emplace(*that);
      else reset();
+   }
    return *this;
  }
```
* [1.4]{.pnum}  If `movable<T>` is not modeled, the move assignment operator is equivalent to:
```diff
- @*semiregular-box*@& operator=(@*semiregular-box*@&& that)
+ @*copyable-box*@& operator=(@*copyable-box*@&& that)
    noexcept(is_nothrow_move_constructible_v<T>)
  {
+   if (this != addressof(that)) {
      if (that) emplace(std::move(*that));
      else reset();
+   }
    return *this;
  }
```

::: addu
[2]{.pnum} *Recommended Practice*: `@*copyable-box*@<T>` should just store a `T` if either `T` models `copyable` or `is_nothrow_copy_constructible_v<T> && is_nothrow_copy_constructible_v<T>` is `true`.
:::
:::

Remove the default constructor and default member initializer from [range.ref.view]{.sref}:

::: bq
[1]{.pnum} `ref_view` is a `view` of the elements of some other `range`.
```diff
namespace std::ranges {
  template<range R>
    requires is_object_v<R>
  class ref_view : public view_interface<ref_view<R>> {
  private:
-   R* r_ @[ = nullptr]{.diffdel}@;            // exposition only
+   R* r_;                      // exposition only
  public:
-   constexpr ref_view() noexcept = default;
    
    // ...
  };
}
```
:::

Constrain the defaulted default constructor and switch to _`copyable-box`_ in [range.filter.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_range V, indirect_unary_predicate<iterator_t<V>> Pred>
    requires view<V> && is_object_v<Pred>
  class filter_view : public view_interface<filter_view<V, Pred>> {
  private:
    V base_ = V();                // exposition only
-   @[*semiregular-box*]{.diffdel}@<Pred> pred_;  // exposition only
+   @[*copyable-box*]{.diffins}@<Pred> pred_;     // exposition only

    // [range.filter.iterator], class filter_view​::​iterator
    class iterator;                     // exposition only
    // [range.filter.sentinel], class filter_view​::​sentinel
    class sentinel;                     // exposition only

  public:
-   filter_view() = default;
+   filter_view() @[requires default_initializable&lt;V> && default_initializable&lt;Pred>]{.diffins}@ = default;
    constexpr filter_view(V base, Pred pred);

    // ...
  };

  // ...
}
```
:::

Constrain the defaulted default constructor in [range.filter.iterator]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_range V, indirect_unary_predicate<iterator_t<V>> Pred>
    requires view<V> && is_object_v<Pred>
  class filter_view<V, Pred>::iterator {
  private:
    iterator_t<V> current_ = iterator_t<V>();   // exposition only
    filter_view* parent_ = nullptr;             // exposition only
  public:
    // ...

-   iterator() = default;
+   iterator() @[requires default_initializable&lt;iterator_t&lt;V>>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

Constrain the defaulted default constructor and switch to _`copyable-box`_ in [range.transform.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_range V, copy_constructible F>
    requires view<V> && is_object_v<F> &&
             regular_invocable<F&, range_reference_t<V>> &&
             can-reference<invoke_result_t<F&, range_reference_t<V>>>
  class transform_view : public view_interface<transform_view<V, F>> {
  private:
    // [range.transform.iterator], class template transform_view​::​iterator
    template<bool> struct iterator;             // exposition only
    // [range.transform.sentinel], class template transform_view​::​sentinel
    template<bool> struct sentinel;             // exposition only

    V base_ = V();            // exposition only
-   @[*semiregular-box*]{.diffdel}@<F> fun_;  // exposition only
+   @[*copyable-box*]{.diffins}@<F> fun_;     // exposition only

  public:
-   transform_view() = default;
+   transform_view() @[requires default_initializable&lt;V> && default_initializable&lt;F>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor in [range.transform.iterator]{.sref}:


::: bq
```diff
namespace std::ranges {
  template<input_range V, copy_constructible F>
    requires view<V> && is_object_v<F> &&
             regular_invocable<F&, range_reference_t<V>> &&
             can-reference<invoke_result_t<F&, range_reference_t<V>>>
  template<bool Const>
  class transform_view<V, F>::iterator {
  private:
    using Parent = @*maybe-const*@<Const, transform_view>;          // exposition only
    using Base = @*maybe-const*@<Const, V>;                         // exposition only
    iterator_t<Base> current_ = iterator_t<Base>();             // exposition only
    Parent* parent_ = nullptr;                                  // exposition only
  public:
    // ...

-   iterator() = default;
+   iterator() @[requires default_initializable&lt;iterator_t&lt;Base>>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

Constrain the defaulted default constructor in [range.take.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V>
  class take_view : public view_interface<take_view<V>> {
  private:
    V base_ = V();                                      // exposition only
    range_difference_t<V> count_ = 0;                   // exposition only
    // [range.take.sentinel], class template take_view​::​sentinel
    template<bool> struct sentinel;                     // exposition only
  public:
-   take_view() = default;
+   take_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor and switch to _`copyable-box`_ in [range.take.while.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V, class Pred>
    requires input_range<V> && is_object_v<Pred> &&
             indirect_unary_predicate<const Pred, iterator_t<V>>
  class take_while_view : public view_interface<take_while_view<V, Pred>> {
    // [range.take.while.sentinel], class template take_while_view​::​sentinel
    template<bool> class sentinel;                      // exposition only

    V base_ = V();                // exposition only
-   @[*semiregular-box*]{.diffdel}@<Pred> pred_;  // exposition only
+   @[*copyable-box*]{.diffins}@<Pred> pred_;     // exposition only

  public:
-   take_while_view() = default;
+   take_while_view() @[requires default_initializable&lt;V> && default_initializable&lt;Pred>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor in [range.drop.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V>
  class drop_view : public view_interface<drop_view<V>> {
  public:
-   drop_view() = default;
+   drop_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...
  private:
    V base_ = V();                              // exposition only
    range_difference_t<V> count_ = 0;           // exposition only
  };

  // ...
}
```
:::

Constrain the defaulted default constructor and switch to _`copyable-box`_ in [range.drop.while.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V, class Pred>
    requires input_range<V> && is_object_v<Pred> &&
             indirect_unary_predicate<const Pred, iterator_t<V>>
  class drop_while_view : public view_interface<drop_while_view<V, Pred>> {
  public:
-   drop_while_view() = default;
+   drop_while_view() @[requires default_initializable&lt;V> && default_initializable&lt;Pred>]{.diffins}@ = default;
    
    // ...

  private:
    V base_ = V();                // exposition only
-   @[*semiregular-box*]{.diffdel}@<Pred> pred_;  // exposition only
+   @[*copyable-box*]{.diffins}@<Pred> pred_;     // exposition only
  };

  template<class R, class Pred>
    drop_while_view(R&&, Pred) -> drop_while_view<views::all_t<R>, Pred>;
}
```
:::

Constrain the defaulted default constructor and switch to `optional` in [range.join.view]{.sref} [This change conflicts with the proposed change in [@P2328R0]. If that change is applied, only the constraint on the default constructor needs to be added]{.ednote}:

::: bq
```diff
namespace std::ranges {
  template<input_range V>
    requires view<V> && input_range<range_reference_t<V>> &&
             (is_reference_v<range_reference_t<V>> ||
              view<range_value_t<V>>)
  class join_view : public view_interface<join_view<V>> {
  private:
    using InnerRng =                    // exposition only
      range_reference_t<V>;
    // [range.join.iterator], class template join_view​::​iterator
    template<bool Const>
      struct iterator;                  // exposition only
    // [range.join.sentinel], class template join_view​::​sentinel
    template<bool Const>
      struct sentinel;                  // exposition only

    V base_ = V();                      // exposition only
-   views::all_t<InnerRng> inner_ =     // exposition only, present only when !is_reference_v<InnerRng>
-     views::all_t<InnerRng>();
+   optional<views::all_t<InnerRng>> inner_;   // exposition only, present only when !is_reference_v<InnerRng>
  public:
-   join_view() = default;
+   join_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...
  };

  // ..
}
```
:::

Constrain the defaulted default constructor in [range.join.iterator]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_range V>
    requires view<V> && input_range<range_reference_t<V>> &&
             (is_reference_v<range_reference_t<V>> ||
              view<range_value_t<V>>)
  template<bool Const>
  struct join_view<V>::iterator {
  private:
    using @*Parent*@    = @*maybe-const*@<Const, join_view>;            // exposition only
    using @*Base*@      = @*maybe-const*@<Const, V>;                    // exposition only
    using @*OuterIter*@ = iterator_t<@*Base*@>;                         // exposition only
    using @*InnerIter*@ = iterator_t<range_reference_t<@*Base*@>>;      // exposition only

    static constexpr bool @*ref-is-glvalue*@ =                      // exposition only
      is_reference_v<range_reference_t<@*Base*@>>;

    @*OuterIter*@ outer_ = @*OuterIter*@();                             // exposition only
    @*InnerIter*@ inner_ = @*InnerIter*@();                             // exposition only
    @*Parent*@* parent_ = nullptr;                                  // exposition only

    constexpr void @*satisfy*@();                                   // exposition only
  public:
    // ...

-   iterator() = default;
+   iterator() @[requires default_initializable&lt;*OuterIter*> && default_initializable&lt;*InnerIter*>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

Update the implementation parts of `join_view::iterator` now that `inner_` is an `optional` in [range.join.iterator]{.sref} (dereferencing `parent_->inner_` in both `satisfy()` and `operator++()`) [Don't apply these changes if [@P2328R0] is adopted]{.ednote}:

::: bq
```
constexpr void @*satisfy*@();       // exposition only
```
[5]{.pnum} *Effects*: Equivalent to:
```diff
  auto update_inner = [this](range_reference_t<@*Base*@> x) -> auto& {
    if constexpr (@*ref-is-glvalue*@) // x is a reference
      return x;
    else
-     return (@*parent_*@->@*inner_*@ = views::all(std::move(x)));
+     @*parent_*@->@*inner_*@ = views::all(std::move(x));
+     return *@*parent_*@->@*inner_*@;
  };
  
  for (; outer_ != ranges::end(@*parent_*@->@*base_*@); ++@*outer_*@) {
    auto& inner = update_inner(*@*outer_*@);
    @*inner_*@ = ranges::begin(inner);
    if (@*inner_*@ != ranges::end(inner))
      return;
  }
  
  if constexpr (@*ref-is-glvalue*@)
    @*inner_*@ = @*InnerIter*@();
```

```
constexpr @*iterator*@& operator++();
```
[9]{.pnum} Let `@*inner-range*@` be:

* [9.1]{.pnum} If `@*ref-is-glvalue*@` is `true`, `*@*outer_*@`.
* [9.2]{.pnum} Otherwise, `@[*]{.addu}@@*parent_*@->@*inner_*@`.

[10]{.pnum} *Effects*: Equivalent to:
```
auto&& inner_rng = @*inner-range*@;
if (++@*inner_*@ == ranges::end(inner_rng)) {
  ++@*outer_*@;
  @*satisfy*@();
}
return *this;
```
:::


Constrain the defaulted default constructor in [range.split.view]{.sref} and change the implementation to use `optional<iterator_t<V>>` instead of a defaulted `iterator_t<V>` (same as `join`) [The same kind of change needs to be applied to [@P2210R2] which is not yet in the working draft. The use of `optional` here should also be changed to _`non-propagating-cache`_ with the adoption of [@P2328R0].]{.ednote}:

::: bq
```diff
namespace std::ranges {
  template<auto> struct require-constant;       // exposition only

  template<class R>
  concept tiny-range =                          // exposition only
    sized_range<R> &&
    requires { typename require-constant<remove_reference_t<R>::size()>; } &&
    (remove_reference_t<R>::size() <= 1);

  template<input_range V, forward_range Pattern>
    requires view<V> && view<Pattern> &&
             indirectly_comparable<iterator_t<V>, iterator_t<Pattern>, ranges::equal_to> &&
             (forward_range<V> || tiny-range<Pattern>)
  class split_view : public view_interface<split_view<V, Pattern>> {
  private:
    V base_ = V();                              // exposition only
    Pattern pattern_ = Pattern();               // exposition only
-   iterator_t<V> current_ = iterator_t<V>();   // exposition only, present only if !forward_range<V>
+   optional<iterator_t<V>> current_;           // exposition only, present only if !forward_range<V>
    // [range.split.outer], class template split_view​::​outer-iterator
    template<bool> struct outer-iterator;       // exposition only
    // [range.split.inner], class template split_view​::​inner-iterator
    template<bool> struct inner-iterator;       // exposition only
  public:
-   split_view() = default;
+   split_view() @[requires default_initializable&lt;*V*> && default_initializable&lt;*Pattern*>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Change the description to dereference what is now an `optional` in [range.split.outer]{.sref}:

::: bq

[1]{.pnum} Many of the specifications in [range.split] refer to the notional member `@*current*@` of `@*outer-iterator*@`.
`@*current*@` is equivalent to `@*current_*@` if `V` models `forward_range`, and `@[*]{.addu}@@*parent_*@->@*current_*@` otherwise.
:::

Constrain the defaulted default constructor in [range.common.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V>
    requires (!common_range<V> && copyable<iterator_t<V>>)
  class common_view : public view_interface<common_view<V>> {
  private:
    V base_ = V();  // exposition only
  public:
-   common_view() = default;
+   common_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor in [range.reverse.view]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<view V>
    requires bidirectional_range<V>
  class reverse_view : public view_interface<reverse_view<V>> {
  private:
    V base_ = V();  // exposition only
  public:
-   reverse_view() = default;
+   reverse_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...
  };
  
  // ...
}
```
:::

Constrain the defaulted default constructor in [range.elements.view]{.sref}:


::: bq
```diff
namespace std::ranges {
  // ...

  template<input_range V, size_t N>
    requires view<V> && has-tuple-element<range_value_t<V>, N> &&
             has-tuple-element<remove_reference_t<range_reference_t<V>>, N> &&
             returnable-element<range_reference_t<V>, N>
  class elements_view : public view_interface<elements_view<V, N>> {
  public:
-   elements_view() = default;
+   elements_view() @[requires default_initializable&lt;V>]{.diffins}@ = default;
    
    // ...

  private:
    // [range.elements.iterator], class template elements_view​::​iterator
    template<bool> struct iterator;                     // exposition only
    // [range.elements.sentinel], class template elements_view​::​sentinel
    template<bool> struct sentinel;                     // exposition only
    V base_ = V();                                      // exposition only
  };
}
```
:::

Constrain the defaulted default constructor in [range.elements.iterator]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_range V, size_t N>
    requires view<V> && has-tuple-element<range_value_t<V>, N> &&
             has-tuple-element<remove_reference_t<range_reference_t<V>>, N> &&
             returnable-element<range_reference_t<V>, N>
  template<bool Const>
  class elements_view<V, N>::iterator {                 // exposition only
    using @*Base*@ = @*maybe-const*@<Const, V>;                 // exposition only

    iterator_t<@*Base*@> current_ = iterator_t<@*Base*@>();     // exposition only

    static constexpr decltype(auto) @*get-element*@(const iterator_t<@*Base*@>& i);     // exposition only

  public:
    // ...

-   iterator() = default;
+   iterator() @[requires default_initializable&lt;iterator_t&lt;*Base*>>]{.diffins}@ = default;
    
    // ...
  };
}
```
:::

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