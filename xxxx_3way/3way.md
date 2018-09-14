Title: 3-way, 3-args
Document-Number: DxxxxRx
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG

# Motivation

See David Stone's [I did not order this!](https://github.com/davidstone/isocpp/blob/master/operator-spaceship/I-did-not-order-this.md) for a very clear, very thorough description of the problem: it does not seem to be possible to implement `<=>` near-optimally for "wrapper" types. For a super brief run-down, the most straightforward approach to implementing `<=>` for `vector<T>` is (let's just assume `strong_ordering`):

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

On the one hand, this is great. We wrote one function instead of six, and this function is really easy to understand too. On top of that, this is a really good implementation for `<`!  As good as you can get. 

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
        const size_t size = lhs.size();
        if (size != rhs.size()) return false;
    
        for (size_t i = 0; i != size; ++i) {
            if (!(lhs[i] == rhs[i])) {
                return false;
            }
        }
        
        return true;
    }
    
    // ... and have to write this one manually
    template<typename T>
    bool operator!=(vector<T> const& lhs, vector<T> const& rhs)
    {
        return !(lhs == rhs);
    }

We have the initial problem that we have this false sense of security - the easy thing we wrote generates bad code. But even if we write this more efficient `==` for containers (`vector`, `string`, etc.), this still doesn't solve our problem. Any types that have these as members wouldn't be able to just use `<=>` either - because `<=>` calls `<=>` all the way down. It can't just call `==` because it doesn't know that it's doing just `==`. So every type would have to write `==` (and `!=`).

So what do we do? 

## Other Languages

How do other languages solve this problem? The most motivating one for me is to look at Rust, and it turns out Rust's decisions in this venue seem to be fairly common across other languages (e.g. Swift, Kotlin). 

Rust deals in Traits (which are roughly analogous to C++0x concepts and Swift protocols) and it has four relevant Traits that have to do with comparisons:

- `PartialEq` (which is a partial equivalence relation spelled `==`, which only requires symmetry and transitivity)
- `Eq` (which extends `PartialEq`, adding reflexivity)
- `PartialOrd` (which allows for incomparability by returning `Option<Ordering>`)
- `Ord` (a total order, which extends `Eq` and `PartialOrd`)

Even if you don't know Rust at all, I think it'd be helpful for the purposes of discussion to look at a simplified version of how these traits are defined. The actual operators are implicitly generated from these functions (e.g. `x < y` invokes `PartialOrd::lt(x, y)`)

    :::rust
    pub trait PartialEq {
        fn eq(&self, other: &Self) -> bool;
        fn ne(&self, other: &Self) -> bool { !self.eq(other) }
    }
    
    pub trait Eq: PartialEq { }
    
    pub trait PartialOrd: PartialEq {
        fn partial_cmp(&self, other: &Self) -> Option<Ordering>;
        
        // these are all defaulted based on the result of partial_cmp()
        fn lt(&self, other: &Self) -> bool { ... }
        fn le(&self, other: &Self) -> bool { ... }
        fn gt(&self, other: &Self) -> bool { ... }
        fn ge(&self, other: &Self) -> bool { ... }
    }
    
    pub trait Ord: Eq + PartialOrd {
        fn cmp(&self, other: &Self) -> Ordering;
    }

The functionality here is split. To get `==` and `!=`, you model (at least) `PartialEq` and implement `eq`. To get `<`, `<=`, `>`, and `>=` for a partial order, you model `PartialOrd` and implement `partial_cmp`. For a total order, you model `Ord` and implement `cmp`.

That is, you don't get six functions for the price of one. You need to write two functions. 

Also instructive here would be to look at how the equivalent comparisons are implemented for Rust's `vector` type. The important parts look like this (again, even if you don't know Rust at all, at least the general idea of what the code is doing should hopefully be clear).

<table style="width:100%">
<tr>
<th style="width:50%">
[`Eq`](https://doc.rust-lang.org/src/core/slice/mod.rs.html?search=#4037-4053)
</th>
<th>
[`Ord`](https://doc.rust-lang.org/src/core/slice/mod.rs.html#4116-4136)
</th>
</tr>
<tr>
<td>
    :::rust
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
    :::rust
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

In other words, `eq` calls `eq` all the way down, `cmp` calls `cmp` all the way down, and these are two separate functions. Both algorithms exactly match our implementation of `==` and `<=>` for `vector` above. Even though `cmp` performs a 3-way ordering, and you can use the result of `a.cmp(b)` to determine that `a == b`, it is _not_ the way that Rust (or other languages in this realm like Swift and Kotlin) determine equality. 

# Proposal

Fundamentally, we have two sets of operations: equality and comparison. In order to be efficient and not throw away performance, we need to implement them separately. `operator<=>()` as specified in the working draft today generating all six functions just doesn't seem to be a good solution.

I think there are two things we can actually do, one which David proposed in the linked paper and one which is completely novel.

## Two Functions

One thing we can do is similar to the Rust model above and is described in [this section](https://github.com/davidstone/isocpp/blob/master/operator-spaceship/I-did-not-order-this.md#make-operator-create-only-operator-operator-operator-and-operator) of the previously linked paper: require two separate functions to implement all the functionality. 

In other words, the rewrite rules would change to the following (in all cases, `a @ b` prefers `a @ b` if it exists, as it does today):

<table>
<th>
Source <br />
`a @ b`
</th>
<th>
Today (p0515/C++2a)
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

This means we have to write two functions instead of just `<=>`, but we get optimal performance. The issues with this approach are, again just citing the paper (with one adjustment):

> 1. Compared to the previous solution, this requires the user to type even more to opt-in to behavior that they almost always want (if you have defaulted relational operators, you probably want the equality operators). Because `operator<=>` is a new feature, we do not have any concerns of legacy code, so if the feature starts out as giving users all six comparison operators, it would be better if they must type only one line rather than having to type <del>three</del> <ins>two</ins>.
> 2. It is a natural side-effect of computing less than vs. greater than that you compute equal to. It is strange that we define an operator that can tell us whether things are equal, but we use it to generate all comparisons other than equal and not equal. For the large set of types for which `operator<=>` alone is sufficient, it also means that users who are not using the default (they are explicitly defining the comparisons) must define two operators that encode much of the same logic of comparison. This mandatory duplication invites bugs as the code is changed under maintenance.

As an extra safety mechanism, we should enforce that if a type implements `<=>` that it also implements `==`. In the languages I've looked at, as well as the three-way comparison as it exists in the working draft today, ordering always implies equality. That is:

    :::cpp
    struct Bad {
        auto operator<=>(Bad const&) const = default;
    }; // ill-formed
    
    struct Good {
        bool operator==(Good const&) const = default;
        auto operator<=>(Good const&) const = default;
    }; // ok

## One Function

Typically, equality and comparison are _really_ closely related operations. As is clear from both the C++ and Rust implementations, they do almost exactly the same thing - we loop over the common-sized subrange and compare all the elements with _some_ operation, possibly doing an early exit if we can. 

But we still need to differentiate between equality and comparison to get that performance benefit.

What if we could do both without having to write two functions?

We can create two objects named `eq` and `ord` that are different types, but encode some value such that they can be compared as constant expressions. And we can use those objects as an convey extra info into `<=>`. That is, we make `operator<=>()` a ternary operator with the following rewrite rules:

<table>
<th>
Source <br />
`a @ b`
</th>
<th>
Today (p0515/C++2a)
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">eq</span><code class="language-cpp">) == 0</code></pre>
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">eq</span><code class="language-cpp">) != 0</code></pre>
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">ord</span><code class="language-cpp">) &lt; 0</code></pre>
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">ord</span><code class="language-cpp">) &lt;= 0</code></pre>
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">ord</span><code class="language-cpp">) &gt; 0</code></pre>
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
<pre style="background:transparent;border:0px"><code class="language-cpp">operator&lt;=&gt;(a, b, </code><span class="token function">ord</span><code class="language-cpp">) &gt;= 0</code></pre>
</td>
</tr>
</table>

The spaceship operator would continue to be usable as a binary operator today, but `a <=> b` would mean `operator<=>(a, b, ord)`.

With that change, our spaceship for an optimally ordered `vector`, as well as the new `compare_3way()` for those types that implement all the operators but not `<=>`, becomes:

<table style="width:100%">
<tr>
<td style="width:50%">
    :::cpp
    template <typename T, typename Comparison>
    auto operator<=>(vector<T> const& lhs, vector<T> const& rhs,
            Comparison cmp_type)
        // strong_equality for eq, strong_ordering for ord
        -> typename Comparison::strong
    {
        if constexpr (cmp_type == eq) {
            // can short circuit in this case
            if (lhs.size() != rhs.size()) {
                return strong_equality::nonequal;
            }
        }
        
        size_t min_size = min(lhs.size(), rhs.size());
        for (size_t i = 0; i != min_size; ++i) {
            // pass forward the comparison type - whatever we
            // do here is what we want to do lower as well
            auto const cmp = compare_3way(
                lhs[i], rhs[i], cmp_type);
            if (cmp != 0) {
                return cmp;
            }
        }
        
        if constexpr (cmp_type == eq) {
            return strong_equality::equal;
        } else {
            return lhs.size() <=> rhs.size();
        }
    }
</td>
<td style="width:50%">
    ::cpp
    template <typename T>
    strong_equality compare_3way(T const& x, T const& y, eq_t)
    {
        return x == y ? strong_equality::equal
                      : strong_equality::nonequal;
    }
    
    // same thing it does now
    template <typename T>
    strong_ordering compare_3way(T const& x, T const& y, ord_t)
    {
        if (x == y) return strong_ordering::equal;
        if (x < y) return strong_ordering::less;
        return strong_ordering::greater;
    }
</td>
</tr>
</table>

    
And this is actually optimal. For equality, we short-circuit. For ordering, we don't. And this is both optimal in the case for where we're directly comparing `vector`s (or `string`s or `optional`s or ...) _and_ it's already optimal in the case where we are wrapping this:

    :::cpp
    struct S {
        vector<string> names;
        auto operator<=>(S const&) = default; // defaulted binary <=> would do the right thing
    };
    
With this model, doing an equality comparison on two `S`s will mean calling `operator<=>(lhs.names, rhs.names, eq)`, which will short-circuit on the sizes of the two `vector`s first and then just do an equality comparison on the underlying `string`s. And _that_ comparison will _also_ itself short-circuit on the sizes of the `string`s first. 

This new suggested implementation of `<=>` for `vector` is admittedly quite a bit more complicated than our initial implementation. But it's arguably still quite a bit _less_ complicated than the code we have to write in C++17 today, and it's arguably less complicated than having to write 2 or 3 functions to achieve optimal performance - and still not really being able to do it. 

## Non-special types

It's also important to note that not all types actually need special treatment. If many cases, we really just want to defer down to members' comparison implementations - we don't always have different algorithms for `==` and `<`. It's important to make those implementations as simple as possible. 

Here is a comparison between the two proposals for how to implement a total ordering for `optional<T>` (throughout this paper I've been punting on dealing with the comparison categories, and I will continue to do that here as well):

<table style="width:100%">
<tr>
<th style="width:50%">
Two functions
</th>
<th style="width:50%">
One function
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    bool operator==(optional<T> const& lhs,
        optional<T> const& rhs)
    {
        if (lhs.has_value() && rhs.has_value()) {
            return *lhs == *rhs;
        } else {
            return lhs.has_value() == rhs.has_value();
        }
    }
    
    template <typename T>
    strong_ordering operator<=>(optional<T> const& lhs,
        optional<T> const& rhs) 
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs);
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }
</td>
<td>
    :::cpp
    template <typename T, typename Comparison>
    auto operator<=>(optional<T> const& lhs,
            optional<T> const& rhs, Comparison cmp_type)
        -> typename Comparison::strong
    {
        if (lhs.has_value() && rhs.has_value()) {
            return compare_3way(*lhs, *rhs, cmp_type);
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }
</td>
</tr>
</table>

## Partial Orders

One notion of comparison that is currently unrepresentable today is the idea of having a partial order but a strong notion of equality. That is, having a type for which `a == b` satisfies substitutibility but for whom `a < b` is not a total order. One example might be sets for which `<` defines subset.

As mentioned earlier, Rust supports this today by having both a `PartialOrd` (whose function returns an optional ordering - there may not be one) and an `Ord` (whose function returns an ordering - there must be one). The approach we've typically taken in C++ is just to return `false` for every operation to mean incomparable. This seems unsatisfactory - it'd be nice to have a way to _expose_ incomparability. Moreover, in the type hierarchy from P0515, `std::partial_ordering` implies `std::weak_equality` - as does `std::weak_ordering`. 

If we're going to reconsider how `<=>` generates individual comparison operators, and we really should, we should also take the time to reconsider the interplay of the ordering categories with the equality ones.