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
perfectly valid use of `inspect`. 

This, in of itself, isn't novel. We already carve out an exception for `throw`
in the conditional operator, and the following rewrite of the above example
has been valid for a long time:

```cpp
int g(int arg) {
    return (arg == 0) ? 42 : throw bad_argument();
}
```

But while `throw` is the only exception (not sorry) for the ternary operator,
there are other statements in C++ which escape their scope and thus could
potentially be excluded from consideration when it comes to types. The full set
of such keyword-driven statements is:

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

The rule we have for attributes is that ignoring an attribute must still result
in a correct program. The above `inspect`, if we added semantic meaning to
`[[noreturn]]`, could work - it would ignore the second pattern and simply
deduce the type of the `inspect`-expression as `int`. But if we ignored `[[noreturn]]`,
then we have two patterns of differing type (`int` and `void`) and the type
of the `inspect`-expression would have to be either `void` or ill-formed. This
violates the attribute rule.

However, being able to `std::terminate()` or `std::abort()` or `std::unreachable()`
in a particular case is an important feature in an `inspect`-expression, and so
the problem of how to make it work must be resolved... somehow.

At a session in Prague, the paper authors introduced new syntax for introducing
a block, effectively marking the block as `noreturn`:

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

But given that we're making a language change anyway, this seems like entirely
the wrong approach to me. The set of `[[noreturn]]`-annotated functions is
very small, and we already have to annotate these functions. We should instead
elevate `noreturn` to be a first-class language feature so that we can treat
`std::terminate()` the same as a `return` or a `throw` without requiring further
annotation on all uses of it.

# Proposal

This paper proposes to elevate the `[[noreturn]]` function attribute into
a first class language feature, so that future language evolution (e.g. pattern
matching) may then take this into account in determining semantics.

The syntax this paper is proposing is:

```cpp
namespace std {
  do not return or else void abort();
  do not return or else void terminate();
}
```

Just kidding. The syntax this paper is actually proposing is a context-sensitive
keyword spelled `noreturn` (similar to how `override` and `final` are context-
sensitive):

```cpp
namespace std {
  void abort() noreturn;
  void terminate() noreturn;
}
```

This paper proposes trailing `noreturn`, instead of leading `noreturn` as would
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
but there are a fairly small number of such functions, so I don't think it's a
huge problem.

# Wording

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
_trailing-specifier_. A _trailing-specifier-seq_ shall appear only in the first declaration of a function. The _trailing-specifier_ s `override` and `final`
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

Move [dcl.attr.noreturn]{.sref} into [dcl.fct] somewhere, applying the following
changes:

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
