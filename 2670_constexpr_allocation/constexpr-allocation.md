---
title: "Non-transient `constexpr` allocation"
document: P2670R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

# Introduction

For C++20, [@P0784R7] introduced the notion of transient `constexpr` allocation. That is, we can allocate during compile time - but only as long as the allocation is completely cleaned up by the end of the evaluation. With that, we can do this:

::: bq
```cpp
constexpr auto f() -> int {
    std::vector<int> v = {1, 2, 3};
    return v.size();
}
static_assert(f() == 3);
```
:::

But not yet this:

::: bq
```cpp
constexpr std::vector<int> v = {1, 2, 3};
static_assert(v.size() == 3);
```
:::

Because `v`'s allocation persists, that is not yet allowed.

## The Problem Cases

The problems with persistent `constexpr` allocation can be demonstrated with this example:

::: bq
```cpp
constexpr std::unique_ptr<int> pi(new int(1));
constexpr std::unique_ptr<std::vector<int>> pvec(new std::vector<int>{1, 2, 3});

int main() {
    *pi = 2;

    pvec->push_back(4);
}
```
:::

If constexpr allocation could persist through runtime, it could presumably be put in read-only memory. This would be quite useful for programmers. But it introduces new restrictions that haven't previously had in the language.

While `pi` above is a `constexpr` (and thus `const`) object, dereferencing it still gives us a mutable `int`, so that assignment would compile. But if `pi`'s allocation is put into read-only memory, this write could lead to a page fault.

The situation with `pvec` is worse. Again, `pvec` dereferences to a mutable `std::vector<int>`, which we can `push_back` onto. Since that `vector` is likely at capacity, this `push_back` would trigger a reallocation. The new allocation is not an issue, but the deallocation of the initial storage is: it wasn't allocated at runtime, it was allocated at compile time, so the allocator is... unlikely to be able to gracefully handle this situation, to say the least.

Both of these uses are, despite being highly problematic, perfectly valid C++ code today, and we get into these problems without even any `const_cast` shenanigans or really even doing anything especially weird. It's not like we're digging ourselves a hole, it's more that we just took a step and fell.

## Proposed Solutions

There have been two proposed solutions to this problem, both with the goal of making the above ill-formed. Specifically, rejecting the construction of a `constexpr std::unique_ptr<T>` object (for non-`const` `T`). The concrete problem here is that `std::unique_ptr<T>` is shallow-const. `std::vector<T>` is structurally a similar type (both have a `T*` member), but `std::vector<T>` is deep-const, and so doesn't expose the kinds of mutable accesses that would be harmful. How can the compiler distinguish between a "good" `T*` member and a "bad" `T*` member?

### `std::mark_immutable_if_constexpr`

The initial design in [@P0784R7], before it was removed, was a new function `std::mark_immutable_if_constexpr(p)` that would mark an allocation as being immutable. A persistent `constexpr` allocation thus had to be either:

* marked as immutable, or
* be a pointer to `const`

`std::vector<T>` would mark its allocation as immutable, but `std::unique_ptr<T>` would not. That way:

::: bq
```cpp
// error: mutable allocation persists
constexpr std::unique_ptr<int> a(new int(1));

// ok: allocation isn't marked immutable, but is const
constexpr std::unique_ptr<int const>(new int(2));

// ok: allocation is marked immutable
constexpr std::vector<int> v = {3, 4, 5};
```
:::

In this way, `std::mark_immutable_as_constexpr(p)` is basically saying: I promise my type is actually properly deep-`const`.

### `T propconst*`

[@P1974R0] proposed something different: a new qualifier that would propagate `const`-ness off the object through the pointer. A persistent `constexpr` allocation would thus have to have only `const` access, after adjusting through `propconst`.

`std::vector<T>` would change to have members of type `T propconst*` instead of `T*`, so that a `constexpr std::vector<int>` would effectively have members of type `int const*`, while `std::unique_ptr<T>` would remain unchanged, still having a `T*`. That way:

::: bq
```cpp
// error: mutable allocation persists
constexpr std::unique_ptr<int> a(new int(1));

// ok: only access to the allocation is via int const*
constexpr std::unique_ptr<int const> b(new int(2));

// ok: now we have a deep const unique_ptr<int>
// since 'c' is a const object, this behaves basically like 'b'
constexpr std::unique_ptr<int propconst> c(new int(3));

// ok: only access to the allocation is via int const*
// (by way of int propconst* member)
constexpr std::vector<int> v = {4, 5, 6};
```
:::

### Disposition

Both of these proposals have similar shape, in that they attempt to reject the same bad thing (persistent mutable allocation, like `constexpr std::unique_ptr<T>`) by way of having to add annotations to the good thing (allowing `constexpr std::vector<T>` to work, by either annotating the pointers or the allocation).

Importantly, both require types opt-in, somehow, to persistent allocation. `T propconst*` is more sound, in that the compiler _ensures_ that there is no mutable persistent allocation possible, while `std::mark_immutable_if_constexpr(p)` is simply a promise that the user did their due diligence and properly made their type deep-`const`.

What I mean is that, using `propconst`, this error won't compile - we can't accidentally expose mutable access (not without `const_cast`):

::: bq
```cpp
class bad_vector_pc {
    int propconst* data_;
    int size_;
    int capacity_;

public:
    // oops, int& instead of int const&
    constexpr auto operator[](int idx) const -> int& {
        // error: data_[idx] is an int const& here
        return data_[idx];
    }
};
```
:::

But this would work just fine (until it explodes at runtime):

::: bq
```cpp
class bad_vector_miic {
    int* data_;
    int size_;
    int capacity_;

public:
    constexpr bad_vector_miic(std::initializer_list<int> xs) {
        data_ = new int[xs.size()];
        std::copy(xs.begin(), xs.end(), data_);
        size_ = xs.size();
        capacity = xs.size();

        std::mark_immutable_if_constexpr(data_);
    }

    constexpr ~bad_vector_miic() {
        delete [] data_;
    }

    // oops, int& instead of int const&
    constexpr auto operator[](int idx) const -> int& {
        // this still compiles though
        return data_[idx];
    }
};

constexpr bad_vector_miic v = {1, 2, 3};

int main() {
    v[0] = 4; // ok: compiles?
}
```
:::

However, `T propconst*` has the downside that it's fairly pervasive throughout the language - where a significant amount of library machinery would have to account for it somehow. On the other hand, `std::mark_immutable_if_constexpr(p)` is extremely limited. No language machinery needs to consider its existence, and the only library machinery that does are those deep-const types that perform allocation that would have to use it.
