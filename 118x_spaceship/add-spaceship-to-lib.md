Title: Adding <=> to library
Document-Number: D1189R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: LEWG

# Introduction

This paper builds on all of David Stone's effort in [P0790R2](https://wg21.link/p0790r2) and exists just to fill in the missing holes. I believe it's very important to ship C++20 with `<=>` for all of these types - and this is my attempt to help get there. Of the types which P0790R2 does not provide a solution, they can be divided into four neat categories, each with a canonical representative. This paper will go through these four in turn:

- `std::basic_string`
- `std::vector`
- `std::optional`
- `std::unique_ptr`

Critically, no change proposed in this paper will change the semantics of any comparison. The answer that a particular comparison gave in C++17 will be the same as what that code will give in C++20. But the path taken to get to that answer may change.

## Motivation

But why should we bother? What's the motivation for doing all this work?

Major benefits of `operator<=>` are that it is easier to implement than the status quo comparisons implemented via `operator<`, that it can provide better performance than the status quo, and that it can be more correct than the status quo. But of course, none of that is true if we don't actually use it in our own library. There is the inevitable user frustration than this doesn't work:

    :::cpp
    struct Person {
        std::string last;
        std::string first;
        auto operator<=>(Person const&) const = default;
    };
    
Here is a type ready-made for a three-way comparison (indeed, it already has one), but defaulting doesn't work? So much for ease of use! 

A user might then, if [P1186R1](https://wg21.link/p1186r1) is adopted, try to do this:

    :::cpp
    struct Person {
        std::string last;
        std::string first;
        strong_ordering operator<=>(Person const&) const = default;
    };

This will, potentially, walk each string twice - when we could have performed this comparison by walking each string only once (at most). So much for performance!

We don't even need to go into user types to see this issue. Consider:

    :::cpp
    void func(std::vector<std::string> const& names, std::vector<std::string> const& other_names)
    {
        if (names < other_names) {
            // ...
        }
    }
    
Even though `std::string` already has a three-way comparison function (by way of `compare()`), the invocation of `<` above won't use it - it'll instead invoke `<` up to twice on each string. That's twice as much work as needs to be done to answer this question. If we adopt `<=>` for all of thse library types as we should, then the above comparison would go through `std::vector`'s `<=>` which would go through `std::string`'s `<=>` which ensures that we only compare each `string` at most one time. We're not just talking about making comparisons for user-defined types easier to write and more performant, we're also talking about making already existing comparisons of standard library types more performant.

And these problems stack, as the standard library types are inevitably used together. A `std::optional<std::vector<std::string>>` should provide an `operator<=>` with a comparison category of `strong_ordering`, which should be efficient, and it should really be there for C++20.

# Most `operator!=()` is obsolete

[P1185R1](https://wg21.link/p1185r1) will likely be moved in Kona. R0 was approved by EWG in San Diego. There is still an open design question, but it is about a part of that proposal which is irrelevant to this paper. Importantly, the first part of that paper changes the candidate set for inequality operators to include equality operators. In other words, for types in which `a != b` is defined to mean `!(a == b)`, we no longer need to define `operator!=`. The language will simply do the right thing for us.

Just about every `operator!=()` in the library does this. Indeed, we have blanket wording in [operators] which lets us avoid having to write the boilerplate every time. The only exceptions to this are:

- the types in the group represented by `std::optional`, which will be discussed [later](#adding-to-stdoptional)
- `std::valarray`, for which `==` doesn't really mean equality anyway

The other ~250 declarations of `operator!=()` can just be removed entirely. The semantics for all callers will remain the same. 

# Adding `<=>` to `std::basic_string`

This group is composed of templates which implement their comparisons via `Traits::compare()`: `std::basic_string`, `std::basic_string_view`, and, `std::sub_match`. Because `Traits::compare()` is already a three-way comparison, this seems like a trivial case. Just write this and ship it right?

    :::cpp
    template <typename CharT, typename Traits, typename Alloc>
    auto operator<=>(basic_string<CharT, Traits, Alloc> const& lhs,
                     basic_string<CharT, Traits, Alloc> const& rhs)
    {
        return lhs.compare(rhs) <=> 0;
    }
    
Unfortunately, since `lhs.compare(rhs)` is an `int`, this would unconditionally yield a `std::strong_ordering`. The result of this will be correct as far as all the binary comparisons though, but comparing arbitrary `basic_string`'s is not necessarily a `strong_ordering`. Indeed, the [canonical example][gotw.29] of providing a custom `Traits` for `basic_string` is to implement a case-insensitive comparison - which should be a `weak_ordering`. So we cannot simply pick `strong_ordering`. On the other hand, `std::string`'s `operator<=>` should _certainly_ yield `strong_ordering` so we cannot simply pick `weak_ordering` either. We'll have to do something slightly more complex.

We could simply bless all the standard-mandated specializations of `std::char_traits` and say that those are `strong_ordering`, and any other trait is a `weak_ordering`. But that seems overly restrictive on users who might want to provide a custom traits which provides an ordering which is either strong or partial. 

Instead, I propose one of two choices: add a member type alias to the traits expressing the comparison category, or add a new static member function performing a three-way comparison and yielding a comparison category.

## Add a type alias

This is the least intrusive option. Simply add a member alias to the standard-manded specializations of `char_traits`:

    :::cpp
    template <> struct char_traits<char> {
        using comparison_category = strong_ordering;
    };

And replace the four relational operators with an `operator<=>` whose return type is based on that category:

    :::cpp
    // exposition-only
    template <typename T>
    struct traits_category {
        using type = weak_ordering;
    };
    
    template <typename T>
        requires requires { typename T::comparison_category; }
    struct traits_category {
        using type = typename T::comparison_category;
    };
    
    template <typename T>
    using traits_category_t = typename traits_category<T>::type;
    
    // actual implementation
    template <typename CharT, typename Traits, typename Alloc>
    class basic_string {
        // ...
        friend traits_category_t<Traits> operator<=>(basic_string const& lhs, basic_string const& rhs)
        {
            return lhs.compare(rhs) <=> 0;
        }
        // ...
    };

This works because `int`'s comparison category is `strong_ordering`, which is implicitly convertible to any other comparison category.

This is a fairly small change. The downside is that any future custom traits will still have to write `compare()` functions that return an `int` instead of one of the standard comparison categories. To that end, we could:

## Add a static member function

Add a static member function returning a comparison category:

    :::cpp
    template <> struct char_traits<char> {
        static constexpr strong_ordering compare_3way(const char* s1, const char* s2, size_t count)
        {
            return compare(s1, s2, count) <=> 0;
        }
    };
    
And then use that if it is present:

    :::cpp
    template <typename CharT, typename Traits, typename Alloc>
    class basic_string {
        // ...
        friend auto operator<=>(basic_string const& lhs, basic_string const& rhs)
        {
            auto impl = [](CharT const* lhs, CharT const* rhs, size_t sz) {
                if constexpr (requires { Traits::compare_3way(lhs, rhs, sz); }) {
                    return Traits::compare_3way(lhs, rhs, sz);
                } else {
                    return static_cast<weak_ordering>(
                        Traits::compare(lhs, rhs, sz) <=> 0);
                }
            };
        
            auto cmp = impl(lhs.data(), rhs.data(), min(lhs.size(), rhs.size()));
            return cmp != 0 ? cmp : lhs.size() <=> rhs.size();
        }
        // ...
    };


This is somewhat more involved than the previous alternative, but not by much. Since we'd still need to provide a version of `compare` that returns an `int`, this may not actually be worth it - since while it's very easy to convert an `int` to a comparison category (simply `<=>` it against `0`), there's no actual easy way of going the other way without branching. 

## Proposal

I weakly favor the member type alias approach. Remove `operator<`, `operator>`, `operator<=`, and `operator>=` for `basic_string`, `basic_string_view`, and `sub_match` and replace them with `operator<=>`s which invoke `x.compare(y) <=> 0` cast to the appropriate comparison category (with argument adjustments as appropriate). This `operator<=>` should be a hidden friend.

# Adding `<=>` to `std::vector`

The vast majority of types in the standard library follow the general pattern that all the relational operators are implemented in terms of `<`:

<table>
<tr>
<th>
Source
</th>
<th>
Implemented as
</th>
</tr>
<tr>
<td>
    :::cpp
    x > y
</td>
<td>
    :::cpp
    y < x
</td>
</tr>
<tr>
<td>
    :::cpp
    x <= y
</td>
<td>
    :::cpp
    !(y < x)
</td>
</tr>
<tr>
<td>
    :::cpp
    x >= y
</td>
<td>
    :::cpp
    !(x < y)
</td>
</tr>
</table>

There is blanket wording to this effect in \[operators\]. Types that fit this category include just about all the familiar containers (e.g. `vector`, `map`, `deque`, `array`, etc.) and utility types (e.g. `pair`, `tuple`). The implication of this kind of rewrite is that the underlying types _must_ implement a total order with `<`. Otherwise, these comparisons will simply provide the wrong answer. We can make use of that to hugely simplify the implementations of all the comparisons in a way that preserves today's semantics (i.e. still being correct for total orders) but also allow them to become correct in the future with types that will provide a `<=>` that returns `partial_ordering` (e.g. floating point types).

I will first describe _how_ to implement `operator<=>` for such types, and then I will describe _why_ we want to do it this way. It will be easier to explain the why having the how to point to.

Because these templates all rely on their underlying types having an `operator<` today, we can continue to rely on that to synthesize an `operator<=>`:

    :::cpp
    template <typename T>
    struct weak_wrapper {
        T const& t;
        
        auto operator<=>(weak_wrapper const&) const requires ThreeWayComparable<T> = default;
        weak_ordering operator<=>(weak_wrapper const& rhs) const {
            if (t < rhs.t) return weak_ordering::less;
            if (rhs.t < t) return weak_ordering::greater;
            return weak_ordering::equivalent;
        }
    };
    
`ThreeWayComparable` is proposed in [P1188R0](https:://wg21.link/p1188r0). It's important that this type fallback to `weak_ordering` - all we know is that `<` exists. We cannot even check if `==` exists because such types might have a non-SFINAE-friendly equality operator. And even if we could, its presence does not mean that the equality is strong equality.

We can use the above type to implement `operator<=>` for `vector` as follows:

    :::cpp
    template <typename T>
    class vector {
        // ...
        friend auto operator<=>(vector const& rhs) const {
            return std::lexicographical_compare_3way(
                begin(), end(),
                rhs.begin(), rhs.end(),
                [](T const& a, T const& b){
                    using C = weak_wrapper<T>;
                    return C{a} <=> C{b};
                });
        }
        // ...
    };

Or for `pair` as follows, relying on `compare_3way_type_t` also from P1188R0:

    :::cpp
    template <typename T, typename U>
    class pair {
        // ...
        friend common_comparison_category<
            compare_3way_type_t<weak_wrapper<T>>,
            compare_3way_type_t<weak_wrapper<U>>,
        > operator<=>(pair const& rhs) const {
            if (auto cmp = weak_wrapper<T>{first} <=> weak_wrapper<T>{rhs.first}; cmp != 0) return cmp;
            return weak_wrapper<U>{second} <=> weak_wrapper<U>{rhs.second};
        }
        // ...
    };
    
Both cases are straightforward.

Now, the important question is why provide `<=>` for these types unconditionally? Why remove the existing relational operators, even in the case that the underlying types do not provide `<=>`? This comes back to the very initial motivation: ease of implementation and performance. By providing `<=>`, we can make it easier to adopt `<=>` elsewhere:

    :::cpp
    // some old type, can't update
    struct Legacy {
        bool operator<(Legacy const&) const;
    };
    
    // new type, just want the default comparisons
    struct New {
        std::vector<Legacy> q;
        std::string name;
        
        auto operator<=>(New const&) const = default;
    };
    
But is that really efficient?

Actually, yes. Consider how we would implement `operator<` for the type `New` today. We'd probably write something like:

    :::cpp
    bool operator<(New const& lhs, New const& rhs) {
        return std::tie(lhs.q, lhs.name) < std::tie(rhs.q, rhs.name);
    }
    
What if the `q`'s were equivalent? We end up walking through every element once (the underlying check for `lhs.q < rhs.q`) and then we end up walking through every element a second time (the underlying check for `rhs.q < lhs.q`). In the synthesized `<=>` above, this is also true - we also will have to compare every pair of `Legacy`'s twice. But, we do both comparisons together - and only have to walk through memory once. That is strictly better. 
    
The one edge-case here is for `pair`. If we were comparing two `pair<Legacy, Legacy>`s against each other with `<`, in the status quo that takes at most three invocations of `Legacy`'s `operator<` but in the synthesized implementation it could technically require four. But since that last one only differentiates between the `greater` and `equivalent` cases, and neither of those are `less`, it seems like a straightforward optimization for a compiler to make to simply remove that last comparison. At which point, we're equivalent to status quo.

It's important to reassure here that the semantics of this comparison are exactly the same for all types which provide a `<` (but not a `<=>`) which implements a total order. We're basically doing the exact same operations that `vector` (and `pair` and ...) would already be doing. There's really not much benefit to keeping around the existing relational operators - we can retire them.

## Proposal

Remove `operator<`, `operator>`, `operator<=`, and `operator>=` for `array`, `deque`, `forward_list`, `list`, `map`, `move_iterator`, `multimap`, `multiset`, `pair`, `set`, `tuple`, `unordered_map`, `unordered_multimap`, `unordered_multiset`, `unordered_set`, and `vector` (including `vector<bool>`). For each of them, add a `operator<=>` with the same comparison semantics as today which compares each type `T` as if by the exposition-only `weak_wrapper<T>` above. This `operator<=>` should be a hidden friend.
    
# Adding `<=>` to `std::optional`

This group is composed of templates which forward _all_ of their operations to underlying types. No operator is written in terms of any other operator. Even `!=` is not defined in terms of `==`. This group includes `std::optional`, `std::variant`, `std::queue`, `std::stack`, and `std::reverse_iterator`.

Unlike types like `pair` and `vector` and `tuple` which implement all the relational operators in terms of `<` (in a way that assumes a total order), the types in this group do not assume any semantics at all - and hence provide the correct answer for types that have partial orders:

    :::cpp
    struct Q {
        float f;
        bool operator==(Q const& rhs) const { return f == rhs.f; }
        bool operator< (Q const& rhs) const { return f < rhs.f; }
        bool operator> (Q const& rhs) const { return f > rhs.f; }
    };
    
    // partial order, so no trichotomy
    Q{1.f} == Q{NAN}; // false
    Q{1.f} < Q{NAN};  // false
    Q{1.f} > Q{NAN};  // false
    
    // the types in the previous category assume trichotomy
    vector<Q>{1.f} == vector<Q>{NAN}; // false
    vector<Q>{1.f} < vector<Q>{NAN};  // false
    vector<Q>{1.f} > vector<Q>{NAN};  // true?!
    
    // but the types in this category do not
    optional<Q>{1.f} == optional<Q>{NAN}; // false
    optional<Q>{1.f} < optional<Q>{NAN};  // false
    optional<Q>{1.f} > optional<Q>{NAN};  // false
    
This design for `optional` and `variant` is a result of [P0307R2](https://wg21.link/p0307r2). I do not know why `queue` and `stack` even have comparisons, or why `reverse_iterator` behaves this way.

But the important thing is that we cannot change the semantics of `optional` - the previous solution of synthesizing a `weak_ordering` from applying `operator<` in both direction is not okay here. That would change the answer for some comparisons, and this paper will not change semantics. 

It turns out we cannot synthesize a `partial_ordering` either. The only sound way to do such a synthesis would be:

    :::cpp
    partial_ordering __synthetic(Q const& a, Q const& b)
    {
        if (a == b) return partial_ordering::equivalent;
        if (a < b)  return partial_ordering::less;
        if (b < a)  return partial_ordering::greater;
        return partial_ordering::unordered;
    }

But this requires additional operations on `Q`. Today, I can compare two `optional<Q>`'s with `<` with `Q` only having `<`... and importantly I can compare two `optional<Q>`'s with `>` with `Q` only having `>`. There's no way to synthesize a `<=>` for `optional<Q>` in this scenario that has both the same semantics and the same operational requirements. That is a non-starter. 

However, it's still really important to add `<=>` to these types for the same reason it's important to add `<=>` to the other types. It's just more difficult to do. Our only option is to adopt a new `operator<=>` that is constrained on the underlying types having `<=>` ensuring that `operator<=>` is _more constrained than_ each of the two-way relational operators. Such an implementation, based on P1188R0, might look like:

    :::cpp
    template <typename T>
    class optional {
        // ...
        template <typename T1, typename T2>
            requires ThreeWayComparableWith<T1, T2>
        friend compare_3way_type_t<T1,T2> operator<=>(optional<T1> const& lhs, optional<T2> const& rhs)
        {
            if (lhs.has_value() && rhs.has_value()) {
                return *lhs <=> *rhs;
            } else {
                return lhs.has_value() <=> rhs.has_value();
            }
        }
        // ...
    };

Making the spaceship operator more constrained that the other relational operators ensures that it gets precedence over them during normal comparisons. 

## Proposal

Add a new `operator<=>` for each kind of comparison for `optional`, `variant`, `queue`, `stack`, and `reverse_iterator` such that `operator<=>` is constrained on the relevant template parameters satisfying `ThreeWayComparableWith` (or just `ThreeWayComparable`), ensuring that `operator<=>` is the best viable candidate for all relational comparisons in code. This `operator<=>` should be a hidden friend.

Do not remove any of the preexisting comparison operators for these types, including even `operator!=()`.

# Adding `<=>` to `std::unique_ptr`

For `std::unique_ptr` and `std::shared_ptr`, the comparison of `<` does not use raw `operator<` but rather goes through `std::less`. However, they do this in a slightly different way: Comparing a `unique_ptr<T1,D1>` to a `unique_ptr<T2,D2>` goes through `less<common_type_t<unique_ptr<T1,D1>::pointer, unique_ptr<T2,D2>::pointer>>` whereas comparing a `shared_ptr<T1>` to a `shared_ptr<T2>` goes through `less<>`. 

Either way, P1188R0 proposes a three-way comparison object named `std::compare_3way` that satisfies the requirement that the ordering is a strict weak order, using the same wording that we have for `std::less` today. We can use this object to implement the comparisons doing something like:

    :::cpp
    template <typename T1, typename D1, typename T2, typename D2>
    auto operator<=>(unique_ptr<T1,D1> const& p1, unique_ptr<T2,D2> const& p2)
    {
        using CT = common_type_t<typename unique_ptr<T1,D1>::pointer, typename unique_ptr<T2,D2>::pointer>;
        return compare_3way<CT>()(p1.get(), p2.get());
    }
    
    template <typename T, typename U>
    auto operator<=>(shared_ptr<T> const& a, shared_ptr<U> const& b)
    {
        return compare_3way()(a.get(), b.get());
    }
   
## Proposal

Remove `operator<`, `operator>`, `operator<=`, and `operator>=` for `unique_ptr` and `shared_ptr` and replace them with `operator<=>`s which go through the proposed `compare_3way` function object in the same way that the preexisting comparisons do today. These `operator<=>`s should be hidden friends.

# Non-member, hidden friend, or member?

Through this paper, I've been suggesting that the added `operator<=>`s should be hidden friends. There are actually three options here:

- non-member functions
- hidden friends
- member functions

Most (if not all?) of these operators are non-member functions today. This has the downside of polluting scope and increasing then number of candidates for each of the comparisons that overload resolution has to look through. It has the additional downside of allowing surprising conversions in certain cases simply because of ADL (see the `path` example). This leads many people to rightly suggest to make operators hidden friends instead, which maintains the advantage of non-member functions by allowing conversions in both arguments but reduces the number of candidates for unrelated types and doesn't have surprise conversions in both arguments.

But with `operator<=>` we can actually take a third approach: member functions. Because `<=>` considers reversed candidates as well, a member function is sufficient:

    :::cpp
    struct X {
        X(int);
        bool operator==(X const&) const;
        strong_ordering operator<=>(X const&) const;
    };
    
    // all of these are ok
    X{42} < X{57};
    X{42} < 57;
    42 < X{57};
    42 == X{57};
    X{42} != 57;

Everyone understands member functions, so maybe we should just use member functions? The question is really more about what kinds of double-conversions we want to allow. In this particular example:

    :::cpp
    X a{42}, b{57};
    a < b;                     // ok
    std::ref(a) < std::ref(b); // ill-formed
    
If we made `operator<=>` a hidden friend, the comparison through `std::reference_wrapper<X>` would compile.

Today, `std::reference_wrapper<std::string>` is not comparable. Adding a new `operator<=>` as a hidden friend would make it comparable. Making the `operator<=>` a member function would make it not comparable - which is the status quo. Perhaps it's better to leave things as they are, and add comparisons to `std::reference_wrapper<T>` if we really care about this case?
    
[gotw.29]: http://www.gotw.ca/gotw/029.htm "GotW #29: Strings||Herb Sutter||1998-01-03"