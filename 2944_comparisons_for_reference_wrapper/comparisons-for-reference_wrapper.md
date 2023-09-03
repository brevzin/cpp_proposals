---
title: "Comparisons for `reference_wrapper`"
document: P2944R2
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

Since [@P2944R1], added section on [ambiguity](#ambiguity-issues) and updated wording accordingly.

Since [@P2944R0], fixed the wording

# Introduction

Typically in libraries, wrapper types are comparable when their underlying types are comparable. `tuple<T>` is equality comparable when `T` is. `optional<T>` is equality comparable when `T` is. `variant<T>` is equality comparable when `T` is.

But `reference_wrapper<T>` is a peculiar type in this respect. It looks like this:

::: bq
```cpp
template<class T> class reference_wrapper {
public:
  // types
  using type = T;

  // [refwrap.const], constructors
  template<class U>
    constexpr reference_wrapper(U&&) noexcept($see below$);
  constexpr reference_wrapper(const reference_wrapper& x) noexcept;

  // [refwrap.assign], assignment
  constexpr reference_wrapper& operator=(const reference_wrapper& x) noexcept;

  // [refwrap.access], access
  constexpr operator T&() const noexcept;
  constexpr T& get() const noexcept;

  // [refwrap.invoke], invocation
  template<class... ArgTypes>
    constexpr invoke_result_t<T&, ArgTypes...> operator()(ArgTypes&&...) const
      noexcept(is_nothrow_invocable_v<T&, ArgTypes...>);
};
```
:::

When `T` is not equality comparable, it is not surprising that `reference_wrapper<T>` is not equality comparable. But what about when `T` *is* equality comparable? There are no comparison operators here, but nevertheless the answer is... maybe?

Because `reference_wrapper<T>` is implicitly convertible to `T&` and `T` is an associated type of `reference_wrapper<T>`, `T`'s equality operator (if it exists) might be viable candidate. But it depends on exactly what `T` is and how the equality operator is defined. Given a type `T` and an object `t` such that `t == t` is valid, let's consider the validity of the expressions `ref(t) == ref(t)` and `ref(t) == t` for various possible types `T`:

|`T`|`ref(t) == ref(t)`|`ref(t) == t`|
|-|-|-|
|builtins|✔️|✔️|
|class or class template with member `==`|❌|✔️ (since C++20)|
|class with non-member or hidden friend `==`|✔️|✔️|
|class template with hidden friend `==`|✔️|✔️|
|class template with non-member, template `==`|❌|❌|
|`std::string_view`|❌|✔️|

That's a weird table!

Basically, if `T` is equality comparable, then `std::reference_wrapper<T>` is... sometimes... depending on how `T`'s comparisons are defined. `std::reference_wrapper<int>` is equality comparable, but `std::reference_wrapper<std::string>` is not. Nor is `std::reference_wrapper<std::string_view>` but you can nevertheless compare a `std::reference_wrapper<std::string_view>` to a `std::string_view`.

So, first and foremost: sense, this table makes none.

Second, there are specific use-cases to want `std::reference_wrapper<T>` to be normally equality comparable, and those use-cases are the same reason what `std::reference_wrapper<T>` exists to begin with: deciding when to capture a value by copy or by reference.

Consider wanting to have a convenient shorthand for a predicate to check for equality against a value. This is something that shows up in lots of libraries (e.g. Björn Fahller's [lift](https://github.com/rollbear/lift/blob/3927d06415f930956341afd5bc223f912042d7e4/include/lift.hpp#L150-L158) or Conor Hoekstra's [blackbird](https://github.com/codereport/blackbird/blob/623490fbfb4b8ef68bcda723d22b055b27e4d6ed/combinators.hpp#L43)), and looks something like this:

::: bq
```cpp
inline constexpr auto equals = [](auto&& value){
  return [value=FWD(value)](auto&& e){ return value == e; };
};
```
:::

Which allows the nice-looking:

::: bq
```cpp
if (std::ranges::any_of(v, equals(0))) {
    // ...
}
```
:::

But this implementation always copies (or moves) the value into the lambda. For larger types, this is wasteful. But we don't want to either unconditionally capture by reference (which sometimes leads to dangling) or write a parallel hierarchy of reference-capturing function objects (which is lots of code duplication and makes the library just worse).

This is *exactly* the problem that `std::reference_wrapper<T>` solves for the standard library: if I want to capture something by reference into `std::bind` or `std::thread` or anything else, I pass the value as `std::ref(v)`. Otherwise, I pass `v`. We should be able to use the exact same solution here, without having to change the definition of `equals`:

::: bq
```cpp
if (std::ranges::any_of(v, equals(std::ref(target)))) {
    // ...
}
```
:::

And this works! Just... only for some types, seemingly randomly. The goal of this proposal is for it to just always work.

## Ambiguity Issues

In the original revision of the paper, the proposal was simply to add this equality operator:

::: bq
```cpp
template<class T> class reference_wrapper {
  friend constexpr bool operator==(reference_wrapper, reference_wrapper);
}
```
:::

But this turns out to be insufficient. It's enough for `reference_wrapper<T>` to become comparable for all cases, but that's not exactly all we need. Consider:

::: bq
```cpp
auto check(int i, std::reference_wrapper<int> r) -> bool {
  return i == r;
}
```
:::

This comparison is valid today, per the table earlier: we convert `r` to `int` through its `operator int&()` and use the builtin comparison. But now we're adding a new candidate, which is also valid: we can convert `i` to `reference_wrapper<int>`. These two candidates are ambiguous. The same is true for many other similar comparisons.

In order to ensure that we catch all the interesting cases, we can build up all the comparisons that we want to check. For non-const `T`:

::: bq
```cpp
template <class T>
concept ref_equality_comparable = requires (T a, T const ca, Ref<T> r, Ref<T const> cr) {
    // the usual T is equality-comparable with itself
    a == a;
    a == ca;
    ca == ca;

    // Ref<T> is equality-comparable with itself
    r == r;
    r == cr;
    cr == cr;

    // T and Ref<T> are equality-comparable
    a == r;
    a == cr;
    ca == r;
    ca == cr;
};
```
:::

We don't need to check both directions of comparison anymore, but we do need to check const and non-const comparisons - which means `T` and `T const` for the objects and `Ref<T>` and `Ref<T const>` for our reference wrapper. We need to be careful to check both because of the case I just showed earlier - `int == reference_wrapper<int>` would be ambiguous with the rules laid out in R0 and R1 of this paper, but `int const == reference_wrapper<int>` actually would be fine (because `int const&` is not convertible to `reference_wrapper<int>`, so we only have one viable candidate).

That concept fails for every type with the R0/R1 proposal. To disambigugate, we need to add an extra comparison to handle the `T == Ref<T>` case:`

::: bq
```cpp
template<class T> class reference_wrapper {
  friend constexpr bool operator==(reference_wrapper, reference_wrapper);
  friend constexpr bool operator==(reference_wrapper, T const&);
}
```
:::

That gets us a lot closer, but it still isn't sufficient. Actually only one single expression now fails: the `r == cr` (`Ref<T> == Ref<T const>`) check, which fails for all `T`. The previous ambiguity is annoying, but this one particularly so since we just need a dedicated comparison operator _just_ for this case. Which we can add:

::: bq
```cpp
template<class T> class reference_wrapper {
  friend constexpr bool operator==(reference_wrapper, reference_wrapper);
  friend constexpr bool operator==(reference_wrapper, T const&);
  friend constexpr bool operator==(reference_wrapper, reference_wrapper<T const>); // only for non-const T
}
```
:::

And that, now, passes [all the tests](https://godbolt.org/z/eTs71o49o).

## Non-boolean comparisons

Another question that came up with in the LEWG telecon was how this proposal interacts with non-boolean comparison operators. For instance:

::: bq
```cpp
void f(std::valarray<int> v) {
  // this is a valid expression today, whose type is not bool, but rather
  // something convertible to std::valarray<bool>
  v == v;
}
```
:::

Now, `std::valarray<T>`'s comparison operators are specified as non-member function templates, so any comparison using `std::reference_wrapper<std::valarray<T>>` doesn't work today. But let's make our own version of this type that's more friendly (or hostile, depending on your perspective) to this paper and consider:

::: bq
```cpp
template <typename T>
struct ValArray {
  friend auto operator==(ValArray const&, ValArray const&) -> ValArray<bool> {
    return {};
  }
};

void f(ValArray<int> v) {
  // this is valid and has type ValArray<bool>
  v == v;

  // this is also valid today and has the same type
  std::ref(v) == std::ref(v);
}
```
:::

Now, does anybody write such code? Who knows. If we [constrain](#constraints-vs-mandates) the comparisons of `std::reference_wrapper<T>` (and also the other standard library types), then this code will continue to work fine anyway - since the comparisons would be constrained away by types like `ValArray<T>` not satisfying `equality_comparable`. This paper would not be adding any new candidates to the candidate set, so no behavior changes.

But, as always, there is an edge case.

1. there is a type `T`, whose comparisons return a type like `int`
2. and those comparisons are written in such a way that comparison `T` to `std::reference_wrapper<T>` works (see table above)
3. and users are relying on such comparisons to actually return `int`

Then the comparisons to `std::reference_wrapper<T>` will instead start returning `bool`. That is:

::: bq
```cpp
struct ComparesAsInt {
  friend auto operator==(ComparseAsInt, ComparesAsInt) -> int;
};

auto f(std::reference_wrapper<ComparesAsInt> a, std::reference_wrapper<ComparesAsInt> b) {
  // today: compiles and returns int
  // proposed: compiles and returns bool
  return a == b;
}
```
:::

Here, the added comparison operators would be valid, and wouldn't constrain away, since `std::equality_comparable` is based on `$boolean-testable$` which only requires convertibility to `bool` (and some other nice behavior), which `int` does satisfy. And those added comparison operators would be better matches than the existing ones, so they would win.

This would be the only case where any behavior would change.

# Proposal

Add `==` and `<=>` to `std::reference_wrapper<T>` so that `std::reference_wrapper<T>` is always comparable when `T` is, regardless of how `T`'s comparisons are defined.

Change [refwrap.general]{.sref}:

::: bq
```diff
  template<class T> class reference_wrapper {
  public:
    // types
    using type = T;

    // [refwrap.const], constructors
    template<class U>
      constexpr reference_wrapper(U&&) noexcept($see below$);
    constexpr reference_wrapper(const reference_wrapper& x) noexcept;

    // [refwrap.assign], assignment
    constexpr reference_wrapper& operator=(const reference_wrapper& x) noexcept;

    // [refwrap.access], access
    constexpr operator T& () const noexcept;
    constexpr T& get() const noexcept;

    // [refwrap.invoke], invocation
    template<class... ArgTypes>
      constexpr invoke_result_t<T&, ArgTypes...> operator()(ArgTypes&&...) const
        noexcept(is_nothrow_invocable_v<T&, ArgTypes...>);

+   // [refwrap.comparisons], comparisons
+   friend constexpr bool operator==(reference_wrapper, reference_wrapper);
+   friend constexpr bool operator==(reference_wrapper, const T&);
+   friend constexpr bool operator==(reference_wrapper, reference_wrapper<const T>);

+   friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper, reference_wrapper);
+   friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper, const T&);
+   friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper, reference_wrapper<const T>);
  };
```
:::

Add a new clause, [refwrap.comparisons], after [refwrap.invoke]{.sref}:

::: bq
::: addu
```
friend constexpr bool operator==(reference_wrapper x, reference_wrapper y);
```

[#]{.pnum} *Mandates*: The expression `x.get() == y.get()` is well-formed and its result is convertible to `bool`.

[#]{.pnum} *Returns*: `x.get() == y.get()`.

```
friend constexpr bool operator==(reference_wrapper x, const T& y);
```

[#]{.pnum} *Mandates*: The expression `x.get() == y` is well-formed and its result is convertible to `bool`.

[#]{.pnum} *Returns*: `x.get() == y`.

```
friend constexpr bool operator==(reference_wrapper x, reference_wrapper<const T> y);
```

[#]{.pnum} *Constraints*: `is_const_v<T>` is `false`.

[#]{.pnum} *Mandates*: The expression `x.get() == y.get()` is well-formed and its result is convertible to `bool`.

[#]{.pnum} *Returns*: `x.get() == y.get()`.

```
friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper x, reference_wrapper y);
```

[#]{.pnum} *Returns*: `$synth-three-way$(x.get(), y.get())`.

```
friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper x, const T& y);
```

[#]{.pnum} *Returns*: `$synth-three-way$(x.get(), y)`.

```
friend constexpr $synth-three-way-result$<T> operator<=>(reference_wrapper x, reference_wrapper<const T> y);
```
[#]{.pnum} *Constraints*: `is_const_v<T>` is `false`.

[#]{.pnum} *Returns*: `$synth-three-way$(x.get(), y.get())`.
:::
:::

## Feature-test macro

We don't have a feature-test macro for `std::reference_wrapper<T>`, and there doesn't seem like a good one to bump for this, so let's add a new one to [version.syn]{.sref}

::: bq
```diff
+ #define __cpp_lib_reference_wrapper 20XXXXL // also in <functional>
```
:::

## Constraints vs Mandates

The wording here uses *Mandates* for the equality comparison, even though the spaceship operator is constrained (by way of `$synth-three-way-result$<T>`). This is, surprisingly, consistent with the other standard library types (`std::pair`, `std::tuple`, etc.). There does not seem to be a particularly good reason for this. It kind of just happened - the relational comparisons became constrained by way of my [@P1614R2], and the equality ones just weren't touched. It would make a lot more sense to have all of them constrained, so that `std::equality_comparable<std::tuple<T>>` wasn't just `true` for all `T` (well, except `void` and incomplete types).

If we agree that we should just consistently constrain all the comparison operators, then we should additionally make the following wording changes (in addition to changing the *Mandates* to a *Constraints* above):

In [pairs.spec]{.sref}/1:

::: bq
[1]{.pnum} [*Preconditions*]{.rm} [*Constraints*]{.addu}: [`x.first == y.first` and `x.second == y.second` are valid expressions and each]{.addu} [Each]{.rm} of `decltype(x.first == y.first)` and `decltype(x.second == y.second)` models `$boolean-testable$`.
:::

In [tuple.rel]{.sref}/2:

::: bq
[2]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: For all `i`, where `0 <= i < sizeof...(TTypes)`, `get<i>(t) == get<i>(u)` is a valid expression [and `decltype(get<i>(t) == get<i>(u))` models `$boolean-testable$`]{.addu}. `sizeof...(TTypes)` equals `tuple_size_v<UTuple>`.

::: rm
[3]{.pnum} *Preconditions*: For all `i`, `decltype(get<i>(t) == get<i>(u))` models `$boolean-testable$`.
:::
:::

In [optional.relops]{.sref}, change all the *Mandates* to *Constraints*:

::: bq
[1]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x == *y` is well-formed and its result is convertible to `bool`.

[4]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x != *y` is well-formed and its result is convertible to `bool`.

[7]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x < *y` is well-formed and its result is convertible to `bool`.

[10]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x > *y` is well-formed and its result is convertible to `bool`.

[13]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x <= *y` is well-formed and its result is convertible to `bool`.

[16]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x >= *y` is well-formed and its result is convertible to `bool`.
:::

In [optional.comp.with.t]{.sref}, change all the *Mandates* to *Constraints*:

::: bq
[1]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x == v` is well-formed and its result is convertible to `bool`.

[3]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v == *x` is well-formed and its result is convertible to `bool`.

[5]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x != v` is well-formed and its result is convertible to `bool`.

[7]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v != *x` is well-formed and its result is convertible to `bool`.

[9]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x < v` is well-formed and its result is convertible to `bool`.

[11]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v < *x` is well-formed and its result is convertible to `bool`.

[13]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x > v` is well-formed and its result is convertible to `bool`.

[15]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v > *x` is well-formed and its result is convertible to `bool`.

[17]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x <= v` is well-formed and its result is convertible to `bool`.

[19]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v <= *x` is well-formed and its result is convertible to `bool`.

[21]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `*x >= v` is well-formed and its result is convertible to `bool`.

[23]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: The expression `v >= *x` is well-formed and its result is convertible to `bool`.
:::

In [variant.relops]{.sref}, change all the *Mandates* to *Constraints*:

::: bq
[1]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) == get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.

[3]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) != get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.

[5]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) < get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.

[7]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) > get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.

[9]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) <= get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.

[11]{.pnum} [*Mandates*]{.rm} [*Constraints*]{.addu}: `get<i>(v) >= get<i>(w)` is a valid expression that is convertible to `bool`, for all `i`.
:::
