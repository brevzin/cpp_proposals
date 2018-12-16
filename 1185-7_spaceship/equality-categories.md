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

## Would this actually be useful

We do not currently have a programmatic way of differentiating partial and total equality. Would we actually want one and where we would we use such a thing? The motivation would have to come from wanting a particular algorithm to fail to compile rather than be undefined behavior at runtime. 

The first example to consider is `find()`. Let's take a simplified form of the algorithm where the value we're searching for has to match the range's value type.

    :::cpp
    template <Range R>
        requires TotalEquality<range_value_t<R>>
    iterator_t<R> find1(R&, range_value_t<R> const&);
    
    template <Range R>
        requires EqualityComparable<range_value_t<R>>
    iterator_t<R> find2(R&, range_value_t<R> const& value)
        [[ expects: value == value ]];
        
With `find1()`, searching a value in a range of `float`s would just fail to compile. with `find2()`, it would compile - but if you tried to search for `NaN`, that would be a precondition failure. If no handler is installed, then the result would just be that `find2()` returns the `end()` of the range, regardless of its contents. To me, `find2()` seems strictly more useful - even in the case of `NaN`, while it's a logical failure, it's probably not going to cause harm?

However, consider a different example - a simplified form of hashtable:

    :::cpp
    template <TotalEquality Key, typename Value, typename Hash = std::hash<Key>>
    struct hashtable1;
    
    template <EqualityComparable Key, typename Value, typename Hash = std::hash<Key>>
    struct hashtable2;  

Here, we really do want to ensure that the key type of our hashtable has a total equality. The downside of an irreflexive comparison for hashtable seems much greater than the downside of the `find()` because you can just keep inserting `NaN`s forever.

As point of precedence, we can look to Rust, which has had `PartialEq` and `Eq` as traits from the start. Rust's [`HashMap`](https://doc.rust-lang.org/std/collections/struct.HashMap.html) requires that the key type implements `Eq` - you cannot create a `HashMap` of floating point in Rust. But `Rust`'s equivalent of [`find()`](https://doc.rust-lang.org/std/iter/trait.Iterator.html#method.find) takes a predicate (there is no `find()` that takes a value that I'm aware of), so we can't look for an example there.

## Would this even be adoptable?

It's possible that the conclusion from the previous section is: yeah, we don't actually need equality comparison categories.

But suppose we decide that such a thing would provide value. How would we even adopt it? There are, at this point, so many types in so many code bases that provide `operator==`. What would we say about those types' equality comparison? Would we assume it's a total equality (an assumption that most code and most programmers probably make)? Would we assume it's a partial equality (a safer assumption, since any types which have salient floating point members that could be `NaN` would not have a total equality)? Not making any assumption seems pretty much equivalent to assuming partial equality 

If we were to write a new hashtable today, and we really want to ensure that the key type has total equality - it seems to me that the only way to do that would be enforce that the type opt-in to that behavior. The easiest and most familiar way of doing this would be a type trait. 

    :::cpp
    template <typename T> struct equality_category;
    
    // assume partial
    template <EqualityComparable T> struct equality_category<T> { using type = partial_equality; };
    
    // all the fundamentals
    template <> struct equality_category<int> { using type = strong_equalty; };
    template <> struct equality_category<float> { using type = partial_equality; };
    // ...
    
which would be easy, if tedious, to propagate
    
    :::cpp
    template <EqualityComparable T>
    struct equality_category<optional<T>> : equality_category<T> { };
    template <EqualityComparable T, EqualityComparable U>
    struct equality_category<pair<T,U>> : common_equality_category<T,U> { };
    // ...

Would this be useful?

## What about `std::strong_equal()`?

This brings me to the two equality comparison algorithms in [cmp.alg]: `std::strong_equal()` and `std::weak_equal()`. Currently, `std::strong_equal(a, b)` is defined as:

- Returns `a <=> b` if that expression is well-formed and convertible to `strong_equality`.
- Otherwise, if the expression `a <=> b` is well-formed, then the function is defined as deleted.
- Otherwise, if the expression `a == b` is well-formed and convertible to `bool`, then
    - if `a == b` is `true`, returns `strong_equality::equal`;
    - otherwise, returns `strong_equality::nonequal`.
- Otherwise, the function is defined as deleted.

After P1185, the first two bullets here don't make sense. We wouldn't want `strong_equal` to go through `operator<=>`. We would want it to only go through `==`. But then, how would we ensure that we do the right thing? As mentioned earlier, we could just _check_ that `operator<=>` returns something convertible to `strong_equality` but just call `a == b` anyway. Or we could use this type trait:

    :::cpp
    template <typename T>
      concept HasWeakEquivalence =
        EqualityComparable<T> &&
        ConvertibleTo<equality_category_t<T>, weak_equivalence>;
    
    template <typename T>
      concept HasTotalEquality =
        HasWeakEquivalence<T> &&
        Same<equality_category_t<T>, strong_equality>;
     
    template <EqualityComparable T>
    constexpr partial_equality partial_equal(const T& a, const T& b) {
      return a == b ? partial_equality::equal
                    : partial_equality::nonequal;
    }
     
    template <HasWeakEquivalence T>
    constexpr weak_equivalence weak_equivalent(const T& a, const T& b) {
      return a == b ? weak_equivalence::equivalent
                    : weak_equivalence::nonequivalent;
    }
        
    template <HasTotalEquality T>
    constexpr strong_equality strong_equal(const T& a, const T& b) {
      return a == b ? strong_equality::equal
                    : strong_equality::nonequal;
    }

Is this... better? It does provide a way to use the type trait to lift the result of existing `operator==` into the type system, if we want to actually make use of these comparison categories.

But it may prove difficult to actually provide user-defined types with the correct equality category, since we're trying to add this decades after `operator==` was introduced. 
    
[stepanov.fogp]: http://stepanovpapers.com/DeSt98.pdf "Fundamentals of Generic Programming||James C. Dehnert and Alexander Stepanov||1998"