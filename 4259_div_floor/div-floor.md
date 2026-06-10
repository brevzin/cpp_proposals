---
title: "The name of the floor division function should have floor in it"
document: P4259R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
status: progress
---

# Introduction

In 1962, Kenneth Iverson introduced the names "floor" and "ceiling", and the syntax `⌊x⌋` and `⌈x⌉`, for operations that return the greatest integer less than or equal to `x` and the smallest integer greater than or equal to `x`, respectively. Since then, these names have become broadly used in math and programming.

Here is a list of programming languages and the name they give their functions for these operations:

| Language   | Greatest integer ≤ x | Smallest integer ≥ x |
| ---------- | -------------------- | -------------------- |
| C          | `floor`              | `ceil`               |
| C++        | `std::floor`         | `std::ceil`          |
| C#         | `Math.Floor`         | `Math.Ceiling`       |
| Java       | `Math.floor`         | `Math.ceil`          |
| Kotlin     | `floor`              | `ceil`               |
| Swift      | `floor`              | `ceil`               |
| Rust       | `f64::floor`         | `f64::ceil`          |
| Go         | `math.Floor`         | `math.Ceil`          |
| Python     | `math.floor`         | `math.ceil`          |
| JavaScript | `Math.floor`         | `Math.ceil`          |
| TypeScript | `Math.floor`         | `Math.ceil`          |
| Ruby       | `floor`              | `ceil`               |
| PHP        | `floor`              | `ceil`               |
| R          | `floor`              | `ceiling`            |
| Julia      | `floor`              | `ceil`               |
| Haskell    | `floor`              | `ceiling`            |
| OCaml      | `floor`              | `ceil`               |
| F#         | `floor`              | `ceil`               |
| Elixir     | `Float.floor`        | `Float.ceil`         |
| Erlang     | `math:floor`         | `math:ceil`          |
| Lua        | `math.floor`         | `math.ceil`          |
| MATLAB     | `floor`              | `ceil`               |
| Octave     | `floor`              | `ceil`               |
| Fortran    | `floor`              | `ceiling`            |
| Clojure    | `math/floor`         | `math/ceil`          |
| OCaml      | `Float.floor`        | `Float.ceil`         |
| Excel      | `FLOOR`              | `CEILING`            |

There simply is not a lot of diversity in the names of these functions. But that's for unary floating point/decimal operations. Many languages also offer binary integer functions that return the mathematical results of `⌊x/y⌋` and `⌈x/y⌉`. There is a bit more diversity to those operations:

| Language    | `⌊x/y⌋` | `⌈x/y⌉` |
|-|--|--|
| Rust        | `x.div_floor(y)` | `x.div_ceil(y)` |
| Zig         | `@divFloor(x, y)` | `@divCeil(x, y)` |
| MATLAB      | `idivide(x, y, "floor")` | `idivide(x, y, "ceil")` |
| Racket      | `(floor-quotient x y)` | `(ceiling-quotient x y)` |
| Clojure     | `(floor-div x y)` | n/a |
| Java        | `Math.floorDiv(x, y)` | `Math.ceilDiv(x, y)` |
| Common LISP | `(floor x y)` | `(ceiling x y)` |
| Nim         | `floorDiv(x, y)` | `ceilDiv(x, y)` |
| Ruby        | `x.div(y)` or `x/y` | `x.ceildiv(y)` |
| Scala       | `Math.floorDiv(x, y)` | `Math.ceilDiv(x, y)` |
| Elixir      | `Integer.floor_div(x, y)` | `Integer.ceil_div(x, y)` |
| Julia       | `div(x, y, RoundDown)` or `fld(x, y)` | `div(x, y, RoundUp)` or `cld(x, y)` |
| Swift       | `x.divided(by: y, rounding: .down)` | `x.divided(by: y, rounding: up)` |

Not all languages provide integer division functions of this form, some simply rely on their regular integer division operation for doing floor division (since in C++, `-5 / 2` is `-2` but in some languages it is `-3`). Nevertheless, there is still a great deal of uniformity in the API space here.

There are only two languages I've found which provide both operations and also do not use the word floor or ceiling in them, which I deliberately put last in the above table: both [Julia](https://docs.julialang.org/en/v1/base/math/#Base.div-Tuple{Any,%20Any,%20RoundingMode}) and [Swift Numerics](https://deepwiki.com/apple/swift-numerics/4.3-division-with-rounding) provide integer division as ternary functions that take a rounding mode, and in both cases the rounding mode for floor is spelled "down" while the rounding mode for ceiling is spelled "up." Nevertheless, Julia still provides a terser form that is simply `fld` and `cld`. Which makes Swift unique.

## The C++29 Proposal

Meanwhile, [@P3724R4]{.title} introduces a number of integer division functions with different rounding modes (10 rounding modes and Euclidean division). That proposal's names for the two rounding modes describe above are:

| Language    | `⌊x/y⌋` | `⌈x/y⌉` |
|-|--|--|
| P3724 | `std::div_to_neg_inf(x, y)` | `std::div_to_pos_inf(x, y)` |

That is extremely different from established practice.

The proposal's justification for these names is:

::: quote
While the use of names like floor and ceil is common in various domains, including in `<cmath>`, I do not believe we should perpetuate this design for integer division.

Anecdotally, during the LEWG telecon for P3724R3, there was visible confusion when the room was asked

> What does `div_floor` do for negative numbers?

While the answer may be obvious to a mathematician, that does not apply to everyone. The key benefit of the proposed naming scheme is that it is entirely self-documenting.

To provide some more rationale:

* The proposed scheme does not nicely extend to division with rounding away from zero; there is no established term for that.
* A hypothetical `std::div_round` (for rounding to the nearest integer) would be somewhat perplexing because all proposed functions round, just towards different targets. However, taking `std::div_floor` as a counterpart to `std::floor` strongly suggests that there should be a `std::div_round` function as a counterpart to `std::round`, which is not the case.

Regardless whether the functions end up called `std::div_floor` or `std::div_to_neg_inf`, the names should remain somewhat brief so they take up a reasonable amount of space in C++ expressions.
:::

## A Response to the Rationale

Indeed, the use of names like floor and ceil is common. Not just common, but all but ubiquitous, to the point where people will naturally look for these functions under those names. As in: "What is C++'s integer floor division function?"

The answer to what `div_floor` does for negative numbers is of course the same as it is for positive numbers: it gives the largest integer less than or equal to the quotient. It is difficult to accept the claim that this is hard for a non-mathematician to understand, while `div_to_neg_inf` is somehow easier, especially given the landscape that we find ourselves in. Additionally, `to_neg_inf` is appreciably more mathematically-oriented than `floor` (not to mention `down`).

That there is no established term for rounding away from zero (indeed as far as I know only Julia and Swift provide this sort of rounding, and both provide it in the ternary form where the rounding mode is an operand) does not mean that we should ignore the fact that there _is_ an established term for floor and ceiling division. And even for rounding away from zero, the paper's choice of naming is too terse. Both Swift and Julia call this this rounding _from_ zero, whereas the proposed name is just "away zero":

|Language|Integer Division, rounding away from zero|
|-|-|
|Julia|`div(x, y, RoundFromZero)`|
|Swift|`x.divided(by: y, rounding: .awayFromZero)`|
|Proposed C++|`std::div_away_zero(x, y)`|

I also don't find a hypothetical `div_round` all that perplexing either. Arguably that's the very clear analogue to the floating point function we already have, which makes it easily discoverable. After all, we have these four unary floating point operations, and the integer division functions conceptually do the same thing as a floating point division following by the particular specified rounding (of course without actually round-tripping through floating point types and entirely avoiding any potential rounding errors):

|Unary Floating Point|Binary Integer Division|
|-|-|
|`floor`|`div_floor`|
|`ceil`|`div_ceil`|
|`round`|`div_round`|
|`trunc`|`div_trunc`|

Although the last of these, `div_trunc(x, y)` — which in the paper is proposed as `div_to_zero(x, y)` — is probably the least interesting and unlikely to be used, since it is simply `x / y`.

## The Other Rounding Modes

For the less ubiquitous rounding modes, the names should prioritize clarity over minimizing the number of characters. To compare the names of the other functions proposed here to the rounding modes provided in Julia and Swift, I think the proposed names are the worst of the three:

|Julia|Swift|P3724|
|--|---|--|
|`div(x, y, RoundNearestTiesAway)`|`x.divided(by: y, rounding: toNearestOrAway)`|`div_ties_away_zero(x, y)`|
|`div(x, y, RoundNearestTiesUp)`|`x.divided(by: y, rounding: toNearestOrUp)`|`div_ties_to_pos_inf(x, y)`|
|`div(x, y, RoundToZero)`|`x.divided(by: y, rounding: towardZero)`|`div_to_zero(x, y)`|
|`div(x, y, RoundFromZero)`|`x.divided(by: y, rounding: awayFromZero)`|`div_away_zero(x, y)`|

There's probably a good argument that the common rounding modes (floor, ceiling) and Euclidean division merit their own named functions, while the rest can be handled via a rounding mode enumeration as in Julia ([7 options](https://docs.julialang.org/en/v1/base/math/#Base.div-Tuple%7BAny,%20Any,%20RoundingMode%7D)), Swift ([11 options](https://deepwiki.com/apple/swift-numerics/4.3-division-with-rounding)), and Matlab ([4 options](https://www.mathworks.com/help/matlab/ref/idivide.html)).

The paper's argument _against_ such a rounding mode function argument is:

::: quote
In virtually every case, the rounding mode for an integer division is a fixed choice. This is evidenced by the ten trillion existing uses of the / operator which always truncate. Also, the implementation of the proposed functions does not lend itself to a runtime parameter:

[...]

As can be seen, these implementations are substantially different.

[...]

If we now provided a runtime `std::rounding`, the obvious implementation would look like:

::: std
```cpp
enum struct rounding { to_zero, to_neg_inf, away_zero, /* ... */ };

int divide(int x, int y, rounding_mode mode) {
  switch (mode) {
  case rounding::to_zero:    return __div_to_zero(x);
  case rounding::to_neg_inf: return __div_to_neg_inf(x);
  case rounding::away_zero:  return __div_away_zero(x);
  // ...
  }
  std::unreachable();
}
```
:::

The user can trivially make such an enum class and switch themselves, if they actually need to. If they don't (which is likely), all we accomplish is making the user write `std::divide(std::rounding::to_neg_inf, x, y)` instead of `std::div_to_neg_inf(x, y)`.
:::

To start with, there are many claims here that I think are just obviously true: that the rounding mode is almost always a fixed choice, that the implementations of the different modes are substantively different, and that the likely implementation will be the `switch` presented here. But I don't actually think this is an argument against such a shape.

What we're actually doing here is parametrizing an algorithm — a rounding mode parameter allows us to clearly separate the algorithm (division) from the parameter (the rounding mode). Even if all of the different rounding mode approaches are consistently named as `div_`, having them have literally the same name is still an improvement.

Since the rounding mode will almost always be passed in as a constant — that `switch` implementation will [reliably optimize out](https://compiler-explorer.com/z/1xEdj9Gs3) (they have identical codegen even with `-Og`). And in the rare situations where the rounding mode is non-constant, having it be a parameter allows that use-case at all.

And the spelling of the mode allows for more straightforwardly readable spellings of the rarer rounding modes, which is I think a good win. Let's go through a few rounding modes, using Swift's naming approach.

Towards zero:

::: std
```cpp
x / y                                  // just the language operator
div_trunc(x, y)                        // following trunc()
div_to_zero(x, y)                      // P3724
div(x, y, rounding::towards_zero)      // with mode
```
:::

Away from zero:

::: std
```cpp
div_away_zero(x, y)                    // P3724
div(x, y, rounding::away_from_zero)    // with mode
```
:::

To the nearest integer, rounding ties up

::: std
```cpp
div_ties_to_pos_inf(x, y)              // P3724
div(x, y, rounding::to_nearest_or_up)  // with mode
```
:::

Of course, the ones that explicitly provide a rounding more are longer than the other options — although the relative margin of difference goes down if the variable names are longer than a single character. But the enum allows for, and arguably even forces, clearer names.

I think clearer names are more valuable than terser names for these functions — especially for these rarer used rounding modes.

# Proposal

The names `std::div_to_pos_inf` and `std::div_to_neg_inf` are very poor names. There is broadly established precedent for referring to these operations are ceiling and floor division, respectively. People looking for these operations will look for them _under those names_. The names should reflect that.

The less common rounding modes, I think we should seriously consider simply passing the rounding mode as a function argument. Having a rounding mode enumeration doesn't preclude explicit names for the more common operations, and there's existing practice for just that in Julia. At the very least, the names of several of the rounding modes should be reconsidered to read better. The proposed `div_away_zero` is worst of the names since it's just missing a preposition, but the family of nearest rounding rules aren't very well named either since they omit the "nearest" part in the name — you just have to know that `div_ties_to_even` has an implicit `to_nearest` in the name (e.g. Swift names this mode `toNearestOrEven`).