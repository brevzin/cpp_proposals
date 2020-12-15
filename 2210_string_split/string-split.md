---
title: "Superior String Splitting"
document: P2210R1
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Revision History

Since [@P2210R0], corrected the explanation of `const`-iteration and corrected that it is sort of possible to build a better `split` on top of the existing one. Changed the proposal to keep the preexisting `split` with its semantics under a different name instead. 

# Introduction

Let's say you have a string like `"1.2.3.4"` and you want it turn it into
a range of integers. You might expect to be able to write:

```cpp
std::string s = "1.2.3.4";

auto ints =
    s | views::split('.')
      | views::transform([](auto v){
            int i = 0;
            std::from_chars(v.data(), v.size(), &i);
            return i;
        })
```

But that doesn't work. Nor does this:

```cpp
std::string s = "1.2.3.4";

auto ints =
    s | views::split('.')
      | views::transform([](auto v){
          return std::stoi(std::string(v.begin(), v.end()));
        });
}
```

although for a different reason.

The problem ultimately is that splitting a `string` using C++20's `views::split`
gives you a range that is _only_ a forward range. Even though the source range
is contiguous! And that forward range isn't a common range either. As a result,
can't use `from_chars` (it needs a pointer) and can't construct a `std::string`
(its range constructor takes an iterator pair, not iterator/sentinel).

## Can we get there from here

The reason it's like this is that `views::split` is maximally lazy. It kind of
needs to be in order to support splitting an input range. But the intent of
laziness is that you can build more eager algorithms on top of them. We can
sort of do that with this one:

```cpp
std::string input = "1.2.3.4";
auto parts = input | views::split('.')
                   | views::transform([](auto r){
                        auto b = r.begin();
                        auto e = ranges::next(b, r.end());
                        return ranges::subrange(b.base(), e.base());
                     });
```

This implementation requires `b.base()` (`split_view`'s iterators do not provide
such a thing at the moment), but once we add that, this solution provides the
right overall shape that we need. The individual elements of `parts` here are
contiguous views, which will have a `data()` member function that can be passed
to `std::from_chars`.

Given the range adapter closure functionality, we could just stash this somewhere:

```cpp
template <typename Pattern>
auto split2(Pattern pattern) {
    return views::split('.')
         | views::transform([](auto r){
              auto b = r.begin();
              auto e = ranges::next(b, r.end());
              return ranges::subrange(b.base(), e.base());
           });    
}

std::string input = "1.2.3.4";
auto parts = input | split2('.');
```

Is this good enough?

It's better, but there's some downsides with this. First, the above fares pretty badly with input iterators - since we've already consumed the whole range before even providing it to the user. If they touch the range in any way, it's undefined behavior. The narrowest possible contrast?

We can fix that:

```cpp
template <typename Pattern>
auto split2(Pattern pattern) {
    return views::split('.')
         | views::transform([](auto r){
                if constexpr (ranges::forward_iterator<decltype(r.begin().base())>) {
                    auto b = r.begin();
                    auto e = ranges::next(b, r.end());
                    return ranges::subrange(b.base(), e.base());
                } else {
                    return r;
                }
           });    
}
```

Now we properly handle input ranges as well, although we can't really avoid the extra completely-pointless `transform` writing a proper range adapter closure ourselves. 

Second, this has the problem that every iterator dereference from the resulting view has to do a linear search. That's a cost that's incurred entirely due to this implementation strategy. This cost could be alleviated by using [@P2214R0]'s suggested `views::cache_latest`, though this itself has the problem that it demotes the resulting range to input-only... whereas a `split_view` could be a forward range. 

Third, this is complicated! There are two important points here:

1. `split` is overwhelmingly likely to be used on, specifically, a contiguous
range of characters.
2. `split`ting a `string` is a common operation to want to do.

While I've `transform`ed and `filter`ed all sorts of other kinds of ranges, and
have appreciated the work that went into making them as flexible as they are,
I've really never wanted to split anything other than a `string` (or a
`string_view` or other equivalent types). But for the most common use case of a
fairly common operation, the `views::split` that we have ends up falling short
because of what it gives us: a forward range. Pretty much every interesting
string algorithm requires more than that:

* The aforementioned `std::from_chars` requires a contiguous range (really,
specifically, a `char const*` and a length)
* `std::regex` and `boost::regex` both require a bidirectional range.
* Many text and unicode algorithms require a bidirectional range.

It would be great if `views::split` could work in such a way that the result
was maximally usable - which in this case means that splitting a contiguous
range should provide contiguous sub-ranges.

# Design

I wrote a blog on this topic [@revzin.split], which implements a version of
`split_view` that operates on contiguous ranges (and naughtily partially
specializes `std::ranges::split_view`) such that the following actually works:

```cpp
auto ip = "127.0.0.1"s;
auto parts = ip | std::views::split('.')
                | std::views::transform([](std::span<char const> s){
                      int i;
                      std::from_chars(s.data(), s.data() + s.size(), i);
                      return i;
                  });

struct zstring_sentinel {
    bool operator==(char const* p) const {
        return *p == '\0';
    }
};

struct zstring : view_interface<zstring> {
    char const* p = nullptr;
    zstring() = default;
    zstring(char const* p) : p(p) { }
    auto begin() const { return p; }
    auto end() const { return zstring_sentinel{}; }
};

char const* words = "A quick brown fox";
for (std::span ss : zstring{words} | std::views::split(' ')) {
    auto sv = std::string_view(ss.data(), ss.size());
    std::cout << sv << '\n';
}
```

There are a few big questions to be resolved here:


## What should the reference type of this be?

Given a range `V`, the reference type of `split_view<V>` should be
`subrange<iterator_t<V>>`. This is a generic solution, that works well for all
range categories (as long as they're forward-or-better).

For splitting a `string`, this means we get a range of `subrange<string::iterator>`
where we might wish we got a `span<char const>` or a `string_view`, but
`span<char const>` is already constructible from `subrange<string::iterator>` and
`string_view` would be with the adoption of [@P1989R0].

We could additionally favor the `char` case (since, again, splitting `string`s
is the overwhemlingly common case) by doing something like:

```cpp
struct reference : subrange<iterator_t<V>> {
    using subrange::subrange;
    
    operator string_view() const
        requires same_as<range_value_t<V>, char>
              && contiguous_range<V>
    {
        return {this->data(), this->size()};
    }
};
```

But this just seems weirdly specific and not a good idea.


## How would `const` iteration work?

The big issue for moving forward with `std::ranges::split_view` is how to
square some constraints:

1. `begin()` must be amortized constant time to model `range`
2. Member functions that are marked `const` in the standard library cannot
introduce data results (see [res.on.data.races]{.sref}).
3. The existing `split_view` is `const`-iterable.

So how do we get the first piece? What I did as part of my implementation was
to not allow `const`-iteration. `split_view` just has an `optional<iterator>`
member which is populated the first time you call `begin` and then is just
returned thereafter. This is similar to other views that need to do arbitrary
work to yield the first element (canonically, `filter_view`). See also [@issue385].

You can't do this in a `const` member function. We cannot simply specify the
`optional<iterator>` member as `mutable`, since two threads couldn't safely
update it concurrently. We _could_ introduce a synchronization mechanism
into `split_view` which would safely allow such concurrent modification, but
that's a very expensive solution which would also pessimize the typical
non-const-iteration case. So we _shouldn't_. 

So that's okay, we just don't support `const`-iteration. What's the big deal?

Well, as I noted, the _existing_
`split_view` _is_ `const`-iterable - because it's lazy! It doesn't yield
`subrange`s, it yields a lazy range. So it doesn't actually need to do work in
`begin()` - this is a non-issue.

On the other hand, any kind of implementation that eagerly
produces `subrange`s for splitting a `string const` necessarily has to
do work to get that first `subrange` and that ends up being a clash with the existing
design.

The question is - how much code currently exists that iterates over, 
specifically, a `const split_view` that is splitting a contiguous range? It's
likely to be exceedingly small, both because of the likelihood if such iteration
to begin with (you'd have to _specifically_ declare an object of type `split_view`
as being `const`) and the existing usability issues described in this paper.
Plus, at this point, only libstdc++ ships `split_view`.

Personally, I think the trade-off is hugely in favor of making this change -
it makes `split` substantially more useful.

## What about input ranges?

This strategy of producing a range of `subrange<iterator_t<V>>` works fine for
forward ranges, and is a significant improvement over status quo for bidirectional,
random access, and contiguous ranges. 

But it fails miserably for input ranges, which the current `views::split` supports.
While `const`-iteration doesn't strike me as important functionality, being able
to `split` an input range definitely does. 

A `subrange`-yielding split could still fall back to the existing `views::split`
behavior for splitting input ranges, but then we end up with fairly different
semantics for splitting input ranges and splitting forward-or-better ranges. But
we also don't want to _not_ support splitting input ranges.

This leads us to the big question...

## Replace or Add

Do we _replace_ the existing `views::split` with the one outlined in this paper,
or do we add a new one?

The only way to really replace is if we have the one-view-two-semantics
implementation described in the previous section: for forward-or-better, the
view produces `subrange`s, while for input, it produces a lazy-searching range.
This just doesn't seem great from a design perspective. And if we _don't_ do that,
then the only way we can replace is by dropping input range support entirely.
Which doesn't seem great from a user perspective since we lose functionality.
Granted, we gain the ability to actually split strings well, which is drastically
more important than splitting input ranges, but still.

The alternative is to add a new kind of `views::split` that only supports splitting
forward-or-better ranges. We could adopt such a new view under a different name
(`views::split2` is the obvious choice, but `std2::split` is even better), but I think
it would be better to give the good name to the more broadly useful facility.

That is, provide a new `views::split` that produces `subrange`s and rename the
existing facility to `views::lazy_split`. This isn't a great name, since
`views::split` is _also_ a lazy split, but it's the best I've got at the moment.

Note that, as described earlier, `views::split` should not be specified (or
implemented) in terms of `views::lazy_split`, since a proper implementation of it
could be much more efficient. 

# Proposal

This paper proposes the following:

1. Rename the existing `views::split` / `ranges::split_view` to
`views::lazy_split` / `ranges::lazy_split_view`. Add `base()` member functions
to the _`inner-iterator`_ type to get back to the adapted range's iterators.

2. Introduce a new range adapter under the name `views::split` /
`ranges::split_view` with the following design:

    a. It can only support splitting forward-or-better ranges.
    b. Splitting a `V` will `subrange<iterator_t<V>>`s, ensuring that the adapted range's
    category is preserved. Splitting a bidirectional range gives out bidirectional
    subranges. Spltiting a contiguous range gives out contiguous subranges.
    c. `views::split` will not be `const`-iterable. 

This could certainly break some C++20 code. But I would argue that `views::split`
is so unergonomic for its most common intended use-case that the benefit of
making it actually usable for that case far outweighs the cost of potentially
breaking some code (which itself is likely to be small given both the usability
issues and lack of implementations thus far).

## Implementation

The implementation can be found in action here [@revzin.split.impl], but
reproduced here for clarity.

This implementation does the weird `operator string_view() const` hack
described earlier as a workaround for the lack of [@P1989R0], and currently
just hijacks the real `views::split` as a shortcut, but other than that it
should be a valid implementation.

```cpp
using namespace std::ranges;

template <forward_range V, forward_range Pattern>
    requires view<V> && view<Pattern> &&
    std::indirectly_comparable<iterator_t<V>,
                               iterator_t<Pattern>,
                               equal_to>
class split2_view
    : public view_interface<split2_view<V, Pattern>>
{
public:
    split2_view() = default;
    split2_view(V base, Pattern pattern)
        : base_(base)
        , pattern_(pattern)
    { }

    template <forward_range R>
	    requires std::constructible_from<V, views::all_t<R>>
	        && std::constructible_from<Pattern, single_view<range_value_t<R>>>
	split2_view(R&& r, range_value_t<R> elem)
	    : base_(std::views::all(std::forward<R>(r)))
	    , pattern_(std::move(elem))
	{ }

    struct sentinel;
    struct as_sentinel_t { };

    class iterator {
    private:
        friend sentinel;

        split2_view* parent = nullptr;
        iterator_t<V> cur = iterator_t<V>();
        iterator_t<V> next = iterator_t<V>();

    public:
        iterator() = default;
        iterator(split2_view* p)
            : parent(p)
            , cur(std::ranges::begin(p->base_))
            , next(lookup_next())
        { }
        
        iterator(as_sentinel_t, split2_view* p)
            : parent(p)
            , cur(std::ranges::end(p->base_))
            , next()
        { }

        using iterator_category = std::forward_iterator_tag;
        struct reference : subrange<iterator_t<V>> {
            using reference::subrange::subrange;

            operator std::string_view() const
                requires std::same_as<range_value_t<V>, char>
                      && contiguous_range<V>
            {
                return {this->data(), this->size()};
            }            
        };
        using value_type = reference;
        using difference_type = std::ptrdiff_t;

        bool operator==(iterator const& rhs) const {
            return cur == rhs.cur;
        }

        auto lookup_next() const -> iterator_t<V> {
            return std::ranges::search(
                subrange(cur, std::ranges::end(parent->base_)),
                parent->pattern_
                ).begin();
        }

        auto operator++() -> iterator& {
            cur = next;
            if (cur != std::ranges::end(parent->base_)) {
                cur += distance(parent->pattern_);
                next = lookup_next();
            }
            return *this;
        }
        auto operator++(int) -> iterator {
            auto tmp = *this;
            ++*this;
            return tmp;
        }

        auto operator*() const -> reference {
            return {cur, next};
        }
    };

    struct sentinel {
        bool operator==(iterator const& rhs) const {
            return rhs.cur == sentinel;
        }

        sentinel_t<V> sentinel;
    };


    auto begin() -> iterator {
        if (not cached_begin_) {
            cached_begin_.emplace(this);
        }
        return *cached_begin_;
    }
    auto end() -> sentinel {
        return {std::ranges::end(base_)};
    }

    auto end() -> iterator requires common_range<V> {
        return {as_sentinel_t(), this};
    }

private:
    V base_ = V();
    Pattern pattern_ = Pattern();
    std::optional<iterator> cached_begin_;
};

// It's okay if you're just writing a paper?
namespace std::ranges {
    template<forward_range V, forward_range Pattern>
    requires view<V> && view<Pattern>
      && indirectly_comparable<iterator_t<V>, iterator_t<Pattern>, equal_to>
    class split_view<V, Pattern> : public split2_view<V, Pattern>
    {
        using split2_view<V, Pattern>::split2_view;
    };
}
```

# Wording

Change [ranges.syn]{.sref} to add the new view:

```diff
  // [range.split], split view
  template<class R>
    concept tiny-range = see below;   // exposition only

  template<input_range V, forward_range Pattern>
    requires view<V> && view<Pattern> &&
             indirectly_comparable<iterator_t<V>, iterator_t<Pattern>, ranges::equal_to> &&
             (forward_range<V> || tiny-range<Pattern>)
- class split_view;
+ class @[lazy_]{.diffins}@split_view;

+ template<forward_range V, forward_range Pattern>
+   requires view<V> && view<Pattern> &&
+            indirectly_comparable<iterator_t<V>, iterator_t<Pattern>, ranges::equal_to>
+ class split_view;

- namespace views { inline constexpr unspecified split = unspecified; }
+ namespace views {
+   inline constexpr unspecified split = @_unspecified_@;
+   inline constexpr unspecified lazy_split = @_unspecified_@;
+ }
```

---
references:
  - id: revzin.split
    citation-label: revzin.split
    title: "Implementing a better views::split"
    author:
        - family: Barry Revzin
    issued:
        - year: 2020
    URL: https://brevzin.github.io/c++/2020/07/06/split-view/
  - id: revzin.split.impl
    citation-label: revzin.split.impl
    title: "Implementation of `split2_view`"
    author:
        - family: Barry Revzin
    issued:
        - year: 2020    
    URL: https://godbolt.org/z/Y9Pec8
  - id: issue385
    citation-label: issue385
    title: "`const`-ness of view operations"
    author:
        - family: Casey Carter
    issued:
        - year: 2016
    URL: https://github.com/ericniebler/range-v3/issues/385
---
