---
title: "`consteval` needs to propagate up"
document: P2564R0
date: today
audience: EWG
author:
    - name: Barry Patch Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

[@P1240R2] proposes that we should use a monotype, `std::meta::info`, as part of a value-based reflection. One argument here is that lots of operations return ranges of `meta::info` and it would be valuable if we could simply reuse our plethora of existing range algorithms for these use-cases.

Let's investigate this claim.

We don't need a working implementation of P1240 to test this, it's enough to have this very, very loose approximation:

::: bq
```cpp
namespace std::meta {
    struct info { int value; };

    consteval auto is_invalid(info i) -> bool {
        // we do not tolerate the cult of even here
        return i.value % 2 == 0;
    }
}
```
:::

And let's pick a simple problem. We start some sequence of... we'll call them types:

::: bq
```cpp
constexpr std::meta::info types[] = {1, 3, 5};
```
:::

We want to ensure that none of them are invalid. This is a problem for which we have a direct algorithm: `none_of`. Let's try using it:

<table>
<tr><th>Attempt</th><th>Result</th></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    std::meta::is_invalid
));
```
</td><td>❌. Ill-formed per [expr.prim.id.general]{.sref}/4:

::: bq
[4]{.pnum} A potentially-evaluated id-expression that denotes an immediate function shall appear only

* [4.1]{.pnum} as a subexpression of an immediate invocation, or
* [4.2]{.pnum} in an immediate function context.
:::

Neither of those cases apply here.
</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) {
        return std::meta::is_invalid(i);
    }
));
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13:

::: bq
[13]{.pnum} An expression or conversion is in an immediate function context if it is potentially evaluated and either:

* [13.1]{.pnum} its innermost enclosing non-block scope is a function parameter scope of an immediate function, or
* [13.2]{.pnum} its enclosing statement is enclosed ([stmt.pre]) by the compound-statement of a consteval if statement ([stmt.if]).

An expression or conversion is an immediate invocation if it is a potentially-evaluated explicit or implicit invocation of an immediate function and is not in an immediate function context. An immediate invocation shall be a constant expression.
:::

`std::meta::is_invalid` is an immediate function, the call to it in the lambda is not in an immediate function context, so it's an immediate invocation. But `std::meta::is_invalid(i)` isn't a constant expression, because of the parameter `i` here.
</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }
));
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13 again. This time the lambda itself is fine, but now invoking the lambda from inside of `std::ranges::none_of` is the problem.
</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    +[](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }
));
```
</td>
</td><td>❌. Ill-formed per [expr.const]{.sref}/11:

::: bq
[11]{.pnum} A constant expression is either a glvalue core constant expression that refers to an entity that is a permitted result of a constant expression (as defined below), or a prvalue core constant expression whose value satisfies the following constraints:

* [11.1]{.pnum} if the value is an object of class type, each non-static data member of reference type refers to an entity that is a permitted result of a constant expression,
* [11.2]{.pnum} if the value is of pointer type, it contains the address of an object with static storage duration, the address past the end of such an object ([expr.add]), the address of a non-immediate function, or a null pointer value,
* [11.3]{.pnum} if the value is of pointer-to-member-function type, it does not designate an immediate function, and
* [11.4]{.pnum} if the value is an object of class or array type, each subobject satisfies these constraints for the value.

An entity is a permitted result of a constant expression if it is an object with static storage duration that either is not a temporary object or is a temporary object whose value satisfies the above constraints, or if it is a non-immediate function.
:::

Here, explicitly converting the lambda to a function pointer isn't a permitted result because it's an immediate function.
</td></tr>
</table>

That exhausts the options here. What if instead of trying to directly `static_assert` the result of an algorithm (or, equivalently, use it as an initializer for a constexpr variable or as the condition of an `if constexpr` or as a non-type template argument), we did it inside of a new `consteval` function?

<table>
<tr><th>Attempt</th><th>Result</th></tr>
<tr><td>
```cpp
consteval auto all_valid() -> bool {
    return std::ranges::none_of(
        types,
        std::meta::is_invalid
    );
}
static_assert(all_valid());
```
</td><td>✅. This one is actually valid per the language rules. Except, per the library rules, it's unspecified per [namespace.std]{.sref}/6:

::: bq
[6]{.pnum} Let F denote a standard library function ([global.functions]), a standard library static member function, or an instantiation of a standard library function template.
Unless F is designated an addressable function, the behavior of a C++ program is unspecified (possibly ill-formed) if it explicitly or implicitly attempts to form a pointer to F.
:::

</td></tr>
<tr><td>
```cpp
consteval auto all_valid() -> bool {
    return std::ranges::none_of(
        types,
        [](std::meta::info i) {
            return std::meta::is_invalid(i);
        }
    );
}
static_assert(all_valid());
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13 still.</td></tr>
<tr><td>
```cpp
consteval auto all_valid() -> bool {
    return std::ranges::none_of(
        types,
        [](std::meta::info i) consteval {
            return std::meta::is_invalid(i);
        }
    );
}
static_assert(all_valid());
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13 still.</td></tr>
<tr><td>
```cpp
consteval auto all_valid() -> bool {
    return std::ranges::none_of(
        types,
        +[](std::meta::info i) consteval {
            return std::meta::is_invalid(i);
        }
    );
}
static_assert(all_valid());
```
</td>
</td><td>✅. Valid. Language and library both.</td></tr>
</table>

This leaves a lot to be desired. The only mechanism completely sanctioned by the language and library rules we have today is to write a bespoke consteval function which invokes the algorithm using a non-generic, consteval lambda that just wraps an existing library function and converts it to a function pointer.

 Put simply: algorithms are basically unusable with the consteval rules we have.

# `consteval` is a color

The problem is that `consteval` is a color [@what-color]. `consteval` functions and function templates can only be called by other `consteval` functions and function templates.

`std::ranges::none_of`, like all the other algorithms in the standard library, and probably all the other algorithms outside of the standard library, are not `consteval` functions or function templates. Moreover, none of these algorithms are ever going to either become `consteval` or be duplicated in order to gain a `consteval` twin. Which means that none of these algorithms, including `none_of`, can ever invoke any of the facilities proposed by [@P1240R2].

## Some code is already multi-colored

Almost.

One of the new C++23 features is `if consteval` [@P1938R3], which partially alleviates the `consteval` color problem. That facility allows `consteval` functions to be called by `constexpr` functions in a guarded context, largely in order to solve the same sort of problem that `std::is_constant_evaluated()` [@P0595R2] set out to solve except in a way that would now allow you to invoke `consteval` functions.

The way it works is by establishing an immediate function context. This is important because the rule we were constantly running up against was that:

::: bq
[13]{.pnum} An immediate invocation shall be a constant expression.
:::

And a call to an immediate function that is in an immediate function context is not an immediate invocation.

In our case here, we're not trying to choose between a compile-time friendly algorithm and a run-time efficient one. We're only trying to write compile-time code. But that's okay, there's no rule that says you need an `else`:

<table>
<tr><th>Attempt</th><th>Result</th></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) {
        if consteval {
            return std::meta::is_invalid(i);
        }
    }));
```
</td><td>✅. Valid.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) consteval {
        if consteval {
            return std::meta::is_invalid(i);
        }
    }));
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13. Still.</td></tr>
</table>

So... this is fine. We can still use `consteval` functions with lambdas, as long as we just wrap all of our lambda bodies in `if consteval` (but definitely also not make them `consteval` - they're only kind of `consteval`).

## Making the library multi-colored

We could avoid putting the burden of sprinkling their code with `if consteval` by pushing all of these extra calls into the library.

For instance, we could make this change to `none_of`:

::: cmptable
### Today
```cpp
template <ranges::input_range R, class Pred>
constexpr auto ranges::none_of(R&& r, Pred pred) -> bool {
    auto first = ranges::begin(r);
    auto last = ranges::end(r);
    for (; first != last; ++first) {
        if (pred(*first)) {
            return false;
        }
    }
    return true;
}
```

### Tomorrow?
```cpp
template <ranges::input_range R, class Pred>
constexpr auto my::none_of(R&& r, Pred pred) -> bool {
    auto first = ranges::begin(r);
    auto last = ranges::end(r);
    for (; first != last; ++first) {
        if consteval {
            if (pred(*first)) {
                return false;
            }
        } else {
            if (pred(*first)) {
                return false;
            }
        }
    }
    return true;
}
```
:::

But unfortunately that doesn't actually work. If `pred(*first)` were actually a `consteval` call, then even duplicating the check in both branches of an `if consteval` doesn't help us. The call to `pred(*first)` in the first sub-statement (the case where we are doing constant evluation) is fine, since now we're in an immediate function context, but the call to `pred(*first)` in the second sub-statement (the "runtime" case) is just as problematic as it was without the `if consteval`.

So the attempted solution on the right isn't just ridiculous looking, it also doesn't help. The only library solution here (and I'm using the word solution fairly loosely) is to have one set of algorithms that are `constexpr` and a completely duplicate set of algorithms that are `consteval`.

This has to be solved at the language level.

## Making the language multi-colored

Let's consider the problem in a very local way, going back to the lambda example and presenting it instead as a function (in order to simplify things a bit):

::: cmptable
### ill-formed
```cpp
constexpr auto pred_bad(std::meta::info i) -> bool {
    return std::meta::is_invalid(i);
}
```

### ok
```cpp
constexpr auto pred_good(std::meta::info i) -> bool {
    if consteval {
        return std::meta::is_invalid(i);
    }
}
```

:::

As mentioned multiple times, `pred_bad` is ill-formed today because it contains a call to an immediate function outside a consteval context and that call isn't a constant expression. That is one way we achieve the goal of the `consteval` functions that they are only invoked during compile time. But `pred_good` is good because that call only appears in an `if consteval` branch (i.e. a consteval context), which makes the call safe.

What's interesting about `pred_good` is that while it's marked `constexpr`, it's actually _only_ meaningful during compile time (in this case, it's actually UB at runtime since we just flow off the end of the function). So this isn't really a great solution either. We need to ensure that `pred_good` is only called at compile time.

But we have a way to ensure that: `consteval`.

Put differently, `pred_bad` is today ill-formed, but only because we need to ensure that it's not called at runtime. If we could ensure that, then we calling it during compile time is otherwise totally fine. What if the language just did that ensuring for us? If such `constexpr` functions, that are only ill-formed because of calls to `consteval` functions simply became `consteval`, then we gain the ability to use them at compile time without actually losing anything - we couldn't call them at runtime to begin with.

# Proposal

This paper proposes:

1. If a `constexpr` function that contains a call outside a consteval context to an immediate function and that call isn't a constant expression, that function implicitly becomes a `consteval` function. This is intended to include lambdas, function template specializations, special member functions, and should cover member initializers as well.

2. If an _expression-id_ designates a `consteval` function without it being an immediate call in such a context, it also makes the context implicitly consteval. Such _expression-id_'s are also allowed in contexts that are manifestly constant evaluated.

3. Other manifestly constant evaluated contexts (like _constant-expression_ and the condition of a constexpr if statement) are now considered to be immediate function contexts.

With these rule changes, no library changes are necessary, and any way we want to write the original call just works:

<table>
<tr><th>Attempt</th><th>Proposed</th></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) {
        return std::meta::is_invalid(i);
    }));
```
</td><td>✅. First, the lambda becomes implicitly `consteval` due to the non-constant call `is_invalid(i)`. This, in turn, makes this instantiation of `ranges::none_of` implicitly `consteval`. And then everything else just works.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }));
```
</td><td>✅. Now, the lambda is explicitly `consteval` instead of implicitly `consteval`, which likewise also makes this instantiation of `ranges::none_of` implicitly `consteval`. Everything else just works.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    std::meta::is_invalid
));
```
</td><td>✅. Still bad based on the library wording, but from the second proposed rule `std::meta::is_invalid` is usable in this context because it is manifestly constant evaluated.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    +[](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }
));
```
</td><td>✅. Previously, this was ill-formed because the conversion to function pointer needed to be (in of itself) a constant expression, but with the third proposed rule this conversion would now occur in an immediate function context. The "permitted result" rule no longer has to apply, so this is fine.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    []{
        return std::meta::is_invalid;
    }()
));
```
</td><td>✅. Likewise, still bad based on the library wording, but from the second proposed rule, using `std::meta::is_invalid` in this context makes the lambda `consteval`. The third rule makes the lambda invocation okay because we're in an immediate function context,  which likewise makes `ranges::none_of` consteval. </td></tr>
</table>

## Implementation Experience

This has been implemented in EDG by Daveed Vandevoorde. One caveat he brings up is this example:

::: bq
```cpp
consteval int g(int p) { return p; }
template<typename T> constexpr auto f(T) { return g; }
int r = f(1)(2);  // Okay or not?
```
:::

Per the proposal here, this is still ill-formed. `f` implicitly becomes a `consteval` function template due to use of `g`. Then, `f(1)` is required to be a constant expression, but we're not in an immediate function context (even with the new rules). It's probably possible to come up with a way to make this work, since `g` is still not leaking to runtime in any way (which was the goal of making it ill-formed to begin with). But for now, this seems fine to continue to reject.

# Acknowledgments

Thanks to Daveed Vandevoorde and Tim Song for discussions around this issue and Daveed for implementing it.

---
references:
  - id: what-color
    citation-label: what-color
    title: "What Color is Your Function?"
    author:
      - family: Bob Nystrom
    issued:
      - year: 2015
        month: 2
        day: 1
    URL: https://journal.stuffwithstuff.com/2015/02/01/what-color-is-your-function/
---
