---
title: "The Mothership has Landed"
subtitle: Adding `<=>` to the Library
document: D1614R1
audience: LWG
date: today
author:
	- name: Barry Revzin
	  email: <barry.revzin@gmail.com>
---

# Revision History

[@P1614R0] took the route of adding the new comparison operators as hidden friends. This paper instead preserves the current method of declaring comparisons - typically as non-member functions. See [friendship](#friendship) for a more thorough discussion. 

Additionally, R0 used the `3WAY`{.default}`<R>` wording from [@P1186R1], which was removed in the subsequent [@D1186R2] - so the relevant wording for the fallback objects was changed as well.

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
::: add
```
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

    [...]

    // comparisons
    friend constexpr bool operator==(weak_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator!=(weak_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, weak_equality v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, weak_equality v) noexcept;
+   friend constexpr bool operator==(weak_equality v, weak_equality w) noexcept = default;
    friend constexpr weak_equality operator<=>(weak_equality v, @_unspecified_@) noexcept;
    friend constexpr weak_equality operator<=>(@_unspecified_@, weak_equality v) noexcept;
  };

  [...]
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

    [...]

    // comparisons
    friend constexpr bool operator==(strong_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator!=(strong_equality v, @_unspecified_@) noexcept;
-   friend constexpr bool operator==(@_unspecified_@, strong_equality v) noexcept;
-   friend constexpr bool operator!=(@_unspecified_@, strong_equality v) noexcept;
+   friend constexpr bool operator==(strong_equality v, strong_equality w) noexcept = default;	
    friend constexpr strong_equality operator<=>(strong_equality v, @_unspecified_@) noexcept;
    friend constexpr strong_equality operator<=>(@_unspecified_@, strong_equality v) noexcept;
  };

  [...]
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

Add a new subclause [cmp.result] "spaceship invocation result":

::: add
> The behavior of a program that adds specializations for the `compare_three_way_result` template defined in this subclause is undefined.

> For the `compare_three_way_result` type trait applied to the types `T` and `U`, let `t` and `u` denote lvalues of types `const remove_reference_t<T>` and `const remove_reference_t<U>`, respectively. If the expression `t <=> u` is well-formed when treated as an unevaluted operand ([expr.context]), the member *typedef-name* `type` denotes the type `decltype(t <=> u)`. Otherwise, there is no member `type`.
:::

Add a new subclause [cmp.object] "spaceship object":

::: add
::: bq
[1]{.pnum} In this subclause, `BUILTIN_PTR_3WAY(T, U)` for types `T` and `U` is a boolean constant expression. `BUILTIN_PTR_3WAY(T, U)` is `true` if and only if `<=>` in the expression `declval<T>() <=> declval<U>()` resolves to a built-in operator comparing pointers.

```
struct compare_three_way {
  template<class T, class U>
	requires ThreeWayComparableWith<T,U> || BUILTIN_PTR_3WAY(T, U)
  constexpr auto operator()(T&& t, U&& u) const;
  
  using is_transparent = @_unspecified_@;
};
```

[2]{.pnum} *Expects*: If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`, the conversion sequences from both `T` and `U` to `P` are equality-preserving ([concepts.equality]).

[3]{.pnum} *Effects*: 
 
- [3.1]{.pnum} If the expression `std::forward<T>(t) <=> std::forward<U>(u)` results in a call to a built-in operator `<=>` comparing pointers of type `P`: returns `strong_ordering::less` if (the converted value of) `t` precedes `u` in the implementation-defined strict total order ([range.cmp]) over pointers of type `P`, `strong_ordering::greater` if `u` precedes `t`, and otherwise `strong_ordering::equal`.
- [3.2]{.pnum} Otherwise, equivalent to: `return std::forward<T>(t) <=> std::forward<U>(u);`

[4]{.pnum} In addition to being available via inclusion of the `<compare>` header, the class `compare_three_way` is available when the header `<functional>` is included.
:::
:::

Replace the entirety of 17.11.4 [cmp.alg]. This section had the original design for `strong_order()`, `weak_order()`, `partial_order()`, `strong_equal()`, and `weak_equal()`. The new wording makes them CPOs. 

::: bq
::: add
[1]{.pnum} The name `strong_order` denotes a customization point object ([customization.point.object]). The expression `strong_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:

- [1.1]{.pnum} If the decayed types of `E` and `F` differ, `strong_order(E, F)` is ill-formed. 
- [1.2]{.pnum} Otherwise, `strong_ordering(strong_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::strong_order`.
- [1.3]{.pnum} Otherwise, if the decayed type `T` of `E` and `F` is a floating point type, yields a value of type `strong_ordering` that is consistent with the ordering observed by `T`'s comparison operators, and if `numeric_limits<T>::is_iec559` is `true` is additionally consistent with the totalOrder operation as specified in ISO/IEC/IEEE 60599.
- [1.4]{.pnum} Otherwise, `strong_ordering(E <=> F)` if it is a well-formed expression.
- [1.5]{.pnum} Otherwise, `strong_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `strong_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]

[2]{.pnum} The name `weak_order` denotes a customization point object ([customization.point.object]). The expression `weak_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:

- [2.1]{.pnum} If the decayed types of `E` and `F` differ, `weak_order(E, F)` is ill-formed. 
- [2.2]{.pnum} Otherwise, `weak_ordering(weak_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::weak_order`.
- [2.3]{.pnum} Otherwise, if the decayed type `T` of `E` and `F` is a floating point type, yields a value of type `weak_ordering` that is consistent with the ordering observed by `T`'s comparison operators and `strong_order`, and if `numeric_liits<T>::is_iec559` is `true` is additionally consistent with the following equivalence classes, ordered from lesser to greater:
	- [2.3.1]{.pnum} Together, all negative NaN values
	- [2.3.2]{.pnum} Negative infinity
	- [2.3.3]{.pnum} Each normal negative value
	- [2.3.4]{.pnum} Each subnormal negative value
	- [2.3.5]{.pnum} Together, both zero values
	- [2.3.6]{.pnum} Each subnormal positive value
	- [2.3.7]{.pnum} Each normal positive value
	- [2.3.8]{.pnum} Positive infinity
	- [2.3.9]{.pnum} Together, all positive NaN values
- [2.4]{.pnum} Otherwise, `weak_ordering(strong_order(E, F))` if it is a well-formed expression.
- [2.5]{.pnum} Otherwise, `weak_ordering(E <=> F)` if it is a well-formed expression.
- [2.6]{.pnum} Otherwise, `weak_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::weak_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]

[3]{.pnum} The name `partial_order` denotes a customization point object ([customization.point.object]). The expression `partial_order(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to the following:

- [3.1]{.pnum} If the decayed types of `E` and `F` differ, `partial_order(E, F)` is ill-formed.
- [3.2]{.pnum} Otherwise, `partial_ordering(partial_order(E, F))` if it is a well-formed expression with overload resolution performed in a context that does not include a declaration of `std::partial_order`.
- [3.3]{.pnum} Otherwise, `partial_ordering(weak_order(E, F))` if it is a well-formed expression.
- [3.4]{.pnum} Otherwise, `partial_ordering(E <=> F)` if it is a well-formed expression.
- [3.5]{.pnum} Otherwise, `partial_order(E, F)` is ill-formed. [*Note*: This case can result in substitution failure when `std::partial_order(E, F)` appears in the immediate context of a template instantiation. —*end note*]

[4]{.pnum} The name `compare_strong_order_fallback` denotes a comparison customization point ([customization.point.object]) object. The expression `compare_strong_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:

- [4.1]{.pnum} If the decayed types of `E` and `F` differ, `compare_strong_order_fallback(E, F)` is ill-formed.
- [4.2]{.pnum} Otherwise, `strong_order(E, F)` if it is a well-formed expression.
- [4.3]{.pnum} Otherwise, if the expressions `E == F` and `E < F` are each well-formed and convertible to bool, `(E == F) ? strong_ordering::equal : ((E < F) ? strong_ordering::less : strong_ordering::greater` except that `E` and `F` are only evaluated once.
- [4.4]{.pnum} Otherwise, `compare_strong_order_fallback(E, F)` is ill-formed.

[5]{.pnum} The name `compare_weak_order_fallback` denotes a customization point object ([customization.point.object]). The expression `compare_weak_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:

- [5.1]{.pnum} If the decayed types of `E` and `F` differ, `compare_weak_order_fallback(E, F)` is ill-formed.
- [5.2]{.pnum} Otherwise, `weak_order(E, F)` if it is a well-formed expression.
- [5.3]{.pnum} Otherwise, if the expressions `E == F` and `E < F` are each well-formed and convertible to bool, `(E == F) ? weak_ordering::equal : ((E < F) ? weak_ordering::less : weak_ordering::greater` except that `E` and `F` are only evaluated once.
- [5.4]{.pnum} Otherwise, `compare_weak_order_fallback(E, F)` is ill-formed.

[6]{.pnum} The name `compare_partial_order_fallback` denotes a customization point object ([customization.point.object]). The expression `compare_partial_order_fallback(E, F)` for some subexpressions `E` and `F` is expression-equivalent ([defns.expression-equivalent]) to:

- [6.1]{.pnum} If the decayed types of `E` and `F` differ, `compare_partial_order_fallback(E, F)` is ill-formed.
- [6.2]{.pnum} Otherwise, `partial_order(E, F)` if it is a well-formed expression.
- [6.3]{.pnum} Otherwise, if the expressions `E == F` and `E < F` are each well-formed and convertible to bool, `(E == F) ? partial_ordering::equivalent : ((E < F) ? partial_ordering::less : ((F < E) ? weak_ordering::greater : weak_ordering::unordered))` except that `E` and `F` are only evaluated once.
- [6.4]{.pnum} Otherwise, `compare_partial_order_fallback(E, F)` is ill-formed.

:::
:::

Change 17.13.1 [coroutine.syn]:

::: bq
```diff
namespace std {
  [...]
  // 17.13.5 noop coroutine
  noop_coroutine_handle noop_coroutine() noexcept;

  // 17.13.3.6 comparison operators:
  constexpr bool operator==(coroutine_handle<> x, coroutine_handle<> y) noexcept;
- constexpr bool operator!=(coroutine_handle<> x, coroutine_handle<> y) noexcept;
- constexpr bool operator<(coroutine_handle<> x, coroutine_handle<> y) noexcept;
- constexpr bool operator>(coroutine_handle<> x, coroutine_handle<> y) noexcept;
- constexpr bool operator<=(coroutine_handle<> x, coroutine_handle<> y) noexcept;
- constexpr bool operator>=(coroutine_handle<> x, coroutine_handle<> y) noexcept;
+ constexpr strong_ordering operator<=>(coroutine_handle x, coroutine_handle y) noexcept;

  // 17.13.6 trivial awaitables
  [...]
}
```
:::

Replace the `<` in 17.13.3.6 [coroutine.handle.compare] with the new `<=>`:

::: bq
```cpp
constexpr bool operator==(coroutine_handle<> x, coroutine_handle<> y) noexcept;
```
[1]{.pnum} *Returns*: `x.address() == y.address()`.

::: rm
```
constexpr bool operator<(coroutine_handle<> x, coroutine_handle<> y) noexcept;
```
[2]{.pnum} *Returns*: `less<>()(x.address(), y.address())`.
:::

::: {.addu}
```
constexpr strong_ordering operator<=>(coroutine_handle x, coroutine_handle y) noexcept;
```
[3]{.pnum} *Returns*: `compare_three_way()(x.address(), y.address())`.
:::
:::

## Clause 18: Concepts Library

No changes.

## Clause 19: Diagnostics Library

Changed operators for: `error_category`, `error_code`, and `error_condition`

Change 19.5.1 [system.error.syn]:

::: bq
```diff
namespace std {
  [...]
  // [syserr.compare], comparison functions
  bool operator==(const error_code& lhs, const error_code& rhs) noexcept;
  bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;
- bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;
  bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;
- bool operator!=(const error_code& lhs, const error_code& rhs) noexcept;
- bool operator!=(const error_code& lhs, const error_condition& rhs) noexcept;
- bool operator!=(const error_condition& lhs, const error_code& rhs) noexcept;
- bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept;
- bool operator< (const error_code& lhs, const error_code& rhs) noexcept;
- bool operator< (const error_condition& lhs, const error_condition& rhs) noexcept;
+ strong_ordering operator<=>(const error_code& lhs, const error_code& rhs) noexcept;
+ strong_ordering operator<=>(const error_condition& lhs, const error_condition& rhs) noexcept;
  [...]  
}
```
:::

Change 19.5.2.1 [syserr.errcat.overview]:

::: bq
```diff
namespace std {
  class error_category {
  public:
    [...]

    bool operator==(const error_category& rhs) const noexcept;
-   bool operator!=(const error_category& rhs) const noexcept;
-   bool operator< (const error_category& rhs) const noexcept;
+   strong_ordering operator<=>(const error_category& rhs) const noexcept;
  };

  const error_category& generic_category() noexcept;
  const error_category& system_category() noexcept;
}
```
:::

Change 19.5.2.3 [syserr.errcat.nonvirtuals]:

::: bq
```cpp
bool operator==(const error_category& rhs) const noexcept;
```
[1]{.pnum} *Returns*: `this == &rhs`.

::: rm
```
bool operator!=(const error_category& rhs) const noexcept;
```
[2]{.pnum} *Returns*: `!(*this == rhs)`.
```
bool operator<(const error_category& rhs) const noexcept;
```
[3]{.pnum} *Returns*: `less<const error_category*>()(this, &rhs)`.
:::

::: {.addu}
```
strong_ordering operator<=>(const error_category& rhs) const noexcept;
```
[4]{.pnum} *Returns*: `compare_three_way()(this, &rhs)`.
:::

[*Note*: [`less`]{.rm} [`compare_three_way` ([cmp.object])]{.add} provides a total ordering for pointers. —*end note*]

:::

Change 19.5.5 [syserr.compare]:

::: bq
```cpp
bool operator==(const error_code& lhs, const error_code& rhs) noexcept;
```
[1]{.pnum} *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`
```cpp
bool operator==(const error_code& lhs, const error_condition& rhs) noexcept;
```
[2]{.pnum} *Returns*: `lhs.category().equivalent(lhs.value(), rhs) || rhs.category().equivalent(lhs, rhs.value())`

::: rm
```
bool operator==(const error_condition& lhs, const error_code& rhs) noexcept;
```
[3]{.pnum} *Returns*: `rhs.category().equivalent(rhs.value(), lhs) || lhs.category().equivalent(rhs, lhs.value())`
:::
```cpp
bool operator==(const error_condition& lhs, const error_condition& rhs) noexcept;
```
[4]{.pnum} *Returns*: `lhs.category() == rhs.category() && lhs.value() == rhs.value()`

::: rm
```
bool operator!=(const error_code& lhs, const error_code& rhs) noexcept;
bool operator!=(const error_code& lhs, const error_condition& rhs) noexcept;
bool operator!=(const error_condition& lhs, const error_code& rhs) noexcept;
bool operator!=(const error_condition& lhs, const error_condition& rhs) noexcept;
```
[5]{.pnum} *Returns*: `!(lhs == rhs)`.
```
bool operator<(const error_code& lhs, const error_code& rhs) noexcept;
```
[6]{.pnum} *Returns*: `lhs.category() < rhs.category() || (lhs.category() == rhs.category() && lhs.value() < rhs.value())`
```
bool operator<(const error_condition& lhs, const error_condition& rhs) noexcept;
```
[7]{.pnum} *Returns*: `lhs.category() < rhs.category() || (lhs.category() == rhs.category() && lhs.value() < rhs.value())`
:::

::: {.addu}

```
strong_ordering operator<=>(const error_code& lhs, const error_code& rhs) noexcept;
```
[8]{.pnum} *Effects*: Equivalent to: 

::: bq
```
if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();
```
:::
```
strong_ordering operator<=>(const error_condition& lhs, const error_condition& rhs) noexcept;
```
[9]{.pnum} *Effects*: Equivalent to:

::: bq
```
if (auto c = lhs.category() <=> rhs.category(); c != 0) return c;
return lhs.value() <=> rhs.value();
```
:::
:::
:::

## Clause 20: General utilities library

Change 20.15.7.6 [meta.trans.other] to add special rule for comparison categories:

::: bq
[3]{.pnum} Note A: For the `common_type` trait applied to a template parameter pack `T` of types, the member `type` shall be either defined or not present as follows:

- [3.1]{.pnum} If `sizeof...(T)` is zero, there shall be no member `type`.
- [3.2]{.pnum} If `sizeof...(T)` is one, let `T0` denote the sole type
constituting the pack `T`. The member typedef-name `type` shall denote the
same type, if any, as `common_type_t<T0, T0>`; otherwise there shall be no
member `type`.
- [3.3]{.pnum} If `sizeof...(T)` is two, let the first and second types
constituting `T` be denoted by `T1` and `T2`, respectively, and let `D1` and
`D2` denote the same types as `decay_t<T1>` and `decay_t<T2>`, respectively.
	- [3.3.1]{.pnum} If `is_same_v<T1, D1>` is `false` or `is_same_v<T2, D2>` is `false`,
	let `C` denote the same type, if any, as `common_type_t<D1, D2>`.
    - [3.3.2]{.pnum} [*Note*: None of the following will apply if there is a specialization
	`common_type<D1, D2>`. —*end note*]
	- [3.3.*]{.pnum} [Otherwise, if both `D1` and `D2` denote comparison
	category type ([cmp.categories.pre]), let `C` denote common comparison
	type ([class.spaceship]) of `D1` and `D2`.]{.add}
    - [3.3.3]{.pnum} Otherwise, if `decay_t<decltype(false ? declval<D1>() : declval<D2>())>`
	denotes a valid type, let `C` denote that type.
    - [3.3.4]{.pnum} Otherwise, if `COND_RES(CREF(D1), CREF(D2))` denotes a type,
	let `C` denote the type `decay_t<COND_RES(CREF(D1), CREF(D2))>`.
    In either case, the member typedef-name type shall denote the same type,
	if any, as `C`. Otherwise, there shall be no member `type`.
- [3.4]{.pnum} If `sizeof...(T)` is greater than two, let `T1`, `T2`, and `R`, respectively, denote the first, second, and (pack of) remaining types constituting `T`. Let `C` denote the same type, if any, as `common_type_t<T1, T2>`. If there is such a type `C`, the member typedef-name `type` shall denote the same type, if any, as `common_type_t<C, R...>`. Otherwise, there shall be no member `type`.
:::

---
references:
---