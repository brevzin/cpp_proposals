---
title: "`do` statement-expressions"
document: PxxxxR0
date: today
audience: EWG
author:
    - name: Bruno Cardoso Lopes
      email: <x@y.z>
    - name: Zach Laine
      email: <x@y.z>
    - name: Michael Park
      email: <x@y.z>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

C++ is a language built on statements. `if` is not an expression, loops aren't expressions, statements aren't expressions (except maybe in the specific case of `$expression$;`).

When a single expression is insufficient, the only solution C++ currently has as its disposal is to invoke a function - where that function can now contain arbitrarily many statements. Since C++11, that function can be expressed more conveniently in the form of an immediately invoked lambda.

However, this approach leaves a lot to be desired. An immediately invoked lambda introduced an extra function scope, which makes control flow much more challenging - it becomes impossible to `break` or `continue` out of a loop, and attempting to `return` from the enclosing function or `co_await`, `co_yield`, or `co_return` from the enclosing coroutine becomes an exercise in cleverness.

You also have to deal with the issue that the difference between initializing a variable used an immediately-invoked lambda and initializing a variable from a lambda only differs in the trailing `()`, arbitrarily deep into an expression, which are easy to forget. Some people actually use `std::invoke` in this context, specifically to make it clearer that this lambda is, indeed, intended to be immediately invoked.

This problem surfaces especially brightly in the context of pattern matching [@P1371R3], where the current design is built upon a sequence of:

::: bq
```
$pattern$ => $expression$;
```
:::

This syntax only allows for a single `$expression$`, which means that pattern matching has to figure out how to deal with the situation where the user wants to write more than, well, a single expression. The current design is to allow `{ $statement$ }` to be evaluated as an expression of type `void`. This is a hack, which is kind of weird (since such a thing is not actually an expression of type `void`), but also limits the ability for pattern matching to support another kind of useful syntax: `=> $braced-init-list$`:

::: bq
```cpp
auto f() -> std::pair<int, int> {
    // this is fine
    return {1, 2};

    // this is ill-formed in P1371
    return true match -> std::pair<int, int> {
        _ => {1, 2}
    };
}
```
:::

There's no way to make that work, because `{` starts a statement. So the choice in that paper lacks orthogonality: we have a hack to support multiple expressions (which are very important to support) that is inventing such support on the fly, in a novel way that is very narrow (only supports `void`), that throws other useful syntax under the bus.

What pattern matching really needs here is a statement-expression syntax. But it's not just pattern matching that has a strong desire for statement-expressions, this would be a broadly useful facility, so we should have an orthogonal language feature that supports statement-expressions in a way that would allow pattern matching to simplify its grammar to:

::: bq
```
$pattern$ => $expr-or-braced-init-list$;
```
:::

# `do` statement-expressions

Our proposal is the addition of a new kind of expression, called a `do` statement-expression.

In its simplest form:

::: bq
```cpp
int x = do { do_yield 42; };
```
:::

A `do` statement-expression consists of a sequence of statements, but is still, itself, an expression (and thus has a value and a type). There are a lot of interesting rules that we need to discuss about how those statements behave.

## Scope

A `do` statement-expression does introduce a new block scope - as the braces might suggest. But it does _not_ introduce a new function scope. There is no new stack frame. Which is what allows external control flow to work (see below).

## `do_yield` statement

The new `do_yield` statement has the same form as the `return` statement we have today: `do_yield $expr-or-braced-init-list$@~opt~@;`. It's behavior corresponds closely to that `return`, in unsurprising ways - `do_yield` yields from a `do` statement-expression in the same way that `return` returns from a function.

## Type and Value Category

The expression `do { do_yield 42; }` is a prvalue of type `int`. We deduce the type from all of the `do_yield` statements, in the same way that `auto` return type deduction works for functions and lambdas.

An explicit `$trailing-return-type$` can be provided to override this:

::: bq
```cpp
do -> long { do_yield 42; }
```
:::

If no `do_yield` statement appears in the body of the `do` statement-expression, or every `do_yield` statement is of the form `do_yield;`, then the expression is a prvalue of type `void`.

Falling off the end of a `do` statement-expression behaves like an implicit `do_yield;` - if this is incompatible the type of the `do` statement-expression, the expression is ill-formed. This is the one key difference with functions: this case is not undefined behavior. This will be discussed in more detail later.

This makes the pattern matching cases [@P2688R0] work pretty naturally:

::: cmptable
### P2688R0
```cpp
x match {
    0 => { cout << "got zero"; };
    1 => { cout << "got one"; };
    _ => { cout << "don't care"; };
}
```

### Proposed
```cpp
x match {
    0 => do { cout << "got zero"; };
    1 => do { cout << "got one"; };
    _ => do { cout << "don't care"; };
}
```
:::

Here, the whole `match` expression has type `void` because each arm has type `void` because none of the `do` statement-expressions have a `do_yield` statement.

Yes, this requires an extra `do` for each arm, but it means we have a language that's much easier to explain because it's consistent - `do { cout << "don't care"; }` is a `void` expression in _any_ context. We don't have a `$compound-statement$` that happens to be a `void` expression just in this one spot.

::: cmptable
### P2688R0
```cpp
auto f(int i) {
    return i match -> std::pair<int, int> {
        0 => {1, 2};          // ill-formed
        _ => std::pair{3, 4}; // ok
    }
}
```

### Proposed
```cpp
auto f(int i) {
    return i match -> std::pair<int, int> {
        0 => {1, 2};          // ok
        _ => std::pair{3, 4}; // ok
    }
}
```
:::

Here, the existing pattern matching cannot support a `$braced-init-list$` because `{` is used for the special `void`-statement-case. But if we had `do` statement-expressions, the grammar of pattern matching can use `$expr-or-braced-init-list$` in the same way that we already do in many other places in the C++ grammar. This example just works.

## Copy Elision

All the rules for initializing from `do` statement-expression, and the way the expression that appears in a `do_yield` statement is treated, are the same as what the rules are for `return`.

Implicit move applies, for variables declared within the body of the `do` statement-expression. In the following example, `r` is an unparenthesized `$id-expression$` that names an automatic storage variable declared within the statement, so it's implicitly moved:

```cpp
std::string s = do {
    std::string r = "hello";
    r += "world";
    do_yield r;
};
```

Note that automatic storage variables declared within the function that the `do` statement-expression appears, but not declared within the statement-expression itself, are *not* implicitly moved (since they can be used later).

## Control Flow

In a regular function, there are four ways to escape the function scope:

1. a `return` statement
2. `throw`ing an exception
3. invoking a `[[noreturn]]` function (e.g. `std::abort()`)
4. falling off the end of the function (undefined behavior if the return type is not `void`)

The same is true for coroutines, except substituting `return` for `co_return` (and likewise falling off the end is undefined behavior if there is no `return_void()` function on the promise type).

For a `do` statement expression, we have two different directions where we can escape (in a non-exception, non-`[[noreturn]]` case): we either yield an expression, or we escape the _outer_ scope. That is, we can also:

5. `return` from the enclosing function (or `co_return` from the enclosing coroutine)
6. `break` or `continue` from the innermost enclosing loop (if any, ill-formed otherwise)

Additionally, for point (4) while we could simply (for consistency) propagate the same rules for falling-off-the-end as functions, then lambdas (C++11), then coroutines (C++20), we would like to consider not introducing another case for undefined behavior here and enforcing that the user provides more information themselves.

That is, the rule we propose that the implementation form a control flow graph of the `do` statement-expression and consider each one of the six escaping kinds described above. All `do_yield` statements (including the implicit `do_yield;` introduced by falling off the end, if the implementation cannot prove that it does not happen) need to either have the same type (if no `$trailing-return-type$`) or be compatible with the provided return type (if provided). Anything else is ill-formed.

Let's go through some examples.

<table>
<tr><th>Example</th><th>Discussion</th></tr>
<tr><td>
```cpp
auto a = do {
    if ($cond$) {
        do_yield 1;
    } else {
        do_yield 2;
    }
};
```
</td><td>OK: All yielding control paths have the same type. There's no falling off the end.</td></tr>
<tr><td>
```cpp
auto b = do {
    if ($cond$) {
        do_yield 1;
    } else {
        do_yield 2.0;
    }
};
```
</td><td>Error: The yielding control paths have different types and there is no provided `$trailing-return-type$`. This would be okay if it were `do -> int { ... }` or `do -> double { ... }` or `do -> float { ... }`, etc.</td></tr>
<tr><td>
```cpp
auto c = do {
    if ($cond$) {
        do_yield 1;
    }

    do_yield 2;
};
```
</td><td>OK: Similar to `a`, all yielding control paths yield the same type. There is no falling off the end here, it is not important that a yielding `if` has an `else`.</td></tr>
<tr><td>
```cpp
auto d = do {
    if ($cond$) {
        do_yield 1;
    }
};
```
</td><td>Error: There are two yielding control paths here: the `do_yield 1;` and the implicit `do_yield;` from falling off the end, those types are incompatible. The equivalent in functions and coroutines would be undefined behavior in if `$cond$` is `false`.</td></tr>
<tr><td>
```cpp
int e = do {
    if ($cond$) {
        do_yield;
    }
}, 1;
```
</td><td>OK: As above, there are two yielding control paths here, but both the explicit and the implicit ones are `do_yield;` which are compatible.</tr>
<tr><td>
```cpp
int f = do {
    if ($cond$) {
        do_yield 1;
    }

    throw 2;
};
```
</td><td>OK: We no longer fall off the end here, since we always escape. There is only one yielding path.</td></tr>
<tr><td>
```cpp
int outer() {
    int g = do {
        if ($cond$) {
            do_yield 1;
        }

        return 3;
    };
}
```
</td><td>OK: Similar to the above, it's just that we're escaping by returning from the outer function instead of throwing. Still not falling off the end.</td></tr>
<tr><td>
```cpp
int h = do {
    if ($cond$) {
        do_yield 1;
    }

    std::abort();
};
```
</td><td>Unclear: This is a very interesting case to consider, see discussion on `[[noreturn]]` below.</td></tr>
<tr><td>
```cpp
enum Color {
    Red,
    Green,
    Blue
};

void func(Color c) {
    std::string_view name = do {
        switch (c) {
        case Red:   do_yield "Red"sv;
        case Green: do_yield "Green"sv;
        case Blue:  do_yield "Blue"sv;
        }
    };
}
```
</td><td>Error: This is probably the most interesting case when it comes to falling off the end. Here, the user knows that `c` only has three values, but the implementation does not, so it could still fall off the end. gcc does warn on the equivalent function form of this, clang does not. The typical solution here might be to add `__builtin_unreachable()`, now `std::unreachable()`, to the end of the function, but for this to work we have to discuss `[[noreturn]]` below. Barring that, the user would have to add either some default value or some other kind of control flow (like an exception, etc).</td></tr>
<tr><td>
```cpp
void func() {
    for (;;) {
        int j = do {
            if ($cond$) {
                break;
            }

            for ($something$) {
                if ($cond$) {
                    do_yield 1;
                }
            }

            do_yield 2;
        };
    }
}
```
</td><td>OK: The first `break` escapes the `do` statement-expression and breaks from the outer loop. Otherwise, we have two yielding statements which both yield `int`. If the `do_yield 2;` statement did not exist, this would be ill-formed unless the compiler could prove that the loop itself did not terminate.

If the loop were `for (;;)`, then the lack of `do_yield 2;` would be fine - but anything more complicated than that would require some kind of final yield (or `throw`, etc.)</td></tr>
</table>

To reiterate: the implementation produces a control flow graph of the `do` statement-expression and considers all yielding statements (*including* the implicit `do_yield;` on falling off the end, if the implementation considers that to be a possible path) in order to determine correctness of the statement-expression. The kinds of control flow that escape the statement entirely (exceptions, `return`, `break`, `continue`, `co_return`) do not need to be considered for purposes of consistency of yields (since they do not yield values).

### `noreturn` functions

The language currently has several kinds of escaping control flow that it recognizes. As mentioned, exceptions, `return`, `continue`, `break`, and `co_return`. And, allegedly, `goto`.

But there's one kind of escaping control flow that it _does not_ currently recognize: functions marked `[[noreturn]]`. A call to `std::abort()` or `std::terminate()` or `std::unreachable()` escapes control flow, for sure, but the because this is just an attribute, the language cannot add semantic meaning to it. In particular, it is not feasible at the moment for us to call this necessarily well-formed:

::: bq
```cpp
int i = do {
    if ($cond$) {
        do_yield 5;
    }

    std::abort();
};
```
:::

By the rules laid out above, this is still falling off the end, and would require adding a `do_yield` statement that yields an `int` or to have some other language-recognized escaping control flow. We're not allowed to recognize `std::abort()` itself as escaping control flow.

Pattern Matching has this same problem - it needs to support arms that might `std::terminate()` or are `std::unreachable()`, but because it cannot recognize these functions in any way, it needs dedicated syntax to do this. Currently, the paper spells it `!{ std::terminate(); }`. Which is... less than ideal.

There are three solutions to this problem.

The first solution would be not do anything special about `[[noreturn]]` functions, but have all the implementations simply agree to Do The Right Thing here. We all know that `[[noreturn]]` functions don't return, so even though the rules would say that this example leads to an incompatible yield, implementations just accept anyway because they know that it's perfectly valid. Perhaps there's a way to word this sufficiently loosely that it doesn't end up seeming like the implementations just aren't implementing what's in the standard.

The second, narrow solution would be to simply enumerate all the `[[noreturn]]` functions in the standard library (they are `abort`, `exit`, `_Exit`, `quick_exit`, `terminate`, `rethrow_exception`, `throw_with_nested`, `longjmp`, and `unreachable`) and recognize them. This solves the problem for standard library non-returning functions, but doesn't help any user-defined functions. That's a bit unsatisfying, but at least it does solve a significant subset of the problem. Users would end up having to write something like:

::: bq
```cpp
int i = do {
    if ($cond$) {
        do_yield 5;
    }

    my::abort(); // doesn't count as escaping
    std::abort(); // counts, but utterly pointless
                  // and may cause compiler warnings for unreachable code
};
```
:::

The last solution would be introduce a new function specifier to replace `[[noreturn]]` with something that the language _can_ add semantics to. C already has such a specifier, spelled `_Noreturn`, which is already reserved keyword. That's an ugly and unfamiliar keyword for C++ purposes, but there are a lot of advantages to specifically this choice of keyword:

* there are so few non-returning functions that the value of consistency with C seems more important than trying to come up with any other keyword
* consistency with C in this case means that implementations already support it, and allowing it for C++ is largely flipping a switch
* for how rare non-returning functions should be, having an ugly keyword seems like a benefit in its own right

Of these, the last approach seems like the right one. Elevate `[[noreturn]]` to be a first-class language feature, which allows us to ascribe language semantics to it, which means that `do` statement-expressions and pattern matching arms can recognize non-returning functions properly as actually escaping control flow.

## Grammar Disambiguation

We have to disambiguate between a `do` statement-expression and a `do`-`while` loop.

In an expression-only context, the latter isn't possible, so we're fine there.

In a statement context, a `do` statement-expression is completely pointless - you can just write statements. So we disambiguate in favor of the `do`-`while` loop. If somebody really, for some reason, wants to write a `do` statement-expression statement, they can parenthesize it: `(do { do_yield 42; });`. A statement that begins with a `(` has to then be an expression, so we're now in an expression-only context.

## Prior Art

GCC has an extension called [statement-expressions](https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html), which look very similar to what we're proposing here:

::: cmptable
### gcc
```cpp
({
    int y = foo();
    int z;
    if (y > 0) z = y;
    else z = -y;
    z;
})
```

### Proposed
```cpp
do {
    int y = foo();
    if (y > 0) {
        do_yield y;
    } else {
        do_yield -y;
    }
}
```
:::

The reason we're not simply proposing to standardize the existing extension is that there are two features we see that are lacking in it that are not easy to add:

1. The ability to specify a return type, which is critical for allowing statement-expressions to be lvalues.
2. The ability to support yielding out of different branches of `if`, due the implicit nature of the yield.

For (1), there is simply no obvious place to put the `$trailing-return-type$`. For (2), you can't turn `if`s into expressions in any meaningful way. It is fairly straightforward to answer both questions for our proposed form.

### Namespace Scope

Note that gcc's statement-expressions are not usable at namespace-scope. This is due to the question of where the implementation would put any local variables:

::: bq
```cpp
int i = ({
    int j = 2; // where does this get allocated?
    j;
});
```
:::

At namespace scope, there is a much smaller difference than a statement-expression and an immediately invoked lambda since you don't have any other interesting control flow that you can do - the expression either yields a value or the program terminates. But if we're going to add a new language feature, it seems better to allow it to be used in all expression contexts - we would just have to say what happens in this case. Perhaps it simply behaves as a function scope in such a context?

# Wording

Add to [expr.prim]{.sref}:

::: bq
```diff
$primary-expression$:
  $literal$
  this
  ( $expression$ )
  $id-expression$
  $lambda-expression$
  $fold-expression$
  $requires-expression$
+ $do-statement-expression$
```
:::

Add a new clause [expr.prim.do]:

::: bq
::: addu
[1]{.pnum} A *do-statement-expression* provides a way to combine multiple statements into a single expression without introducing a new function scope.

```
$do-statement-expression$:
  do $trailing-return-type$@~opt~@ { $statement$ }
```
:::
:::

Add to [stmt.pre]:

::: bq
[1]{.pnum} Except as indicated, statements are executed in sequence.
```diff
$statement$:
  $labeled-statement$
  $attribute-specifier-seq$@~opt~@ $expression-statement$
  $attribute-specifier-seq$@~opt~@ $compound-statement$
  $attribute-specifier-seq$@~opt~@ $selection-statement$
  $attribute-specifier-seq$@~opt~@ $iteration-statement$
  $attribute-specifier-seq$@~opt~@ $jump-statement$
  $declaration-statement$
  $attribute-specifier-seq$@~opt~@ $try-block$
+ $attribute-specifier-seq$@~opt~@ $do-yield-statement$
```
:::

Change [stmt.expr]{.sref} to disambugate a `do` statement-expression from a `do`-`while` loop:

::: bq
[1]{.pnum} Expression statements have the form
```
$expression-statement$:
  $expression$@~opt~@;
```
The expression is a *discarded-value* expression. All side effects from an expression statement are completed before the next statement is executed. An expression statement with the expression missing is called a *null statement*. [The expression shall not be a *do-statement-expression*.]{.addu}

[Note 1: Most statements are expression statements — usually assignments or function calls. A null statement is useful to supply a null body to an iteration statement such as a while statement ([stmt.while]). — end note]
:::
