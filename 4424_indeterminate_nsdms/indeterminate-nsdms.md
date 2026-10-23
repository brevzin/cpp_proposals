---
title: "`[[indeterminate]]` for non-static data members"
document: P4424R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
---

# Introduction

[@P2795R5]{.title} was adopted for C++26 and implemented in gcc 16. In practice, it ensures that automatic storage duration variables are initialized unless they are marked `[[indeterminate]]` — both [gcc](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-ftrivial-auto-var-init) and [clang](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-ftrivial-auto-var-init) implement the flag `-ftrivial-auto-var-init`, which specifies how automatic storage variables are initialized. GCC in C++26 mode defaults to `-ftrivial-auto-var-init=zero`, which means the requirements of the feature. As also originally proposed in [@P2723R1]{.title}.

Now, the issue is that _only_ block storage variables can be marked `[[indeterminate]]`. And, for some reason, function parameters. But mainly local variables. That's straightforward enough when declaring a local `char buffer[1'024]` to read in some input. But what if you're using a type that, as an implementation detail, has a local buffer?

Consider the `{fmt}` library. One of its types, which is used both internally across every formatting path but is also intended to be used publicly, is `fmt::memory_buffer`. Its declaration [starts with](https://github.com/fmtlib/fmt/blob/0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f/include/fmt/format.h#L946-L967):

::: std
```cpp
// The number of characters to store in the basic_memory_buffer object itself
// to avoid dynamic memory allocation.
enum { inline_buffer_size = 500 };

/**
 * A dynamically growing memory buffer for trivially copyable/constructible
 * types with the first `SIZE` elements stored in the object itself. Most
 * commonly used via the `memory_buffer` alias for `char`.
 *
 * **Example**:
 *
 *     auto out = fmt::memory_buffer();
 *     fmt::format_to(std::back_inserter(out), "The answer is {}.", 42);
 *
 * This will append "The answer is 42." to `out`. The buffer content can be
 * converted to `std::string` with `to_string(out)`.
 */
template <typename T, size_t SIZE = inline_buffer_size,
          typename Allocator = detail::allocator<T>>
class basic_memory_buffer : public detail::buffer<T> {
 private:
  T store_[SIZE];
  // ...
};

// ...

using memory_buffer = basic_memory_buffer<char>;
```
:::

That is, `fmt::memory_buffer` has a data member of type `char[500]`. That buffer is _intended_ to be uninitialized. It is never read before it is written, and by now surely this library has been used by enough people over enough time that we can trust that it gets this right.

The problem is, in C++26, that buffer becomes initialized. gcc zeroes it by default. If I look at the code in the example in the comment, between C++23 and C++26, you can [see the memset](https://compiler-explorer.com/z/cG5EocGf5) in the codegen (I abbreviated some function names here for clarity):

::: cmptable
### C++23
```asm
"example(int)":
        push    r14
        mov     eax, edi
        xor     r9d, r9d
        mov     esi, OFFSET FLAT:.LC1
        push    rbx
        mov     edx, 17
        sub     rsp, 568
        movdqa  xmm0, XMMWORD PTR .LC0[rip]
        mov     DWORD PTR [rsp], eax
        mov     eax, 1
        lea     rbx, [rsp+48]
        mov     r8, rsp
        lea     rdi, [rsp+16]
        mov     rcx, rax
        mov     QWORD PTR [rsp+16], rbx
        mov     QWORD PTR [rsp+40], OFFSET FLAT:"grow"
        movups  XMMWORD PTR [rsp+24], xmm0
        call    "vformat_to"
        mov     rsi, QWORD PTR [rsp+24]
        mov     rdi, QWORD PTR [rsp+16]
        call    "do_something_with(char const*, unsigned long)"
        mov     rdi, QWORD PTR [rsp+16]
        cmp     rdi, rbx
        je      .L7
        call    "free"
.L7:
        add     rsp, 568
        pop     rbx
        pop     r14
        ret
        mov     r14, rax
        jmp     .L9
```

### C++26
```asm
"example(int)":
        push    rbp
        mov     edx, 536
        mov     ebp, edi
        xor     esi, esi
        push    rbx
        sub     rsp, 568
        lea     rdi, [rsp+16]
        lea     rbx, [rsp+48]
        call    "memset"
        mov     eax, 1
        mov     r8, rsp
        xor     r9d, r9d
        mov     rcx, rax
        mov     esi, OFFSET FLAT:.LC1
        mov     edx, 17
        mov     DWORD PTR [rsp], ebp
        movdqa  xmm0, XMMWORD PTR .LC0[rip]
        lea     rdi, [rsp+16]
        mov     QWORD PTR [rsp+16], rbx
        mov     QWORD PTR [rsp+40], OFFSET FLAT:grow"
        movups  XMMWORD PTR [rsp+24], xmm0
        call    "vformat_to"
        mov     rsi, QWORD PTR [rsp+24]
        mov     rdi, QWORD PTR [rsp+16]
        call    "do_something_with(char const*, unsigned long)"
        mov     rdi, QWORD PTR [rsp+16]
        cmp     rdi, rbx
        je      .L7
        call    "free"
```
:::

That cost shows up in benchmarks — on my machine it was roughly a 10ns fixed overhead. Which given that some `format` calls only take 15-25ns, is a significant percentage of the total cost.

Now, within the `{fmt}` library this is solvable. `memory_buffer` is constructed in about a dozen places, it's easy enough to find them and mark them `FMT_INDETERMINATE`. But _outside_ of `{fmt}`, this is a public type. Users would have to know this is a type they have to annotate. That seems very unlikely to happen with regularity, so everyone will just pay this cost.

There has to be a better way to handle this. Until then, I think we have to build our code with `-ftrivial-auto-var-init=uninitialized`.

# Proposal

Currently, `[[indeterminate]]` can only be applied to local variables and function parameters. This paper proposes that it also be able to be applied on non-static data members, with the obvious meaning. That allows types like `fmt::basic_memory_buffer` to annotate their uninitialized members (`store_`), so that only one place needs to be annotated by only one person: the class author. Nobody has to pay cost for initializing memory that need not be initialized.

Note that this isn't even only useful for members that are intentionally uninitialized. Consider:

::: std
```cpp
class Container {
    char buffer[1'024];

public:
    Container();
};
```
:::

What does `Container()` do? Well, you don't know. It happens to actually initialize `buffer`, but that happens in `container.cxx`, not in `container.hh`, so when compiling some translation units that use `Container`, the compiler doesn't know that, and so it helpfully zeroes it out first, before invoking the constructor which _also_ happens to zero it. Marking `Container::buffer` as `[[indeterminate]]` here would avoid that extra initialization as well.

## Wording

Change [dcl.attr.indet]{.sref}:

::: {.std .wording}
[1]{.pnum} The `$attribute-token$` `indeterminate` may be applied to the definition of a block variable with automatic storage duration[ or]{.rm} [,]{.addu} to a `$parameter-declaration$` of a function declaration[.]{.rm} [, or to the declaration of a non-static data member]{.addu}.
No `$attribute-argument-clause$` shall be present.
[The]{.rm} [When applied to the definition of a variable or to a `$parameter-declaration$`, the]{.addu} attribute specifies that the storage of an object with automatic storage duration is initially indeterminate rather than erroneous ([basic.indet]). [When applied to the declaration of a non-static data member M of a class type C, the attribute specifies that for any block variable with automatic storage duration of class type C, the storage associated with member M is initially indeterminate rather than erroneous.]{.addu}

[#]{.pnum} If a function parameter is declared with the `indeterminate` attribute, it shall be so declared in the first declaration of its function. If a function parameter is declared with the `indeterminate` attribute in the first declaration of its function in one translation unit and the same function is declared without the `indeterminate` attribute on the same parameter in its first declaration in another translation unit, the program is ill-formed, no diagnostic required ([ifndr:dcl.attr.indet.mismatched.declarations]).
:::

Bump the feature-test macro for the attribute in [cpp.cond]{.sref}:

::: {.std .wording}
```diff
- indeterminate 202403L
+ indeterminate 2026XXL
```
:::