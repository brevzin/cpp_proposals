---
title: "`if consteval`"
document: P1938R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Richard Smith
      email: <richard@metafoo.co.uk>
    - name: Andrew Sutton
      email: <asutton@lock3software.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>
toc: true
---

# Introduction

Despite this paper missing both our respective NB comment deadlines and the mailing
deadline, we still believe this paper provides a significant enough improvement
to the status quo that it should be considered.

C++20 will have several new features to aid programmers in writing code during
constant evaluation. Two of these are `std::is_constant_evaluated()` [@P0595R2]
and `consteval` [@P1073R3], both adopted in San Diego 2018. `consteval` is for
functions that can only be invoked during constant evaluation.
`is_constant_evaluated()` is a magic library function to check if the current
evaluation is constant evaluation to provide, for instance, a valid implementation
of an algorithm for constant evaluation time and a better implementation for runtime.

However, despite being adopted together, these features interact poorly with
each other and have other issues that make them ripe for confusion.

# Problems with Status Quo

There are two problems this paper wishes to address.

The first is specific to `is_constant_evaluated`. Once you learn what this
magic function is for, the obvious usage of it is:

```cpp
size_t strlen(char const* s) {
    if constexpr (std::is_constant_evaluated()) {
        for (const char *p = s; ; ++p) {
            if (*p == '\0') {
                return static_cast<std::size_t>(p - s);
            }
        }    
    } else {
        __asm__("SSE 4.2 insanity");        
    }
}
```

This example is borrowed from [@P1045R0], except it has a bug: it uses `if constexpr`
to check the conditional `is_constant_evaluated()` rather than a simple `if`.
You have to really deeply understand a lot about how constant evaluation works
in C++ to understand that this is in fact not only _not_ "obviously correct" but
is in fact "obviously incorrect," for some definition of obvious. This is such
a likely source of error that Barry submitted bugs to both [gcc](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=91428)
and [clang](https://bugs.llvm.org/show_bug.cgi?id=42977) to encourage the
compilers to warn on such improper usage. gcc 10.1 will provide a warning
for the [simple case](https://godbolt.org/z/LiiZoW):

```
<source>: In function 'constexpr int f(int)':
<source>:4:45: warning: 'std::is_constant_evaluated' always evaluates to true in 'if constexpr' [-Wtautological-compare]
    4 |     if constexpr (std::is_constant_evaluated()) {
      |                   ~~~~~~~~~~~~~~~~~~~~~~~~~~^~
```

But then people have to understand why this is a warning, and what this even
means. Nevertheless, a compiler warning is substantially better than silently
wrong code, but it is problematic to have an API in which many users are drawn
to a usage that is tautologically incorrect.

A second problem is the interplay between this magic library function and the
new `consteval`. Consider the example:

```cpp
consteval int f(int i) { return i; }

constexpr int g(int i) {
    if (std::is_constant_evaluated()) {
        return f(i) + 1; // <==
    } else {
        return 42;
    }
}

consteval int h(int i) {
    return f(i) + 1;
}
```

The function `h` here is basically a lifted, constant-evaluation-only version
of the function `g`. At constant evaluation time, they do the same thing,
except that during runtime, you cannot call `h`, and `g` has this extra path.
Maybe this code started with just `h` and someone decided a runtime version
would also be useful and turned it into `g`. 

Unfortunately, `h` is well-formed while `g` is ill-formed. You cannot make that
call to `f` (that is ominously marked with an arrow) in that location. Even
though that call will *only* happen during constant evaluation, that's
still not enough.

With specific terms, the call to `f()` inside of `g()` is an
_immediate invocation_ and needs to be a constant expression and it is not.
Whereas the call to `f()` inside of `h()` is *not* considered an _immediate invocation_
because it is in an _immediate function context_ (i.e. it's invoked from another immediate
function), so it has a weaker set of restrictions that it needs to follow.

In other words, this kind of construction of conditionally invoking a
`consteval` function from a `constexpr` function just Does Not Work (modulo
the really trivial cases - one could call `f(42)` for instance, just never
`f(i)`).

We find this lack of composability of features to be problematic and think it
can be improved.

# Proposal

We propose a new form of `if` statement which is spelled:

```cpp
if consteval { }
```

The braces are mandatory and there is no condition. If evaluation of this
statement occurs during constant evaluation, the first substatement is executed.
Otherwise, the second substatement (if there is one) is executed. 

This behaves exactly as today's:

```cpp
if (std::is_constant_evaluated()) { }
```

except with three differences:

1. No header include is necessary.
2. The syntax is different, which completely sidesteps the confusion over the
proper way to check if we're in constant evaluation. You simply cannot misuse
the syntax. 
3. We can use `if consteval` to allow invoking immediate functions.

To explain the last point a bit more, the current language rules allow you to invoke
a `consteval` function from inside of another `consteval` function
([\[expr.const\]/12](http://eel.is/c++draft/expr.const#12)) - we can do this by
construction:

::: bq
An expression or conversion is in an _immediate function context_ if it is
potentially evaluated and its innermost non-block scope is a function parameter
scope of an immediate function. An expression or conversion is an
_immediate invocation_ if it is an explicit or implicit invocation of an
immediate function and is not in an immediate function context.
An immediate invocation shall be a constant expression.
:::


By extending the term _immediate function context_ to also include
an `if consteval` block, we can allow the second example to work:

```cpp
consteval int f(int i) { return i; }

constexpr int g(int i) {
    if consteval {
        return f(i) + 1; // ok: immediate function context
    } else {
        return 42;
    }
}

consteval int h(int i) {
    return f(i) + 1; // ok: immediate function context
}
```

Additionally, such a feature would allow for an easy implementation of the
original `std::is_constant_evaluated()`:

```cpp
constexpr bool is_constant_evaluated() {
    if consteval {
        return true;
    } else {
        return false;
    }
}
```

Which in itself suggests that this is the more fundamental feature. As
such, `std::is_constant_evaluated()` may itself no longer be necessary. However,
given the very late date of this proposal, we would more than happily keep it
if it allows us this new language feature.

# History

The initial revision of the `std::is_constant_evaluated()` proposal [@P0595R0]
was actually targeted as a language feature rather than a library feature. The
original spelling was `if (constexpr())`. The paper was presented in Kona 2017
and was received very favorably in the form it was presented (17-4). The poll
to consider a magic library alternative was only marginally more preferred (17-3). 
We believe that in the two years since these polls were taken, having a dedicated
language feature with an impossible-to-misuse API, that can coexist with the rest
of the constant ecosystem, is the right direction.

# Acknowledgments

Thank you to David Stone and Tim Song for working through these examples.
