---
title: "Using `optional<T&>` in `std::inplace_vector`"
document: P3981R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Jonathan Wakely
      email: <cxx@kayari.org>
    - name: Tomasz Kamiński
      email: <tomaszkam@gmail.com>
toc: true
---

# Introduction

This paper seeks to address the following NB comments:

* [PL-006](https://github.com/cplusplus/nbballot/issues/813)
* [US 150-228](https://github.com/cplusplus/nbballot/issues/799)
* [GB 08-225](https://github.com/cplusplus/nbballot/issues/796)

The new C++26 container `std::inplace_vector<T, N>` contains four functions or function templates which conditionally try to perform some operation, which might fail due to exceeding capacity. Those signatures are currently:

::: std
```cpp
template<class... Args>
  constexpr pointer try_emplace_back(Args&&... args);
constexpr pointer try_push_back(const T& x);
constexpr pointer try_push_back(T&& x);

template<$container-compatible-range$<T> R>
  constexpr ranges::borrowed_iterator_t<R> try_append_range(R&& rg);
```
:::

We argue in this paper that there is a better choice for the return type for each of these algorithms: `optional<reference>` for the first three and `ranges::borrowed_subrange_t<R>` for the fourth.

# `T*` makes for a poor `optional<T&>`

The functions `emplace_back` and `push_back` return a `reference` to the new element that was added to the container. The functions `try_emplace_back` and `try_push_back` seek to do the same thing, except that these functions signal failure via the return path: they can only _conditionally_ return a reference to the element that was added, so they need to return something else on failure.

When [@P0843R14]{.title}, the only sensible choice for the return type was `T*`: either a pointer that points to the new element or a null pointer. However, now that [@P2988R12]{.title} was adopted for C++26, there is another choice: `optional<T&>`. These types are, superficially, quite similar to each other. And, indeed, whenever the idea of an optional reference comes up, inevitably somebody will bring up the point that we don't need `optional<T&>` because we already have `T*`. Barry wrote a whole blog post several years about about how [`T*` makes for a poor `optional<T&>`](https://brevzin.github.io/c++/2021/12/13/optional-ref-ptr/) responding to this claim.

There are several significant benefits to `optional<T&>` when it comes to the return type here, which we will enumerate.

First, as you can already see in the opening paragraph of this section, in some ways `optional<T&>` is already inherently the correct choice. We have `push_back` returning a `T&`. So `try_push_back`, which instead of having a precondition simply tries and might fail, should return an `optional<T&>`. That is the general shape of fallible functions.

Second, consider the semantics. `T*` has many possible semantics: it is either an owning or non-owning pointer, that could point to a single object, to an array of objects, or past-the-end of an object or array. In our case, we are returning either a reference to a single object or nothing — which is exactly the singular semantic of `optional<T&>`.

Third, consider the potential API of the return type:

![](optional-vs-ptr.png)

There are some operations that `T*` and `optional<T&>` share in the middle, those have the same semantics and meaning either way. All of the operations in the purple circle are highly relevant and useful to this problem. We want to have an optional reference, so it is useful to have the chaining operations that give us a different kind of optional, or to provide a default value, or to emplace or reset, or even to have a throwing accessor. But all the operations in the orange circle are highly _irrelevant_ to this problem and would be completely wrong to use. They are bugs waiting to happen. We don’t have an array, so none of the indexing operations are valid. And we don’t have an owning pointer, so neither `delete` nor `delete []` are valid. Nevertheless, these operations will actually compile – even though they are all undefined behavior.

You'll note that "pattern matching" appears in both circles, differently — this is because [@P2688R5]{.title} supports matching both `optional<U>` and `T*`, but they are matched _differently_:

* an `optional<U>` matches against `U` or `nullopt`, because that's precisely what it represents.
* a `T*` doesn't match against a `T`, rather it matches polymorphically.

We still won't have pattern matching in C++26, but if we ever do, we'll want to be able to match on whether our optional reference actually contains a reference, or not. We do not need to match whether we're holding a derived type or not.

Fourth, with standard library hardening, we know that `*x` and `x->m` will be checked if `x` is an `optional<T&>`. But there is no such guaranteed checking for raw pointers. Hopefully, you segfault?

These are very significant benefits to returning `optional<T&>`. There are simply no benefits to returning `T*`.

# Iterator or Subrange?

The last algorithm is `try_append_range`. Currently, `v.try_append_range(r)` returns an iterator pointing to the first element of `r` that was _not_ inserted into `v`. This is inconvenient, as pointed out by PL-006.

First, it makes it more tedious to check if all of the elements were inserted:

::: cmptable
### Returning an Iterator
```cpp
auto range = get_some_elements();
auto it = v.try_append_range(range);
if (it == range.end()) {
  // success
}
```

### Returning a Subrange
```cpp
auto range = get_some_elements();
if (v.try_append_range(range).empty()) {
  // success
}
```
:::

Returning the whole subrange gives you all the information you need in the return type directly. So if you want to do further manipulation on it, you can, without having to re-acquire the original range:

::: cmptable

### Returning an Iterator
```cpp
auto range = get_some_elements();
auto it = v.try_append_range(range);
if (it != range.end()) {
  do_something_else(ranges::subrange(it, range.end()));
}
```

### Returning a Subrange
```cpp
auto range = get_some_elements();
auto sr = v.try_append_range(range);
if (not sr.empty()) {
  do_something_else(sr);
}
```
:::

This follows the general principle that ranges are simply more convenient for users than iterators, because you only need the one object rather than two.

# Proposal

We propose to change the return types of four algorithms in `std::inplace_vector<T, N>`:

* `try_emplace_back` and both overloads of `try_push_back` to return `optional<T&>` instead of `T*`
* `try_append_range` to return a `borrowed_subrange_t<R>` instead of a `borrowed_iterator_t<R>`.

## Wording

TBD, but easy enough