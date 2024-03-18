---
title: "`constexpr` placement new"
document: P2747R2
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

# Revision History

R0 [@P2747R0] of this paper proposed three related features:

1. Allowing casts from `$cv$ void*` to `$cv$ T*` during constant evaluation
2. Allowing placement new during constant evaluation
3. Better handling an array of uninitialized objects

Since then, [@P2738R1] was adopted in Varna, which resolves problem #1. Separately, #3 is kind of a separate issue and there are ongoing conversations about how to handle this in order to make `inplace_vector` [@P0843R9] actually during during constant evaluation for all types. So this paper refocuses to just solve problem #2 and has been renamed accordingly.

Since [@P2747R1], fixed the wording.

# Introduction

Consider this implementation of `std::uninitialized_copy`, partially adjusted from [cppreference](https://en.cppreference.com/w/cpp/memory/uninitialized_copy):

::: bq
```cpp
template <input_iterator I, sentinel_for<I> S, nothrow_forward_iterator I2>
constexpr auto uninitialized_copy(I first, S last, I2 d_first) -> I2 {
    using T = iter_value_t<I2>;
    I2 current = d_first;
    try {
        for (; first != last; ++first, (void)++current) {
            ::new (std::addressof(*current)) T(*first);
        }
    } catch (...) {
        std::destroy(d_first, current);
        throw;
    }
}
```
:::

This fails during constant evaluation today because placement new takes a `void*`. But it takes a `void*` that points to a `T` - we know that by construction. It's just that we happen to lose that information along the way.

Moreover, that's not _actually_ how `uninitialized_copy` is specified, we actually do this:

::: bq
```diff
- ::new (std::addressof(*current)) T(*first);
+ ::new ($voidify$(*current)) T(*first);
```
:::

where:

::: bq
```cpp
template<class T>
  constexpr void* $voidify$(T& obj) noexcept {
    return const_cast<void*>(static_cast<const volatile void*>(std::addressof(obj)));
  }
```
:::

Which exists to avoid users having written a global placement new that takes a `T*`.


The workaround, introduced by [@P0784R7], is a new library function:

::: bq
```cpp
template<class T, class... Args>
constexpr T* construct_at( T* p, Args&&... args );
```
:::

This is a magic library function that is specified to do the same `$voidify$` dance, but which the language simply recognizes as an allowed thing to do. `std::construct_at` is explicitly allowed in [\[expr.const\]/6](https://eel.is/c++draft/expr.const):

> [6]{.pnum} [...] Similarly, the evaluation of a call to `std​::​construct_­at` or `std​::​ranges​::​construct_­at` ([specialized.construct]) does not disqualify `E` from being a core constant expression unless the first argument, of type `T*`, does not point to storage allocated with `std​::​allocator<T>` or to an object whose lifetime began within the evaluation of `E`, or the evaluation of the underlying constructor call disqualifies `E` from being a core constant expression.

It's good that we actually have a solution - we can make `uninitialized_copy` usable during constant evaluation simply by using `std::construct_at`. There's even a paper to do so ([@P2283R2]). But that paper also had hinted at a larger problem: `std::construct_at` is an _extremely_ limited tool as compared to placement new.

Consider the different kinds of initialization we have in C++:

|kind|placement new|`std::construct_at`|
|-|-|---|
|value initialization|`new (p) T(args...)`|`std::construct_at(p, args...)`|
|default initialization|`new (p) T`|Not currently possible. [@P2283R1] proposed `std::default_construct_at`|
|list initialization|`new (p) T{a, b}`|Not currently possible, could be a new function?|
|designated initialization|`new (p) T{.a=a, .b=b}`|Not possible to even write a function|

That's already not a great outlook for `std::construct_at`, but for use-cases like `uninitialized_copy`, we have to also consider the case of guaranteed copy elision:

::: bq
```cpp
auto get_object() -> T;

void construct_into(T* p) {
    // this definitely moves a T
    std::construct_at(p, get_object());

    // this definitely does not move a T
    :::new (p) T(get_object());

    // this now also definitely does not move a T, but it isn't practical
    // and you also have to deal with delightful edge cases - like what if
    // T is actually constructible from defer?
    struct defer {
        constexpr operator T() const { return get_object(); }
    };
    std::construct_at(p, defer{});
}
```
:::

Placement new is only unsafe because the language allows you to do practically anything - want to placement new a `std::string` into a `double*`? Sure, why not. But during constant evaluation we already have a way of limiting operations to those that make sense - we can require that the pointer we're constructing into actually is a `T*`. The fact that we have to go through a `void*` to get there doesn't make it unsafe.

Now that we have support for `static_cast<T*>(static_cast<void*>(p))`, we can adopt the same rules to make placement new work.

# Wording

Today, we have an exception for `std::construct_at` and `std::ranges::construct_at` to avoid evaluating the placement new that they do internally. But once we allow placement new, we no longer need an exception for those cases - we simply need to move the lifetime requirement from the exception into the general rule for placement new.

Change [expr.new]{.sref}/15:

::: bq
[15]{.pnum} During an evaluation of a constant expression, a call to [an]{.rm} [a replaceable]{.addu} allocation function is always omitted [([expr.const])]{.addu}.

::: rm
[ Only new-expressions that would otherwise result in a call to a replaceable global allocation function can be evaluated in constant expressions ([expr.const]).]{.note}
:::
:::

Change [expr.const]{.sref}/5.18 (paragraph 14 here for context was the [@P2738R1] fix to allow converting from `void*` to `T*` during constant evaluation, as adjusted by [@CWG2755]):

::: bq
* [5.14]{.pnum} a conversion from a prvalue `P` of type “pointer to cv `void`” to a "`$cv1$` pointer to `T`", where `T` is not `$cv2$ void`, unless `P` points to an object whose type is similar to `T`;
* [5.15]{.pnum} ...
* [5.16]{.pnum} ...
* [5.17]{.pnum} ...
* [5.18]{.pnum} a *new-expression* ([expr.new]{.sref}), unless [either]{.addu}
  * [5.18.1]{.pnum} the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is deallocated within the evaluation of `E`[, or]{.addu}
  * [5.18.2]{.pnum} [the selected allocation function is a non-allocating form ([new.delete.placement]) with an allocated type `T`, where]{.addu}
    * [5.18.2.1]{.pnum} [the placement argument to the *new-expression* points to an object that is pointer-interconvertible with an object of type `T`, and]{.addu}
    * [5.#.#.#]{.pnum} [the placement argument points to storage whose duration began within the evaluation of `E`]{.addu};
:::

Remove the special case for `construct_at` in [expr.const]{.sref}/6:

::: bq
* [6]{.pnum} For the purposes of determining whether an expression `E` is a core constant expression, the evaluation of the body of a member function of `std​::​allocator<T>` as defined in [allocator.members], where T is a literal type, is ignored. [Similarly, the evaluation of the body of `std​::​construct_at` or `std​::​ranges​::​construct_at` is considered to include only the initialization of the `T` object if the first argument (of type `T*`) points to storage allocated with `std​::​allocator<T>` or to an object whose lifetime began within the evaluation of `E`.]{.rm}
:::

Change [new.syn]{.sref} to mark the placement new functions `constexpr`:

::: bq
```
// all freestanding
namespace std {
// [new.delete], storage allocation and deallocation
[[nodiscard]] @[constexpr]{.addu}@ void* operator new  (std::size_t size, void* ptr) noexcept;
[[nodiscard]] @[constexpr]{.addu}@ void* operator new[](std::size_t size, void* ptr) noexcept;
}
```
:::

And likewise in [new.delete.placement]{.sref}:

::: bq
```
[[nodiscard]] @[constexpr]{.addu}@ void* operator new(std::size_t size, void* ptr) noexcept;
```
[2]{.pnum} *Returns*: `ptr`.

...
```
[[nodiscard]] @[constexpr]{.addu}@ void* operator new[](std::size_t size, void* ptr) noexcept;
```
[5]{.pnum} *Returns*: `ptr`.
:::
