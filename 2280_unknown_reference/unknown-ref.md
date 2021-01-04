---
title: "Using unknown references in constant expressions"
document: D2280R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

Let's say I have an array and want to get its size as a constant expression. In C, I had to write a macro:

```cpp
#define ARRAY_SIZE(a) (sizeof(a)/sizeof(a[0]))
```

But in C++, we should be able to do better. We have `constexpr` and templates, so we can use them:

```cpp
template <typename T, size_t N>
constexpr auto array_size(T (&)[N]) -> size_t {
    return N;
}
```

This seems like it should be a substantial improvement, yet it has surprising limitations:

```cpp
void check(int const (&param)[3]) {
    int local[] = {1, 2, 3};
    constexpr auto s0 = array_size(local); // ok
    constexpr auto s1 = array_size(param); // error
}
```

The goal of this paper is to make that second case valid.

## Wait, why?

Pass.
