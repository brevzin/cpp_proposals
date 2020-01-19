---
title: "Function parameter constraints are too fragile"
document: D2089R0
date: today
audience: SG7
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

[@P1733R0] introduced the idea of function parameter constraints, which was then
elaborated upon and extended in [@P2049R0]. The initial example demonstrating
the feature at hand is:

```cpp
double pow( double base, int iexp );
double pow( double base, int iexp ) requires (iexp == 2);// proposed
```

But whose real motivation is to allow:

```cpp
namespace std::meta {
    struct class_info {
        consteval class_info(info x) requires is_class(x);
    };
}

constexpr std::meta::class_info c = reflexpr(some_class); // proposed ok
```

That is, to allow a rich type hierarchy for reflection while still getting
all the benefits that the monotype `info` API is able to provde.

The way this is intended to work, from the initial paper, is described as:

::: quote
Overload resolution in C++ happens at compile time,_not_ run time, so how could this ever work?
Considerthe call to `pow` in the following function:

```cpp
void f(double in) {
    in += 5.0;
    double d = pow(in, 2);
    // ...
}
```

Here the compiler knows _at compile time_ that the second argument to `pow` is
`2` so it can theoretically make use of the overload with the parameter constraint.
In what other cases does the compiler know at compile time the value of a parameter?
As it turns out, we already have standardese for such an argument
(or generally an expression) in C++: _constant expression_.

In short, this concepts extension will allow for parameter identifiers to appear in requires clauses and duringoverload resolution:

* if the argument is a _constant expression_ it is evaluated as part of evaluation of the requires clause, and
* if the argument is _not_ a _constant expression_ the entire overload is discarded.
:::


# Problems

I have some serious concerns that this direction is viable, that I would like
to express here. 

## Ephemerality 

The fundamental problem is that whether or not an expression is a _constant 
expression_ is an ephemeral property of an expression. It vanishes right away.
Relying on an expression being a constant expression is going to prevent a whole
class of abstractions using normal programming models.

Consider again the code we want to work with this feature. This can work fine:

```cpp
constexpr std::meta::class_info c = reflexpr(some_class);
```

Because `reflexpr(some_class)` is a constant expression. Indeed, even this can
work fine:

```cpp
constexpr std::meta::info i = reflexpr(some_class)
constexpr std::meta::class_info c = i;
```

Because `i` is almost a constant expression. But what happens when we try to use
other library features:

```cpp
std::vector<std::meta::class_info> classes;
classes.push_back(reflexpr(some_class));    // ok
classes.emplace_back(reflexpr(some_class)); // error
```

`push_back` succeeds because it takes a `class_info&&`, so the conversion happens
while our expression is still a constant expression. But `emplace_back` fails
because it deduces its parameter to `info&&` and has to perform the construction
of `class_info` internally, at which point our object is no longer a constant
expression. 

The general problem here is that the conversion has to happen _right away_.

## Type-based overload resolution

The proposal at hand introduces the notion of value-based overloading, but
everything else in the language and library only ever deal with type-based
overloading. 

What would `constructible_from<meta::class_info, meta::info>` yield? By the rules
laid out in these papers, it would yield `false`. Except sometimes, it actually
is constructible - but only from specific values, and only in specific situations.

Consider:

```cpp
template <std::meta::class_info C> struct X { };
template <auto I> requires is_class(C) struct Y { };
template <convertible_to<meta::class_info> auto I> struct Z { };

X<reflexpr(some_class)> x; // ok
Y<reflexpr(some_class)> y; // ok
Z<reflexpr(some_class)> z; // error, probably?
```

Dealing with these properly types would end up requiring their own little
shadow library. 

## Function parameters aren't constant expressions except when they are

Everyone trying to do something during constant evaluation will eventually try
to do something to the effect of:

```cpp
constexpr int foo(int i) {
    // or any other code which requires i to
    // be a constant expression
    static_assert(i >= 0);
    // ...
}

foo(42);
```
 
And be surprised that this fails, even though the function is `constexpr`, even
though the argument is a constant expression. And so we have to repeat the
mantra over and over that function parameters are never constant expressions.
Function parameters are never constant expressions.

Except, suddenly, with this paper, they can be. But only in a `requires` clause.
This adds more wrinkles into an already very-complex model that just makes it
harder to understand. 

# Conclusion

While function parameter constraints is a creative and interesting compromise
to trying to have both a monotype and a rich class hierarchy, I think it has
serious problems. I think the  programming model that it ends up introducing
isn't well supported in C++ and would just lead to a ton more confusion.

I am not sure that these problems are solvable without much more involved
language changes, so I think in light of wanting reflection sooner rather than
later, I think we should reconsider the direction of constrained function
parameters.
