---
title: "Non-transient allocation with `std::vector` and `std::basic_string`"
document: P3554R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
toc: true
tag: constexpr
---

# Introduction

We've wanted to have non-transient constexpr allocation for quite some time, and there have been multiple papers on this topic so far. Three dealing with the general problem [@P0784R7] [@P1974R0] [@P2670R1] and one more attempting to make it work just in a narrow situation [@P3032R2].

During discussion of the latter paper (which only talked about persisting `constexpr` variables within `consteval` functions) in St. Louis, we took a poll to change the paper to only allow `std::vector` and `std::basic_string`, even for persistent allocation:

|SF|F|N|A|SA|
|-|-|-|-|-|
|4|9|8|3|0|

As of this writing (January 2025), we do not yet have consensus for a general design for non-transient allocation. We have ideas (see above papers), and we have a promising library workaround ([@P3491R0]). But we know that regardless of what the general solution will end up being, `std::vector<T>` and `std::basic_string<Char, Traits, std::allocator<Char>>` will work and allow their allocations to persist. These are the most common dynamic containers. And while `define_static_array` gets us some of the way there, simply allowing these two containers to persist has a lot of ergonomic value.

In the below comparison, assume the existence of:

::: cmptable

### With `define_static_array()`
```cpp
constexpr auto ints() -> std::vector<int>;

// data is a span<int const>
constexpr auto data = define_static_array(ints());
```

### Works By Fiat
```cpp
constexpr auto ints() -> std::vector<int>;

// data is a vector<int>
constexpr auto data = ints();
```

---

```cpp
constexpr auto strs() -> std::vector<std::string>;

// data is a span<string_view const>
constexpr auto data = define_static_array(
  strings() | std::views::transform([&](auto&& str){
    return std::string_view(define_static_string(str));
  })
);
```

```cpp
constexpr auto strs() -> std::vector<std::string>;

// data is a vector<string>
constexpr auto data = strings();
```

:::

While with a single layer of wrapping, having to add an extra call to `define_static_array` is tedious but fine, once we get into more complicated data structures, the wrapping itself gets more complicated. The good news here is that it is actually _possible_ to do. The fact that the left column can exist at all after [@P3491R0] is quite exciting.

But there are plenty of places where you might want persistent constexpr allocation — template arguments, the initializer in an expansion statement [@P1306R2], etc. Having to perform the correct wrapping in all of these places would get old really fast. So we think that even with the addition of `std::define_static_array` and `std::define_static_string`, we'd want a little bit more help.

# Proposal

The current rule is that any allocation within a constant expression `E` _must_ be deallocated within `E`. That rule can be found in [expr.const]{.sref}/10:

::: std
* [10.18]{.pnum} a *new-expression*, unless either:
    * [10.18.1]{.pnum} the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is deallocated within the evaluation of `E`, or
    * [10.18.2]{.pnum} [...]
* [10.19]{.pnum} a *delete-expression*, unless [...]
* [10.20]{.pnum} a call to an instance of `std​::​allocator<T>​::​allocate` ([allocator.members]), unless the allocated storage is deallocated within the evaluation of `E`;
:::

We instead propose introducing the concept of eligible for persistence:

::: std
::: addu
An allocation `A` that occurs within the evaluation of a core constant expression `E` is _eligible for persistence_ if:

* `E` is used to initialize an object `O` that is potentially usable in constant expressions, and
* the destruction of `O` deallocates `A` within a call to destructor of an object whose type is a specialization of either `std::vector` or `std::basic_string`.
:::
:::

And adjusting the wording as appropriate:

::: std
* [10.18]{.pnum} a *new-expression*, unless either:
    * [10.18.1]{.pnum} the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is [either eligible for persistence or]{.addu} deallocated within the evaluation of `E`, or
    * [10.18.2]{.pnum} [...]
* [10.19]{.pnum} a *delete-expression*, unless [...]
* [10.20]{.pnum} a call to an instance of `std​::​allocator<T>​::​allocate` ([allocator.members]), unless the allocated storage is [either eligible for persistence]{.addu} or deallocated within the evaluation of `E`;
:::