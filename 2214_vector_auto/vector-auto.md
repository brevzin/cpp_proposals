---
title: "`vector<auto>`"
document: P2214R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

The Concepts TS [@N4674] was mostly merged into C++20, with many small changes,
but there was one aspect of the original TS that fell through the cracks. When
the TS was originally merged [@P0734R0], the abbreviated function syntax was
removed. A new abbreviated function syntax was eventually adopted as part of
[@P1141R2]. But while the latter paper re-added support for an abbreviated
syntax as part of variables and function templates, it missed one aspect of
the original TS - in fact it didn't even mention it at all. And that was the 
ability to use placeholders in template arguments. As in:

```cpp
std::vector<auto> v = f();
std::tuple<std::regular auto...> t = g();

void h(std::optional<std::convertible_to<int> auto>);
```

All of these declarations are ill-formed in C++20, yet all are very useful. We
should add them (back) for C++23. 

## Why not CTAD?

It may seem at first that, at least for the variable declarations, this is
already doable with CTAD:

```cpp
std::vector v = f();
std::tuple t = g();
```

But CTAD would be a poor choice for wanting to constrain these variables like
this - because CTAD is about creating new objects, not constraining. The
above doesn't actually even work, since this will happily compile if `g` returns
an `int` or a `std::pair<int, int>`, which is very different behavior from what
`std::tuple<auto...>` would do in this situation.

So really, CTAD would only solve the variable constraint problem if there's no
applicable constructor. 

## Can't we do this already?

What you _could_ do today, that would unconditionally have the desired behavior,
is write a concept such that:

```cpp
specializes<std::vector> auto v = f();
specializes<std::tuple> auto t = g();
```

But while this works for this case, it's arguably more cumbersome and less clear
to read, but also much more limited in functionality. It doesn't offer the
ability to do something like... constrain your `tuple` on being all references:

```cpp
std::tuple<auto&...> = g();
```

Or having at least 1 element:

```cpp
std::tuple<auto, auto...> = g();
```

Or being all regular:

```cpp
std::tuple<std::regular auto...> = g();
```

The status quo is that for the very simplest case, we can basically handle it.
But not always, since there's no way to write a constrained declaration for a
`std::array`. To be fair, there wouldn't always be one with this feature either,
but if you knew the size up front, you could at least:

```cpp
std::array<auto, 10> = h();
```

# Proposal

Currently, [dcl.spec.auto]{.sref} reads (slightly abbreviated):

::: bq
[2]{.pnum} A _placeholder-type-specifier_ of the form _type-constraint_~opt~ `auto` can be used as a _decl-specifier_ of the _decl-specifier-seq_ of a _parameter-declaration_ of a function declaration or _lambda-expression_ and, if it is not the `auto` _type-specifier_ introducing a _trailing-return-type_ (see below), is a generic parameter type placeholder of the function declaration or _lambda-expression_. [...]

[3]{.pnum} The placeholder type can appear with a function declarator in the _decl-specifier-seq_, _type-specifier-seq_, _conversion-function-id_, or _trailing-return-type_, in any context where such a declarator is valid. [...]

[4]{.pnum} The type of a variable declared using a placeholder type is deduced from its initializer.
This use is allowed in an initializing declaration ([dcl.init]) of a variable.
The placeholder type shall appear as one of the _decl-specifiers_ in the _decl-specifier-seq_ [...] 
:::

This proposal extends the use of placeholders in variables following the wording
from the Concepts TS:

::: bq
A placeholder can appear anywhere in the declared type of a variable, but
`decltype(auto)` shall appear only as one of the _decl-specifiers_ of the
_decl-specifier-seq_.
:::

With wording adjustments as appropriate such that `[](std::vector<auto>){}` is
a generic lambda and so forth. Basically, this proposal is to remove the
restriction on where a _placeholder-type-specifier_ can be used - allowing it
to be used in place of any _type-specifier_.

## Pack of Placeholders

The paper also proposes allowing a pack of _placeholder-type-specifier_
when used as a template argument, as in:

```cpp
std::tuple<auto...> x = f();
```

This works the same way as any other constrained declaration - we synthesize
a corresponding function template and ensure that deduction works:

```cpp
template <typename... T>
void __does_it_deduce(std::tuple<T...>);

__does_it_deduce(f());
```

Notably this doesn't require any other changes to pack machinery, since this
particular pack only participates in ensuring that deduction succeeds - it's
not otherwise a pack that can be used in any kind of expression. So the fact that
this pack can appear outside of templates should not cause any issues.

## Use in _nested-name-specifier_

The Concepts TS also allowed the use of a placeholder in a _nested-name-specifier_.
From the example in the paper, adjusted for C++20 syntax:

```cpp
template <typename T> concept C = sizeof(T) == sizeof(int);
template <int N> concept D = true;    

struct S1 { int n; };
struct S2 { char c; };
struct S3 { struct X { using Y = int; }; };

int auto::* p1 = &S1::n; // auto deduced as S1
int D::* p2 = &S1::n;    // error: D does not designate a placeholder type
int C::* p3 = &S1::n;    // OK: Cdeduced as S1
char C::* p4 = &S2::c;   // error: deduction fails because constraints are not satisfied

void f(typename auto::X::Y);
f(S1());  // error: auto cannot be deduced from S1()
f<S3>(0); // OK
```

This paper proposes adopting this part of the TS as well.

## Other considerations

At a high level, a type like `vector<auto>` is a concept. It constrains
a declaration. But like real concepts, it cannot be used directly to check against
a type.

```cpp
requires C<T>; // direct
requires (T t) { [](std::vector<auto> const&){}(t); } // not very direct
```

It would be very useful to come up with a syntax such that `vector<auto>` could
be checked more directly. Such a syntax could be useful to, for instance,
in [@P0798R4]'s `and_then` we need to constrain the result of `F` to be a
specialization of `optional`:

```cpp
template <typename F, typename U = std::invoke_result_t<F, T const&>>
    requires ???
auto and_then(F&&) const& -> U;
```

We want to say that `U` is some kind of `optional`. Which we could do with the
`specializes` concept I mentioned earlier (`requires specializes<U, std::optional>`)
but in the same way it'd be nice to avoid using that in variable declarations,
it'd also be nice to avoid using that in function template constraints. 

Perhaps in the same way that a _compound-requirement_ is spelled `{ e } -> C`
we could introduce `(T -> C)`, such that above becomes

```cpp
template <typename F, typename U = std::invoke_result_t<F, T const&>>
    requires (U -> std::optional<auto>)
auto and_then(F&&) const& -> U;
```


# Wording

The wording here largely already exists in [@N4674], it simply needs to be
updated based on the other changes that we've made since the TS (e.g.
_constrained-type-specifier_ is now _placeholder-type-specifier_, the
abbreviated function template syntax has changed, reusing the same concept
name now does independent deduction, etc.). 