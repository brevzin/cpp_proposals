---
title: "Change scope of lambda _trailing-return-type_"
document: D2036R1
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

Another example arises from trying to write a SFINAE-friendly function composer:

```cpp
template <typename F, typename G>
auto compose(F f, G g) {
    return [=](auto... args) -> decltype(f(g(args...))) {
        return f(g(args...));
    }
}
```

This implementation is buggy. The problem is the `f` and `g` from the body of
the lambda are accessed as `const`, but from the _trailing-return-type_ are not.
Pass in a callable that's intended to be non-`const`-invocable (like, say, a
`mutable` lambda), and we end up with a hard error when we finally instantiate
the body.

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
just remove this corner case. It's also blocking reasonable future language
evolution, and is likely a source of subtle bugs and preexisting user
frustration.  

# Potential Impact

Let's go through the various types of capture and see what the impact of this
proposed change would be on usage and implementation.

### No capture: `[]` {-}

There is no capture, so there is no new thing to find. No change.

### _init-capture_: `[a=expr]` or `[&a=expr]` {-}

By the time we get to the _trailing-return-type_, we know the types of all the
_init-capture_ and we know whether the lambda is `mutable` or not, which means
that we will know how to correctly interpret uses of `a` in the
_trailing-return-type_. This will likely change the meaning of such code, if
such code exists today. But note that such code seems fundamentally questionable
so it's unlikely that much such code exists today.

### _simple-capture_: `[b]`, `[&b]`, `[this]`, or `[*this]` {-}

This is basically the same result as the _init-capture_ case: we know the types
by the time we get to the beginning of the _trailing-return-type_, so there are
no issues determining what it should be. 

With the reference capture cases (as well the _init-capture_ spelling `[&a=a]`),
there is actually no difference in interpretation anyway. 

### _capture-default_ with `[&]` {-}

With reference captures, there is no difference in interpretation between
considered the capture and considering the outer scope variable. This paper
would change nothing.

### _capture-default_ with `[=]` {-}

This is the sad case. Specifically, in the case where:

1. We have a _capture-default_ of `=`, and
2. We have a _trailing-return-type_, and
3. That _trailing-return-type_ has an _id-expression_ which is not otherwise
covered by any other kind of capture, and
4. The use of that _id-expression_, if it appeared in the body, would be
affected by the rule in [expr.prim.id.unqual]{.sref}/3 (that is, it's not just
`decltype(x)` but has to be either `decltype((x))` or something like
`decltype(f(x))`), and
5. The lambda is not `mutable`, and
6. The variable is not `const`

Then we have a problem. First, let's go over the cases that are not problematic.


3. Eliminates cases like `[=, a]() -> decltype(f(a))`, which we know captures
`a` by copy so we can figure out what the type of `a` would be when nominated
in the body.
4. Eliminates cases like `[=]() -> X<decltype(a)>`, which actually have the
same meaning in the body already. 
5. Eliminates cases like `[=]() mutable -> decltype(f(a))`. Whether or not we
end up having to capture `a`, the meaning of `f(a)` is the same in the body
as it is in the _trailing-return-type_.
6. Eliminates cases like `[=]() -> decltype(g(c))` where `c` is, say, an
`int const&`. Whether or not we end up having to capture `c`, the meaning of
`g(c)` is the same in the body as it is in the _trailing-return-type_.

We're left with this pathological case:

```cpp
int i;
[=]() -> decltype(f(i))
```

At this point, we do not know if we're capturing `i` or not. Today, this
treats `i` as an lvalue of type `int` here. But with the proposed rule change,
this _might_ have to treat `i` as a `const` access, but only _if_ we end
up having to capture `i`:

```cpp
auto f(int&)       -> int;
auto f(int const&) -> double;

int i;

auto should_capture = [=]() -> decltype(f(i)) {
    return f(i);
};
auto should_not_capture = [=]() -> decltype(f(i)) {
    return 42;
};
```

Today, both lambdas return `int`. With the suggested change, the
_trailing-return-type_ needs to consider the capture, so we need to delay
parsing it until we see what the lambda bodies actually look like. And then,
we might determine that the lambda `should_capture` actually returns a `double`.

How can we handle this case?

1. We can, in this specific scenario (capture has an `=` and the lambda is
`const`) just treat the _trailing-return-type_ as token soup. The simplified
rules for capture aren't based on return type [@P0588R1] in any way, so this
can work.
2. We can, in this specific scenario, just say that `i` is captured when used
this way and that if it would not have been captured following the usual rules
that the lambda is ill-formed.
3. We can say generally that any capturable entity in the _trailing-return-type_
will behave as if it's captured (regardless of if it ends up being captured
or not).

This paper suggests option 3. As with the rest of this paper, it is easy
to come up with examples where the rules would change. Lambdas like the following
would change meaning:

```cpp
int i;
// previously returned int&, proposed returns int const&
// even though i is not actually captured in this lambda
auto f = [=](int& j) -> decltype((i)) {
    return j;
};
```

But it is difficult to come up with actual real-world examples that would break.
And easy to come up with real-world examples that would be fixed by this change.
The lambda `should_capture` would change to return a `double`, which seems
more likely to be correct, and much more realistic an example than `f`.

# Proposal

This paper proposes that name lookup in the _trailing-return-type_ of a lambda
first consider that lambda's captures before looking further outward. We may not
know at the time of parsing the return type which names actually are captured,
so this paper proposes to treat all capturable entities as if they were captured.

That is, treat the _trailing-return-type_ like the function body rather than
treating it like a function parameter.

Such a change fixes the lambda in a way that almost certainly matches user
intent, fixes the `counter` and `compose` lambdas presented earlier, and fixes
all current and future lambdas that use a macro to de-duplicate the
_trailing-return-type_ from the body.

For the pathologically bad case (the use of a name in
a _trailing-return-type_ of a `const` lambda that nominates a non-`const`
variable not otherwise accounted for in other lambda capture) that means we
might have a lambda where we treat a name as captured when it might end up not
actually having been captured - which would be a mistreatment in the opposite
direction of the problem that this paper has been describing. This is
unfortunate, but it's an especially strange corner case - one that's much
more unlikely to appear in real code than the cases that this paper is trying
to resolve.

# Wording

This wording is based on the working draft after Davis Herring's opus [@P1787R6] was merged.

Extend the example in [expr.prim.id.unqual]{.sref}/3 to demonstrate this rule:

::: bq
[*Example 1:*
```diff
  void f() {
    float x, &r = x;
-   [=] {
+   [=]() -> decltype((x)) {      // lambda returns float const& because this lambda
+                                 // is not mutable and x is an lvalue
      decltype(x) y1;             // y1 has type float
-     decltype((x)) y2 = y1;      // y2 has type float const& @[because this lambda]{.diffdel}@
-                                 // @[is not mutable and x is an lvalue]{.diffdel}@
+     decltype((x)) y2 = y1;      // y2 has type float const&
      decltype(r) r1 = y1;        // r1 has type float&
      decltype((r)) r2 = y2;      // r2 has type float const&
+     return y2;
    };
  }
```
*- end example*]
:::

Insert a new clause at the end of [expr.prim.lambda.general]{.sref}

::: bq
::: addu
[6]{.pnum} A _lambda-expression_ `E` introduces a _lambda scope_ that includes `E` and extends to the end of the _compound-statement_ in `E`. A lambda scope is a block scope.
:::
:::

Change [expr.prim.lambda.capture]{.sref}/6:

::: bq
[6]{.pnum} An _init-capture_ inhabits the [function parameter]{.addu} scope of the _lambda-expression_’s [_compound-statement_]{.rm} [_parameter-declaration-clause_]{.addu}. An _init-capture_ without ellipsis behaves as if it declares and explicitly captures a variable of the form [...]
:::

And adjust the example to demonstrate this usage:

::: bq
```diff
  int x = 4;
- auto y = [&r = x, x = x+1]()->@[int]{.diffdel}@ {
+ auto y = [&r = x, x = x+1]()->@[decltype(x)]{.diffins}@ {
              r += 2;
              return x+2;
           }();                               // Updates ​::​x to 6, and initializes y to 7.
           
  auto z = [a = 42](int a) { return 1; };     // error: parameter and local variable have the same name
```
:::

# Acknowledgements

Thanks to Davis Herring for all of his work, just in general. 