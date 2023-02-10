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

There's no way to make that work, because `{` starts a statement. So the choice in that paper lacks orthogonality: we have a hack to support multiple expressions (which are very important to support!) that is inventing such support on the fly, in a novel way, that throws other useful syntax under the bus.

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

## Type and Value Category

In the above, `do { do_yield 42; }` is a prvalue of type `int`. We deduce the type from all of the `do_yield` statements, in the same way that `auto` return type deduction works for functions and lambdas.

An explicit `$trailing-return-type$` can be provided to override this:

::: bq
```cpp
do -> long { do_yield 42; }
```
:::

If no `do_yield` statement appears in the body of the `do` statement-expression, or every `do_yield` statement is of the form `do_yield;`, then the expression is a prvalue of type `void`.

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

---

```cpp
auto f(int i) {
    return i match -> std::pair<int, int> {
        0 => {1, 2};          // ill-formed
        _ => std::pair{3, 4}; // ok
    }
}
```

```cpp
auto f(int i) {
    return i match -> std::pair<int, int> {
        0 => {1, 2};          // ok
        _ => std::pair{3, 4}; // ok
    }
}
```
:::

Here, the whole `match` expression has type `void` because each arm has type `void` because none of the `do` statement-expressions have a `do_yield` statement.

Yes, this requires an extra `do` for each arm, but it means we have a language that's much easier to explain because it's consistent - `do { cout << "don't care"; }` is a `void` expression in _any_ context. We don't have a `$compound-statement$` that happens to be a `void` expression just in this one spot.

## Scope

A `do` statement-expression does introduce a new block scope - as the braces might suggest. But it does _not_ introduce a new function scope. There is no new stack frame. Which is what allows [external control flow](#external-control-flow) to work.

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

There are two kinds of control flow to be considered: the kinds _internal_ to a `do` statement-expression and the kinds _external_ to the expression.

### Internal Control Flow

There are two kinds of internal control flow: conditions and loops. If the conditions and loops don't contain a `do_yield` statement, there's nothing else interesting to say about them. But what if they do? Should that be allowed?

The rule for `if` is that an `if` statement that contains a `do_yield` statement (in any branch) has to have the same type in every branch:

::: bq
```cpp
// ok
auto a = do {
    if (true) {
        do_yield 1;
    } else {
        do_yield 2;
    }
};

// error: type mismatch
auto b = do {
    if (true) {
        do_yield 1;
    } else {
        do_yield 2.0;
    }
};

// ok: same type (implicit else)
auto c = do {
    if (true) {
        do_yield 1;
    }

    do_yield 2;
};

// error: type mismatch (implicit else has type void)
auto d = do {
    if (true) {
        do_yield 1;
    }
};

// ok: both branches have type void
int e = do {
    if (true) {
        do_yield;
    }
}, 1;
```
:::

`if constexpr` count as a control flow in the same way in this context, since the instantiated branch is the only one that is considered. `if consteval`, though, does count as an `if`.

The rule for loops is actually similar - except the two "branches" of a loop are the inside and the outside of the loop. If there is a `do_yield` statement inside of the loop, then the outside of the loop also needs to yield the same type:

::: bq
```cpp
// error: the inside of the loop yields int but the outside is void
int f = do {
    while (get()) {
        do_yield 42;
    }
};

// ok: inside and outside are both bool.
// Here, the else condition of the if is implicitly the
bool g = do {
    for (int i : r) {
        if (i == 0) {
            do_yield false;
        }
    }
    std::print("cool");
    do_yield true;
}
```
:::

### External Control Flow

External control flow, which is one of the main motivations for this facility, escape the statement-expression entirely. For example:

::: bq
```cpp
auto f() -> std::expected<int, E>;

auto g() -> std::expected<int, E> {
    int i = do {
        auto r = f();
        if (not r) {
            return std::unexpected(r.error());
        }
        do_yield *r;
    };

    return i * i;
}
```
:::

The `return` statement there escapes out of the `do` statement-expression, it does not initialize `i`. In the same way that an exception would do, except we're just returning.

The same is true for `continue`, `break`, and `co_return` - the entire expression is escaped entirely.

`co_await` and `co_yield` suspend from the outer coroutine, which would then resume back into the middle of this expression, but the expression would then actually continue.

Note that the above example does not break the `if` rule we articulated above: the branch that returns doesn't `do_yield`, so there's no type mismatch. This is actually similar to something we already have in the language: `false ? throw 42 : 5` is a valid expression, because we know that the exception escapes the expression - it's just that we happen to not support `return`, `continue`, `break`, or `co_return` in this context.

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

## Lifetimes and Lifetime-Extension

TODO

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
  do $trailing-return-type$@~opt~@ { $statement-seq$ }
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
