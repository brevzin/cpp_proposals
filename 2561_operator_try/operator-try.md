---
title: "`operator??`"
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

This paper simply recognizes that there are many code bases (or parts thereof) that do not use exceptions and probably will not in the future. That could be for performance or space reasons. It could be because exceptions are unsupported on a particular platform. It could be for code understandability reasons. Regardless, some code bases do not use exceptions. Moreover, some problems are not solved well by exceptions -- even in code bases that otherwise use them to solve problems that they are more tailored to solve.

The problem is, C++ does not currently have a good story for error handling without exceptions. We're moving away from returning bool or error codes in favor of solutions like `std::expected` ([@P0323R12]), but the ergonomics of such types are not there yet. Bad ergonomics leads to code that is clunkier than it needs to be, harder to follow, and, significantly and ironically, error-prone.

We should try to improve such uses too.

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

There's a lot to like about exceptions. One nice advantage is the zero syntactic overhead necessary for propagating errors. Errors just propagate. You don't even have to know which functions can fail.

We don't even need to declare variables to hold the results of `foo` and `bar`, we can even use those expressions inline:

::: bq
```cpp
auto foo(int i) -> int; // might throw an E
auto bar(int i) -> int; // might throw an E

auto strcat(int i) -> std::string {
    return std::format("{}{}", foo(i), bar(i));
}
```
:::

But with the newly adopted `std::expected<T, E>`, it's not quite so nice:

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

In an effort to avoid... that... many libraries or code bases that use this sort approach to error handling provide a macro, which usually looks like this ([Boost.LEAF](https://www.boost.org/doc/libs/1_75_0/libs/leaf/doc/html/index.html#BOOST_LEAF_ASSIGN), [Boost.Outcome](https://www.boost.org/doc/libs/develop/libs/outcome/doc/html/reference/macros/try.html), [mediapipe](https://github.com/google/mediapipe/blob/master/mediapipe/framework/deps/status_macros.h), etc. Although not all do, neither `folly`'s `fb::Expected` nor `tl::expected` nor `llvm::Expected` provide such):

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

Maybe this is doable with parsing heroics, but at some point I have to ask if it's worth it. Especially since we can just pick something else: `??`

This is only one character longer, and just as questioning. It's easily unambiguous by virtue of not even being a valid token sequence today. But it's worth commenting on the usage of `??` in other languages.

### `??` in other languages

`??` is called a "null (or nil) coalescing operator" in some languages (like C# or JavaScript or Swift) where `x ?? y` is roughly equivalent to what C++ would spell as `x ? *x : y` except that `x` is only evaluated once. Kotlin spells this operator `?:`, but it behaves differently from the gcc extension since `x ?: y` in gcc evaluates as `x ? x : y` rather than `x ? *x : y`.

For `x` being some kind of `std::optional<T>` or `std::expected<T, E>`, this can *mostly* already be spelled `x.value_or(y)`. The difference is that here `y` is unconditionally evaluated, which is why [@P2218R0] proposes a separate `opt.value_or_else(f)` which invokes `f`. Which would make a proper equivalence be spelled `x.value_or_else([&]{ return y; })`.

I'm not aware of any proposals to add this particular operator in C++, but because we already have two types that directly provide that functionality (as would many other non-`std` flavors thereof), and because it's fairly straightforward to write such an algorithm generically, it wouldn't seem especially valuable to have a dedicated operator for this functionality -- so it's probably safe to take for this use-case.

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
* constructing a new object from either a value (`from_value`, not necessary in the above example, but will demonstrate a use later) or an error (`from_error`)

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
  auto extract_value(auto&& o) -> auto&& {
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
  auto extract_value(auto&& e) -> auto&& {
    return *FWD(e);
  }
  auto extract_error(auto&& e) -> auto&& {
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
* `extract_value` takes some kind of `O` and returns a type that, after stripping qualifiers, is `value_type`
* `extract_error` takes some kind of `O` and returns a type that, after stripping qualifiers, is `error_type`
* `from_value` and `from_error` return `O`.

In the above case, `try_traits<expected<T, E>>::extract_error` will always give some kind of reference to `E` (either `E&`, `E const&`, `E&&`, or `E const&&`, depending on the value category of the argument), while `try_traits<optional<T>>::extract_error` will always be `std::nullopt_t`, by value. Both are fine, it simply depends on the type.

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

### Short-circuiting fold

One of the algorithms considered in the `ranges::fold` paper ([@P2322R5]) was a short-circuiting fold. That paper ultimately didn't propose such an algorithm, since there isn't really a good way to generically write such a thing. Probably the best option in the paper was to have a mutating accumulation function that returns `bool` on failure?

But with this facility, there is a clear direction for how to write a generic, short-circuiting fold:

::: bq
```cpp
template <typename T>
concept Try = requires (T t) {
    typename try_traits<T>::value_type;
    typename try_traits<T>::error_type;

    { try_traits<T>::is_ok(t) } -> $boolean-testable$;
    // etc. ...
};

template <input_iterator I,
          sentinel_for<I> S,
          class T,
          invocable<T, iter_refrence_t<R>> F,
          Try Return = invoke_result_t<F&, T, iter_reference_t<R>>
    requires same_as<
        typename try_traits<Return>::value_type,
        T>
constexpr auto try_fold(I first, S last, T init, F accum) -> Ret
{
    for (; first != last; ++first) {
        init = std::invoke(accum,
            std::move(init),
            *first)??;
    }

    return try_traits<Ret>::from_value(std::move(init));
}
```
:::

This `try_fold` can be used with an accumulation function that returns `optional<T>` or `expected<T, E>` or `boost::outcome::result<T>` or ... Any type that opts into being a `Try` will work.

Note that this may not be exactly the way we'd specify this algorithm, since we probably want to return something like a `pair<I, Ret>` instead, so the body wouldn't be able to use `??` and would have to go through `try_traits` manually for the error propogation. But that's still okay, since the important part was being able to have a generic algorithm to begin with.

## Potential directions to go from here

This paper is proposing just `??` and the machinery necessary to make that work (including a `concept`, opt-ins for `optional` and `expected`, but not the short-circuiting fold algorithm).

However, it's worth it for completeness to point out a few other directions that such an operator can take us.

### Error continuations

Several languages have a facility that allows for continuing to invoke member functions on optional values. This facility is called something different in every language (optional chaining in Swift, null-conditional operator in C#, safe call operator in Kotlin), but somehow it's all spelled the same and does the same thing anyway.

Given a `std::optional<std::string>` named `opt`, what that operator -- spelled `?.` -- means is approximately:

|expression|C++ equivalent|
|-|---|
|`opt?.size()`|`opt.transform(&std::string::size) // technically UB`|
|`opt?.substr(from, to)`|`opt.transform([&](auto& s){ return s.substr(from, to); })`|

That is, the expression `E1?.E2`, if `E1` is an `optional`, basically means `E1.value_or_else([&](auto&& e){ return FWD(e).E2; })` (if we had the `value_or_else` facility as proposed in [@P2218R0]).

Like the null coalescing meaning of `??` described above, the semantics of `opt?.f()` can be achieved using library facilities today. Quite unlike `??`, there is a significant drop in readability and just the general nice-ness of the syntax.

The `try_traits` facility very nearly gives us the tools necessary to support such a continuation operator. Since what we need to do is:

* check is `E1` is truthy or falsey (`Traits::is_ok(E1)`)
* extract the value of `E1` in order to perform the subsequent operation (`Traits::extract_value(E1).E2`)
* extract the error of `E1` in order to return early (`Traits::extract_error(E2)`)

We mostly need one more customization point: to put the types back together. What I mean is, consider:

::: bq
```cpp
auto f(int) -> std::expected<std::string, E>;

auto x = f(42)?.size();
```
:::

The type of `x` needs to be `std::expected<size_t, E>`, since that's what the value case ends up being here. If we call that customization point `mapped_type`, as in:

::: bq
```cpp
template <typename T, typename E>
struct try_traits<expected<T, E>> {
    // ... rest as before ...

    template <class U>
    using mapped_type = expected<U, E>;
};
```
:::

Then the above can be desugared into:

::: bq
```cpp
using $_Traits$ = try_traits<remove_cvref_t<decltype(f(42))>>;
using $_R$ = $_Traits$::mapped_type<decltype($_Traits$::extract_value(f(42)).size())>;

auto&& $e$ = f(42);
auto x = $_Traits$::is_ok($e$)
       ? try_traits<$_R$>::from_value($_Traits$::extract_value(FWD($e$)).size())
       : try_traits<$_R$>::from_error(FWD($e$));
```
:::

That may seem like a mouthful, but all the user had to write was `f(42)?.size()` and this does do the right thing.

At least, this mostly does the right thing. We still have to talk about copy elision. Consider this version:

::: bq
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

::: bq
```cpp
auto&& $e$ = g();
auto n = $_Traits$::is_ok($e$)
       ? try_traits<$_R$>::from_value_func([&]() -> decltype(auto) {
            return $_Traits$::extract_value(FWD($e$)).f()
         })
       : try_traits<$_R$>::from_error(FWD($e$));
```
:::

By default, `try_traits<R>::from_value_func(f)` would just be `try_traits<R>::from_value(f())`.

This is weird, but it's something to thing about it.

Note also error continuation would only help in the member function case. If we want to continue into a non-member function, you'd need the sort of `.transform()` member function anyway.

### Not propagating errors

The `??` approach seems to work quite well at propagating errors: it's syntactically cheap, performant, and allows for integrating multiple libraries.

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

But this doesn't seem as valuable as `??` or even `?.` since this case is easy to add as a member function. Indeed, that's what `x.value()` and `*x` do for `optional` and `expected`.

Moreover, any of the kinds of behavior you want can be written as a free function:

::: bq
```cpp
template <class T, Try U = std::remove_cvref_t<T>>
auto narrow_value(T&& t) -> decltype(auto) {
    assert(std::try_traits<U>::is_ok(t));
    return std::try_traits<U>::extract_value(FWD(t));
}

template <class T, Try U = std::remove_cvref_t<T>>
auto wide_value(T&& t) -> decltype(auto) {
    if (not std::try_traits<U>::is_ok(t)) {
        [[unlikely]] throw std::try_traits<U>::extact_error(FWD(t));
    }
    return std::try_traits<U>::extract_value(FWD(t));
}

// etc.
```
:::

Which further demonstrates the utility of the proposed facility.
