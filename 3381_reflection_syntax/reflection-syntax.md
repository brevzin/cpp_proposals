---
title: "Syntax for Reflection"
document: P3381R0
date: today
audience: EWG
author:
    - name: Wyatt Childers
      email: <wcc@edg.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Andrew Sutton
      email: <andrew.n.sutton@gmail.com>
    - name: Faisal Vali
      email: <faisalv@gmail.com>
    - name: Daveed Vandevoorde
      email: <daveed@edg.com>

toc: true
toc-depth: 4
tag: constexpr
---

# Introduction

[@P1240R2] originally proposed `^` as the reflection operator, which was also what [@P2996R5] proposed and what both existing implementations (EDG and Clang) used. We've grown to really like the syntax: it's terse and stands out visually.

Unfortunately, it turns out that `^` is not a viable choice for reflection for C++26, due to the collision with the block extension used by Clang and commonly used in Objective-C++.

It was pointed out that the syntax:

::: std
```cpp
type-id(^ident)();
```
:::

is ambiguous. This can be parsed as both:

* a variable named `ident` holding a block returning `type-id` and taking no arguments, and
* a cast of `std::meta::info` (a reflection of `ident`) into a `type-id` and then a call of `operator()`.

This gets worse with the [@P3294R1] usage of `^{ ... }` as a token sequence. This sequence is completely ambiguous:

::: std
```cpp
auto A = ^{ f(); };
```
:::

As such, the goal of this paper is to come up with new syntax for the reflection operator.

There are three approaches that we can go with this:

* a keyword,
* [a single character](#single-character), or
* [multiple characters](#multiple-characters).

The original reflection design did use a keyword — `reflexpr(e)`. But that is far too verbose, which is why the design had changed to `^` to begin with. We would strongly prefer not to go back down that road.

That leaves either of the other choices.

# Single Character

There are not too many single character options available to us, presuming we want to stick with the characters that are easy to type and not just start perusing the available Unicode characters (`↑` is available after all). It also makes life much easier if we do not need to add new basic source characters (which a character like `↑` would require).

Ignoring those characters that are already unary operators in C++ and the quotation marks, these are all the options available to us and what we think of them:

<table>
<tr><th>Token</th><th>Notes</th><th>Disposition</th></tr>
<tr><td>`#e`{.op}</td><td>The problem with `#e`{.op} is that precludes use in existing C macros. This code already has meaning:

::: std
```cpp
#define MACRO(x) do_something_with(#x)
```
:::

And that meaning is not taking the reflection of `x` and cannot change.
</td><td>❌</td></tr>
<tr><td>`$e`{.op}</td><td>Syntactically `$e`{.op} is pretty nice and doesn't conflict with anything else in C++ since we only just added it to the basic character set.

Unfortunately, compilers support the use of `$`{.op} in identifiers as an extension, so the simplest usage of `$T`{.op} as the reflection of the type `T` is already ambiguous with the use an identifier that happens to start with a dollar sign.
</td><td>❌</td></tr>
<tr><td>`%e`</td><td>
We were initially fairly excited about the use of `%e`. Like `^e`, this token is already used as a binary operator — but not an especially common one, so it seemed potentially viable as a reflection operator.

Unfortunately, one way to use a reflection would be to pass it directly as the first template argument:

::: std
```cpp
C<%T> c;
```
:::

And it turns out that `<%` is a digraph, for `{`. This doesn't make it a complete non-starter, since this _is_ something that can be worked around with use of parens or a space. But it's not great!
</td><td>😞</td></tr>
<tr><td>`,e`</td><td>No.</td><td>❌</td></tr>
<tr><td>`/e`</td><td>The forward slash is the first character we come to that seems like a viable option. Here is some usage of it:

::: std
```cpp
constexpr auto r = /int;

template for (constexpr auto e : enumerators_of(/E)) ;

fn</R>();

[: /int :]
[:/int:]
```
:::

Use of `/` also offers nice symmetry with the use of `\` for interpolation in code injection (or perhaps arguably the wrong kind of symmetry, since the splice syntax we're not proposing to change).

The downside of `/` is that it's pretty close to opening a comment. Now, we don't intend on `//e` to be valid syntax — reflecting an expression would require parentheses, so it would have to be `/(/e)`. Ditto `/*e` is not valid, would have to be `/(*e)`. Are those too close to comments?
</td><td>✅</td></tr>
<tr><td>`:e`</td><td>

Similar to the problems we ran into with `C<%T>` not working because `<%` is a digraph for `{`, `C<:T>` would also not work because `<:` is a digraph for `[`. If we're going to pick a token that has digraph problems, `%` is definitely the better option.

</td><td>❌</td>
</tr>
<tr><td>`=e`</td><td>While `^` and `%` already exist as binary operators, they are fairly rare. `=` just seems way too common to be viable to overload to mean reflection</td><td>❌</td>
<tr><td>`?e`</td><td>Actually this one doesn't seem to have any particular problems associated with it. It's not ambiguous with the conditional operator. We could even come up with a story for why the question mark — we are asking what an entity is, and getting a reflection back to answer the question. It's just not our favorite, but it is viable.

::: std
```cpp
constexpr auto r = ?int;

template for (constexpr auto e : enumerators_of(?E)) ;

fn<?R>();

[: ?int :]
[:?int:]
```
:::

The downside of `?` is that this token tends to be strongly associated with predicates across programming languages. In C++ we just have the conditional operator, but in other languages we have null coalescing, optional chaining, and error propagation all spelled with `?` - plus for some languages it's convention to spell predicates as `v.empty?`, all of which has nothing to do with reflection.
</td><td>✅</td></tr>
<tr><td>`@e`</td><td>`@` is already used as an extension in Objective-C.

For many potential identifiers, there is already concrete meaning (`@property`, `@dynamic`, `@optional`, etc.). There is no way to escape this either, since `@(e)` is a boxed experssion, and `@[e]` and `@{e}` are container literals.
</td><td>❌</td>
</tr>
<tr><td>`\e`</td><td>Similar to `/e`, this is viable. But we prefer it as an interpolator (for which there is prior art), so we'd rather not use it here.

Additionally, this runs into issues with UCNs: `\u0` is parsed as a UCN, not a reflection of `u0`.</td><td>❌</td></tr>
<tr><td>``e`</td><td>The third character recently added to the basic character set (after `$` and `@`) is the backtick (or GRAVE ACCENT). The backtick has the advantage that it's pretty small, even smaller than `^`.

But it has the disadvantage that backtick is used by Markdown everywhere inline code blocks, and not all Markdown implementations properly give you mechanisms to escape it. While not necessarly a show-stopper, we also just don't think it's good enough to reasonably pursue.</td><td>❌</td></tr>
<tr><td>`|e`</td><td>The last available single token, this one is also viable, unambiguous, and not a part of a digraph:

::: std
```cpp
constexpr auto r = |int;

template for (constexpr auto e : enumerators_of(|E)) ;

fn<|R>();

[: |int :]
[:|int:]
```
:::

One potential issue with `|` is when using multiple reflections in a single line, for instance:

::: std
```cpp
if (|T == |U)
```
:::

While not ambiguous to the compiler, those with math backgrounds might want to see `|T == |` as a magnitude of some sort.
</td><td>✅</td></tr>
</table>

To summarize, there are only a few single tokens that we feel are completely viable:

* `/e`
* `?e`
* `|e`

If we're open to people struggling with digraphs, also `%e`.

# Multiple Characters

Once we extend our search space to multiple characters, there are an infinite amount of possibilities to explorer — it's easy enough to come up with some sequence of tokens that is not a digraph and isn't ambiguous to parse.

For help with this investigation, Wyatt Childers put together a [simple utility](https://syntax-tester.haxing.ninja/?op=%5E%5B&sop=%5D) to test various alternatives for syntax. That utility allows you choose a prefix and/or suffix tokens and see what that looks like using various expected usage patterns.

Some choices that we have considered:

* `^^e` — While a single caret isn't viable, two carets would have no ambiguity with blocks. It's twice as long as the status quo, but requiring an additional character wouldn't be the end of the world. Two characters is short enough.
* `^[e]` — A different way to work around the block ambiguity is to throw more characters at it, in this case surrounding the operand with square brackets. On the one hand, this is more symmetric with splicing. On the other hand, it's a heavier syntax.
* `${e}`{.op} — One way to work around the identifier issue with `$`{.op} presented in the previous section is to use additional tokens that cannot be in identifiers. This would work fine for reflection, but doesn't have as nice a mirror for token sequences — would those use an extra set of braces?
* `/\e`{.op} — If we can't have a small caret, can we have a big caret? This actually has the same issue as just `\e` because of the UCN issue.

# Proposal

As a group, our preference is `^^e` (where `^^` is a new, single token).

It doesn't have any of the issues of the single-character solutions. Having the reflection operator be two characters as compared to only one isn't really a sufficiently large cost that we feel the need to reconsider it in favor of `|e`, `/e`, or `%e`. A two-character reflection operator is still overwhelmingly shorter than the 10-character operator that we started from (`reflexpr(e)`, if you count the parentheses), and that's good enough for us.

This has already been implemented in both EDG and Clang. Pending Evolution approval, we will simply update [@P2996R5] to use the new syntax throughout — mostly just a search and replace, but with the extra addition in the wording of `^^` to the grammar of `$operator-or-punctuator$`.

