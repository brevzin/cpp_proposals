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

Strike [operators].

## Clause 16: Language support library

Changed types:

- `std::type_info`

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

## Clause 17: Concepts Library

Nothing.

## Clause 18: Diagnostics Library

Changed types:

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
> <code><pre><del>bool operator<(const error_condition& lhs, const error_condition& rhs) noexcept;</del></pre></code>
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

TBD