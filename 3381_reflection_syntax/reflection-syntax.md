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
tag: reflection
---

# Introduction

[@P1240R2] originally proposed `^` as the reflection operator, which was also what [@P2996R5] proposed and what both existing implementations (EDG and Clang) used. We've grown to really like the syntax: it's terse and stands out visually.

Unfortunately, it turns out that `^` is not a viable choice for reflection for C++26, due to the collision with the block extension used by Clang and commonly used in Objective-C++.

It was pointed out that the syntax:

::: std
```cpp
$type-id$(^ident)();
```
:::

is ambiguous. This can be parsed as both:

* a variable named `ident` holding a block returning `$type-id$` and taking no arguments, and
* a cast of `std::meta::info` (a reflection of `ident`) into a `$type-id$` and then a call of `operator()`.

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
<tr><td>`?e`</td><td>While `?` already exists in the language today, we do not believe that using `?e` would be ambiguous with the conditional operator. We could even come up with a story for why the question mark — we are asking what an entity is, and getting a reflection back to answer the question. It's just not our favorite, but it is potentially viable.

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

Pattern Matching ([@P2688R1]) additionally proposes using `? $pattern$` as the optional pattern, which would conflict with the use of `?` as the reflection operator. So while it's technically available for use today, The optional pattern strikes us as a better use of unary `?` than reflection.
</td><td>🤷</td></tr>
<tr><td>`@e`</td><td>`@` is already used as an extension in Objective-C.

For many potential identifiers, there is already concrete meaning (`@property`, `@dynamic`, `@optional`, etc.). There is no way to escape this either, since `@(e)` is a boxed expression, and `@[e]` and `@{e}` are container literals.
</td><td>❌</td>
</tr>
<tr><td>`\e`</td><td>Similar to `/e`, this is viable. But we prefer it as an interpolator (for which there is prior art), so we'd rather not use it here.

Additionally, this runs into issues with UCNs: `\u0` is parsed as a UCN, not a reflection of `u0`.</td><td>❌</td></tr>
<tr><td>``e`</td><td>The third character recently added to the basic character set (after `$` and `@`) is the backtick (or GRAVE ACCENT). The backtick has the advantage that it's pretty small, even smaller than `^`.

But it has the disadvantage that backtick is used by Markdown everywhere inline code blocks, and not all Markdown implementations properly give you mechanisms to escape it. While not necessarily a show-stopper, we also just don't think it's good enough to reasonably pursue.</td><td>❌</td></tr>
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
* `|e`
* `?e` is viable today, but would compete with pattern matching
* `%e` if we're open to people struggling with digraphs


# Multiple Characters

Once we extend our search space to multiple characters, there are an infinite amount of possibilities to explorer — it's easy enough to come up with some sequence of tokens that is not a digraph and isn't ambiguous to parse.

For help with this investigation, Wyatt Childers put together a [simple utility](https://syntax-tester.haxing.ninja/?op=%5E%5B&sop=%5D) to test various alternatives for syntax. That utility allows you choose a prefix and/or suffix tokens and see what that looks like using various expected usage patterns.

Some choices that we have considered:

* `^^e` — While a single caret isn't viable, two carets would have no ambiguity with blocks. It's twice as long as the status quo, but requiring an additional character wouldn't be the end of the world. Two characters is short enough.
* `^[e]` — A different way to work around the block ambiguity is to throw more characters at it, in this case surrounding the operand with square brackets. On the one hand, this is more symmetric with splicing. On the other hand, it's a heavier syntax.
* `${e}`{.op} or `$(e)`{.op} — One way to work around the identifier issue with `$`{.op} presented in the previous section is to use additional tokens that cannot be in identifiers. This would work fine for reflection, but doesn't have as nice a mirror for token sequences — would those use an extra set of braces? Additionally `$`{.op} seems more associated with interpolation than reflection in many languages, so simply seems like the opposite choice here. In contrast, `$$(e)`{.op} might be an interesting choice for a splice operator — and could arguably have some symmetry with `^^e` with reflection (both having a double character).
* `/\e`{.op} — If we can't have a small caret, can we have a big caret? This actually has the same issue as just `\e` because of the UCN issue.

# Proposal

As a group, our preference is `^^e` (where `^^` is a new, single token).

It doesn't have any of the issues of the single-character solutions. Having the reflection operator be two characters as compared to only one isn't really a sufficiently large cost that we feel the need to reconsider it in favor of `|e`, `/e`, or `%e`. A two-character reflection operator is still overwhelmingly shorter than the 10-character operator that we started from (`reflexpr(e)`, if you count the parentheses), and that's good enough for us.

This has already been implemented in both EDG and Clang. Pending Evolution approval, we will simply update [@P2996R5] to use the new syntax throughout — mostly just a search and replace, but with the extra addition in the wording of `^^` to the grammar of `$operator-or-punctuator$`.

# Why Not A Keyword?

The question that always comes up is: why some kind of punctuation mark (`^^` as proposed) instead of a keyword as originally proposed?

::: cmptable
### [@P1240R0]
```cpp
constexpr auto r = reflexpr(S::m);
auto m = s.unreflexpr(r);
```

### Proposed
```cpp
constexpr auto r = ^^S::m;
auto m = s.[:r:];
```
:::

Whether that keyword is `reflexpr` or `reflectof` or `reflof` or `metaof`, we feel that a keyword would be the wrong choice for a reflection (or, especially, a splice) operator and are strongly opposed to that direction.

The primary reason for this is how heavy any keyword solution is compared to a punctuation solution, and how much that distracts from the intent of the code being presented.

We can start with a simple example of a typelist:

::: std
```cpp
mp_list<signed char, short, int, long, long long>
```
:::

Which corresponds to this as proposed, which still makes the types themselves stand out:

::: std
```cpp
{^^signed char, ^^short, ^^int,  ^^long, ^^long long}
```
:::

But is much uglier and filled with tons of syntactic noise that doesn't contribute at all to readability if using a keyword:

::: std
```cpp
{metaof(signed char), metaof(short), metaof(int), metaof(long), metaof(long long)}
```
:::

The impulse to emphasize the reflection operation by making it stand out is, while understandable, fundamentally misguided. The code is never about taking a reflection; it's always about passing an entity to a function that then operates on it. The fact that we need to prefix the entity with `^^` is the price we're paying because entities aren't ordinary values so we need to apply an operator to turn them into ones. Not something to proudly write home about.

Or, in other words, we cannot [^sucks] write the expression `f(int, float)` so we have to write `f(^^int, ^^float)`, which is the next best thing. Adding a ton of `metaof` is not a feature.

[^sucks]: Consider the expression `f(X(int))`, where `X` is a type. This could be calling `f` with the function type `X(int)` or with a value of `X` constructed with `int`. How do you differentiate?

We can see this more clearly if we compare some language operators with their reflection equivalents. We have several operators in the language that take a type and produce a value of specific type — which makes them initially seem like precedent for how the reflection operator should behave. But:

|Operation|Language Operator|Reflection|
|-|-|-|
|size|`sizeof(T)`|`size_of(^^T)`|
|alignment|`alignof(T)`|`align_of(^^T)`|
|is noexcept?|`noexcept(E)`|`is_noexcept(^^(E))`|

In all cases, the operation being performed is to get the size of the type (`sizeof` or `size_of`), the alignment of the type (`alignof` or `align_of`), or to check whether the expression is noexcept (`noexcept` or `is_noexcept`, noting that we don't have expression reflection yet — but when we do it'll look like this) and the operand is the type (`T`) or expression (`E`). Getting the reflection of the type/expression is *not* an important operation here. Making it stand out more does not have value. It would hide the actual operation.

The same is true for all operations that we might want to perform. Consider wanting to iterate over the numerators of `Color`:

::: std
```cpp
// reads as the enumerators of Color
enumerators_of(^^Color)

// reads as the enumerators of the reflection of Color
enumerators_of(reflectof(Color))
```
:::

Or to substituting `std::map` with `std::string` and `int`:

::: std
```cpp
// reads as substitute map with string and int
substitute(^^std::map, {^^std::string, ^^int})

// reads as substitute the reflection of map of with
// the reflection of string and the reflection of int
substitute(reflectof(std::map), {reflectof(std::string), reflectof(int)}))
```
:::


The former reading more clearly expresses the intent. Keywords demand to be read, whereas sigils may be internally skipped over. Eliding the sigil from the internal dialogue lets the user put aside the fact that reflection is happening. They may read it as "enumerators of `Color`" or "substitute `map` with `string` and `int`." Once the keyword-name enters the "internal token stream," the user cannot hope to understand the meaning of the expression without learning the meaning of `reflectof` (or `metaof` or `reflof` or ...). That is exactly the opposite of novice-friendly.

A parallel can be made with templates. In `vector<int>` the template machinery is often "invisible". the `<>` could be replaced by a keyword here as well. For instance, we could've had a keyword `substitute` and made forming a template something like `substitute(vector, int)` instead of `vector<int>`. However, we think most people would agree the punctuator puts the focus on the operation itself not the grammatical limitations of the language. Lots of novices engage in using templates long before writing templates, and while we've seen arguments in favor of a _different_ punctuator for template arguments (such as `vector(int)` or D's `vector!(int)`, which avoid the `<` ambiguity), we've never seen an argument for a keyword here.

The other thing to point out is that reflection is not necessarily going to be a prominently user-facing facility. Certainly not a novice-facing one. Reflection opens the door to a large variety of libraries that can make otherwise very complex operations very easy — from even expert-unfriendly to novice-friendly. But the user-facing API of those libraries need not actually expose the reflection operator at all, and most use-cases would not do so at all. Arguing for a reflection keyword to be novice-friendly thus doubly misses the point — not only is it not novice friendly, but novices may rarely even have to look at such code.