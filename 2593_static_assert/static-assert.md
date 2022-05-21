---
title: "Allowing `static_assert(false)`"
document: P2593R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

Consider the two functions below:

::: cmptable
### Runtime
```cpp
template <class T>
void do_something(T t) {
  if (is_widget(t)) {
    use_widget(t);
  } else if (is_gadget(t)) {
    use_gadget(t);
  } else {
    assert(false);
  }
}
```

### Compile time
```cpp
template <class T>
void do_something(T t) {
  if constexpr (is_widget<T>) {
    use_widget(t);
  } else if constexpr (is_gadget<T>) {
    use_gadget(t);
  } else {
    static_assert(false);
  }
}
```
:::

The code on the left is fairly unremarkable. Having a bunch of runtime conditions that are intended to be exhaustive with a trailing `assert(false);` is not an uncommon pattern, in C++ or in C. It's just: if I get here, it's a bug, so assert here.

The code on the right is also fairly unremarkable, simply the compile-time version of the code on the left, where all the checks can be done statically. This is safer, since we can verify _at compile time_ that we actually covered all the cases, ensuring that any bugs are compile errors rather than runtime errors. This is great. People expect the code on the right to do roughly the same as the code on the left, just reporting the error earlier.

Except while the code on the left works, the code on the right is ill-formed, no diagnostic required. And, in practice, all compilers diagnose this case immediately.

But users still want to write code using this structure. Sure, in this particular case, they can rewrite it:

::: cmptable
### Desired structure (IFNDR)
```cpp
template <class T>
void do_something(T t) {
  if constexpr (is_widget<T>) {
    use_widget(t);
  } else if constexpr (is_gadget<T>) {
    use_gadget(t);
  } else {
    static_assert(false);
  }
}
```

### Working structure (✔️)
```cpp
template <class T>
void do_something(T t) {
  if constexpr (is_widget<T>) {
    use_widget(t);
  } else {
    static_assert(is_gadget<T>);
    use_gadget(t);
  }
}
```
:::

But in other cases that might not be possible. Such as wanting to ensure that the primary template is not instantiated - there's no convenient, ready-made condition to put here:

::: bq
```cpp
template <class T>
struct should_be_specialized {
  static_assert(false, "this isn't the specialization you're looking for");
};
```
:::

## The Actual Rule

The actual rule that `static_assert(false)` runs afoul of is [temp.res]{.sref}/6:

::: bq
[6]{.pnum} The validity of a template may be checked prior to any instantiation.

[*Note 3*: Knowing which names are type names allows the syntax of every template to be checked in this way.
— *end note*]

The program is ill-formed, no diagnostic required, if:

* [6.1]{.pnum} no valid specialization can be generated for a template or a substatement of a constexpr if statement within a template and the template is not instantiated, or
* [6.2]{.pnum} ...
:::

Since there is, indeed, no valid specialization that would case `static_assert(false)` to be valid, the program is ill-formed, no diagnostic required.

But... that's the goal of the code, to not have a valid specialization in this case! It's just that the obvious solution to the goal happens to cause us to not even have a program.

## Workarounds

As a result of `static_assert(false)` not working, people turn to workarounds. Which are, basically: how can I write `false` in a sufficiently complex way so as to confuse the compiler?

Some of these workarounds actually still do run afoul of the above rule, but compilers aren't smart enough to diagnose them, so they Just Happen To Work. Checks like:

* `static_assert(sizeof(T) == 0);`
* `static_assert(sizeof(T*) == 0);`
* `[]<bool flag=false>(){ static_assert(flag); }();` from [this answer](https://stackoverflow.com/a/64354296/2069064).

These are all terrible. And also wrong. But they happen to get the job done so people use them.

## Valid Workaround

A more valid workaround is to have some template that is always false (e.g. from [this answer](https://stackoverflow.com/a/14637534/2069064)):

::: bq
```cpp
static_assert(always_false<T>::value);
```
:::

This is also terrible, but as long as the condition is dependent, there could hypothetically be some instantiation that is `true`, and thus we don't run afoul of the \[temp.res\] rule. So it is technically valid. This is really the only real workaround for this problem, and is something that people have to be taught explicitly as a workaround.

## History

There was a proposal to add this to the standard library [@P1830R1]. The problem with having a library solution to this problem is that you have to handle various kinds. It's not enough to have `always_false_v<Type>` since that would require having to write `always_false_v<integral_constant<decltype(V), V>>` if what you happen to have is a value. And if all you have is a class template, this is even worse. You need [@P1985R1] simply to even claim to be able to solve this problem in the library.

This was followed up by a language proposal to make `static_assert` more dependent [@P1936R0] - which proposed `static_assert<T>(false)`. This has the same issues with kind as the library solution, but also is just adding more stuff to `static_assert` that still needs to be taught. Sometimes you need this extra stuff, sometimes you don't?

# Proposal

The proposal here is quite simple. `static_assert(false)` should just work.

Change [temp.res]{.sref}/6:

::: bq
[6]{.pnum} The validity of a template may be checked prior to any instantiation.

[*Note 3*: Knowing which names are type names allows the syntax of every template to be checked in this way.
— *end note*]

The program is ill-formed, no diagnostic required, if:

* [6.1]{.pnum} [ignoring all <i>static-assert-declaration</i>s,]{.addu} no valid specialization can be generated for a template or a substatement of a constexpr if statement within a template and the template is not instantiated, or
* [6.2]{.pnum} ...
:::

This sidesteps the question of whether it's _just_ `static_assert(false)` that should be okay or `static_assert(0)` or `static_assert(1 - 1)` or `static_assert(not_quite_dependent_false)`. anything else. Just, all `static_assert` declarations should be delayed until the template (or appropriate specialization or constexpr if substatement thereof) is actually instantiated.

If the condition is false, we're going to get a compiler error anyway. And that's fine! But let's just actually fail the program when it's actually broken, and not early.

## Implementation Experience

I implemented this in EDG. It's a two line code change: simply don't try to diagnose `static_assert` declarations if we're still in a template dependent context. Wait until they're instantiated.

# Acknowledgements

Thanks to Daveed Vandevoorde for helping with the implementation.

Thanks to all the people over the years to whom I've had to explain why `static_assert(false);` doesn't work and what they have to write instead for not immediately destroying their computers and switching to Rust.
