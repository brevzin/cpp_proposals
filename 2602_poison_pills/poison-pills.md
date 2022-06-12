---
title: "Poison Pills are Too Toxic"
document: P2602R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tags: ranges
---

# Introduction

Given the following declarations:

::: bq
```cpp
struct A {
    friend auto begin(A const&) -> int const*;
    friend auto end(A const&)   -> int const*;
};

struct B {
    friend auto begin(B&) -> int*;
    friend auto end(B&) -> int*;
};
```
:::

`B` and `const A` satisfy `std::ranges::range`, but `A` does not. The goal of this paper is that both of these should count as ranges.

# History

During the design of Ranges, originally rvalues were porhibited entirely [@stl2.429]. Then, there was a desire ([@stl2.547])

> to force range types to opt in to working with rvalues, thereby giving users a way to detect that, for a particular range type, iterator validity does not depend on the range's lifetime.


The problem is (as [@P0970R1] demonstrates), when you have a construction like

::: bq
```
struct Buffer { /* ... */ };

char* begin(Buffer&);
const char* begin(const Buffer&);
char* end(Buffer&);
const char* end(const Buffer&);
```
:::

While `begin(Buffer{})` is valid, it's only valid because it binds to a const lvalue reference, and that doesn't actually offer any information as to whether the iterator would remain valid after the range is destroyed. That paper addressed this problem by introducing the concept of poison pill overloads. `std::ranges::begin(E)`, for rvalues `E`, will never consider `E.begin()` (since none of thoese are going to be ref-qualified) and will only consider overloads of `begin(E)` looked up in a context that includes:

::: bq
```cpp
// *Poison pill* overload:
template <class T>
void begin(T&&) = delete;
```
:::

Here, `begin(Buffer{})` would prefer the poison pill, so we avoid that particular problem. `Buffer` is not a *borrowed* range (this is not the term P0970 used, but in an effort to limit confusion, I'll stick to the term we now have for this idea).

One reason this poison pill is necessary is that without it, `begin(std::vector<int>())` would happily find the `std::begin` overload that invokes member `begin()`, which is something we're trying to avoid.

As [@P1870R1] pointed out, this is a very subtle distinction which makes it difficult to even properly opt into this concept. That paper proposed what is now the current opt-in model for borrowed ranges: specializing a variable template. `ranges::begin(E)` still only ever invokes `begin` (member or non-member) on an lvalue, but it decides whether it is allowed to upgrade an rvalue to an lvalue based on `enable_borrowed_range`.

# Do we still need the poison pills?

Given the existence of `enable_borrowed_range`, are the poison pills still useful? Well, we don't need to prevent lookup for `begin(Buffer{})` finding `::begin` or lookup for `begin(std::vector<int>())` from finding `std::begin`, since these types aren't borrowed ranges anyway.

For types that are borrowed ranges, there isn't any particular harm in finding `std::begin`, since that simply forwards to member `begin` (the customization point for `begin` is defined in such a way that member `begin` takes priority to non-member `begin`, so had that one been valid we would have used it. Finding it doesn't add any value, but it also doesn't cause harm).

The poison pills were modified from one function template that takes a forwarding reference to two that take a const and non-const lvalue reference by [@P2091R0], which contained this description:

::: quote
While implementing this change, I realized that the forwarding-reference poison pills in the working draft
are insufficiently poisonous. `void foo(auto&&)` is less-specialized than either `void foo(auto&)` or `void foo(const auto&)`,
so a `void foo(auto&&)` poison pill fails to intercept/ambiguate calls to such overgeneric
lvalue functions as intended. We should fix the poison pills by replacing them with two lvalue overloads. (I’m
not certain the poison pills serve a useful design purpose anymore, and I’d like to remove them, but it’s too
late in the cycle for even so small a design change.)
:::

Like the parenthetical says, it's not clear what we actually need the poison pills to poison anymore. It's clear what we needed to poison originally, but that motivation no longer exists. Unfortunately, their current formulation does cause harm.

First, consider [@LWG3480]. In that issue, `std::filesystem::directory_iterator` and `std::filesystem::recursive_directory_iterator` both opted into being ranges via non-member functins that looked like this:

::: bq
```cpp
directory_iterator begin(directory_iterator iter) noexcept;
directory_iterator end(const directory_iterator&) noexcept;

recursive_directory_iterator begin(recursive_directory_iterator iter) noexcept;
recursive_directory_iterator end(const recursive_directory_iterator&) noexcept;
```
:::

The by-value overload was fine, but the reference-to-const one ends up being a worse match the `void end(auto&) = delete;` overload, so you end up in this state where `const directory_iterator` satisfies `range` but `directory_iterator` does not. This issue was resolved by simply making `end` for both of these iterator types take by value.

Second, consider [this example](https://stackoverflow.com/q/72548689/2069064) from StackOverflow recently:

::: bq
```cpp
class Test {
    friend size_t size(/*const*/ Test&) {
        return 0;
    }
};

size_t f(Test t) {
   return std::ranges::size(t);
}
```
:::

In order for this to compile, the `size` function must take a `Test&` (or a `Test`) and not a `const Test&`, even if no mutation is necessary. A member function `size() const` would've also sufficed, but this somewhat defeats the purpose of allowing both member and non-member opt-in. Notice that here we're not even dealing with rvalue ranges - we're trying to invoke `size` with an lvalue, but it still won't work.

With the status quo, the poison pills prevent reasonable code from working and it's entirely unclear whether they prevent unreasonable code from working.

# Proposal

There are two things we could do to improve the situation:

1. We could refine the poison pills further by ensuring that they only exist to ensure ADL-only lookup, by simply dropping their arguments entirely. That is, instead of `ranges::begin` considering the poison pills `void begin(auto&) = delete;` and `void begin(auto const&) = delete;`, we have the single poison pill `void begin() = delete;` This way, the poison pill is never the best (or even a viable) match - it simply enforces ADL lookup.

2. We could remove the poison pills entirely.

Given that there doesn't seem to be a remaining reason for the poison pills to exist, this paper proposes #2. Note that both options here would also have fixed [@LWG3480]. I'm happy to consider #1 if someone offers a still-valid reason to do this sort of thing, I'm simply unaware of such a reason and not for lack of searching.

## Wording

Change [range.access.begin]{.sref}:

::: bq
* [2.5]{.pnum} Otherwise, if `T` is a class or enumeration type and `auto(begin(t))` is a valid expression whose type models `input_­or_­output_­iterator` [with overload resolution performed in a context in which unqualified lookup for `begin` finds only the declarations]{.rm}

::: rm
```
void begin(auto&) = delete;
void begin(const auto&) = delete;
```
:::

then `ranges​::​begin(E)` is expression-equivalent to `auto(begin(t))` [with overload resolution performed in the above context]{.rm}.
:::

---
references:
    - id: stl2.429
      citation-label: stl2.429
      title: "Consider removing support for rvalue ranges from range access CPOs"
      author:
        - family: Casey Carter
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/429
    - id: stl2.547
      citation-label: stl2.547
      title: "Redesign begin/end CPOs to eliminate deprecated behavior"
      author:
        - family: Eric Niebler
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/547
    - id: stl2.592
      citation-label: stl2.592
      title: "`const subrange<I,S,[un]sized>` is not a _`forwarding-range`_"
      author:
        - family: Eric Niebler
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/592
    - id: P1871R0
      citation-label: P1871R0
      title: "Should concepts be enabled or disabled?"
      author:
        - family: Barry Revzin
      issued:
        - year: 2019
      URL: https://wg21.link/p1871r0
    - id: P1900R0
      citation-label: P1900R0
      title: "Concepts-adjacent problems"
      author:
        - family: Barry Revzin
      issued:
        - year: 2019
      URL: https://wg21.link/p1900r0
    - id: msvc.basic_string_view
      citation-label: msvc.basic_string_view
      title: "non-member `begin()`/`end()` for `basic_string_view`"
      issued:
        -year: 2019
      URL: https://github.com/microsoft/STL/blame/92508bed6387cbdae433fc86279bc446af6f1b1a/stl/inc/xstring#L1207-L1216
---
