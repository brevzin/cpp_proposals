Title: Equality categories
Document-Number: DxxxxRx
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG, LEWG

# History

[P0100R2](https://wg21.link/p0100r2), in addition to laying the three-way comparison groundwork, proposed three different boolean functions for different kinds of equality:

    :::cpp
    template<typename T> bool partial_unordered(const T&,const T&);
    template<typename T> bool weak_equivalence(const T&,const T&);
    template<typename T> bool total_equal(const T&,const T&);
    
Where `total_equal(a,b)` implies `weak_equivalence(a,b)` implies `partial_unordered(a,b)`. The paper makes clear that the notable difference between an equivalence (`weak_equivalence`) and an equality (`total_equal`) is that the latter implies substitutability whereas the former does not. In [Fundamentals of Generic Programming][stepanov.fogp], Stepanov provides the useful meaning of substitutability as `a == b` implies `f(a) == f(b)`. Notably, in R0 of this paper, the term used was `weak_equal` - it was changed in R1 to be `weak_equivalence`, to clarify that it specifies an _equivalence_ and not an _equality_.

[P0515R3](https://wg21.link/p0515r3) (adopted in Albuquerque 2017) built on top of P0100 by introducing `operator<=>` along with five comparison category types to be used as its return type:

<img src="https://cdn-images-1.medium.com/max/1250/1*_jNZaURC_swN3Iy2KFrNnQ.png" />

Note that the name of the weakest comparison category is `weak_equality`, despite its enumerators being `weak_equality::equivalent` and `weak_equality::nonequivalent`. Nevertheless, the intent of of `operator<=>` was very much that types would need to declare just one comparison function, with the one return type encoding the complete category information about the kind of comparison. The six regular comparison functions would synthesize appropriate calls to `<=>`.

[P1190R0](https://wg21.link/p1190r0) pointed out that having just `operator<=>` would be insufficient for performance reasons. Equality and ordering are different fundamental operations, and trying to use an ordering function (like `operator<=>`) to give an answer for equality could be a severe pessimization. [P1185R0](https://wg21.link/p1185r0) (approved by EWG in San Diego 2018, though not yet formally adopted) provided a solution by effectively segregating the equality comparisons from the ordering ones: `==` and `!=` would no longer synthesize calls to `<=>`.

# Motivation

Even after P1185, we still have five comparison categories. The three ordering categories (`partial_ordering`, `weak_ordering`, and `strong_ordering`) and two equality categories (`weak_equality` and `strong_equality`). The problem is, the two equality categories have questionable use.

They do not make sense as the return type of `operator<=>` because that operator is no longer used in code that invokes equality comparisons:

    :::cpp
    struct A {
        strong_equality operator<=>(A const&) const;
    };
    
    void foo(A x, A y) {
        x == y; // error: we never synthesize == from <=>
        x < y;  // error: strong_equality doesn't have an associated <
    }
    
And they also do not make sense as the return type of `operator==` because there is a very strong assumption by an enormous amount of code that `operator==` has type `bool`. And as specified, using those comparison categories as the return type of `operator==` fails in even the most basic usage:

    :::cpp
    struct B {
        strong_equality operator==(B const&) const;
    }
    
    void foo(B x, B y) {
        if (x == y) {       // error: not contextually convertible to bool
            do_something();
        }
    }
    
Furthermore, they also don't make sense to be convertible-to from the three-way comparison categories - because `<=>` is now _solely_ used for orderings. 

Moreover, there is one category that even P0515. An equivalence must be reflexive, symmetric, and transitive. But for floating point types, while `operator==` is both symmetric and transitive, it is not reflexive. It is thus a _partial_ equality.

How do you express programmatically that `float` has partial equality but `int` has total equality? Before P1185, we would do that by looking at the return type of `<=>`.  But what if I want to write a type that does _not_ have an ordering but does provide a partial equality? Would I just declare `<=>` anyway, even though using it wouldn't make sense?

    :::cpp
    struct C {
        // intended to be a partial equality
        bool operator==(C const&) const;
        
        // don't actually use this, not defined?
        std::partial_equality operator<=>(C const&) const;
    };

Or do I just stick with the comment and hope the type is used correctly? It'd be nice to have something more direct.

# Proposal

This proposal has two parts: sanitizing the equality comparison categories and allowing users to express equality comparison categories.

## Sanitizing Equality Comparison Categories

There is a missing equality comparison category: `std::partial_equality`. This paper proposes adding it. Following in the footsteps of [P1307R0](https://wg21.link/p1307r0), this paper proposes _removing_ `std::weak_equality`. 

Additionally, while still is a strong motivation for having the three three-way comparison categories be convertible (i.e. `strong_ordering` is convertible to `weak_ordering` is convertible to `partial_ordering`), there does not seem as much reason for there to be conversions from the three-way categories to the equality categories - since those two operations are now strictly separate. This paper proposes to remove those conversions from everywhere appropriate: from the types themselves, from `common_comparison_category`, and from the specification for defaulting `operator<=>`. However, `strong_equality` should be convertible to `partial_equality`, and we would need a new `common_equality_category` type trait.

Further, there does not seem to be a strong motivation for having the existing `strong_equality` or the proposed `partial_equality` actually having all the functionality they currently have. All the comparisons to `0` made sense in the context of being the return type for `<=>` - but that's not how they would be used anymore. It seems that the right way to specify those types should now be:

    :::cpp
    struct partial_equality { };
    struct strong_equality : partial_equality { };
    
As to how we would actually _use_ these types...

## Expressing Equality Comparison Categories

We still need to a way to specify that a type's equality is either a total equality or a partial equality. This comparison still _needs_ to be `bool`. As precedent, Rust provides the traits [`PartialEq`](https://doc.rust-lang.org/std/cmp/trait.PartialEq.html) and [`Eq`](https://doc.rust-lang.org/beta/std/cmp/trait.Eq.html) - where the former requires you to define a boolean function, and the latter does not have any extra associated functions and is solely annotation. 

One way to do this might be to just use a type trait mechanism, where you can opt-in by specializing a type trait:

    :::cpp
    struct C {
        bool operator==(C const&) const;
    };
    template <> struct std::equality_type<C> { using type = std::partial_equality; };
    
where composite types could propagate their equality types as appropriate:

    :::cpp
    namespace std {
      template <EqualityComparable T>
      struct equality_type<optional<T>>
        : equality_type<T>
      { };
    }

This works, and will be pretty familiar to C++ programmers. It's the same way you opt-in to hashing and structured bindings at the moment. 
    
A more aggressive solution would be to take the need of annotation, and the need for equality to return `bool`, and let the user provide an annotated return type:

    :::cpp
    struct C {
        std::partial_equality bool operator==(C const&) const;
    };
    
And simply provide a magic type trait to pick out the equality category of `C`. The point of the above would be to satisfy that `decltype(c1 == c2)` is still `bool`, and have the function still return `true` or `false`, but provide a first class type trait such that `std::equality_type_t<C>` is `std::partial_equality`. This would allow composite types to propagate the equality of their members too:

    :::cpp
    template <typename T>
    equality_type_t<T> bool operator==(optional<T> const& lhs, optional<T> const& rhs) {
        if (lhs && rhs) {
            return *lhs == *rhs;
        } else {
            return lhs.has_value() == rhs.has_value();
        }
    }

This proposal is effectively providing an observable type annotation on `operator==` (and `operator!=`, though there is less point to writing both functions after P1185).

## What about existing types?

One of the problems with migrating to a scheme with first-class comparison semantics expressed in the type system 20 years after the language was initially standardized is that there are many, many, many types in many, many, many codebases that currently define `operator==` without any kind of annotation. 

Regardless of whether we pick a library or language solution, what should `std::equality_type_t<T>` say for a type that does not have a type annotation? This is fundamentally the same problem that [P1186R0](https://wg21.link/p1186r0) ran into. It could be:

- `std::strong_equality`. Just assume that `==` is a total equality. Most code, and most programmers, basically already make this assumption. 
- `std::partial_equality`. The safer assumption, requiring users to opt-in to total equality (rather than opt-out of it). 
- `void`. Simply do not make any assumption at all on the language level, leave it up to the individual libraries to make a decision. 

I do not know what the right answer is here.

Either way, the answer is clear for the fundamental types. The floating point types have partial equality and all the other types have total equality. 

[stepanov.fogp]: http://stepanovpapers.com/DeSt98.pdf "Fundamentals of Generic Programming||James C. Dehnert and Alexander Stepanov||1998"