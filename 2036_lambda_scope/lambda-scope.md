---
title: "Change scope of lambda _trailing-return-type_"
document: P2036R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

There's a surprising aspect to the way that name lookup works in lambdas: it
behaves differently in the _trailing-return-type_ than it does in the lambda body.
Consider the simple lambda implementing a counter:

```cpp
auto counter1 = [j=0]() mutable -> decltype(j) {
	return j++;
};
```

The `decltype(j)` here is pointless (the deduced return type would be the
same), but the real issue here is that it does not actually compile. That's
because the variable `j` we're "declaring" in the init-capture isn't actually
"visible" yet (I'm using these terms somewhat loosely). The `j` in the body
refers to the lambda's "member" `j`, but the `j` in the _trailing-return-type_
needs to find some outer `j` instead. Despite the capture being lexically
closer to the lambda itself, and certainly far more likely to be the
programmer's intended meaning.

The best case scenario is that such code does not compile. The worst case
scenario is that it does - because when it does compile, it means we had a
situation like this:

```cpp
double j = 42.0;
// ...
auto counter2 = [j=0]() mutable -> decltype(j) {
	return j++;
};
```

And now our lambda returns a `double` instead of an `int`.

This problem is most clear with _init-capture_, where we may actually be
introducing new names. But it can show up in far more subtle ways with normal
copy capture:

```cpp
template <typename T> int bar(int&, T&&);        // #1
template <typename T> void bar(int const&, T&&); // #2

int i;
auto f = [=](auto&& x) -> decltype(bar(i, x)) {
    return bar(i, x);
}
f(42); // error
```

Here, in the _trailing-return-type_, `x` refers to the parameter of the lambda,
but `i` doesn't refer to the lambda's member (the lexically closest thing,
declared implicitly via the `[=]`) but actually refers to the block scope
variable, `i`. These are both `int`s, but the outer one is a mutable `int`
while within the call operator of the lambda, is a `const int` (because the
call operator is implicitly `const`). Hence the _trailing-return-type_ gets
deduced as `int` (via `#1`) while the expression in the body has type `void`
(via `#2`). This doesn't compile.

For the _trailing-return-type_ case, this problem only surfaces with
_init-capture_ (which can introduce new names) and any kind of copy capture
(which may change the const qualification on some names). With reference
capture (specifically either just `[&]` or `[&a]`), both the inner and outer
uses of names are equivalent so there is no issue. 

While it is possible (and quite easy) to produce examples that demonstrate this
sort of different behavior, it's quite difficult to come up with examples in
which this difference is actually desired and intended. I wrote a clang-tidy
check to find any uses of problematic captures (those that are come from a copy
capture or _init-capture_) and ran it on multiple code bases and could not find
one. I would love to see a real world example. 

This issue (the potentially-different interpretations of the same name in the
_trailing-return-type_ and lambda body) was one of (but not the only) reason
that [@P0573R2] was rejected. Consider this equivalent formulation of the
earlier example, but with the abbreviated lambda:

```cpp
template <typename T> int bar(int&, T&&);        // #1
template <typename T> void bar(int const&, T&&); // #2

int i;
auto f = [=](auto&& x) => bar(i, x);
f(42); // still error
```

Here, we still error, for all the same reasons, because this lambda is defined
to be equivalent to the previous one. But here, we only have one single `bar(i,
x)` expression which nevertheless is interpreted two different ways.

As pointed out in that paper, it is quite common for users to "hack" this kind
of lambda expression by using a macro that does the de-duplication for them.
Such lambdas are broken if they use any kind of copy or init-capture. Or, more
likely, somebody tried to write such a lambda, became confused when it didn't
compile, flipped over a table, and then wrote it the long way.

This is one of those incredibly subtle aspects of the language today that are
just needlessly confounding. It seems to me that whenever the meaning of an
_id-expression_ differs between the two contexts, it's a bug. I think we should
just remove this corner case. 

# Lookahead

One of the problems with trying to change these rules and say that the
_trailing-return-type_ refers to the capture, is that we might not necessarily know
what all of its captures are and know what the types might refer to.

Consider something related to the motivating case:

```cpp
int i;
auto f = [=](auto&& x) -> decltype(bar(i, x)) {
```

At this point, we do not yet know if `i` is captured or not, so we do not yet
know if the expression `i` is `const` or not. But parsing the _trailing-return-type_
isn't something we necessarily need up front. Is delaying parsing this until
we know a problem? 

This particular issue surface _only_ with the _default-capture_ `=`. In all other
cases we either know that we're capturing something already (both the 
_simple-capture_ and _init-capture_ cases are clear by this point) or we may
not be capturing but there's no difference (as with the _default-capture_ `&` case).

With the rest of the _lambda-declarator_, there's really no difference:

```cpp
int i;
auto f = [=](decltype(i) a, decltype((i)) b)
```

This is a lambda that takes an `int` and an `int&`, regardless of whether it's
const or mutable, regardless of whether `i` is captured. This would be consistent
with class types. 


# Proposal

Effectively, the model we have today is that the lambda `f` behaves as if it
were this type:

```cpp
int i;
struct F {
	// lambda-declarator, then lambda body
    auto operator()(auto&& x) const -> decltype(bar(i, x)) {
		return bar(i, x);
	}
	
	// ... then captures
	int i;
};
```

I propose that this should behave as if it were this type:

```cpp
int i;
struct F2 {
	// captures, then...
	int i;

	// lambda-declarator, then lambda body
    auto operator()(auto&& x) const -> decltype(bar(i, x)) {
		return bar(i, x);
	}
};
```

That is, the captures are themselves in scope for all name lookup in the rest
of the lambda. Note that this applies for the parameters as well, following the
principle of least surprise.

```cpp
double j = 42.0;

// Today: 'x' is a double
// Proposed: 'x' is an int
auto g = [j=0](decltype(j) x) { /* ... */ };
```

Such a change fixes the lambda in a way that almost certainly matches user
intent, fixes the `counter` lambdas presented earlier, and fixes all current
and future lambdas that use a macro to de-duplicate the _trailing-return-type_
from the body.
