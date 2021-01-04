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

The goal of this paper is to make that second case, and others like it, valid.

## Wait, why?

The reason is that in order for `array_size(param)` to work, we have to pass that reference to param into array_size - and that involves “reading” the reference. The specific rule we’re violating is [expr.const]{.sref}/5.12:

::: bq
[5]{.pnum} An expression `E` is a _core constant expression_ unless the evaluation of `E`, following the rules of the abstract machine ([intro.execution]), would evaluate one of the following: 

- [5.12]{.pnum} an _id-expression_ that refers to a variable or data member of reference type unless the reference has a preceding initialization and either 
    - [5.12.1]{.pnum} it is usable in constant expressions or
    - [5.12.2]{.pnum} its lifetime began within the evaluation of `E`;
:::


The reason we violate the reference rule is due to the underlying principle that the constant evaluator has to reject all undefined behavior, so the compiler has to check that all references are valid.

This would be more obvious if our situation used pointers instead of references:

```cpp
template <typename T, size_t N>
constexpr size_t array_size(T (*)[N]) {
    return N;
}

void check(int const (*param)[3]) {
    constexpr auto s2 = array_size(param); // error
}
```

This case _has_ to be ill-formed, copying a function parameter during constant evaluation means it has to itself be a constant expression, and function parameters are not constant expressions - even in `constexpr` or `consteval` functions.

But if the `param` case is ill-formed, why does the `local` case work? An unsatisfying answer is that… there just isn’t any rule in [expr.const] that we’re violating. There’s no lvalue-to-rvalue conversion (we’re not reading through the reference in any way yet) and we’re not referring to a reference (that’s the previous rule we ran afoul of). With the `param` case, the compiler cannot know whether the reference is valid, so it must reject. With the `local` case, the compiler can see for sure that the reference to `local` would be a valid reference, so it’s happy.

Notably, the rule we’re violating is only about _references_. We can’t write a function that takes an array by value, so let’s use the next-best thing: `std::array` and use the standard library’s `std::size` (cppref):

```cpp
void check_arr_val(std::array<int, 3> const param) {
    std::array<int, 3> local = {1, 2, 3};
    constexpr auto s3 = std::size(local); // ok
    constexpr auto s4 = std::size(param); // ok
}
```

If `param` were a reference, the initialization of `s4` would be ill-formed (for the same reason as previously), but because it’s a value, this is totally fine.

So as long as you pass all your containers around by value, you’re able to use get and use the size as a constant expression. Which is the kind of thing that’s intellectually interesting, but also wildly impractical because obviously nobody’s about to start passing all their containers around _by value_.

## Other Examples

Here are few other cases, which currently are ill-formed because of this reference-to-unknown rule. 

From Andrzej Krzemienski:

::: bq
Another situation where being able to use a reference to a
non-core-constant object is wen I am only interested in the type of the
reference rather than the value of the object: 

```cpp
template <typename T, typename U>
constexpr bool is_type(U &&)
{
    return std::is_same_v<T, std::decay_t<U>>;
}
```

So that I can use it like this: 

```cpp
auto visitor = [](auto&& v) {
    if constexpr(is_type<Alternative1>(v)) {
        // ...
    } else if constexpr(is_type<Alternative2>(v)) {
        // ...
    }
}; 
```

I can do it with a macro: 

```cpp
#define IS_TYPE(TYPE, EXPR) (std::is_same_v<TYPE, std::decay_t<decltype(EXPR)>>)
```
:::

From Jonathan Wakely:

::: bq
```cpp
auto rando(std::uniform_random_bit_generator auto& g)
{
  if constexpr (std::has_single_bit(g.max() - g.min()))
    // ...
  else
    // ...
} 
```

The concept requires that `g.max()` and `g.min()` are constexpr static member
functions, so this should work. And if I did it with an object of that
type, it would work. But because `g` is a reference, it's not usable in a
constant expression. That makes it awkward to refactor code into a function
(or function template), because what worked on the object itself doesn't
work in a function that binds a reference to that object.

I can rewrite it as something like:

```cpp
using G = remove_reference_t<decltype(g)>;
if constexpr (std::has_single_bit(G::max() - G::min()))
```

Or avoid abbreviated function syntax so I have a name for the type:

```cpp
template<std::uniform_random_bit_generator G>
auto rando(G& g)
{
  if constexpr (std::has_single_bit(G::max() - G::min()))
}
```

But it's awkward that the first version doesn't Just Work. 
:::

Another from me:

::: bq
I have a project that has a structure like:

```cpp
template <typename... Types>
struct Widget {
    struct Config : Types::config... {
        template <typename T>
        static constexpr auto sends(T) -> bool {
            return std::is_base_of_v<typename T::config, Config>;
        }
    };
    
    Config config;
};
```

With the intent that this function makes for a nice and readable way of doing dispatch:

```cpp
void do_configuration(auto& config) {
    // the actual type of config is... complicated
    
    if constexpr (config.sends(Goomba{})) {
        // do something
    }
    if constexpr (config.sends(Paratroopa{})) {
        // do something else
    }
}
```

Except this doesn't work, and I have to write:

```cpp
void do_configuration(auto& config) {
    using Config = std::remove_cvref_t<decltype(config)>;
    
    if constexpr (Config::sends(Goomba{})) {
        // ...
    }
```

Which is not really "better."
:::

What all of these examples have in common is that they are using a reference to an object of type `T` but do not care at all about the identity of that object. We're either querying properties of the type, invoking static member functions, or even when invoking a non-static member function (as in `std::array::size`), not actually accessing any non-static data members. The result would be the same for every object of type `T`... so if the identity doesn't change the result, why does the lack of identity cause the result to be non-constant? It's very much constant. 

# Proposal

The proposal is to allow these cases to just work. That is, if during constant evaluation, we run into a reference with unknown origin, this is still okay, we keep going. If we ever perform an operation that actually _needs_ this address, fail at that point. 

Some operations are allowed to propagate a reference-to-unknown node (such as class member access or derived-to-non-virtual-base conversions). But most operations are definitely non-constant (such as lvalue-to-rvalue conversion, assignment, any polymorphic operations, conversion to a virtual base class, etc.). This paper is _just_ proposing allowing those cases that work irrespective of the value of the reference (i.e. those that are truly constant), so any operation that depends on the value in any way needs to continue to be forbidden.

Notably, this paper is definitively _not_ proposing any kind of short-circuiting evaluation. For example:

```cpp
constexpr auto g() -> std::array<int, 10>&;
static_assert(g().size() == 10);
```

This check still must evaluate `g()`, which may or may not be a constant expression in its own right, even if `g().size()` is "obviously" 10. This paper is focused solely on those cases where we have an _id-expression_ of reference type.

## Implementation Experience

I've implemented this in EDG at least to the extent that the test cases prestend in this paper all pass, whereas previously they had all failed. 

## Wording

My impression is that simply removing the offending rule might be sufficient. That is, all the cases that we need to reject should already be rejected by other means (e.g. reading through the reference is already covered, in much the same words, by 5.8, reproduced below for clarity). Perhaps it is enough to simply strike [expr.const]{.sref}/5.12.

::: bq
[5]{.pnum} An expression `E` is a _core constant expression_ unless the evaluation of `E`, following the rules of the abstract machine ([intro.execution]), would evaluate one of the following: 

- [5.1]{.pnum} [...]
- [5.7]{.pnum} [...]
- [5.8]{.pnum} an lvalue-to-rvalue conversion unless it is applied to 
    - [5.8.1]{.pnum} a non-volatile glvalue that refers to an object that is usable in constant expressions, or
    - [5.8.2]{.pnum} a non-volatile glvalue of literal type that refers to a non-volatile object whose lifetime began within the evaluation of `E`
- [5.9]{.pnum} [...]
- [5.10]{.pnum} [...]
- [5.11]{.pnum} an invocation of an implicitly-defined copy/move constructor or copy/move assignment operator for a union whose active member (if any) is mutable, unless the lifetime of the union object began within the evaluation of `E`;
- [5.12]{.pnum} [an _id-expression_ that refers to a variable or data member of reference type unless the reference has a preceding initialization and either]{.rm}
    - [5.12.1]{.pnum} [it is usable in constant expressions or]{.rm}
    - [5.12.2]{.pnum} [its lifetime began within the evaluation of `E`;]{.rm}
- [5.13]{.pnum} in a _lambda-expression_, a reference to `this` or to a variable with automatic storage duration defined outside that _lambda-expression_, where the reference would be an odr-use; 
- [5.14]{.pnum} [...]
:::

# Acknowledgments

Thanks to Daveed Vandevoorde for the encouragement and help. Thanks to Richard Smith for carefully describing the correct rule on the reflector. Thanks to Michael Park for pointing out the issue to me, Tim Song for explaining it, and Jonathan Wakely for suggesting I pursue it. 