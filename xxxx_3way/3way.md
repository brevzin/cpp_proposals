Title: 3-way, 3-args
Document-Number: DxxxxRx
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWG

# Motivation

See David Stone's [I did not order this!](https://github.com/davidstone/isocpp/blob/master/operator-spaceship/I-did-not-order-this.md) for a very clear, very thorough description of the problem: it does not seem to be possible to implement `<=>` near-optimally for "wrapper" types. For a super brief run-down, the most straightforward approach to implementing `<=>` for `std::vector<T>` is (let's just assume `strong_ordering`):

    :::cpp
    template<typename T>
    std::strong_ordering operator<=>(vector<T> const& lhs, vector<T> const& rhs) {
        size_t min_size = std::min(lhs.size(), rhs.size());
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

So what we really have to do is write two... nearly identical functions:

<table style="width:100%">
<tr>
<td style="width:50%">
    :::cpp
    template<typename T>
    std::strong_ordering operator<=>(vector<T> const& lhs,
            vector<T> const& rhs)
    {
        size_t min_size = std::min(lhs.size(), rhs.size());
        for (size_t i = 0; i != min_size; ++i) {
            auto const cmp = compare_3way(lhs[i], rhs[i]);
            if (cmp != 0) {
                return cmp;
            }
        }
        return lhs.size() <=> rhs.size();
    }
</td>
<td style="width:50%">
    :::cpp
    template<typename T>
    bool operator==(vector<T> const& lhs,
            vector<T> const& rhs)
    {
        const size_t size = lhs.size();
        if (size != rhs.size()) return false;
    
        for (size_t i = 0; i != size; ++i) {
            if (lhs[i] != rhs[i]) {
                return false;
            }
        }
        
        return true;
    }
</td>
</tr>
</table>

Both cases are doing an element-by-element comparison and bailing on when the elements are unequal, but do slightly different comparisons. But both iterate over the common range, and return at the end based on sizes. 

David already pointed out the issues of having two functions - in addition to just having to write two functions, this still doesn't solve the problem. But what if we still had one function. Just... different?

# Proposal

Today, `x == y` will either find some `operator==` or it'll try to do `x <=> y == 0`. Likewise, `x < y` will either find some `operator<` or it'll try to do `x <=> y < 0`. The underlying principle of spaceship is that there's one ordering, so it's one operation that just yields the trichotomy. 

But as this example points out, it's not _entirely_ one operation. It's more like two: equality and ordering. When we do ordering (i.e. `<`, `>`, `<=`, or `>=`), we need to do everything - we can't short circuit and we need to know the trichotomy of each pair of elements. But when we do equality (i.e. `==` or `!=`), we don't - we can short circuit and we just need to know whether the elements are equal or not.

So what if instead, we had these two objects named `eq` and `ord` that were different types, but encoded some value such that they could be compared as constant expressions. And we used those objects as an convey extra info into `<=>`. That is:

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

With that change, our spaceship for a totally ordered `vector`, as well as the new `compare_3way()` for those types that implement all the operators but not `<=>`, becomes:

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
        
        size_t min_size = std::min(lhs.size(), rhs.size());
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
        auto operator<=>(S const&) = default;
    };
    
With this model, doing an equality comparison on two `S`s will mean calling `operator<=>(lhs.names, rhs.names, eq)`, which will short-circuit on the sizes of the two `vector`s first and then just do an equality comparison on the underlying `string`s. And _that_ comparison will _also_ itself short-circuit on the sizes of the `string`s first. 

This new suggested implementation of `<=>` for `vector` is admittedly quite a bit more complicated than our initial implementation. But it's arguably still quite a bit _less_ complicated than the code we have to write in C++17 today, and it's arguably less complicated than having to write 2 or 3 functions to achieve optimal performance - and still not really being able to do it. 