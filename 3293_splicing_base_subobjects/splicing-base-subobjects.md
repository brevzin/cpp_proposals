---
title: "Splicing a base class subobject"
document: P3293R0
date: today
audience: EWG
author:
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

[@P2996R3] proposing many different forms of splicing, including splicing a non-static data member. However, it does not yet support splicing a base class subobject.

This means that iterating over the `std::meta::subobjects_of` a type does not allow for uniform access to all of those subobjects:

::: std
```cpp
template <class T, class F>
void for_each_subobject(T const& obj, F f) {
    template for (constexpr auto sub : subobjects_of(^T)) {
      f(obj.[:sub:]); // this is valid for non-static data members
                      // but not for base classes
    }
}
```
:::

Instead we have to handle bases distinctly:

::: std
```cpp
template <class T, class F>
void for_each_subobject(T const& obj, F f) {
    template for (constexpr auto base : bases_of(^T)) {
      f(static_cast<type_of(base) const&>(obj));
    }

    template for (constexpr auto sub : nonstatic_data_members_of(^T)) {
      f(obj.[:sub:]);
    }
}
```
:::

Except this is now a normal `static_cast` and so requires access checking, thus prohibiting accessing private base classes.

We could avoid access checking by using a C-style cast:

::: std
```cpp
template <class T, class F>
void for_each_subobject(T const& obj, F f) {
    template for (constexpr auto base : bases_of(^T)) {
      f((typename [: type_of(base) :]&)obj);
    }

    template for (constexpr auto sub : nonstatic_data_members_of(^T)) {
      f(obj.[:sub:]);
    }
}
```
:::

But this opens up other problems: I forgot to write `const` and so now I accidentally cast away `const`-ness unintentionally. Oops. Not to mention that this cast actually works regardless of whether `base` refers to a base class of `T`, so it's not exactly the best programming practice.

On top of that, both the `static_cast` and C-style cast approaches suffer from having to correctly spell the destination type - which requires manually propagating the const-ness and value category of the object.

The way to avoid all of these problems is to just defer to a function template:

::: std
```cpp
template <std::meta::info M, class T>
constexpr auto subobject_cast(T&& arg) -> auto&& {
    constexpr auto stripped = remove_cvref(^T);
    if constexpr (is_base(M)) {
        static_assert(is_base_of(type_of(M), stripped));
        return (typename [: copy_cvref(^T, type_of(M)) :])arg;
    } else {
        static_assert(parent_of(M) == stripped);
        return ((T&&)arg).[:M:];
    }
}

template <class T, class F>
void for_each_subobject(T const& obj, F f) {
    template for (constexpr auto sub : subobjects_of(^T)) {
      f(subobject_cast<sub>(obj));
    }
}
```
:::

But this feels a bit silly? Why should we have to write this?

# Proposal

We propose to define `obj.[:mem:]` (where `mem` is a reflection of a base class of the type of `obj`) as being an access to that base class subobject, in the same way that `obj.[:nsdm:]` (where `nsdm` is a reflection of a non-static data member) is an access to that data member.

Additionally `&[:mem:]` where `mem` is a reflection of a base class `B` of type `T` should yield a `B T::*` with appropriate offset.

We argue that these are the obvious, useful, and only possible meanings of these syntaxes, so we should simply support them in the language.

The only reason this isn't initially part of [@P2996R3] is that while there _is_ a way to access a data member of an object directly (just `obj.mem`), there is _no_ way to access a base class subobject directly outside of one of the casts described above. Part of the reason for this is that while a data member is always just an `$identifier$`, a base class subobject can have an arbitrary complex name.

This means that adding this support in reflection would mean that splicing can achieve something the language cannot do natively. But we don't really see that as a problem. Reflection is already allowing all sorts of things that the language cannot do natively. What's one more?

---
references:
  - id: P2996R3
    citation-label: P2996R3
    title: "Reflection for C++26"
    author:
      - family: Barry Revzin
      - family: Wyatt Childers
      - family: Peter Dimov
      - family: Andrew Sutton
      - family: Faisal Vali
      - family: Daveed Vandevoorde
      - family: Dan Katz
    issued:
      - year: 2024
        month: 05
        day: 16
    URL: https://wg21.link/p2996r3
---
