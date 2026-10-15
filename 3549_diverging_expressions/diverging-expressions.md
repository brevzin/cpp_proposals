---
title: "Diverging Expressions"
document: P3549R1
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


# Revision History {- .unlisted .collapsed}

Since [@P3549R0]: keeping the deduction rules equivalent for `do` expressions, functions, and lambdas — each can now deduce `noreturn_t` if they diverge. Added [implementation experience](#implementation-experience) section.

# Introduction

One pattern that will occur with some regularity with pattern matching [@P2688R6]{.title} (and [@P2806R5]{.title}) is the desire to produce values for some patterns but not for all. Consider the following simplified example:

<table>
<tr><td>
```cpp
void f(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => std::terminate();
  };
  use(j);
}
```
</td>
<td>
```cpp
void g(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => do {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
<td>
```cpp
void h(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => do -> int {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
</tr>
</table>

In all of these cases, the desire is that if `i == 0` then we initialize `j` to the value `0`, otherwise we `std::terminate()`. Given that `std::terminate` does in fact, as the name suggests, terminate, we don't actually have to worry about producing a value in that case. These examples are all equivalent — or at least, they should be. But C++ does not currently recognize them as such (with one exception that we'll get to later).

The rule for pattern matching by default is that the type of the match expression is deduced from each arm and each arm has to have the same type. This is the same rule that we have for `auto` deduction in functions and lambdas. In the above examples, the first arm always has type `int`. But for `f()` and `g()`, the second arm has type `void`. In order to type check, we have to wrap our call to `std::terminate` in an expression that actually has type `int`. The `do` expression in `h()` is one such way.

The extra `-> int` does solve the problem, but it's also misleading. We're not actually ever producing an `int` here, we're just writing a type annotation to hammer the compiler into submission. That's just not a great place to be.

Our goal with this paper is to have all of `f()`, `g()`, and `h()` above type check. Concretely: we need to recognize that an expression can _diverge_ and then have pattern matching's type deduction rules only consider those arms that do not diverge. Importantly, we want to treat the expressions:

::: std
```cpp
std::terminate()
```
:::

and

::: std
```cpp
do { std::terminate(); }
```
:::

and

::: std
```cpp
do { log::error("hasta la vista"); std::terminate(); }
```
:::

as all being diverging expressions in order to facilitate code evolution. It would be annoying if a naked call to `std::terminate` worked but once you wrap it in a `do` expression that does so much as add a single log statement that it suddenly doesn't. We want to properly recognize divergence, not just the most trivial of cases.

# Diverging Expressions

An expression is said to _diverge_ if it unconditionally escapes control flow. Currently in C++, we have two such expressions in the language:

* a `$throw-expression$`, and
* a call to a `[[noreturn]]` function.

The former explicitly diverges (control flow immediately escapes and doesn't return to keep evaluating in the original spot) while the latter implicitly diverges (technically a function marked `[[noreturn]]` can still return, we just state that doing so is undefined behavior in [dcl.attr.noreturn]{.sref}).

As hinted at earlier, the language already recognizes that expressions can diverge in one spot: the conditional operator. Consider:

::: std
```cpp
int x = $condition$ ? 42 : throw std::runtime_error("oops"); // OK
```
:::

The conditional operator has to merge two values into one. But if one of the operands is a `$throw-expression$`, then the value of the conditional operator is trivially the other operand — or there is just no value to speak of. The above is perfectly valid code, the conditional expression is a prvalue `int` (that sometimes throws).

The pattern matching paper recognizes this as well, and explicitly recognizes pattern arms that are `$throw-expression$`s as not participating in deduction either, while also introducing syntax for a non-returning expression handler:

::: std
```cpp
int x = match (v) { // OK
  case 0 => 0;
  case _ => throw std::runtime_error("oops");
};

int y = match (v) { // OK
  case 0 => 0;
  case _ => not return std::terminate();
};
```
:::

But this does not work today, despite being just as diverging as a `throw`:

::: std
```cpp
int x = $condition$ ? 42 : std::terminate();
```
:::

Instead you could do something like this (which we think is the kind of thing that really belongs on a T-shirt):

::: std
```cpp
int x = $condition$ ? 42 : throw (std::terminate(), 0);
```
:::

Note that this _does_ work — we will terminate before we evaluate the `throw`.

But there's not any difference between throwing an exception and invoking a non-returning function when we're talking about whether an expression produces a value. Neither produces a value, so neither needs to participate in any language rules that involve merging multiple expressions!

We think it's important to recognize invocations of non-returning functions as diverging because we want people to be able to write the code on the left, and not have to write either of the two workarounds presented (or the bespoke `! return` syntax):

<table>
<tr><th>Desired</ht><th>Workaround with `throw`</th><th>Workaround with explicit type</th></tr>
<tr><td>
```cpp
void f(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => std::terminate();
  };
  use(j);
}
```
</td>
<td>
```cpp
void g(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => throw (std::terminate(), 0);
  };
  use(j);
}
```
</td>
<td>
```cpp
void h(int i) {
  int j = match (i) {
    case 0 => 0;
    case _ => do -> int {
      std::terminate();
    };
  };
  use(j);
}
```
</td>
</tr>
</table>

## Diverging `do` expressions

Consider the following expressions:

1. `throw 42`
2. `std::terminate()`
3. `do { throw 42; }`
4. `do { std::terminate(); }`

All four of these expressions diverge, the bottom two just trivially wrap the top two. So we'd want these to have the same properties when it comes to divergence — otherwise any slight refactoring could have too much impact. For instance, the desire to change:

::: std
```cpp
std::terminate()
```
:::

into

::: std
```cpp
do {
    log::fatal("oops, I did it again");
    std::terminate();
}
```
:::

should ideally not change any properties of the expression. Both diverge, one just additionally adds some logging.

One question we have to answer is: What is the `decltype` of such an expression?

Right now, `decltype(throw 42)` is defined as `void`, [explicitly](https://eel.is/c++draft/expr.throw#1). For some reason. `decltype(std::terminate())` is more obviously `void` simply because that's how the function is defined — it is a `void()` (although a `[[noreturn]]` one). So there's certainly something to be said for going ahead and wanting to define `decltype(do { throw 42; })` as `void` as well.

This approach leads to the following pair of rules:

1. the _type_ of a `do` expression is either
    * the _trailing-return-type_, if one is explicitly provided, or
    * the type that is `do_return`-ed consistently (same as the lambda rule except with `do_return` instead of `return`).
2. A `do` expression is said to _diverge_ if every control flow path leads to executing a diverging expression (i.e. a `$throw-expression$`, a call to a `[[noreturn]]` function, or evaluating another such diverging expression).

This effectively means that we keep adding properties to expressions: an expression would have a type, a value category, whether it's a bit-field, and now also whether it diverges.

There is another approach though.

## A bottom type

Several languages have a notion of a bottom type (`⊥`) for diverging expressions. This type is spelled `Nothing` in Scala/Kotlin, `never` in TypeScript/PHP, `Never` in Python, `noreturn` in D/Zig, `!` in Rust, `void` in Haskell (but notably not `void` in C++), `Base.Bottom` in Julia, etc. That list is non-exhaustive. This type represents an expression that diverges.

Let's imagine what it would look like if C++ also had such a type. We'll call it `noreturn_t`. It would have some interesting properties:

* You cannot produce an instance of such a type. It wouldn't be very divergent otherwise! In this case, it's not just that this type isn't constructible, but we would also have to eliminate all of C++'s other clever ways of producing a value (like `reinterpret_cast`, `std::bit_cast`, etc.).
* On the other hand, `noreturn_t` is convertible to any other type (including reference types). This is currently impossible to emulate in C++ today, since you cannot make a type that is both convertible to `T` and `T&` for all `T`.
* Function pointer conversions are more interesting. From a type theory perspective, `auto(*)(T) -> noreturn_t` is convertible to `auto(*)(T) -> U` for all `U`. For the same reason Haskell has the [`absurd` function](https://hackage.haskell.org/package/void-0.7/docs/Data-Void.html). But this doesn't quite work out in C++ because of calling convention reasons: the hidden return-pointer argument shifts the other arguments on multiple platforms. `void` doesn't have this problem, so `auto(*)(T) -> noreturn_t` can be simply converted to `auto(*)(T) -> void`. But for all other return types, `auto(*)(Args...) -> noreturn_t` can only be converted to an `auto(*)(Args...) -> U` if the source is constant (as if the conversion were `consteval`), via a thunk.

This makes sense from a type-theoretic perspective and makes a lot of other uses just work.

We would then change the type of a `$throw-expression$` to be `noreturn_t` (instead of `void`). This change means we'd replace the special case of `$throw-expression$` in the conditional operator with generic handling of diverging expressions.

Consider:

::: std
```cpp
$condition$ ? x : throw std::runtime_error("oops")
```
:::

This evaluates as an lvalue referring to `x`, since the third operand diverges.


We would also want to change the signatures of all of our never-returning functions to have this in their types:

::: std
```diff
namespace std {
- [[noreturn]] auto terminate() -> void;
+              auto terminate() -> noreturn_t;
}
```
:::

Which would likewise allow for the desired conditional with `std::terminate()` to work for the same reasons as the conditional with `throw`:

::: std
```cpp
$condition$ ? 42 : std::terminate()
```
:::


We're approaching `std::noreturn_t` from the perspective of wanting to detect diverging expressions and statements. But there are other reasons to want to have a bottom type in the type system. Consider...

### `std::function<std::noreturn_t()>`

How do you have a callback that signals that it must not terminate? Well, you can't really. Because that's not something you can signal in the type system today. But with a bottom type it can be, and the type `std::function<std::noreturn_t()>` becomes meaningful.

### `std::expected<T, std::noreturn_t>`

Consider the type `std::expected<T, std::noreturn_t>`. What are its properties? Well, in general, a `std::expected<T, E>` is holding either a `T` in its valid state or an `E` in its error state, and so its storage is something like a `bool` and a `union { T; E; }`. But if `E` is `std::noreturn_t`, then we know that there _cannot be_ an error state. You cannot form a value of type `std::noreturn_t`, so we don't even need to store it at all. The layout of `std::expected<T, std::noreturn_t>` can simply be `struct { T; }`. Likewise for `std::expected<std::noreturn_t, E>`, which can only ever be in the error state.

This is a useful thing to be able to express in the type system, since you might have an API whose contract is that it returns some kind of `expected<T, E>` — but this particular implementation of that API might simply never fail (or always fail). So we can return an `expected<T, noreturn_t>` to conform to the expected shape of the return type while also having all the state checks trivially optimize away (since its `operator bool() const` is trivially just `return true;`). Moreover, a lot of code that expects a particular `E` will continue to work fine since `noreturn_t` is convertible to `E`.

Note that this will just work out of the box, but the standard library will probably want to actually provide a specialization for `std::expected<T, std::noreturn_t>` and `std::expected<std::noreturn_t, E>` so that rather than storing a `bool` and a `union { T t; E ; }` the specialization can just unconditionally store a `T` or an `E`.

### Valueless Ranges

By some coincidence, Barry just ran into this issue (again) while we were writing the initial revision of this paper.

A Range (in the C++20 sense) has a few associated types. Its `reference` is just `decltype(*it)`, unfortunately named in retrospect since it need not be a reference type. Then, a range has a `value_type` — which is the type you would use if you wanted an independent value. And the range's `value_type` and `reference` have to be related somehow, they need to have a `common_reference`. For most ranges, there isn't much to think about here — `reference` is either `T&` or `T const&` for some object type `T`, `value_type` is `T`, everything just falls out straightforwardly.

But there are few situations where this just doesn't work out:

1. Let's say we have an abstract base class, `Abstract`. We can produce a range whose `reference` is `Abstract&` (let's say we don't want to deal with pointers, since we know none of ours are ever null). But we simply _cannot_ produce a `value_type` of `Abstract`. It's... abstract. There are situations where you can get away with it, but in a lot of ranges code, you just can't. This is [@LWG3864].

2. Let's say we have a type `S` with a trailing flexible array member. We can produce a range whose reference is `S&`. Unlike `Abstract`, it might be valid to form an object of type `S` — it would just be semantically wrong to do so. Moreover, `enumerate`ing such a range would end up producing a `value_type` of `tuple<ptrdiff_t, S>`, which in libstdc++'s layout is `struct { S; ptrdiff_t; }`. That puts the flexible array member in the middle of the struct, and gcc 14 starts rejecting this code.

3. Similar to (2), there are other cases of dynamically sized ranges. For instance, rather than a `span<T>` I might want to produce an `erased_span<Base>` which is a range of `Base&` but whose dynamic type (and size) is chosen at runtime. Any code that attempts to actually produce a `value_type` of `Base` would be logically wrong — even if it might compile.

In all of these cases, what we really want to be able to say is: there _is_ no `value_type`. Any algorithm that attempts to produce one is broken.

A bottom type solves this quite nicely. Adding `using value_type = std::noreturn_t;` meets the other requirements, as long as `std::noreturn_t&` is convertible to `reference`. We get a valid C++20 range and the compile errors we get are actual logic errors — using algorithms that try to actually form a `value_type`.

This, in of itself, isn't a complete solution, since all the invocation concepts in Ranges ([indirectcallable]{.sref}) still will attempt to invoke the provided callable with `value_type&` (or the projected `value_type&`), and we'd need to do something there too. But simply having a `std::noreturn_t` to use here does solve a lot of the problem.


## Doing Our Due Diligence to Deduce `do` Divergence

Consider the following pattern arms:

::: std
```cpp
@*pattern*~1~@ => do { std::println("never gonna give you up"); throw 2; };
@*pattern*~2~@ => do { std::println("never gonna let you down"); std::terminate(); };
@*pattern*~3~@ => do { std::println("never gonna run around"); do_return std::terminate(); };
@*pattern*~4~@ => do { std::println("and desert you"); continue; };
@*pattern*~5~@ => break;
```
:::

The last case here is simple — we can special case escaping statements, and the pattern matching paper already does this.

The first four cases though also all unconditionally diverge. We know that from simply examining the code. But what is the specific language rule by which we could ensure that the pattern match expression can exclude these arms when deducing the type? In `@*pattern*~3~@`, we actually have a `do_return` statement, so the `do` expression there could straightforwardly be said to have type `noreturn_t` and thus diverge. But the other three have no `do_return` and thus would, by default, end up having type `void` — which we do not want. So what do we do?

One option is to copy the logic from some other languages (like Rust) and also the model of GCC's  statement-expression model and have an implicit value from `do` expressions. That is, the other three patterns above implicitly take their values from `throw 2`, `std::terminate()`, and `continue` respectively — which all have type `noreturn_t`. So this would just work. But we've already decided to _not_ pursue implicit last value in `do` expressions (see [that paper's discussion](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2026/p2806r5.html#implicit-last-value)).


So if we don't want to use implicit last value, how else can we deduce divergence? We suggest the following rule, inspired by implicit last value:

::: {.std .wording}
[1]{.pnum} An expression is a *diverging expression* if its type is `noreturn_t`.

[2]{.pnum} A statement can *complete normally* unless it is one of the following:

* [2.1]{.pnum} a `$jump-statement$`; [this is `break`, `continue`, `return`, `co_return`, `goto`, or `do_return`]{.draftnote}
* [2.#]{.pnum} an `$expression-statement$` whose $expression$ is a diverging expression;
* [2.#]{.pnum} a `$compound-statement$` whose last statement cannot complete normally, or that contains a statement `S` that cannot complete normally such that no statement following `S` in the `$compound-statement$` is or encloses a `$labeled-statement$`;
* [2.#]{.pnum} a `$labeled-statement$` whose `$statement$` cannot complete normally;
* [2.#]{.pnum} an `if` statement other than a `constexpr if`, with an `else`, both of whose substatements cannot complete normally;
* [2.#]{.pnum} a constexpr `if` statement whose non-discarded substatement exists and cannot complete normally; or
* [2.#]{.pnum} a `$try-block$` whose `$compound-statement$` and the `$compound-statement$` of each of its $handler$s cannot complete normally.

Such a statement cannot complete normally.

[3]{.pnum} The returning statements of a `$do-expression$` `E` are the non-discarded `do_return` statements whose innermost enclosing `$do-expression$` is `E`. The type of `E` is determined as follows:

* [3.1]{.pnum} If `E` has a `$trailing-return-type$` that is not a placeholder, that type.
* [3.#]{.pnum} Otherwise, if any returning statement of `E` has no operand, or has an operand that is not a diverging expression, the type deduced from those returning statements, where a `do_return` statement with no operand deduces `void`. If the type deduced is not the same in each deduction, the program is ill-formed.
* [3.#]{.pnum} Otherwise, if the `$compound-statement$` of `E` can complete normally, `void`.
* [3.#]{.pnum} Otherwise, `noreturn_t`.
:::

If there are any `do_return` statements whose expression doesn't diverge, then the `do` expression doesn't diverge. It might produce a value. Maybe that `do_return` statement is logically unreachable, but we can't in general determine that. But if there are no `do_return` statements, we need to see if we actually diverge. Which is non-trivial because we need to handle things like:

::: std
```cpp
// just a jump-statement
do { continue; }

// a statement-expression that is a diverging expression
do { std::terminate(); }

// an if statement with just an if, this is NOT diverging
// because if the $condition$ is false, then we're just a void expression
do { if ($condition$) { throw 1; } }

// but this one DOES diverge (just differently in both branches)
do { if ($condition$) { continue; } else { break; } }

// this diverges iff the condition is true
do { if constexpr ($condition$) { std::terminate(); } }
```
:::

This rule ensures that all of our initial pattern arms diverge, as desired:

::: std
```cpp
@*pattern*~1~@ => do { std::println("never gonna make you cry"); throw 2; };
@*pattern*~2~@ => do { std::println("never gonna say goodbye"); std::terminate(); };
@*pattern*~3~@ => do { std::println("never gonna tell a lie"); do_return std::terminate(); };
@*pattern*~4~@ => do { std::println("and hurt you"); continue; };
@*pattern*~5~@ => break;
```
:::

## Consistency with lambdas and functions

If the expression `do { std::terminate(); }` changes from having type `void` to having type `noreturn_t`, this raises the question of whether we should do the same for functions and lambdas:

::: std
```cpp
auto f() {
  throw 1;
}

auto g = []{
  std::terminate();
};
```
:::

Today the types of `f()` and `g()` are both `void`. We think it's important that the rules for lambdas, functions, and `do` expressions be consistent — so we do propose keeping one single, consistent deduction rule across all three kinds. That is, `f` and `g` would both now return `noreturn_t` because they both diverge.

Note that this does break code, see [implementation experience](#implementation-experience).

## Alternative with `[[noreturn]]`

The advantage of the approach described with `noreturn_t` is that diverging expressions can appear in the type system. This provides a better solution than `[[noreturn]]`, in a way that can further generalize to other scenarios (as we showed earlier). We can have a `std::function<std::noreturn_t()>`, we can have `std::expected<T, std::noreturn_t>`, valueless ranges, etc.

But an alternative approach would be to avoid changing any of the existing functions (like `std::abort()`, `std::terminate()`, etc.) or changing the type of a `$throw-expression$`, and instead recognize `[[noreturn]]` more explicitly.

The rule we lay out above would instead become:

::: std
[1]{.pnum} A `$statement$` is a *diverging statement* if it is:

* [1.1]{.pnum} A `$compound-statement$` where the last statement is a diverging statement,
* [1.2]{.pnum} a `$jump-statement$`,
* [1.3]{.pnum} a `$statement-expression$` whose `$expression$` is a diverging expression, or
* [1.4]{.pnum} an `if` statement with an `else` branch, where both substatements are diverging statements.
* [1.5]{.pnum} a constexpr `if` statement where the taken substatement is a diverging statement.

[2]{.pnum} An expression is a *diverging expression* if:

* [2.1]{.pnum} it is an invocation of a `[[noreturn]]` function,
* [2.2]{.pnum} it is a `$throw-expression$`, or
* [2.3]{.pnum} it is a `$do-expression$` whose type is `void` and whose last statement is a diverging statement.
:::

This is a simpler change. It doesn't involve introducing a new language/library type or changing existing standard library functions or the meanings of some code — although we doubt too many people are relying on `decltype(throw 1)` being `void`. However, it doesn't compose. `[[noreturn]]` isn't deduced, so wrapping becomes challenging.

Leaning on `[[noreturn]]` also means that we end up leaning on `void` even harder to mean two completely different things: `void f() { }` is a function that returns a value, while `[[noreturn]] void g() { std::exit(-1); }` is a function that doesn't.

# Implementation Experience

This proposal is implemented in Barry's fork of clang, the patch is [here](https://github.com/brevzin/llvm-project/commit/60366a0a52cd10aac1886f7a69b405a57a92ee52) and can be used on compiler explorer.

That patch also includes the changes to function and lambda deduction described [earlier](#consistency-with-lambdas-and-functions), so we tried to compile a bunch of existing projects to see what fails. The breakage was pretty interesting.

Most of the breakage occurs in library types that mimic existing function deduction rules, which we're changing in this proposal, so those libraries calculations have to adjust as well. An example is variant visitation:

::: std
```cpp
auto f(std::variant<A, B> var) -> void {
    std::visit([&]<class T>(T x){
      if constexpr (std::same_as<T, A>) {
        std::cout << "Got an A\n";
      } else {
        throw 42;
      }
    }, var);
}
```
:::

In the C++26 status quo, that lambda is `void` in both instantiations. But with this proposal, that lambda has type `void` for `A` and `noreturn_t` for `B`. This is an easy library fix — adjust the rules for `std::visit` to line up with the proposed changes to the rules for functions. Which is to say, rather than all the return types having to be the same, only all the non-`noreturn_t` return types have to be the same. There were a bunch of breakages of this form, across several different `variant` implementations.

Another breakage with is `libunifex`, in some tests which create the sender:

::: std
```cpp
auto s = just_from([]() { throw 1; });
```
:::

Which likewise used to be a sender producing no value and now is a sender producing a `noreturn_t`. There are tests which validate that this is a sender of `void` and also that an `any_sender<>` can be constructed from this. That can be easily fixed as well.

Lastly, LLVM's own `ErrorHandlerTraits` special-cases `void` and would need to handle `noreturn_t` as well.

# Proposal

We propose to:

* to introduce a new type `std::noreturn_t` which represents an expression with no value, that unconditionally diverges.
  * Defined as

    ::: std
    ```cpp
    namespace std {
      using noreturn_t = decltype(throw 0);
    }
    ```
    :::
  * `noreturn_t` is a complete object type, with no constructors (other than copy and move).
  * `noreturn_t` is trivially copyable.
  * `std::noreturn_t` is convertible to any type (including reference types) with Exact Match semantics.
  * `std::noreturn_t(*)(Args...)` is convertible to `void(*)(Args...)`
  * `std::noreturn_t(*)(Args...)` is convertible to `R(*)(Args...)` for all `R` (again, including reference types), but only if the source function pointer is a constant expression (i.e. this conversion behaves as if it is a `consteval` function).
* change all the standard library functions owned by C++ that are currently `[[noreturn]] void f()` to instead be `std::noreturn_t f()`: `std::terminate`, `std::unreachable`, `std::rethrow_exception`, and `std::throw_with_nested`. The C library functions (e.g. `abort`, `longjmp`) keep `[[noreturn]]` until we can check to make sure that we can change the result to be ABI preserving.
* change the type of `$throw-expression$` to `std::noreturn_t`
* remove the current... uh... exception for exceptions in the conditional operator, and replace it with a rule that if either the 2nd or 3rd operand diverges, then the result is simply the other operand. If both the 2nd and 3rd operands diverge, the expression diverges.
* change the deduction rules for `do` expressions, functions, and lambdas to deduce `noreturn_t` as their return type if the `do` expression/function/lambda diverges. Similarly for pattern match expressions in [@P2688R6].
* adjust the rules for variant visitation to ignore `noreturn_t` arms, similar to the rules for `do` expressions, functions, and lambdas.