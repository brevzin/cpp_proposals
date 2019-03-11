Title: The Mothership Has Landed <br /> Adding `<=>` to the Library
Document-Number: D1614R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: LWG

# Introduction

The work of integrating `operator<=>` into the library has been performed by multiple different papers, each addressing a different aspect of the integration. In the interest of streamlining review by the Library Working Group, the wording has been combined into a single paper. This is that paper.

In San Diego and Kona, several papers were approved by LEWG adding functionality to the library related to comparisons. What follows is the list of those papers, in alphabetical order, with a brief description of what those papers are. The complete motivation and design rationale for each can be found within the papers themselves.

- [P0790R2](https://wg21.link/p0790r2) - adding `operator<=>` to the standard library types whose behavior is not dependent on a template parameter.
- [P0891R2](https://wg21.link/p0891r2) - making the `XXX_order` algorithms customization points and introducing `compare_XXX_order_fallback` algorithms that preferentially invoke the former algorithm and fallback to synthesizing an ordering from `==` and `<` (using the rules from [P1186R1](https://wg21.link/p1186r1)).
- [P1154R1](https://wg21.link/p1154r1) - adding the type trait `has_strong_structural_equality<T>` (useful to check if a type can be used as a non-type template parameter).
- [P1188R0](https://wg21.link/p1188r0) - adding the type trait `compare_three_way_result<T>`, the concepts `ThreeWayComparable<T>` and `ThreeWayComparableWith<T,U>`, removing the algorithm `compare_3way` and replacing it with a function comparison object `compare_three_way` (i.e. the `<=>` version of `std::ranges::less`).
- [P1189R0](https://wg21.link/p1189r0) - adding `operator<=>` to the standard library types whose behavior is dependent on a template parameter, removing those equality operators made redundant by [P1185R1](https://wg21.link/p1185r1) and defaulting `operator==` where appropriate.
- [P1191R0](https://wg21.link/p1191r0) - adding equality to several previously incomparable standard library types.
- [P1295R0](https://wg21.link/p1295r0) - adding equality and `common_type` for the comparison categories.
- [P1380R1](https://wg21.link/p1380r1) - extending the floating point customization points for `strong_order` and `weak_order`.

LEWG's unanimous preference was that `operator<=>`s be declared as hidden friends.

# Known behavioral changes

There are a few things that will change behavior as a result of all these papers and the chosen direction for declaring operators as hidden friends. While any change will break somebody, we're probably not terribly concerned about examples like:

    :::cpp
    using namespace std;
    struct X { operator error_code() const; };
    X{} == X{}; // ok in C++17, ill-formed with this change

Here is an example of something that was well-formed and becomes ill-formed, reproduced from LLVM:

    :::cpp hl_lines="8"
    struct StringRef {
        StringRef(std::string const&); // NB: non-explicit
        operator std::string() const;  // NB: non-explicit
    };
    bool operator==(StringRef, StringRef);

    bool f(StringRef a, std::string b) {
        return a == b; // (*)
    }

In C++17, the marked line is well-formed. The `operator==` for `basic_string` is a non-member function template, and so would not be considered a candidate; the only viable candidate is the `operator==` taking two `StringRef`s. With the proposed changes, the `operator==` for `basic_string` becomes a non-member hidden friend, _non-template_, which makes it a candidate (converting `a` to a `string`). That candidate is ambiguous with the `operator==(StringRef, StringRef)` candidate - each requires a conversion in one argument, so the call becomes ill-formed.

Here is an example of something that was ill-formed and becomes well-formed:

    :::cpp hl_lines="2"
    bool is42(std::variant<int, std::string> const& v) {
        return v == 42; // (*)
    }

In C++17, the `operator==` for `variant` is a non-member function template and is thus not a viable candidate for the marked line. That check is ill-formed. With the proposed changes, the `operator==` for `variant` becomes a non-member hidden friend, _non-template_, which makes it a candidate (converting `42` to a `variant<int, string>`). This is arguably a fix, since both `variant<int, string> v = 42;` and `v = 42;` are already well-formed, so it is surely reasonable that `v == 42` is as well.

# Acknowledgments

Thank you to Casey Carter for the tremendous wording review.

# Wording

## Clause 15: Library Introduction

Change 15.4.2.1/2 [expos.only.func]:

> The following <del>function is</del> <ins>are</ins> defined for exposition only to aid in the specification of the library:

and append:

<blockquote><pre><code class="language-cpp">constexpr auto </code><code><i>synth-3way</i></code><code class="language-cpp"> =
  []&lt;class T, class U&gt;(const T& t, const U& u)
    requires requires {
      { t < u } -> bool;
      { u < t } -> bool;
    }
  {
    if constexpr (ThreeWayComparableWith&lt;T, U&gt;) {
      return t <=> u;
    } else {
      if (t < u) return weak_ordering::less;
      if (u < t) return weak_ordering::greater;
      return weak_ordering::equivalent;
    }
  };

template&lt;class T, class U=T&gt;
using </code><code><i>synth-3way-result</i></code><code class="language-cpp"> = decltype(</code><code><i>synth-3way</i></code><code class="language-cpp">(declval&lt;T&&gt;(), declval&lt;U&&gt;()));</code></pre></blockquote>

Remove 15.4.2.3 [operators], which begins:

> <del>In this library, whenever a declaration is provided for an `operator!=`, `operator>`, `operator<=`, or `operator>=` for a type `T`, its requirements and semantics are as follows, unless explicitly specified otherwise.</del>

Add a clause to 15.5.5 [conforming], probably after 15.5.5.4 [global.functions]. Not strictly related to `<=>` as a whole, but it's a requirement that's currently missing and needs to be added somewhere. See also P1601.

> **15.5.5.x Hidden friend functions [conforming.hidden.friend]**
>
> An implementation shall not provide any additional out-of-class declarations or redeclarations for any non-member function specified as a non-member `friend` and defined within the body of a class. [ *Note*: The intent is that such functions are to be found via argument-dependent lookup only. *-end note* ]

## Clause 16: Language support library

Added: `compare_three_way_result`, concepts `ThreeWayComparable` and `ThreeWayComparableWith`, `compare_three_way` and `compare_XXX_order_fallback`

Changed operators for: `type_info`

Respecified: `strong_order()`, `weak_order()`, and `partial_order()`

Removed: `compare_3way()`, `strong_equal()`, and `weak_equal()`

In 16.7.2 [type.info], remove `operator!=`:

<blockquote><pre><code>namespace std {
  class type_info {
  public:
    virtual ~type_info();
    bool operator==(const type_info& rhs) const noexcept;
    <del>bool operator!=(const type_info& rhs) const noexcept;</del>
    bool before(const type_info& rhs) const noexcept;
    size_t hash_code() const noexcept;
    const char* name() const noexcept;
    type_info(const type_info& rhs) = delete; // cannot be copied
    type_info& operator=(const type_info& rhs) = delete; // cannot be copied
  };
}</code></pre></blockquote> and

> <pre><code>bool operator==(const type_info& rhs) const noexcept;</code></pre>
> *Effects*: Compares the current object with rhs.  
> *Returns*: `true` if the two values describe the same type.  
> <pre><code><del>bool operator!=(const type_info& rhs) const noexcept;</del></code></pre>
> <del>*Returns*: `!(*this == rhs)`.</del>

Add into 16.11.1 [compare.syn]:

<blockquote><pre><code>namespace std {
  // [cmp.categories], comparison category types
  class weak_equality;
  class strong_equality;
  class partial_ordering;
  class weak_ordering;
  class strong_ordering;

  // named comparison functions
  constexpr bool is_eq  (weak_equality cmp) noexcept    { return cmp == 0; }
  constexpr bool is_neq (weak_equality cmp) noexcept    { return cmp != 0; }
  constexpr bool is_lt  (partial_ordering cmp) noexcept { return cmp < 0; }
  constexpr bool is_lteq(partial_ordering cmp) noexcept { return cmp <= 0; }
  constexpr bool is_gt  (partial_ordering cmp) noexcept { return cmp > 0; }
  constexpr bool is_gteq(partial_ordering cmp) noexcept { return cmp >= 0; }
  
  <ins>// common_type specializations</ins>
  <ins>template&lt;&gt; struct common_type&lt;strong_equality, partial_ordering&gt;</ins>
  <ins>  { using type = weak_equality; };</ins>
  <ins>template&lt;&gt; struct common_type&lt;partial_ordering, strong_equality&gt;</ins>
  <ins>  { using type = weak_equality; };</ins>
  <ins>template&lt;&gt; struct common_type&lt;strong_equality, weak_ordering&gt;</ins>
  <ins>  { using type = weak_equality; };</ins>
  <ins>template&lt;&gt; struct common_type&lt;weak_ordering, strong_equality&gt;</ins>
  <ins>  { using type = weak_equality; };</ins>
  
  // [cmp.common], common comparison category type  
  template&lt;class... Ts&gt;
  struct common_comparison_category {
    using type = see below;
  };
  template&lt;class... Ts&gt;
    using common_comparison_category_t = typename common_comparison_category&lt;Ts...&gt;::type;  
  
  <ins>// [cmp.concept], concept ThreeWayComparable</ins>
  <ins>template&lt;class T, class Cat = partial_ordering&gt;</ins>
    <ins>concept ThreeWayComparable = <i>see below</i>;</ins>
  <ins>template&lt;class T, class U, class Cat = partial_ordering&gt;</ins>
    <ins>concept ThreeWayComparableWith = <i>see below</i>;</ins>
  
  <ins>// [cmp.result], spaceship invocation result</ins>
  <ins>template&lt;class T, class U = T&gt; struct compare_three_way_result;</ins>
  
  <ins>template&lt;class T, class U = T&gt;</ins>
  <ins>  using compare_three_way_result_t = typename compare_three_way_result&lt;T, U&gt;::type;</ins>
  
  <ins>// [cmp.object], spaceship object</ins>
  <ins>struct compare_three_way;</ins>
  
  // [cmp.alg], comparison algorithms
  <del>template&lt;class T&gt; constexpr strong_ordering strong_order(const T& a, const T& b);</del>
  <del>template&lt;class T&gt; constexpr weak_ordering weak_order(const T& a, const T& b);</del>
  <del>template&lt;class T&gt; constexpr partial_ordering partial_order(const T& a, const T& b);</del>
  <del>template&lt;class T&gt; constexpr strong_equality strong_equal(const T& a, const T& b);</del>
  <del>template&lt;class T&gt; constexpr weak_equality weak_equal(const T& a, const T& b);</del>
  <ins>inline namespace <i>unspecified</i> {</ins>
    <ins>inline constexpr <i>unspecified</i> strong_order = <i>unspecified</i>;</ins>
    <ins>inline constexpr <i>unspecified</i> weak_order = <i>unspecified</i>;</ins>
    <ins>inline constexpr <i>unspecified</i> partial_order = <i>unspecified</i>;</ins>
    <ins>inline constexpr <i>unspecified</i> compare_strong_order_fallback = <i>unspecified</i>;</ins>
    <ins>inline constexpr <i>unspecified</i> compare_weak_order_fallback = <i>unspecified</i>;</ins>
    <ins>inline constexpr <i>unspecified</i> compare_partial_order_fallback = <i>unspecified</i>;</ins>
  <ins>}</ins>
}</code></pre></blockquote>

Change 16.11.2.2 [cmp.weakeq]:

<blockquote><pre><code>namespace std {
  class weak_equality {
    int value;  // exposition only
    [...]

    // comparisons
    friend constexpr bool operator==(weak_equality v, <i>unspecified</i>) noexcept<del>;</del>
    <ins> { return v.value == 0; }</ins>
    <del>friend constexpr bool operator!=(weak_equality v, <i>unspecified</i>) noexcept;</del>
    <del>friend constexpr bool operator==(<i>unspecified</i>, weak_equality v) noexcept;</del>
    <Del>friend constexpr bool operator!=(<i>unspecified</i>, weak_equality v) noexcept;</del>
    <ins>friend constexpr bool operator==(weak_equality v, weak_equality w) noexcept = default;</ins>
    friend constexpr weak_equality operator<=>(weak_equality v, <i>unspecified</i>) noexcept<del>;</del>
    <ins> { return v; }</ins>
    friend constexpr weak_equality operator<=>(<i>unspecified</i>, weak_equality v) noexcept<del>;</del>
    <ins> { return v; }</ins>
  };

  // valid values' definitions
  inline constexpr weak_equality weak_equality::equivalent(eq::equivalent);
  inline constexpr weak_equality weak_equality::nonequivalent(eq::nonequivalent);
}</code></pre></blockquote>

Remove the rest of the clause (now defined inline):

> <pre><code><del>constexpr bool operator==(weak_equality v, unspecified) noexcept;
constexpr bool operator==(unspecified, weak_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v.value == 0`.</del>
> <pre><code><del>constexpr bool operator!=(weak_equality v, unspecified) noexcept;
constexpr bool operator!=(unspecified, weak_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v.value != 0`.</del>
> <pre><code><del>constexpr weak_equality operator<=>(weak_equality v, unspecified) noexcept;
constexpr weak_equality operator<=>(unspecified, weak_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v`.</del>

Change 16.11.2.3 [cmp.strongeq]:

<blockquote><pre><code>namespace std {
  class strong_equality {
    int value;  // exposition only
    [...]

    // comparisons
    friend constexpr bool operator==(strong_equality v, <i>unspecified</i>) noexcept<del>;</del>
    <ins>  { return v.value == 0; }</ins>
    <del>friend constexpr bool operator!=(strong_equality v, <i>unspecified</i>) noexcept;</del>
    <del>friend constexpr bool operator==(<i>unspecified</i>, strong_equality v) noexcept;</del>
    <del>friend constexpr bool operator!=(<i>unspecified</i>, strong_equality v) noexcept;</del>
    <ins>friend constexpr bool operator==(strong_equality v, strong_equality w) noexcept = default;</ins>
    friend constexpr strong_equality operator&lt;=&gt;(strong_equality v, <i>unspecified</i>) noexcept<del>;</del>
    <ins>  { return v; }</ins>
    friend constexpr strong_equality operator&lt;=&gt;(unspecified, strong_equality v) noexcept<del>;</del>
    <ins>  { return v; }</ins>
  };

  // valid values' definitions
  inline constexpr strong_equality strong_equality::equal(eq::equal);
  inline constexpr strong_equality strong_equality::nonequal(eq::nonequal);
  inline constexpr strong_equality strong_equality::equivalent(eq::equivalent);
  inline constexpr strong_equality strong_equality::nonequivalent(eq::nonequivalent);
}</code></pre></blockquote>

Remove most of the rest of the clause:

> <pre><code>constexpr operator weak_equality() const noexcept;</code></pre>
> *Returns*: `value == 0 ? weak_equality::equivalent : weak_equality::nonequivalent`.
> <pre><code><del>constexpr bool operator==(strong_equality v, unspecified) noexcept;
constexpr bool operator==(unspecified, strong_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v.value == 0`.</del>
> <pre><code><del>constexpr bool operator!=(strong_equality v, unspecified) noexcept;
constexpr bool operator!=(unspecified, strong_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v.value != 0`.</del>
> <pre><code><del>constexpr strong_equality operator&lt;=&gt;(strong_equality v, unspecified) noexcept;
constexpr strong_equality operator&lt;=&gt;(unspecified, strong_equality v) noexcept;</del></code></pre>
> <del>*Returns*: `v`.</del>

Change 16.11.2.4 [cmp.partialord]:

<blockquote><pre><code>namespace std {
  class partial_ordering {
    int value;          // exposition only
    bool is_ordered;    // exposition only

    [...]
    // conversion
    constexpr operator weak_equality() const noexcept;

    // comparisons
    friend constexpr bool operator==(partial_ordering v, <i>unspecified</i>) noexcept;
    <del>friend constexpr bool operator!=(partial_ordering v, <i>unspecified</i>) noexcept;</del>
    <ins>friend constexpr bool operator==(partial_ordering v, partial_ordering w) noexcept = default;</ins>
    friend constexpr bool operator&lt; (partial_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt; (partial_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&lt;=(partial_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt;=(partial_ordering v, <i>unspecified</i>) noexcept;
    <del>friend constexpr bool operator==(<i>unspecified</i>, partial_ordering v) noexcept;</del>
    <del>friend constexpr bool operator!=(<i>unspecified</i>, partial_ordering v) noexcept;</del>
    friend constexpr bool operator&lt; (<i>unspecified</i>, partial_ordering v) noexcept;
    friend constexpr bool operator&gt; (<i>unspecified</i>, partial_ordering v) noexcept;
    friend constexpr bool operator&lt;=(<i>unspecified</i>, partial_ordering v) noexcept;
    friend constexpr bool operator&gt;=(<i>unspecified</i>, partial_ordering v) noexcept;
    friend constexpr partial_ordering operator&lt;=&gt;(partial_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr partial_ordering operator&lt;=&gt;(<i>unspecified</i>, partial_ordering v) noexcept;
  };

  [...]
}</code></pre></blockquote>

Remove just the extra `==` and `!=` operators in 16.11.2.4 [cmp.partialord]/3 and 4:

> <pre><code>constexpr bool operator==(partial_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&lt; (partial_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt; (partial_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&lt;=(partial_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt;=(partial_ordering v, <i>unspecified</i>) noexcept;</code></pre>
> *Returns*: For `operator@`, `v.is_ordered && v.value @ 0`.
> <pre><code><del>constexpr bool operator==(<i>unspecified</i>, partial_ordering v) noexcept;</del>
constexpr bool operator< (<i>unspecified</i>, partial_ordering v) noexcept;
constexpr bool operator> (<i>unspecified</i>, partial_ordering v) noexcept;
constexpr bool operator<=(<i>unspecified</i>, partial_ordering v) noexcept;
constexpr bool operator>=(<i>unspecified</i>, partial_ordering v) noexcept;</code></pre>
> *Returns*: For `operator@`, `v.is_ordered && 0 @ v.value`.
> <pre><code><del>constexpr bool operator!=(partial_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator!=(<i>unspecified</i>, partial_ordering v) noexcept;</del></code></pre>
> <del>*Returns*: For `operator@`, `!v.is_ordered || v.value != 0`.</del>

Change 16.11.2.5 [cmp.weakord]:

<blockquote><pre><code>namespace std {
  class weak_ordering {
    int value;  // exposition only

    [...]
    // comparisons
    friend constexpr bool operator==(weak_ordering v, <i>unspecified</i>) noexcept;
    <ins>friend constexpr bool operator==(weak_ordering v, weak_ordering w) noexcept = default;</ins>
    <del>friend constexpr bool operator!=(weak_ordering v, <i>unspecified</i>) noexcept;</del>
    friend constexpr bool operator&lt; (weak_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt; (weak_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&lt;=(weak_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt;=(weak_ordering v, <i>unspecified</i>) noexcept;
    <del>friend constexpr bool operator==(<i>unspecified</i>, weak_ordering v) noexcept;</del>
    <del>friend constexpr bool operator!=(<i>unspecified</i>, weak_ordering v) noexcept;</del>
    friend constexpr bool operator&lt; (<i>unspecified</i>, weak_ordering v) noexcept;
    friend constexpr bool operator&gt; (<i>unspecified</i>, weak_ordering v) noexcept;
    friend constexpr bool operator&lt;=(<i>unspecified</i>, weak_ordering v) noexcept;
    friend constexpr bool operator&gt;=(<i>unspecified</i>, weak_ordering v) noexcept;
    friend constexpr weak_ordering operator&lt;=&gt;(weak_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr weak_ordering operator&lt;=&gt;(<i>unspecified</i>, weak_ordering v) noexcept;
  };

  [...]
};</code></pre></blockquote>

Remove just the extra `==` and `!=` operators from 16.11.2.5 [cmp.weakord]/4 and /5:

> <pre><code>constexpr bool operator==(weak_ordering v, <i>unspecified</i>) noexcept;
<del>constexpr bool operator!=(weak_ordering v, <i>unspecified</i>) noexcept;</del>
constexpr bool operator&lt; (weak_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt; (weak_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&lt;=(weak_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt;=(weak_ordering v, <i>unspecified</i>) noexcept;</code></pre>
> *Returns*: `v.value @ 0` for `operator@`.
> <pre><code><del>constexpr bool operator==(<i>unspecified</i>, weak_ordering v) noexcept;</del>
<del>constexpr bool operator!=(<i>unspecified</i>, weak_ordering v) noexcept;</del>
constexpr bool operator&lt; (<i>unspecified</i>, weak_ordering v) noexcept;
constexpr bool operator&gt; (<i>unspecified</i>, weak_ordering v) noexcept;
constexpr bool operator&lt;=(<i>unspecified</i>, weak_ordering v) noexcept;
constexpr bool operator&gt;=(<i>unspecified</i>, weak_ordering v) noexcept;</code></pre>
> *Returns*: `0 @ v.value` for `operator@`.

Change 16.11.2.6 [cmp.strongord]:

<blockquote><pre><code>namespace std {
  class strong_ordering {
    int value;  // exposition only

    [...]
    
    // comparisons
    friend constexpr bool operator==(strong_ordering v, <i>unspecified</i>) noexcept;
    <ins>friend constexpr bool operator==(strong_ordering v, strong_ordering w) noexcept = default;</ins>
    <del>friend constexpr bool operator!=(strong_ordering v, <i>unspecified</i>) noexcept;</del>
    friend constexpr bool operator&lt; (strong_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt; (strong_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&lt;=(strong_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr bool operator&gt;=(strong_ordering v, <i>unspecified</i>) noexcept;
    <del>friend constexpr bool operator==(<i>unspecified</i>, strong_ordering v) noexcept;</del>
    <del>friend constexpr bool operator!=(<i>unspecified</i>, strong_ordering v) noexcept;</del>
    friend constexpr bool operator&lt; (<i>unspecified</i>, strong_ordering v) noexcept;
    friend constexpr bool operator&gt; (<i>unspecified</i>, strong_ordering v) noexcept;
    friend constexpr bool operator&lt;=(<i>unspecified</i>, strong_ordering v) noexcept;
    friend constexpr bool operator&gt;=(<i>unspecified</i>, strong_ordering v) noexcept;
    friend constexpr strong_ordering operator&lt;=&gt;(strong_ordering v, <i>unspecified</i>) noexcept;
    friend constexpr strong_ordering operator&lt;=&gt;(<i>unspecified</i>, strong_ordering v) noexcept;
  };

  [...]
}</code></pre></blockquote>

Remove just the extra `==` and `!=` operators from 16.11.2.6 [cmp.strongord]/6 and /7:

> <pre><code>constexpr bool operator==(strong_ordering v, <i>unspecified</i>) noexcept;
<del>constexpr bool operator!=(strong_ordering v, <i>unspecified</i>) noexcept;</del>
constexpr bool operator&lt; (strong_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt; (strong_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&lt;=(strong_ordering v, <i>unspecified</i>) noexcept;
constexpr bool operator&gt;=(strong_ordering v, <i>unspecified</i>) noexcept;</code></pre>
> *Returns*: `v.value @ 0` for `operator@`.
> <pre><code><del>constexpr bool operator==(<i>unspecified</i>, strong_ordering v) noexcept;</del>
<del>constexpr bool operator!=(<i>unspecified</i>, strong_ordering v) noexcept;</del>
constexpr bool operator&lt; (<i>unspecified</i>, strong_ordering v) noexcept;
constexpr bool operator&gt; (<i>unspecified</i>, strong_ordering v) noexcept;
constexpr bool operator&lt;=(<i>unspecified</i>, strong_ordering v) noexcept;
constexpr bool operator&gt;=(<i>unspecified</i>, strong_ordering v) noexcept;</code></pre>
> *Returns*: `0 @ v.value` for `operator@`.


Add a new subclause [cmp.concept] "concept `ThreeWayComparable`":

> <pre><code class="language-cpp">template &lt;typename T, typename Cat&gt;
  concept </code><code><i>compares-as</i></code><code class="language-cpp"> = // exposition only
    Same&lt;common_comparison_category_t&lt;T, Cat&gt;, Cat&gt;;</code></pre>

> <pre><code class="language-cpp">template&lt;class T, class U&gt;
  concept </code><code><i>partially-ordered-with</i></code><code class="language-cpp"> = // exposition only
    requires(const remove_reference_t&lt;T&gt;& t,
             const remove_reference_t&lt;U&gt;& u) {
      { t &lt; u } -&gt; Boolean;
      { t &gt; u } -&gt; Boolean;
      { t &lt;= u } -&gt; Boolean;
      { t &gt;= u } -&gt; Boolean;
      { u &lt; t } -&gt; Boolean;
      { u &gt; t } -&gt; Boolean;
      { u &lt;= t } -&gt; Boolean;
      { u &gt;= t } -&gt; Boolean;      
    };</code></pre>
    
> Let `t` and `u` be lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>` respectively. <code><i>partially-ordered-with</i>&lt;T, U&gt;</code> is satisfied only if:
>
> - `t < u`, `t <= u`, `t > u`, `t >= u`, `u < t`, `u <= t`, `u > t`, and `u >= t` have the same domain.
> - `bool(t < u) == bool(u > t)`
> - `bool(u < t) == bool(t > u)`
> - `bool(t <= u) == bool(u >= t)`
> - `bool(u <= t) == bool(t >= u)`
    
> <pre><code class="language-cpp">template &lt;typename T, typename Cat = partial_ordering&gt;
  concept ThreeWayComparable =
    </code><code><i>weakly-equality-comparable-with</i></code><code class="language-cpp">&lt;T, T&gt; &&
    (!ConvertibleTo&lt;Cat, partial_ordering&gt; || </code><code><i>partially-ordered-with</i></code><code class="language-cpp">&lt;T, T&gt;) &&
    requires(const remove_reference_t&lt;T&gt;& a,
             const remove_reference_t&lt;T&gt;& b) {
      { a &lt;=&gt; b } -&gt; </code><code><i>compares-as</i></code><code class="language-cpp">&lt;Cat&gt;;
    };</code></pre>
        
> Let `a` and `b` be lvalues of type `const remove_reference_t<T>`. `T` and `Cat` model `ThreeWayComparable<T, Cat>` only if:
> 
> - `(a <=> b == 0) == bool(a == b)`.
> - `(a <=> b != 0) == bool(a != b)`.
> - `((a <=> b) <=> 0)` and `(0 <=> (b <=> a))` are equal
> - If `Cat` is convertible to `strong_equality`, `T` models `EqualityComparable` ([concept.equalitycomparable]).
> - If `Cat` is convertible to `partial_ordering`:
>       - `(a <=> b < 0) == bool(a < b)`.
>       - `(a <=> b > 0) == bool(a > b)`.
>       - `(a <=> b <= 0) == bool(a <= b)`.
>       - `(a <=> b >= 0) == bool(a >= b)`.
> - If `Cat` is convertible to `strong_ordering`, `T` models `StrictTotallyOrdered` ([concept.stricttotallyordered]). 

> <pre><code class="language-cpp">template &lt;typename T, typename U,
          typename Cat = partial_ordering&gt;
  concept ThreeWayComparableWith = 
    </code><code><i>weakly-equality-comparable-with</i></code><code class="language-cpp">&lt;T, U&gt; &&
    (!ConvertibleTo&lt;Cat, partial_ordering&gt; || </code><code><i>partially-ordered-with</i></code><code class="language-cpp">&lt;T, U&gt;) &&
    ThreeWayComparable&lt;T, Cat&gt; &&
    ThreeWayComparable&lt;U, Cat&gt; &&
    CommonReference&lt;const remove_reference_t&lt;T&gt;&, const remove_reference_t&lt;U&gt;&&gt; &&
    ThreeWayComparable&lt;
      common_reference_t&lt;const remove_reference_t&lt;T&gt;&, const remove_reference_t&lt;U&gt;&&gt;,
      Cat&gt; &&
    requires(const remove_reference_t&lt;T&gt;& t,
             const remove_reference_t&lt;U&gt;& u) {
      { t &lt;=&gt; u } -&gt; </code><code><i>compares-as</i></code><code class="language-cpp">&lt;Cat&gt;;
      { u &lt;=&gt; t } -&gt; </code><code><i>compares-as</i></code><code class="language-cpp">&lt;Cat&gt;;
    };</code></pre>
> Let `t` and `u` be lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`, respectively. Let `C` be `common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>`. `T`, `U`, and `Cat` model `ThreeWayComparableWith<T, U, Cat>` only if:
>
> - `t <=> u` and `u <=> t` have the same domain.
> - `((t <=> u) <=> 0)` and `(0 <=> (u <=> t))` are equal
> - `(t <=> u == 0) == bool(t == u)`.
> - `(t <=> u != 0) == bool(t != u)`.
> - `Cat(t <=> u) == Cat(C(t) <=> C(u))`.
> - If `Cat` is convertible to `strong_equality`, `T` and `U` model `EqualityComparableWith<T, U>` ([concepts.equalitycomparable]).
> - If `Cat` is convertible to `partial_ordering`:
>       - `(t <=> u < 0) == bool(t < u)`
>       - `(t <=> u > 0) == bool(t > u)`
>       - `(t <=> u <= 0) == bool(t <= u)`
>       - `(t <=> u >= 0) == bool(t >= u)`
> - If `Cat` is convertible to `strong_ordering`, `T` and `U` model `StrictTotallyOrderedWith<T, U>` ([concepts.stricttotallyordered]).

Add a new subclause [cmp.result] "spaceship invocation result":

> The behavior of a program that adds specializations for the `compare_three_way_result` template defined in this subclause is undefined.

> For the `compare_three_way_result` type trait applied to the types `T` and `U`, let `t` and `u` denote lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`, respectively. If the expression `t <=> u` is well-formed, the member *typedef-name* `type` denotes the type `decltype(t <=> u)`. Otherwise, there is no member `type`.

Add a new subclause [cmp.object] "spaceship object":

> In this subclause, `BUILTIN_PTR_3WAY(T, U)` for types `T` and `U` is a boolean constant expression. `BUILTIN_PTR_3WAY(T, U)` is `true` if and only if `<=>` in the expression `declval<T>() <=> declval<U>()` resolves to a built-in operator comparing pointers.

> 
    :::cpp
    struct compare_three_way {
      template<class T, class U>
        requires ThreeWayComparableWith<T,U> || BUILTIN_PTR_3WAY(T, U)
      constexpr auto operator()(T&& t, U&& u) const;
>      
      using is_transparent = unspecified;
    };

> *Expects*: If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`, the conversion sequences from both `T` and `U` to `P` are equality-preserving ([concepts.equality]).

> *Effects*: 
> 
> - If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`: returns `strong_ordering::less` if (the converted value of) `t` precedes `u` in the implementation-defined strict total order ([range.cmp]) over pointers of type `P`, `strong_ordering::greater` if `u` precedes `t`, and otherwise `strong_ordering::equal`.
> - Otherwise, equivalent to: `return std::forward<T>(t) <=> std::forward<U>(u);`

> In addition to being available via inclusion of the `<compare>` header, the class `compare_three_way` is available when the header `<functional>` is included.
        
Replace the entirety of 16.11.4 [cmp.alg]. This wording relies on the specification-only function `3WAY<R>` defined in [P1186R1](https://wg21.link/p1186r1).

> <pre><code><del>template&lt;class T&gt; constexpr strong_ordering strong_order(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type `strong_ordering`:</del>  
> 
> - <del>If numeric_limits<T>::is_iec559 is true, returns a result of type strong_ordering that is consistent with the totalOrder operation as specified in ISO/IEC/IEEE 60559.</del>
> - <del>Otherwise, returns a <=> b if that expression is well-formed and convertible to strong_ordering.</del>
> - <del>Otherwise, if the expression a <=> b is well-formed, then the function is defined as deleted.</del>
> - <del>Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, then</del>
>       - <del>if a == b is true, returns strong_ordering::equal;</del>
>       - <del>otherwise, if a < b is true, returns strong_ordering::less;</del>
>       - <del>otherwise, returns strong_ordering::greater.</del>
> - <del>Otherwise, the function is defined as deleted.</del>

> <pre><code><del>template&lt;class T&gt; constexpr weak_ordering weak_order(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type weak_ordering:</del>
>
> - <del>Returns a <=> b if that expression is well-formed and convertible to weak_ordering.</del>
> - <del>Otherwise, if the expression a <=> b is well-formed, then the function is defined as deleted.</del>
> - <del>Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, then</del>
>       - <del>if a == b is true, returns weak_ordering::equivalent;</del>
>       - <del>otherwise, if a < b is true, returns weak_ordering::less;</del>
>       - <del>otherwise, returns weak_ordering::greater.</del>
> - <del>Otherwise, the function is defined as deleted.</del>
>
> <pre><code><del>template&lt;class T&gt; constexpr partial_ordering partial_order(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type partial_ordering:</del>
> 
> - <del>Returns a <=> b if that expression is well-formed and convertible to partial_ordering.</del>
> - <del>Otherwise, if the expression a <=> b is well-formed, then the function is defined as deleted.</del>
> - <del>Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, then</del>
>       - <del>if a == b is true, returns partial_ordering::equivalent;</del>
>       - <del>otherwise, if a < b is true, returns partial_ordering::less;</del>
>       - <del>otherwise, returns partial_ordering::greater.</del>
> - <del>Otherwise, the function is defined as deleted.</del>

> <pre><code><del>template&lt;class T&gt; constexpr strong_equality strong_equal(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type strong_equality:</del>
> 
> - <del>Returns a <=> b if that expression is well-formed and convertible to strong_equality.</del>
> - <del>Otherwise, if the expression a <=> b is well-formed, then the function is defined as deleted.</del>
> - <del>Otherwise, if the expression a == b is well-formed and convertible to bool, then</del>
>       - <del>if a == b is true, returns strong_equality::equal;</del>
>       - <del>otherwise, returns strong_equality::nonequal.</del>
> - <del>Otherwise, the function is defined as deleted.</del>

> <pre><code><del>template&lt;class T&gt; constexpr weak_equality weak_equal(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type weak_equality:</del>
> 
> - <del>Returns a <=> b if that expression is well-formed and convertible to weak_equality.</del>
> - <del>Otherwise, if the expression a <=> b is well-formed, then the function is defined as deleted.</del>
> - <del>Otherwise, if the expression a == b is well-formed and convertible to bool, then</del>
>       - <del>if a == b is true, returns weak_equality::equivalent;</del>
>       - <del>otherwise, returns weak_equality::nonequivalent.</del>
> - <del>Otherwise, the function is defined as deleted.</del>

> <ins>The name `strong_order` denotes a customization point object ([customization.point.object]). The expression `strong_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:
> 
> - <ins>If the decayed types of `E` and `F` differ, `strong_order(E, F)` is ill-formed.</ins>
> - <ins>Otherwise, `strong_ordering(strong_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::strong_order`.</ins>
> - <ins>Otherwise, if the decayed type `T` of `E` and `F` is a floating point type, yields a value of type `strong_ordering` that is consistent with the ordering observed by `T`'s comparison operators, and if `numeric_limits<T>::is_iec559` is `true` is additionally consistent with the totalOrder operation as specified in ISO/IEC/IEEE 60599.</ins>
> - <ins>Otherwise, `strong_ordering(E <=> F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `strong_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `strong_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>

> <ins>The name `weak_order` denotes a customization point object ([customization.point.object]). The expression `weak_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:</ins>
>
> - <ins>If the decayed types of `E` and `F` differ, `weak_order(E, F)` is ill-formed.</ins> 
> - <ins>Otherwise, `weak_ordering(weak_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::weak_order`.</ins>
> - <ins>Otherwise, if the decayed type `T` of `E` and `F` is a floating point type, yields a value of type `weak_ordering` that is consistent with the ordering observed by `T`'s comparison operators and `strong_order`, and if `numeric_liits<T>::is_iec559` is `true` is additionally consistent with the following equivalence classes, ordered from lesser to greater:</ins>
>      - <ins>Together, all negative NaN values</ins>
>      - <ins>Negative infinity</ins>
>      - <ins>Each normal negative value</ins>
>      - <ins>Each subnormal negative value</ins>
>      - <ins>Together, both zero values</ins>
>      - <ins>Each subnormal positive value</ins>
>      - <ins>Each normal positive value</ins>
>      - <ins>Positive infinity</ins>
>      - <ins>Together, all positive NaN values</ins>
> - <ins>Otherwise, `weak_ordering(strong_order(E, F))` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `weak_ordering(E <=> F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `weak_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::weak_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>

> <ins>The name `partial_order` denotes a customization point object ([customization.point.object]). The expression `partial_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:</ins>
> 
> - <ins>If the decayed types of `E` and `F` differ, `partial_order(E, F)` is ill-formed.</ins>
> - <ins>Otherwise, `partial_ordering(partial_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::partial_order`.</ins>
> - <ins>Otherwise, `partial_ordering(weak_order(E, F))` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `partial_ordering(E <=> F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `partial_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::partial_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>
> 

> <ins>The name `compare_strong_order_fallback` denotes a comparison customization point ([customization.point.object]) object. The expression `compare_strong_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:</ins>
> 
> - <ins>If the decayed types of `E` and `F` differ, `compare_strong_order_fallback(E, F)` is ill-formed.</ins>
> - <ins>Otherwise, `strong_order(E, F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `3WAY<strong_ordering>(E, F)` ([class.spaceship]) if it is a well-formed expression.</ins>
> - <ins>Otherwise, `compare_strong_order_fallback(E, F)` is ill-formed.</ins>

> <ins>The name `compare_weak_order_fallback` denotes a customization point object ([customization.point.object]). The expression `compare_weak_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:</ins>
> 
> - <ins>If the decayed types of `E` and `F` differ, `compare_weak_order_fallback(E, F)` is ill-formed.</ins>
> - <ins>Otherwise, `weak_order(E, F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `3WAY<weak_ordering>(E, F)` ([class.spaceship]) if it is a well-formed expression.</ins>
> - <ins>Otherwise, `compare_weak_order_fallback(E, F)` is ill-formed.</ins>

> <ins>The name `compare_partial_order_fallback` denotes a customization point object ([customization.point.object]). The expression `compare_partial_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:</ins>
> 
> - <ins>If the decayed types of `E` and `F` differ, `compare_partial_order_fallback(E, F)` is ill-formed.</ins>
> - <ins>Otherwise, `partial_order(E, F)` if it is a well-formed expression.</ins>
> - <ins>Otherwise, `3WAY<partial_ordering>(E, F)` ([class.spaceship]) if it is a well-formed expression.</ins>
> - <ins>Otherwise, `compare_partial_order_fallback(E, F)` is ill-formed.</ins>

## Clause 17: Concepts Library

Nothing.

## Clause 18: Diagnostics Library

Changed operators for: `error_category`, `error_code`, and `error_condition`

Change 18.5.1 [system_error.syn]

<blockquote><pre><code>namespace std {
  [...]
  // [syserr.errcondition.nonmembers], non-member functions
  error_condition make_error_condition(errc e) noexcept;

<del>  // [syserr.compare], comparison functions
  bool operator==(const error_code& lhs, const error_code& rhs) noexcept;
  bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;
  bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;
  bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;
  bool operator!=(const error_code& lhs, const error_code& rhs) noexcept;
  bool operator!=(const error_code& lhs, const error_condition& rhs) noexcept;
  bool operator!=(const error_condition& lhs, const error_code& rhs) noexcept;
  bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept;
  bool operator&lt; (const error_code& lhs, const error_code& rhs) noexcept;
  bool operator&lt; (const error_condition& lhs, const error_condition& rhs) noexcept;</del>

  // [syserr.hash], hash support
  [...]
}</code></pre></blockquote>

Change 18.5.2.1 [syserr.errcat.overview]:

<blockquote><pre><code>namespace std {
  class error_category {
    [...]
    bool operator==(const error_category& rhs) const noexcept;
    <del>bool operator!=(const error_category& rhs) const noexcept;</del>
    <del>bool operator< (const error_category& rhs) const noexcept;</del>
    <ins>strong_ordering operator<=>(const error_category& rhs) const noexcept;</ins>
  };
  [...]
}</code></pre></blockquote>

Change 18.5.2.3 [syserr.errcat.nonvirtuals]:

> <pre><code>bool operator==(const error_category& rhs) const noexcept;</code></pre>
> *Returns*: `this == &rhs`.
<pre><code><del>bool operator!=(const error_category& rhs) const noexcept;</del></code></pre>
> <del>*Returns*: `!(*this == rhs)`.</del>
<pre><code><del>bool operator<(const error_category& rhs) const noexcept;</del></code></pre>
> <del>*Returns*: `less<const error_category*>()(this, &rhs)`.</del>  
> <del>[Note: `less` (19.14.7) provides a total ordering for pointers. —end note]</del>
> <pre><code><ins>strong_ordering operator<=>(const error_category& rhs) const noexcept;</ins></code></pre>
> <ins>*Returns*: `compare_three_way()(this, &rhs)`.</ins>  
> <ins>[Note: `compare_three_way` (cmp.object) provides a total ordering for pointers. —end note]</ins>

Change 18.5.3.1 [syserr.errcode.overview]:

<blockquote><pre><code>namespace std {
  class error_code {
    [...]
    <ins>// [syserr.compare], comparison functions</ins>
    <ins>friend bool operator==(const error_code&, const error_code&) { <i>see below</i>; }</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const error_code&, const error_code&) { <i>see below</i>; }</ins>
    <ins>friend bool operator==(const error_code&, const error_condition&) { <i>see below</i>; }</ins>
  private:
    int val_;                   // exposition only
    const error_category* cat_; // exposition only
  };

  [...]
}</code></pre></blockquote>  

Change 18.5.4.1 [syserr.errcondition.overview]:

<blockquote><pre><code>namespace std {
  class error_condition {
  public:
    [...]
    <ins>// [syserr.compare], comparison functions</ins>
    <ins>friend bool operator==(const error_condition&, const error_condition&) { <i>see below</i>; }</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const error_condition&, const error_condition&) { <i>see below</i>; }</ins>
    <ins>friend bool operator==(const error_condition&, const error_code&) { <i>see below</i>; }</ins>
  private:
    int val_;                   // exposition only
    const error_category* cat_; // exposition only
  };
}</code></pre></blockquote>

Change 18.5.5 [syserr.compare]

> <pre><code><ins>friend </ins>bool operator==(const error_code& lhs, const error_code& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>friend </ins>bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category().equivalent(lhs.value(), rhs) || rhs.category().equivalent(lhs, rhs.value())`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>friend </ins>bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;</code></pre>
> *Returns*: `rhs.category().equivalent(rhs.value(), lhs) || lhs.category().equivalent(rhs, lhs.value())`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>friend </ins>bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>bool operator!=(const error_code& lhs, const error_code& rhs) noexcept;</del>
<del>bool operator!=(const error_code& lhs, const error_condition& rhs) noexcept;</del>
<del>bool operator!=(const error_condition& lhs, const error_code& rhs) noexcept;</del>
<del>bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept;</del></code></pre>
> <del>*Returns*: `!(lhs == rhs)`.</del>
> <code><pre><del>bool operator<(const error_code& lhs, const error_code& rhs) noexcept;</del></code></pre>
> <del>*Returns*:
> ```lhs.category() < rhs.category() ||
(lhs.category() == rhs.category() && lhs.value() < rhs.value())```</del>
> <code><pre><del>bool operator<(const error_condition& lhs, const error_condition& rhs) noexcept;</del></code></pre>
> <del>*Returns*:
> ```lhs.category() < rhs.category() ||
(lhs.category() == rhs.category() && lhs.value() < rhs.value())```</del>
> <pre><code><ins>friend strong_ordering operator<=>(const error_code& lhs, const error_code& rhs) noexcept;</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();</code></pre></blockquote>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>friend strong_ordering operator<=>(const error_condition& lhs, const error_condition& rhs) noexcept;</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();</code></pre></blockquote>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>

## Clause 19: General utilities library

Changed operators for: `pair`, `tuple`, `optional`, `variant`, `monostate`, `bitset`, `allocator`, `unique_ptr`, `shared_ptr`, `memory_resource`, `polymorphic_allocator`, `scoped_allocator_adaptor`, `function`, `type_index`

Change 19.2.1 [utility.syn]

<blockquote><pre><code>#include <initializer_list> // see 16.10.1

namespace std {
  [...]
  // 19.4, class template pair
  template&lt;class T1, class T2&gt;
  struct pair;
  
  <del>// 19.4.3, pair specialized algorithms</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator==(const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator!=(const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator&lt; (const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator&gt; (const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator&lt;=(const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>constexpr bool operator&gt;=(const pair&lt;T1, T2&gt;&, const pair&lt;T1, T2&gt;&);</del>
  
  [...]
}</code></pre></blockquote>

Change 19.4.2 [pairs.pair]:

<blockquote><pre><code>namespace std {
template&lt;class T1, class T2&gt;
struct pair {
  [...]
  constexpr void swap(pair& p) noexcept(<i>see below</i>);
  
  <ins>friend constexpr bool operator==(const pair&, const pair&) = default;</ins>
  <ins>friend constexpr common_comparison_category_t&lt;<i>synth-3way-result</i>&lt;T1&gt;, <i>synth-3way-result</i>&lt;T2&gt;&gt;</ins>
  <ins>  operator<=>(const pair&, const pair&)</ins>
  <ins>  { <i>see below</i> }</ins>
};</code></pre>
</blockquote>
> [...]
> <pre><code>constexpr void swap(pair& p) noexcept(<i>see below</i>);</code></pre>
> *Requires*: `first` shall be swappable with (15.5.3.2) `p.first` and `second` shall be swappable with `p.second`.  
> *Effects*: Swaps `first` with `p.first` and `second` with `p.second`.  
> *Remarks*: The expression inside noexcept is equivalent to:
`is_nothrow_swappable_v<first_type> && is_nothrow_swappable_v<second_type>`
> <pre><code><ins>friend constexpr common_comparison_category_t&lt;<i>synth-3way-result</i>&lt;T1&gt;, <i>synth-3way-result</i>&lt;T2&gt;&gt;</ins>
<ins>  operator<=>(const pair& lhs, const pair& rhs);</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = <i>synth-3way</i>(lhs.first, rhs.first); c != 0) return c;
return <i>synth-3way</i>(lhs.second, rhs.second);</code></pre></blockquote>
> <ins>*Remarks*:  This function is to be found via argument-dependent lookup only.</ins>

Change 19.4.3 [pairs.spec].

> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator==(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `x.first == y.first && x.second == y.second`.</del>
> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator!=(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `!(x == y)`.</del>
> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator&lt;(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `x.first < y.first || (!(y.first < x.first) && x.second < y.second)`.</del>
> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator&gt;(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `y < x`</del>.
> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator&lt;=(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `!(y < x)`.</del>
> <pre><code><del>template&lt;class T1, class T2&gt;
constexpr bool operator&gt;=(const pair&lt;T1, T2&gt;& x, const pair&lt;T1, T2&gt;& y);</del></code></pre>
> <del>*Returns*: `!(x < y)`.</del>
<pre><code>template&lt;class T1, class T2&gt;
constexpr void swap(pair&lt;T1, T2&gt;& x, pair&lt;T1, T2&gt;& y) noexcept(noexcept(x.swap(y)));</code></pre>
> [...]

Change 19.5.2 [tuple.syn]:

<blockquote><pre><code>namespace std {
  // 19.5.3, class template tuple
  template&lt;class... Types&gt;
    class tuple;      
    
  [...]  
  
  template&lt;class T, class... Types&gt;
    constexpr const T&& get(const tuple&lt;Types...&gt;&& t) noexcept;
    
  <del>// 19.5.3.8, relational operators</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator==(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator!=(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator&lt;(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator&gt;(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator&lt;=(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
  <del>template&lt;class... TTypes, class... UTypes&gt;</del>
    <del>constexpr bool operator&gt;=(const tuple&lt;TTypes...&gt;&, const tuple&lt;UTypes...&gt;&);</del>
    
  // 19.5.3.9, allocator-related traits
  template&lt;class... Types, class Alloc&gt;
    struct uses_allocator&lt;tuple&lt;Types...&gt;, Alloc&gt;;  
    
  [...]
}</code></pre></blockquote>
  
Change 19.5.3 [tuple.tuple]:

<blockquote><pre><code>namespace std {
template<class... Types>
class tuple {
public:
  [...]
  
  // 19.5.3.3, tuple swap
  constexpr void swap(tuple&) noexcept(see below );
  
  <ins>// 19.5.3.8, tuple relational operators</ins>
  <ins>template&lt;class... UTypes&gt;</ins>
  <ins>  friend constexpr bool operator==(const tuple&, const tuple&lt;UTypes...&gt;&)</ins>  
  <ins>  { <i>see below</i> }</ins>
  <ins>template&lt;class... UTypes&gt;</ins>
  <ins>  friend constexpr common_comparison_category_t&lt;<i>synth-3way-result</i>&lt;Types, UTypes&gt;...&gt;</ins>
  <ins>    operator<=>(const tuple&, const tuple&lt;UTypes...&gt;&)</ins>
  <ins>  { <i>see below</i> }</ins>
};</code></pre></blockquote>

Change 19.5.3.8 [tuple.rel]:

> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;
  constexpr bool operator==(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <pre><code><ins>template&lt;class... UTypes&gt;</ins>
<ins>  friend constexpr bool operator==(const tuple&, const tuple&lt;UTypes...&gt;&)</ins></code></pre>
> *Requires*: For all `i`, where `0 <= i` and <code>i &lt; sizeof...(<del>TTypes</del> <ins>Types</ins>)</code>, `get<i>(t) == get<i>(u)` is a well-formed expression returning a type that is convertible to `bool`. <code>sizeof...(<del>TTypes</del> <ins>Types</ins>) == sizeof...(UTypes)</code>.  
> *Returns*: `true` if `get<i>(t) == get<i>(u)` for all `i`, otherwise `false`. For any two zero-length tuples `e` and `f`, `e == f` returns `true`.  
> *Effects*: The elementary comparisons are performed in order from the zeroth index upwards. No comparisons or element accesses are performed after the first equality comparison that evaluates to `false`.  
> <ins>*Remarks*:  This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;<del>
> <del>  constexpr bool operator!=(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <del>*Returns*: `!(t == u)`.</del>
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;</del>
<del>constexpr bool operator&lt;(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <pre><code><ins>template&lt;class... UTypes&gt;</ins>
<ins>  friend constexpr common_comparison_category_t&lt;<i>synth-3way-result</i>&lt;Types, UTypes&gt;...&gt;</ins>
<ins>    operator<=>(const tuple& t, const tuple&lt;UTypes...&gt;& u);</ins></code></pre>
> *Requires*: For all `i`, where `0 <= i` and `i < sizeof...(Types)`, <del>both `get<i>(t) < get<i>(u)` and `get<i>(u) < get<i>(t)` are well-formed expressions returning types that are convertible to `bool`</del> <ins><code><i>synth-3way</i>(get&lt;i&gt;(t), get&lt;i&gt;(u))</code></ins> is a well-formed expression. <code>sizeof...(<del>TTypes</del> <ins>Types</ins>) == sizeof...(UTypes)</code>.  
> <del>*Returns*: The result of a lexicographical comparison between `t` and `u`. The result is defined as:
`(bool)(get<0>(t) < get<0>(u)) || (!(bool)(get<0>(u) < get<0>(t)) && ttail < utail)`, where
<code>r<sub>tail</sub></code> for some tuple `r` is a tuple containing all but the first element of `r`. For any two zero-length tuples `e` and `f`, `e < f` returns `false`.</del>  
> <ins>*Effects*: Performs a lexicographical comparison between `t` and `u`. For any two zero-length tuples `t` and `u`, `t <=> u` returns `strong_ordering::equal`. Otherwise, equivalent to:</ins>
> <blockquote class="ins"><pre><code>auto c = <i>synth-3way</i>(get<0>(t), get<0>(u));
return (c != 0) ? c : (t<sub>tail</sub> <=> u<sub>tail</sub>);</code></pre></blockquote>
> <ins>where <code>r<sub>tail</sub></code> for some tuple `r` is a tuple containing all but the first element of `r`.</ins>  
> <ins>*Remarks*:  This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;
constexpr bool operator>(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <del>*Returns*: `u < t`</del>.
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;
constexpr bool operator<=(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <del>*Returns*: `!(u < t)`</del>
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;
constexpr bool operator>=(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <del>*Returns*: `!(t < u)`</del>  
> *[Note:* The above definitions for comparison functions do not require <code>t<sub>tail</sub></code> (or <code>u<sub>tail</sub></code>) to be constructed. It may not even be possible, as `t` and `u` are not required to be copy constructible. Also, all comparison functions are short circuited; they do not perform element accesses beyond what is required to determine the result of the
comparison. *—end note]*

Change 19.6.2 [optional.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [optional.bad.access], class bad_optional_access
  class bad_optional_access;

<del>  // [optional.relops], relational operators
  template&lt;class T, class U&gt;
    constexpr bool operator==(const optional&lt;T&gt;&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt;
    constexpr bool operator!=(const optional&lt;T&gt;&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt;
    constexpr bool operator&lt;(const optional&lt;T&gt;&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt;
    constexpr bool operator&gt;(const optional&lt;T&gt;&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt;
    constexpr bool operator&lt;=(const optional&lt;T&gt;&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt;
    constexpr bool operator&gt;=(const optional&lt;T&gt;&, const optional&lt;U&gt;&);

  // [optional.nullops], comparison with nullopt
  template&lt;class T&gt; constexpr bool operator==(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator==(nullopt_t, const optional&lt;T&gt;&) noexcept;
  template&lt;class T&gt; constexpr bool operator!=(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator!=(nullopt_t, const optional&lt;T&gt;&) noexcept;
  template&lt;class T&gt; constexpr bool operator&lt;(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator&lt;(nullopt_t, const optional&lt;T&gt;&) noexcept;
  template&lt;class T&gt; constexpr bool operator&gt;(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator&gt;(nullopt_t, const optional&lt;T&gt;&) noexcept;
  template&lt;class T&gt; constexpr bool operator&lt;=(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator&lt;=(nullopt_t, const optional&lt;T&gt;&) noexcept;
  template&lt;class T&gt; constexpr bool operator&gt;=(const optional&lt;T&gt;&, nullopt_t) noexcept;
  template&lt;class T&gt; constexpr bool operator&gt;=(nullopt_t, const optional&lt;T&gt;&) noexcept;

  // [optional.comp_with_t], comparison with T
  template&lt;class T, class U&gt; constexpr bool operator==(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator==(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator!=(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator!=(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;=(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;=(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;=(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;=(const T&, const optional&lt;U&gt;&);</del>

  // [optional.specalg], specialized algorithms
  template&lt;class T&gt;
    void swap(optional&lt;T&gt;&, optional&lt;T&gt;&) noexcept(see below);
  [...]
}</code></pre></blockquote>  

Change 19.6.3 [optional.optional]:

<blockquote><pre><code>namespace std {
  template&lt;class T&gt;
  class optional {
  public:
    [...]
    
    // [optional.mod], modifiers
    void reset() noexcept;

    <ins>// [optional.relops], relational operators</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator==(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator!=(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator&lt;(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator&gt;(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator&lt;=(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend constexpr bool operator&gt;=(const optional&, const optional&lt;U&gt;&) { <i>see below</i> }</ins>
    <ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
    <ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
    <ins>    operator&lt;=&gt;(const optional&, const optional&lt;U&gt;&)</ins>
    <ins>    { <i>see below</i> }</ins>

    <ins>// comparison with nullopt</ins>
    <ins>friend constexpr bool operator==(const optional& x, nullopt_t) { return !x; }</ins>
    <ins>friend constexpr strong_ordering operator&lt;=&gt;(const optional& x, nullopt_t) { return bool(x) &lt;=&gt; false; }</ins>
    
    <ins>// [optional.comp_with_t], comparison with T</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator==(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator==(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator!=(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator!=(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&lt;(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&lt;(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&gt;(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&gt;(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&lt;=(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&lt;=(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&gt;=(const optional&, const U&) { <i>see below</i> }</ins>
    <ins>template&lt;class U&gt; friend constexpr bool operator&gt;=(const U&, const optional&) { <i>see below</i> }</ins>
    <ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
    <ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
    <ins>    operator&lt;=&gt;(const optional&, const U&)</ins>
    <ins>    { <i>see below</i> }</ins>
    
  private:
    T *val;         // exposition only
  };
}</code></pre></blockquote>

Change 19.6.6 [optional.relops]:

> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator==(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x == *y` shall be well-formed and its result shall be convertible to `bool`. [*Note*: `T` need not be `Cpp17EqualityComparable`. —*end note*]  
> *Returns*: If `bool(x) != bool(y)`, `false`; otherwise if `bool(x) == false`, `true`; otherwise `*x == *y`.  
> *Remarks*: Specializations of this function template for which `*x == *y` is a core constant expression shall be constexpr functions. <ins>This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator!=(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x != *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `bool(x) != bool(y)`, `true`; otherwise, if `bool(x) == false`, `false`; otherwise `*x != *y`.  
> *Remarks*: Specializations of this function template for which `*x != *y` is a core constant expression shall be constexpr functions. <ins>This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `*x < *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `!y`, `false`; otherwise, if `!x`, `true`; otherwise `*x < *y`.  
> *Remarks*: Specializations of this function template for which `*x < *y` is a core constant expression shall be constexpr functions. <ins>This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&gt;(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x > *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `!x`, `false`; otherwise, if `!y`, `true`; otherwise `*x > *y`.
> *Remarks*: Specializations of this function template for which `*x > *y` is a core constant expression shall be constexpr functions. <ins>This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;=(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x <= *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `!x`, `true`; otherwise, if `!y`, `false`; otherwise `*x <= *y`.  
> *Remarks*: Specializations of this function template for which `*x <= *y` is a core constant expression shall be constexpr functions. <ins>This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T</del>, class U&gt; <ins>friend </ins>constexpr bool operator>=(const optional<del>&lt;T&gt;</del>& x, const optional&lt;U&gt;& y);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x >= *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `!y`, `true`; otherwise, if `!x`, `false`; otherwise `*x >= *y`.  
> *Remarks*: Specializations of this function template for which `*x >= *y` is a core constant expression shall be constexpr functions. This function is to be found via argument-dependent lookup only.<ins>
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
<ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
<ins>    operator&lt;=&gt;(const optional& x, const optional&lt;U&gt;& y);</ins></code></pre>
> <ins>*Returns*: If `x && y`, `*x <=> *y`; otherwise `bool(x) <=> bool(y)`.</ins>  
> <ins>*Remarks*: Specializations of this function template for which `*x <=> *y` is a core constant expression shall be `constexpr` functions. This function is to be found via argument-dependent lookup only.</ins>
 
Remove 19.6.7 [optional.nullops] (it is now fully expressed by the two hidden friends defined in the header):

> <pre><code><del>template&lt;class T&gt; constexpr bool operator==(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del>
<del>template&lt;class T&gt; constexpr bool operator==(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `!x`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator!=(const optional&lt;T&gt;& x, nullopt_t) noexcept;
template&lt;class T&gt; constexpr bool operator!=(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `bool(x)`.</del>
> [...]

Change 19.6.8 [optional.comp_with_t]:

> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator==(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x == v` shall be well-formed and its result shall be convertible to `bool`. [*Note*: `T` need not be `Cpp17EqualityComparable`. —*end note*]  
> *Effects*: Equivalent to: `return bool(x) ? *x == v : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator==(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v == *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v == *x : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator!=(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x != v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x != v : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator!=(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v != *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v != *x : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x < v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x < v : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v < *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v < *x : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&gt;(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x > v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x > v : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&gt;(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v > *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v > *x : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;=(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x <= v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x <= v : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&lt;=(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v <= *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v <= *x : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&gt;=(const optional<del>&lt;T&gt;</del>& x, const U& v);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `*x >= v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x >= v : false;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T, </del>class U&gt; <ins>friend </ins>constexpr bool operator&gt;=(const <del>T</del><ins>U</ins>& v, const optional<del>&lt;U&gt;</del>& x);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: The expression `v >= *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v >= *x : true;`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
<ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
<ins>    operator&lt;=&gt;(const optional& x, const U& v);</ins></code></pre>
> <ins>*Effects*: Equivalent to: `return bool(x) ? *x <=> v : strong_ordering::less;`</ins>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>


Change 19.7.2 [variant.syn]:

<blockquote><pre><code>namespace std {
  [...]
  template&lt;class T, class... Types&gt;
    constexpr add_pointer_t&lt;T&gt;
      get_if(variant&lt;Types...&gt;*) noexcept;
  template&lt;class T, class... Types&gt;
    constexpr add_pointer_t&lt;const T&gt;
      get_if(const variant&lt;Types...&gt;*) noexcept;

  // [variant.relops], relational operators
<del>  template&lt;class... Types&gt;
    constexpr bool operator==(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);
  template&lt;class... Types&gt;
    constexpr bool operator!=(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);
  template&lt;class... Types&gt;
    constexpr bool operator&lt;(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);
  template&lt;class... Types&gt;
    constexpr bool operator&gt;(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);
  template&lt;class... Types&gt;
    constexpr bool operator&lt;=(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);
  template&lt;class... Types&gt;
    constexpr bool operator&gt;=(const variant&lt;Types...&gt;&, const variant&lt;Types...&gt;&);</del>

  // [variant.visit], visitation
  [...]  
  
  // [variant.monostate], class monostate
  struct monostate;

<del>  // [variant.monostate.relops], monostate relational operators
  constexpr bool operator==(monostate, monostate) noexcept;
  constexpr bool operator!=(monostate, monostate) noexcept;
  constexpr bool operator&lt;(monostate, monostate) noexcept;
  constexpr bool operator&gt;(monostate, monostate) noexcept;
  constexpr bool operator&lt;=(monostate, monostate) noexcept;
  constexpr bool operator&gt;=(monostate, monostate) noexcept;</del>
  
  // [variant.specalg], specialized algorithms
  template&lt;class... Types&gt;
    void swap(variant&lt;Types...&gt;&, variant&lt;Types...&gt;&) noexcept(<i>see below</i>);
  [...]
}</code></pre></blockquote>  

Change 19.7.3 [variant.variant]:

<blockquote><pre><code>namespace std {
  template&lt;class... Types&gt;
  class variant {
  public:
    [...]
    
    // [variant.swap], swap
    void swap(variant&) noexcept(see below);

    <ins>// [variant.relops], relational operators</ins>
    <ins>friend constexpr bool operator==(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr bool operator!=(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr bool operator&lt;(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr bool operator&gt;(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr bool operator&lt;=(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr bool operator&gt;=(const variant&, const variant&) { <i>see below</i> }</ins>
    <ins>friend constexpr common_comparison_category_t&lt;compare_three_way_result_t&lt;Types&gt;...&gt;</ins>
    <ins>  operator&lt;=&gt;(const variant&, const variant&)</ins>
    <ins>    requires (ThreeWayComparable&lt;Types&gt; && ...)</ins>
    <ins>    { <i>see below</i> }</ins>
  };
}</code></pre></blockquote>

Change 19.7.6 [variant.relops]:

> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator==(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) == get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.  
> *Returns*: If `v.index() != w.index()`, `false`; otherwise if `v.valueless_by_exception()`, `true`; otherwise `get<i>(v) == get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator!=(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) != get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.
> *Returns*: If `v.index() != w.index()`, `true`; otherwise if `v.valueless_by_exception()`, `false`; otherwise `get<i>(v) != get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator&lt;(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) < get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.  
> *Returns*: If `w.valueless_by_exception()`, `false`; otherwise if `v.valueless_by_exception()`, `true`; otherwise, if `v.index() < w.index()`, `true`; otherwise if `v.index() > w.index()`, `false`; otherwise `get<i>(v) < get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator&gt;(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) > get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.  
> *Returns*: If `v.valueless_by_exception()`, `false`; otherwise if `w.valueless_by_exception()`, `true`; otherwise, if `v.index() > w.index()`, `true`; otherwise if `v.index() < w.index()`, `false`; otherwise `get<i>(v) > get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator&lt;=(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) <= get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.  
> *Returns*: If `v.valueless_by_exception()`, `true`; otherwise if `w.valueless_by_exception()`, `false`; otherwise, if `v.index() < w.index()`, `true`; otherwise if `v.index() > w.index()`, `false`; otherwise `get<i>(v) <= get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class... Types&gt;</del>
<ins>friend </ins>constexpr bool operator&gt;=(const variant<del>&lt;Types...&gt;</del>& v, const variant<del>&lt;Types...&gt;</del>& w);</code></pre>
> <del>*Requires*</del><ins>*Mandates*</ins>: `get<i>(v) >= get<i>(w)` is a valid expression returning a type that is convertible to `bool`, for all `i`.  
> *Returns*: If `w.valueless_by_exception()`, `true`; otherwise if `v.valueless_by_exception()`, `false`; otherwise, if `v.index() > w.index()`, `true`; otherwise if `v.index() < w.index()`, `false`; otherwise `get<i>(v) >= get<i>(w)` with `i` being `v.index()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>constexpr common_comparison_category_t&lt;compare_three_way_result_t&lt;Types&gt;...&gt;</ins>
<ins>  friend operator&lt;=&gt;(const variant& v, const variant& w)</ins>
<ins>    requires (ThreeWayComparable&lt;Types&gt; && ...);</ins></code></pre>
> <ins>*Returns*: Let `c` be `(v.index() + 1) <=> (w.index() + 1)`. If `c != 0`, `c`. Otherwise, `get<i>(v) <=> get<i>(w)` with `i` being `v.index()`.</ins>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>

Change 19.7.8 [variant.monostate]:

<blockquote><pre><code><del>struct monostate{};</del>
<ins>struct monostate {
  friend constexpr bool operator==(monostate, monostate) noexcept = default;
  friend constexpr strong_ordering operator<=>(monostate, monostate) noexcept = default;
};</ins></code></pre>

<ins>[<i>Note</i>: monostate objects have only a single state; they thus always compare equal. —<i>end note</i>]</ins></blockquote>

Remove 19.7.9 [variant.monostate.relops]:

<blockquote><pre><code><del>constexpr bool operator==(monostate, monostate) noexcept { return true; }</del>
<del>constexpr bool operator!=(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator<(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator>(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator<=(monostate, monostate) noexcept { return true; }</del>
<del>constexpr bool operator>=(monostate, monostate) noexcept { return true; }</del></code></pre>

<del>[<i>Note</i>: monostate objects have only a single state; they thus always compare equal. —<i>end note</i>]</del></blockquote>

Change 19.9.2 [template.bitset]:

<blockquote><pre><code>namespace std {
  template&lt;size_t N&gt; class bitset {
  public:
    [...]
    constexpr size_t size() const noexcept;
    bool operator==(const bitset&lt;N&gt;& rhs) const noexcept;
    <del>bool operator!=(const bitset&lt;N&gt;& rhs) const noexcept;</del>
    bool test(size_t pos) const;
    [...]
  };
}</code></pre></blockquote>

Change 19.9.2.2 [bitset.members]:

> <pre><code>bool operator==(const bitset&lt;N&gt;& rhs) const noexcept;</code></pre>
> *Returns*: `true` if the value of each bit in `*this` equals the value of the corresponding bit in `rhs`.
> <del><pre><code>bool operator!=(const bitset&lt;N&gt;& rhs) const noexcept;</code></pre></del>
> <del>*Returns*: `true` if `!(*this == rhs)`.</del></blockquote>

Change 19.10.2 [memory.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [default.allocator], the default allocator
  template&lt;class T&gt; class allocator;
<del>  template&lt;class T, class U&gt;
    bool operator==(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;
  template&lt;class T, class U&gt;
    bool operator!=(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</del>
  [...]
  template&lt;class T, class D&gt; 
    void swap(unique_ptr&lt;T, D&gt;& x, unique_ptr&lt;T, D&gt;& y) noexcept;

<del>  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator==(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);
  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator!=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);
  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator&lt;(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);
  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator&gt;(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);
  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator&lt;=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);
  template&lt;class T1, class D1, class T2, class D2&gt;
    bool operator&gt;=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);

  template&lt;class T, class D&gt;
    bool operator==(const unique_ptr&lt;T, D&gt;& x, nullptr_t) noexcept;
  template&lt;class T, class D&gt;
    bool operator==(nullptr_t, const unique_ptr&lt;T, D&gt;& y) noexcept;
  template&lt;class T, class D&gt;
    bool operator!=(const unique_ptr&lt;T, D&gt;& x, nullptr_t) noexcept;
  template&lt;class T, class D&gt;
    bool operator!=(nullptr_t, const unique_ptr&lt;T, D&gt;& y) noexcept;
  template&lt;class T, class D&gt;
    bool operator&lt;(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
  template&lt;class T, class D&gt;
    bool operator&lt;(nullptr_t, const unique_ptr&lt;T, D&gt;& y);
  template&lt;class T, class D&gt;
    bool operator&gt;(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
  template&lt;class T, class D&gt;
    bool operator&gt;(nullptr_t, const unique_ptr&lt;T, D&gt;& y);
  template&lt;class T, class D&gt;
    bool operator&lt;=(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
  template&lt;class T, class D&gt;
    bool operator&lt;=(nullptr_t, const unique_ptr&lt;T, D&gt;& y);
  template&lt;class T, class D&gt;
    bool operator&gt;=(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
  template&lt;class T, class D&gt;
    bool operator&gt;=(nullptr_t, const unique_ptr&lt;T, D&gt;& y);</del>

  template&lt;class E, class T, class Y, class D&gt;
    basic_ostream&lt;E, T&gt;& operator&lt;&lt;(basic_ostream&lt;E, T&gt;& os, const unique_ptr&lt;Y, D&gt;& p);  
  [...]
  <del>// [util.smartptr.shared.cmp], shared_ptr comparisons</del>
<del>  template&lt;class T, class U&gt;
    bool operator==(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;
  template&lt;class T, class U&gt;
    bool operator!=(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;
  template&lt;class T, class U&gt;
    bool operator&lt;(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;
  template&lt;class T, class U&gt;
    bool operator&gt;(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;
  template&lt;class T, class U&gt;
    bool operator&lt;=(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;
  template&lt;class T, class U&gt;
    bool operator&gt;=(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;

  template&lt;class T&gt;
    bool operator==(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator==(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;
  template&lt;class T&gt;
    bool operator!=(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator!=(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;
  template&lt;class T&gt;
    bool operator&lt;(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator&lt;(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;
  template&lt;class T&gt;
    bool operator&gt;(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator&gt;(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;
  template&lt;class T&gt;
    bool operator&lt;=(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator&lt;=(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;
  template&lt;class T&gt;
    bool operator&gt;=(const shared_ptr&lt;T&gt;& x, nullptr_t) noexcept;
  template&lt;class T&gt;
    bool operator&gt;=(nullptr_t, const shared_ptr&lt;T&gt;& y) noexcept;</del>

  // [util.smartptr.shared.spec], shared_ptr specialized algorithms
  template&lt;class T&gt;
    void swap(shared_ptr&lt;T&gt;& a, shared_ptr&lt;T&gt;& b) noexcept;
  [...]    
}</code></pre></blockquote>

Change 19.10.10 [default.allocator]:

<blockquote><pre><code>namespace std {
  template&lt;class T&gt; class allocator {
   public:
    using value_type      = T;
    using size_type       = size_t;
    using difference_type = ptrdiff_t;
    using propagate_on_container_move_assignment = true_type;
    using is_always_equal = true_type;

    constexpr allocator() noexcept;
    constexpr allocator(const allocator&) noexcept;
    template&lt;class U&gt; constexpr allocator(const allocator&lt;U&gt;&) noexcept;
    ~allocator();
    allocator& operator=(const allocator&) = default;

    [[nodiscard]] T* allocate(size_t n);
    void deallocate(T* p, size_t n);
    
    <ins>template&lt;class U&gt;</ins>
    <ins>  friend bool operator==(const allocator&, const allocator&lt;U&gt;&) { return true; }</ins>
  };
}</code></pre></blockquote>

Remove 19.10.10.2 [allocator.globals]:

> <pre><code><del>template&lt;class T, class U&gt;
  bool operator==(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</del></code></pre>
> <del>*Returns*: `true`.</del>
> <del><pre><code>template&lt;class T, class U&gt;
  bool operator!=(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</code></pre></del>
> <del>*Returns*: `false`.</del>

Change 19.11.1.2 [unique.ptr.single]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class D = default_delete&lt;T&gt;&gt; class unique_ptr {
  public:
    using pointer      = <i>see below</i>;
    using element_type = T;
    using deleter_type = D;
    [...]
    // disable copy from lvalue
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
<ins>    // [unique.ptr.special] Specialized algorithms
    template&lt;class T2, class D2&gt;
      friend bool operator==(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { return x.get() == y.get(); }
    template&lt;class T2, class D2&gt;
      friend bool operator&lt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&lt;=(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&gt;=(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
        requires ThreeWayComparableWith&lt;pointer, typename unique_ptr&lt;T2, D2&gt;::pointer&gt;
      friend auto operator&lt;=&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y)
      { return compare_three_way()(x.get(), y.get()); }

    friend bool operator==(const unique_ptr& x, nullptr_t) noexcept { return !x; }
    friend auto operator&lt;=&gt;(const unique_ptr& x, nullptr_t)
      requires ThreeWayComparableWith&lt;pointer, nullptr_t&gt;
      { return compare_three_way()(x.get(), nullptr); }</ins>
  };
}</code></pre></blockquote>

Change 19.11.1.3 [unique.ptr.runtime]:

<blockquote><pre><code>namespace std {
  template<class T, class D> class unique_ptr<T[], D> {
  public:
    [...]
    // disable copy from lvalue
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;    
  
<ins>    // [unique.ptr.special] Specialized algorithms
    template&lt;class T2, class D2&gt;
      friend bool operator==(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { return x.get() == y.get(); }
    template&lt;class T2, class D2&gt;
      friend bool operator&lt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&lt;=(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
      friend bool operator&gt;=(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y) { <i>see below</i> }
    template&lt;class T2, class D2&gt;
        requires ThreeWayComparableWith&lt;pointer, typename unique_ptr&lt;T2, D2&gt;::pointer&gt;
      friend auto operator&lt;=&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y)
      { return compare_three_way()(x.get(), y.get()); }

    friend bool operator==(const unique_ptr& x, nullptr_t) noexcept { return !x; }
    friend auto operator&lt;=&gt;(const unique_ptr& x, nullptr_t)
      requires ThreeWayComparableWith&lt;pointer, nullptr_t&gt;
      { return compare_three_way()(x.get(), nullptr); }</ins>  
  };
}</code></pre></blockquote>

Change 19.11.1.5 [unique.ptr.special]:

> <pre><code>template&lt;class T, class D&gt; void swap(unique_ptr&lt;T, D&gt;& x, unique_ptr&lt;T, D&gt;& y) noexcept;</code></pre>
> *Remarks*: This function shall not participate in overload resolution unless `is_swappable_v<D>` is `true`.
> *Effects*: Calls `x.swap(y)`.
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;</del>
  <del>bool operator==(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `x.get() == y.get()`.</del>
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator!=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `x.get() != y.get()`.</del>
> <pre><code>template&lt;<del>class T1, class D1, </del>class T2, class D2&gt;
  <ins>friend </ins>bool operator&lt;(const unique_ptr<del>&lt;T1, D1&gt;</del>& x, const unique_ptr&lt;T2, D2&gt;& y);</code></pre>
> *Requires*: Let `CT` denote <code>common_type_t&lt;<del>typename unique_ptr&lt;T1, D1&gt;::</del>pointer, typename unique_ptr&lt;T2, D2&gt;::pointer&gt;</code> Then the specialization `less<CT>` shall be a function object type that induces a strict weak ordering on the pointer values.  
> *Returns*: `less<CT>()(x.get(), y.get())`.  
> <del>*Remarks*: If `unique_ptr<T1, D1>::pointer` is not implicitly convertible to `CT` or `unique_ptr<T2, D2>::pointer` is not implicitly convertible to `CT`, the program is ill-formed.</del>  
> <ins>*Mandates*: `pointer` and `unique_ptr<T2, D2>::pointer` are implicitly convertible to `CT`.</ins>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T1, class D1, </del>class T2, class D2&gt;
  <ins>friend </ins>bool operator&gt;(const unique_ptr<del>&lt;T1, D1&gt;</del>& x, const unique_ptr&lt;T2, D2&gt;& y);</code></pre>
> *Returns*: `y < x`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T1, class D1, </del>class T2, class D2&gt;
  <ins>friend </ins>bool operator&lt;=(const unique_ptr<del>&lt;T1, D1&gt;</del>& x, const unique_ptr&lt;T2, D2&gt;& y);</code></pre>
> *Returns*: `!(y < x)`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class T1, class D1, </del>class T2, class D2&gt;
  <ins>friend </ins>bool operator&gt;=(const unique_ptr<del>&lt;T1, D1&gt;</del>& x, const unique_ptr&lt;T2, D2&gt;& y);</code></pre>
> *Returns*: `!(x < y)`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>

Change 19.11.3, [util.smartptr.shared]:

<blockquote><pre><code>namespace std {
  template&lt;class T&gt; class shared_ptr {
    [...]
    // [util.smartptr.shared.obs], observers
    element_type* get() const noexcept;
    T& operator*() const noexcept;
    T* operator-&gt;() const noexcept;
    element_type& operator[](ptrdiff_t i) const;
    long use_count() const noexcept;
    explicit operator bool() const noexcept;
    template&lt;class U&gt;
      bool owner_before(const shared_ptr&lt;U&gt;& b) const noexcept;
    template&lt;class U&gt;
      bool owner_before(const weak_ptr&lt;U&gt;& b) const noexcept;
      
    <ins>// [util.smartptr.shared.cmp], shared_ptr comparisons</ins>
    <ins>template&lt;class U&gt;</ins>
      <ins>friend bool operator==(const shared_ptr& a, const shared_ptr&lt;U&gt;& b) noexcept</ins>
      <ins>{ return a.get() == b.get(); }</ins>
    <ins>template&lt;class U&gt;</ins>
      <ins>friend strong_ordering operator&lt;=&gt;(const shared_ptr& a, const shared_ptr&lt;U&gt;& b) noexcept</ins>
      <ins>{ return compare_three_way()(a.get(), b.get()); }</ins>
      
    <ins>friend bool operator==(const shared_ptr& a, nullptr_t) noexcept { return !a; }</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const shared_ptr& a, nullptr_t) noexcept</ins>
    <ins>  { return compare_three_way()(a.get(), nullptr); }</ins>
  };
}</code></pre></blockquote>

Remove all of 19.11.3.7 [util.smartptr.shared.cmp]:

> <pre><code><del>template&lt;class T, class U&gt;</del>
<del>  bool operator==(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `a.get() == b.get()`.</del>
> <pre><code><del>template&lt;class T, class U&gt;
  bool operator&lt;(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `less<>()(a.get(), b.get())`.</del>  
> [...]

Change 19.12.1 [mem.res.syn]:

<blockquote><pre><code>namespace std::pmr {
  // [mem.res.class], class memory_resource
  class memory_resource;

  <del>bool operator==(const memory_resource& a, const memory_resource& b) noexcept;</del>
  <del>bool operator!=(const memory_resource& a, const memory_resource& b) noexcept;</del>

  // [mem.poly.allocator.class], class template polymorphic_allocator
  template&lt;class Tp&gt; class polymorphic_allocator;

  <del>template&lt;class T1, class T2&gt;</del>
  <del>  bool operator==(const polymorphic_allocator&lt;T1&gt;& a,</del>
  <del>                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</del>
  <del>template&lt;class T1, class T2&gt;</del>
  <del>  bool operator!=(const polymorphic_allocator&lt;T1&gt;& a,</del>
  <del>                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</del>

  // [mem.res.global], global memory resources
  memory_resource* new_delete_resource() noexcept;
  [...]
}</code></pre></blockquote>

Change 19.12.2 [mem.res.class]:

<blockquote><pre><code>namespace std::pmr {
  class memory_resource {
    static constexpr size_t max_align = alignof(max_align_t);   // exposition only

  public:
    memory_resource() = default;
    memory_resource(const memory_resource&) = default;
    virtual ~memory_resource();

    memory_resource& operator=(const memory_resource&) = default;

    [[nodiscard]] void* allocate(size_t bytes, size_t alignment = max_align);
    void deallocate(void* p, size_t bytes, size_t alignment = max_align);

    bool is_equal(const memory_resource& other) const noexcept;
    
    <ins>friend bool operator==(const memory_resource&, const memory_resource&) noexcept { <i>see below</i> }</ins>

  private:
    virtual void* do_allocate(size_t bytes, size_t alignment) = 0;
    virtual void do_deallocate(void* p, size_t bytes, size_t alignment) = 0;

    virtual bool do_is_equal(const memory_resource& other) const noexcept = 0;
  };
}</code></pre></blockquote>

Change 19.12.2.3 [mem.res.eq]:

> <pre><code><ins>friend </ins>bool operator==(const memory_resource& a, const memory_resource& b) noexcept;</code></pre>
> *Returns*: `&a == &b || a.is_equal(b)`.
> <pre><code><del>bool operator!=(const memory_resource& a, const memory_resource& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.12.3 [mem.poly.allocator.class]:

<blockquote><pre><code>namespace std::pmr {
  template&lt;class Tp&gt; class polymorphic_allocator {
    memory_resource* memory_rsrc;     // exposition only

  public:
    [...]

    memory_resource* resource() const;
    
    <ins>template&lt;class T2&gt;</ins>
    <ins>  friend bool operator==(const polymorphic_allocator& a, const polymorphic_allocator&lt;T2&gt;& b)</ins>
    <ins>  { <i>see below</i> }</ins>
  };
}</code></pre></blockquote>

Change 19.12.3.3 [mem.poly.allocator.eq]:

> <pre><code>template&lt;<del>class T1, </del>class T2&gt;
  <ins>friend </ins>bool operator==(const polymorphic_allocator<del>&lt;T1&gt;</del>& a,
                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</code></pre>
> *Returns*: `*a.resource() == *b.resource()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class T1, class T2&gt;</del>
<del>  bool operator!=(const polymorphic_allocator&lt;T1&gt;& a,</del>
<del>                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.13.1 [allocator.adaptor.syn]:

<blockquote><pre><code>namespace std {
  // class template scoped allocator adaptor
  template&lt;class OuterAlloc, class... InnerAlloc&gt;
    class scoped_allocator_adaptor;

  <del>// [scoped.adaptor.operators], scoped allocator operators</del>
  <del>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;</del>
  <del>  bool operator==(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,</del>
  <del>                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</del>
  <del>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;</del>
  <del>  bool operator!=(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,</del>
  <del>                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</del>
}</code></pre></blockquote>

<blockquote><pre><code>namespace std {
  template&lt;class OuterAlloc, class... InnerAllocs&gt;
  class scoped_allocator_adaptor : public OuterAlloc {
    [...]
    scoped_allocator_adaptor select_on_container_copy_construction() const;
    
<ins>    // [scoped.adaptor.operators], scoped allocator operators
    template&lt;class Outer2&gt;
      friend bool operator==(const scoped_allocator_adaptor& a,
                             const scoped_allocator_adaptor&lt;Outer2, InnerAllocs...&gt;& b) noexcept
      { <i> see below</i> }</ins>
  };

  template&lt;class OuterAlloc, class... InnerAllocs&gt;
    scoped_allocator_adaptor(OuterAlloc, InnerAllocs...)
      -&gt; scoped_allocator_adaptor&lt;OuterAlloc, InnerAllocs...&gt;;
}</code></pre></blockquote>

Change 19.13.5 [scoped.adaptor.operators]:

> <pre><code>template&lt;<del>class OuterA1, </del>class OuterA2, class... InnerAllocs&gt;
  <ins>friend </ins>bool operator==(const scoped_allocator_adaptor<del>&lt;OuterA1, InnerAllocs...&gt;</del>& a,
                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</code></pre>
> *Returns*: If `sizeof...(InnerAllocs)` is zero,  
> `a.outer_allocator() == b.outer_allocator()`  
> otherwise  
> `a.outer_allocator() == b.outer_allocator() && a.inner_allocator() == b.inner_allocator()`  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;</del>
<del>  bool operator!=(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,</del>
<del>                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.14.1 [functional.syn]

<blockquote><pre><code>namespace std {
  [...]
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>

  // [func.search], searchers
  [...]
}</code></pre></blockquote>  

Change 19.14.8 [range.cmp]/2 to add `<=>`:

> There is an implementation-defined strict total ordering over all pointer values of a given type. This total ordering is consistent with the partial order imposed by the builtin operators `<`, `>`, `<=`, <del>and</del> `>=`<ins>, and `<=>`</ins>.


Change 19.14.16.2 [func.wrap.func]:

<blockquote><pre><code>namespace std {
  template&lt;class&gt; class function; // not defined

  template&lt;class R, class... ArgTypes&gt;
  class function&lt;R(ArgTypes...)&gt; {
  public:
    using result_type = R;
    [...]
    
    // [func.wrap.func.targ], function target access
    const type_info& target_type() const noexcept;
    template&lt;class T&gt;       T* target() noexcept;
    template&lt;class T&gt; const T* target() const noexcept;
  
    <ins>friend bool operator==(const function& f, nullptr_t) noexcept { return !f; }</ins>
  };    
  [...]
  <del>// [func.wrap.func.nullptr], Null pointer comparisons</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>
  [...]
}</code></pre></blockquote>  

Remove 19.14.16.2.6 [func.wrap.func.nullptr]:

> <pre><code><del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator==(const function&lt;R(ArgTypes...)&gt;& f, nullptr_t) noexcept;</del>
<del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;& f) noexcept;</del></code></pre>
> <del>*Returns*: `!f`.</del>
> <pre><code><del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;& f, nullptr_t) noexcept;</del>
<del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;& f) noexcept;</del></code></pre>
> <del>*Returns*: `(bool)f`.</del>

Add a new row to 19.15.4.3 [meta.unary.prop], the "Type property predicates" table:

<blockquote><table>
<tr><th>Template</th><th>Condition</th><th>Preconditions</th></tr>
<tr><td colspan="3"><center>...</center></td></tr>
<tr><td><pre style="background:transparent;border:0px"><code><ins>template&lt;class T&gt;
struct has_strong_structural_equality;</ins></code></pre></td><td><ins>The type <code>T</code> has strong structural equality ([class.compare.default])</ins>.</td><td><ins><code>T</code> shall be a complete type, <code class="language-cpp">cv void</code>, or an array of unknown bound.</ins></td></tr>
</table></blockquote>

Change 19.17.2 [type.index.overview]. Note that the relational operators on `type_index` are based on `type_info::before` (effectively `<`). `type_info` _could_ provide a three-way ordering function, but does not. Since an important motivation for the existence of `type_index` is to be used as a key in an associative container, we do not want to pessimize `<` - but do want to provide `<=>`.

<blockquote><pre><code>namespace std {
  class type_index {
  public:
    type_index(const type_info& rhs) noexcept;
    bool operator==(const type_index& rhs) const noexcept;
    <del>bool operator!=(const type_index& rhs) const noexcept;</del>
    bool operator&lt; (const type_index& rhs) const noexcept;
    bool operator&gt; (const type_index& rhs) const noexcept;
    bool operator&lt;= (const type_index& rhs) const noexcept;
    bool operator&gt;= (const type_index& rhs) const noexcept;
    <ins>strong_ordering operator&lt;=&gt;(const type_index& rhs) const noexcept;</ins>
    size_t hash_code() const noexcept;
    const char* name() const noexcept;

  private:
    const type_info* target;    // exposition only
    // Note that the use of a pointer here, rather than a reference,
    // means that the default copy/move constructor and assignment
    // operators will be provided and work as expected.
  };
}</code></pre></blockquote>

Change 19.17.3 [type.index.members]:

> <pre><code>type_index(const type_info& rhs) noexcept;</code></pre>
> *Effects*: Constructs a `type_index` object, the equivalent of `target = &rhs`.
> <pre><code>bool operator==(const type_index& rhs) const noexcept;</code></pre>
> *Returns*: `*target == *rhs.target`.
> <pre><code><del>bool operator!=(const type_index& rhs) const noexcept;</del></code></pre>
> <del>*Returns*: `*target != *rhs.target`.</del>
> <pre><code>bool operator&lt;(const type_index& rhs) const noexcept;</code></pre>
> *Returns*: `target->before(*rhs.target)`.
> <pre><code>bool operator&gt;(const type_index& rhs) const noexcept;</code></pre>
> *Returns*: `rhs.target->before(*target)`.
> <pre><code>bool operator&lt;=(const type_index& rhs) const noexcept;</code></pre>
> *Returns*: `!rhs.target->before(*target)`.
> <pre><code>bool operator&gt;=(const type_index& rhs) const noexcept;</code></pre>
> *Returns*: `!target->before(*rhs.target)`.
> <pre><code><ins>strong_ordering operator&lt;=&gt;(const type_index& rhs) const noexcept;</ins></code></pre>
> <ins>*Effects*: Equivalent to</ins>
> <blockquote class="ins"><pre><code>if (\*target == \*rhs.target) return strong_ordering::equal;
if (target->before(\*rhs.target)) return strong_ordering::less;
return strong_ordering::greater;</code></pre></blockquote>
> <pre><code>size_t hash_code() const noexcept;</code></pre>
> *Returns*: `target->hash_code()`.
> [...]

## Clause 20: Strings library

Changing the operators for `basic_string` and `basic_string_view` and adding extra type alises to the `char_traits` specializations provided by the standard.

Change 20.2.3.1 [char.traits.specializations.char]:

<blockquote><pre><code>namespace std {
  template&lt;&gt; struct char_traits&lt;char&gt; {
    using char_type  = char;
    using int_type   = int;
    using off_type   = streamoff;
    using pos_type   = streampos;
    using state_type = mbstate_t;
    <ins>using comparison_category = strong_ordering;</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.2.3.2 [char.traits.specializations.char8_t]:

<blockquote><pre><code>namespace std {
  template&lt;&gt; struct char_traits&lt;char8_t&gt; {
    using char_type = char8_t;
    using int_type = unsigned int;
    using off_type = streamoff;
    using pos_type = u8streampos;
    using state_type = mbstate_t;
    <ins>using comparison_category = strong_ordering;</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.2.3.3 [char.traits.specializations.char16_t]:

<blockquote><pre><code>namespace std {
  template&lt;&gt; struct char_traits&lt;char16_t&gt; {
    using char_type  = char16_t;
    using int_type   = uint_least16_t;
    using off_type   = streamoff;
    using pos_type   = u16streampos;
    using state_type = mbstate_t;
    <ins>using comparison_category = strong_ordering;</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.2.3.4 [char.traits.specializations.char32_t]

<blockquote><pre><code>namespace std {
  template&lt;&gt; struct char_traits&lt;char32_t&gt; {
    using char_type  = char32_t;
    using int_type   = uint_least32_t;
    using off_type   = streamoff;
    using pos_type   = u32streampos;
    using state_type = mbstate_t;
    <ins>using comparison_category = strong_ordering;</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.2.3.5 [char.traits.specializations.wchar.t]

<blockquote><pre><code>namespace std {
  template&lt;&gt; struct char_traits&lt;wchar_t&gt; {
    using char_type  = wchar_t;
    using int_type   = wint_t;
    using off_type   = streamoff;
    using pos_type   = wstreampos;
    using state_type = mbstate_t;
    <ins>using comparison_category = strong_ordering;</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.3.1 [string.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  [...]
<del>  template&lt;class charT, class traits, class Allocator&gt;
    bool operator==(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator==(const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator==(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator!=(const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);

  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt; (const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt; (const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);

  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&lt;=(const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                    const charT* rhs);
  template&lt;class charT, class traits, class Allocator&gt;
    bool operator&gt;=(const charT* lhs,
                    const basic_string&lt;charT, traits, Allocator&gt;& rhs);</del>
  [...]
}</code></pre></blockquote>

Change 20.3.2 [basic.string]/3. Insert wherever the editor deems appropriate:

<blockquote><pre><code>namespace std {
  template&lt;class charT, class traits = char_traits&lt;charT&gt;,
           class Allocator = allocator&lt;charT&gt;&gt;
  class basic_string {
    [...]
    <ins>friend bool operator==(const basic_string& lhs, const basic_string& rhs) { <i>see below</i> }</ins>
    <ins>friend bool operator==(const basic_string& lhs, const charT* rhs) { <i>see below</i> }</ins>
    <ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const basic_string& rhs) { <i>see below</i> }</ins>
    <ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const charT* rhs) { <i>see below</i> }</ins>
    [...]
  };
}</code></pre></blockquote>

Change 20.3.3.2 [string.cmp].

<blockquote><pre><code><del>template&lt;class charT, class traits, class Allocator&gt;
  bool operator==(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator==(const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator==(const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);</del>
  
<ins>friend bool operator==(const basic_string& lhs, const basic_string& rhs);</ins>
<ins>friend bool operator==(const basic_string& lhs, const charT* rhs);</ins>

<del>template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);</code></pre></blockquote>

> <ins>*Effects*: Equivalent to `return basic_string_view<charT, traits>(lhs) == basic_string_view<charT, traits>(rhs);`</ins>  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>

<blockquote><pre><code><del>template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt; (const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);

template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt; (const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt; (const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);

template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt;=(const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&lt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);

template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt;=(const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator&gt;=(const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);</del>

<ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const basic_string& rhs);</ins>  
<ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const charT* rhs);</ins>  
</code></pre></blockquote>

> *Effects*: <del>Let `op` be the operator</del>. Equivalent to:
  <pre><code>return basic_string_view&lt;charT, traits&gt;(lhs) <del>op</del> <ins>&lt;=&gt;</ins> basic_string_view&lt;charT, traits&gt;(rhs);</code></pre>
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>

Change 20.4.1 [string.view.synop]:

<blockquote><pre><code>namespace std {
  // [string.view.template], class template basic_string_view
  template&lt;class charT, class traits = char_traits&lt;charT&gt;&gt;
  class basic_string_view;

  // [string.view.comparison], non-member comparison functions
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator==(basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator!=(basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator&lt; (basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator&gt; (basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator&lt;=(basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>template&lt;class charT, class traits&gt;</del>
  <del>  constexpr bool operator&gt;=(basic_string_view&lt;charT, traits&gt; x,</del>
  <del>                            basic_string_view&lt;charT, traits&gt; y) noexcept;</del>
  <del>// see [string.view.comparison], sufficient additional overloads of comparison functions</del>
  [...]
}</code></pre></blockquote>
  
Change 20.4.2 [string.view.template], insert wherever the editor deems appropriate

<blockquote><pre><code>template&lt;class charT, class traits = char_traits&lt;charT&gt;&gt;
class basic_string_view {
  [...]
  <ins>friend constexpr bool operator==(basic_string_view, basic_string_view) noexcept { <i>see below</i> }</ins>
  <ins>friend constexpr <i>see below</i> operator&lt;=&gt;(basic_string_view, basic_string_view) noexcept { <i>see below</i> }</ins>
  [...]
};</code></pre></blockquote>

Remove the entirety of 20.4.3 [string.view.comparison]. The proposed two hidden friend declarations satisfy the requirements without needing extra wording. Replace it with the following:

> <pre><code>friend constexpr bool operator==(basic_string_view lhs, basic_string_view rhs) noexcept;</code></pre>
> *Returns:* `lhs.compare(rhs) == 0`.  
> *Remarks*: This function is to be found via argument-dependent lookup only.
> <pre><code>friend constexpr <i>see below</i> operator&lt;=&gt;(basic_string_view, basic_string_view) noexcept;</code></pre>
> Let `R` denote the type `traits::comparison_category` if it exists, otherwise `R` is `weak_ordering`.  
> *Returns:* `static_cast<R>(lhs.compare(rhs) <=> 0)`.  
> *Remarks*: This function is to be found via argument-dependent lookup only.
  
## Clause 21: Containers library

Change 21.3.2 [array.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [array], class template array
  template&lt;class T, size_t N&gt; struct array;

<del>  template&lt;class T, size_t N&gt;
    constexpr bool operator==(const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);
  template&lt;class T, size_t N&gt;
    constexpr bool operator!=(const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);
  template&lt;class T, size_t N&gt;
    constexpr bool operator&lt; (const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);
  template&lt;class T, size_t N&gt;
    constexpr bool operator&gt; (const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);
  template&lt;class T, size_t N&gt;
    constexpr bool operator&lt;=(const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);
  template&lt;class T, size_t N&gt;
    constexpr bool operator&gt;=(const array&lt;T, N&gt;& x, const array&lt;T, N&gt;& y);</del>
  template&lt;class T, size_t N&gt;
    constexpr void swap(array&lt;T, N&gt;& x, array&lt;T, N&gt;& y) noexcept(noexcept(x.swap(y)));
  [...]
}</code></pre></blockquote>

Change 21.3.3 [deque.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [deque], class template deque
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt; class deque;

<del>  template&lt;class T, class Allocator&gt;
    bool operator==(const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator!=(const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt; (const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt; (const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt;=(const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt;=(const deque&lt;T, Allocator&gt;& x, const deque&lt;T, Allocator&gt;& y);</del>

  template&lt;class T, class Allocator&gt;
    void swap(deque&lt;T, Allocator&gt;& x, deque&lt;T, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));
      
  [...]
}</code></pre></blockquote>

Change 21.3.4 [forward_list.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [forwardlist], class template forwardlist
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt; class forward_list;

<del>  template&lt;class T, class Allocator&gt;
    bool operator==(const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator!=(const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt; (const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt; (const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt;=(const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt;=(const forward_list&lt;T, Allocator&gt;& x, const forward_list&lt;T, Allocator&gt;& y);</del>

  template&lt;class T, class Allocator&gt;
    void swap(forward_list&lt;T, Allocator&gt;& x, forward_list&lt;T, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  [...]
}</code></pre></blockquote>

Change 21.3.5 [list.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [list], class template list
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt; class list;

<del>  template&lt;class T, class Allocator&gt;
    bool operator==(const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator!=(const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt; (const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt; (const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt;=(const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt;=(const list&lt;T, Allocator&gt;& x, const list&lt;T, Allocator&gt;& y);</del>

  template&lt;class T, class Allocator&gt;
    void swap(list&lt;T, Allocator&gt;& x, list&lt;T, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  [...]
}</code></pre></blockquote>

Change 21.3.6 [vector.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [vector], class template vector
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt; class vector;

<del>  template&lt;class T, class Allocator&gt;
    bool operator==(const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator!=(const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt; (const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt; (const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&lt;=(const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);
  template&lt;class T, class Allocator&gt;
    bool operator&gt;=(const vector&lt;T, Allocator&gt;& x, const vector&lt;T, Allocator&gt;& y);</del>

  template&lt;class T, class Allocator&gt;
    void swap(vector&lt;T, Allocator&gt;& x, vector&lt;T, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  [...]
}</code></pre></blockquote>

Add to 21.2.1 [container.requirements.general], paragraph 4:

> In Tables 62, 63, and 64 `X` denotes a container class containing objects of type `T`, `a` and `b` denote values of type `X`, <ins>`i` and `j` denote values of type (possibly-const) `X::iterator`,</ins> `u` denotes an identifier, `r` denotes a non-const value of type `X`, and `rv` denotes a non-const rvalue of type `X`.

Add a row to Table 62 — Container requirements:

<blockquote><table>
<tr><th>Expression</th><th>Return<br/>type</th><th>Operational<br/>semantics</th><th>Assertion/note<br />pre/post-condition</th><th>Complexity</th></tr>
<tr><td><pre><code><ins>i &lt;=&gt; j</ins></code></pre></td><td><ins><code>strong_ordering</code> if <code>X::iterator</code> meets the random access iterator requirements, otherwise <code>strong_equality</code></ins></td><td></td><td></td><td><ins>constant</ins></td></tr>
</table></blockquote>

Add to 21.2.1 [container.requirements.general], paragraph 7:

> In the expressions
> <pre><code>i == j
i != j
i < j
i <= j
i >= j
i > j
<ins>i <=> j</ins>
i - j</code></pre>
> where `i` and `j` denote objects of a container's `iterator` type, either or both may be replaced by an object of the container's `const_iterator` type referring to the same element with no change in semantics.

Remove 21.2.1 [container.requirements.general] table 64 - the optional container operations are now just `<=>` instead of the four relational operators, and will be defined inline following the LWG guidance for `flat_map`.

<blockquote><del>Table 64 lists operations that are provided for some types of containers but not others. Those containers for which the listed operations are provided shall implement the semantics described in Table 64 unless otherwise stated. If the iterators passed to <code>lexicographical_compare</code> satisfy the constexpr iterator requirements ([iterator.requirements.general]) then the operations described in Table 64 are implemented by constexpr functions.</del>

<table>
<tr><th><del>Expression</del></th><th><del>Return<br/>type</del></th><th><del>Operational<br/>semantics</del></th><th><Del>Assertion/note<br />pre/post-condition</del></th><th><del>Complexity</del></th></tr>
<tr><td><pre><code><del>a &lt; b</del></code></pre></td><td><del>convertible to <code>bool</code></del></td><td><pre><code><del>lexicographical_compare(
  a.begin(), a.end(),
  b.begin(), b.end())</del></code></pre></td><td><del><i>Requires</i>: <code>&lt;</code> is defined for values of <code>T</code>. <code>&lt;</code> is a total ordering relationship.</del></td><td><del>linear</del></td></tr>
<tr><td><pre><code><del>a &gt; b</del></code></pre></td><td><del>convertible to <code>bool</code></del><td><pre><code><del>b &lt; a</del></code></pre></td><td></td><td><del>linear</del></td></tr>
<tr><td><pre><code><del>a &lt;= b</del></code></pre></td><td><del>convertible to <code>bool</code></del><td><pre><code><del>!(a &gt; b)</del></code></pre></td><td></td><td><del>linear</del></td></tr>
<tr><td><pre><code><del>a &gt;= b</del></code></pre></td><td><del>convertible to <code>bool</code></del><td><pre><code><del>!(a &lt; b)</del></code></pre></td><td></td><td><del>linear</del></td></tr>
</table>

<Del>[<i>Note</i>: The algorithm <code>lexicographical_compare()</code> is defined in [algorithms]. —<i>end note</i>]</del>

</blockquote>

Change 21.3.7.1, paragraph 4 [array.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class T, size_t N&gt;
  struct array {
    [...]
    
    constexpr T *       data() noexcept;
    constexpr const T * data() const noexcept;
    
    <ins>friend constexpr bool operator==(const array&, const array&) = default;</ins>
    <ins>friend constexpr <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const array& x, const array& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  };

  template&lt;class T, class... U&gt;
    array(T, U...) -&gt; array&lt;T, 1 + sizeof...(U)&gt;;
}</code></pre></blockquote>

Change 21.3.8.1, paragraph 2 [deque.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt;
  class deque {
  public:
    [...]
    void     swap(deque&)
      noexcept(allocator_traits&lt;Allocator&gt;::is_always_equal::value);
    void     clear() noexcept;
    
    <ins>friend bool operator==(const deque& x, const deque& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const deque& x, const deque& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  };

  [...]
}</code></pre></blockquote>  

Change 21.3.9.1, paragraph 3 [forwardlist.overview]

<blockquote><pre><code>namespace std {
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt;
  class forward_list {
  public:
    [...]
    void sort();
    template&lt;class Compare&gt; void sort(Compare comp);

    void reverse() noexcept;
    
    <ins>friend bool operator==(const forward_list& x, const forward_list& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const forward_list& x, const forward_list& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  };

  [...]
}</code></pre></blockquote>  

Change 21.3.10.1, paragraph 2 [list.overview]

<blockquote><pre><code>namespace std {
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt;
  class list {
  public:
    [...]
    void sort();
    template&lt;class Compare&gt; void sort(Compare comp);

    void reverse() noexcept;
    
    <ins>friend bool operator==(const list& x, const list& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const list& x, const list& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  };
  
  [...]  
}</code></pre></blockquote>

Change 21.3.11.1, paragraph 2 [vector.overview]

<blockquote><pre><code>namespace std {
  template&lt;class T, class Allocator = allocator&lt;T&gt;&gt;
  class vector {
  public:
    [...]
    void     swap(vector&)
      noexcept(allocator_traits&lt;Allocator&gt;::propagate_on_container_swap::value ||
               allocator_traits&lt;Allocator&gt;::is_always_equal::value);
    void     clear() noexcept;
    
    <ins>friend bool operator==(const vector& x, const vector& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const vector& x, const vector& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>   
  };
  [...]
}</code></pre></blockquote>  

Change 21.3.12 [vector.bool]:

<blockquote><pre><code>namespace std {
  template&lt;class Allocator&gt;
  class vector&lt;bool, Allocator&gt; {
  public:
    [...]
    static void swap(reference x, reference y) noexcept;
    void flip() noexcept;       // flips all bits
    void clear() noexcept;
    
    <ins>friend bool operator==(const vector& x, const vector& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const vector& x, const vector& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), compare_three_way()); }</ins>
  };
}</code></pre></blockquote>

Change 21.4.2 [associative.map.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [map], class template map
  template&lt;class Key, class T, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
    class map;

<del>  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator==(const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator!=(const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&lt; (const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&gt; (const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&lt;=(const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&gt;=(const map&lt;Key, T, Compare, Allocator&gt;& x,
                    const map&lt;Key, T, Compare, Allocator&gt;& y);</del>

  template&lt;class Key, class T, class Compare, class Allocator&gt;
    void swap(map&lt;Key, T, Compare, Allocator&gt;& x,
              map&lt;Key, T, Compare, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  template &lt;class Key, class T, class Compare, class Allocator, class Predicate&gt;
    void erase_if(map&lt;Key, T, Compare, Allocator&gt;& c, Predicate pred);

  // [multimap], class template multimap
  template&lt;class Key, class T, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
    class multimap;

<del>  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator==(const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator!=(const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&lt; (const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&gt; (const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&lt;=(const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);
  template&lt;class Key, class T, class Compare, class Allocator&gt;
    bool operator&gt;=(const multimap&lt;Key, T, Compare, Allocator&gt;& x,
                    const multimap&lt;Key, T, Compare, Allocator&gt;& y);</del>

  template&lt;class Key, class T, class Compare, class Allocator&gt;
    void swap(multimap&lt;Key, T, Compare, Allocator&gt;& x,
              multimap&lt;Key, T, Compare, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  template &lt;class Key, class T, class Compare, class Allocator, class Predicate&gt;
    void erase_if(multimap&lt;Key, T, Compare, Allocator&gt;& c, Predicate pred);

  namespace pmr {
    template&lt;class Key, class T, class Compare = less&lt;Key&gt;&gt;
      using map = std::map&lt;Key, T, Compare,
                           polymorphic_allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;;

    template&lt;class Key, class T, class Compare = less&lt;Key&gt;&gt;
      using multimap = std::multimap&lt;Key, T, Compare,
                                     polymorphic_allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;;
  }
}</code></pre></blockquote>

Change 21.4.3 [associative.set.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [set], class template set
  template&lt;class Key, class Compare = less&lt;Key&gt;, class Allocator = allocator&lt;Key&gt;&gt;
    class set;

<del>  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator==(const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator!=(const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&lt; (const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&gt; (const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&lt;=(const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&gt;=(const set&lt;Key, Compare, Allocator&gt;& x,
                    const set&lt;Key, Compare, Allocator&gt;& y);</del>

  template&lt;class Key, class Compare, class Allocator&gt;
    void swap(set&lt;Key, Compare, Allocator&gt;& x,
              set&lt;Key, Compare, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  template &lt;class Key, class Compare, class Allocator, class Predicate&gt;
    void erase_if(set&lt;Key, Compare, Allocator&gt;& c, Predicate pred);

  // [multiset], class template multiset
  template&lt;class Key, class Compare = less&lt;Key&gt;, class Allocator = allocator&lt;Key&gt;&gt;
    class multiset;

<del>  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator==(const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator!=(const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&lt; (const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&gt; (const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&lt;=(const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);
  template&lt;class Key, class Compare, class Allocator&gt;
    bool operator&gt;=(const multiset&lt;Key, Compare, Allocator&gt;& x,
                    const multiset&lt;Key, Compare, Allocator&gt;& y);</del>

  template&lt;class Key, class Compare, class Allocator&gt;
    void swap(multiset&lt;Key, Compare, Allocator&gt;& x,
              multiset&lt;Key, Compare, Allocator&gt;& y)
      noexcept(noexcept(x.swap(y)));

  template &lt;class Key, class Compare, class Allocator, class Predicate&gt;
    void erase_if(multiset&lt;Key, Compare, Allocator&gt;& c, Predicate pred);

  namespace pmr {
    template&lt;class Key, class Compare = less&lt;Key&gt;&gt;
      using set = std::set&lt;Key, Compare, polymorphic_allocator&lt;Key&gt;&gt;;

    template&lt;class Key, class Compare = less&lt;Key&gt;&gt;
      using multiset = std::multiset&lt;Key, Compare, polymorphic_allocator&lt;Key&gt;&gt;;
  }
}</code></pre></blockquote>

Change 21.4.4.1 [map.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key, class T, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
  class map {
  public:
    [...]
    pair&lt;iterator, iterator&gt;               equal_range(const key_type& x);
    pair&lt;const_iterator, const_iterator&gt;   equal_range(const key_type& x) const;
    template&lt;class K&gt;
      pair&lt;iterator, iterator&gt;             equal_range(const K& x);
    template&lt;class K&gt;
      pair&lt;const_iterator, const_iterator&gt; equal_range(const K& x) const;
      
    <ins>friend bool operator==(const map& x, const map& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const map& x, const map& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  };

  [...]
}</code></pre></blockquote>

Change 21.4.5.1 [multimap.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key, class T, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
  class multimap {
  public:
    [...]
    pair&lt;iterator, iterator&gt;               equal_range(const key_type& x);
    pair&lt;const_iterator, const_iterator&gt;   equal_range(const key_type& x) const;
    template&lt;class K&gt;
      pair&lt;iterator, iterator&gt;             equal_range(const K& x);
    template&lt;class K&gt;
      pair&lt;const_iterator, const_iterator&gt; equal_range(const K& x) const;
      
    <ins>friend bool operator==(const multimap& x, const multimap& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const multimap& x, const multimap& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>
  
  };

  [...]
}</code></pre></blockquote>  

Change 21.4.6.1 [set.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;Key&gt;&gt;
  class set {
  public:
    [...]
    pair&lt;iterator, iterator&gt;               equal_range(const key_type& x);
    pair&lt;const_iterator, const_iterator&gt;   equal_range(const key_type& x) const;
    template&lt;class K&gt;
      pair&lt;iterator, iterator&gt;             equal_range(const K& x);
    template&lt;class K&gt;
      pair&lt;const_iterator, const_iterator&gt; equal_range(const K& x) const;
      
    <ins>friend bool operator==(const set& x, const set& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const set& x, const set& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>          
  };

  [...]
}</code></pre></blockquote>  

Change 21.4.7.1 [multiset.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key, class Compare = less&lt;Key&gt;,
           class Allocator = allocator&lt;Key&gt;&gt;
  class multiset {
  public:
    [...]
    
    pair&lt;iterator, iterator&gt;               equal_range(const key_type& x);
    pair&lt;const_iterator, const_iterator&gt;   equal_range(const key_type& x) const;
    template&lt;class K&gt;
      pair&lt;iterator, iterator&gt;             equal_range(const K& x);
    template&lt;class K&gt;
      pair&lt;const_iterator, const_iterator&gt; equal_range(const K& x) const;
      
    <ins>friend bool operator==(const multiset& x, const multiset& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
    <ins>friend <i>synth-3way-result</i>&lt;value_type&gt; operator&lt;=&gt;(const multiset& x, const multiset& y)</ins>
    <ins>  { return lexicographical_compare_three_way(x.begin(), x.end(), y.begin(), y.end(), <i>synth-3way</i>); }</ins>      
  };

  [...]
}</code></pre></blockquote>  

Change 21.5.2 [unord.map.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [unord.map], class template unordered_map
  template&lt;class Key,
           class T,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Alloc = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
    class unordered_map;

  // [unord.multimap], class template unordered_multimap
  template&lt;class Key,
           class T,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Alloc = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
    class unordered_multimap;

<del>  template&lt;class Key, class T, class Hash, class Pred, class Alloc&gt;
    bool operator==(const unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& a,
                    const unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& b);
  template&lt;class Key, class T, class Hash, class Pred, class Alloc&gt;
    bool operator!=(const unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& a,
                    const unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& b);

  template&lt;class Key, class T, class Hash, class Pred, class Alloc&gt;
    bool operator==(const unordered_multimap&lt;Key, T, Hash, Pred, Alloc&gt;& a,
                    const unordered_multimap&lt;Key, T, Hash, Pred, Alloc&gt;& b);
  template&lt;class Key, class T, class Hash, class Pred, class Alloc&gt;
    bool operator!=(const unordered_multimap&lt;Key, T, Hash, Pred, Alloc&gt;& a,
                    const unordered_multimap&lt;Key, T, Hash, Pred, Alloc&gt;& b);</del>

  template&lt;class Key, class T, class Hash, class Pred, class Alloc&gt;
    void swap(unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& x,
              unordered_map&lt;Key, T, Hash, Pred, Alloc&gt;& y)
      noexcept(noexcept(x.swap(y)));
  
  [...]
}</code></pre></blockquote>

Change 21.5.3 [unord.set.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  // [unord.set], class template unordered_set
  template&lt;class Key,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Alloc = allocator&lt;Key&gt;&gt;
    class unordered_set;

  // [unord.multiset], class template unordered_multiset
  template&lt;class Key,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Alloc = allocator&lt;Key&gt;&gt;
    class unordered_multiset;

<del>  template&lt;class Key, class Hash, class Pred, class Alloc&gt;
    bool operator==(const unordered_set&lt;Key, Hash, Pred, Alloc&gt;& a,
                    const unordered_set&lt;Key, Hash, Pred, Alloc&gt;& b);
  template&lt;class Key, class Hash, class Pred, class Alloc&gt;
    bool operator!=(const unordered_set&lt;Key, Hash, Pred, Alloc&gt;& a,
                    const unordered_set&lt;Key, Hash, Pred, Alloc&gt;& b);

  template&lt;class Key, class Hash, class Pred, class Alloc&gt;
    bool operator==(const unordered_multiset&lt;Key, Hash, Pred, Alloc&gt;& a,
                    const unordered_multiset&lt;Key, Hash, Pred, Alloc&gt;& b);
  template&lt;class Key, class Hash, class Pred, class Alloc&gt;
    bool operator!=(const unordered_multiset&lt;Key, Hash, Pred, Alloc&gt;& a,
                    const unordered_multiset&lt;Key, Hash, Pred, Alloc&gt;& b);</del>

  template&lt;class Key, class Hash, class Pred, class Alloc&gt;
    void swap(unordered_set&lt;Key, Hash, Pred, Alloc&gt;& x,
              unordered_set&lt;Key, Hash, Pred, Alloc&gt;& y)
      noexcept(noexcept(x.swap(y)));

  [...]
}</code></pre></blockquote>

Change 21.5.4.1 [unord.map.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key,
           class T,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
  class unordered_map {
  public:
    [...]
    // hash policy
    float load_factor() const noexcept;
    float max_load_factor() const noexcept;
    void max_load_factor(float z);
    void rehash(size_type n);
    void reserve(size_type n);
    
    <ins>friend bool operator==(const unordered_map& x, const unordered_map& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>
  };

  [...]
}</code></pre></blockquote>

Change 21.5.5.1 [unord.multimap.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key,
           class T,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Allocator = allocator&lt;pair&lt;const Key, T&gt;&gt;&gt;
  class unordered_multimap {
  public:
    [...]
    // hash policy
    float load_factor() const noexcept;
    float max_load_factor() const noexcept;
    void max_load_factor(float z);
    void rehash(size_type n);
    void reserve(size_type n);
    
    <ins>friend bool operator==(const unordered_multimap& x, const unordered_multimap& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>    
  };

  [...]
}</code></pre></blockquote>

Change 21.5.6.1 [unord.set.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Allocator = allocator&lt;Key&gt;&gt;
  class unordered_set {
  public:
    [...]
    // hash policy
    float load_factor() const noexcept;
    float max_load_factor() const noexcept;
    void max_load_factor(float z);
    void rehash(size_type n);
    void reserve(size_type n);
    
    <ins>friend bool operator==(const unordered_set& x, const unordered_set& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>        
  };

  [...]
}</code></pre></blockquote>

Change 21.5.7.1 [unord.multiset.overview]:

<blockquote><pre><code>namespace std {
  template&lt;class Key,
           class Hash = hash&lt;Key&gt;,
           class Pred = equal_to&lt;Key&gt;,
           class Allocator = allocator&lt;Key&gt;&gt;
  class unordered_multiset {
  public:
    [...]
    // hash policy
    float load_factor() const noexcept;
    float max_load_factor() const noexcept;
    void max_load_factor(float z);
    void rehash(size_type n);
    void reserve(size_type n);
    
    <ins>friend bool operator==(const unordered_multiset& x, const unordered_multiset& y)</ins>
    <ins>  { return ranges::equal(x, y); }</ins>        
  };

  [...]
}</code></pre></blockquote>

Change 21.6.2 [queue.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  template&lt;class T, class Container = deque&lt;T&gt;&gt; class queue;
  template&lt;class T, class Container = vector&lt;T&gt;,
           class Compare = less&lt;typename Container::value_type&gt;&gt;
    class priority_queue;

<del>  template&lt;class T, class Container&gt;
    bool operator==(const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator!=(const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&lt; (const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&gt; (const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&lt;=(const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&gt;=(const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);</del>

  template&lt;class T, class Container&gt;
    void swap(queue&lt;T, Container&gt;& x, queue&lt;T, Container&gt;& y) noexcept(noexcept(x.swap(y)));
  template&lt;class T, class Container, class Compare&gt;
    void swap(priority_queue&lt;T, Container, Compare&gt;& x,
              priority_queue&lt;T, Container, Compare&gt;& y) noexcept(noexcept(x.swap(y)));
}</code></pre></blockquote>

Change 21.6.3 [stack.syn]:

<blockquote><pre><code>#include &lt;initializer_list&gt;

namespace std {
  template&lt;class T, class Container = deque&lt;T&gt;&gt; class stack;

<del>  template&lt;class T, class Container&gt;
    bool operator==(const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator!=(const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&lt; (const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&gt; (const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&lt;=(const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);
  template&lt;class T, class Container&gt;
    bool operator&gt;=(const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);</del>

  template&lt;class T, class Container&gt;
    void swap(stack&lt;T, Container&gt;& x, stack&lt;T, Container&gt;& y) noexcept(noexcept(x.swap(y)));
}</code></pre></blockquote>

Change 21.6.4.1 [queue.defn]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class Container = deque&lt;T&gt;&gt;
  class queue {
  public:
    using value_type      = typename Container::value_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;
    using size_type       = typename Container::size_type;
    using container_type  =          Container;

  protected:
    Container c;

  public:
    queue() : queue(Container()) {}
    explicit queue(const Container&);
    explicit queue(Container&&);
    template&lt;class Alloc&gt; explicit queue(const Alloc&);
    template&lt;class Alloc&gt; queue(const Container&, const Alloc&);
    template&lt;class Alloc&gt; queue(Container&&, const Alloc&);
    template&lt;class Alloc&gt; queue(const queue&, const Alloc&);
    template&lt;class Alloc&gt; queue(queue&&, const Alloc&);

    [[nodiscard]] bool empty() const    { return c.empty(); }
    size_type         size()  const     { return c.size(); }
    reference         front()           { return c.front(); }
    const_reference   front() const     { return c.front(); }
    reference         back()            { return c.back(); }
    const_reference   back() const      { return c.back(); }
    void push(const value_type& x)      { c.push_back(x); }
    void push(value_type&& x)           { c.push_back(std::move(x)); }
    template&lt;class... Args&gt;
      decltype(auto) emplace(Args&&... args)
        { return c.emplace_back(std::forward&lt;Args&gt;(args)...); }
    void pop()                          { c.pop_front(); }
    void swap(queue& q) noexcept(is_nothrow_swappable_v&lt;Container&gt;)
      { using std::swap; swap(c, q.c); }
    
    <ins>friend bool operator==(const queue& x, const queue& y)</ins>
    <ins>  { return x.c == y.c; }</ins>
    <ins>friend bool operator!=(const queue& x, const queue& y)</ins>
    <ins>  { return x.c != y.c; }</ins>
    <ins>friend bool operator&lt; (const queue& x, const queue& y)</ins>
    <ins>  { return x.c &lt; y.c; }</ins>
    <ins>friend bool operator&gt; (const queue& x, const queue& y)</ins>
    <ins>  { return x.c &gt; y.c; }</ins>
    <ins>friend bool operator&lt;=(const queue& x, const queue& y)</ins>
    <ins>  { return x.c &lt;= y.c; }</ins>
    <ins>friend bool operator&gt;=(const queue& x, const queue& y)</ins>
    <ins>  { return x.c &gt;= y.c; }</ins>
    <ins>friend auto operator&lt;=&gt;(const queue& x, const queue& y)</ins>
    <ins>  requires ThreeWayComparable&lt;Container&gt;</ins>
    <ins>    { return x.c &lt;=&gt; y.c; }</ins>
  };
  [...]
}</code></pre></blockquote>  

Remove 21.6.4.4 [queue.ops] (as we've now defined them all inline in the header):

> <pre><code><del>template&lt;class T, class Container&gt;
  bool operator==(const queue&lt;T, Container&gt;& x, const queue&lt;T, Container&gt;& y);</del></code></pre>
> <del>*Returns*: `x.c == y.c`.</del>  
> [...]

Change 21.6.6.1 [stack.defn]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class Container = deque&lt;T&gt;&gt;
  class stack {
  public:
    using value_type      = typename Container::value_type;
    using reference       = typename Container::reference;
    using const_reference = typename Container::const_reference;
    using size_type       = typename Container::size_type;
    using container_type  = Container;

  protected:
    Container c;

  public:
    stack() : stack(Container()) {}
    explicit stack(const Container&);
    explicit stack(Container&&);
    template&lt;class Alloc&gt; explicit stack(const Alloc&);
    template&lt;class Alloc&gt; stack(const Container&, const Alloc&);
    template&lt;class Alloc&gt; stack(Container&&, const Alloc&);
    template&lt;class Alloc&gt; stack(const stack&, const Alloc&);
    template&lt;class Alloc&gt; stack(stack&&, const Alloc&);

    [[nodiscard]] bool empty() const    { return c.empty(); }
    size_type size()  const             { return c.size(); }
    reference         top()             { return c.back(); }
    const_reference   top() const       { return c.back(); }
    void push(const value_type& x)      { c.push_back(x); }
    void push(value_type&& x)           { c.push_back(std::move(x)); }
    template&lt;class... Args&gt;
      decltype(auto) emplace(Args&&... args)
        { return c.emplace_back(std::forward&lt;Args&gt;(args)...); }
    void pop()                          { c.pop_back(); }
    void swap(stack& s) noexcept(is_nothrow_swappable_v&lt;Container&gt;)
      { using std::swap; swap(c, s.c); }
      
    <ins>friend bool operator==(const stack& x, const stack& y)</ins>
    <ins>  { return x.c == y.c; }</ins>
    <ins>friend bool operator!=(const stack& x, const stack& y)</ins>
    <ins>  { return x.c != y.c; }</ins>
    <ins>friend bool operator&lt; (const stack& x, const stack& y)</ins>
    <ins>  { return x.c &lt; y.c; }</ins>
    <ins>friend bool operator&gt; (const stack& x, const stack& y)</ins>
    <ins>  { return x.c &gt; y.c; }</ins>
    <ins>friend bool operator&lt;=(const stack& x, const stack& y)</ins>
    <ins>  { return x.c &lt;= y.c; }</ins>
    <ins>friend bool operator&gt;=(const stack& x, const stack& y)</ins>
    <ins>  { return x.c &gt;= y.c; }</ins>
    <ins>friend auto operator&lt;=&gt;(const stack& x, const stack& y)</ins>
    <ins>  requires ThreeWayComparable&lt;Container&gt;</ins>
    <ins>    { return x.c &lt;=&gt; y.c; }</ins>      
  };
  
  [...]
}</code></pre></blockquote>

Remove 21.6.6.4 [stack.ops] (as we've now defined them all inline in the header):

> <pre><code><del>template&lt;class T, class Container&gt;
  bool operator==(const stack&lt;T, Container&gt;& x, const stack&lt;T, Container&gt;& y);</del></code></pre>
> <del>*Returns*: `x.c == y.c`.</del>  
> [...]

## Clause 22: Iterators library

Change 22.2 [iterator.synopsis]:

<blockquote><pre><code>#include &lt;concepts&gt;

namespace std {
  [...]
  // [predef.iterators], predefined iterators and sentinels
  // [reverse.iterators], reverse iterators
  template&lt;class Iterator&gt; class reverse_iterator;

<del>  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator==(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator!=(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&lt;(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&gt;(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&lt;=(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&gt;=(
      const reverse_iterator&lt;Iterator1&gt;& x,
      const reverse_iterator&lt;Iterator2&gt;& y);</del>
  [...]
  
  // [move.iterators], move iterators and sentinels
  template&lt;class Iterator&gt; class move_iterator;

<del>  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator==(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator!=(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&lt;(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&gt;(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&lt;=(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);
  template&lt;class Iterator1, class Iterator2&gt;
    constexpr bool operator&gt;=(
      const move_iterator&lt;Iterator1&gt;& x, const move_iterator&lt;Iterator2&gt;& y);</del>
      
  [...]

  // [stream.iterators], stream iterators
  template&lt;class T, class charT = char, class traits = char_traits&lt;charT&gt;,
           class Distance = ptrdiff_t&gt;
  class istream_iterator;
<del>  template&lt;class T, class charT, class traits, class Distance&gt;
    bool operator==(const istream_iterator&lt;T,charT,traits,Distance&gt;& x,
            const istream_iterator&lt;T,charT,traits,Distance&gt;& y);
  template&lt;class T, class charT, class traits, class Distance&gt;
    bool operator!=(const istream_iterator&lt;T,charT,traits,Distance&gt;& x,
            const istream_iterator&lt;T,charT,traits,Distance&gt;& y);</del>

  template&lt;class T, class charT = char, class traits = char_traits&lt;charT&gt;&gt;
      class ostream_iterator;

  template&lt;class charT, class traits = char_traits&lt;charT&gt;&gt;
    class istreambuf_iterator;
<del>  template&lt;class charT, class traits&gt;
    bool operator==(const istreambuf_iterator&lt;charT,traits&gt;& a,
            const istreambuf_iterator&lt;charT,traits&gt;& b);
  template&lt;class charT, class traits&gt;
    bool operator!=(const istreambuf_iterator&lt;charT,traits&gt;& a,
            const istreambuf_iterator&lt;charT,traits&gt;& b);</del>

  template&lt;class charT, class traits = char_traits&lt;charT&gt;&gt;
    class ostreambuf_iterator;  
  [...]
}</code></pre></blockquote>

Change 22.5.1.1 [reverse.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;class Iterator&gt;
  class reverse_iterator {
  public:
    [...]
    template&lt;IndirectlySwappable&lt;Iterator&gt; Iterator2&gt;
      friend constexpr void
        iter_swap(const reverse_iterator& x,
                  const reverse_iterator&lt;Iterator2&gt;& y) noexcept(see below);

<ins>    // [reverse.iter.cmp] Comparisons
    template&lt;class Iterator2&gt;
      friend constexpr bool operator==(const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator!=(const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&lt; (const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&gt; (const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&lt;=(const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&gt;=(const reverse_iterator& x,
                                       const reverse_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;ThreeWayComparableWith&lt;Iterator&gt; Iterator2&gt;
      friend constexpr compare_three_way_result_t&lt;Iterator, Iterator2&gt;
        operator&lt;=&gt;(const reverse_iterator& x,
                    const reverse_iterator&lt;Iterator2&gt;& y)
        { see below }</ins>
                  
  protected:
    Iterator current;
  };
}</code></pre></blockquote>

Change 22.5.1.7 [reverse.iter.cmp]:

> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator==(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current == y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current == y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator!=(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current != y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current != y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator&lt;(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current > y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current > y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator&gt;(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current < y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current < y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator&lt;=(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current >= y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current >= y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator&gt;=(
    const reverse_iterator<del>&lt;Iterator1&gt;</del>& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.current <= y.current` is well-formed and convertible to `bool`.  
> *Returns*: `x.current <= y.current`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;Iterator&gt; Iterator2&gt;
  friend constexpr compare_three_way_result_t&lt;Iterator, Iterator2&gt; operator&lt;=&gt;(
    const reverse_iterator& x,
    const reverse_iterator&lt;Iterator2&gt;& y);</ins></code></pre>
> <ins>*Returns*: `y.current <=> x.current`.</ins>  
> <ins>*Remarks*: This function is more constrained than ([temp.constr.order]) each of the other relational operator function templates. This function is to be found via argument-dependent lookup only.</ins>

Change 22.5.3.1 [move.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;class Iterator&gt;
  class move_iterator {
    [...]
<ins>    // [move.iter.op.comp] Comparisons
    template&lt;class Iterator2&gt;
      friend constexpr bool operator==(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&lt;(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&gt;(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&lt;=(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;class Iterator2&gt;
      friend constexpr bool operator&gt;=(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
      { see below }
    template&lt;ThreeWayComparableWith&lt;Iterator&gt; Iterator2&gt;
      friend constexpr compare_three_way_result_t&lt;Iterator, Iterator2&gt;
        operator&lt;=&gt;(const move_iterator& x, const move_iterator&lt;Iterator2&gt;& y)
        { see below }</ins>
      
    template&lt;Sentinel&lt;Iterator&gt; S&gt;
      friend constexpr bool
        operator==(const move_iterator& x, const move_sentinel&lt;S&gt;& y);
<del>    template&lt;Sentinel&lt;Iterator&gt; S&gt;
      friend constexpr bool
        operator==(const move_sentinel&lt;S&gt;& x, const move_iterator& y);
    template&lt;Sentinel&lt;Iterator&gt; S&gt;
      friend constexpr bool
        operator!=(const move_iterator& x, const move_sentinel&lt;S&gt;& y);
    template&lt;Sentinel&lt;Iterator&gt; S&gt;
      friend constexpr bool
        operator!=(const move_sentinel&lt;S&gt;& x, const move_iterator& y);</del>
    template&lt;SizedSentinel&lt;Iterator&gt; S&gt;
      friend constexpr iter_difference_t&lt;Iterator&gt;
        operator-(const move_sentinel&lt;S&gt;& x, const move_iterator& y);
    [...]
  private:
    Iterator current;   // exposition only
  };
}</code></pre></blockquote>

Change 22.5.3.7 [move.iter.op.comp]:

> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
  <ins>friend </ins>constexpr bool operator==(const move_iterator<del>&lt;Iterator1&gt;</del>& x,
                            const move_iterator&lt;Iterator2&gt;& y);
template&lt;Sentinel&lt;Iterator&gt; S&gt;
  friend constexpr bool operator==(const move_iterator& x,
                                   const move_sentinel&lt;S&gt;& y);
template&lt;Sentinel&lt;Iterator&gt; S&gt;
  friend constexpr bool operator==(const move_sentinel&lt;S&gt;& x,
                                   const move_iterator& y);</code></pre>
> *Constraints*: `x.base() == y.base()` is well-formed and convertible to `bool`.  
> *Returns*: `x.base() == y.base()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><del>template&lt;class Iterator1, class Iterator2&gt;
  constexpr bool operator!=(const move_iterator&lt;Iterator1&gt;& x,
                            const move_iterator&lt;Iterator2&gt;& y);
template&lt;Sentinel&lt;Iterator&gt; S&gt;
  friend constexpr bool operator!=(const move_iterator& x,
                                   const move_sentinel&lt;S&gt;& y);
template&lt;Sentinel&lt;Iterator&gt; S&gt;
  friend constexpr bool operator!=(const move_sentinel&lt;S&gt;& x,
                                   const move_iterator& y);</del></code></pre>
> <del>*Constraints*: `x.base() == y.base()` is well-formed and convertible to `bool`.</del>  
> <del>*Returns*: `!(x == y)`.</del>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
<ins>friend </ins>constexpr bool operator&lt;(const move_iterator<del>&lt;Iterator1&gt;</del>& x, const move_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.base() < y.base()` is well-formed and convertible to `bool`.  
> *Returns*: `x.base() < y.base()`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
<ins>friend </ins>constexpr bool operator&gt;(const move_iterator<del>&lt;Iterator1&gt;</del>& x, const move_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `y.base() < x.base()` is well-formed and convertible to `bool`.  
> *Returns*: `y < x`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
<ins>friend </ins>constexpr bool operator&lt;=(const move_iterator<del>&lt;Iterator1&gt;</del>& x, const move_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `y.base() < x.base()` is well-formed and convertible to `bool`.  
> *Returns*: `!(y < x)`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code>template&lt;<del>class Iterator1, </del>class Iterator2&gt;
<ins>friend </ins>constexpr bool operator&gt;=(const move_iterator<del>&lt;Iterator1&gt;</del>& x, const move_iterator&lt;Iterator2&gt;& y);</code></pre>
> *Constraints*: `x.base() < y.base()` is well-formed and convertible to `bool`.  
> *Returns*: `!(x < y)`.  
> <ins>*Remarks*: This function is to be found via argument-dependent lookup only.</ins>
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;Iterator&gt; Iterator2&gt;
  friend constexpr compare_three_way_result_t&lt;Iterator, Iterator2&gt; operator&lt;=&gt;(
    const move_iterator& x,
    const move_iterator&lt;Iterator2&gt;& y);</ins></code></pre>
> <ins>*Returns*: `x.base() <=> y.base()`.</ins>  
> <ins>*Remarks*: This function is more constrained than ([temp.constr.order]) each of the other relational operator function templates. This function is to be found via argument-dependent lookup only.</ins>

Remove the `operator!=` from 22.5.4.1 [common.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;Iterator I, Sentinel&lt;I&gt; S&gt;
    requires (!Same&lt;I, S&gt;)
  class common_iterator {
  public:
    [...]
    template&lt;class I2, Sentinel&lt;I&gt; S2&gt;
      requires Sentinel&lt;S, I2&gt;
    friend bool operator==(
      const common_iterator& x, const common_iterator&lt;I2, S2&gt;& y);
    template&lt;class I2, Sentinel&lt;I&gt; S2&gt;
      requires Sentinel&lt;S, I2&gt; && EqualityComparableWith&lt;I, I2&gt;
    friend bool operator==(
      const common_iterator& x, const common_iterator&lt;I2, S2&gt;& y);
<del>    template&lt;class I2, Sentinel&lt;I&gt; S2&gt;
      requires Sentinel&lt;S, I2&gt;
    friend bool operator!=(
      const common_iterator& x, const common_iterator&lt;I2, S2&gt;& y);</del>
    [...]
  private:
    variant&lt;I, S&gt; v_;   // exposition only
  };
  [...]
}</code></pre></blockquote>

Remove the `operator!=` from 22.5.4.6 [common.iter.cmp]:

> <pre><code><del>template&lt;class I2, Sentinel&lt;I&gt; S2&gt;
  requires Sentinel&lt;S, I2&gt;
friend bool operator!=(
  const common_iterator& x, const common_iterator&lt;I2, S2&gt;& y);</del></code></pre>
> <del>*Effects*: Equivalent to: `return !(x == y);`</del>

Change 22.5.6.1 [counted.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;Iterator I&gt;
  class counted_iterator {
  public:
    using iterator_type = I;
    [...]
    template&lt;Common&lt;I&gt; I2&gt;
      friend constexpr bool operator==(
        const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
    friend constexpr bool operator==(
      const counted_iterator& x, default_sentinel_t);
    <del>friend constexpr bool operator==(</del>
    <del>  default_sentinel_t, const counted_iterator& x);</del>

    <del>template&lt;Common&lt;I&gt; I2&gt;</del>
    <del>  friend constexpr bool operator!=(</del>
    <del>    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</del>
    <del>friend constexpr bool operator!=(</del>
    <del>  const counted_iterator& x, default_sentinel_t y);</del>
    <del>friend constexpr bool operator!=(</del>
    <del>  default_sentinel_t x, const counted_iterator& y);</del>

    template&lt;Common&lt;I&gt; I2&gt;
      friend constexpr bool operator&lt;(
        const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
    template&lt;Common&lt;I&gt; I2&gt;
      friend constexpr bool operator&gt;(
        const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
    template&lt;Common&lt;I&gt; I2&gt;
      friend constexpr bool operator&lt;=(
        const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
    template&lt;Common&lt;I&gt; I2&gt;
      friend constexpr bool operator&gt;=(
        const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
    <ins>template&lt;Common&lt;I&gt; I2&gt; requires ThreeWayComparableWith&lt;I, I2&gt;</ins>
    <ins>  friend constexpr compare_three_way_result_t&lt;I, I2&gt; operator&lt;=&gt;(</ins>
    <ins>    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</ins>

    friend constexpr iter_rvalue_reference_t&lt;I&gt; iter_move(const counted_iterator& i)
      noexcept(noexcept(ranges::iter_move(i.current)))
        requires InputIterator&lt;I&gt;;
    template&lt;IndirectlySwappable&lt;I&gt; I2&gt;
      friend constexpr void iter_swap(const counted_iterator& x, const counted_iterator&lt;I2&gt;& y)
        noexcept(noexcept(ranges::iter_swap(x.current, y.current)));

  private:
    I current = I();                    // exposition only
    iter_difference_t&lt;I&gt; length = 0;    // exposition only
  };
  [...]
}</code></pre></blockquote>

Change 22.5.6.6 [counted.iter.cmp]:

> <pre><code>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator==(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</code></pre>
> *Expects*: `x` and `y` refer to elements of the same sequence ([counted.iterator]).  
> *Effects*: Equivalent to: return `x.length == y.length;`
> <pre><code>friend constexpr bool operator==(
  const counted_iterator& x, default_sentinel_t);
<del>friend constexpr bool operator==(</del>
<del>  default_sentinel_t, const counted_iterator& x);</del></code></pre>
> *Effects*: Equivalent to: `return x.length == 0;`
> <pre><code><del>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator!=(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);
friend constexpr bool operator!=(
  const counted_iterator& x, default_sentinel_t y);
friend constexpr bool operator!=(
  default_sentinel_t x, const counted_iterator& y);</del></code></pre>
> <del>*Effects*: Equivalent to: `return !(x == y);`</del>
> <pre><code>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator&lt;(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</code></pre>
> *Expects*: `x` and `y` refer to elements of the same sequence ([counted.iterator]).  
> *Effects*: Equivalent to: `return y.length < x.length;`  
> [*Note*: The argument order in the *Effects*: element is reversed because length counts down, not up. —*end note*]
> <pre><code>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator&gt;(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</code></pre>
> *Effects*: Equivalent to: `return y < x;`
> <pre><code>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator&lt;=(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</code></pre>
> *Effects*: Equivalent to: `return !(y < x);`
> <pre><code>template&lt;Common&lt;I&gt; I2&gt;
  friend constexpr bool operator&gt;=(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</code></pre>
> *Effects*: Equivalent to: `return !(x < y);`
> <pre><code><ins>template&lt;Common&lt;I&gt; I2&gt; requires ThreeWayComparableWith&lt;I, I2&gt;
  friend constexpr compare_three_way_result_t&lt;I, I2&gt; operator&lt;=&gt;(
    const counted_iterator& x, const counted_iterator&lt;I2&gt;& y);</ins></code></pre>
> <ins>*Effects*: Equivalent to: `return y <=> x;`</ins>

Change 22.5.7.1 [unreachable.sentinel]:

<blockquote><pre><code>namespace std {
  struct unreachable_sentinel_t {
    template&lt;WeaklyIncrementable I&gt;
      friend constexpr bool operator==(unreachable_sentinel_t, const I&) noexcept<del>;</del>
      <ins>{ return true; }</ins>
<del>    template&lt;WeaklyIncrementable I&gt;
      friend constexpr bool operator==(const I&, unreachable_sentinel_t) noexcept;
    template&lt;WeaklyIncrementable I&gt;
      friend constexpr bool operator!=(unreachable_sentinel_t, const I&) noexcept;
    template&lt;WeaklyIncrementable I&gt;
      friend constexpr bool operator!=(const I&, unreachable_sentinel_t) noexcept;</del>
  };
}</code></pre></blockquote>

Remove 22.5.7.2 [unreachable.sentinel.cmp] (as it's now entirely defined in the synopsis):

> <pre><code><del>template&lt;WeaklyIncrementable I&gt;
  friend constexpr bool operator==(unreachable_sentinel_t, const I&) noexcept;
template&lt;WeaklyIncrementable I&gt;
  friend constexpr bool operator==(const I&, unreachable_sentinel_t) noexcept;</del></code></pre>
> <del>*Returns*: `false`.</del>
> <pre><code><del>template&lt;WeaklyIncrementable I&gt;
  friend constexpr bool operator!=(unreachable_sentinel_t, const I&) noexcept;
template&lt;WeaklyIncrementable I&gt;
  friend constexpr bool operator!=(const I&, unreachable_sentinel_t) noexcept;</del></code></pre>
> <del>*Returns*: `true`.</del>

Change 22.6.1 [istream.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class charT = char, class traits = char_traits&lt;charT&gt;,
           class Distance = ptrdiff_t&gt;
  class istream_iterator {
  public:
    using iterator_category = input_iterator_tag;
    using value_type        = T;
    using difference_type   = Distance;
    using pointer           = const T*;
    using reference         = const T&;
    using char_type         = charT;
    using traits_type       = traits;
    using istream_type      = basic_istream&lt;charT,traits&gt;;

    constexpr istream_iterator();
    constexpr istream_iterator(default_sentinel_t);
    istream_iterator(istream_type& s);
    istream_iterator(const istream_iterator& x) = default;
    ~istream_iterator() = default;
    istream_iterator& operator=(const istream_iterator&) = default;

    const T& operator*() const;
    const T* operator-&gt;() const;
    istream_iterator& operator++();
    istream_iterator  operator++(int);

    friend bool operator==(const istream_iterator& i, default_sentinel_t)<del>;</del>
    <ins>{ return !i.in_stream; }</ins>
    <ins>friend bool operator==(const istream_iterator& x, const istream_iterator& y)</ins>
    <ins>{ return x.in_stream == y.in_stream; }</ins>
    <del>friend bool operator==(default_sentinel_t, const istream_iterator& i);</del>
    <del>friend bool operator!=(const istream_iterator& x, default_sentinel_t y);</del>
    <del>friend bool operator!=(default_sentinel_t x, const istream_iterator& y);</del>

  private:
    basic_istream&lt;charT,traits&gt;* in_stream; // exposition only
    T value;                                // exposition only
  };
}</code></pre></blockquote>

Remove the specifications for `operator==` and `operator!=` from 22.6.1.2 [istream.iterator.ops]:

> <pre><code><del>template&lt;class T, class charT, class traits, class Distance&gt;
  bool operator==(const istream_iterator&lt;T,charT,traits,Distance&gt;& x,
                  const istream_iterator&lt;T,charT,traits,Distance&gt;& y);</del></code></pre>
> <del>*Returns*: `x.in_stream == y.in_stream`.</del>
> <pre><code><del>friend bool operator==(default_sentinel_t, const istream_iterator& i);
friend bool operator==(const istream_iterator& i, default_sentinel_t);</del></code></pre>
> <del>*Returns*: `!i.in_stream`.</del>
> <pre><code><del>template&lt;class T, class charT, class traits, class Distance&gt;
  bool operator!=(const istream_iterator&lt;T,charT,traits,Distance&gt;& x,
                  const istream_iterator&lt;T,charT,traits,Distance&gt;& y);
friend bool operator!=(default_sentinel_t x, const istream_iterator& y);
friend bool operator!=(const istream_iterator& x, default_sentinel_t y);</del></code></pre>
> <del>*Returns*: `!(x == y)`</del>

Change 22.6.3 [istreambuf.iterator]:

<blockquote><pre><code>namespace std {
  template&lt;class charT, class traits = char_traits&lt;charT&gt;&gt;
  class istreambuf_iterator {
  public:
    [...]
    bool equal(const istreambuf_iterator& b) const;

    <del>friend bool operator==(default_sentinel_t s, const istreambuf_iterator& i);</del>
    friend bool operator==(const istreambuf_iterator& i, default_sentinel_t s)<del>;</del>
    <ins>{ return i.equal(s); }</ins>
    <del>friend bool operator!=(default_sentinel_t a, const istreambuf_iterator& b);</del>
    <del>friend bool operator!=(const istreambuf_iterator& a, default_sentinel_t b);</del>
    <ins>friend bool operator==(const istreambuf_iterator& a, const istreambuf_iterator& b)</ins>
    <ins>{ return a.equal(b); }</ins>

  private:
    streambuf_type* sbuf_;                // exposition only
  };
}</code></pre></blockquote>

Remove the specifications for `operator==` and `operator!=` from 22.6.3.3 [istreambuf.iterator.ops]:

> <pre><code><del>template&lt;class charT, class traits&gt;
  bool operator==(const istreambuf_iterator&lt;charT,traits&gt;& a,
                  const istreambuf_iterator&lt;charT,traits&gt;& b);</del></code></pre>
> <del>*Returns*: `a.equal(b)`.</del>
> <pre><code><del>friend bool operator==(default_sentinel_t s, const istreambuf_iterator& i);
friend bool operator==(const istreambuf_iterator& i, default_sentinel_t s);</del></code></pre>
> <del>*Returns*: `i.equal(s)`.</del>
> <pre><code><del>template&lt;class charT, class traits&gt;
  bool operator!=(const istreambuf_iterator&lt;charT,traits&gt;& a,
                  const istreambuf_iterator&lt;charT,traits&gt;& b);
friend bool operator!=(default_sentinel_t a, const istreambuf_iterator& b);
friend bool operator!=(const istreambuf_iterator& a, default_sentinel_t b);</del></code></pre>
> <del>*Returns*: `!a.equal(b)`.</del>

## Clause 23: Ranges library

TBD
  
## Clause 24: Algorithms library

Change 24.4 [algorithm.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [alg.3way], three-way comparison algorithms
  <del>template&lt;class T, class U&gt;</del>
  <del>  constexpr auto compare_3way(const T& a, const U& b);</del>
  template&lt;class InputIterator1, class InputIterator2, class Cmp&gt;
    constexpr auto
      <del>lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,</del>
      <ins>lexicographical_compare_three_way(InputIterator1 b1, InputIterator1 e1,</ins>
                                   InputIterator2 b2, InputIterator2 e2,
                                   Cmp comp)
        -&gt; common_comparison_category_t&lt;decltype(comp(*b1, *b2)), strong_ordering&gt;;
  template&lt;class InputIterator1, class InputIterator2&gt;
    constexpr auto
      <del>lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,</del>
      <ins>lexicographical_compare_three_way(InputIterator1 b1, InputIterator1 e1,</ins>
                                   InputIterator2 b2, InputIterator2 e2);
  [...]
}</code></pre></blockquote>

Change 24.7.11 \[alg.3way\]:

<blockquote><del><code>template&lt;class T, class U&gt; constexpr auto compare_3way(const T& a, const U& b);</code>

<p><i>Effects</i>: Compares two values and produces a result of the strongest applicable comparison category type:
<ul>
<li> Returns a <=> b if that expression is well-formed.
<li> Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, returns strong_ordering​::​equal when a == b is true, otherwise returns strong_ordering​::​less when a < b is true, and otherwise returns strong_ordering​::​greater.
<li> Otherwise, if the expression a == b is well-formed and convertible to bool, returns strong_equality​::​equal when a == b is true, and otherwise returns strong_equality​::​nonequal.
<li>Otherwise, the function is defined as deleted.
</ul></del></blockquote>
    
Change 24.7.11 [alg.3way] paragraph 2:

<blockquote><pre><code>template&lt;class InputIterator1, class InputIterator2, class Cmp&gt;
  constexpr auto
    <del>lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,</del>
    <ins>lexicographical_compare_three_way(InputIterator1 b1, InputIterator1 e1,</ins>
                                 InputIterator2 b2, InputIterator2 e2,
                                 Cmp comp);</code></pre></blockquote>
    
Change 24.7.11 \[alg.3way\] paragraph 4:

<blockquote><pre><code>template&lt;class InputIterator1, class InputIterator2&gt;
  constexpr auto
    <del>lexicographical_compare_3way(InputIterator1 b1, InputIterator1 e1,</del>
    <ins>lexicographical_compare_three_way(InputIterator1 b1, InputIterator1 e1,</ins>
                                 InputIterator2 b2, InputIterator2 e2);</code></pre>

<i>Effects</i>: Equivalent to:
<pre><code><del>return lexicographical_compare_3way(b1, e1, b2, e2,</del>
                                    <del>[](const auto& t, const auto& u) {</del>
                                    <del>  return compare_3way(t, u);</del>
                                    <del>});</del>
<ins>return lexicographical_compare_three_way(b1, e1, b2, e2, compare_three_way());</ins></code></pre>             
</blockquote>

## Clause 25: Numeric library

TBD

## Clause 26: Time library

TBD

## Clause 27: Localization library

TBD

## Clause 28: Input/output library

TBD

## Clause 29: Regular expressions library

TBD

## Clause 30: Atomic operations library

TBD

## Clause 31: Thread support library

TBD