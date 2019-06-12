---
title: "The Mothership has Landed"
subtitle: Adding `<=>` to the Library
document: D1614R1
audience: LWG
author:
	- name: Barry Revzin
	  email: <barry.revzin@gmail.com>
---

# Introduction

The work of integrating `operator<=>` into the library has been performed by multiple different papers, each addressing a different aspect of the integration. In the interest of streamlining review by the Library Working Group, the wording has been combined into a single paper. This is that paper.

In San Diego and Kona, several papers were approved by LEWG adding functionality to the library related to comparisons. What follows is the list of those papers, in alphabetical order, with a brief description of what those papers are. The complete motivation and design rationale for each can be found within the papers themselves.

- [@P0790R2] - adding `operator<=>` to the standard library types whose behavior is not dependent on a template parameter.
- [@P0891R2] - making the `XXX_order` algorithms customization points and introducing `compare_XXX_order_fallback` algorithms that preferentially invoke the former algorithm and fallback to synthesizing an ordering from `==` and `<` (using the rules from [@P1186R1]).
- [@P1154R1] - adding the type trait `has_strong_structural_equality<T>` (useful to check if a type can be used as a non-type template parameter).
- [@P1188R0] - adding the type trait `compare_three_way_result<T>`, the concepts `ThreeWayComparable<T>` and `ThreeWayComparableWith<T,U>`, removing the algorithm `compare_3way` and replacing it with a function comparison object `compare_three_way` (i.e. the `<=>` version of `std::ranges::less`).
- [@P1189R0] - adding `operator<=>` to the standard library types whose behavior is dependent on a template parameter, removing those equality operators made redundant by [@P1185R2] and defaulting `operator==` where appropriate.
- [@P1191R0] - adding equality to several previously incomparable standard library types.
- [@P1295R0] - adding equality and `common_type` for the comparison categories.
- [@P1380R1] - extending the floating point customization points for `strong_order` and `weak_order`.

# Friendship

LEWG's unanimous preference was that the new `operator<=>`s be declared as hidden friends. It would follow therefore that we would move the `operator==`s to be declared the same way as well, since it would be pretty odd if the two different comparison operators had different semantics. 

However, a few issues have come up with this approach that are worth presenting clearly here. 

## Was well-formed, now ill-formed

Here is an example that came up while I attempted to implement these changes to measure any improvements in build time that might come up. This is a reproduction from the LLVM codebase:

```cpp
struct StringRef {
	StringRef(std::string const&); // NB: non-explicit
	operator std::string() const;  // NB: non-explicit
};
bool operator==(StringRef, StringRef);

bool f(StringRef a, std::string b) {
	return a == b; // (*)
}
```

In C++17, the marked line is well-formed. The `operator==` for `basic_string` is a non-member function template, and so would not be considered a candidate; the only viable candidate is the `operator==` taking two `StringRef`s. With the proposed changes, the `operator==` for `basic_string` becomes a non-member hidden friend, _non-template_, which makes it a candidate (converting `a` to a `string`). That candidate is ambiguous with the `operator==(StringRef, StringRef)` candidate - each requires a conversion in one argument, so the call becomes ill-formed.

Many people might consider such a type - implicitly convertible in both directions (note that `string` to `string_view` is implicit, but `string_view` to `string` is explicit) - questionable. But this is still a breaking change to consider.

## Was ill-formed, now well-formed

```cpp
bool ref_equal(std::reference_wrapper<std::string> a,
			   std::reference_wrapper<std::string> b)
{
	return a == b;
}
```

The comparisons for `std::reference_wrapper<T>` are very strange. It's not that this type is comparable based on _whether_ `T` is comparable. It's actually that this type is comparable based on _how_ `T` is comparable. We can compare `std::reference_wrapper<int>`s, but we cannot compare `std::reference_wrapper<std::string>`s because the comparisons for `basic_string` are non-member function templates. That's just weird. This change wouldn't actually resolve that weirdness generally (it wouldn't affect any user types whose comparisons are non-member function templates), but it would at least reduce it for the standard library. Arguably an improvement.

However, the more interesting case is:

```cpp
bool is42(std::variant<int, std::string> const& v) {
	return v == 42; // (*)
}
```

In C++17, the `operator==` for `variant` is a non-member function template and is thus not a viable candidate for the marked line. That check is ill-formed. With the proposed changes, the `operator==` for `variant` becomes a non-member hidden friend, _non-template_, which makes it a candidate (converting `42` to a `variant<int, string>`). Many would argue that this a fix, since both `variant<int, string> v = 42;` and `v = 42;` are already well-formed, so it is surely reasonable that `v == 42` is as well.

But we already had a proposal to do precisely this: [@P1201R0], which failed to gain consensus in LEWGI in San Diego (vote was 2-6-2-3-3). 

## Alternatives

The benefit of the hidden friend technique wasn't the only way to achieve the ultimate goal of reducing the overload candidate set. Casey Carter suggested another:

```cpp
template<class T, class Traits = char_traits<T>, class Alloc = allocator<T>> class basic_string;

namespace __foo {
  struct __tag {};

  template<class T, class Traits, class Alloc> 
  bool operator==(const basic_string<T, Traits, Alloc>&, const basic_string<T, Traits, Alloc>&);

  /* ... */
}

template<class T, class Traits, class Alloc> class basic_string : private __foo::__tag {
  /* ... */
};
```

That is, we keep `basic_string`'s comparisons as non-member function templates -- but we move them into a different namespace that is _only_ associated with `basic_string`. This is an interesting direction to take, but is novel and has some specification burden.

## Proposed Direction

Ultimately, the goal here is to add `<=>` to all the types in the standard library. While I think the goal of reducing the candidate set for comparisons with standard library types is absolutely worth pursuing, it is a completely orthogonal goal and can be addressed by a different proposal in the future.

Given that we've said that users aren't allowed to take the address of most standard library functions, Casey's proposed implementation might even be valid under today's wording for those standard library class templates whose comparisons are non-member templates, so I'd encourage implementors to experiment there.

The direction this paper is taking is the path of least resistance: keep all the comparison operators as they are. Add `<=>` in the same form that `<` appears today. With a few exceptions: those types for which adding a defaulted `operator==` would allow them to be used as non-type template parameters after [@P0732R2] (and only those types) will have their comparisons implemented as hidden friends.

# Acknowledgements

Thank you to all the paper authors that have committed time to making sure all this works: Gašper Ažman, Walter Brown, Lawrence Crowl, Tomasz Kamiński, Arthur O'Dwyer, Jeff Snyder, David Stone, and Herb Sutter. 

Thank you to Casey Carter for the tremendous wording review.

# Wording

## Clause 16: Library Introduction

Change 16.4.2.1/2 [expos.only.func]:

> The following [function is]{.rm} [are]{.add} defined for exposition only to aid in the specification of the library:

and append:

::: bq
```cpp
constexpr auto @_synth-3way_@ =
  []<class T, class U>(const T& t, const U& u)
	requires requires {
	  { t < u } -> bool;
	  { u < t } -> bool;
	}
  {
	if constexpr (ThreeWayComparableWith<T, U>) {
	  return t <=> u;
	} else {
	  if (t < u) return weak_ordering::less;
	  if (u < t) return weak_ordering::greater;
	  return weak_ordering::equivalent;
	}
  };

template<class T, class U=T>
using @_synth-3way-result_@ = decltype(@_synth-3way_@(declval<T&>(), declval<U&>()));
```
:::

Remove all of 16.4.2.3 [operators], which begins:

> [In this library, whenever a declaration is provided for an `operator!=`, `operator>`, `operator<=`, or `operator>=` for a type `T`, its requirements and semantics are as follows, unless explicitly specified otherwise.]{.rm}

## Clause 17: Language support library

Added: `compare_three_way_result`, concepts `ThreeWayComparable` and `ThreeWayComparableWith`, `compare_three_way` and `compare_XXX_order_fallback`

Changed operators for: `type_info`

Respecified: `strong_order()`, `weak_order()`, and `partial_order()`

Removed: `compare_3way()`, `strong_equal()`, and `weak_equal()`

In 17.7.2 [type.info], remove `operator!=`:

::: bq
```diff
namespace std {
  class type_info {
  public:
    virtual ~type_info();
    bool operator==(const type_info& rhs) const noexcept;
-   bool operator!=(const type_info& rhs) const noexcept;
    bool before(const type_info& rhs) const noexcept;
    size_t hash_code() const noexcept;
    const char* name() const noexcept;
    type_info(const type_info& rhs) = delete; // cannot be copied
    type_info& operator=(const type_info& rhs) = delete; // cannot be copied
  };
}
```
:::

and:

> ```cpp
> bool operator==(const type_info& rhs) const noexcept;
> ```
> [2]{.pnum} *Effects*: Compares the current object with `rhs`.
> [3]{.pnum} *Returns*: `true` if the two values describe the same type.
> 
> ```cpp
> @[bool operator!=(const type_info& rhs) const noexcept;]{.rm}@
> ```
> [4]{.pnum} [*Returns*: `!(*this == rhs)`.]{.rm}

Add into 17.11.1 [compare.syn]:

::: bq
```diff
namespace std {
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

+ // common type specializations
+ template<> struct common_type<strong_equality, partial_ordering>
+   { using type = weak_equality; };
+ template<> struct common_type<partial_ordering, strong_equality>
+   { using type = weak_equality; };
+ template<> struct common_type<strong_equality, weak_ordering>
+   { using type = weak_equality; };
+ template<> struct common_type<weak_ordering, strong_equality>
+   { using type = weak_equality; };

  // [cmp.common], common comparison category type
  template<class... Ts>
  struct common_comparison_category {
    using type = @_see below_@;
  };
  template<class... Ts>
    using common_comparison_category_t = typename common_comparison_category<Ts...>::type;

+ // [cmp.concept], concept ThreeWayComparable
+ template<class T, class Cat = partial_ordering>
+   concept ThreeWayComparable = @_see below_@;
+ template<class T, class U, class Cat = partial_ordering>
+   concept ThreeWayComparableWith = @_see below_@;
+
+ // [cmp.result], spaceship invocation result
+ template<class T, class U = T> struct compare_three_way_result;
+
+ template<class T, class U = T>
+   using compare_three_way_result_t = typename compare_three_way_result<T, U>::type;
+
+ // [cmp.object], spaceship object
+ struct compare_three_way;

  // [cmp.alg], comparison algorithms
- template<class T> constexpr strong_ordering strong_order(const T& a, const T& b);
- template<class T> constexpr weak_ordering weak_order(const T& a, const T& b);
- template<class T> constexpr partial_ordering partial_order(const T& a, const T& b);
- template<class T> constexpr strong_equality strong_equal(const T& a, const T& b);
- template<class T> constexpr weak_equality weak_equal(const T& a, const T& b);
+ inline namespace @_unspecified_@ {
+   inline constexpr @_unspecified_@ strong_order = @_unspecified_@;
+   inline constexpr @_unspecified_@ weak_order = @_unspecified_@;
+   inline constexpr @_unspecified_@ partial_order = @_unspecified_@;
+   inline constexpr @_unspecified_@ compare_strong_order_fallback = @_unspecified_@;
+   inline constexpr @_unspecified_@ compare_weak_order_fallback = @_unspecified_@;
+   inline constexpr @_unspecified_@ compare_partial_order_fallback = @_unspecified_@;
+ }
}
```
:::

Change 17.11.2.2 [cmp.weakeq]:

::: bq
``` diff
namespace std {
  class weak_equality {
    int value;  // exposition only

    // exposition-only constructor
    constexpr explicit weak_equality(eq v) noexcept : value(int(v)) {}  // exposition only

  public:
    // valid values
    static const weak_equality equivalent;
    static const weak_equality nonequivalent;

    // comparisons
    friend constexpr bool operator==(weak_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator!=(weak_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, weak_equality v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, weak_equality v) noexcept;
+   friend constexpr bool operator==(weak_equality v, weak_equality w) noexcept = default;
    friend constexpr weak_equality operator<=>(weak_equality v, @_unspecified_@) noexcept;
    friend constexpr weak_equality operator<=>(@_unspecified_@, weak_equality v) noexcept;
  };

  // valid values' definitions
  inline constexpr weak_equality weak_equality::equivalent(eq::equivalent);
  inline constexpr weak_equality weak_equality::nonequivalent(eq::nonequivalent);
}
```
:::

and remove those functions from the description:

::: bq
```cpp
constexpr bool operator==(weak_equality v, @_unspecified_@) noexcept;
@[constexpr bool operator==(_unspecified_, weak_equality v) noexcept;]{.rm}@
```
[2]{.pnum} *Returns*: `v.value == 0`.
```cpp
@[constexpr bool operator!=(weak_equality v, _unspecified_) noexcept;]{.rm}@
@[constexpr bool operator!=(_unspecified_, weak_equality v) noexcept;]{.rm}@
```
[3]{.pnum} [*Returns*: `v.value != 0`.]{.rm}
:::

Change 17.11.2.3 [cmp.strongeq]:

:::bq
```diff
namespace std {
  class strong_equality {
    int value;  // exposition only

    // exposition-only constructor
    constexpr explicit strong_equality(eq v) noexcept : value(int(v)) {}    // exposition only

  public:
    // valid values
    static const strong_equality equal;
    static const strong_equality nonequal;
    static const strong_equality equivalent;
    static const strong_equality nonequivalent;

    // conversion
    constexpr operator weak_equality() const noexcept;

    // comparisons
    friend constexpr bool operator==(strong_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator!=(strong_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, strong_equality v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, strong_equality v) noexcept;
+   friend constexpr bool operator==(strong_equality v, strong_equality w) noexcept = default;	
    friend constexpr strong_equality operator<=>(strong_equality v, @_unspecified_@) noexcept;
    friend constexpr strong_equality operator<=>(@_unspecified_@, strong_equality v) noexcept;
  };

  // valid values' definitions
  inline constexpr strong_equality strong_equality::equal(eq::equal);
  inline constexpr strong_equality strong_equality::nonequal(eq::nonequal);
  inline constexpr strong_equality strong_equality::equivalent(eq::equivalent);
  inline constexpr strong_equality strong_equality::nonequivalent(eq::nonequivalent);
}
```
:::

and remove those functions from the description:

::: bq
```cpp
constexpr bool operator==(strong_equality v, @_unspecified_@) noexcept;
@[constexpr bool operator==(_unspecified_, strong_equality v) noexcept;]{.rm}@
```
[3]{.pnum} *Returns*: `v.value == 0`.
```cpp
@[constexpr bool operator!=(strong_equality v, _unspecified_) noexcept;]{.rm}@
@[constexpr bool operator!=(_unspecified_, strong_equality v) noexcept;]{.rm}@
```
[4]{.pnum} [*Returns*: `v.value != 0`.]{.rm}
:::

Change 17.11.2.4 [cmp.partialord]:

::: bq
```diff
namespace std {
  class partial_ordering {
    int value;          // exposition only
    bool is_ordered;    // exposition only

    [...]

    // conversion
    constexpr operator weak_equality() const noexcept;

    // comparisons
    friend constexpr bool operator==(partial_ordering v, @_unspecified_@) noexcept;
-   friend constexpr bool operator!=(partial_ordering v, @_unspecified_@) noexcept;
+   friend constexpr bool operator==(partial_ordering v, partial_ordering w) noexcept = default;
    friend constexpr bool operator< (partial_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator> (partial_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator<=(partial_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator>=(partial_ordering v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, partial_ordering v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, partial_ordering v) noexcept;
    friend constexpr bool operator< (@_unspecified_@, partial_ordering v) noexcept;
    friend constexpr bool operator> (@_unspecified_@, partial_ordering v) noexcept;
    friend constexpr bool operator<=(@_unspecified_@, partial_ordering v) noexcept;
    friend constexpr bool operator>=(@_unspecified_@, partial_ordering v) noexcept;
    friend constexpr partial_ordering operator<=>(partial_ordering v, @_unspecified_@) noexcept;
    friend constexpr partial_ordering operator<=>(@_unspecified_@, partial_ordering v) noexcept;
  };

  [...]
}
```
:::

Remove just the extra `==` and `!=` operators in 17.11.2.4 [cmp.partialord]/4-5:

::: bq
```cpp
constexpr bool operator==(partial_ordering v, @_unspecified_@) noexcept;
constexpr bool operator< (partial_ordering v, @_unspecified_@) noexcept;
constexpr bool operator> (partial_ordering v, @_unspecified_@) noexcept;
constexpr bool operator<=(partial_ordering v, @_unspecified_@) noexcept;
constexpr bool operator>=(partial_ordering v, @_unspecified_@) noexcept;
```
[3]{.pnum} *Returns*: For `operator@`, `v.is_ordered && v.value @ 0`.
```cpp
@[constexpr bool operator==(_unspecified_, partial_ordering v) noexcept;]{.rm}@
constexpr bool operator< (@_unspecified_@, partial_ordering v) noexcept;
constexpr bool operator> (@_unspecified_@, partial_ordering v) noexcept;
constexpr bool operator<=(@_unspecified_@, partial_ordering v) noexcept;
constexpr bool operator>=(@_unspecified_@, partial_ordering v) noexcept;
```
[4]{.pnum} *Returns*: For `operator@`, `v.is_ordered && 0 @ v.value`.
```cpp
@[constexpr bool operator!=(partial_ordering v, _unspecified_) noexcept;]{.rm}@
@[constexpr bool operator!=(_unspecified_, partial_ordering v) noexcept;]{.rm}@
```
[5]{.pnum} [*Returns*: For `operator@`, `!v.is_ordered || v.value != 0`.]{.rm}
:::

Change 17.11.2.5 [cmp.weakord]:

::: bq
```diff
namespace std {
  class weak_ordering {
    int value;  // exposition only

    [...]
    // comparisons
    friend constexpr bool operator==(weak_ordering v, @_unspecified_@) noexcept;
+   friend constexpr bool operator==(weak_ordering v, weak_ordering w) noexcept = default;
-   friend constexpr bool operator!=(weak_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator< (weak_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator> (weak_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator<=(weak_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator>=(weak_ordering v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, weak_ordering v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, weak_ordering v) noexcept;
    friend constexpr bool operator< (@_unspecified_@, weak_ordering v) noexcept;
    friend constexpr bool operator> (@_unspecified_@, weak_ordering v) noexcept;
    friend constexpr bool operator<=(@_unspecified_@, weak_ordering v) noexcept;
    friend constexpr bool operator>=(@_unspecified_@, weak_ordering v) noexcept;
    friend constexpr weak_ordering operator<=>(weak_ordering v, @_unspecified_@) noexcept;
    friend constexpr weak_ordering operator<=>(@_unspecified_@, weak_ordering v) noexcept;
  };

  [...]
}
```
:::

Remove just the extra `==` and `!=` operators from 17.11.2.5 [cmp.weakord]/4 and /5:

::: bq
```cpp
constexpr bool operator==(weak_ordering v, @_unspecified_@) noexcept;
@[constexpr bool operator!=(weak_ordering v, _unspecified_) noexcept;]{.rm}@
constexpr bool operator< (weak_ordering v, @_unspecified_@) noexcept;
constexpr bool operator> (weak_ordering v, @_unspecified_@) noexcept;
constexpr bool operator<=(weak_ordering v, @_unspecified_@) noexcept;
constexpr bool operator>=(weak_ordering v, @_unspecified_@) noexcept;
```
[4]{.pnum} *Returns*: `v.value @ 0` for `operator@`.
```cpp
@[constexpr bool operator==(_unspecified_, weak_ordering v) noexcept;]{.rm}@
@[constexpr bool operator!=(_unspecified_, weak_ordering v) noexcept;]{.rm}@
constexpr bool operator< (@_unspecified_@, weak_ordering v) noexcept;
constexpr bool operator> (@_unspecified_@, weak_ordering v) noexcept;
constexpr bool operator<=(@_unspecified_@, weak_ordering v) noexcept;
constexpr bool operator>=(@_unspecified_@, weak_ordering v) noexcept;
```
[5]{.pnum} *Returns*: `0 @ v.value` for `operator@`.
:::

Change 17.11.2.6 [cmp.strongord]:

::: bq
```diff
namespace std {
  class strong_ordering {
    int value;  // exposition only

    [...]

    // comparisons
    friend constexpr bool operator==(strong_ordering v, @_unspecified_@) noexcept;
+   friend constexpr bool operator==(strong_ordering v, strong_ordering w) noexcept = default;
-   friend constexpr bool operator!=(strong_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator< (strong_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator> (strong_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator<=(strong_ordering v, @_unspecified_@) noexcept;
    friend constexpr bool operator>=(strong_ordering v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, strong_ordering v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, strong_ordering v) noexcept;
    friend constexpr bool operator< (@_unspecified_@, strong_ordering v) noexcept;
    friend constexpr bool operator> (@_unspecified_@, strong_ordering v) noexcept;
    friend constexpr bool operator<=(@_unspecified_@, strong_ordering v) noexcept;
    friend constexpr bool operator>=(@_unspecified_@, strong_ordering v) noexcept;
    friend constexpr strong_ordering operator<=>(strong_ordering v, @_unspecified_@) noexcept;
    friend constexpr strong_ordering operator<=>(@_unspecified_@, strong_ordering v) noexcept;
  };

  [...]
}
```
:::

Remove just the extra `==` and `!=` operators from 17.11.2.6 [cmp.strongord]/6 and /7:

::: bq
```cpp
constexpr bool operator==(strong_ordering v, @_unspecified_@) noexcept;
@[constexpr bool operator!=(strong_ordering v, _unspecified_) noexcept;]{.rm}@
constexpr bool operator< (strong_ordering v, @_unspecified_@) noexcept;
constexpr bool operator> (strong_ordering v, @_unspecified_@) noexcept;
constexpr bool operator<=(strong_ordering v, @_unspecified_@) noexcept;
constexpr bool operator>=(strong_ordering v, @_unspecified_@) noexcept;
```
[6]{.pnum} *Returns*: `v.value @ 0` for `operator@`.
```cpp
@[constexpr bool operator==(_unspecified_, strong_ordering v) noexcept;]{.rm}@
@[constexpr bool operator!=(_unspecified_, strong_ordering v) noexcept;]{.rm}@
constexpr bool operator< (@_unspecified_@, strong_ordering v) noexcept;
constexpr bool operator> (@_unspecified_@, strong_ordering v) noexcept;
constexpr bool operator<=(@_unspecified_@, strong_ordering v) noexcept;
constexpr bool operator>=(@_unspecified_@, strong_ordering v) noexcept;
```
[7]{.pnum} *Returns*: `0 @ v.value` for `operator@`.
:::

Add a new subclause [cmp.concept] "concept ThreeWayComparable":

::: bq
::: add
```
template <typename T, typename Cat>
  concept @_compares-as_@ = // exposition only
    Same<common_comparison_category_t<T, Cat>, Cat>;
```	

```
template<class T, class U>
  concept @_partially-ordered-with_@ = // exposition only
    requires(const remove_reference_t<T>& t,
             const remove_reference_t<U>& u) {
      { t < u } -> Boolean;
      { t > u } -> Boolean;
      { t <= u } -> Boolean;
      { t >= u } -> Boolean;
      { u < t } -> Boolean;
      { u > t } -> Boolean;
      { u <= t } -> Boolean;
      { u >= t } -> Boolean;    
    };
```

[1]{.pnum} Let `t` and `u` be lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>` respectively. _`partially-ordered-with`_`<T, U>` is satisfied only if:

- [1.1]{.pnum} `t < u`, `t <= u`, `t > u`, `t >= u`, `u < t`, `u <= t`, `u > t`, and `u >= t` have the same domain.
- [1.2]{.pnum} `bool(t < u) == bool(u > t)`
- [1.3]{.pnum} `bool(u < t) == bool(t > u)`
- [1.4]{.pnum} `bool(t <= u) == bool(u >= t)`
- [1.5]{.pnum} `bool(u <= t) == bool(t >= u)`

```
template <typename T, typename Cat = partial_ordering>
  concept ThreeWayComparable =
    @_weakly-equality-comparable-with_@<T, T> &&
    (!ConvertibleTo<Cat, partial_ordering> || @_partially-ordered-with_@<T, T>) &&
    requires(const remove_reference_t<T>& a,
             const remove_reference_t<T>& b) {
      { a <=> b } -> @_compares-as_@<Cat>;
    };
```

[2]{.pnum} Let `a` and `b` be lvalues of type `const remove_reference_t<T>`. `T` and `Cat` model `ThreeWayComparable<T, Cat>` only if:

- [2.1]{.pnum} `(a <=> b == 0) == bool(a == b)`.
- [2.2]{.pnum} `(a <=> b != 0) == bool(a != b)`.
- [2.3]{.pnum} `((a <=> b) <=> 0)` and `(0 <=> (b <=> a))` are equal
- [2.4]{.pnum} If `Cat` is convertible to `strong_equality`, `T` models `EqualityComparable` ([concept.equalitycomparable]).
- [2.5]{.pnum} If `Cat` is convertible to `partial_ordering`:
	- [2.5.1]{.pnum} `(a <=> b < 0) == bool(a < b)`.
    - [2.5.2]{.pnum} `(a <=> b > 0) == bool(a > b)`.
    - [2.5.3]{.pnum} `(a <=> b <= 0) == bool(a <= b)`.
    - [2.5.4]{.pnum} `(a <=> b >= 0) == bool(a >= b)`.
- [2.5.5]{.pnum} If `Cat` is convertible to `strong_ordering`, `T` models `StrictTotallyOrdered` ([concept.stricttotallyordered]).

```
template <typename T, typename U,
          typename Cat = partial_ordering>
  concept ThreeWayComparableWith = 
    @_weakly-equality-comparable-with_@<T, U> &&
    (!ConvertibleTo<Cat, partial_ordering> || @_partially-ordered-with_@<T, U>) &&
    ThreeWayComparable<T, Cat> &&
    ThreeWayComparable<U, Cat> &&
    CommonReference<const remove_reference_t<T>&, const remove_reference_t<U>&> &&
    ThreeWayComparable<
      common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>,
      Cat> &&
    requires(const remove_reference_t<T>& t,
             const remove_reference_t<U>& u) {
      { t <=> u } -> @_compares-as_@<Cat>;
      { u <=> t } -> @_compares-as_@<Cat>;
    };
```

[3]{.pnum} Let `t` and `u` be lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`, respectively. Let `C` be `common_reference_t<const remove_reference_t<T>&, const remove_reference_t<U>&>`. `T`, `U`, and `Cat` model `ThreeWayComparableWith<T, U, Cat>` only if:

- [3.1]{.pnum} `t <=> u` and `u <=> t` have the same domain.
- [3.2]{.pnum} `((t <=> u) <=> 0)` and `(0 <=> (u <=> t))` are equal
- [3.3]{.pnum} `(t <=> u == 0) == bool(t == u)`.
- [3.4]{.pnum} `(t <=> u != 0) == bool(t != u)`.
- [3.5]{.pnum} `Cat(t <=> u) == Cat(C(t) <=> C(u))`.
- [3.6]{.pnum} If `Cat` is convertible to `strong_equality`, `T` and `U` model `EqualityComparableWith<T, U>` ([concepts.equalitycomparable]).
- [3.7]{.pnum} If `Cat` is convertible to `partial_ordering`:
	- [3.7.1]{.pnum} `(t <=> u < 0) == bool(t < u)`
    - [3.7.2]{.pnum} `(t <=> u > 0) == bool(t > u)`
    - [3.7.3]{.pnum} `(t <=> u <= 0) == bool(t <= u)`
    - [3.7.4]{.pnum} `(t <=> u >= 0) == bool(t >= u)`
- [3.8]{.pnum} If `Cat` is convertible to `strong_ordering`, `T` and `U` model `StrictTotallyOrderedWith<T, U>` ([concepts.stricttotallyordered]).

:::
:::


---
references:
---