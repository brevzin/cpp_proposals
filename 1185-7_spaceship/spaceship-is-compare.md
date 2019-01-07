Title: When do you actually use `<=>`?
Document-Number: D1186R1
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG, LEWG

# Revision History

[R0](https://wg21.link/p1186r0) of this paper was approved by both EWG and LEWG. Under Core review, the issue of [unintentional comparison category strengthening](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1186r0.html#unintentional-comparison-category-strengthening) was brought up as a reason to oppose the design. As a result, this revision proposes a different way to solve the issues presented in R0.

# Motivation

[P0515](https://wg21.link/p0515r3) introduced `operator<=>` as a way of generating all six comparison operators from a single function. As a result of [P1185R0](https://wg21.link/p1185r0), that has become two functions, but importantly you still only need to declare one operator function to generate each of the four relational comparison operators.

In a future world, where all types have adopted `<=>`, this will work great. It will be very easy to implement `<=>` for a type like `optional<T>` (writing as a non-member function for clarity):

    :::cpp
    template <typename T>
    compare_3way_type_t<T> // see P1187
    operator<=>(optional<T> const& lhs, optional<T> const& rhs)
    {
        if (lhs.has_value() && rhs.has_value()) {
            return *lhs <=> *rhs;
        } else {
            return lhs.has_value() <=> rhs.has_value();
        }
    }

This is a clean and elegant way of implementing this functionality, and gives us `<`, `>`, `<=`, and `>=` that all do the right thing. What about `vector<T>`?

    :::cpp
    template <typename T>
    compare_3way_type_t<T>
    operator<=>(vector<T> const& lhs, vector<T> const& rhs)
    {
        return lexicographical_compare_3way(
            lhs.begin(), lhs.end(),
            rhs.begin(), rhs.end());
    }
    
Even better.

What about a simple aggregate type, where all we want is to do normal member-by-member lexicographical comparison? No problem:

    :::cpp
    struct Aggr {
        X x;
        Y y;
        Z z;
        
        auto operator<=>(Aggr const&) const = default;
    };

Beautiful.

The problem is that we're not in this future world quite yet. No program-defined types have `<=>`, the only standard library type that has `<=>` so far is `nullptr_t`. Which means we can't just replace the existing relational operators from `optional<T>` and `vector<T>` with `<=>` and probably won't be able to just default `Aggr`'s `<=>`. We need to do something more involved.

## Conditional Spaceship

R0 of this paper [argued](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2018/p1186r0.html#the-initial-premise-is-false-optionalt-shouldnt-always-have) against the claim that "[a]ny compound type should have `<=>` only if all of its constituents have `<=>`." At the time, my understand of what "conditional spaceship" meant was this:

    :::cpp
    template <Cpp17LessThanComparable T>
    bool operator<(vector<T> const&, vector<T> const&);
    
    template <ThreeWayComparable T> // see P1188
    compare_3way_type_t operator<=>(vector<T> const&, vector<T> const&);
    
This is, indeed, a bad implementation strategy because `v1 < v2` would invoke `operator<` even if `operator<=>` was a viable option, so we lose the potential performance benefit.

But since I wrote R0, I've come up with a much better way of [conditionally adopting spaceship][revzin.sometimes]:

    :::cpp
    template <Cpp17LessThanComparable T>
    bool operator<(vector<T> const&, vector<T> const&);
    
    template <ThreeWayComparable T> requires Cpp17LessThanComparable<T>
    compare_3way_type_t operator<=>(vector<T> const&, vector<T> const&);

It's a small, seemingly redundant change (after all, if `ThreeWayComparable<T>` then surely `Cpp17LessThanComparable<T>` for all types other than pathologically absurd ones that provide `<=>` but explicitly delete `<`), but it ensures that `v1 < v2` invokes `operator<=>` where possible. 

Conditionally adopting spaceship between C++17 and C++20 is actually even easier:

    :::cpp
    template <typename T>
    enable_if_t<supports_lt<T>::value, bool> // normal C++17 SFINAE machinery;
    operator<(vector<T> const&, vector<T> const&);

    // use the feature-test macro for operator<=>
    #if __cpp_impl_three_way_comparison
    template <ThreeWayComparable T>
    compare_3way_type_t<T> operator<=>(vector<T> const&, vector<T> const&);
    #endif    


In short, conditionally adopting `<=>` has a good user story, once you know how to do it. This is very doable.

## Unconditional spaceship

If we want to adopt `<=>` unconditionally, which is something we have to do for normal class types like `Aggr` and something we might want to do for containers like `vector`, we need to provide scaffolding as a stop-gap workaround. We need some way of invoking `<=>` where possible, but falling back to a synthesized three-way comparison from the two-way comparison oeprators. 

That only scaffolding we have today is in the form of a standard library algorithm named `std::compare_3way()`. What this function does is add more fall-back implementations, in a way best illustrated by this skeleton:

    :::cpp
    template<class T, class U>
    auto compare_3way(const T& a, const U& b) {
        if constexpr (/* can invoke a <=> b */)
            return a <=> b;
        else if constexpr (/* can invoke a<b and a==b */)
            return a==b ? strong_ordering::equal : a<b ? strong_ordering::less : strong_ordering::greater;
        else if constexpr (/* can invoke a==b */)
            return a == b ? strong_equality::equal : strong_equality::unequal;
        else
            /* ill-formed, defined as deleted */
    }

Since the typical case is that types won't have `<=>` implemented, we basically want to make sure that we use `compare_3way()` for the functionality that we need. In other words, the way we want to implement `<=>` for a simple type like `Aggr` is:

    :::cpp
    struct Legacy {
        bool operator==(Legacy const&) const;
        bool operator<(Legacy const&) const;
    };
    
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        strong_ordering operator<=>(Aggr const& rhs) const {
            if (auto cmp = i <=> rhs.i; cmp != 0) return cmp;
            if (auto cmp = c <=> rhs.c; cmp != 0) return cmp;
            return compare_3way(q, rhs.q);
        }
    };    

Now, that implementation isn't so bad. It's a lot of boilerplate, but it's not particularly complex.

But it's important to keep in mind that we're assuming and enforcing a _strong_ ordering on `Legacy`. What if we only wanted to assume and enforce a _weak_ ordering? What if we only wanted to assume a _partial_ ordering? We don't have those kinds of options in the standard library at the moment. Indeed, `std::compare_3way()` itself is somewhat of a scary function since it's assuming a strong ordering semantic that is hidden behind a fairly innocuous name. 

## `<=>` is `compare_3way()`

It's at this point that R0 of this paper made the following argument: because you have to use `compare_3way()` as a stop-gap right now, you may as well just always use `compare_3way()` (since it transparently forwards to `<=>` where possible). And if you're always using `compare_3way()` anyway, we might as well make `<=>` be the useful thing and not the useless thing. 

I now more fully appreciate the concern that many people share for assuming strong comparison semantics on behalf of the language directly. If we had `<=>` just assume `strong_ordering`, the utility of comparison categories would decrease dramatically - you wouldn't know if someone actually really did make that conscious decision or not. 

It's possible that a version of R0 that simply assumed `weak_ordering` instead of `strong_ordering` would be viable. But I am not sure that assuming `weak_ordering` is any better than assuming `strong_ordering`. Perhaps we can come up with a way that doesn't make any assumptions at all?

## Status Quo

To be perfectly clear, the current rule for defaulting `operator<=>` for a class `C` is roughly as follows:

- For two objects `x` and `y` of type `const C`, we compare their corresponding subobjects <code>x<sub>i</sub></code> and <code>y<sub>i</sub></code> until the first _i_ where given <code>auto v<sub>i</sub> = x<sub>i</sub> &lt;=&gt; y<sub>i</sub></code>, <code>v<sub>i</sub> != 0</code>. If such an _i_ exists, we return <code>v<sub>i</sub></code>. Else, we return `strong_ordering::equal`.
- If the return type of defaulted `operator<=>` is `auto`, we determine the return type by taking the common comparison category of all of the <code>x<sub>i</sub> &lt;=&gt; y<sub>i</sub></code> expressions. If the return type is provided, we ensure that it is valid. If any of the pairwise comparisons is invalid, or are not compatible with the provided return type, the defaulted `operator<=>` is defined as deleted.

In other words, for the `Aggr` example, the declaration `strong_ordering operator<=>(Aggr const&) const = default;` expands into something like

    :::cpp hl_lines="9"
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        strong_ordering operator<=>(Aggr const& rhs) const {
            if (auto cmp = i <=> rhs.i; cmp != 0) return cmp;
            if (auto cmp = c <=> rhs.c; cmp != 0) return cmp;
            if (auto cmp = q <=> rhs.q; cmp != 0) return cmp;
            return strong_ordering::equal
        }
    };

Or it would, if the highlighted line were valid. `Legacy` has no `<=>`, so that pairwise comparison is invalid, so the operator function would be defined as deleted. 

# Proposal

This paper proposes a new direction for a stop-gap adoption measure for `operator<=>`: we will synthesize an `operator<=>` for a type, but only under very specific conditions, and only when the user provides the comparison category that the comparison needs to use. All we need is a very narrow ability to help with `<=>` adoption. This is that narrow ability.

Currently, the pairwise comparison of the subobjects is always <code>x<sub>i</sub> &lt;=&gt; y<sub>i</sub></code>. Always `operator<=>`.

This paper proposes defining a new magic specification-only function <code><i>3WAY</i>(a, b)</code>, which only has meaning in the context of defining what a defaulted `operator<=>` does. The following function definition is very wordy, but it's not actually complicated: we will use the provided return type to synthesize an appropriate ordering. The key points are:

- We will _only_ synthesize an ordering if the user provides an explicit return type. We do not synthesize any ordering when the declared return type is `auto`.
- The presence of `<=>` is _always_ preferred to any kind of synthetic fallback. 

We then change the meaning of defaulted `operator<=>` to be defined in terms of <code><i>3WAY</i>(x<sub>i</sub>, y<sub>i</sub>)</code> instead of in terms of <code>x<sub>i</sub> &lt;=&gt; y<sub>i</sub></code>.

The proposed definition of <code><i>3WAY</i>(a, b)</code> is as follows:

- If `a <=> b` is a valid expression, `a <=> b`.
- Otherwise, if the declared return type of the defaulted `operator<=>` is `strong_ordering`, then:
    - If `a == b` is `true`, then `strong_ordering::equal`
    - Otherwise, if `a < b` is `true`, then `strong_ordering::less`
    - Otherwise, `strong_ordering::greater`.
- Otherwise, if the declared return type of defaulted `operator<=>` is `weak_ordering`, then:
    - If `a == b` is well-formed and convertible to `bool`, then:
        - If `a == b` is `true`, then `weak_ordering::equivalent`
        - Otherwise, if `a < b` is `true`, then `weak_ordering::less`
        - Otherwise, `weak_ordering::greater`
    - Otherwise:
        - If `a < b` is `true`, then `weak_ordering::less`
        - Otherwise, if `b < a` is `true`, then `weak_ordering::greater`
        - Otherwise, `weak_ordering::equivalent`
- Otherwise, if the declared return type of defaulted `operator<=>` is `partial_ordering`, then:
    - If `a == b` is well-formed and convertible to `bool`, then:
        - If `a == b` is `true`, then `partial_ordering::equivalent`
        - Otherwise, if `a < b` is `true`, then `partial_ordering::less`
        - Otherwise, if `b < a` is `true`, then `partial_ordering::greater`
        - Otherwise, `partial_ordering::unordered`
    - Otherwise:
        - If `a < b` is `true`, then `partial_ordering::less`
        - Otherwise, if `b < a` is `true`, then `partial_ordering::greater`
        - Otherwise, `partial_ordering::equivalent`

If <code><i>3WAY</i>(a, b)</code> uses an expression without checking for it, and that expression is invalid, the function is defined as deleted.

## Explanatory Examples

This might make more sense with examples.

<table style="width:100%">
<tr>
<th style="width:50%">
Source Code
</th>
<th style="width:50%">
Meaning
</th>
</tr>
<tr>
<td>
    :::cpp
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        auto operator<=>(Aggr const&) const = default;
    };
</td>
<td>
    :::cpp
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        // x.q <=> y.q is invalid and we have no return type
        // to guide our synthesis. Hence, deleted
        auto operator<=>(Aggr const&) const = delete;
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        strong_ordering operator<=>(Aggr const&) const = default;
    };
</td>
<td>
    :::cpp
    struct Aggr {
        int i;
        char c;
        Legacy q;
        
        strong_ordering operator<=>(Aggr const& rhs) const {
            if (auto cmp = i <=> rhs.i; cmp != 0) return cmp;
            if (auto cmp = c <=> rhs.c; cmp != 0) return cmp;
            
            // synthesizing strong_ordering from == and <
            if (q == rhs.q) return strong_ordering::equal;
            if (q < rhs.q) return strong_ordering::less;
            
            // sanitizers might also check for
            [[ assert: rhs.q < q; ]]
            return strong_ordering::greater;
        }
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    struct X {
        bool operator<(X const&) const;
    };
    
    struct Y {
        X x;
        
        strong_ordering operator<=>(Y const&) const = default;
    };
</td>
<td>
    :::cpp
    struct X {
        bool operator<(X const&) const;
    };
    
    struct Y {
        X x;
        
        // defined as deleted because Y has no <=>, so we fallback
        // to synthesizing from == and <, but we have no ==.
        strong_ordering operator<=>(Y const&) const = delete;
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    struct W {
        weak_ordering operator<=>(W const&) const;
    };
    
    struct Z {
        W w;
        Legacy q;
        
        strong_ordering operator<=>(Z const&) const = default;
    };
</td>
<td>
    :::cpp
    struct W {
        weak_ordering operator<=>(W const&) const;
    };
    
    struct Z {
        W w;
        Legacy q;
        
        // strong_ordering as a return type is not compatible with
        // W's comparison category, which is weak_ordering. Hence
        // defined as deleted
        strong_ordering operator<=>(Z const&) const = delete;
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    struct W {
        weak_ordering operator<=>(W const&) const;
    };
    
    struct Q {
        bool operator<(Q const&) const;
    };
    
    struct Z {
        W w;
        Q q;
        
        weak_ordering operator<=>(Z const&) const = default;
    };
</td>
<td>
    :::cpp
    struct W {
        weak_ordering operator<=>(W const&) const;
    };
    
    struct Q {
        bool operator<(Q const&) const;
    };
    
    struct Z {
        W w;
        Q q;
        
        weak_ordering operator<=>(Z const& rhs) const
        {
            if (auto cmp = w <=> rhs.w; cmp != 0) return cmp;
            
            // synthesizing weak_ordering from JUST <
            if (q < rhs.q) return weak_ordering::less;
            if (rhs.q < q) return weak_ordering::greater;
            return weak_ordering::equivalent;
        }
    };
</td>
</tr>
</table>

## Differences from Status Quo and P1186R0

Consider the highlighted lines in the following example:

    :::cpp hl_lines="6,10,15"
    struct Q {
        bool operator==(Q const&) const;
        bool operator<(Q const&) const;
    };
    
    Q{} <=> Q{}; // #1
    
    struct X {
        Q q;
        auto operator<=>(X const&) const = default; // #2
    };
    
    struct Y {
        Q q;
        strong_ordering operator<=>(Y const&) const = default; // #3
    };
    
In the working draft, `#1` is ill-formed and `#2` and `#3` are both defined as deleted because `Q` has no `<=>`.

With P1186R0, `#1` is a valid expression of type `std::strong_ordering`, and `#2` and `#3` are both defined as defaulted. In all cases, synthesizing a strong comparison.

With this proposal, `#1` is _still_ ill-formed. `#2` is defined as deleted, because `Q` still has no `<=>`. The only change is that in the case of `#3`, because we know the user wants `strong_ordering`, we provide one.

## Building complexity

The proposal here _only_ applies to the specific case where we are defaulting `operator<=>` and provide the comparison category that we want to default to. That might seem inherently limiting, but we can build up quite a lot from there.

Consider `std::pair<T, U>`. Today, its `operator<=` is defined in terms of its `operator<`, which assumes a weak ordering. One thing we could do (which this paper is not proposing, this is just a thought experiment) is to synthesize `<=>` with weak ordering as a fallback. 

We do that with just a simple helper trait (which this paper is also not proposing):

    :::cpp
    // use whatever <=> does, or pick weak_ordering
    template <typename T, typename C>
    using fallback_to = conditional_t<ThreeWayComparable<T>, compare_3way_type_t<T>, C>;
    
    // and then we can just...
    template <typename T, typename U>
    struct pair {
        T first;
        U second;
        
        common_comparison_category_t<
            fallback_to<T, weak_ordering>,
            fallback_to<U, weak_ordering>>
        operator<=>(pair const&) const = default;
    };
    
`pair<T,U>` is a simple type, we just want the default comparisons. Being able to default spaceship is precisely what we want. This proposal gets us there, with minimal acrobatics. 

We can also use this type trait to build more powerful library machinery for synthesizing `<=>` as follows:

    :::cpp
    // a type that defaults a 3-way comparison for T for the given category
    template <typename T, typename Cat>
    struct cmp_with_fallback {
        T const& t;
        fallback_to<T,Cat> operator<=>(cmp_with_fallback const&) const = default;
    };
    
    template <typename T, typename Cat>
    concept FallbackThreeWayComparable = ThreeWayComparable<cmp_with_fallback<T, Cat>>;    
    
    // a function object that invokes a 3-way comparison for the given category
    template <typename Cat>
    struct compare_3way_fallback_t {
        template <FallbackThreeWayComparable<Cat> T>
        constexpr auto operator()(T const& lhs, T const& rhs) {
            using C = cmp_with_fallback<T, Cat>;
            return C{lhs} <=> C{rhs};
        }
    };
    
    template <typename Cat>
    inline constexpr compare_3way_fallback_t<Cat> compare_3way_fallback{};
   
   
And now implementing `<=>` for `vector<T>` unconditionally is straightforward:
    
    :::cpp
    template <FallbackThreeWayComparable<weak_ordering> T>
    constexpr auto operator<=>(vector<T> const& lhs, vector<T> const& rhs) {
        return lexicographical_compare_3way(
            lhs.begin(), lhs.end(),
            rhs.begin(), rhs.end(),
            compare_3way_fallback<weak_ordering>);
    }

## What about `compare_3way()`?

Notably absent from this paper has been a real discussion over the fate of `std::compare_3way()`. R0 of this paper made this algorithm obsolete, but that's technically no longer true. It does, however, fall out from the tools we will need to build up in code to solve other problems. In fact, we've already written it:

    :::cpp
    constexpr inline auto compare_3way = compare_3way_fallback<strong_ordering>;

Nevertheless, this algorithm is very easy to misuse without thinking about it because it automatically gives you a strong ordering - which isn't apparent from the name. It's very easy to fall into the trap of thinking that this is just the correct algorithm to use whenever you want a three-way comparison, and it's really not. Using `compare_3way_fallback<Cat>` would be strictly superior as it requires clear thought. 

On the other hand, having a function object that just invokes `<=>` would actually be useful, and `compare_3way` is a good name for it. 
    
# Wording

## Core Language Wording

Remove a sentence from 10.10.2 [class.spaceship], paragraph 1:

> Let <code>x<sub>i</sub></code> be an lvalue denoting the ith element in the expanded list of subobjects for an object x (of length n), where <code>x<sub>i</sub></code> is formed by a sequence of derived-to-base conversions ([over.best.ics]), class member access expressions ([expr.ref]), and array subscript expressions ([expr.sub]) applied to x. <del>The type of the expression <code>x<sub>i</sub></code> <=> <code>x<sub>i</sub></code> is denoted by <code>R<sub>i</sub></code></del>. It is unspecified whether virtual base class subobjects are compared more than once.

Insert a new paragraph after 10.10.2 [class.spaceship], paragraph 1:

> <ins>If the declared return type of a defaulted three-way comparison operator function is `auto`, define <code><i>3WAY</i>(a, b)</code> as `a <=> b`. Otherwise, define <code><i>3WAY</i>(a, b)</code> as follows:</ins>
> 
- <ins>If `a <=> b` is well-formed, `a <=> b`;</ins>
- <ins>Otherwise, if the declared return type of the defaulted `operator<=>` is `strong_ordering`, then `(a == b) ? strong_ordering::equal : ((a < b) ? strong_ordering::less : strong_ordering::greater)`;</ins>
- <ins>Otherwise, if the declared return type of defaulted `operator<=>` is `weak_ordering`, then:</ins>
    - <ins>If `a == b` is well-formed and convertible to `bool`, then `(a == b) ? weak_ordering::equivalent : ((a < b) ? weak_ordering::less : weak_ordering::greater)`;</ins>
    - <ins>Otherwise, `(a < b) ? weak_ordering::less : ((b < a) ? weak_ordering::greater : weak_ordering::equivalent)`;</ins>
- <ins>Otherwise, if the declared return type of defaulted `operator<=>` is `partial_ordering`, then:</ins>
    - <ins>If `a == b` is well-formed and convertible to `bool`, then `(a == b) ? partial_ordering::equivalent : ((a < b) ? partial_ordering::less : ((b < a) ? partial_ordering::greater : partial_ordering::unordered))`;</ins>
    - <ins>Otherwise, `(a < b) ? partial_ordering::less : ((b < a) ? partial_ordering::greater : partial_ordering::equivalent)`;</ins>
- <ins> Otherwise, <code><i>3WAY</i>(a, b)</code> is invalid.</ins>

> <ins>The type of the expression <code><i>3WAY</i>(x<sub>i</sub>, x<sub>i</sub>)</code> is denoted by <code>R<sub>i</sub></code>. If the expression is invalid, <code>R<sub>i</sub></code> is `void`.</ins>

Add a sentence to 10.10.2 [class.spaceship], paragraph 2:

> If the declared return type of a defaulted three-way comparison operator function is `auto`, then the return type is deduced as the common comparison type (see below) of <code>R<sub>0</sub></code>, <code>R<sub>1</sub></code>, …, <code>R<sub>n-1</sub></code>. [ Note: Otherwise, the program will be ill-formed if the expression <code>x<sub>i</sub> &lt; x<sub>i</sub></code> is not implicitly convertible to the declared return type for any <code>i</code>. — end note ] If the return type is deduced as `void`, the operator function is defined as deleted. <ins>If the declared return type of a defaulted three-way comparison operator function is not `auto` and any <code>R<sub>i</sub></code> is not convertible to the provided return type, the operator function is defined as deleted.</ins>

Change 10.10.2 [class.spaceship], paragraph 3, to use `3WAY` instead of `<=>`

> The return value `V` of type `R` of the defaulted three-way comparison operator function with parameters `x` and `y` of the same type is determined by comparing corresponding elements <code>x<sub>i</sub></code> and <code>y<sub>i</sub></code> in the expanded lists of subobjects for `x` and `y` until the first index `i` where <del>x<sub>i</sub> &lt;=&gt; y<sub>i</sub></del> <ins><code><i>3WAY</i>(x<sub>i</sub>, y<sub>i</sub>)</code></ins> yields a result value <code>v<sub>i</sub></code> where <code>v<sub>i</sub> != 0</code>, contextually converted to `bool`, yields `true`; `V` is <code>v<sub>i</sub></code> converted to `R`. If no such index exists, `V` is `std::strong_ordering::equal` converted to `R`. 

## Library Wording

Add a new comparison function object to the synopsis in 16.11.1 [compare.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [cmp.common], common comparison category type  
  template&lt;class... Ts&gt;
  struct common_comparison_category {
    using type = see below;
  };
  template&lt;class... Ts&gt;
    using common_comparison_category_t = typename common_comparison_category&lt;Ts...&gt;::type;  
  
  <ins>// [cmp.3way], compare_3way</ins>
  <ins>template&lt;class T = void&gt; struct compare_3way;</ins>
  <ins>template&lt;&gt; struct compare_3way&lt;void&gt;;</ins>
  [...]
}</code></pre></blockquote>  

Add a specification for `compare_3way` to a new clause after 16.11.3 [cmp.common] named [cmp.3way]:

> <ins>The specializations of <code>compare_3way</code> for any pointer type yield a strict total order that is consistent among those specializations and is also consistent with the partial order imposed by the built-in operator <code>&lt;=&gt;</code>. For template specialization <code>compare_3way&lt;void&gt;</code>, if the call operator calls a built-in operator comparing pointers, the call operator yields a strict total order that is consistent among those specializations and is also consistent with the partial order imposed by that built-in operator.</ins>

<blockquote><ins><pre><code>template&lt;class T = void&gt; struct compare_3way {
  constexpr auto operator()(const T& x, const T& y) const;
};</code></pre>

<p><code>constexpr auto operator()(const T& x, const T& y) const;</code>

<p><i>Returns</i>: <code>x &lt;=&gt; y</code>.

<p><pre><code>template&lt;&gt; struct compare_3way&lt;void&gt; {
  template&lt;class T, class U&gt; constexpr auto operator()(T&& t, U&& u) const
    -&gt; decltype(std::forward&lt;T&gt;(t) &lt;=&gt; std::forward&lt;U&gt;(u));

  using is_transparent = unspecified;
};</code></pre>

<pre><code>template&lt;class T, class U&gt; constexpr auto operator()(T&& t, U&& u) const
    -&gt; decltype(std::forward&lt;T&gt;(t) &lt;=&gt; std::forward&lt;U&gt;(u));</code></pre>

<p><i>Returns</i>: <code>std​::​forward&lt;T&gt;(t) &lt;=&gt; std​::​forward&lt;U&gt;(u)</code>.
</ins></blockquote>

Remove `std::compare_3way()` from synopsis in 23.4 [algorithm.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [alg.3way], three-way comparison algorithms
  <del>template&lt;class T, class U&gt;</del>
  <del>  constexpr auto compare_3way(const T& a, const U& b);</del>
  template&lt;class InputIterator1, class InputIterator2, class Cmp&gt;
    constexpr auto
      lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,
                                   InputIterator2 b2, InputIterator2 e2,
                                   Cmp comp)
        -&gt; common_comparison_category_t&lt;decltype(comp(*b1, *b2)), strong_ordering&gt;;
  template&lt;class InputIterator1, class InputIterator2&gt;
    constexpr auto
      lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,
                                   InputIterator2 b2, InputIterator2 e2);
  [...]
}</code></pre></blockquote>

Remove the specification of `std::compare_3way()` of 23.7.11 \[alg.3way\]:

<blockquote><del><code>template&lt;class T, class U&gt; constexpr auto compare_3way(const T& a, const U& b);</code>

<p><i>Effects</i>: Compares two values and produces a result of the strongest applicable comparison category type:
<ul>
<li> Returns a <=> b if that expression is well-formed.
<li> Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, returns strong_­ordering​::​equal when a == b is true, otherwise returns strong_­ordering​::​less when a < b is true, and otherwise returns strong_­ordering​::​greater.
<li> Otherwise, if the expression a == b is well-formed and convertible to bool, returns strong_­equality​::​equal when a == b is true, and otherwise returns strong_­equality​::​nonequal.
<li>Otherwise, the function is defined as deleted.
</ul></del></blockquote>
    
Change the specification of `std::lexicographical_compare_3way` in 23.7.11 \[alg.3way\] paragraph 4:

<blockquote><pre><code>template&lt;class InputIterator1, class InputIterator2&gt;
  constexpr auto
    lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,
                                 InputIterator2 b2, InputIterator2 e2);</code></pre>

<i>Effects</i>: Equivalent to:
<pre><code>return lexicographical_compare_3way(b1, e1, b2, e2, <ins>compare_3way());</ins>
                                    <del>[](const auto& t, const auto& u) {</del>
                                    <del>  return compare_3way(t, u);</del>
                                    <del>});</del></code></pre>
</blockquote>
                                    
# Acknowledgments
    
Thanks to Agustín Bergé, Richard Smith, Herb Sutter, and Tony van Eerd for the many discussions around these issues. Thanks to the Core Working Group for being vigilant and ensuring a better proposal.
    
[revzin.sometimes]: https://brevzin.github.io/c++/2018/12/21/spaceship-for-vector/ "Conditionally implementing spaceship||Barry Revzin||2018-12-21"