---
title: "`do not return or else`"
document: D2121R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

This is somewhat of a novel proposal in that it is not motivated by any problem
which currently exists. Instead, it is motivated by the problems that arising
out of the Pattern Matching [@P1371R2] proposal and its excursion into more
complex expressions.

The expression form of `inspect` needs its return type to be compatible with
all of the cases. But not every case actually contributes to the type:

```cpp
int f(int arg) {
  return inspect (i) {
    0: 42;
    _: throw bad_argument();
  };
}
```

Here, we have two cases: one has type `int`, and the other technically has type
`void`. But because the `throw` actually escapes the scope anyway, we don't need
to consider that case when resolving the type. As a result, the above can be a
perfectly valid use of `inspect` - the type of the expression can be said to
be `int`.

This, in of itself, isn't novel. We already carve out an exception for `throw`
in the conditional operator, and the following rewrite of the above example
has been valid for a long time:

```cpp
int g(int arg) {
    return (arg == 0) ? 42 : throw bad_argument();
}
```

But while `throw` is the only exception (not sorry) for the conditional operator,
there are other statements in C++ which escape their scope and thus could
potentially be excluded from consideration when it comes to types, and could
be used as scope-escaping expressions. The full set of such keyword-driven
statements is:

- `break`
- `continue`
- `co_return`
- `goto`
- `return`
- `throw`

`goto` is a little special, but the others behave a lot like `throw`: they 
necessarily escape scope and have no possible value, so they can't meaningfully
affect the type of an expression. The following could be a perfectly reasonable
function (though this paper is not proposing it):

```cpp
int h(int arg) {
    int r = (arg == 0) ? 42 : return 17;
    std::cout << r;
    return r;
}
```

But in addition to those keywords, there's one more thing in C++ that is
guaranteed to escape scope: invoking a function marked `[[noreturn]]` (such
as `std::abort()` or `std::terminate()`). And here, we run into a problem. While
it's straightforward to extend the rules to treat all escaping statements as
valueless, we cannot do the same for `[[noreturn]]` functions:

```cpp
int maybe_terminate(int arg) {
  return inspect (i) {
    0: 42;
    _: std::terminate();
  };
}
```

The guidance we adopted in Albuquerque [@Attributes] during the discussion of
[@P0840R0] was:

::: quote
Compiling a valid program with all instances of a particular attribute ignored
must result in a correct implementation of the original program.
:::

The above `inspect`, if we added semantic meaning to
`[[noreturn]]`, could work - it would ignore the second pattern and simply
deduce the type of the `inspect`-expression as `int`. But if we ignored `[[noreturn]]`,
then we have two patterns of differing type (`int` and `void`) and the type
of the `inspect`-expression would have to be either `void` or ill-formed, either
way definitely not `int`. This violates the attribute rule.

However, being able to `std::terminate()` or `std::abort()` or `std::unreachable()`
in a particular case is an important feature in an `inspect`-expression, and so
the problem of how to make it work must be resolved... somehow.

# Presentation of Alternatives

This paper goes through four mechanisms for how we could get this behavior to
work. It will first introduce the four mechanisms, and then compare
and contrast them.

## Annotate escaping blocks

At a session in Prague, the paper authors proposed new syntax for introducing
a block which marked the block as `noreturn`:

```cpp
int maybe_terminate(int arg) {
  return inspect (i) {
    0: 42;
    _: !{ std::terminate(); }
  };
}
```

The above could then be allowed: the second case would be considered an escaping
statement by virtue of the `!{ ... }` and would thus not participate in determining
the type. This leaves a single case having type `int`. 

Such an annotation could be enforced by the language to not escape (e.g. by
inserting a call to `std::terminate` on exiting the scope), so there is no UB
concern here or anything.

There may be other mechanisms to annotate escaping blocks besides `!{ ... }` but
this paper considers any others to be just differences in spelling anyway, so 
only this one is considered (and the spelling isn't really important anyway).

## Annotate escaping functions

The other three suggested mechanisms all are based on applying some kind of
annotation to the _functions_ that escape rather than to the blocks that
invoke these functions. These are:

### A new type indicating escaping

C and C++ have the type `void`, but despite the name, it's not an entirely
uninhabited type. You can't have an object of type `void` (yet?), but functions
which return `void` do, in fact, return. We could introduce a new type that
actually has zero possible values, which would indicate that a function can
never return. For the sake of discussion, let's call this type `true void`. 
Actually, that's a bit much. Let's call it `std::never`.

We could then change the declaration of functions like `std::abort()`:

```diff
- [[noreturn]] void abort() noexcept;
+ never abort() noexcept;
```

The advantage here is that once the noreturn functions return `std::never`, the
language can understand that a block ending with one of these functions can never
return, so we don't need the `!{ ... }` syntax. The motivating example just
works:

```cpp
int maybe_terminate(int arg) {
  return inspect (i) {
    0: 42;
    _: std::terminate(); // ok, returns std::never, so never returns
  };
}
```

### A keyword indicating escaping

We could take the `[[noreturn]]` function attribute and elevate it into
a first class language feature, so that future language evolution (e.g. pattern
matching) may then take this into account in determining semantics.

The syntax this paper proposes is:

```cpp
namespace std {
  do not return or else void abort();
  do not return or else void terminate();
}
```

Just kidding. The syntax this paper is actually proposing (for reasons that will become clearer shortly) is the keyword spelled `@[_Noreturn]{.kw}@` (this is already a keyword in C so seems straightforwardly available):

```cpp
namespace std {
  @[_Noreturn]{.kw}@ void abort();
  @[_Noreturn]{.kw}@ void terminate();
}
```

This means that libraries straddling multiple language versions may end up having
to write:
```cpp
NORETURN_SPECIFIER void abort();
```
but because there are a fairly small number of such functions, I don't think
it's a huge problem.

### Adding semantics to the `[[noreturn]]` attribute

Instead of introducing a new language feature to mark a block as escaping (as
proposed in Prague), or introducing a new language feature to mark a function
as escaping (as in the previous two sections), let's actually just take advantage
of the fact that we already have a language feature to mark a function as
escaping: the `[[noreturn]]` attribute.

That is: don't introduce anything new at all. Just allow the `[[noreturn]]`
attribute to have semantic meaning. Say that a function so annotated counts
as an escaping function, as a language rule, and allow that to work.

# Comparison of Alternatives

The set of `[[noreturn]]`-annotated functions is
very small (the standard library has 9: `std::abort`, `std::exit`, `std::_Exit`, 
`std::quick_exit`, `std::terminate`, `std::rethrow_exception`,
`std::throw_with_nested`, `std::nested_exception::rethrow_nested`, and
`std::longjmp` - with `std::unreachable` on the way),
and we already have to annotate these functions. It's hard to count how many
uses of this attribute exist in the wild since it so frequently shows up being
a macro, but I think it's safe to say that the number of invocations of
escaping functions far exceeds the number of declared escaping functions. By
many orders of magnitude.

Given that, and the fact that we need to make some kind of language change to
make this work anyway, it seems like we should change the language to recognize
the escaping functions themselves rather than recognize uses of them. The
suggested path of `!{ std::abort(); }` isn't exactly enormous syntactic overhead
over `std::abort()`, it's probably about as minimal an annotation as you can
really get, but it just seems like the wrong direction to take - and it seems
better for the annotation to be localized to the functions rather than the
invocations of them. We should instead elevate `noreturn` to be a first-class
language feature so that we can treat `std::terminate()` the same as a `return`
or a `throw` without requiring further annotation on all uses of it.

Let's go through the suggested options for annotating the function itself.

The problem with introducing a new type like `std::never` is the enormous amount
of work necessary to really work through what `std::never` means in the type
system. Can you have a...

- `never&`? Much like `void&`, there cannot be such an object, so forming
a valid reference is impossible. Would that mean that the type itself is
ill-formed or would that mean that a function returning a `never&` never returns?
- `never*`? Since there can never be a `never` object to point to, this seems
like the same case as `never&` - but there is one exception. A `never*` could
still have a null pointer value. That's not pointing to an object right? Does
this mean that a `never* f()` necessarily returns a null pointer? 
- `pair<never, int>`? As a proxy for having a class type with a
`never` - this would also be a type that's impossible to form. So this one,
like `never&`, would either also be an "escaping type" or ill-formed. 
- `optional<never>`? This is a lot like `never*` - it can never hold a value
but it's perfectly fine to empty? But how would you construct the
language rules such that it's implementable properly? If you could, then this
would be a conditionally escaping type? What would that mean? Another isomorphic
type would be `variant<T, never>`, which would necessarily hold a `T`.

All of these questions seem quite interesting to think about, but ultimately
the benefit doesn't seem to be there at all. It's nice to only have to annotate
the escaping functions - rather than all escaping uses of those functions - but
this direction just has too many other questions.

That reduces us to the last two choices:

1. Introduce `@[_Noreturn]{.kw}@` as a _function-specifier_, or
2. Allow the language to impart semantics on `[[noreturn]]`.

The advantage of the former is it allows us to preserve the adopted guidance
on the meaning of attributes from Albuquerque. 

The advantage of the latter is: we already have an existing solution to exactly
this problem, and having to introduce a new language feature, to solve exactly
the same problem, seems like artificial and pointless language churn. The issue
is that we are not allowing ourselves to use the existing solution to this
problem. Maybe we should?

Sure, such a direction would open the door to wanting to introduce other
attributes that may want to have normative semantic impact, and we'd lose the
ability to just reject all of those uniformly. But I think we should seriously
consider this direction. It would mean that we would not have to make any
changes to the standard library at all. Any user-defined `[[noreturn]]`
functions that already exist would just seamlessly work without them having to
make any changes.

Note that `[[no_unique_address]]`, the attribute during whose discussion we
adopted this guidance, already is somewhat fuzzy with this rule. The correctness
of a program may well depend annotated members taking no space (e.g. if a type
so annotated needs to be constructed in a fixed-length buffer). We more or less
say this doesn't count, and there is certainly no such fuzziness with the other
attributes like `[[likely]]`, `[[fallthrough]]`, or `[[deprecated]]`.

But there's one other important thing to consider...

# C Compatibility

An important thing to consider is C compatibility. C _also_ has functions that
do not return, and we should figure out how to treat those as escaping functions
as well. C has a _different_ function annotation to indicate an escaping
function, introduced in C11 by [@C.N1478]:

```cpp
_Noreturn void fatal(void); 

void fatal() { 
  /* ... */
  exit(1); 
}
```

Where the C functions `longjmp()`, `abort()`, `exit()`, `_Exit()`, and
`quick_exit()` so annotated. C also provides a header which `#define`s
`noreturn` to `_Noreturn`.

On top of this, WG14 is pursuing the C++ `[[noreturn]]` attribute itself,
via [@C.N2410].

This suggests that pursuing a different keyword (possibly a context-sensitive one)
for `noreturn`
would just introduce a _new_ incompatibility with C, that C is currently working
to remedy. Unless the keyword we picked was, specifically, `@[_Noreturn]{.kw}@`.

But the C compatibility issue is actually even stronger than this. While in C++,
we just have _guidance_ that attributes _should_ be ignorable, this is actually
normative in C. From the latest C working draft, 6.7.11.1p3 [@C.N2478]:

::: quote
A strictly conforming program using a standard attribute remains strictly conforming in the absence of that attribute.
:::

with corresponding footnote:

::: quote
Standard attributes specified by this document can be parsed but ignored by an implementation without changing thesemantics of a correct program; the same is not true for attributes not specified by this document.
:::

That's pretty clear. If C++ adopts semantics for `[[noreturn]]`, that kills any attempt at C compatibility going forward. 

# Proposal

Given WG21's guidance that attributes should be ignorable, and WG14's normative
rule of the same, it seems like the best course of action is to introduce a new,
keyword to indicate that a function will not return. 

For compatibility with C, which already has exactly this feature, we should just
adopt the C feature.

This would be a novel direction in C++, since we typically don't use these kinds
of names, but as mentioned before, the number of noreturn functions is small so
it seems far more important to get a consistent feature than it is to have that
feature have nice spelling.

We would then go through the library and swap out the `[[noreturn]]` attribute
for the `@[_Noreturn]{.kw}@` specifier:

```diff
- [[noreturn]] void terminate() noexcept;
+ _Noreturn void terminate() noexcept;
```

## Interaction with the type system

I want to be very clear that regardless of the direction taken for this paper,
given:

```cpp
@[_Noreturn]{.kw}@ void f();
void g();
```

the type of `f` is still `void()`, the same as the type of `g`. Though it turns
out that clang _already_ models `__attribute((noreturn))` (but not `[[noreturn]]`)
in the type system:

```cpp
template <typename T>
constexpr bool is_noreturn(T ()) { return false; }
template <typename T>
constexpr bool is_noreturn(__attribute__((noreturn)) T ()) { return true; }

int x();
__attribute__((noreturn)) float y();
[[noreturn]] double z();

static_assert(not is_noreturn(x));
static_assert(is_noreturn(y));
static_assert(not is_noreturn(z));
```

But, again, not something I'm interesting in.

## Proposed Wording

In [lex.key]{.sref}, add `@[_Noreturn]{.kw}@` as a keyword.

Change [expr.prim.lambda]{.sref}/3:

::: bq
[3]{.pnum} In the _decl-specifier-seq_ of the _lambda-declarator_, each _decl-specifier_ shall be one of `mutable`, `constexpr`, [or]{.rm} `consteval` [, or `_Noreturn`]{.addu}.
[_Note_: The trailing requires-clause is described in [dcl.decl].
— _end note_]
:::

Add somewhere in [expr.prim.lambda.closure]{.sref}:

::: addu
[*]{.pnum} If the _lambda-expression_'s _decl-specifier-seq_ contains `_Noreturn` and if the function call operator or any given operator template specification is called and eventually returns, the behavior is undefined.
::: 

In [dcl.fct.spec]{.sref}, change the grammar to add `@[_Noreturn]{.kw}@` as a _function-specifier_:

```diff
@_function-specifier_@:
    virtual
+   _Noreturn
    @_explicit-specifier_@
    
@_explicit-specifier_@:
    explicit ( @_constant-expression_@ )
    explicit
```

Add a new paragraph to the end of [dcl.fct.spec]{.sref} (this is the same wording as in [dcl.attr.noreturn]{.sref}):

::: addu
[5]{.pnum} If a function `f` is called where `f` was previously declared with the `_Noreturn` specifier and `f` eventually returns, the behavior is undefined.
[_Note_: The function may terminate by throwing an exception.
— _end note_]
:::

Change all uses of `[[noreturn]]` as an attribute in [support]{.sref}
to use the `@[_Noreturn]{.kw}@` specifier instead. Those uses are:

::: bq
* `abort`, `exit`, `_Exit`, and `quick_exit` in [cstdlib.syn]{.sref}
* `abort`, `exit`, `_Exit`, and `quick_exit` in [support.start.term]{.sref}
* `terminate`, `rethrow_exception`, and `throw_with_nested` in [exception.syn]{.sref}
* `terminate` in [terminate]{.sref}
* `rethrow_exception` in [propagation]{.sref}
* `nested_exception::rethrow_nested` and `throw_with_nested` in [except.nested]{.sref}
 `longjmp` in [csetjmp.syn]{.sref}
:::     

## Feature-test macro

Add the feature-test macro `__cpp_noreturn`. This will let users properly
add non-returning semantics to their functions:

```cpp
#if __cpp_noreturn
#  define NORETURN _Noreturn
#elif __cpp_has_attribute(noreturn)
#  define NORETURN [[noreturn]]
#else
#  define NORETURN
#endif
```

# Acknowledgments

Thanks to Aaron Ballman for pointing me to the relevant C rules and discussing
the issues with me.

---
references:
  - id: C.N1478
    citation-label: C.N1478
    title: "Supporting the 'noreturn' property in C1x"
    author:
        - family: David Svoboda
    issued:
        - year: 2010
    URL: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n1478.htm
  - id: C.N2410
    citation-label: C.N2410
    title: "The noreturn attribute"
    author:
        - family: Aaron Ballman
    issued:
        - year: 2019
    URL: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2410.pdf
  - id: C.N2478
    citation-label: C.N2478
    title: "C Working Draft"
    author:
        - family: WG14
    issued:
        - year: 2020
    URL: http://www.open-std.org/jtc1/sc22/wg14/www/docs/n2478.pdf
  - id: Attributes
    citation-label: Attributes
    title: "EWG discussion of P0840R0"
    author:
      - family: EWG
    issued:
      - year: 2017
    URL: http://wiki.edg.com/bin/view/Wg21albuquerque/P0840R0
---