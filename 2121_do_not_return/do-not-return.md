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
    __: throw bad_argument();
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

`goto` is a little special, but the others have a lot like `throw`: they 
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
    __: std::terminate();
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
work. It will first introduce the four mechanisms, and then provide a compare
and contrast for them.

## Annotate escaping blocks

At a session in Prague, the paper authors proposed new syntax for introducing
a block which marked the block as `noreturn`:

```cpp
int maybe_terminate(int arg) {
  return inspect (i) {
    0: 42;
    __: !{ std::terminate(); }
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
never return. For the sake of discussion, let's call this type `std::never`. 

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
    __: std::terminate(); // ok, returns std::never, so never returns
  };
}
```

### A context-sensitive keyword indicating escaping

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

Just kidding. The syntax this paper is actually proposing is a context-sensitive
keyword spelled `@[noreturn]{.kw}@` (similar to how `override` and `final` are context-
sensitive):

```cpp
namespace std {
  void abort() @[noreturn]{.kw}@;
  void terminate() @[noreturn]{.kw}@;
}
```

This paper proposes trailing `@[noreturn]{.kw}@`, instead of leading `@[noreturn]{.kw}@` as would
match the use of the attribute, to simplify the grammar: it fits in the same spot
as the _`virt-specifier`_ s that it most similarly otherwise fits with. This
means that libraries straddling multiple language versions may end up having
to write either:
```cpp
NORETURN_ATTR void abort() NORETURN_SPECIFIER
```
or
```cpp
NORETURN(void abort())
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
escaping functions far exceeds the number of declared escaping functions.

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

1. Introduce a context-sensitive `@[noreturn]{.kw}@` as a trailing specifier, or
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

In my opinion, this:

```cpp
namespace std {
   [[noreturn]] void terminate();
}
```

should be enough to make:

```cpp
int maybe_terminate(int arg) {
  return inspect (i) {
    0: 42;
    __: std::terminate();
  };
}
```

work. I would not want to either add a new annotation to, or change the
existing annotion of, functions like `terminate`. And I would not want to have
to annotate every use of `terminate` in an `inspect`-expression, when the
compiler should already know that `terminate` is an escaping function. 

# Wording

My preferred direction is to allow functions
annotated with `[[noreturn]]` to be considered escaping functions, in the same
way that `throw`, `break`, `return`, `continue`, `goto`, and `co_return` can
be considered escaping statements. This requires no wording change at all, since
the feature this change would be necessary for doesn't even exist yet.

But just in case we want to go a different route, this paper does provide wording
for my second preferred direction: a new, non-attribute version of `[[noreturn]]`.

Add `noreturn` to the identifiers with special meanings table in [lex.name]{.sref}/2.

Change [dcl.fct.def.general]{.sref}/1:

::: bq
[1]{.pnum} Function definitions have the form:

```diff
  @_function-definition_@:
-   @_attribute-specifier-seq_~opt~ _decl-specifier-seq_~opt~ _declarator_ [_virt-specifier-seq_~opt~]{.diffdel} _function-body_@
+   @_attribute-specifier-seq_~opt~ _decl-specifier-seq_~opt~ _declarator_ [_trailing-specifier-seq_~opt~]{.diffins} _function-body_@
    @_attribute-specifier-seq_~opt~ _decl-specifier-seq_~opt~ _declarator_ _requires-clause_ _function-body_@

+ @_trailing-specifier-seq_@:
+   @_trailing-specifier_@
+   @_trailing-specifier-seq_ _trailing-specifier_@

+ @_trailing-specifier_@:
+   override
+   final
+   noreturn

  @_function-body_@:
    @_ctor-initializer_~opt~ _compound-statement_@
    @_function-try-block_@
    = default ;
    = delete ;
```

Any informal reference to the body of a function should be interpreted as a reference to the non-terminal _function-body_.
The optional _attribute-specifier-seq_ in a _function-definition_ appertains to the function.
[A _virt-specifier-seq_ can be part of a _function-definition_ only if it is a _member-declaration_]{.rm}.

[b]{.pnum} [A _trailing-specifier-seq_ shall contain at most one of each
_trailing-specifier_. The _trailing-specifier_ s `override` and `final`
shall appear only in the first declaration of a virtual member function ([class.virtual]).]{.addu}
:::

Change the grammar in [class.mem]{.sref}

::: bq
```diff
  @_member-declarator_@:
-   @_declarator_ [_virt-specifier-seq_~opt~]{.diffdel} _pure-specifier_~opt~@
+   @_declarator_ [_trailing-specifier-seq_~opt~]{.diffins} _pure-specifier_~opt~@
    @_declarator_ _requires-clause_@
    @_declarator_ _brace-or-equal-initializer_~opt~@
    @_identifier_~opt~ _attribute-specifier-seq_~opt~ : _constant-expression_ _brace-or-equal-initializer_~opt~@

- @_virt-specifier-seq_@:
-   @_virt-specifier_@
-   @_virt-specifier-seq_ _virt-specifier_@

- @_virt-specifier_@:
-   override
-   final
```
:::

Remove [class.mem]{.sref}/14:

::: bq
[A _virt-specifier-seq_ shall contain at most one of each _virt-specifier_.]{.rm}
[A _virt-specifier-seq_ shall appear only in the first declaration of a virtual member function ([class.virtual]).]{.rm}
:::

Change [class.virtual]{.sref}/4:

::: bq
If a virtual function `f` in some class `B` is marked with the [_virt-specifier_]{.rm}
[_trailing-specifier_]{.addu} `final` and in a class `D` derived from `B` a function `D​::​f` overrides `B​::​`f, the program is ill-formed.
:::

Change [class.virtual]{.sref}/5:

::: bq
If a virtual function is marked with the [_virt-specifier_]{.rm} [_trailing-specifier_]{.addu} `override` and does not override a member function of a base class, the program is ill-formed.
:::

Copy [dcl.attr.noreturn]{.sref} into [dcl.fct] somewhere, applying the following
changes (presented as a diff for reviewer clarity):

::: bq
[1]{.pnum} The [_attribute-token_]{.rm} [_trailing-specifier_]{.addu}
`noreturn` specifies that a function does not return.
[It shall appear at most once in each _attribute-list_ and no _attribute-argument-clause_ shall be present.]{.rm}
[The attribute may be applied to the _declarator-id_ in a function declaration.]{.rm}
The first declaration of a function shall specify the `noreturn` [attribute]{.rm} [specifier]{.addu} if any declaration of that function specifies the
[_trailing-specifier_]{.addu} `noreturn` [attribute]{.rm}.
If a function is declared with the [_trailing-specifier_]{.addu} `noreturn` [attribute]{.rm} in one translation unit and the same function is declared without the [_trailing-specifier_]{.addu} `noreturn` [attribute]{.rm} in another translation unit, the program is ill-formed, no diagnostic required.

[2]{.pnum} If a function `f` is called where `f` was previously declared with the
[_trailing-specifier_]{.addu} `noreturn` [attribute]{.rm} and `f` eventually returns, the behavior is undefined.
[ *Note*: The function may terminate by throwing an exception.
— *end note*
 ]
[ *Note*: Implementations should issue a warning if a function marked `noreturn` might return.
— *end note*
 ]

[3]{.pnum} [ *Example*:

```diff
- @[[[ noreturn ]]]{.diffdel}@ void f() {
+ void f() @[noreturn]{.diffins}@ {
    throw "error";                // OK
  }

- @[[[ noreturn ]]]{.diffdel}@ void q(int i) {  // behavior is undefined if called with an argument <= 0
+ void q(int i) @[noreturn]{.diffins}@ {  // behavior is undefined if called with an argument <= 0
    if (i > 0)
      throw "positive";
  }
```
— *end example*
 ]

:::

Change all uses of `[[noreturn]]` as an attribute in [language.support]{.sref}
to be trailing `noreturn` instead. Those uses are:

::: bq
* `abort`, `exit`, `_Exit`, and `quick_exit` in [cstdlib.syn]{.sref}
* `abort`, `exit`, `_Exit`, and `quick_exit` in [support.start.term]{.sref}
* `terminate`, `rethrow_exception`, and `throw_with_nested` in [exception.syn]{.sref}
* `terminate` in [terminate]{.sref}
* `rethrow_exception` in [propagation]{.sref}
* `nested_exception::rethrow_nested` and `throw_with_nested` in [except.nested]{.sref}
* `longjmp` in [csetjmp.syn]{.sref}
:::     

## Feature test macro

This feature requires the macro `__cpp_noreturn`.

---
references:
  - id: Attributes
    citation-label: Attributes
    title: "EWG discussion of P0840R0"
    author:
      - family: EWG
    issued:
      - year: 2017
    URL: http://wiki.edg.com/bin/view/Wg21albuquerque/P0840R0
---