Title: `<=> != ==`
Document-Number: D1185R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG

# Motivation

[P0515](https://wg21.link/p0515r3) introduced `operator<=>` as a way of generating all six comparison operators from a single function, as well as the ability to default this so as to avoid writing any code at all. See David Stone's [I did not order this!][Stone.Order] for a very clear, very thorough description of the problem: it does not seem to be possible to implement `<=>` optimally for "wrapper" types. What follows is a super brief rundown.

Consider a type like:

    :::cpp
    struct S {
        vector<string> names;
        auto operator<=>(S const&) const = default;
    };
    
Today, this is ill-formed, because `vector` does not implement `<=>`. In order to make this work, we need to add that implementation. It is _not_ recommended that `vector` only provide `<=>`, but we will start there and it will become clear why that is the recommendation.

The most straightforward implementation of `<=>` for `vector` is (let's just assume `strong_ordering` and note that I'm deliberately not using `std::lexicographical_compare_3way()` for clarity):

    :::cpp
    template<typename T>
    strong_ordering operator<=>(vector<T> const& lhs, vector<T> const& rhs) {
        size_t min_size = min(lhs.size(), rhs.size());
        for (size_t i = 0; i != min_size; ++i) {
            if (auto const cmp = compare_3way(lhs[i], rhs[i]); cmp != 0) {
                return cmp;
            }
        }
        return lhs.size() <=> rhs.size();
    }

On the one hand, this is great. We wrote one function instead of six, and this function is really easy to understand too. On top of that, this is a really good implementation for `<`!  As good as you can get. And our code for `S` works (assuming we do something similar for `string`).

On the other hand, as David goes through in a lot of detail (seriously, read it) this is quite bad for `==`. We're failing to short-circuit early on size differences, and on top of that, we're doing more work in the body than we need to. `compare_3way()` on types that don't implement `<=>` will basically do:

    :::cpp
    template <typename T>
    strong_ordering compare_3way(T const& lhs, T const& rhs) {
        if (x == y) return strong_ordering::equal;
        if (x < y) return strong_ordering::less;
        return strong_ordering::greater;
    }    
    
If we're doing `==` on the outer container, we don't care if `lhs[i] != rhs[i]` because `lhs[i] < rhs[i]` or `lhs[i] > rhs[i]`. We just care that they're unequal. So this extra comparison is unnecessary. Granted, this is one extra unwanted comparison as compared to the arbitrarily many unwanted comparisons from not short-circuiting, but it's still less than ideal.

In order to do `==` efficiently, we have to short-circuit and do `==` all the way down. That is:

    :::cpp
    template<typename T>
    bool operator==(vector<T> const& lhs, vector<T> const& rhs)
    {
        // short-circuit on size early
        const size_t size = lhs.size();
        if (size != rhs.size()) {
            return false;
        }
    
        for (size_t i = 0; i != size; ++i) {
            // use ==, not <=>, in all nested comparisons
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        
        return true;
    }
    
    // ... and have to write this one manually today
    template<typename T>
    bool operator!=(vector<T> const& lhs, vector<T> const& rhs)
    {
        return !(lhs == rhs);
    }

We have the initial problem that we have this false sense of security - the easy thing we wrote generates bad code.

But even if we write this more efficient `==` for containers (`vector`, `string`, etc.), this still doesn't solve our problem. When we do an equality comparison on our `S` above, that will still go through `<=>` which calls `<=>` all the way down!

The only way to get efficiency is to have every type, even `S` above, implement both not just `<=>` but also `==` and `!=`. That is the status quo today and the problem that needs to be solved.

## Other Languages

In order how to best figure out how to solve this problem for C++, it is helpful to look at how other languages have already addressed this issue. While P0515 listed many languages which have a three-way comparison returning a signed integer, there is another set of otherwise mostly-unrelated languages that take a different approach. 

### Rust

Rust, Kotlin, Swift, Haskell, and Scala are rather different languages in many respects. But they all solve this particular problem in basically the same way: they treat _equality_ and _comparison_ as separate operations. I want to focus specifically on Rust here as it's arguably the closest language to C++ of the group, but the other three are largely equivalent for the purposes of this specific discussion.

Rust deals in Traits (which are roughly analogous to C++0x concepts and Swift protocols) and it has four relevant Traits that have to do with comparisons:

- `PartialEq` (which is a partial equivalence relation spelled which only requires symmetry and transitivity)
- `Eq` (which extends `PartialEq`, adding reflexivity)
- `PartialOrd` (which allows for incomparability by returning `Option<Ordering>`, where `Ordering` is an enum)
- `Ord` (a total order, which extends `Eq` and `PartialOrd`)

The actual operators are [implicitly generated][rust.oper] from these traits, but not all from the same one. Importantly, `x == y` is translated as `PartialEq::eq(x, y)` whereas ` x < y` is translated as `PartialOrd::lt(x, y)` (which is effectively checking that `PartialOrd::partial_cmp(x, y)` is `Less`).

That is, you don't get *six* functions for the price of one. You need to write _two functions_. 

Even if you don't know Rust (and I really don't know Rust), I think it would be instructive here would be to look at how the equivalent comparisons are implemented for Rust's `vector` type. The important parts look like this:

<table style="width:100%">
<tr>
<th style="width:50%">
[`Eq`][Rust.Eq]
[Rust.Eq]: https://doc.rust-lang.org/src/core/slice/mod.rs.html?search=#4037-4053 "Implementation of Eq for Slice"
</th>
<th>
[`Ord`][Rust.Ord]
[Rust.Ord]: https://doc.rust-lang.org/src/core/slice/mod.rs.html#4116-4136 "Implementation of Ord for Slice"
</th>
</tr>
<tr>
<td>
    :::rust hl_lines="5,6,7,10"
    impl<A, B> SlicePartialEq<B> for [A]
        where A: PartialEq<B>
    {
        default fn eq(&self, other: &[B]) -> bool {
            if self.len() != other.len() {
                return false;
            }

            for i in 0..self.len() {
                if !self[i].eq(&other[i]) {
                    return false;
                }
            }

            true
        }
    }    
</td>
<td>
    :::rust hl_lines="11"
    impl<A> SliceOrd<A> for [A]
        where A: Ord
    {
        default fn cmp(&self, other: &[A]) -> Ordering {
            let l = cmp::min(self.len(), other.len());

            let lhs = &self[..l];
            let rhs = &other[..l];

            for i in 0..l {
                match lhs[i].cmp(&rhs[i]) {
                    Ordering::Equal => (),
                    non_eq => return non_eq,
                }
            }

            self.len().cmp(&other.len())
        }
    }    
</td>
</tr>
</table>

In other words, `eq` calls `eq` all the way down while doing short-circuiting whereas `cmp` calls `cmp` all the way down, and these are two separate functions. Both algorithms exactly match our implementation of `==` and `<=>` for `vector` above. Even though `cmp` performs a 3-way ordering, and you can use the result of `a.cmp(b)` to determine that `a == b`, it is _not_ the way that Rust (or other languages in this realm like Swift and Kotlin and Haskell) determine equality. 

### Other Languages

Swift has [`Equatable`][swift.eq] and [`Comparable`][swift.comp] protocols. For types that conform to `Equatable`, `!=` is implicitly generated from `==`. For types that conform to `Comparable`, `>`, `>=`, and `<=` are implicitly generated from `<`. Swift does not have a 3-way comparison function.

There are other languages that make roughly the same decision in this regard that Rust does: `==` and `!=` are generated from a function that does equality whereas the four relational operators are generated from a three-way comparison. Even though the three-way comparison _could_ be used to determine equality, it is not:

- Kotlin, like Java, has a [`Comparable`][kotlin.comp] interface and a separate `equals` method inherited from [`Any`][kotlin.any]. Unlike Java, it has [operator overloading][kotlin.oper]: `a == b` means `a?.equals(b) ?: (b === null)` and `a < b` means `a.compareTo(b) < 0`.
- Haskell has the [`Data.Eq`][haskell.eq] and [`Data.Ord`][haskell.ord] type classes. `!=` is generated from `==` (or vice versa, depending on which definition is provided for `Eq`). If a `compare` method is provided to conform to `Ord`, `a < b` means `(compare a b) < 0`.
- Scala's equality operators come from the root [`Any`][scala.any] interface, `a == b` means `if (a eq null) b eq null else a.equals(b)`. Its relational operators come from the [`Ordered`][scala.ord] trait, where `a < b` means `(a compare b) < 0`.

# Proposal

Fundamentally, we have two sets of operations: equality and comparison. In order to be efficient and not throw away performance, we need to implement them separately. `operator<=>()` as specified in the working draft today generating all six functions just doesn't seem to be a good solution.

We can do something similar to the Rust model above and first described in [this section](https://github.com/davidstone/isocpp/blob/master/operator-spaceship/I-did-not-order-this.md#make-operator-create-only-operator-operator-operator-and-operator) of the previously linked paper: require two separate functions to implement all the functionality. That is, change the rewrite rules for how the lookup for operators works to the following (in all cases, `a @ b` prefers `a @ b` if it exists, as it does today):

<table>
<th>
Source <br />
`a @ b`
</th>
<th>
Today (P0515/C++2a)
</th>
<th>
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    a == b
</td>
<td>
    :::cpp
    (a <=> b) == 0
</td>
<td>
    :::cpp
    a == b // no rewrite
</td>
</tr>
<tr>
<td>
    :::cpp
    a != b
</td>
<td>
    :::cpp
    (a <=> b) != 0
</td>
<td>
    :::cpp
    !(a == b)
</td>
</tr>
<tr>
<td>
    :::cpp
    a < b
</td>
<td>
    :::cpp
    (a <=> b) < 0
</td>
<td>
    :::cpp
    (a <=> b) < 0  // unchanged
</td>
</tr>
<tr>
<td>
    :::cpp
    a <= b
</td>
<td>
    :::cpp
    (a <=> b) <= 0
</td>
<td>
    :::cpp
    (a <=> b) <= 0 // unchanged
</td>
</tr>
<tr>
<td>
    :::cpp
    a > b
</td>
<td>
    :::cpp
    (a <=> b) > 0
</td>
<td>
    :::cpp
    (a <=> b) > 0  // unchanged
</td>
</tr>
<tr>
<td>
    :::cpp
    a >= b
</td>
<td>
    :::cpp
    (a <=> b) >= 0
</td>
<td>
    :::cpp
    (a <=> b) >= 0 // unchanged
</td>
</tr>
</table>

The inverse lookup rules would also be changed. Whereas today `a == b` can find either `(a <=> b) == 0` or `0 == (b <=> a)`, the proposal is that it instead either find `a == b` or `b == a`. Likewise `a != b` would find either `!(a == b)` or `!(b == a)`, but never look for `<=>`.

This means we have to write two functions instead of just `<=>`, but we get optimal performance. The issues with this approach are, from David (with one adjustment):

> 1. Compared to the previous solution, this requires the user to type even more to opt-in to behavior that they almost always want (if you have defaulted relational operators, you probably want the equality operators). Because `operator<=>` is a new feature, we do not have any concerns of legacy code, so if the feature starts out as giving users all six comparison operators, it would be better if they must type only one line rather than having to type <del>three</del> <ins>two</ins>.
> 2. It is a natural side-effect of computing less than vs. greater than that you compute equal to. It is strange that we define an operator that can tell us whether things are equal, but we use it to generate all comparisons other than equal and not equal. For the large set of types for which `operator<=>` alone is sufficient, it also means that users who are not using the default (they are explicitly defining the comparisons) must define two operators that encode much of the same logic of comparison. This mandatory duplication invites bugs as the code is changed under maintenance.

Getting back to our initial example, we would write:

    :::cpp
    struct S {
        vector<string> names;
        bool operator==(S const&) const = default;
        auto operator<=>(S const&) const = default;
    };

We have to explicitly default two functions, but we can get optimal behavior out of this - which seems like a good tradeoff. But let's discuss those two points in more detail.

It's tempting to want to shoehorn templates to solve this problem (such as by adding an extra argument to `operator<=>`). But that's effectively using templates like a macro and doesn't seem like sound design.

## Implications for non-special types

There are many kinds of types for which the defaulted comparison semantics are incorrect, but nevertheless don't have to do anything different between equality and ordering. One such example is `optional<T>`. Having to write two functions here gets exactly at David's point of duplication above:

<table style="width:100%">
<tr>
<th style="width:50%">
[P0515/C++2a][revzin.impl]
[revzin.impl]: https://medium.com/@barryrevzin/implementing-the-spaceship-operator-for-optional-4de89fc6d5ec "Implementing the spaceship operator for optional||Barry Revzin||2017-11-16"
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
            optional<U> const& rhs) const
        -> decltype(compare_3way(*lhs, *rhs))
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs);
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }
</td>
<td>
    :::cpp hl_lines="4,7,9,16,19,21"
    template <typename T, typename U>
    constexpr auto operator<=>(optional<T> const& lhs,
            optional<U> const& rhs) const
        -> decltype(compare_3way(*lhs, *rhs))
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs);
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }

    template <typename T, typename U>
    constexpr auto operator==(optional<T> const& lhs,
            optional<U> const& rhs) const
        -> decltype(*lhs == *rhs)
    {
        if (lhs.has_value() && rhs.has_value()) {
            return *lhs == *rhs;
        } else {
            return lhs.has_value() == rhs.has_value();
        }
    }    
</td>
</tr>
</table>

As is probably obvious, the implementations of `==` and `<=>` are basically identical: the only difference is that `==` calls `==` and `<=>` calls `<=>` (or really `compare_3way`). 

But it's important to keep in mind three things.

1. In C++17 we'd have to write six functions, so writing two is a large improvement. 
2. These two functions may be duplicated, but they give us optimal performance - writing the one `<=>` to generate all six comparison functions does not. 
3. The amount of special types of this kind - types that have non-default comparison behavior but perform the same algorithm for both `==` and `<=>` - is fairly small. Most container types would have separate algorithms. Typical types default both, or just default `==`. The canonical examples that would need special behavior are `std::array` and `std::forward_list` (which either have fixed or unknown size and thus cannot short-circuit) and `std::optional` and `std::variant` (which can't do default comparison). So this particular duplication is a fairly limited problem.

## Implications for defaulting

As previously hinted, this proposal implies the need for defaulting _two_ functions instead of _one_. This section makes this difference more explicit:

<table style="width:100%">
<tr>
<th style="width:50%">
P0515/C++2a
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    // all six
    struct A {
        auto operator<=>(A const&) const = default;
    };
    
    
    // just equality, no relational
    struct B {
        strong_equality operator<=>(B const&) const = default;
    };
</td>
<td>
    :::cpp
    // all six
    struct A {
        bool operator==(A const&) const = default;
        auto operator<=>(A const&) const = default;
    };
    
    // just equality, no relational
    struct B {
        bool operator==(B const&) const = default;
    };
</td>
</tr>
</table>

Arguably, `A` isn't so bad here and `B` is somewhat simpler. But if [P0847](https://wg21.link/p0847r0) is adopted, we could move the defaulting logic into a new-style mixin and simply use inheritance as a way of adopting this functionality:

    :::cpp
    struct Eq {
        template <typename Self>
        bool operator==(this Self const&, Self const&) = default;
    };
    
    struct Ord : Eq {
        template <typename Self>
        auto operator<=>(this Self const&, Self const&) = default;
    };
    
    struct A : Ord { };    
    struct B : Eq { };

And now we're defaulting zero functions instead of one.

Our initial motivating example becomes:

    :::cpp
    struct S : Ord {
        vector<string> names;
    };
    
and has optimal comparison operators all the way down.

The ability to do this is completely orthogonal to this proposal - given P0847, we could write `Ord` already. The point is simply to illustrate that defaulting two functions is not necessarily a large burden.

## Extension: other means of defaulting `==`

One of the concerns could be that we now require class authors to default both `==` and `<=>` and this seems like an unnecessary amount of function declarations to write. To that end, this proposal could be extended to generate a defaulted `operator==` from a couple different places.

### Defaulted `==` from defaulted `<=>`

This extension would be that a defaulted `operator<=>` also creates a defaulted `operator==`. The result would be no change in typing at all for classes that just want a default, member-wise total order (as in `A`) above. 

The reasoning here is that having `<=>` certainly implies equality, and the likely intent behind choosing default, member-wise ordering is that you also want default, member-wise equality. 

On the other hand, it seems a little strange that `<=>` never leads to the generation of an `==` operation (that is, `a == b` is no longer `(a <=> b) == 0`), `<=>` can nevertheless lead to the generation of an `==` _function_. But if a leading concern with this proposal is the requirement of an additional function declaration, this is surely worth doing.

### Defaulted `==` from defaulted copy constructor/assignment

A different potential extension is to take advantage of the clear ties between copying and equality. Stepanov's [Fundamentals of Generic Programming][stepanov.fogp] puts forward axioms that equate the semantics of copying with the semantics of equals. Copying gives you an equal object, which is an idea explicitly noted in [P0515R0](https://wg21.link/p0515r0):

> Default comparison should do exactly the same thing as default copying.

As originally suggested in [P0432](https://wg21.link/p0432) and [P0481](https://wg21.link/p0481), we could generate a default, memberwise comparison if the copy constructor is not user-provided and all the members are equality comparable.

### Impact on example

The earlier example was:

    :::cpp
    struct A {
        auto operator<=>(A const&) const = default;
    };
    
Today, `A` has all six comparison operators - all of which invoke `<=>`. This proposal prevents `==` and `!=` from working, so those need to be handled separately. The core proposal suggests that this be handled explictly - with an explicitly defaulted `operator==`.

Both of these extensions lead to no change necessary for `A`. The former generates the defaulted `operator==` function from the existence of the defaulted `operator<=>`, the latter generates the defaulted `operator==` from the implicit copy constructor/assignment.

Status quo and both of these extensions have the same _semantics_, but both extensions are likely to generate better code for defaulted `==`. 

# Acknowledgements

This paper most certainly would not exist without David Stone's extensive work in this area. Thanks also to Agustín Bergé for discussing issues with me.
    
[Stone.Order]: https://github.com/davidstone/isocpp/blob/b2db8e00dfec04a7742c67a5ea6e9575c9aba03d/operator-spaceship/I-did-not-order-this.md "I did not order this! Why is it on my bill?||David Stone||2018-08-06"
[rust.oper]: https://doc.rust-lang.org/reference/expressions/operator-expr.html#comparison-operators "Comparison Operators - The Rust Reference"
[swift.eq]: https://developer.apple.com/documentation/swift/equatable "Equatable - Swift Standard Library"
[swift.comp]: https://developer.apple.com/documentation/swift/comparable "Comparable - Swift Standard Library"
[kotlin.comp]: https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-comparable/index.html "Comparable - Kotlin Programming Language"
[kotlin.any]: https://kotlinlang.org/api/latest/jvm/stdlib/kotlin/-any/index.html "Any - Kotlin Programming Language"
[kotlin.oper]: https://kotlinlang.org/docs/reference/operator-overloading.html#equals "Operator overloading - Kotlin Programming Language"
[haskell.eq]: http://hackage.haskell.org/package/base-4.11.1.0/docs/Data-Eq.html "Data.Eq - Haskell documentation"
[haskell.ord]: http://hackage.haskell.org/package/base-4.11.1.0/docs/Data-Ord.html "Data.Ord - Haskell documentation"
[scala.any]: https://www.scala-lang.org/api/current/scala/Any.html#==(x$1:Any):Boolean "Scala Standard Library - Any"
[scala.ord]: https://www.scala-lang.org/api/current/scala/math/Ordered.html "Scala Standard Library - Ordered"
[stepanov.fogp]: http://stepanovpapers.com/DeSt98.pdf "Fundamentals of Generic Programming||James C. Dehnert and Alexander Stepanov||1998"
