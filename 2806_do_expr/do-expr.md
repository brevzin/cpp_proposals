---
title: "`do` expressions"
document: P2806R4
date: today
audience: EWG
author:
    - name: Bruno Cardoso Lopes
      email: <bruno.cardoso@gmail.com>
    - name: Zach Laine
      email: <whatwasthataddress@gmail.com>
    - name: Michael Park
      email: <mcypark@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
---

# Revision History

Since [@P2806R3], implementation, wording, and introducing implicit last value.

Since [@P2806R2], wording and referencing a longer discussion on divergence in [@P3549R0]{.title}.

Since [@P2806R1], switched syntax from `do return` to `do_return` to avoid ambiguity. Added section on [lifetime](#lifetime).

Since [@P2806R0], some more discussion about implicit last value vs explicit return, reflection, and a grammar fix to the still-incomplete wording.

# Introduction

C++ is a language built on statements. `if` is not an expression, loops aren't expressions, statements aren't expressions (except maybe in the specific case of `$expression$;`).

When a single expression is insufficient, the only solution C++ currently has as its disposal is to invoke a function - where that function can now contain arbitrarily many statements. Since C++11, that function can be expressed more conveniently in the form of an immediately invoked lambda.

However, this approach leaves a lot to be desired. An immediately invoked lambda introduced an extra function scope, which makes control flow much more challenging - it becomes impossible to `break` or `continue` out of a loop, and attempting to `return` from the enclosing function or `co_await`, `co_yield`, or `co_return` from the enclosing coroutine becomes an exercise in cleverness.

You also have to deal with the issue that the difference between initializing a variable used an immediately-invoked lambda and initializing a variable from a lambda only differs in the trailing `()`, arbitrarily deep into an expression, which are easy to forget. Some people actually use `std::invoke` in this context, specifically to make it clearer that this lambda is, indeed, intended to be immediately invoked.

This problem surfaces especially brightly in the context of [@P2688R4]{.title}, where the current design is built upon a sequence of:

::: std
```
$pattern$ => $expression$;
```
:::

This syntax only allows for a single `$expression$`, which means that pattern matching has to figure out how to deal with the situation where the user wants to write more than, well, a single expression. The current design is to allow `{ $statement$ }` to be evaluated as an expression of type `void`. This is a hack, which is kind of weird (since such a thing is not actually an expression of type `void`), but also limits the ability for pattern matching to support another kind of useful syntax: `=> $braced-init-list$`:

::: std
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

::: std
```
$pattern$ => $expr-or-braced-init-list$;
```
:::

# `do` expressions

Our proposal is the addition of a new kind of expression, called a `do` expression.

In its simplest form:

::: std
```cpp
int x = do { do_return 42; };
int y = do { 42 }; // the last expression can omit the semicolon, equivalent to above
```
:::

A `do` expression consists of a sequence of statements, but is still, itself, an expression (and thus has a value and a type). There are a lot of interesting rules that we need to discuss about how those statements behave.

## Scope

A `do` expression does introduce a new block scope - as the braces might suggest. But it does _not_ introduce a new function scope. There is no new stack frame. Which is what allows external control flow to work (see below).

## `do_return` statement

The new `do_return` statement has the same form as the `return` statement we have today: `do_return $expr-or-braced-init-list$@~opt~@;`. It's behavior corresponds closely to that `return`, in unsurprising ways - `do_return` yields from a `do` expression in the same way that `return` returns from a function.

While `do_return $value$;` and `return $value$;` do look quite close together and mean fairly different things, the leading `do` we think should be sufficiently clear, and we think it is a good spelling for this statement.

Other alternative spellings we've considered:

* `do return` (in the previous revision of this paper, which has an ambiguity with `do ... while` loops)
* `do_yield` (presented to EWG in Issaquah as the initial pre-publication draft of this proposal)
* `do yield`
* `do break` (similarly to `return`, we are breaking out of this expression, but is less likely to conflict since `break` is less likely to be used than `return` and also the corresponding `break $value$;` is invalid today)
* `=>` (or some other arrow, like `<-` or `<=`)

## Implicit Last Value

Let's take the example motivating case from [@P2561R2]{.title} and compare implicit last expression to explicit return:

::: cmptable
### Implicit Last Value
```cpp
auto foo(int i) -> std::expected<int, E>

auto bar(int i) -> std::expected<int, E> {
    int j = do {
        auto r = foo(i);
        if (not r) {
            return std::unexpected(r.error());
        }
        *r // <== NB: no semicolon
    };

    return j * j;
}
```

### Explicit Return
```cpp
auto foo(int i) -> std::expected<int, E>

auto bar(int i) -> std::expected<int, E> {
    int j = do {
        auto r = foo(i);
        if (not r) {
            return std::unexpected(r.error());
        }
        do_return *r;
    };

    return j * j;
}
```
:::

In the simple cases, explicit last value (on the left) will be shorter than an explicit return (on the right). But implicit last value is more limited. We cannot do early return (by design), which means that a `do` expression would not be able to return from a loop either. We would have to extend the language to support `if` expressions, so that at the very least the first example above could be made easier - which would add more complexity to the design.

Which is to say — the `do_return` statement is still a valuable and necessary addition, given that we do not have `if` or loop expressions, and we are unlikely to add them.

However, for many common uses of `do` expressions, we don't actually need early return, so paying the syntactic cost seems unnecessary. Which is why we're proposing both.

Note that Rust also allows both (you can label a block expression and then `break` out of it).


## Type and Value Category

The expression `do { do_return 42; }` is a prvalue of type `int`. We deduce the type from all of the `do_return` statements, in the same way that `auto` return type deduction works for functions and lambdas.

An explicit `$trailing-return-type$` can be provided to override this:

::: std
```cpp
do -> long { do_return 42; }
```
:::

If no `do_return` statement appears in the body of the `do` expression, or every `do_return` statement is of the form `do_return;`, then the expression is a prvalue of type `void`.

Falling off the end of a `do` expression behaves like an implicit `do_return;` - if this is incompatible with the type of the `do` expression, the expression is ill-formed. This is the one key difference with functions: this case is not undefined behavior. This will be discussed in more detail later.

This makes the pattern matching cases [@P2688R4] work pretty naturally:

::: cmptable
### P2688R4
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

Here, the whole `match` expression has type `void` because each arm has type `void` because none of the `do` expressions have a `do_return` statement.

Yes, this requires an extra `do` for each arm, but it means we have a language that's much easier to explain because it's consistent - `do { cout << "don't care"; }` is a `void` expression in _any_ context. We don't have a `$compound-statement$` that happens to be a `void` expression just in this one spot.

::: cmptable
### P2688R4
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

Here, the existing pattern matching cannot support a `$braced-init-list$` because `{` is used for the special `void`-statement-case. But if we had `do` expressions, the grammar of pattern matching can use `$expr-or-braced-init-list$` in the same way that we already do in many other places in the C++ grammar. This example just works.

## Copy Elision

All the rules for initializing from `do` expression, and the way the expression that appears in a `do_return` statement is treated, are the same as what the rules are for `return`.

Implicit move applies, for variables declared within the body of the `do` expression. In the following example, `r` is an unparenthesized `$id-expression$` that names an automatic storage variable declared within the statement, so it's implicitly moved:

```cpp
std::string s = do {
    std::string r = "hello";
    r += "world";
    do_return r;
};
```

Note that automatic storage variables declared within the function that the `do` expression appears, but not declared within the statement-expression itself, are *not* implicitly moved (since they can be used later).

## Control Flow

In a regular function, there are four ways to escape the function scope:

1. a `return` statement
2. `throw`ing an exception
3. invoking a `[[noreturn]]` function, `std::abort()` and `std::unreachable()`
4. falling off the end of the function (undefined behavior if the return type is not `void`)

The same is true for coroutines, except substituting `return` for `co_return` (and likewise falling off the end is undefined behavior if there is no `return_void()` function on the promise type).

For a `do` expression, we have two different directions where we can escape (in a non-exception, non-`[[noreturn]]` case): we either yield an expression, or we escape the _outer_ scope. That is, we can also:

1. `return` from the enclosing function (or `co_return` from the enclosing coroutine)
2. `break` or `continue` from the innermost enclosing loop (if any, ill-formed otherwise)

Additionally, for point (4) while we could simply (for consistency) propagate the same rules for falling-off-the-end as functions, then lambdas (C++11), then coroutines (C++20), we would like to consider not introducing another case for undefined behavior here and enforcing that the user provides more information themselves.

That is, the rule we propose that the implementation form a control flow graph of the `do` expression and consider each one of the six escaping kinds described above. All `do_return` statements (including the implicit `do_return;` introduced by falling off the end, if the implementation cannot prove that it does not happen) need to either have the same type (if no `$trailing-return-type$`) or be compatible with the provided return type (if provided). Anything else is ill-formed.

Let's go through some examples.

<table>
<tr><th>Example</th><th>Discussion</th></tr>
<tr><td>
```cpp
auto a = do {
    if ($cond$) {
        do_return 1;
    } else {
        do_return 2;
    }
};
```
</td><td>OK: All yielding control paths have the same type. There's no falling off the end.</td></tr>
<tr><td>
```cpp
auto b = do {
    if ($cond$) {
        do_return 1;
    } else {
        do_return 2.0;
    }
};
```
</td><td>Ill-formed: The yielding control paths have different types and there is no provided `$trailing-return-type$`. This would be okay if it were `do -> int { ... }` or `do -> double { ... }` or `do -> float { ... }`, etc.</td></tr>
<tr><td>
```cpp
auto c = do {
    if ($cond$) {
        do_return 1;
    }

    do_return 2;
};
```
</td><td>OK: Similar to `a`, all yielding control paths yield the same type. There is no falling off the end here, it is not important that a yielding `if` has an `else`.</td></tr>
<tr><td>
```cpp
auto d = do {
    if ($cond$) {
        do_return 1;
    }
};
```
</td><td>Ill-formed: There are two yielding control paths here: the `do_return 1;` and the implicit `do_return;` from falling off the end, those types are incompatible. The equivalent in functions and coroutines would be undefined behavior in if `$cond$` is `false`.</td></tr>
<tr><td>
```cpp
int e = do {
    if ($cond$) {
        do_return;
    }
}, 1;
```
</td><td>OK: As above, there are two yielding control paths here, but both the explicit and the implicit ones are `do_return;` which are compatible.</tr>
<tr><td>
```cpp
int f = do {
    if ($cond$) {
        do_return 1;
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
            do_return 1;
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
        do_return 1;
    }

    std::abort();
};
```
</td><td>OK: `std::abort()` means that we cannot fall off the end, see discussion on `[[noreturn]]` below.</td></tr>
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
        case Red:   do_return "Red"sv;
        case Green: do_return "Green"sv;
        case Blue:  do_return "Blue"sv;
        }
    };
}
```
</td><td>Ill-formed: This is probably the most interesting case when it comes to falling off the end. Here, the user knows that `c` only has three values, but the implementation does not, so it could still fall off the end. gcc does warn on the equivalent function form of this, clang does not. The typical solution here might be to add `__builtin_unreachable()`, now `std::unreachable()`, to the end of the function, but for this to work we have to discuss `[[noreturn]]` below. Barring that, the user would have to add either some default value or some other kind of control flow (like an exception, etc).</td></tr>
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
                    do_return 1;
                }
            }

            do_return 2;
        };
    }
}
```
</td><td>OK: The first `break` escapes the `do` expression and breaks from the outer loop. Otherwise, we have two yielding statements which both yield `int`. If the `do_return 2;` statement did not exist, this would be ill-formed unless the compiler could prove that the loop itself did not terminate.

If the loop were `for (;;)`, then the lack of `do_return 2;` would be fine - but anything more complicated than that would require some kind of final yield (or `throw`, etc.)</td></tr>
</table>

To reiterate: the implementation produces a control flow graph of the `do` expression and considers all yielding statements (*including* the implicit `do_return;` on falling off the end, if the implementation considers that to be a possible path) in order to determine correctness of the statement-expression. The kinds of control flow that escape the statement entirely (exceptions, `return`, `break`, `continue`, `co_return`) do not need to be considered for purposes of consistency of yields (since they do not yield values).

### `noreturn` functions

The language currently has several kinds of escaping control flow that it recognizes. As mentioned, exceptions, `return`, `continue`, `break`, and `co_return`. And, allegedly, `goto`.

But there's one kind of escaping control flow that it _does not_ currently recognize: functions marked `[[noreturn]]`. A call to `std::abort()` or `std::terminate()` or `std::unreachable()` escapes control flow, for sure, but today this is just an attribute:

::: std
```cpp
int i = do {
    if ($cond$) {
        do_return 5;
    }

    std::abort();

    // we know control flow never gets here, so we should not need to
    // insert an implicit "do_return;"
};
```
:::

Pattern Matching has this same problem - it needs to support arms that might `std::terminate()` or are `std::unreachable()`, so it that proposal currently is introducing a dedicated syntax to mark an arm as non-returning: `!{ std::terminate(); }`. Which is... less than ideal.

However, the rule in [dcl.attr.noreturn]{.sref}/2 is:

::: std
[2]{.pnum} If a function `f` is called where `f` was previously declared with the `noreturn` attribute and `f` eventually returns, the behavior is undefined.
:::

That is normative wording which we can rely on. The above `do` expression can only fall off the end if `std::abort` returns, which is _already_ undefined behavior. We can avoid introducing any new undefined behavior ourselves as part of this feature.

That is: invoking a function marked `[[noreturn]]` can be considering an escaping control flow in exactly the same way that `return`, `break`, `throw`, etc., are already.

Note that this violates the so-called Second Ignorability Rule suggested in [@P2552R2]{.title}, which is a great reason to ignore that rule.

### Always-escaping expressions

Consider:

::: std
```cpp
int i = do -> int {
    throw 42;
};
```
:::

This is weird, but might end up as a result of template instantiation where maybe other control paths (guarded with an `if constexpr`) actually had `do_return` statements in them. So it needs to be allowed.

It does lead to an interesting question: what is `decltype(do { return; })`? We already define `decltype(throw 42)` to be `void`, so would this also be `void`? It's kind of an odd choice, and it would be nice if we had a specific type for an escaping expression. While we could come up with the right language facility to allow the conditional operator (`?:`) and pattern matching to work correctly by ignoring arms that are always-escaping, user-defined code would have no way of differentiating between a real void expression (`std::print("Hello {}!", "EWG")` is actually an expression of type `void`) and an artificial one (`std::abort()` is not really the same thing).

We could instead introduce a new type, `std::noreturn_t` (as an easier-to-type spelling of `⊥`), change `decltype(throw e)` to be `std::noreturn_t` (since nobody actually writes this - code search results are exclusively in compiler test suites) and treat the return types of `[[noreturn]]` functions as `std::noreturn_t`. Then the type system gains understanding of always-escaping expressions/statements and the rules for pattern matching, the conditional operator, `do` expressions, and arbitrary user-defined libraries just fall out.

See reflector discussion [here](https://lists.isocpp.org/ext/2023/05/21202.php) and more thorough discussion in [@P3549R0]{.title}.

### `goto`

Using `goto` in a `do` expression has some unique problems.

Jumping *within* a `do` expression should follow whatever restrictions we already have (see [stmt.dcl]{.sref}). Jumping *into* a `do` expression should be completely disallowed (we would call the `$statement$` of a `do` expression a control-flow limited statement).

Jumping *out* of a `do` expression is potentially useful though, in the same way that `break`, `continue`, and `return` are:

::: std
```cpp
    for (@*loop*~1~@) {
        for (@*loop*~2~@) {
            int i = do {
                if ($cond$) {
                    goto done;
                }

                do_return $value$;
            };
        }
    }
done:
```
:::

Breaking out of multiple loops is one of the uses of `goto` that has no real substitute today. The above example should be fine. But referring to any label that is in scope of the variable we're initializing needs to be disallowed - since we wouldn't have actually initialized the variable. We need to ensure that the [stmt.dcl]{.sref} rule is extended to cover this case.

Also, while computed goto is not a standard C++ feature, it would be nice to disallow this example, courtesy of (of course) JF Bastien (in this case, we are referring to a label that is within `v`'s scope. We're not jumping to it directly, but the ability to jump to it indirectly is still problematic):

::: std
```cpp
#include <stdio.h>

struct label {
    static inline void* e;
    int v;

    label()
    try
        : v(({
            fprintf(stderr, "oh\n");
            e = &&awesome;
            throw 1;
            42;
        }))
    {
        fprintf(stderr, "no\n");
        awesome:
        fprintf(stderr, "you\n");
    } catch(...) {
        fprintf(stderr, "don't\n");
        goto *e;
    }
};

int main() {
    label l;
}
```
:::

### Should falling off the end be undefined behavior?

Consider:

::: std
```cpp
int i = do {
    if ($cond$) {
        do_return 0;
    }
};
```
:::

Is this statement ill-formed (because there is a control path that falls off the end of the `do` expression, as discussed in this section) or should this statement be undefined behavior? The latter would be consistent with functions, lambdas, and coroutines (and not a if-you-squint-enough kind of consistency either, this would be exactly identical).

It would make for a simpler design if we adopted undefined behavior here, but we think it's a better design to force the user to cover all control paths themselves.

## Lifetime

One important question is: when are local variables declared within a `do` expression destroyed?

Let's start with this example:

::: std
```cpp
int i = do {
        std::lock_guard _(mtx);
        do_return get(0);
    } + do {
        std::lock_guard _(mtx);
        do_return get(1);
    };
```
:::

Each `do` expression is locking the same mutex, `mtx`. We believe that it is the overwhelming presumption of C++ that both `lock_guard`s will be destroyed at their nearest `}` (i.e. this will not deadlock). There really is no other reasonable alternative.

However, consider this example, courtesy of Lauri Vasama in the context of discussing lifetime questions in control flow operator ([@P2561R2]). Consider this set of functions, where `find_interesting` returns some `span` into the given `vector` (or returns an `unexpected`):

::: std
```cpp
auto get_data() -> std::vector<int>;
auto find_interesting(std::vector<int> const&) -> std::expected<std::span<int const>, std::string>;
auto best_of(std::span<int const>) -> int;
```
:::

With `do` expressions, it would be tempting to implement a `TRY` macro similar to Rust's `try!` macro. In this case, we could try to do it this way (note that in this case decaying is irrelevant, so we just return everything by value):

::: std
```cpp
#define TRY(expr) do {                          \
    auto __r = expr;                            \
    if (not r) {                                \
        return std::unexpected(__r.error());    \
    }                                           \
    do_return *__r;                             \
}

auto do_something() -> std::expected<int, std::string> {
    int value = best_of(TRY(find_interesting(get_data())));
    return value;
}
```
:::

This _looks_ reasonable. Sure, `get_data()` is a temporary `vector`, but it _looks_ like it persists until the `;`. But that's not what would actually happen. If we expand the macro, and add the explicit return type for clarity, this evaluates as:

::: std
```cpp
auto do_something() -> std::expected<int, std::string> {
    int value = best_of(do -> std::span<int const> {
        auto __r = find_interesting(get_data());
        //                                     ^
        //                              vector destroyed here
        if (not __r) {
            return std::unexpected(__r.error());
        }
        do_return *__r;
    });
    return value;
}
```
:::

The temporary `std::vector<int>` doesn't persist through the whole `do` expression, it gets destroyed too soon — so our `__r` would be holding a dangling `span` (in the non-error case). That's... bad.

So we need some way to "persist" that temporary through the end of the outer expression. And it definitely cannot happen automatically, as just pointed out, so it has to be explicit in some way. We can think of three approaches to doing this:

1. We can annotate the local variable `__r` somehow, such that any temporaries in its initializer persist through the outer full-expression.
2. We can annotate the `do` expression with some kind of anti-capture list, as in `do [__r] { ... }`
3. We can introduce a new kind of expression expressly for this purpose.

For the latter, we mean something like this:

::: cmptable
### Current
```cpp
do {
    auto __r = expr;
    if (not r) {
        return std::unexpected(__r.error());
    }
    do_return *__r;
}
```

### Proposed
```cpp
(auto __r = expr; do {
    if (not r) {
        return std::unexpected(__r.error());
    }
    do_return *__r;
})
```
:::

A new expression fo the form `($decl$, $expr$)` where any temporaries in the declaration last until the end of their full-expression — which is just the usual rule for when temporaries last. Except in this case, the temporary `get_data()` will not be inside of a new full-expression (the `do`-expression), it will be in the place where it needs to be. We think this is the right approach to addressing lifetime issues, in a way that likely scales better.

### Conditional Lifetime Extension

An interesting sub-question on lifetimes is what does this do:

::: std
```cpp
auto prvalue() -> T;

auto f() -> void {
    // lifetime extension, reference bound to temporary
    T const& r1 = prvalue();

    // dangling
    T const& r2 = []() -> T const& { return prvalue(); };

    // lifetime extension??
    T const& r3 = do -> T const& { do_return prvalue(); };
}
```
:::

`r1` is our familiar lifetime extension case - a reference is bound to a temporary. `r2` is definitely dangling. The temporary is destroyed and definitely does not last as long as `r2`. But what about `r3`, is it more like `r1` or `r2`?

In this case, we see the entire `do` expression - so unlike the general callable case, it may actually be possible for lifetime extension to work here.

But as we're thinking about it, let's make the example slightly more complicated:

::: std
```cpp
auto lvalue() -> T const&;
auto prvalue() -> T;

auto g(bool c) -> void {
    T const& r4 = do -> T const& {
        if (c) {
            do_return lvalue();
        } else {
            do_return prvalue();
        }
    }

    T const& r5 = do -> T const& {
        if (c) {
            do_return lvalue();
        } else {
            T x = prvalue();
            do_return x;
        }
    }
}
```
:::

If `r3` doesn't dangle (and we do lifetime extension), does `r4`? Well, presumably. But here we have a form of runtime-conditional lifetime extension. That's still seemingly doable - we would effectively have an `optional<T> __storage` that is declared before `r4` and then `r4` is either a reference into that or whatever we got from `lvalue()`.

But even if we made `r4` work, `r5` now almost certainly cannot - now we're definitely not binding a temporary to a reference, this is quite adrift from our usual rules.

Which makes us wonder if there's really any value in being adventurous here - and instead probably consider that there is no lifetime extension in any of these cases, not in `r3`, not in `r4`, and definitely not in `r5`.


## SFINAE-Friendliness

The only part of a `do` expression that is in the immediate context of a template substitution is the trailing-return-type (if any). Any alternative choice would require SFINAE on statements, which the language does not currently support and we are not trying to tackle in this paper.

This is consistent with lambdas, where the lambda body is not in the immediate context.

## Grammar Disambiguation

We have to disambiguate between a `do` expression and a `do`-`while` loop.

In an expression-only context, the latter isn't possible, so we're fine there.

In a statement context, a `do` expression is completely pointless - you can just write statements. So we disambiguate in favor of the `do`-`while` loop. If somebody really, for some reason, wants to write a `do` expression statement, they can parenthesize it: `(do { do_return 42; });`. A statement that begins with a `(` has to then be an expression, so we're now in an expression-only context.

A previous iteration of the paper used `do return` (with a space), which would lead to an ambiguity in the following in the context of a `do` expression:

::: std
```cpp
do return $value$; while ($cond$);
```
:::

This could have been parsed as a `do return` statement followed by an infinite loop (that would never be executed because we've already returned out of the expression) or as a `do`-`while` loop containing a single, unbraced, return statement. If we use the `do_return` spelling, there's no such ambiguity, since the above can only be a `do ... while` loop.

Also because we would unconditionally parse a statement as beginning with `do` as a `do`-`while` loop, code like this would not work:

::: std
```cpp
do { do_return X{}; }.foo();
```
:::

Such code would also have to be parenthesized to disambiguate, which doesn't seem like a huge burden on the user.


## Prior Art

GCC has had an extension called [statement-expressions](https://gcc.gnu.org/onlinedocs/gcc/Statement-Exprs.html) for decades, which look very similar to what we're proposing here:

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
        do_return y;
    } else {
        do_return -y;
    }
}
```
:::

The reason we're not simply proposing to standardize the existing extension is that there are two features we see that are lacking in it that are not easy to add:

1. The ability to specify a return type, which is critical for allowing statement-expressions to be lvalues.
2. The ability to support yielding out of different branches of `if`, due the implicit nature of the yield.

For (1), there is simply no obvious place to put the `$trailing-return-type$`. For (2), you can't turn `if`s into expressions in any meaningful way. It is fairly straightforward to answer both questions for our proposed form.

With allowing [implicit last value](#implicit-last-value), the simple translation from gcc statement-expression to `do` expression is just a matter of swapping parentheses for a leading `do`. We're just generalizing to make the feature more flexible.

## What About Reflection?

A question that often comes up, for any language feature: if we had reflection and, in particular, code injection: would we need this facility?

The answer is not only yes, but reflection is a good motivating use-case for this facility. Because the language does not have any kind of block expression today, adding support for one would increase the amount of ways that code injection could work.

One example might be, again, the control flow operator proposal in [@P2561R2]{.title}. If reflection allows me to write a hygienic macro that does code injection, perhaps we could write a library such that `try_(E)` would inject an expression that would evaluate in the way that that paper proposes. But in order to do such a thing, we would need to be able to have a block expression to inject. This paper provides such a block expression.

## Where can `do` expressions appear

gcc's statement-expressions are not usable in all expression contexts. Trying to use them at namespace-scope, or in a default member initializer, etc, fails:

::: std
```cpp
int i = ({      // error: statement-expressions are not allowed outside functions
    int j = 2;  //        nor in template-argument lists
    j;
});
```
:::

In such contexts, there is a much smaller difference than a statement-expression and an immediately invoked lambda since you don't have any other interesting control flow that you can do - the expression either yields a value or the program terminates.

But if we're going to add a new language feature, it seems better to allow it to be used in all expression contexts - we would just have to say what happens in this case. Especially since if we're adding a feature to subsume immediately invoked lambdas, it would be preferable to subsume _all_ immediately invoked lambdas, not just some or most.

We can think of a `do` expression as simply behaving like an immediately invoked lambda in such contexts. Not in the sense of allowing `return` statements (there's still no enclosing function to return out of), but the sense that any local variables declared would exist in a function stack. But this is probably more of a compiler implementation detail rather than a language design detail.

In short: `do` expressions should be usable in any expression context.

# Wording

## Lex / Intro

Add the keyword `do_return` to the table of keywords in [lex.key]{.sref}:

::: std
```diff
  constinit
  const_cast
  continue
  contract_assert
  co_await
  co_return
  co_yield
  decltype
  default
  delete
  do
+ do_return
  double
  dynamic_cast
  else
  enum
  explicit
  export
```
:::

Add a note to [intro.execution]{.sref}:

::: std
[5]{.pnum} A *full-expression* is

* [5.1]{.pnum} an unevaluated operand ([expr.context]),
* [5.2]{.pnum} a `$constant-expression$` ([expr.const]),
* [5.3]{.pnum} an immediate invocation ([expr.const]),
* [5.4]{.pnum} an `$init-declarator$` ([dcl.decl]) (including such introduced by a structured binding ([dcl.struct.bind])) or a `$mem-initializer$` ([class.base.init]), including the constituent expressions of the initializer,
* [5.5]{.pnum} an invocation of a destructor generated at the end of the lifetime of an object other than a temporary object ([class.temporary]) whose lifetime has not been extended,
* [5.6]{.pnum} the predicate of a contract assertion ([basic.contract]), or
* [5.7]{.pnum} an expression that is not a subexpression of another expression and that is not otherwise part of a full-expression.

[...]

::: {.note .addu}
An expression `E` within a `$do-expression$` `D` ([expr.prim.do]) can still be a full-expression even though the `$do-expression$` itself is an expression because `E` is not a subexpression of `D`.
:::
:::

## Expressions

Add `$do-expression$` to the grammar in [expr.prim.grammar]{.sref}:

::: std
```diff
$primary-expression$:
  $literal$
  this
  ( $expression$ )
  $id-expression$
  $lambda-expression$
  $fold-expression$
  $requires-expression$
+ $do-expression$
  $splice-expression$
```
:::

Extend the rule for move-eligible expressions in [expr.prim.id.unqual]{.sref}:

::: std
[15]{.pnum} An *implicitly movable entity* is a variable with automatic storage duration that is either a non-volatile object or an rvalue reference to a non-volatile object type. An `$id-expression$` or `$splice-expression$` ([expr.prim.splice]) is *move-eligible* if

* [15.1]{.pnum} it designates an implicitly movable entity,
* [15.2]{.pnum} it is the (possibly parenthesized) operand of a `return` ([stmt.return]) [or]{.rm} [,]{.addu} `co_return` ([stmt.return.coroutine])[, or `do_return` ([stmt.do.return])]{.addu} statement, or of a `$throw-expression$` ([expr.throw]), and
* [15.3]{.pnum} each intervening scope between the declaration of the entity and the innermost enclosing scope of the expression is a block scope and, for a `$throw-expression$`, is not the block scope of a `$try-block$` or `$function-try-block$`.
:::

Add a new clause [expr.prim.do] after [expr.prim.req.nested]{.sref}:

::: std
::: addu
**Do expressions [expr.prim.do]**

[1]{.pnum} A *do-expression* provides a way to combine multiple statements into a single expression without introducing a new function scope. A `do_return` statement ([stmt.do.return]) produces the result of the `$do-expression$`. Other jump statements can transfer control out of a `$do-expression$` without producing a value.

::: example
```cpp
constexpr int f(int i) {
    int half = do {
        if (i % 2 != 0) {
            return -1;              // returns from f
        }
        do_return i / 2;            // produces the value of the do-expression
    };
    return half;
}

static_assert(f(5) == -1);
static_assert(f(4) == 2);
```
:::

```
$do-expression$:
    do $trailing-return-type$@~opt~@ { $do-compound-statement$ }

$do-compound-statement$:
    $statement-seq$@~opt~@ $label-seq$@~opt~@ $do-result-expression$@~opt~@

$do-result-expression$:
    $expression$
```

A `$do-result-expression$` is treated as a `do_return` statement with an operand that is the `$expression$`.

::: example
```cpp
int a = do { 42 };               // OK, equivalent to do { do_return 42; }
int b = do { int x = 2; x + 3 }; // OK, equivalent to do { int x = 2; do_return x + 3; }
```
:::

[#]{.pnum} The `$do-compound-statement$` of a `$do-expression$` is a control-flow-limited statement ([stmt.label]).

[#]{.pnum} A `return` statement ([stmt.return]) appearing in a `$do-expression$` transfers control to the caller of the function that lexically contains the `$do-expression$`.
A `co_return`, `co_await`, or `co_yield` statement or expression appearing in a `$do-expression$` acts on the coroutine that lexically contains the `$do-expression$`. [That is, a `$do-expression$` is transparent to the enclosing function or coroutine.]{.note}

[#]{.pnum} A `break` or `continue` statement appearing in a `$do-expression$` refers to an enclosing `$iteration-statement$` or `switch` statement that contains the `$do-expression$`. [A `break` or `continue` statement cannot be used to exit a `$do-expression$` itself.]{.note}

[#]{.pnum} A `$do-expression$` that is not within a function shall not contain a `return` statement, a `co_return` statement, a `co_await` expression, or a `co_yield` expression.

[#]{.pnum} A `goto` statement ([stmt.goto]) shall not transfer control into a `$do-expression$` from outside that `$do-expression$`.

[#]{.pnum} The type `$DO-TYPE$` of a `$do-expression$` is determined as follows:

* [#.#]{.pnum} If there is a `$trailing-return-type$` that does not contain a placeholder type ([dcl.spec.auto]), then `$DO-TYPE$` is the type specified by that `$trailing-return-type$`.
* [#.#]{.pnum} Otherwise, `$DO-TYPE$` is deduced from the `do_return` statements in the body:
    * [#.#.#]{.pnum} If there is no `do_return` statement (including no `$do-result-expression$`), or every `do_return` statement has no operand, `$DO-TYPE$` is `void`.
    * [#.#.#]{.pnum} Otherwise, `$DO-TYPE$` is deduced as if from a `return` statement using the rules in [dcl.spec.auto.general]. All `do_return` statements shall deduce to the same type; otherwise, the program is ill-formed.

::: example
```cpp
auto a = do { do_return 1; };           // OK, deduces int
auto b = do -> long { do_return 1; };   // OK, explicit type is long
auto c = do {                           // error: inconsistent deduction
    if (cond) do_return 1;
    do_return 2.0;
};
```
:::

[#]{.pnum} The type and value category of the `$do-expression$` are determined from `$DO-TYPE$` as follows:

* [#.#]{.pnum} If `$DO-TYPE$` is `T&`, the `$do-expression$` is an lvalue of type `T`.
* [#.#]{.pnum} Otherwise, if `$DO-TYPE$` is `T&&`, the `$do-expression$` is an xvalue of type `T`.
* [#.#]{.pnum} Otherwise, the `$do-expression$` is a prvalue of type `$DO-TYPE$`.

[#]{.pnum} If control can flow off the end of the `$do-compound-statement$` of a `$do-expression$` whose type is not `$cv$ void`, the program is ill-formed.
[Control flows off the end if there exists any path through the compound statement that does not terminate with a `do_return` statement, a `throw` expression, a call to a `[[noreturn]]` function, or a jump statement that exits the `$do-expression$`.]{.note}

::: example
```cpp
extern bool cond;
void f() {
    auto a = do {                       // error: control may flow off the end
        if (cond) do_return 1;
    };

    auto b = do {                       // OK, all paths yield or exit
        if (cond) do_return 1;
        throw 2;
    };

    auto c = do {                       // OK, return exits the do-expression
        if (cond) do_return 1;
        return;
    };

    (do -> void {                       // OK, void type allows flow off the end
        if (cond) do_return;
    });
}
```
:::

[#]{.pnum} The value of a `$do-expression$` of type `$cv$ void` is the value produced by the executed `do_return` statement.

[#]{.pnum} The copy-initialization of the result of a `$do-expression$` is sequenced before the destruction of local variables declared within the `$do-compound-statement$`.
[A copy or move operation associated with a `do_return` statement can be elided or converted to a move operation the same as for a `return` statement ([class.copy.elision]).]{.note}

[#]{.pnum} A `$do-expression$` can appear in any context where an expression is permitted, including at namespace scope.
[At namespace scope or in a default member initializer, there is no enclosing function, so `return`, `break`, `continue`, and coroutine statements/expressions cannot be used.]{.note}
:::
:::

## Statements

Change [stmt.expr]{.sref} to disambiguate a `$do-expression$` from a `do`-`while` loop:

::: std
[1]{.pnum} Expression statements have the form
```
$expression-statement$:
  $expression$@~opt~@;
```
The expression is a *discarded-value expression* ([expr.context]). All side effects from an expression statement are completed before the next statement is executed. An expression statement with the expression missing is called a *null statement*. [The `$expression$` shall not be a `$do-expression$`.]{.addu}

[A statement beginning with the keyword `do` is always parsed as a `do`-`while` statement ([stmt.do]). A `$do-expression$` used as a statement must be parenthesized.]{.note .addu}
:::

Add to the grammar in [stmt.jump.general]{.sref}:

::: std
[1]{.pnum} Jump statements unconditionally transfer control.

```diff
$jump-statement$:
    break ;
    continue ;
    return $expr-or-braced-init-list$@~opt~@ ;
+   do_return $expr-or-braced-init-list$@~opt~@ ;
    $coroutine-return-statement$
    goto $identifier$ ;
```

[2]{.pnum} [...]
:::

Change [stmt.break]{.sref} to prohibit its use in `$do-expression$`s in confusing places:

::: std
[1]{.pnum} A `break` statement shall be enclosed by ([stmt.pre]) an `$iteration-statement$` ([stmt.iter]), an `$expansion-statement$` ([stmt.expand]), or a `switch` statement ([stmt.switch]).
The `break` statement causes termination of the innermost such enclosing statement; control passes to the statement following the terminated statement, if any.

::: addu
[2]{.pnum} A `break` statement shall not appear in a `$do-expression$` ([expr.prim.do]) that is a subexpression of:

* [2.#]{.pnum} the `$init-statement$`, `$condition$`, or `$expression$` of a `for` statement ([stmt.for]),
* [2.#]{.pnum} the `$condition$` of a `while` statement ([stmt.while]),
* [2.#]{.pnum} the `$init-statement$`, `$for-range-declaration$`, or `$for-range-initializer$` of a range-based `for` statement ([stmt.ranged]),
* [2.#]{.pnum} the `$expression$` of a `do` statement ([stmt.do]), or
* [2.#]{.pnum} the `$init-statement$` or `$condition$` of a `switch` statement ([stmt.switch])

unless the enclosing `$iteration-statement$` or `switch` statement of that `break` statement is also within the same `$do-expression$`.
:::

::: example
```cpp
for (int i = do { if (done) break; do_return start; }; // error: break targets the for loop
     i < n; ++i) { }

for (int i = 0; i < n; ++i) {
    int x = do {
        if (done) break;        // OK, exits the for loop
        do_return compute(i);
    };
}

int x = do {
    for (;;) {
        if (done) break;        // OK, break targets loop within the do-expression
    }
    do_return 0;
};
```
:::
:::

Change [stmt.cont]{.sref} similarly:

::: std
[1]{.pnum} A `continue` statement shall be enclosed by ([stmt.pre]) an `$iteration-statement$` ([stmt.iter]) or an `$expansion-statement$` ([stmt.expand]). [...]

::: addu
[2]{.pnum} A `continue` statement shall not appear in a `$do-expression$` ([expr.prim.do]) that is a subexpression of:

* [2.#]{.pnum} the `$init-statement$`, `$condition$`, or `$expression$` of a `for` statement ([stmt.for]),
* [2.#]{.pnum} the `$condition$` of a `while` statement ([stmt.while]),
* [2.#]{.pnum} the `$init-statement$`, `$for-range-declaration$`, or `$for-range-initializer$` of a range-based `for` statement ([stmt.ranged]), or
* [2.#]{.pnum} the `$expression$` of a `do` statement ([stmt.do])

unless the enclosing `$iteration-statement$` of that `continue` statement is also within the same `$do-expression$`.
:::
:::

Add a new subclause [stmt.do.return] "The `do_return` statement" after [stmt.return.coroutine]{.sref}:

::: std
::: addu
**The `do_return` statement [stmt.do.return]**

[#]{.pnum} A `do_return` statement shall appear only within the `$do-compound-statement$` of a `$do-expression$` ([expr.prim.do]), and not within an intervening `$lambda-expression$` or function body.

::: example
```cpp
int f() {
    return do {
        auto g = []() {
            do_return 1;        // error: crosses lambda boundary
        };
        do_return 2;            // OK
    };
}
```
:::

[#]{.pnum} The `$expr-or-braced-init-list$` of a `do_return` statement is called its operand. A `do_return` statement with no operand shall be used only in a `$do-expression$` whose type is `$cv$ void`. A `do_return` statement with an operand of type `void` shall be used only in a `$do-expression$` whose type is `$cv$ void`. A `do_return` statement with any other operand shall be used only in a `$do-expression$` whose type is not `$cv$ void`; the `do_return` statement initializes the result object of the `$do-expression$` by copy-initialization ([dcl.init]) from the operand.

[#]{.pnum} A `do_return` statement that binds

* [#.#]{.pnum} a returned reference or
* [#.#]{.pnum} a constituent reference ([intro.object]) of a returned object

to a temporary expression ([class.temporary]) is ill-formed.

::: example
```cpp
struct S { int& r; };
int& f();

auto a = do -> int const& { do_return f(); };     // OK
auto b = do -> int const& { do_return 42; };      // error: binds reference to temporary
auto c = do -> S { do_return {f()}; };            // OK
auto d = do -> S {
    int x = 1;
    do_return {x};                                // error: binds constituent reference to local
};
```
:::
:::
:::

## Feature-Test Macro

Add to [cpp.predefined]{.sref}:

::: std
```diff
+ __cpp_do_expressions 20XXXXL
```
:::

---
references:
  - id: P3549R0
    citation-label: P3549R0
    title: "Diverging Expressions"
    author:
      - family: Bruno Cardoso Lopes
      - family: Zach Laine
      - family: Michael Park
      - family: Barry Revzin
    issued:
      - year: 2025
        month: 01
        day: 09
    URL: https://wg21.link/p3549r0
---
