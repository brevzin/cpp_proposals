---
title: "Improve default container formatting"
document: P2585R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: ranges
---

# Introduction

[@P2286R7] adds support for formatting any range whose underlying type is formattable. Additionally, it adds support for different kinds of formatting that users can opt into, while also providing a default choice for associating containers that is more suited to what those containers represent.

For example, simply using `"{}"` as the specifier, we get the following outputs:

|Expression|Output|
|-|-|
|`std::vector<std::pair<int, int>>{{1, 2}, {3, 4}}`|`[(1, 2), (3, 4)]`|
|`std::set<std::pair<int, int>>{{1, 2}, {3, 4}}`|`{(1, 2), (3, 4)}`|
|`std::map<int, int>{{1, 2}, {3, 4}}`|`{1: 2, 3: 4}`|

In each case, we have a range over a pair of ints, but we have three different outputs - as appropriate for the different kinds of containers.

However, this distinction is a result of [@P2286R7] explicitly providing formatters for all the standard library map and set containers, and applying those changes to them. This is something that users _can_ do for their own containers as well, but which also means that it is something users _have_ to do - if this is the behavior they want. For instance, the containers in Boost or Abseil just format like normal ranges:

|Expression|Output|
|-|-|
|`boost::container::flat_set<int>{1, 2, 3}`|`[1, 2, 3]`|
|`absl::flat_hash_map<int, int>{{1, 2}, {3, 4}}`|`[(1, 2), (3, 4)]`|

This output isn't _wrong_, per se. It's just that it's not ideal. And, sure, Abseil could certainly add the code necessary to make this happen. Which, at a bare minimum, would be:

::: bq
```cpp
template <class K, class V, class... Rest>
  requires std::formattable<const K, char>
        && std::formattable<const V, char>
struct std::formatter<absl::flat_hash_map<K, V, Rest...>, char>
  : std::range_formatter<std::pair<const K, V>, char>
{
  constexpr formatter() {
    this->set_brackets("{", "}");
    this->underlying().set_brackets({}, {});
    this->underlying().set_separator(": ");
  }
};
```
:::

Now, this isn't a lot of code, nor is it especially complex. But it is writing code to do what seems like it should be the default behavior.

And, indeed, this is what `{fmt}` does. By default, formatting a map (whether it's a `std::map` or an `absl::flat_hash_map`) does give you a string like `{1: 2, 3: 4}` rather than `[(1, 2), (3, 4)]`. `{fmt}` determines whether a type is map-like simply by checking if it has a member type named `mapped_type` ([here](https://github.com/fmtlib/fmt/blob/7e4ad40171aa552d38cb99a5c181a0d7b150facc/include/fmt/ranges.h#L67-L78)). Similarly, `{fmt}` determines whether a type is set-like if it has a member type named `key_type` (and it is not map-like) ([here](https://github.com/fmtlib/fmt/blob/7e4ad40171aa552d38cb99a5c181a0d7b150facc/include/fmt/ranges.h#L80-L91)).

We can do something similar in the standard library.

# Design

There are several kinds of range formatting styles:

* map-like, as `{k@~1~@: v@~1~@, k@~2~@: v@~2~@}`
* set-like, as `{v@~1~@, v@~2~@}`
* regular sequence, as `[v@~1~@, v@~2~@]`
* string, as `abc`
* debug string, `"\naan"`
* disabled

We can introduce an enum class for all of these kinds:

::: bq
```cpp
enum class range_format_kind {
    map,
    set,
    sequence,
    string,
    debug_string,
    disabled
};
```
:::

And a variable template (allowed to be specialized) to determine the right kind:

::: bq
```cpp
template <class R>
inline constexpr range_format_kind format_kind = []{
  if constexpr (requires { typename R::key_type; typename R::mapped_type; }) {
    return range_format_kind::map;
  } else if constexpr (requires { typename R::key_type; }) {
    return range_format_kind::set;
  } else {
    return range_format_kind::sequence;
  }
}();
```
:::

As far as heuristics go, this is a pretty safe bet. While we've previously tried to do a heuristic for `view` and then had to walk it back a lot, here the stakes are much lower. Worst-case, we're just talking about getting the default formatting wrong - for some definition of wrong - though we'd still format all the elements. And there probably aren't too many range types floating around that define `key_type` and `mapped_type` but aren't maps?

Note that we're deliberately not making any sort of guess about `string`. Not looking at convertibility to `string` or `string_view` or `char const*` or anything of the sort. If users want some container to be formatted as a string, they can do so. Explicitly.

If we then change the way the standard library does formatting based on `format_kind<R>`, then with no additional code changes necessary, all the Boost and Abseil containers would start being formatted in the same kind of way that the standard library ones are today. That and many other user-defined associative containers.

Moreover, for user-defined containers, specializing a variable template is quite a bit less work than almost anything else you can do. So even for containers where the heuristic is wrong, this design offers an easier way forward to get the desired behavior.

While this design as-is has not been implemented, `{fmt}`'s approach is pretty comparable.

# Wording

This is in terms of [@P2286R7].

## Trait and Regular Sequences

Change [format.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...

  // [format.formatter], formatter
  template<class T, class charT = char> struct formatter;

  // [format.range.formatter], class template range_formatter
+ enum class range_format_kind {
+   map,
+   set,
+   sequence,
+   string,
+   debug_string,
+   disabled
+ };
+
+ template<class R>
+   inline constexpr range_format_kind format_kind = $see below$;

  template<class T, class charT = char>
      requires same_as<remove_cvref_t<T>, T> && formattable<T, charT>
    class range_formatter;

  template<ranges::input_range R, class charT>
-         requires (!same_as<remove_cvref_t<ranges::range_reference_t<R>>, R>)
+         requires (format_kind<R> == range_format_kind::sequence)
            && formattable<ranges::range_reference_t<R>, charT>
    struct formatter<R, charT>;

  // ...
}
```
:::

Add to [format.range]:

::: bq
::: addu
```cpp
template<class R>
  inline constexpr range_format_kind format_kind = $see below$;
```

[a]{.pnum} For a type `R`, `format_kind<R>` is defined as follows:

  * [a.#]{.pnum} If `same_as<remove_cvref_t<ranges::range_reference_t<R>>, R>` is `true`, `format_kind<R>` is `range_format_kind::disabled`. [*Note*: This prevents constraint recursion for ranges whose reference type is the same range type. For example, `std::filesystem::path` is a range of `std::filesystem::path`. *-end note* ]
  * [a.#]{.pnum} If the _qualified-id_ s `R::key_type` and `R::mapped_type` are valid and denote types, `format_kind<R>` is `range_format_kind::map`.
  * [a.#]{.pnum} If the _qualified-id_ `R::key_type` is valid and denotes a type, `format_kind<R>` is `range_format_kind::set`.
  * [a.#]{.pnum} Otherwise, `format_kind<R>` is `range_format_kind::sequence`.

[b]{.pnum} *Remarks*: Pursuant to [namespace.std], users may specialize `format_kind` for *cv*-unqualified program-defined types.
Such specializations shall be usable in constant expressions ([expr.const]) and have type `const range_format_kind`.
:::
:::

And later:

::: bq
```diff
namespace std {
  template<ranges::input_range R, class charT>
-         requires (!same_as<remove_cvref_t<ranges::range_reference_t<R>>, R>)
+         requires (format_kind<R> == range_format_kind::sequence)
            && formattable<ranges::range_reference_t<R>, charT>
  struct formatter<R, charT> {
  private:
    using $maybe-const-r$ = $fmt-maybe-const$<R, charT>;
    range_formatter<remove_cvref_t<ranges::range_reference_t<$maybe-const-r$>>, charT> $underlying_$; // exposition only

  public:
    constexpr void set_separator(basic_string_view<charT> sep);
    constexpr void set_brackets(basic_string_view<charT> opening, basic_string_view<charT> closing);

    template <class ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);

    template <class FormatContext>
      typename FormatContext::iterator
        format($maybe-const-r$& elems, FormatContext& ctx) const;
  };
}
```

::: rm
[13]{.pnum} [*Note*: The `(!same_as<remove_cvref_t<ranges::range_reference_t<R>>, R>)` constraint prevents constraint recursion for ranges whose reference type is the same range type. For example, `std::filesystem::path` is a range of `std::filesystem::path`. *-end note* ]
:::

```
constexpr void set_separator(basic_string_view<charT> sep);
```

[#]{.pnum} *Effects*: Equivalent to `$underlying_$.set_separator(sep)`;
:::

## Maps and sets

Change the the wording for associative containers as follows:

::: bq
::: rm
[1]{.pnum} For each of `map`, `multimap`, `unordered_map`, and `unordered_multimap`, the library provides the following formatter specialization where `$map-type$` is the name of the template:
:::

```diff
namespace std {
- template <class charT, class Key, formattable<charT> T, class... U>
-   requires formattable<const Key, charT>
- struct formatter<$map-type$<Key, T, U...>, charT>
+ template <input_range R, class charT>
+   requires (format_kind<R> == range_format_kind::map)
+         && formattable<tuple_element_t<0, remove_reference_t<ranges::range_reference_t<R>>>, charT>
+         && formattable<tuple_element_t<1, remove_reference_t<ranges::range_reference_t<R>>>, charT>
+ struct formatter<R, charT>
  {
  private:
-   using $maybe-const-map$ = $fmt-maybe-const$<$map-type$<Key, T, U...>, charT>;  // exposition only
+   using $maybe-const-map$ = $fmt-maybe-const$<R, charT>;                       // exposition only
    range_formatter<remove_cvref_t<ranges::range_reference_t<$maybe-const-map$>>, charT> $underlying_$; // exposition only
  public:
    constexpr formatter();

    template <class ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);

    template <class FormatContext>
      typename FormatContext::iterator
        format($maybe-const-map$& r, FormatContext& ctx) const;
  };
}
```

```
constexpr formatter();
```

::: addu
[#]{.pnum} *Mandates*: Let `T` denote `ranges::range_value_t<R>`. Either:

  * [#.#]{.pnum} `T` is a specialization of `std::pair`, or
  * [#.#]{.pnum} `T` is a specialization of `std::tuple` and `std::tuple_size_v<T> == 2`.
:::

[#]{.pnum} *Effects*: Equivalent to:

```
$underlying_$.set_brackets($STATICALLY-WIDEN$<charT>("{"), $STATICALLY-WIDEN$<charT>("}"));
$underlying_$.underlying().set_brackets({}, {});
$underlying_$.underlying().set_separator($STATICALLY-WIDEN$<charT>(": "));
```

```
template <class ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```

[#]{.pnum} *Effects*: Equivalent to `return $underlying_$.parse(ctx);`

```
template <class FormatContext>
  typename FormatContext::iterator
    format($maybe-const-map$& r, FormatContext& ctx) const;
```

[#]{.pnum} *Effects*: Equivalent to `return $underlying_$.format(r, ctx);`

::: rm
[#]{.pnum} For each of `set`, `multiset`, `unordered_set`, and `unordered_multiset`, the library provides the following formatter specialization where `$set-type$` is the name of the template:
:::

```diff
namespace std {
- template <class charT, class Key, class... U>
-   requires formattable<const Key, charT>
- struct formatter<$set-type$<Key, U...>, charT>
+ template <input_range R, class charT>
+   requires (format_kind<R> == range_format_kind::set)
+         && formattable<ranges::range_reference_t<const R>, charT>
+ struct formatter<R, charT>
  {
  private:
-   range_formatter<Key, charT> $underlying_$; // exposition only
+   range_formatter<remove_cvref_t<ranges::range_reference_t<R>>, charT> $underlying_$; // exposition only

  public:
    constexpr formatter();

    template <class ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);

    template <class FormatContext>
      typename FormatContext::iterator
-       format(const $set-type$<Key, U...>& r, FormatContext& ctx) const;
+       format(const R& r, FormatContext& ctx) const;
  };
}
```

```
constexpr formatter();
```

[#]{.pnum} *Effects*: Equivalent to:

```
$underlying_$.set_brackets($STATICALLY-WIDEN$<charT>("{"), $STATICALLY-WIDEN$<charT>("}"));
```

```
template <class ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```

[#]{.pnum} *Effects*: Equivalent to `return $underlying_$.parse(ctx);`

```diff
template <class FormatContext>
  typename FormatContext::iterator
-   format(const $set-type$<Key, U...>& r, FormatContext& ctx) const;
+   format(const R& r, FormatContext& ctx) const;
```

[#]{.pnum} *Effects*: Equivalent to `return $underlying_$.format(r, ctx);`
:::


## Strings

Also add this partial specialization to handle string types:

::: bq
::: addu
```
template <input_range R, class charT>
  requires (format_kind<R> == range_format_kind::string ||
            format_kind<R> == range_format_kind::debug_string)
        && same_as<remove_cvref_t<range_reference_t<R>>, charT>
struct formatter<R, charT>
{
private:
  formatter<basic_string<charT>, charT> $underlying_$; // exposition only

public:
  template <class ParseContext>
    constexpr typename ParseContext::iterator
      parse(ParseContext& ctx);

  template <class FormatContext>
    typename FormatContext::iterator
      format(const R& str, FormatContext& ctx) const;
};
```

```
template <class ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```

[#]{.pnum} *Effects*: Equivalent to:

::: bq
```
auto i = $underlying_$.parse(ctx);
if constexpr (format_kind<R> == range_format_kind::debug_string) {
  $underlying_$.set_debug_format();
}
return i;
```
:::

```
template <class FormatContext>
  typename FormatContext::iterator
    format(const R& r, FormatContext& ctx) const;
```

[#]{.pnum} *Returns*: `$underlying_$.format(basic_string<charT>(from_range, r), ctx);`
:::
:::

# Acknowledgements

Thanks to Tomasz Kamiński for suggesting this approach, Victor Zverovich for having more or less already implemented it, and Tim Song for everything else.
