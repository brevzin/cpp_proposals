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

[@P2795R5]{.title} was adopted for C++26 and implemented in gcc 16. In practice, it ensures that automatic storage duration variables are initialized unless they are marked `[[indeterminate]]` — both [gcc](https://gcc.gnu.org/onlinedocs/gcc/Optimize-Options.html#index-ftrivial-auto-var-init) and [clang](https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-ftrivial-auto-var-init) implement the flag `-ftrivial-auto-var-init`{.default}, which specifies how automatic storage variables are initialized. GCC in C++26 mode defaults to `-ftrivial-auto-var-init=zero`{.default}, which meets the requirements of the feature. As also originally proposed in [@P2723R1]{.title}.

Now, the issue is that _only_ block variables can be marked `[[indeterminate]]` ((and also function parameters). That's straightforward enough when declaring a local `char buffer[1'024]` to read in some input. But what if you're using a type that, as an implementation detail, has a local buffer?

## `fmt::memory_buffer`

Consider the `{fmt}` library. One of its types, which is used both internally across many formatting paths (it's what `vformat` uses) but is also intended to be used publicly, is `fmt::memory_buffer`. Its declaration [starts with](https://github.com/fmtlib/fmt/blob/0fc31633e7d53ac895ae0a688b83b7d44f0b0c1f/include/fmt/format.h#L946-L967):

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
        pop     rbp
        ret
        mov     rbp, rax
        jmp     .L9
```
:::

That cost shows up in benchmarks — on my machine it was roughly a 10ns fixed overhead. Which given that some `format` calls only take 15-25ns, is a significant percentage of the total cost.

Now, within the `{fmt}` library this is solvable. `memory_buffer` is constructed in about a dozen places, it's easy enough to find them and mark them `FMT_INDETERMINATE` (a macro wrapping `[[indeterminate]]`). But _outside_ of `{fmt}`, this is a public type. Users would have to know this is a type they have to annotate. That seems very unlikely to happen with regularity, so everyone will just pay this cost.

This is hardly the only example though, there are plenty of others.

## `std::string`

`fmt::memory_buffer` isn't the only example that has unnecessary overhead. `std::string` does as well — it has a union with local storage in the small string case. So an example like this, with a temporary, must still [zero that buffer](https://compiler-explorer.com/z/Y117ob6W7):

::: std
```cpp
auto use(std::string const&) -> void;

auto temp_from_call() -> void {
    use(std::string("hi"));
}
```
:::

::: cmptable
### C++23
```asm
"temp_from_call()":
        sub     rsp, 56
        mov     eax, 26984
        mov     QWORD PTR [rsp+40], rbx
        mov     rdi, rsp
        lea     rbx, [rsp+16]
        mov     QWORD PTR [rsp], rbx
        mov     WORD PTR [rsp+16], ax
        mov     BYTE PTR [rsp+18], 0
        mov     QWORD PTR [rsp+8], 2
        call    "use(...)"
        mov     rdi, QWORD PTR [rsp]
        cmp     rdi, rbx
        je      .L1
        mov     rax, QWORD PTR [rsp+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L1:
        mov     rbx, QWORD PTR [rsp+40]
        add     rsp, 56
        ret
        mov     QWORD PTR [rsp+48], r14
        mov     r14, rax
        jmp     .L3
```

### C++26
```asm
"temp_from_call()":
        sub     rsp, 56
        pxor    xmm0, xmm0
        mov     eax, 26984
        mov     QWORD PTR [rsp+40], rbx
        mov     rdi, rsp
        lea     rbx, [rsp+16]
        movaps  XMMWORD PTR [rsp+16], xmm0
        mov     QWORD PTR [rsp], rbx
        mov     WORD PTR [rsp+16], ax
        mov     QWORD PTR [rsp+8], 2
        call    "use(...)"
        mov     rdi, QWORD PTR [rsp]
        cmp     rdi, rbx
        je      .L1
        mov     rax, QWORD PTR [rsp+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L1:
        mov     rbx, QWORD PTR [rsp+40]
        add     rsp, 56
        ret
        mov     QWORD PTR [rsp+48], r14
        mov     r14, rax
        jmp     .L3
```
:::

And the same is true with local variable that [uses a small string](https://compiler-explorer.com/z/8sdxf9Yhv), though gcc is smart enough to group the zeroing together a bit:

::: std
```cpp
auto named_short() -> void {
    std::string s = "hi";
    use(s);
}
```
:::

::: cmptable
### C++23
```asm
"named_short()":
        sub     rsp, 56
        mov     eax, 26984
        mov     QWORD PTR [rsp+40], rbx
        mov     rdi, rsp
        lea     rbx, [rsp+16]
        mov     QWORD PTR [rsp], rbx
        mov     WORD PTR [rsp+16], ax
        mov     BYTE PTR [rsp+18], 0
        mov     QWORD PTR [rsp+8], 2
        call    "use(...)"
        mov     rdi, QWORD PTR [rsp]
        cmp     rdi, rbx
        je      .L1
        mov     rax, QWORD PTR [rsp+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L1:
        mov     rbx, QWORD PTR [rsp+40]
        add     rsp, 56
        ret
        mov     QWORD PTR [rsp+48], r14
        mov     r14, rax
        jmp     .L3
```

### C++26
```asm
"named_short()":
        sub     rsp, 56
        pxor    xmm0, xmm0
        mov     eax, 26984
        mov     QWORD PTR [rsp+40], rbx
        mov     rdi, rsp
        lea     rbx, [rsp+16]
        movaps  XMMWORD PTR [rsp+16], xmm0
        mov     QWORD PTR [rsp], rbx
        mov     WORD PTR [rsp+16], ax
        mov     QWORD PTR [rsp+8], 2
        call    "use(...)"
        mov     rdi, QWORD PTR [rsp]
        cmp     rdi, rbx
        je      .L1
        mov     rax, QWORD PTR [rsp+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L1:
        mov     rbx, QWORD PTR [rsp+40]
        add     rsp, 56
        ret
        mov     QWORD PTR [rsp+48], r14
        mov     r14, rax
        jmp     .L3
```
:::

And worse still [with an array](https://compiler-explorer.com/z/n3a93aG14):

::: std
```cpp
auto fill(std::string*) -> void;

auto parts() -> void {
    std::string parts[4];
    fill(parts);
}
```
:::

::: cmptable
### C++23
```asm
"parts()":
        sub     rsp, 152
        lea     rax, [rsp+16]
        mov     rdi, rsp
        mov     BYTE PTR [rsp+16], 0
        mov     QWORD PTR [rsp], rax
        lea     rax, [rsp+48]
        mov     QWORD PTR [rsp+32], rax
        lea     rax, [rsp+80]
        mov     QWORD PTR [rsp+64], rax
        lea     rax, [rsp+112]
        mov     QWORD PTR [rsp+136], rbp
        mov     rbp, rsp
        mov     QWORD PTR [rsp+8], 0
        mov     BYTE PTR [rsp+48], 0
        mov     QWORD PTR [rsp+40], 0
        mov     BYTE PTR [rsp+80], 0
        mov     QWORD PTR [rsp+72], 0
        mov     QWORD PTR [rsp+96], rax
        mov     BYTE PTR [rsp+112], 0
        mov     QWORD PTR [rsp+104], 0
        call    "fill(...)"
        mov     QWORD PTR [rsp+128], rbx
        lea     rbx, [rsp+128]
.L2:
        sub     rbx, 32
        mov     rdi, QWORD PTR [rbx]
        lea     rax, [rbx+16]
        cmp     rdi, rax
        je      .L5
        mov     rax, QWORD PTR [rbx+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L5:
        cmp     rbx, rbp
        jne     .L2
        mov     rbx, QWORD PTR [rsp+128]
        mov     rbp, QWORD PTR [rsp+136]
        add     rsp, 152
        ret
        mov     QWORD PTR [rsp+144], r14
        mov     r14, rax
        mov     QWORD PTR [rsp+128], rbx
        jmp     .L3
```

### C++26
```asm
"parts()":
        sub     rsp, 152
        pxor    xmm0, xmm0
        lea     rax, [rsp+16]
        mov     rdi, rsp
        mov     QWORD PTR [rsp+136], rbp
        mov     rbp, rsp
        mov     QWORD PTR [rsp], rax
        lea     rax, [rsp+48]
        mov     QWORD PTR [rsp+32], rax
        lea     rax, [rsp+80]
        mov     QWORD PTR [rsp+64], rax
        lea     rax, [rsp+112]
        mov     QWORD PTR [rsp+8], 0
        mov     QWORD PTR [rsp+40], 0
        mov     QWORD PTR [rsp+72], 0
        mov     QWORD PTR [rsp+96], rax
        mov     QWORD PTR [rsp+104], 0
        movaps  XMMWORD PTR [rsp+16], xmm0
        movaps  XMMWORD PTR [rsp+48], xmm0
        movaps  XMMWORD PTR [rsp+80], xmm0
        movaps  XMMWORD PTR [rsp+112], xmm0
        call    "fill(...)"
        mov     QWORD PTR [rsp+128], rbx
        lea     rbx, [rsp+128]
.L2:
        sub     rbx, 32
        mov     rdi, QWORD PTR [rbx]
        lea     rax, [rbx+16]
        cmp     rdi, rax
        je      .L5
        mov     rax, QWORD PTR [rbx+16]
        lea     rsi, [rax+1]
        call    "operator delete(void*, unsigned long)"
.L5:
        cmp     rbx, rbp
        jne     .L2
        mov     rbx, QWORD PTR [rsp+128]
        mov     rbp, QWORD PTR [rsp+136]
        add     rsp, 152
        ret
        mov     QWORD PTR [rsp+144], r14
        mov     r14, rax
        mov     QWORD PTR [rsp+128], rbx
        jmp     .L3
```
:::


## `std::inplace_vector<T, N>`

Similar behavior [can be seen](https://compiler-explorer.com/z/MT6vjaa94) using `std::inplace_vector`, which is a type explicitly designed around uninitialized inline storage. Now, this type didn't exist in C++23 yet, so the "C++23" version is instead compiling with `-ftrivial-auto-var-init=uninitialized`{.default} to disable the feature. You can see two `memset`s in the C++26 version, one of which can be avoided with `[[indeterminate]]`, but the other is a temporary so cannot be:

::: std
```cpp
auto use(std::inplace_vector<int, 64> const&) -> void;

auto one() -> void {
    std::inplace_vector<int, 64> v;
    v.push_back(1);
    use(v);
}

auto temp() -> void {
    use(std::inplace_vector<int, 64>{1, 2, 3});
}
```
:::

::: cmptable
### C++23
```asm
"one()":
        sub     rsp, 280
        mov     DWORD PTR [rsp+256], 0
        mov     rax, rsp
        mov     eax, DWORD PTR [rsp+256]
        cmp     eax, 63
        ja      .L3
        mov     DWORD PTR [rsp+rax*4], 1
        mov     rdi, rsp
        add     DWORD PTR [rsp+256], 1
        call    "use(std::inplace_vector<int, 64ul> const&)"
        add     rsp, 280
        ret
"temp()":
        sub     rsp, 280
        mov     DWORD PTR [rsp+256], 0
        mov     rdi, rsp
        mov     rax, rsp
        mov     rax, QWORD PTR "C.2.0"[rip]
        mov     DWORD PTR [rsp+256], 3
        mov     QWORD PTR [rsp], rax
        mov     eax, DWORD PTR "C.2.0"[rip+8]
        mov     DWORD PTR [rsp+8], eax
        call    "use(std::inplace_vector<int, 64ul> const&)"
        add     rsp, 280
        ret
```

### C++26
```asm
"one()":
        sub     rsp, 280
        mov     edx, 260
        xor     esi, esi
        mov     rdi, rsp
        call    "memset"
        mov     eax, DWORD PTR [rsp+256]
        cmp     eax, 63
        ja      .L3
        mov     DWORD PTR [rsp+rax*4], 1
        mov     rdi, rsp
        add     DWORD PTR [rsp+256], 1
        call    "use(std::inplace_vector<int, 64ul> const&)"
        add     rsp, 280
        ret
"temp()":
        sub     rsp, 280
        mov     edx, 260
        xor     esi, esi
        mov     rdi, rsp
        call    "memset"
        mov     rax, QWORD PTR "C.2.0"[rip]
        mov     rdi, rsp
        mov     DWORD PTR [rsp+256], 3
        mov     QWORD PTR [rsp], rax
        mov     eax, DWORD PTR "C.2.0"[rip+8]
        mov     DWORD PTR [rsp+8], eax
        call    "use(std::inplace_vector<int, 64ul> const&)"
        add     rsp, 280
        ret
```
:::

## Temporaries and Subobjects

Consider the difference between:

::: std
```cpp
struct Msg {
    int id;
    char payload[256]; // filled in later, by design
    explicit Msg(int i): id(i) { }
};

auto send(Msg const&) -> void;

auto temporary() -> void {
    send(Msg(42));
}

auto named() -> void {
    [[indeterminate]] auto m = Msg(42);
    send(m);
}
```
:::

There's nowhere in `temporary` to attach `[[indeterminate]]`, so if I want to avoid the `memset` before writing `id`, I have to implement this function as in `named()` (which compiles the same in C++23 and C++26) instead of `temporary()`.

But now consider I put my message in an envelope:

::: std
```cpp
struct Envelope {
    uint64_t seq = 0;
    Msg msg{7};
};

auto subobject() -> void {
    Envelope e;
    send(e.msg);
}

auto subobject_all() -> void {
    [[indeterminate]] Envelope e;
    send(e.msg);
}
```
:::

Indeterminacy is all-or-nothing. C++26 gives me an extra `memset` which I can remove to get back the C++23 code, but it also drops the guarantee for `e.seq` and for any later member. There's no way to say that _just_ `Msg::payload` is indeterminate.

## User Experience

While attempting to upgrade a large library to C++26, the release build had around 50,500 sites which were now being zero-initialized that were not before (note that this is already excluding those cases where an object is zero-initialized but the compiler optimizes out that extra initialization). Of those, 88% had class type yet only involve a mere 704 distinct class types. It's a lot of concentration. Over 25,000 of those extra zero-initializations are just between `std::string` and types in `{fmt}` (`value` and `iterator_buffer` being the most common)!

74% of those class type sites are partly uninitialized by design, whether that's a union, a member left for later, an inline buffer, etc. And many of those are temporaries.

A further 14% though are fully initialized, but by code that gcc could not see — because the initialization happens in an out-of-line constructor or function. For instance:

::: std
```cpp
class Container {
    char buffer[1'024];

public:
    Container(); // actually does zero buffer
                 // but you don't know that
};
```
:::

Uses of such types just get double-initialized.

There has to be a better way to handle this. Until then, I think we have to build our code with `-ftrivial-auto-var-init=uninitialized`{.default}.

# Proposal

Currently, `[[indeterminate]]` can only be applied to local variables and function parameters. This paper proposes that it also be able to be applied on non-static data members, with the obvious meaning. That allows types like `fmt::basic_memory_buffer` (and `fmt::value` and `std::string` and `std::optional` and ...) to annotate their uninitialized members, so that only one place needs to be annotated by only one person: the class author. Nobody has to pay cost for initializing memory that need not be initialized.

This one change alone covers most of our extra-initialized objects. It also covers temporaries, which cannot be annotated today either. It seems like a straightforward omission.

What it does not help are those cases that are initialized, but out-of-line, and become double-initialization. I'm not sure what to do about those.

## Wording

Change [basic.indet]{.sref}:

::: {.std .wording}
[1]{.pnum} When storage for an object with automatic or dynamic storage duration is obtained, the bytes comprising the storage for the object have the following initial value:

* [1.1]{.pnum} If the object has dynamic storage duration, or is the object associated with a variable or function parameter whose first declaration is marked with the `[[indeterminate]]` attribute ([dcl.attr.indet]), the bytes have indeterminate values;
* [1.*]{.pnum} [otherwise, each byte that is within the storage occupied, or that would be occupied, by a subobject of the object corresponding to a non-static data member declared with the `[[indeterminate]]` attribute has an indeterminate validate;]{.addu}
* [1.2]{.pnum} otherwise, the bytes have *erroneous values*, where each value is determined by the implementation independently of the state of the program.

If no initialization is performed for an object (including subobjects), such a byte retains its initial value until that value is replaced ([dcl.init.general], [expr.assign]). If any bit in the value representation has an indeterminate value, the object has an indeterminate value; otherwise, if any bit in the value representation has an erroneous value, the object has an erroneous value.
:::

Change [dcl.attr.indet]{.sref}:

::: {.std .wording}
[1]{.pnum} The `$attribute-token$` `indeterminate` may be applied to the definition of a block variable with automatic storage duration [or]{.rm} [,]{.addu} to a `$parameter-declaration$` of a function declaration[, or to the declaration of a non-static data member]{.addu}.
No `$attribute-argument-clause$` shall be present.
The attribute specifies that [the storage of an object with automatic storage duration is]{.rm} [certain bytes of storage with automatic storage duration are]{.addu} initially indeterminate rather than erroneous ([basic.indet]).

[#]{.pnum} If a function parameter is declared with the `indeterminate` attribute, it shall be so declared in the first declaration of its function. If a function parameter is declared with the `indeterminate` attribute in the first declaration of its function in one translation unit and the same function is declared without the `indeterminate` attribute on the same parameter in its first declaration in another translation unit, the program is ill-formed, no diagnostic required ([ifndr:dcl.attr.indet.mismatched.declarations]).

::: note
Reading from an uninitialized variable that is marked `[[indeterminate]]` can cause undefined behavior.

```diff
  void f(int);
  void g() {
    int x [[indeterminate]], y;
    f(y);                 // erroneous behavior ([basic.indet])
    f(x);                 // undefined behavior
  }

  struct T {
    T() {}
    int x;
  };
  int h(T t [[indeterminate]]) {
    f(t.x);               // undefined behavior when called below
    return 0;
  }
  int _ = h(T());

+ struct S {
+   S() {}
+   int a;
+   int buf [[indeterminate]] [16];
+ };
+ void k() {
+   S s;
+   f(s.a);        // erroneous behavior
+   f(s.buf[0]);   // undefined behavior
+   f(S().buf[0]); // undefined behvaior
+ }
```
:::
:::

Bump the feature-test macro for the attribute in [cpp.cond]{.sref}:

::: {.std .wording}
```diff
- indeterminate 202403L
+ indeterminate 2026XXL
```
:::
