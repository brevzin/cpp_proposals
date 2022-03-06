---
title: "`consteval` needs to propagate up"
document: P2564R0
date: today
audience: EWG
author:
    - name: Barry Revzin
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
constexpr std::meta::info types[] = {1, 2, 3};
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

### Definitely, Absolutely Not Proposed
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

As shown earlier, the code on the left doesn't work with a lambda that either is `consteval` or directly invokes a `consteval` function, but the code on the right would work just fine with a `consteval` lambda:

<table>
<tr><th>Attempt</th><th>Result</th></tr>
<tr><td>
```cpp
static_assert(my::none_of(
    types,
    [](std::meta::info i) {
        return std::meta::is_invalid(i);
    }));
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13 still.</td></tr>
<tr><td>
```cpp
static_assert(my::none_of(
    types,
    [](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }));
```
</td><td>✅. Now this is okay.</td></tr>
</table>

The former still fails because the `is_invalid` call isn't guarded, but the latter is fine because the whole lambda now is.

This is better, at least from the user's perspective. Having to explain why you need the extra `consteval` annotation surely beats having to explain why you need the extra `if consteval`. But not better enough.

## Making the language multi-colored

The reason that `my::none_of` works is because I carefully guarded the call to `pred` in `if consteval`. But notice that the code is actually the same in both branches.

I didn't need to do anything different between compile time and run time. I just needed to inform the language that it is actually safe to call any `consteval` function that might be there. That is actually the motivation for the rigid `consteval` rules [@P1073R3] to begin with: to ensure that they are only invoked at compile time.

In the `my::none_of` implementation, I only guarded the call to `pred`.  I could've instead written it this way:

::: bq
```cpp
template <ranges::input_range R, class Pred>
constexpr auto my::none_of_2(R&& r, Pred pred) -> bool {
    if consteval {
        auto first = ranges::begin(r);
        auto last = ranges::end(r);
        for (; first != last; ++first) {
            if (pred(*first)) {
                return false;
            }
        }
        return true;
    } else {
        auto first = ranges::begin(r);
        auto last = ranges::end(r);
        for (; first != last; ++first) {
            if (pred(*first)) {
                return false;
            }
        }
        return true;
    }
}
```
:::

This looks facially ridiculous, but it's not completely without a point. Any code in this function that _directly_ contains an immediate call is safe to evaluate at compile time, because we already know that we're evaluating it at compile time.

This approach isn't just true of `none-of`, it's similarly true of any `constexpr` function or function template. There's a simple replacement that we can do:

::: cmptable
### Status quo function body
```cpp
{
    E;
}
```

### Multi-colored function body
```cpp
{
    if consteval {
        E;
    } else {
        E;
    }
}
```
:::

I wouldn't want to go through and just rewrite all of my code everywhere to look like this. I want the language to do it for me. If it did, then the simple lambda just works:

<table>
<tr><th>Attempt</th><th>Result with the above change</th></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) {
        return std::meta::is_invalid(i);
    }));
```
</td><td>❌. Ill-formed per [expr.const]{.sref}/13. Still.</td></tr>
</table>

Just kidding.

The simple rewrite doesn't cut it because we'd rewrite the lambda to:

::: bq
```cpp
[](std::meta::info i) {
    if consteval {
        // this part is fine
        return std::meta::is_invalid(i);
    } else {
        // this part is not
        return std::meta::is_invalid(i);
    }
}
```
:::

But this is still possible to resolve. We just need two different resolutions:

* If the function contains a direct, unguarded immediate invocation that is never a constant expression, then the function becomes a `consteval` function.
* Otherwise, the function is treated as if it were written as `if consteval { E; } else { E; }`

Meaning that the lambda `[](std::meta::info i) { return std::meta::is_invalid(i); }` is treated as if it were declared `consteval`, just without the user having to do it. Since it basically already is: this lambda isn't invocable at runtime anyway, such a rule change would actually allow it to be invoked at compile time (rather than making the whole program ill-formed).

This decision could be made on function template directly. If a function template contained a non-dependent call to a `consteval` function (template), then you can know at that point that the function template needs to become `consteval`. But a dependent call could be `consteval` or not depending on what actually call is made, so the decision generally needs to be pushed to instantiation time.

And with that, finally:

<table>
<tr><th>Attempt</th><th>Result with the above change</th></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) {
        return std::meta::is_invalid(i);
    }));
```
</td><td>✅. The lambda becomes implicitly `consteval`, which also makes this instantiation of `ranges::none_of` implicitly `consteval`. Everything else just works.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    [](std::meta::info i) consteval {
        return std::meta::is_invalid(i);
    }));
```
</td><td>✅. The lambda is explicitly `consteval`, which likewise also makes this instantiation of `ranges::none_of` implicitly `consteval`. Everything else just works.</td></tr>
<tr><td>
```cpp
static_assert(std::ranges::none_of(
    types,
    std::meta::is_invalid
));
```
</td><td>❌. Still bad based on the library wording, but this case still isn't completely covered by this design approach thus far. The call to `is_invalid` from the `none_of` instantiation wouldn't be an immediate call, since `consteval` isn't part of the type. If it were though, then we'd satisfy the [expr.prim.id.general] rule since `none_of` would become `consteval` and thus we'd only be naming `is_invalid` in an immediate invocation.
</table>

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
