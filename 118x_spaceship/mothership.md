Title: The Mothership Has Landed - Adding `<=>` to library
Document-Number: DxxxxR0
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
- [P1380R1](https://wg21.link/p1380r1) - extending the floating point customization points for `strong_order` and `weak_order`.

LEWG's unanimous preference was that `operator<=>`s be declared as hidden friends.

# Wording

## Clause 15: Library Introduction

Remove 15.4.2.3 [operators].

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
  
  // [cmp.common], common comparison category type  
  template&lt;class... Ts&gt;
  struct common_comparison_category {
    using type = see below;
  };
  template&lt;class... Ts&gt;
    using common_comparison_category_t = typename common_comparison_category&lt;Ts...&gt;::type;  
  
  <ins>// [cmp.threewaycomparable], concept ThreeWayComparable</ins>
  <ins>template&lt;class T, class Cat = weak_equality&gt;</ins>
    <ins>concept ThreeWayComparable = <i>see below</i>;</ins>
  <ins>template&lt;class T, class U, class Cat = weak_equality&gt;</ins>
    <ins>concept ThreeWayComparableWith = <i>see below</i>;</ins>
  
  <ins>// [cmp.???], compare_three_way_result</ins>
  <ins>template&lt;class T, class U = T&gt; struct compare_three_way_result;</ins>
  
  <ins>template&lt;class T, class U = T&gt;</ins>
  <ins>  using compare_three_way_result_t = typename compare_three_way_result&lt;T, U&gt;::type;</ins>
  
  <ins>// [cmp.???], compare_three_way</ins>
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

Add a new clause \[cmp.threewaycomparable\]. We don't need to add any new semantic constraints. The requirement that the `<=>`s used have to be equality-preserving is picked up through [concepts.equality] already.

> 
    :::cpp
    template <typename T, typename Cat>
      concept compares-as = // exposition only
        Same<common_comparison_category_t<T, Cat>, Cat>;
> 
    template <typename T, typename Cat=std::weak_equality>
      concept ThreeWayComparable =
        requires(const remove_reference_t<T>& a,
                 const remove_reference_t<T>& b) {
          { a <=> b } -> compares-as<Cat>;
        };
> 
    template <typename T, typename U,
              typename Cat=std::weak_equality>
      concept ThreeWayComparableWith = 
        ThreeWayComparable<T, Cat> &&
        ThreeWayComparable<U, Cat> &&
        CommonReference<const remove_reference_t<T>&, const remove_reference_t<U>&> &&
        ThreeWayComparable<
          common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>,
          Cat> &&
        requires(const remove_reference_t<T>& t,
                 const remove_reference_t<U>& u) {
          { t <=> u } -> compares-as<Cat>;
          { u <=> t } -> compares-as<Cat>;
        };

Add a new specification for `compare_three_way_result` in a new clause after 16.11.3 \[cmp.common\] named \[cmp.???\]:

> The behavior of a program that adds specializations for the `compare_three_way_result` template defined in this subclause is undefined.

> For the `compare_three_way_result` type trait applied to the types `T` and `U`, let `t` and `u` denote lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`. If the expression `t <=> u` is well formed, the member *typedef-name* `type` shall equal `decltype(t <=> u)`. Otherwise, there shall be no member `type`.

Add a new specification for `compare_three_way` in a new clause named [cmp.???]:

> In this subclause, `BUILTIN_PTR_3WAY(T, U)` for types `T` and `U` is a boolean constant expression. `BUILTIN_PTR_3WAY(T, U)` is `true` if and only if `<=>` in the expression `declval<T>() <=> declval<U>()` resolves to a built-in operator comparing pointers.

> There is an implementation-defined strict total ordering over all pointer values of a given type. This total ordering is consistent with the partial order imposed by the builtin operator `<=>`.

> 
    :::cpp
    struct compare_three_way {
      template<class T, class U>
        requires ThreeWayComparableWith<T,U> || BUILTIN_PTR_3WAY(T, U)
      constexpr auto operator()(T&& t, U&&u) const;
>      
      using is_transparent = unspecified;
    };

> *Expects*: If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`, the conversion sequences from both `T` and `U` to `P` shall be equality-preserving ([concepts.equality]).

> *Effects*: 
> 
> - If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`: returns `strong_ordering::less` if (the converted value of) `t` precedes `u` in the implementation-defined strict total order over pointers of type `P`, `strong_ordering::greater` if `u` precedes `t`, and otherwise `strong_ordering::equal`.
> - Otherwise, equivalent to: `return std::forward<T>(t) <=> std::forward<U>(u);`

Add a new specification-only `kebab-case` algorithm and type trait. Note that this is subtly different from the [P1186R1](https://wg21.link/p1186r1) algorithm `3WAY<weak_ordering>` since that one synthesizes a `weak_ordering` from both `==` and `<`, whereas this one (for backwards compatibility with C++17) only uses `<`.

> Given types `T` and `U` and lvalues of those types `t` and `u`, define <code><i>synth-3way</i>(t, u)</code> as follows:
> 
> - `t <=> u` when `U` models `ThreeWayComparableWith<T>`;
> - Otherwise, equivalent to:
> 
        :::cpp
        if (t < u) return weak_ordering::less;
        if (u < t) return weak_ordering::greater;
        return weak_ordering::equivalent;
> 
> Define <code><i>synth-3way-type</i>&lt;T,U&gt;</code> as follows:
> 
> - `compare_three_way_result_t<T,U>` when `U` models `ThreeWayComparableWith<T>`;
> - Otherwise, `weak_ordering`
>
> Define <code><i>synth-3way-type</i>&lt;T&gt;</code> as <code><i>synth-3way-type</i>&lt;T,T&gt;</code>.
        
Replace the entirety of 16.11.4 [cmp.alg]

> <pre><code><del>template&lt;class T&gt; constexpr strong_ordering strong_order(const T& a, const T& b);</del></code></pre>
> <del>*Effects*: Compares two values and produces a result of type `strong_ordering`:</del>  
> 
> - <del>If numeric_­limits<T>::is_iec559 is true, returns a result of type strong_ordering that is consistent with the totalOrder operation as specified in ISO/IEC/IEEE 60559.</del>
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

> <ins>A _comparison customization point object_ is a customization point object ([customization.point.object]) that converts its return value to a specific comparison category, known as its _associated comparison category_.</ins>

> <ins>The function call operator template of a comparison customization point object type `T` with associated comparison category `C` is equivalent to:</ins>

> <blockquote class="ins"><pre><code>template &lt;typename U&gt;
C operator()(const U&, const U&) const;</code></pre></blockquote>
    
> <ins>with additional requirements specified in that customization point object's definition. [ *Note*: This means that attempting to invoke a comparison customization point object with expressions having different decayed type will fail. *-end note*] </ins>

> <ins>The name `std::strong_order` denotes a comparison customization point object with associated comparison category `strong_ordering`. The expression `std::strong_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent to the following:
> 
> - <ins>`strong_order(E, F)` if it is a valid expression, convertible to `strong_ordering`, with overload resolution performed in a context that does not include a declaration of `std::strong_order`.</ins>
> - <ins>Otherwise, if `T` is a floating point type, then:</ins>
>       - <ins>If `numeric_limits<T>::is_iec559` is `true`, yields a value of type `strong_ordering` that is consistent with both the ordering observed by `T`'s comparison operators and the `totalOrder` operation as specified in ISO/IEC/IEEE 60599.</ins>
>       - <ins>Otherwise, yields a value of type `strong_ordering` that is a strong order and is consistent with ordering observed by `T`'s comparison operators.</ins>
> - <ins>Otherwise, `E <=> F` if it is a valid expression convertible to `strong_ordering`.</ins>
> - <ins>Otherwise, `std::strong_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::strong_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>

> <ins>The name `std::weak_order` denotes a comparison customization point object with associated comparison category `weak_ordering`. The expression `std::weak_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent to the following:</ins>
> 
> - <ins>`weak_order(E, F)` if it is a valid expression, convertible to `weak_ordering`, with overload resolution performed in a context that does not include a declaration of `std::weak_order`.</ins>
> - <ins>Otherwise, if `T` is a floating point type, then:</ins>
>       - <ins>If `numeric_limits<T>::is_iec559` is `true`, yields a value of type `weak_ordering` that is consistent with both the ordering observed by `T`'s comparison operators and `strong_order`, which has the following equivalence classes, ordered from lesser to greater:</ins>
>           - <ins>Together, all negative NaN values</ins>
>           - <ins>Negative infinity</ins>
>           - <ins>Each normal negative value</ins>
>           - <ins>Each subnormal negative value</ins>
>           - <ins>Together, both zero values</ins>
>           - <ins>Each subnormal positive value</ins>
>           - <ins>Each normal positive value</ins>
>           - <ins>Positive infinity</ins>
>           - <ins>Together, all positive NaN values</ins>
>       - <ins>Otherwise, yields a value of type `weak_ordering` that is a weak order and is consistent with both the ordering observed by `T`'s comparison operators and `strong_order`.
> - <ins>Otherwise, `std::strong_order(E, F)` if it is a valid expression.</ins>
> - <ins>Otherwise, `E <=> F` if it is a valid expression convertible to `weak_ordering`.</ins>
> - <ins>Otherwise, `std::weak_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::weak_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>

> <ins>The name `std::partial_order` denotes a comparison customization point object with associated comparison category `partial_ordering`. The expression `std::partial_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent to the following:</ins>
> 
> - <ins>`partial_order(E, F)` if it is a valid expression, convertible to `partial_ordering`, with overload resolution performed in a context that does not include a declaration of `std::partial_order`.</ins>
> - <ins>Otherwise, `std::weak_order(E, F)` if it is a valid expression.</ins>
> - <ins>Otherwise, `E <=> F` if it is a valid expression convertible to `partial_ordering`.</ins>
> - <ins>Otherwise, `std::partial_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::partial_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]</ins>
> 

> <ins>The name `std::compare_strong_order_fallback` denotes a comparison customization point object with associated comparison category `strong_ordering`. The expression `std::compare_strong_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent to:</ins>
> 
> - <ins>`std::strong_order(E, F)` if it is a valid expression.</ins>
> - <ins>Otherwise, `3WAY<strong_ordering>(E, F)` ([class.spaceship]) if it is a valid expression.</ins>
> - <ins>Otherwise, `std::compare_strong_order_fallback(E, F)` is ill-formed.</ins>

> <ins>The name `std::compare_weak_order_fallback` denotes a comparison customization point object with associated comparison category `weak_ordering`. The expression `std::compare_weak_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent to:</ins>
> 
> - <ins>`std::weak_order(E, F)` if it is a valid expression.</ins>
> - <ins>Otherwise, `3WAY<weak_ordering>(E, F)` ([class.spaceship]) if it is a valid expression.</ins>
> - <ins>Otherwise, `std::compare_weak_order_fallback(E, F)` is ill-formed.</ins>

> <ins>The name `std::compare_partial_order_fallback` denotes a customization point object with associated comparison category `partial_ordering`. The expression `std::compare_partial_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent to:</ins>
> 
> - <ins>`std::partial_order(E, F)` if it is a valid expression.</ins>
> - <ins>Otherwise, `3WAY<partial_ordering>(E, F)` ([class.spaceship]) if it is a valid expression.</ins>
> - <ins>Otherwise, `std::compare_partial_order_fallback(E, F)` is ill-formed.</ins>

## Clause 17: Concepts Library

Nothing.

## Clause 18: Diagnostics Library

Changed operators for:

- `error_category`
- `error_code`
- `error_condition`

Change 18.5.1 [system_error.syn]

<blockquote><pre><code>namespace std {
  [...]
  // 18.5.5, comparison functions
  bool operator==(const error_code& lhs, const error_code& rhs) noexcept;
  bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;
  <del>bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;</del>
  bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;
  <del>bool operator!=(const error_code& lhs, const error_code& rhs) noexcept;</del>
  <del>bool operator!=(const error_code& lhs, const error_condition& rhs) noexcept;</del>
  <del>bool operator!=(const error_condition& lhs, const error_code& rhs) noexcept;</del>
  <del>bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept;</del>
  <del>bool operator< (const error_code& lhs, const error_code& rhs) noexcept;</del>
  <del>bool operator< (const error_condition& lhs, const error_condition& rhs) noexcept;</del>
  <ins>strong_ordering operator<=>(const error_code& lhs, const error_code& rhs) noexcept;</ins>
  <ins>strong_ordering operator<=>(const error_condition& lhs, const error_condition& rhs) noexcept;</ins>
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
> <ins>[Note: `compare_three_way` (???.???) provides a total ordering for pointers. —end note]</ins>

Change 18.5.5 [syserr.compare]

> <pre><code>bool operator==(const error_code& lhs, const error_code& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`
> <pre><code>bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category().equivalent(lhs.value(), rhs) || rhs.category().equivalent(lhs, rhs.value())`
> <pre><code><del>bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;</del></code></pre>
> <del>*Returns*: `rhs.category().equivalent(rhs.value(), lhs) || lhs.category().equivalent(rhs, lhs.value())`</del>
> <pre><code>bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;</code></pre>
> *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`
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
> <pre><code><ins>strong_ordering operator<=>(const error_code& lhs, const error_code& rhs) noexcept;</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();</code></pre></blockquote>
> <pre><code><ins>strong_ordering operator<=>(const error_condition& lhs, const error_condition& rhs) noexcept;</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();</code></pre></blockquote>

## Clause 19: General utilities library

Changed operators for:

- `pair`, `tuple`, `optional`, `variant`, `monostate`, `bitset`, `allocator`, `unique_ptr`, `shared_ptr`, `memory_resource`, `polymorphic_allocator`, `scoped_allocator_adaptor`, `function`, `type_index`

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
  <ins>friend constexpr auto operator<=>(const pair&, const pair&)</ins>
  <ins>  -> common_comparison_category_t&lt;<i>synth-3way-type</i>&lt;T1&gt;, <i>synth-3way-type</i>&lt;T2&gt;&gt;;</ins>
};</code></pre>
</blockquote>
> [...]
> <pre><code>constexpr void swap(pair& p) noexcept(<i>see below</i>);</code></pre>
> *Requires*: `first` shall be swappable with (15.5.3.2) `p.first` and `second` shall be swappable with `p.second`.  
> *Effects*: Swaps `first` with `p.first` and `second` with `p.second`.  
> *Remarks*: The expression inside noexcept is equivalent to:
`is_nothrow_swappable_v<first_type> && is_nothrow_swappable_v<second_type>`
> <pre><code><ins>constexpr auto operator<=>(const pair& lhs, const pair& rhs)</ins>
<ins>  -> common_comparison_category_t&lt;<i>synth-3way-type</i>&lt;T1&gt;, <i>synth-3way-type</i>&lt;T2&gt;&gt;;</ins></code></pre>
> <ins>*Effects*: Equivalent to:</ins>
<blockquote class="ins"><pre><code>if (auto c = <i>synth-3way</i>(lhs.first, rhs.first); c != 0) return c;
return <i>synth-3way</i>(lhs.second, rhs.second);</code></pre></blockquote>

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
  <ins>template&lt;class... UTypes&gt;</ins>
  <ins>  friend constexpr auto operator<=>(const tuple&, const tuple&lt;UTypes...&gt;&)</ins>
  <ins>    -> common_comparison_category_t&lt;<i>synth-3way-type</i>&lt;Types, UTypes&gt;...&gt;;</ins>
};</code></pre></blockquote>

Change 19.5.3.8 [tuple.rel]:

> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;
  constexpr bool operator==(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <pre><code><ins>template&lt;class... UTypes&gt;</ins>
<ins>  constexpr bool operator==(const tuple&, const tuple&lt;UTypes...&gt;&)</ins></code></pre>
> *Requires*: For all `i`, where `0 <= i` and <code>i &lt; sizeof...(<del>TTypes</del> <ins>Types</ins>)</code>, `get<i>(t) == get<i>(u)` is a valid expression returning a type that is convertible to `bool`. <code>sizeof...(<del>TTypes</del> <ins>Types</ins>) == sizeof...(UTypes)</code>.  
> *Returns*: `true` if `get<i>(t) == get<i>(u)` for all `i`, otherwise `false`. For any two zero-length tuples `e` and `f`, `e == f` returns `true`.  
> *Effects*: The elementary comparisons are performed in order from the zeroth index upwards. No comparisons or element accesses are performed after the first equality comparison that evaluates to `false`.
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;<del>
> <del>  constexpr bool operator!=(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <del>*Returns*: `!(t == u)`.</del>
> <pre><code><del>template&lt;class... TTypes, class... UTypes&gt;</del>
<del>constexpr bool operator&lt;(const tuple&lt;TTypes...&gt;& t, const tuple&lt;UTypes...&gt;& u);</del></code></pre>
> <pre><code><ins>template&lt;class... UTypes&gt;</ins>
<ins>  constexpr auto operator<=>(const tuple& t, const tuple&lt;UTypes...&gt;& u)</ins>
<ins>    -> common_comparison_category_t&lt;<i>synth-3way-type</i>&lt;Types, UTypes&gt;...&gt;;</ins></code></pre>
> *Requires*: For all `i`, where `0 <= i` and `i < sizeof...(Types)`, <del>both `get<i>(t) < get<i>(u)` and `get<i>(u) < get<i>(t)` are valid expressions returning types that are convertible to `bool`</del> <ins><code><i>synth-3way</i>(get&lt;i&gt;(t), get&lt;i&gt;(u))</code></ins> is a valid expression. <code>sizeof...(<del>TTypes</del> <ins>Types</ins>) == sizeof...(UTypes)</code>.  
> <del>*Returns*: The result of a lexicographical comparison between `t` and `u`. The result is defined as:
`(bool)(get<0>(t) < get<0>(u)) || (!(bool)(get<0>(u) < get<0>(t)) && ttail < utail)`, where
<code>r<sub>tail</sub></code> for some tuple `r` is a tuple containing all but the first element of `r`. For any two zero-length tuples `e` and `f`, `e < f` returns `false`.</del>  
> <ins>*Effects*: Performs a lexicographical comparison between `t` and `u`. Equivalent to:</ins>
> <blockquote class="ins"><pre><code>auto c = <i>synth-3way</i>(get<0>(t), get<0>(u));
return (c != 0) ? c : (t<sub>tail</sub> <=> u<sub>tail</sub>);</code></pre></blockquote>
> <ins>For any two zero-length tuples `e` and `f`, `e <=> f` returns `strong_ordering::equal`.</ins>
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
  // [optional.relops], relational operators
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

  <del>// [optional.nullops], comparison with nullopt</del>
  <del>template&lt;class T&gt; constexpr bool operator==(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator==(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator!=(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator!=(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&lt;(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&lt;(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&gt;(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&gt;(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&lt;=(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&lt;=(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&gt;=(const optional&lt;T&gt;&, nullopt_t) noexcept;</del>
  <del>template&lt;class T&gt; constexpr bool operator&gt;=(nullopt_t, const optional&lt;T&gt;&) noexcept;</del>

  // [optional.comp_with_t], comparison with T
  template&lt;class T, class U&gt; constexpr bool operator==(const optional&lt;T&gt;&, const U&);
  <del>template&lt;class T, class U&gt; constexpr bool operator==(const T&, const optional&lt;U&gt;&);</del>
  template&lt;class T, class U&gt; constexpr bool operator!=(const optional&lt;T&gt;&, const U&);
  <del>template&lt;class T, class U&gt; constexpr bool operator!=(const T&, const optional&lt;U&gt;&);</del>
  template&lt;class T, class U&gt; constexpr bool operator&lt;(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;=(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&lt;=(const T&, const optional&lt;U&gt;&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;=(const optional&lt;T&gt;&, const U&);
  template&lt;class T, class U&gt; constexpr bool operator&gt;=(const T&, const optional&lt;U&gt;&);

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
    <ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
    <ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
    <ins>    operator&lt;=&gt;(const optional&, const optional&lt;U&gt;&);</ins>

    <ins>// [optional.nullops]</ins>
    <ins>friend constexpr bool operator==(const optional&, nullopt_t);</ins>
    <ins>friend constexpr strong_ordering operator&lt;=&gt;(const optional&, nullopt_t);</ins>
    
    <ins>// [optional.comp_with_t], comparison with T</ins>
    <ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
    <ins>  friend constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
    <ins>    operator&lt;=&gt;(const optional&, const U&);</ins>
    
  private:
    T *val;         // exposition only
  };
}</code></pre></blockquote>

Change 19.6.6 [optional.relops]:

> [...]
> <pre><code>template&lt;class T, class U&gt; constexpr bool operator>=(const optional&lt;T&gt;& x, const optional&lt;U&gt;& y);</code></pre>
> *Requires*: The expression `*x >= *y` shall be well-formed and its result shall be convertible to `bool`.  
> *Returns*: If `!y`, `true`; otherwise, if `!x`, `false`; otherwise `*x >= *y`.  
> *Remarks*: Specializations of this function template for which `*x >= *y` is a core constant expression shall be `constexpr` functions.  
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
<ins>  constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
<ins>    operator&lt;=&gt;(const optional& x, const optional&lt;U&gt;& y);</ins></code></pre>
> <ins>*Returns*: If `x && y`, `*x <=> *y`; otherwise `bool(x) <=> bool(y)`.</ins>  
> <ins>*Remarks*: Specializations of this function template for which `*x <=> *y` is a core constant expression shall be `constexpr` functions.</ins>

Change 19.6.7 [optional.nullops]:

> <pre><code><del>template&lt;class T&gt; constexpr bool operator==(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del>
<del>template&lt;class T&gt; constexpr bool operator==(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del>
<ins>constexpr bool operator==(const optional& x, nullopt_t) noexcept;</ins></code></pre>
> *Returns*: `!x`.
> <pre><code><del>template&lt;class T&gt; constexpr bool operator!=(const optional&lt;T&gt;& x, nullopt_t) noexcept;
template&lt;class T&gt; constexpr bool operator!=(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `bool(x)`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&lt;(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del></code></pre>
> <del>*Returns*: `false`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&lt;(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `bool(x)`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&gt;(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del></code></pre>
> <del>*Returns*: `bool(x)`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&gt;(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `false`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&lt;=(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del></code></pre>
> <del>*Returns*: `!x`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&lt;=(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `true`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&gt;=(const optional&lt;T&gt;& x, nullopt_t) noexcept;</del></code></pre>
> <del>*Returns*: `true`.</del>
> <pre><code><del>template&lt;class T&gt; constexpr bool operator&gt;=(nullopt_t, const optional&lt;T&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `!x`.</del>
> <pre><code><ins>constexpr strong_ordering operator&lt;=&gt;(const optional& x, nullopt_t) noexcept;</ins></code></pre>
> <ins>*Returns*: `bool(x) <=> false`.</ins>

Change 19.6.8 [optional.comp_with_t]:

> <pre><code>template&lt;class T, class U&gt; constexpr bool operator==(const optional&lt;T&gt;& x, const U& v);</code></pre>
> *Requires*: The expression `*x == v` shall be well-formed and its result shall be convertible to `bool`. [*Note*: `T` need not be `Cpp17EqualityComparable`. —*end note*]  
> *Effects*: Equivalent to: `return bool(x) ? *x == v : false;`  
> <pre><code><del>template&lt;class T, class U&gt; constexpr bool operator==(const T& v, const optional&lt;U&gt;& x);</del></code></pre>
> <del>*Requires*: The expression `v == *x` shall be well-formed and its result shall be convertible to `bool`.</del>  
> <del>*Effects*: Equivalent to: `return bool(x) ? v == *x : false;`</del>  
> <pre><code>template&lt;class T, class U&gt; constexpr bool operator!=(const optional&lt;T&gt;& x, const U& v);</code></pre>
> *Requires*: The expression `*x != v `shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x != v : true;`
> <pre><code><del>template&lt;class T, class U&gt; constexpr bool operator!=(const T& v, const optional&lt;U&gt;& x);</del></code></pre>
> <del>*Requires*: The expression `v != *x` shall be well-formed and its result shall be convertible to `bool`.</del>  
> <del>*Effects*: Equivalent to: `return bool(x) ? v != *x : true;`</del>
> <pre><code>template&lt;class T, class U&gt; constexpr bool operator&lt;(const optional&lt;T&gt;& x, const U& v);</code></pre>
> *Requires*: The expression `*x < v` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? *x < v : true;`  
> [...]
> <pre><code>template&lt;class T, class U&gt; constexpr bool operator&gt;=(const T& v, const optional&lt;U&gt;& x);</code></pre>
> *Requires*: The expression `v >= *x` shall be well-formed and its result shall be convertible to `bool`.  
> *Effects*: Equivalent to: `return bool(x) ? v >= *x : true;`
> <pre><code><ins>template&lt;ThreeWayComparableWith&lt;T&gt; U&gt;</ins>
<ins>  constexpr compare_three_way_result_t&lt;T,U&gt;</ins>
<ins>    operator&lt;=&gt;(const optional& x, const U& v);</ins></code></pre>
> <ins>*Effects*: Equivalent to: `return bool(x) ? *x <=> v : strong_ordering::less;`</ins>

Change 19.7.2 [variant.syn]:

<blockquote><pre><code>namespace std {
  [...]
  // [variant.monostate], class monostate
  struct monostate;

  // [variant.monostate.relops], monostate relational operators
  constexpr bool operator==(monostate, monostate) noexcept;
  <del>constexpr bool operator!=(monostate, monostate) noexcept;</del>
  <del>constexpr bool operator<(monostate, monostate) noexcept;</del>
  <del>constexpr bool operator>(monostate, monostate) noexcept;</del>
  <del>constexpr bool operator<=(monostate, monostate) noexcept;</del>
  <del>constexpr bool operator>=(monostate, monostate) noexcept;</del>
  <ins>constexpr strong_ordering operator<=>(monostate, monostate) noexcept;</ins>
  
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
    <ins>friend constexpr common_comparison_category_t&lt;compare_three_way_result_t&lt;Types&gt;...&gt;</ins>
    <ins>  operator&lt;=&gt;(const variant&, const variant&)</ins>
    <ins>    requires (ThreeWayComparable&lt;Types&gt; && ...);</ins>
  };
}</code></pre></blockquote>

Insert at the end of 19.7.6 [variant.relops]:

> <pre><code><ins>constexpr common_comparison_category_t&lt;compare_three_way_result_t&lt;Types&gt;...&gt;</ins>
<ins>  operator&lt;=&gt;(const variant& v, const variant& w)</ins>
<ins>    requires (ThreeWayComparable&lt;Types&gt; && ...);</ins></code></pre>
> <ins>*Returns*: Let `c` be `(v.index() + 1) <=> (w.index() + 1)`. If `c != 0`, `c`. Otherwise, `get<i> <=> get<i>(w)` with `i` being `v.index()`.</ins>

Change 19.7.9 [variant.monostate.relops]:

<blockquote><pre><code>constexpr bool operator==(monostate, monostate) noexcept { return true; }
<del>constexpr bool operator!=(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator<(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator>(monostate, monostate) noexcept { return false; }</del>
<del>constexpr bool operator<=(monostate, monostate) noexcept { return true; }</del>
<del>constexpr bool operator>=(monostate, monostate) noexcept { return true; }</del>
<ins>constexpr strong_ordering operator<=>(monostate, monostate) noexcept { return strong_ordering::equal; }</ins></code></pre>

[<i>Note</i>: monostate objects have only a single state; they thus always compare equal. —<i>end note</i>]</blockquote>

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
  template&lt;class T, class U&gt;
    bool operator==(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;
  <del>template&lt;class T, class U&gt;</del>
    <del>bool operator!=(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</del>
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
    bool operator&gt;=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del>

<del>  template&lt;class T, class D&gt;
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
  // [util.smartptr.shared.cmp], shared_­ptr comparisons
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

  // [util.smartptr.shared.spec], shared_­ptr specialized algorithms
  template&lt;class T&gt;
    void swap(shared_ptr&lt;T&gt;& a, shared_ptr&lt;T&gt;& b) noexcept;
  [...]    
}</code></pre></blockquote>

Change 19.10.10.2 [allocator.globals]:

> <pre><code>template&lt;class T, class U&gt;
  bool operator==(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</code></pre>
> *Returns*: `true`.
> <del><pre><code>template&lt;class T, class U&gt;
  bool operator!=(const allocator&lt;T&gt;&, const allocator&lt;U&gt;&) noexcept;</code></pre></del>
> <del>*Returns*: `false`.</del>

Change 19.11.1.2 [unique.ptr.single]:

<blockquote><pre><code>namespace std {
  template&lt;class T, class D = default_delete&lt;T&gt;&gt; class unique_ptr {
  public:
    using pointer      = see below;
    using element_type = T;
    using deleter_type = D;
    [...]
    // disable copy from lvalue
    unique_ptr(const unique_ptr&) = delete;
    unique_ptr& operator=(const unique_ptr&) = delete;
    
    <ins>// [unique.ptr.special] Specialized algorithms</ins>
    <ins>template&lt;class T2, class D2&gt;</ins>
      <ins>friend bool operator==(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y);</ins>
    <ins>template&lt;class T2, class D2&gt;</ins>
      <ins>friend strong_ordering operator&lt;=&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y);</ins>  

    <ins>friend bool operator==(const unique_ptr& x, nullptr_t) noexcept;</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const unique_ptr& x, nullptr_t);</ins>
  };
}</code></pre></blockquote>

Change 19.11.1.5 [unique.ptr.special]:

> <pre><code>template&lt;class T, class D&gt; void swap(unique_ptr&lt;T, D&gt;& x, unique_ptr&lt;T, D&gt;& y) noexcept;</code></pre>
> *Remarks*: This function shall not participate in overload resolution unless `is_swappable_v<D>` is `true`.
> *Effects*: Calls `x.swap(y)`.
> <pre><code>template&lt;<del>class T1, class D1</del>, class T2, class D2&gt;
  bool operator==(const unique_ptr<del>&lt;T1, D1&gt;</del>& x, const unique_ptr&lt;T2, D2&gt;& y);</code></pre>
> *Returns*: `x.get() == y.get()`.
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator!=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `x.get() != y.get()`.</del>
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator&lt;(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Requires*: Let CT denote `common_type_t<typename unique_ptr<T1, D1>::pointer, typename unique_ptr<T2, D2>::pointer>` Then the specialization `less<CT>` shall be a function object type that induces a strict weak ordering on the pointer values.</del>  
> <del>*Returns*: `less<CT>()(x.get(), y.get())`.</del>  
> <del>*Remarks*: If `unique_ptr<T1, D1>::pointer` is not implicitly convertible to `CT` or `unique_ptr<T2, D2>::pointer` is not implicitly convertible to `CT`, the program is ill-formed.</del>
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator&gt;(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `y < x`.</del>
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator&lt;=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `!(y < x)`.</del>
> <pre><code><del>template&lt;class T1, class D1, class T2, class D2&gt;
  bool operator&gt;=(const unique_ptr&lt;T1, D1&gt;& x, const unique_ptr&lt;T2, D2&gt;& y);</del></code></pre>
> <del>*Returns*: `!(x < y)`.</del>
> <pre><code><ins>template&lt;class T2, class D2&gt;</ins>
<ins>strong_ordering operator&lt;=&gt;(const unique_ptr& x, const unique_ptr&lt;T2, D2&gt;& y);</ins></code></pre>
> <ins>*Requires*: Let `CT` denote `common_type_t<typename unique_ptr<T, D>::pointer, typename unique_ptr<T2, D2>::pointer>`. Then `BUILTIN_PTR_3WAY(CT, CT)` ([cmp.???]) is `true`.</ins>  
> <ins>*Returns*: `compare_three_way()(static_cast<CT>(x.get()), static_cast<CT>(y.get()))`.</ins>  
> <ins>*Remarks*: If `unique_ptr<T, D>::pointer` is not implicitly convertible to `CT` or `unique_ptr<T2, D2>::pointer` is not implicitly convertible to `CT`, the program is ill-formed.</ins>
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator==(const unique_ptr&lt;T, D&gt;& x, nullptr_t) noexcept;
template&lt;class T, class D&gt;
  bool operator==(nullptr_t, const unique_ptr&lt;T, D&gt;& x) noexcept;</del></code></pre>
> <pre><code><ins>bool operator==(const unique_ptr& x, nullptr_t) noexcept;</ins></code></pre>
> *Returns*: `!x`.
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator!=(const unique_ptr&lt;T, D&gt;& x, nullptr_t) noexcept;
template&lt;class T, class D&gt;
  bool operator!=(nullptr_t, const unique_ptr&lt;T, D&gt;& x) noexcept;</del></code></pre>
> <del>*Returns*: `(bool)x`.</del>
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator&lt;(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
template&lt;class T, class D&gt;
  bool operator&lt;(nullptr_t, const unique_ptr&lt;T, D&gt;& x);</del></code></pre>
> <del>*Requires*: The specialization `less<unique_ptr<T, D>::pointer>` shall be a function object type that induces a strict weak ordering on the pointer values.</del>  
> <del>*Returns*: The first function template returns `less<unique_ptr<T, D>::pointer>()(x.get(), nullptr)` The second function template returns `less<unique_ptr<T, D>::pointer>()(nullptr, x.get())`</del>
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator&gt;(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
template&lt;class T, class D&gt;
  bool operator&gt;(nullptr_t, const unique_ptr&lt;T, D&gt;& x);</del></code></pre>
> <del>*Returns*: The first function template returns `nullptr < x`. The second function template returns `x < nullptr`.</del>
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator&lt;=(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
template&lt;class T, class D&gt;
  bool operator&lt;=(nullptr_t, const unique_ptr&lt;T, D&gt;& x);</del></code></pre>
> <del>*Returns*: The first function template returns `!(nullptr < x)`. The second function template returns `!(x < nullptr)`.</del>
> <pre><code><del>template&lt;class T, class D&gt;
  bool operator&gt;=(const unique_ptr&lt;T, D&gt;& x, nullptr_t);
template&lt;class T, class D&gt;
  bool operator&gt;=(nullptr_t, const unique_ptr&lt;T, D&gt;& x);</del></code></pre>
> <del>*Returns*: The first function template returns `!(x < nullptr)`. The second function template returns `!(nullptr < x)`.</del>
> <pre><code><ins>strong_ordering operator&lt;=&gt;(const unique_ptr& x, nullptr_t);</ins></code></pre>
> <ins>*Requires*: `BUILTIN_PTR_3WAY(pointer, pointer)` shall be `true`.</ins>  
> <ins>*Returns*: `compare_three_way()(x.get(), nullptr)`.</ins> 

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
      <ins>friend bool operator==(const shared_ptr& a, const shared_ptr&lt;U&gt;& b) noexcept;</ins>
    <ins>template&lt;class Ugt;</ins>
      <ins>friend strong_ordering operator&lt;=&gt;(const shared_ptr& a, const shared_ptr&lt;U&gt;& b) noexcept;</ins>
      
    <ins>friend bool operator==(const shared_ptr&, nullptr_t) noexcept;</ins>
    <ins>friend strong_ordering operator&lt;=&gt;(const shared_ptr&, nullptr_t) noexcept;</ins>
  };
}</code></pre></blockquote>

Change 19.11.3.7 [util.smartptr.shared.cmp]:

> <pre><code>template&lt;<del>class T, </del>class U&gt;
  bool operator==(const shared_ptr<del>&lt;T&gt;</del>& a, const shared_ptr&lt;U&gt;& b) noexcept;</code></pre>
> *Returns*: `a.get() == b.get()`.
> <pre><code><del>template&lt;class T, class U&gt;
  bool operator&lt;(const shared_ptr&lt;T&gt;& a, const shared_ptr&lt;U&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `less<>()(a.get(), b.get())`.</del>  
> <pre><code><ins>template&lt;class U&gt;</ins>
<ins>  strong_ordering operator&lt;=&gt;(const shared_ptr& a, const shared_ptr&lt;U&gt; b) noexcept;</ins></code></pre>
> <ins>*Returns*: `compare_three_way()(a.get(), b.get())`.</ins>  
> [*Note*: Defining a comparison function allows `shared_ptr` objects to be used as keys in associative containers. —*end note*]
> <pre><code><del>template&lt;class T&gt;</del>
  bool operator==(const shared_ptr<del>&lt;T&gt;</del>& a, nullptr_t) noexcept;
<del>template&lt;class T&gt;</del>
<del>  bool operator==(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> *Returns*: `!a`.
> <pre><code><del>template&lt;class T&gt;
  bool operator!=(const shared_ptr&lt;T&gt;& a, nullptr_t) noexcept;
template&lt;class T&gt;
  bool operator!=(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> <del>*Returns*: `(bool)a`.</del>
> <pre><code><del>template&lt;class T&gt;
  bool operator&lt;(const shared_ptr&lt;T&gt;& a, nullptr_t) noexcept;
template&lt;class T&gt;
  bool operator&lt;(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> <del>*Returns*: The first function template returns `less<typename shared_ptr<T>::element_type*>()(a.get(), nullptr)`
The second function template returns `less<typename shared_ptr<T>::element_type*>()(nullptr, a.get())`.</del>
> <pre><code><ins>strong_ordering operator&lt;=&gt;(const shared_ptr& a, nullptr_t) noexcept;</ins></code></pre>
> <ins>*Returns*: `compare_three_way()(a.get(), nullptr)`.</ins>
> <pre><code><del>template&lt;class T&gt;
  bool operator&gt;(const shared_ptr&lt;T&gt;& a, nullptr_t) noexcept;
template&lt;class T&gt;
  bool operator&gt;(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> <del>*Returns*: The first function template returns `nullptr < a`. The second function template returns `a < nullptr`.</del>
> <pre><code><del>template&lt;class T&gt;
  bool operator&lt;=(const shared_ptr&lt;T&gt;& a, nullptr_t) noexcept;
template&lt;class T&gt;
  bool operator&lt;=(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> <del>*Returns*: The first function template returns `!(nullptr < a)`. The second function template returns `!(a < nullptr)`.</del>
> <pre><code><del>template&lt;class T&gt;
  bool operator&gt;=(const shared_ptr&lt;T&gt;& a, nullptr_t) noexcept;
template&lt;class T&gt;
  bool operator&gt;=(nullptr_t, const shared_ptr&lt;T&gt;& a) noexcept;</del></code></pre>
> <del>*Returns*: The first function template returns `!(a < nullptr)`. The second function template returns `!(nullptr < a)`.

Change 19.12.1 [mem.res.syn]:

<blockquote><pre><code>namespace std::pmr {
  // [mem.res.class], class memory_­resource
  class memory_resource;

  bool operator==(const memory_resource& a, const memory_resource& b) noexcept;
  <del>bool operator!=(const memory_resource& a, const memory_resource& b) noexcept;</del>

  // [mem.poly.allocator.class], class template polymorphic_­allocator
  template&lt;class Tp&gt; class polymorphic_allocator;

  template&lt;class T1, class T2&gt;
    bool operator==(const polymorphic_allocator&lt;T1&gt;& a,
                    const polymorphic_allocator&lt;T2&gt;& b) noexcept;
  <del>template&lt;class T1, class T2&gt;</del>
  <del>  bool operator!=(const polymorphic_allocator&lt;T1&gt;& a,</del>
  <del>                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</del>

  // [mem.res.global], global memory resources
  memory_resource* new_delete_resource() noexcept;
  [...]
}</code></pre></blockquote>

Change 19.12.2.3 [mem.res.eq]:

> <pre><code>bool operator==(const memory_resource& a, const memory_resource& b) noexcept;</code></pre>
> *Returns*: `&a == &b || a.is_equal(b)`.
> <pre><code><del>bool operator!=(const memory_resource& a, const memory_resource& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.12.3.3 [mem.poly.allocator.eq]:

> <pre><code>template&lt;class T1, class T2&gt;
  bool operator==(const polymorphic_allocator&lt;T1&gt;& a,
                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</code></pre>
> *Returns*: `*a.resource() == *b.resource()`.
> <pre><code><del>template&lt;class T1, class T2&gt;</del>
<del>  bool operator!=(const polymorphic_allocator&lt;T1&gt;& a,</del>
<del>                  const polymorphic_allocator&lt;T2&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.13.1 [allocator.adaptor.syn]:

<blockquote><pre><code>namespace std {
  // class template scoped allocator adaptor
  template&lt;class OuterAlloc, class... InnerAlloc&gt;
    class scoped_allocator_adaptor;

  // [scoped.adaptor.operators], scoped allocator operators
  template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;
    bool operator==(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,
                    const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;
  <del>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;</del>
  <del>  bool operator!=(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,</del>
  <del>                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</del>
}</code></pre></blockquote>

Change 19.13.5 [scoped.adaptor.operators]:

> <pre><code>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;
  bool operator==(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,
                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</code></pre>
> *Returns*: If `sizeof...(InnerAllocs)` is zero,  
> `a.outer_allocator() == b.outer_allocator()`  
> otherwise  
> `a.outer_allocator() == b.outer_allocator() && a.inner_allocator() == b.inner_allocator()`
> <pre><code><del>template&lt;class OuterA1, class OuterA2, class... InnerAllocs&gt;</del>
<del>  bool operator!=(const scoped_allocator_adaptor&lt;OuterA1, InnerAllocs...&gt;& a,</del>
<del>                  const scoped_allocator_adaptor&lt;OuterA2, InnerAllocs...&gt;& b) noexcept;</del></code></pre>
> <del>*Returns*: `!(a == b)`.</del>

Change 19.14.1 [functional.syn]

<blockquote><pre><code>namespace std {
  [...]
  template&lt;class R, class... ArgTypes&gt;
    bool operator==(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>
  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>

  // [func.search], searchers
  [...]
}</code></pre></blockquote>  

Change 19.14.16.2 [func.wrap.func]:

<blockquote><pre><code>namespace std {
  template&lt;class&gt; class function; // not defined
  [...]
  // [func.wrap.func.nullptr], Null pointer comparisons
  template&lt;class R, class... ArgTypes&gt;
    bool operator==(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;&, nullptr_t) noexcept;</del>

  <del>template&lt;class R, class... ArgTypes&gt;</del>
  <del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;&) noexcept;</del>
  [...]
}</code></pre></blockquote>  

Change 19.14.16.2.6 [func.wrap.func.nullptr]:

> <pre><code>template&lt;class R, class... ArgTypes&gt;
  bool operator==(const function&lt;R(ArgTypes...)&gt;& f, nullptr_t) noexcept;
<del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator==(nullptr_t, const function&lt;R(ArgTypes...)&gt;& f) noexcept;</del></code></pre>
> *Returns*: `!f`.

> <pre><code><del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator!=(const function&lt;R(ArgTypes...)&gt;& f, nullptr_t) noexcept;</del>
<del>template&lt;class R, class... ArgTypes&gt;</del>
<del>  bool operator!=(nullptr_t, const function&lt;R(ArgTypes...)&gt;& f) noexcept;</del></code></pre>
> <del>*Returns*: `(bool)f`.</del>

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
    <ins>friend bool operator==(const basic_string& lhs, const basic_string& rhs);</ins>
    <ins>friend bool operator==(const basic_string& lhs, const charT* rhs);</ins>
    <ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const basic_string& rhs);</ins>
    <ins>friend <i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const charT* rhs);</ins>
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
  
<ins>bool operator==(const basic_string& lhs, const basic_string& rhs);</ins>
<ins>bool operator==(const basic_string& lhs, const charT* rhs);</ins>

<del>template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs,
                  const basic_string&lt;charT, traits, Allocator&gt;& rhs) noexcept;
template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const charT* lhs, const basic_string&lt;charT, traits, Allocator&gt;& rhs);
template&lt;class charT, class traits, class Allocator&gt;
  bool operator!=(const basic_string&lt;charT, traits, Allocator&gt;& lhs, const charT* rhs);</code></pre></blockquote>

> <ins>*Effects*: Equivalent to `return basic_string_view<charT, traits>(lhs) == basic_string_view<charT, traits>(rhs);`</ins>

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

<ins><i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const basic_string& rhs);</ins>  
<ins><i>see below</i> operator&lt;=&gt;(const basic_string& lhs, const charT* rhs);</ins>  
</code></pre></blockquote>

> *Effects*: <del>Let `op` be the operator</del>. Equivalent to:
  <pre><code>return basic_string_view&lt;charT, traits&gt;(lhs) <del>op</del> <ins>&lt;=&gt;</ins> basic_string_view&lt;charT, traits&gt;(rhs);</code></pre>

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
  <ins>friend constexpr bool operator==(basic_string_view, basic_string_view) noexcept;</ins>
  <ins>friend constexpr <i>see below</i> operator&lt;=&gt;(basic_string_view, basic_string_view) noexcept;</ins>
  [...]
};</code></pre></blockquote>

Remove the entirety of 20.4.3 [string.view.comparison]. The proposed two hidden friend declarations satisfy the requirements without needing extra wording. Replace it with the following:

> <pre><code>constexpr bool operator==(basic_string_view lhs, basic_string_view rhs) noexcept;</code></pre>
> *Returns:* `lhs.compare(rhs) == 0`.
> <pre><code>constexpr <i>see below</i> operator&lt;=&gt;(basic_string_view, basic_string_view) noexcept;</code></pre>
> Let `R` denote the type `traits::comparison_category` if it exists, otherwise `R` is `weak_ordering`.  
> *Returns:* `static_cast<R>(lhs.compare(rhs) <=> 0)`.
  
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
<li> Otherwise, if the expressions a == b and a < b are each well-formed and convertible to bool, returns strong_­ordering​::​equal when a == b is true, otherwise returns strong_­ordering​::​less when a < b is true, and otherwise returns strong_­ordering​::​greater.
<li> Otherwise, if the expression a == b is well-formed and convertible to bool, returns strong_­equality​::​equal when a == b is true, and otherwise returns strong_­equality​::​nonequal.
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

