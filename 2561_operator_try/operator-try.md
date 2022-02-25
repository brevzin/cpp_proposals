---
title: "`operator?`"
document: P2561R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Preface

It is important to clarify a few things up front. It is not the position of this paper that exceptions are bad. Or that exceptions are good. It is not the goal of this paper to convince you to start using exceptions, nor is it to convince you to stop using exceptions.

This paper simply recognizes that there are many code bases (or parts thereof) that do not use exceptions and probably will not in the future.

# Introduction

Let's start with a fairly small example of a series of functions that can generate errors, but don't themselves handle them - they just need to propagate them up. With exceptions, this might look like:

::: bq
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

There's a lot to like about exceptions. One nice advantage is the zero syntactic overhead necessary for propagating errors. Errors just propagate. You don't even have to know which functions can fail. Indeed, the above could even have been:

::: bq
```cpp
auto foo(int i) -> int; // might throw an E
auto bar(int i) -> int; // might throw an E

auto strcat(int i) -> std::string {
    return std::format("{}{}", foo(i), bar(i));
}
```
:::

But with the newly adopted `std::expected` ([@P0323R12]), it's not quite so nice:

::: bq
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

* we're giving a name, `f`, to the `expected` object, not the success value. The error case is typically immediately handled, but the value case could be used multiple times and now has to be used as `*f`
* the "nice" syntax for propagation is inefficient - if `E` is something more involved than `std::error_code`, we really should `std::move(f).error()` into that. And even then, we're moving the error twice when we optimally could move it just once.

In an effort to avoid... that... many libraries or code bases that use this sort approach to error handling provide a macro, which usually looks like this ([Boost.LEAF](https://www.boost.org/doc/libs/1_75_0/libs/leaf/doc/html/index.html#BOOST_LEAF_ASSIGN), [Boost.Outcome](https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/reference/macros/try.html), [mediapipe](https://github.com/google/mediapipe/blob/master/mediapipe/framework/deps/status_macros.h), etc. Although not all do, neither `folly`'s `fb::Expected` nor `tl::expected` provide such):

::: bq
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    SOMETHING_TRY(int f, foo(i));
    SOMETHING_TRY(int b, bar(i));
    return std::format("{}{}", f, b);
}
```
:::

Which avoids all those problems, though each such library type will have its own corresponding macro. Also these `TRY` macros (not all of them have `TRY` in the name) need to be written on their own line, since they are declarations - thus the one-line version of `strcat` in the exception version isn't possible.

Some more adventurous macros take advantage of the statement-expression extension, which would allow you to do this:

::: bq
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    int f = SOMETHING_TRY_EXPR(foo(i));
    int b = SOMETHING_TRY_EXPR(bar(i));
    return std::format("{}{}", f, b);
}
```
:::

And thus also write both macros inline. But this relies on compiler extensions, and this particular extension isn't quite as efficient as it could be - and in particular it doesn't move when it should.

Both macros also suffer when the function in question returns `expected<void, E>`, since you cannot declare (or assign to) a variable to hold that value, so the macro needs to emit different code to handle this case ([Boost.LEAF](https://www.boost.org/doc/libs/1_75_0/libs/leaf/doc/html/index.html#BOOST_LEAF_CHECK), [Boost.Outcome](https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/reference/macros/tryv.html), etc.)

To that end, in search for nice syntax, some people turn to coroutines:

::: bq
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

Originally, there was a [`try!` macro](https://doc.rust-lang.org/std/macro.try.html) which was defined mostly as that `match` expression I have above. But then this got generalized into `operator?`, whose behavior is driven by the [`Try` trait](https://doc.rust-lang.org/std/ops/trait.Try.html). That allows simply writing this:


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

## Syntax for C++

Before diving too much into semantics, let's just start by syntax. Unfortunately, C++ cannot simply grab the Rust syntax of a postfix `?` here, because we also have the conditional operator `?:`, with which it can be ambiguous:

::: bq
```cpp
auto res = a ? * b ? * c : d;
```
:::

That could be parsed two ways:

::: bq
```cpp
auto res1 = a ? (*(b?) * c) : d;
auto res2 = ((a?) * b) ? (*c) : d;
```
:::

What if you assume that a `?` is a conditional operator and try to parse that until it fails, then back up and try again to parse a postfix `?` operator? Is that really a viable strategy? If we assume both `?`s are the beginning of a conditional, then that will eventually fail since we hit a `;` before a second `:` - but it's the outer `?` that failed, not the inner - do we retry the inner first (which would lead to the `res1` parse eventually) or the outer first (which would lead to the `res2` one)?

Maybe this is doable with parsing heroics, but at some point I have to ask if it's worth it. Especially since this isn't the only possible syntax for this.

Two other options worth considering are: `??` and `.?` Both are easily unambiguous by virtue of not even being valid token sequences today.

### `??`

`??` is called a "null (or nil) coalescing operator" in some languages (like C# or JavaScript or Swift) where `x ?? y` is roughly equivalent to what C++ would spell as `x ? *x : y` except that `x` is only evaluated once. Kotlin spells this operator `?:`, but it behaves differently from the gcc extension since `x ?: y` in gcc evaluates as `x ? x : y` rather than `x ? *x : y`.

For `x` being some kind of `std::optional<T>` or `std::expected<T, E>`, this can *mostly* already be spelled `x.value_or(y)`. The difference is that here `y` is unconditionally evaluated, which is why [@P2218R0] proposes a separate `opt.value_or_else(f)` which invokes `f`. Which would make a proper equivalence be spelled `x.value_or_else([&]{ return y; })`.

I'm not aware of any proposals to add this particular operator in C++, but because we already have two types that directly provide that functionality (as would many other non-`std` flavors thereof), and because it's fairly straightforward to write such an algorithm generically, it wouldn't seem especially valuable to have a dedicated operator for this functionality -- so it's probably safe to take for this use-case.

### `.?`

I'm not aware of a language that uses `.?` specifically, but it is worth commenting on the reverse: `?.` This one is called optional chaining in Swift, the null-conditional operator in C#, and the safe call operator in Kotlin.

Given a `std::optional<std::string>` object named `opt`, this means:

|expression|C++ equivalent|
|-|---|
|`opt?.size()`|`opt.transform(&std::string::size) // technically UB`|
|`opt?.substr(from, to)`|`opt.transform([&](auto& s){ return s.substr(from, to); })`|

Like `??` described above, the semantics of `opt?.f()` can be achieved using library facilities today. Quite unlike `??`, there is a significant drop in readability and just the general nice-ness of the syntax.

With `value_or` and especially `value_or_else`, the library syntax is fine and doesn't strike me as sufficiently bad to merit dedicated language help - but that is decidedly not the case here. Note that `transform` is still quite useful - if you're trying to call a free function, `opt.transform(f)` is great. It's only when you're trying to call a member that it's quite bad. Kotlin's approach in particular allows you to use `?.` for both cases quite well, since it's either `opt?.member` or `opt?.let { free(it) }`.

I'm not aware of any proposal to add this feature into C++ either, but between the two of them, this one at least seems like it could merit dedicated language help. Even though the spelling of that operator would be `?.` rather than `.?`, that seems a bit too close for comfort to have two different operators spelled nearly the same with different meanings entirely.

### Comparison of the two

Just putting our example back up, using these syntaxes (though still not describing semantics), they look basically the same (as you might expect from two different two-token sequences, each of which uses `?` as the second token):

::: cmptable
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    int f = foo(i)??;
    int b = bar(i)??;
    return std::format("{}{}", f, b);

    // ... or simply ...
    return std::format("{}{}", foo(i)??, bar(i)??);
}
```

```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    int f = foo(i).?;
    int b = bar(i).?;
    return std::format("{}{}", f, b);

    // ... or simply ...
    return std::format("{}{}", foo(i).?, bar(i).?);
}
```
:::

It's easy to say that both look pretty great compared to today's alternatives: they are far terser, don't require either macros or coroutines to achieve nice syntax, and don't lose anything on performance by providing nice syntax.

If the parsing heroics are acceptable, then `?` is a better spelling than either `??` or `.?`. If the parsing heroics are _not_ acceptable, then it seems like `??` is the better choice of the two.

Of course, now we have to talk about semantics.

## Semantics

This paper suggests that `??` evaluate as follows:

::: cmptable
```cpp
auto strcat(int i) -> std::expected<std::string, E>
{


    int f = foo(i)??;









    int b = bar(i)??;








    return std::format("{}{}", f, b);
}
```

```cpp
auto strcat(int i) -> std::expected<std::string, E>
{
    using $_Return$ = std::try_traits<
        std::expected<std::string, E>>;

    auto&& $__f$ = foo(i);
    using $_TraitsF$ = std::try_traits<
        std::remove_cvref_t<decltype($__f$)>>;
    if (not $_TraitsF$::is_ok($__f$)) {
        return $_Return$::from_error(
            $_TraitsF$::extract_error(FWD($__f$)));
    }
    int f = $_TraitsF$::extract_value(FWD($__f$));

    auto&& $__b$ = bar(i);
    using $_TraitsB$ = std::try_traits<
        std::remove_cvref_t<decltype($__b$)>>;
    if (not $_TraitsB$::is_ok(__b)) {
        return $_Return$::from_error(
            $_TraitsB$::extract_error(FWD($__b$)));
    }
    int b = $_TraitsB$::extract_value(FWD($__b$));

    return std::format("{}{}", f, b);
}
```
:::

The functionality here is driven by a new traits type called `std::try_traits`, such that a given specialization supports:

* telling us when the object is truthy: `is_ok`
* extracting a value (`extract_value`) or error (`extract_error`) from it
* constructing a new object from either a value (`from_value`, not necessary above) or an error (`from_error`)

Note that this does not support deducing return type, since we need the return type in order to know how construct it - the above desugaring uses the return type of `std::expected<std::string, E>` to know how to re-wrap the potential error that `foo(i)` or `bar(i)` could return. This is important because it avoids the overhead that nicer syntax like `std::unexpected` or `outcome::failure` introduces (neither of which allow for deducing return type anyway, at least unless the function unconditionally fails), while still allowing nicer syntax.

These functions are all very easy to implement for the kinds of types that would want to support a facility like `??`. Here are examples for `optional` and `expected` (with `constexpr` omitted to fit):

::: cmptable
```cpp
template <class T>
struct try_traits<optional<T>> {
  using value_type = T;
  using error_type = nullopt_t;

  auto is_ok(optional<T> const& o) -> bool {
    return o.has_value();
  }

  // extractors
  auto extract_value(auto&& o) -> value_type {
    return *FWD(o);
  }
  auto extract_error(auto&&) -> error_type {
    return nullopt;
  }

  // factories
  auto from_value(auto&& v) -> optional<T> {
    return optional<T>(in_place, FWD(v));
  }
  auto from_error(nullopt_t) -> optional<T> {
    return {};
  }
};
```

```cpp
template <class T, class E>
struct try_traits<expected<T, E>> {
  using value_type = T;
  using error_type = E;

  auto is_ok(expected<T, E> const& e) -> bool {
    return e.has_value();
  }

  // extractors
  auto extract_value(auto&& e) -> value_type {
    return *FWD(e);
  }
  auto extract_error(auto&& e) -> error_type {
    return FWD(e).error();
  }

  // factories
  auto from_value(auto&& v) -> expected<T, E> {
    return expected<T, E>(in_place, FWD(v));
  }
  auto from_error(auto&& e) -> expected<T, E> {
    return expected<T, E>(unexpect, FWD(e));
  }
};
```
:::

This also helps demonstrate the requirements for what `try_traits<O>` have to return:

* `is_ok` is invoked on an lvalue of type `O` and returns `bool`
* `extract_value` takes some kind of `O` and returns `value_type`
* `extract_error` takes some kind of `O` and returns `error_type`
* `from_value` and `from_error` return `O`.

Since the extractors are only invoked on an `O` directly, you can safely assume that the object passed in is basically a forwarding reference to `O`, so `auto&&` is fine (at least pending something like [@P2481R0]). The extractors have the implicit precondition that the object is in the state specified (e.g. `extract_value(o)` should only be called if `is_ok(o)`, with the converse for `extract_error(o)`) The factories can accept anything though, and should probably be constrained.

The choice of desugaring based specifically on the return type (rather than relying on each object to produce some kind of construction disambiguator like `nullopt_t` or `unexpected<E>`) is not only that we can be more performant, but also we can allow conversions between different kinds of error types, which is useful when joining various libraries together:

::: bq
```cpp
auto foo(int i) -> tl::expected<int, E>;
auto bar(int i) -> std::expected<int, E>;

auto strcat(int i) -> Result<string, E>
{
    // this works
    return std::format("{}{}", foo(i)??, bar(i)??);
}
```
:::

As long as each of these various error types opts into `try_traits` so that they can properly be constructed from an error, this will work just fine.

### Handling errors without propagating

This approach seems to work quite well at propagating errors: it's syntactically cheap, performant, and allows for integrating multiple libraries.

But what if we didn't want to propagate the error, but rather do something else with it? For `std::optional` and `std::expected`, we already have a UB-if-error accessor in the form of `*x` and a throw-if-error accessor in the form of `x.value()`. It seems like the corollary to an error-propagating `x??` would be some sort of `x!!` that somehow forces the error differently.

While propagating the error only really has one way to go (you return it), there are quite a few different things you can do differently:

* `assert` that `is_ok()`
* `abort()` if `not is_ok()`
* `unreachable()` (or `[[assume]]`) if `not is_ok()`
* `throw extract_error()` if `not is_ok()`
* `throw f(extract_error())` if `not is_ok()` for some `f`

That's a lot of different options, and the right one likely depends on context too.

An additional template parameter on the error type could drive what `x!!` does (as Boost.Outcome does, for instance), which would allow you to preserve the nice syntax if a particular error handling strategy is sufficiently common (maybe you always `throw`, so why would you want to write extra syntax for this case), but at a cost of suddenly having way more types. Although the `try_traits` approach does at least allow those "way more types" to interact well.

This behavior can be achieved by adding a new function to `try_traits` which desugars as follows:

::: cmptable
```cpp
auto val = expr!!;
```

```cpp
auto&& $__val$ = expr;
using $_Traits$ = std::try_traits<
  std::remove_cvref_t<decltype($__val$)>>;
if (not $_Traits$::is_ok($__val$)) {
  $_Traits$::fail(FWD($__val$));
}
auto val = $_Traits$::extract_value(FWD($__val$));
```
:::

Alternatively, perhaps `!!` is a binary operator somehow that takes its policy as an argument, like `x!!(abort)`. This reduces the number of types necessary, at the cost of simply looking bizarre.

For now, this paper is only proposing `??` as only child lacking a `!!` sibling.

