---
title: "A control flow operator"
document: P2561R3
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
highlighting:
  keywords:
    cpp:
      - match
      - do_return
---

# Revision History

Since [@P2561R2], rewrote [semantics section](#semantics-for-c), added [implementation experience](#implementation-experience), and some more [syntax options](#other-potential-syntaxes-considered).

The title of [@P2561R1] was "An error propagation operator", but this feature is much more general than simply propagating errors - it's really about control flow. So renaming to a control flow operator (and a bunch of other renames of the customization points). The operator itself was renamed from `e??` to `e.try?`.

The title of [@P2561R0] was `operator??`, but isn't actually proposing that token, so it's not the best title. Likewise, `try_traits` is a bad name for the collection of functionality for the same reason that the paper described `try` as being a bad spelling for the operator. `is_ok` has been renamed to `has_value`, since that's actually what we name that facility everywhere. A few other details added in addition to the two renames.

# Preface

It is important to clarify a few things up front. It is not the position of this paper that exceptions are bad. Or that exceptions are good. It is not the goal of this paper to convince you to start using exceptions, nor is it to convince you to stop using exceptions.

This paper simply recognizes that there are many code bases (or parts thereof) that do not use exceptions and probably will not in the future. That could be for performance or space reasons. It could be because exceptions are unsupported on a particular platform. It could be for code understandability reasons. Regardless, some code bases do not use exceptions. Moreover, some problems are not solved well by exceptions -- even in code bases that otherwise use them to solve problems that they are more tailored to solve.

The problem is, C++ does not currently have a good story for error handling without exceptions. We're moving away from returning bool or error codes in favor of solutions like `std::expected` ([@P0323R12]), but the ergonomics of such types are not there yet. Bad ergonomics leads to code that is clunkier than it needs to be, harder to follow, and, significantly and ironically, error-prone.

We should try to improve such uses too.

# Introduction

Let's start with a fairly small example of a series of functions that can generate errors, but don't themselves handle them - they just need to propagate them up. With exceptions, this might look like:

::: std
```cpp
auto foo(int i) -> int; // might throw an E
auto bar(int i) -> int; // might throw an E

auto strcat(int i) -> std::string {
    int f = foo(i);
    int b = bar(i);
    return std::format("{}{}", f, b);
}
```
:::

There's a lot to like about exceptions. One nice advantage is the zero syntactic overhead necessary for propagating errors. Errors just propagate. You don't even have to know which functions can fail.

We don't even need to declare variables to hold the results of `foo` and `bar`, we can even use those expressions inline, knowing that we'll only call `format` if neither function throws an exception:

::: std
```cpp
auto foo(int i) -> int; // might throw an E
auto bar(int i) -> int; // might throw an E

auto strcat(int i) -> std::string {
    return std::format("{}{}", foo(i), bar(i));
}
```
:::

But with the newly adopted `std::expected<T, E>`, it's not quite so nice:

::: std
```cpp
auto foo(int i) -> std::expected<int, E>;
auto bar(int i) -> std::expected<int, E>;

auto strcat(int i) -> std::expected<std::string, E>
{
    auto f = foo(i);
    if (not f) {
        return std::unexpected(f.error());
    }

    auto b = bar(i);
    if (not b) {
        return std::unexpected(b.error());
    }

    return std::format("{}{}", *f, *b);
}
```
:::

This is significantly longer and more tedious because we have to do manual error propagation. This manual error propagation is most of the code in this short example, and is bad not just because of the lengthy boilerplate, but also because:

* we're giving a name, `f`, to the `expected` object, not the success value. The error case is typically immediately handled, but the value case could be used multiple times and now has to be used as `*f` (which is pretty weird for something that is decidedly not a pointer or even, unlike iterators, a generalization of pointer) or `f.value()`
* the "nice" syntax for propagation - `return std::unexpected(e)` - is inefficient - if `E` is something more involved than `std::error_code`, we really should `std::move(f).error()` into that. And even then, we're moving the error twice when we optimally could move it just once. The ideal would be: `return {std::unexpect, std::move(f).error()};`, which is something I don't expect a lot of people to actually write.

In an effort to avoid... that... many libraries or code bases that use this sort of approach to error handling provide a macro, which either comes in a statement form or an expression form:

::: cmptable
### Statement Macro
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    SOMETHING_STMT(int f, foo(i));
    SOMETHING_STMT(int b, bar(i));
    return std::format("{}{}", f, b);
}
```

### Expression Macro
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    int f = SOMETHING_EXPR(foo(i));
    int b = SOMETHING_EXPR(bar(i));
    return std::format("{}{}", f, b);
}
```
:::

Not every library provides such, but here is a non-exhaustive list of those that do:

| Library / project | Statement form                                    | Expression form         |
| ----------------- | ------------------------------------------------- | ----------------------- |
| Boost.Outcome     | `BOOST_OUTCOME_TRY` / `BOOST_OUTCOME_TRYV`        | `BOOST_OUTCOME_TRYX`    |
| Boost.LEAF        | `BOOST_LEAF_AUTO` / `BOOST_LEAF_CHECK`            | `BOOST_LEAF_CHECK`      |
| Abseil            | `ABSL_ASSIGN_OR_RETURN` / `ABSL_RETURN_IF_ERROR`  | —                       |
| Apache Arrow      | `ARROW_ASSIGN_OR_RAISE` / `ARROW_RETURN_NOT_OK`   | —                       |
| TensorFlow/TSL    | `TF_ASSIGN_OR_RETURN` / `TF_RETURN_IF_ERROR`      | —                       |
| Mozilla Gecko     | —                                                 | `MOZ_TRY`               |
| SerenityOS        | —                                                 | `TRY`                   |

These macros certainly avoid some of these problems and allow the code to focus more on the logic than the specific control flow, though each such library type will have its own corresponding macro. Also these statement macros (the more common ones, since they don't rely on the statement-expression extension) need to be written on their own line, since they are declarations - thus the one-line version of `strcat` in the exception version isn't possible. The expression version allows you to write both macros inline, but isn't as efficient as it could be — and in particular it doesn't move when it should.

The statement macros suffer from when the function in question returns something like `expected<void, E>`, where there's no _value_ to be returned, so the macro needs to emit different code to handle this — hence the pairs of macros above.

To that end, in search for nice syntax, some people turn to coroutines:

::: std
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    int f = co_await foo(i);
    int b = co_await bar(i);
    co_return std::format("{}{}", f, b);

    // ... or
    co_return std::format("{}{}", co_await foo(i), co_await bar(i));
}
```
:::

This can be made to work in a fully-conformant way (at the syntactic cost of having to now write `co_return`), and we can use the same syntax for both the `void` and non-`void` cases.

However, currently even the simple cases allocate which make this approach unusuable in many production contexts. The coroutine machinery also isn't fully composable and runs into problems once you start doing something like `optional<expected<T, E>>` (or vice versa) or `task<optional<T>>`.

Which means the best-case today still involves ~~being jealous of exceptions~~ macros.

# An automatic propagation operator

Let's talk about Rust.

Rust's primary form of error handling is a sum type named `Result<T, E>`. Taking our original example here and rewriting it in Rust (as one does) would look like this:


::: cmptable
### Rust
```rust
fn strcat(i: i32) -> Result<String, E> {
    let f = match foo(i) {
        Ok(i) => i,
        Err(e) => return Err(e),
    };

    let b = match bar(i) {
        Ok(i) => i,
        Err(e) => return Err(e),
    }

    Ok(format!("{}{}", f, b))
}
```

### C++
```cpp
auto strcat(int i) -> std::expected<std::string, E> {
    auto f = foo(i);
    if (not f) {
        return std::unexpected(f.error());
    }

    auto b = bar(i);
    if (not b) {
        return std::unexpected(b.error());
    }

    return std::format("{}{}", *f, *b);
}
```
:::

This fully manual version is already better than the C++ version due to pattern matching's ability to just give a name to the thing we care about (the value) and avoid giving a name to the thing we don't care about (the `Result` object).

But this isn't the way you do things in Rust.

Originally, there was a [`try!` macro](https://doc.rust-lang.org/std/macro.try.html) which was defined mostly as that `match` expression I have above. But then this got generalized into `operator?`, whose behavior is driven by the [`Try` trait](https://doc.rust-lang.org/std/ops/trait.Try.html) (originally there was [try-v1](https://rust-lang.github.io/rfcs/1859-try-trait.html), now this is [try-v2](https://rust-lang.github.io/rfcs/3058-try-trait-v2.html)). That allows simply writing this:


::: cmptable
### Rust
```rust
fn strcat(i: i32) -> Result<String, E> {
    let f = foo(i)?;
    let b = bar(i)?;
    Ok(format!("{}{}", f, b))

    // ... or simply ...
    Ok(format!("{}{}", foo(i)?, bar(i)?))
}
```

### C++ with exceptions
```cpp
auto strcat(int i) -> std::string {
    int f = foo(i);
    int b = bar(i);
    return std::format("{}{}", f, b);

    // ... or simply ...
    return std::format("{}{}", foo(i), bar(i));
}
```
:::

Now, Rust still has manual error propagation, but it's the minimal possible syntactic overhead: one character per expression.

Importantly, one character per expression is still actually an enormous amount more overhead than zero characters per expression, since that implies that you cannot have error-neutral functions - they have to manually propagate errors too.

But to those people who write code using types like `std::expected` today, who may use the kinds of macros I showed earlier or foray into coroutines, this is kind of a dream?

## Semantics for C++

Let's talk about semantics first. I'm going to define the propagation operator in terms of two other features that don't actually exist in C++29 yet: `do` expressions ([@P2806R5]{.title}) and pattern matching ([@P2688R6]{.title}). Specifically, the expression (and I'll talk about [syntax later](#syntax-for-c)):

::: std
```cpp
expr.try?
```
:::

will lower to:

::: std
```cpp
match (expr) -> decltype(auto) {
    case auto&& $__e$ => do -> decltype(auto) {
        using $ETraits$ = std::try_traits<std::remove_cvref_t<decltype($__e$)>>;
        using $RTraits$ = std::try_traits<
			typename [: return_type_of(std::meta::current_function()) :]>;

		if (not $ETraits$::should_continue($__e$)) {
            return $RTraits$::from_break($ETraits$::extract_break(FWD($__e$)));
		}

		do_return $ETraits$::extract_continue(FWD($__e$));
    };
}
```
:::

### Introducing `std::try_traits`

The functionality here is driven by a new traits type called `std::try_traits`, such that a given specialization supports:

* telling us when the object is truthy: `should_continue`
* extracting the continue type (`extract_continue`) or break type (`extract_break`) from it
* constructing a new object from either the continuation type (`from_continue`, not necessary in the above example, but will demonstrate a use later) or the break type (`from_break`)

Note that this does not support deducing return type, since we need the return type in order to know how construct it - the above desugaring uses the return type of `std::expected<std::string, E>` to know how to re-wrap the potential error that `foo(i)` or `bar(i)` could return. This is important because it avoids the overhead that nicer syntax like `std::unexpected` or `outcome::failure` introduces (neither of which allow for deducing return type anyway, at least unless the function unconditionally fails), while still allowing nicer syntax.

This isn't really a huge loss, since in these contexts, you can't really deduce the return type anyway - since you'll have some error type and some value type. So this restriction isn't actually restrictive in practice.

These functions are all very easy to implement for the kinds of types that would want to support a facility like `try?`. Here are examples for `optional` and `expected` (with `constexpr` omitted to fit):

::: cmptable
```cpp
template <class T>
struct try_traits<optional<T>> {
  using continue_type = T;
  using break_type = nullopt_t;

  auto should_continue(optional<T> const& o) -> bool {
    return o.has_value();
  }

  // extractors
  auto extract_continue(auto&& o) -> auto&& {
    return *FWD(o);
  }
  auto extract_break(auto&&) -> error_type {
    return nullopt;
  }

  // factories
  auto from_continue(auto&& v) -> optional<T> {
    return optional<T>(in_place, FWD(v));
  }
  auto from_break(nullopt_t) -> optional<T> {
    return {};
  }
};
```

```cpp
template <class T, class E>
struct try_traits<expected<T, E>> {
  using continue_type = T;
  using break_type = E;

  auto should_continue(expected<T, E> const& e) -> bool {
    return e.has_value();
  }

  // extractors
  auto extract_continue(auto&& e) -> auto&& {
    return *FWD(e);
  }
  auto extract_break(auto&& e) -> auto&& {
    return FWD(e).error();
  }

  // factories
  auto from_continue(auto&& v) -> expected<T, E> {
    return expected<T, E>(in_place, FWD(v));
  }
  auto from_break(auto&& e) -> expected<T, E> {
    return expected<T, E>(unexpect, FWD(e));
  }
};
```
:::

### What's with the pattern match?

I'm proposing that `expr.try?` lower into something that starts with `match (expr) { case auto&& $__e$ => /* ... */ }`. Why that pattern match wrapper? After all, it doesn't actually seem like we need pattern matching, since we're only using one pattern that matches everything?

The motivation here is to handle temporaries properly. We need any temporaries that exist as part of `expr` to last until the end of the full-expression containing `expr` — which is the current C++ rule and what everybody would expect the behavior to be. That `match` formulation allows `expr` to be evaluated _outside of_ the `do` expression, and thus have temporaries handled properly simply as a consequence of the existing language rules.

You can see more in the [lifetime section](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p2806r5.html#lifetime) of the `do` expression paper.

### Difference in Semantics vs Rust

In Rust's [try-v2](https://rust-lang.github.io/rfcs/3058-try-trait-v2.html) model, the semantics are slightly different. Using the same `$ETraits$` and `$RTraits$` definitions from above (the `try_traits` specialization for our current expression and the return type, respectively), the semantic difference is:

::: cmptable
### Proposed
```cpp
match (expr) -> decltype(auto) {
    case auto&& $__e$ => do -> decltype(auto) {
		if (not $ETraits$::should_continue($__e$)) {
            return $RTraits$::from_break(
                $ETraits$::extract_break(FWD($__e$)));
		}

		do_return $ETraits$::extract_continue(FWD($__e$));
    };
}
```

### Rust
```cpp
match ($ETraits$::branch(expr)) -> decltype(auto) {
    case { .Continue: auto&& $v$ }
        => FWD($v$);
    case { .Break: auto&& $r$ }
        => return $RTraits$::from_residual(FWD($r$));
}
```
:::

In Rust, `Try::branch(expr)` takes an expression and turns it into some [`ControlFlow`](https://doc.rust-lang.org/beta/std/ops/enum.ControlFlow.html), which is another Rust `enum` (which we'd think of as a language variant in C++ terms). In a sense, this type erases our input (that we don't know anything about) into a variant whose shape we understand. `ControlFlow` has two alternatives named `Continue` and `Break`, and we handle those in the obvious way (`Continue` gives us a value, `Break` means we return from the function).

There's good reason for the Rust shape to look like this, since if you have language variants, then obviously you will design your language features around using them. But C++ doesn't have language variants — so producing that intermediate `ControlFlow` object is both unergonomic for us and also inefficient, since it requires doing more work (to be hopefully optimized out later). The shape I'm proposing is conceptually identical, except just cuts out the middle man.

Another difference is something that Rust does not (and does not need to) support, but my shape does: allowing for convertibility between different library expected-like types. In Rust, `Result` has always existed, everyone uses `Result`. But in C++, this model is fairly new (on the timeline of C++) and there is a proliferation of types in different libraries that behave like this. Being able to use multiple libraries together is pretty valuable, hence wanting a shape that allows this kind of cross conversion:

::: std
```cpp
auto foo(int i) -> tl::expected<int, E>;
auto bar(int i) -> std::expected<int, E>;

auto strcat(int i) -> Result<string, E>
{
    // this works
    return std::format("{}{}", foo(i).try?, bar(i).try?);
}
```
:::

Note that in Rust the "residual" of `Result<T, E>` isn't `E`, it's `Result<!, E>`. That's important for some Rust-specific reasons that don't really apply to us either.

### Handling `void`

The one aspect in the lowering earlier that isn't quite right is handling `void`. Now, if the `continue` type is `void`, this works fine, the whole expression will just have type `void` because of special rules we have in the language to just make this work. But if the `break` type is `void`, that's not going to work, because we don't have regular `void`. Now, `std::optional<void>` and `std::expected<T, void>` aren't supported in the standard library — but they are supported by some third party libraries (including mine), so I would like it to be able to work as well.

This is straightforward to express in the lowering, we simply replace this part:

::: std
```cpp
return $RTraits$::from_break($ETraits$::extract_break(FWD($__e$)));
```
:::

With this (which is honestly peak C++):

::: std
```cpp
template for (auto _ : "") {
    if constexpr (std::is_void_v<typename $ETraits$::break_type>) {
        $ETraits$::extract_break(FWD($__e$));
        return $RTraits$::from_break();
    } else {
        return $RTraits$::from_break($ETraits$::extract_break(FWD($__e$)));
    }
}
```
:::

The expansion statement here is the C++26 approach to explicit implicit template regions ([@P3525R0]), which we need because we need `if constexpr` to actually discard the non-instantiated branch. Once we have a template region, we just do the right thing based on `void` — which is just calling `extract_break` and `from_break` separately, the latter with no arguments.

### Unevaluated Contexts

The two interesting unevaluated contexts are: what is `decltype(expr.try?)` and what happens if you write `expr.try?` in a `$requires-expression$`. With the lowering I provided above, those would be valid within the context of a function in which you could write the expression `expr.try?`, otherwise they would be invalid (due to the `return` present in the `do` expression). There are two options I think for how to handle this operator in such contexts:

1. Exactly as would fall out from the `do` expression lowering: sometimes valid, sometimes not. This is arguably unsurprising, since we're defining this expression to mean this other expression, so you get the behavior of that other expression. Seems justifiable.
2. Treat `expr.try?` in an evaluated context as basically meaning `std::try_traits<std::remove_cvref_t<decltype(expr)>>::extract_continue(expr)`. This would actually be the value of the expression you would get, so it at least makes some sense.

We do have precedent for other operators not being easily used in unevaluated contexts (`co_await`, `co_yield`), so this wouldn't be the first one. It's unclear to me yet whether it is actually useful to make this work. I think, though, that shipping (1) now doesn't necessarily prevent changing to (2) later, as it would be largely a matter of taking ill-formed expressions and making them well-formed with their desired meaning.

### Lifetime and Value Category

Consider this fragment:

::: std
```cpp
auto f() -> expected<string, E>;

auto g() -> expected<string, E> {
    decltype(auto) v1 = f().try?;

    auto r = f();
    decltype(auto) v2 = r.try?;

    // ...
}
```
:::

What should the types of `v1` and `v2` be? I think `v2` basically has to be `string&` — `r` is an lvalue, we don't want this to have to be a copy. But what about `v1`? There, we have two options:

1. We just follow the forwarding principle, and this would have whatever type `*f()` / `f().value()` have — which is `string&&`. In this particular case, that means we would have a dangling reference, since the temporary `expected` object that we are referring into would get destroyed at the end of the statement.
2. We could ensure as a language rule that we always decay xvalues. That is, instead of `do_return expr;` we do `do_return static_cast<remove_rvalue_reference_t<decltype((expr))>>(expr);`

Note that even if the language always just returns what `extract_continue` returns, users can still choose `extract_continue` to never return an xvalue.

The advantage of never returning an xvalue is that we avoid a common cause of dangling references. That seems like a good thing. The disadvantage of forcing materialization is that we force a completely unnecessary move, if you're directly passing the result of `expr.try?` into a function. Which to choose? I think, if the dangling reference can be reliably diagnosed, then avoiding it in the language seems like pure cost. And initial experimentation with this feature [implemented as a macro](https://compiler-explorer.com/z/554YMqaaW) suggests that it _can_ be reliably diagnosed:

::: std
```cpp
enum class E { };
template <class T>
auto get_data() -> std::expected<T, E>;

auto f1() -> std::expected<int, E> {
    auto&& data = TRY(get_data<int>()); // <== warning about dangling reference on this line
    return data;
}

auto consume(int x) -> int { return x; }

auto f2() -> std::expected<int, E> {
    auto&& data = consume(TRY(get_data<int>())); // <== no warning here
    return data;
}
```
:::

As a result, I think the formulation I showed is the way to go.

## Syntax for C++

Now that we've established the semantics, let's talk about the syntax. Unfortunately, C++ cannot simply grab the Rust syntax of a postfix `?` here, because we also have the conditional operator `?:`, with which it can be ambiguous:

::: std
```cpp
auto res = a ? * b ? * c : d;
```
:::

That could be parsed two ways:

::: std
```cpp
auto res1 = a ? (*(b?) * c) : d;
auto res2 = ((a?) * b) ? (*c) : d;
```
:::

What if you assume that a `?` is a conditional operator and try to parse that until it fails, then back up and try again to parse a postfix `?` operator? Is that really a viable strategy? If we assume both `?`s are the beginning of a conditional, then that will eventually fail since we hit a `;` before a second `:` - but it's the outer `?` that failed, not the inner - do we retry the inner first (which would lead to the `res1` parse eventually) or the outer first (which would lead to the `res2` one)?

Maybe this is doable with parsing heroics, but at some point I have to ask if it's worth it.

Another reason that a single `?` might not be a good idea, even if it were possible to parse, would be [optional chaining](#error-continuations). With that facility, if `o` were an `optional<string>`, `o?.size()` would be an `optional<size_t>` (that is either engaged with the original string's size, or empty). But if `o?` propagated the error, then `o?.size()` would itself be a valid expression that is a `size_t` (the string's size, and if we didn't have a string we would have returned). So if we want to support error continuations, we'd need distinct syntax for these cases.

So if `expr?` is not viable, what can we choose instead? There's a bunch of things to consider.

### Postfix is better than Prefix

For the purposes of this section, let's stick with `?` as the token, and talk about whether this should be a prefix operator or postfix operator. Now, `?` would be viable as a prefix operator whereas it's not viable as a postfix operator, but it's still worth going through an example nevertheless:

::: std
```cpp
struct U { ... };

struct T {
    auto next() -> std::expected<U, E>;
};

auto lookup() -> std::expected<T, E>;

auto func() -> std::expected<U, E> {
    // as postfix
    U u = lookup()?.next()?;

    // using the monadic operations
    U u = lookup().and_then(&T::next);

    // as prefix
    U u = ?(?lookup()).next();

    do_something_with(u);

    return u;
}
```
:::

The postfix version chains in a way that is quite easy to read.

Using the monadic operations ([@P2505R4]) is fine, they're nice in this case (which is basically optimal for them) but they tend to be quite tedious once you stray from this exact formulation (e.g. if `T::next()` took another argument).

The prefix version is borderline illegible to me once the expression you need to propagate is even slightly complicated.

Even if we consider only one or the other side of the member access as needing propagation:

* Accessing a member after propagating: `x?.y` vs `(?x).y`
* Propagating after accessing a member: `x.y?` vs `?x.y` or `?(x.y)`

The postfix operator is quite a bit easier to understand, since it's always right next to the expression that is potentially failing.

### Postfix `??`

While postfix `?` isn't viable, postfix `??` could be. And a previous revision of this paper proposed just that [@P2561R1]. However, while `?` has precedent in Rust for exactly the operation being proposed here, `??` has precedent in other languages as well - just for something quite different.

`??` is called a "null (or nil) coalescing operator" in some languages (like C# or JavaScript or Swift) where `x ?? y` is roughly equivalent to what C++ would spell as `x ? *x : y` except that `x` is only evaluated once. Kotlin spells this operator `?:`, but it behaves differently from the gcc extension since `x ?: y` in gcc evaluates as `x ? x : y` rather than `x ? *x : y`.

For `x` being some kind of `std::optional<T>` or `std::expected<T, E>`, this can *mostly* already be spelled `x.value_or(y)`. The difference is that here `y` is unconditionally evaluated, which is why [@P2218R0] proposes a separate `opt.value_or_else(f)` which invokes `f`. Which would make a proper equivalence be spelled `x.value_or_else([&]{ return y; })`.

I'm not aware of any proposals to add this particular operator in C++, but because we already have two types that directly provide that functionality (as would many other non-`std` flavors thereof), and because it's fairly straightforward to write such an algorithm generically, it wouldn't seem especially valuable to have a dedicated operator for this functionality -- so it's probably safe to take for this use-case.

It certainly would be nice to have both, but given a choice between a null coalescing operator and a control flow propagation one, I'd choose the latter. That said, given that `??` does appear in many languages as this one particular thing, even if I don't personally consider that particular thing useful in C++, I don't think it's a good idea to take that operator in C++ to mean something very different.

### Why `e.try?`

There is quite a bit of existing practice for this facility under this name. Several C++ macros as shown above use `TRY` as the name. [@P0779R0]{.title} previously suggested this sort of facility under the name `operator try`. As mentioned, Rust previously had an error propagation macro named `try!` and multiple other languages have such an error propagation operator ([Zig](https://ziglang.org/documentation/master/#try), [Swift](https://docs.swift.org/swift-book/LanguageGuide/ErrorHandling.html), [Midori](http://joeduffyblog.com/2016/02/07/the-error-model/), etc.).

The problem is, in C++, `try` is strongly associated with _exceptions_. That's what a `try` block is for: to catch exceptions. In [@P0709R4], there was a proposal for a `try` expression (in §4.5.1). That, too, was tied in with exceptions. Not only for us is it tied into exceptions, but it's used to _not_ propagate the exception - `try` blocks are for handling errors.

Having a facility for error propagation in C++ which has nothing to do with exceptions still use the keyword `try`  and do the opposite of a what a `try` block does today (i.e. propagate the error, instead of handling it) would be, I think, potentially misleading. And the goal here isn't to interact with exceptions at all - it's simply to provide automated error propagation for those error handling cases that _don't_ use exceptions.

That said, postfix `.try?` is viable syntax (it's ill-formed today) and would be better than prefix `try e` or `try? e` (as discussed [earlier](#postfix-is-better-than-prefix)), and despite being unrelated to exceptions, it is quite commonly used in practice for this functionality in C++ anyway. It seems like a pretty reasonable choice, all things considered.

Plus, we need a name for the traits for all the functionality to implement, and `std::try_traits` mirrors `expr.try?` really nicely.

### Other potential syntaxes considered

Here is a list of other potential syntaxes I've considered:

|Syntax|Notes|
|-|----|
|`?e`|Don't like the prefix|
|`try e` or `try? e`|Don't like the prefix|
|`e???`|`???` was the trigraph for `?`, and looks ridiculous, but at least doesn't conflict with other languages' `??`|
|`e!`|Viable, but seems like the wrong punctuation for something that may or may not continue|
|`e.continue?`|Viable, and not completely terrible, but doesn't seem as nice as `e.try?`|
|`e.or_return?`|Clearly expresses behavior, but seems strictly worse than using `try?` or `continue?`|
|`e!?`|Lauri Vasama's suggestion and what he implemented. The benefit is being a very terse syntax, even shorter than `!?`. The main downside for me is that because it's just two characters, you have to remember what order they go in, and `e?!` could be the beginning of a conditional expression.|
|`e.?`|Also a terse syntax, with the added benefit of the ordering of the two characters being arguably more obvious. Would heavily clash if we ever wanted to pursue [optional chaining](#potential-directions-to-go-from-here).|

The other problem with the punctuation syntaxes is needing a name for traits. If we don't like `e.try?` because of the keyword `try`, then we probably don't like `try_traits` either. That particular name doesn't actually have to be super terse, so we could always go for the longer `continuation_traits` or something.

## Other use-cases

While the bulk of this paper up to this point is focused on the specific use case of propagating errors, there are several other uses for this kind of operator, which is part of why calling it an _error_-propagation operator specifically is not a good name.

### Short-circuiting fold

One of the algorithms considered in the `ranges::fold` paper ([@P2322R5]) was a short-circuiting fold. That paper ultimately didn't propose such an algorithm, since there isn't really a good way to generically write such a thing. Probably the best option in the paper was to have a mutating accumulation function that returns `bool` on failure?

But with this facility, there is a clear direction for how to write a generic, short-circuiting fold:

::: std
```cpp
template <typename T>
concept Try = requires (T t) {
    typename try_traits<T>::continue_type;
    typename try_traits<T>::break_type;

    { try_traits<T>::should_continue(t) } -> $boolean-testable$;
    // etc. ...
};

template <input_iterator I,
          sentinel_for<I> S,
          class T,
          invocable<T, iter_reference_t<R>> F,
          Try Return = invoke_result_t<F&, T, iter_reference_t<R>>
    requires same_as<
        typename try_traits<Return>::continue_type,
        T>
constexpr auto try_fold(I first, S last, T init, F accum) -> Ret
{
    for (; first != last; ++first) {
        init = std::invoke(accum,
            std::move(init),
            *first).try?;
    }

    return init;
}
```
:::

This `try_fold` can be used with an accumulation function that returns `optional<T>` or `expected<T, E>` or `boost::outcome::result<T>` or ... Any type that opts into being a `Try` will work.

Note that this may not be exactly the way we'd specify this algorithm, since we probably want to return something like a `pair<I, Ret>` instead, so the body wouldn't be able to use `.try?` and would have to go through `try_traits` manually for the error propogation. But that's still okay, since the important part was being able to have a generic algorithm to begin with.

### Range of `expected` to `expected` of Range

There's an algorithm in Haskell called `sequence` which takes a `t (m a)` and yields a `m (t a)`. In C++ terms, that might be an algorithm that takes a range of `expected<T, E>` and yields a `expected<vector<T>, E>` - which contains either all the results or the first error.

With the same `Try` concept from a above, this can be generalized to also work for `optional<T>` or any number of other `Result`-like types:

::: std
```cpp
template <ranges::input_range R,
          Try T = remove_cvref_t<ranges::range_reference_t<R>>,
          typename Traits = try_traits<T>,
          typename Result = Traits::rebind<vector<typename Traits::value_type>>>
auto sequence(R&& r) -> Result
{
    vector<typename Traits::value_type> results;
    for (auto it = ranges::begin(r); it != ranges::end(r); ++it) {
        results.push_back((*it).try?);
    }
    return results;
}
```
:::

This would require some way of doing rebinding the type — from `RangeOf<expected<T, E>>` to `expected<vector<T>, E>`. Just assuming that first template parameter is the value type probably gets you a lot of the way there.

### Internal iteration

With internal iteration, using a sink function that gets pushed values (instead of having an iterator that pulls values), there is a need for the sink to indicate when to stop receiving values. That's not an error, per se, that's just a signal to break. Having a control flow propagation operator makes such generators much more convenient to write:

::: cmptable
### Current
```cpp
struct generator123 {
    auto operator()(auto sink) const {
        std::control_flow flow = sink(1);
        if (!flow) return flow;

        flow = sink(2);
        if (!flow) return flow;

        return sink(3);
    }
};
```
### Proposed
```cpp
struct generator123 {
    auto operator()(auto sink) const -> control_flow {
        sink(1).try?;


        sink(2).try?;


        return sink(3);
    }
};
```

---

```cpp
template <range R, class Pred>
auto filter(R&& r, Pred pred)
{
    return [pred, &r](auto sink){
        for (auto&& elem : r) {
            if (pred(elem)) {
                auto result = sink(FWD(elem));
                if (result == break_) {
                    return break_;
                }
            }
        }
        return continue_;
    };
}
```

```cpp
template <range R, class Pred>
auto filter(R&& r, Pred pred)
{
    return [pred, &r](auto sink) -> control_flow {
        for (auto&& elem : r) {
            if (pred(elem)) {
                sink(FWD(elem)).try?;



            }
        }
        return continue_;
    };
}
```
:::

See [@P2881R0] for more information.

### Naming

Because we don't have a proper language customization mechanism, we need to have two distinct things:

* a class template that contains all the functionality (which I'm naming here `std::try_traits`)
* a `concept` that checks if this class template is (a) specialized (b) correctly (which I'm naming here... `Try`)

I think it's unfortunate that we need two different names for this, but that's the way of things at the moment. Also I have no idea what a good name for this `concept` is. Rust calls this `Try`, but we want our concepts to be `snake_case`, and `try` is not an option. I'm open to suggestion.

## Potential directions to go from here

This paper is proposing just `.try?` and the machinery necessary to make that work (including a `concept`, opt-ins for `optional` and `expected`, but not the short-circuiting fold algorithm).

However, it's worth it for completeness to point out a few other directions that such an operator can take us.

### Error continuations

Several languages have a facility that allows for continuing to invoke member functions on optional values. This facility is called something different in every language (optional chaining in Swift, null-conditional operator in C#, safe call operator in Kotlin), but somehow it's all spelled the same and does the same thing anyway.

Given a `std::optional<std::string>` named `opt`, what that operator -- spelled `?.` -- means is approximately:

|expression|C++ equivalent|
|-|---|
|`opt?.size()`|`opt.transform(&std::string::size) // technically UB`|
|`opt?.substr(from, to)`|`opt.transform([&](auto& s){ return s.substr(from, to); })`|

Like the null coalescing meaning of `??` [described above](#in-other-languages), the semantics of `opt?.f()` can be achieved using library facilities today. The expression `E1?.E2`, if `E1` is an `optional`, basically means `E1.transform([&](auto&& e){ return FWD(e).E2; })`

Quite unlike `??`, there is a significant drop in readability and just the general nice-ness of the syntax.

The `try_traits` facility very nearly gives us the tools necessary to support such a continuation operator. Since what we need to do is:

* check is `E1` is truthy or falsey (`Traits::should_continue(E1)`)
* extract the value of `E1` in order to perform the subsequent operation (`Traits::extract_continue(E1).E2`)
* extract the error of `E1` in order to return early (`Traits::extract_break(E2)`)

We mostly need one more customization point: to put the types back together. What I mean is, consider:

::: std
```cpp
auto f(int) -> std::expected<std::string, E>;

auto x = f(42)?.size();
```
:::

The type of `x` needs to be `std::expected<size_t, E>`, since that's what the value case ends up being here. If we call that customization point `rebind`, as in:

::: std
```cpp
template <typename T, typename E>
struct try_traits<expected<T, E>> {
    // ... rest as before ...

    template <class U>
    using rebind = expected<remove_cvref_t<U>, E>;
};
```
:::

Then the above can be desugared into:

::: std
```cpp
using $_Traits$ = try_traits<remove_cvref_t<decltype(f(42))>>;
using $_R$ = $_Traits$::rebind<decltype($_Traits$::extract_continue(f(42)).size())>;

auto&& $e$ = f(42);
auto x = $_Traits$::should_continue($e$)
       ? try_traits<$_R$>::from_continue($_Traits$::extract_continue(FWD($e$)).size())
       : try_traits<$_R$>::from_break($_Traits$::extract_break(FWD($e$)));
```
:::

That may seem like a mouthful. Because it is a mouthful. But it's a mouthful that the user doesn't have to write any part of, they just put `f(42)?.size()` and this does do the right thing.

At least, this mostly does the right thing. We still have to talk about copy elision. Consider this version:

::: std
```cpp
struct X {
    auto f() -> std::mutex;
};

auto g() -> Result<X, E>;
auto n = g()?.f();
```
:::

Presumably, `n` is a `Result<std::mutex, E>`, but in order for this to work, we can't just evaluate this as something like `Result<std::mutex, E>(g().value().f())`. `std::mutex` isn't movable.

The only way for this to work today is be able to pass a callable all the way through into this `Result`'s constructor. Which is to say, we desugar like so:

::: std
```cpp
auto&& $e$ = g();
auto n = $_Traits$::should_continue($e$)
       ? try_traits<$_R$>::from_continue_func([&]() -> decltype(auto) {
            return $_Traits$::extract_continue(FWD($e$)).f()
         })
       : try_traits<$_R$>::from_break($_Traits$::extract_break(FWD($e$)));
```
:::

By default, `try_traits<R>::from_continue_func(f)` would just be `try_traits<R>::from_continue(f())`. This is weird, but it's something to think about. Note also error continuation would only help in the member function or member variable cases. If we want to continue into a non-member function, you'd need the sort of `.transform()` member function anyway.

Note that Rust has an error propagation operator (`?`) but does not support optional chaining. And indeed, since we have `optional<T>::transform`, it's worth asking if all of this complexity is worth it for such a small syntactic benefit. I don't think it is.

### Not propagating errors

The `.try?` approach seems to work quite well at propagating errors: it's syntactically cheap, performant, and allows for integrating multiple libraries.

But what if we didn't want to propagate the error, but rather do something else with it? For `std::optional` and `std::expected`, we already have a UB-if-error accessor in the form of `*x` and a throw-if-error accessor in the form of `x.value()`. It seems like the corollary to an error-propagating `x.try?` would be some sort of `x.try!` that somehow forces the error differently.

While propagating the error only really has one way to go (you return it), there are quite a few different things you can do differently:

* `assert` that `should_continue()`
* `abort()` if `not should_continue()`
* `terminate()` if `not should_continue()`
* `unreachable()` (or `[[assume]]`) if `not should_continue()`
* `throw extract_break()` if `not should_continue()`
* `throw f(extract_break())` if `not should_continue()` for some `f`
* log an error and come up with some default value if `not should_continue()`

That's a lot of different options, and the right one likely depends on context too.

An additional template parameter on the error type could drive what `x.try!` does (as Boost.Outcome does, for instance), which would allow you to preserve the nice syntax if a particular error handling strategy is sufficiently common (maybe you always `throw`, so why would you want to write extra syntax for this case), but at a cost of suddenly having way more types. Although the `try_traits` approach does at least allow those "way more types" to interact well.

This behavior can be achieved by adding a new function to `try_traits` which desugars as follows:

::: cmptable
```cpp
auto val = expr.try!;
```

```cpp
auto&& $__val$ = expr;
using $_Traits$ = std::try_traits<
  std::remove_cvref_t<decltype($__val$)>>;
if (not $_Traits$::should_continue($__val$)) {
  $_Traits$::fail(FWD($__val$));
}
auto val = $_Traits$::extract_continue(FWD($__val$));
```
:::

But this doesn't seem as valuable as `.try?` or even `e?.x` since this case is easy to add as a member function. Indeed, that's what `x.value()` and `*x` do for `optional` and `expected` (throw and undefined behavior, respectively).

Moreover, any of the kinds of behavior you want can be written as a free function:

::: std
```cpp
template <class T, Try U = std::remove_cvref_t<T>>
auto narrow_value(T&& t) -> decltype(auto) {
    assert(std::try_traits<U>::should_continue(t));
    return std::try_traits<U>::extract_continue(FWD(t));
}

template <class T, Try U = std::remove_cvref_t<T>>
auto wide_value(T&& t) -> decltype(auto) {
    if (not std::try_traits<U>::should_continue(t)) {
        [[unlikely]] throw std::try_traits<U>::extact_error(FWD(t));
    }
    return std::try_traits<U>::extract_continue(FWD(t));
}

// etc.
```
:::

Which further demonstrates the utility of the proposed facility.

## Implementation Experience

There are two kinds of implementation experience I can point to with this facility:

1. Lauri Vasama implemented this paper directly in his fork of clang, using the syntax `expr!?` for the operator. His implementation can be found on compiler explorer [here](https://godbolt.org/z/8EMoGEEcK).
2. I implemented `do` expressions in my fork of clang, which Michael Park cherry-picked and implemented pattern matching in his fork of clang. While I don't have an implementation of `expr.try?`, I can implement this proposal as a C macro. Our implementation can be found on compiler explorer [here](https://compiler-explorer.com/z/554YMqaaW).

---
references:
  - id: P2688R6
    citation-label: P2688R6
    title: "Pattern Matching: `match` expression"
    author:
      - family: Michael Park
    issued:
      date-parts:
      - - 2026
        - 09
        - 22
    URL: https://wg21.link/p2688r6
---
