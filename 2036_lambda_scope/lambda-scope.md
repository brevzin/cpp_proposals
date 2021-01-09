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

## Parts of a Lambda

If we write out a lambda that has all the parts that it can have, they would be in the following order (most of these are optional):

1. _lambda-introducer_
2. _template-parameter-list_
3. _requires-clause_ (#1)
4. _lambda-declarator_
    a. _parameter-declaration-clause_
    b. _decl-specifier-seq_
    c. _noexcept-specifier_
    d. _attribute-specifier-seq_
    e. _trailing-return-type_
    f. _requires-clause_ (#2)
5. _compound-statement_

If we have a copy capture (whether it's a _simple-capture_ or a _capture-default_ of `=` or an _init-capture_ that isn't a reference), the issue is we do not know what the type of a capture should be until we've seen whether the lambda is `mutable` or not (in the _decl-specifier-seq_).

What do we want to do about a case like this?

```cpp
double x;
[x=1](decltype((x)) y){ return x; }
```

There are four options for what this lambda could mean:

1. this is a lambda that takes a `double&` (status quo).
2. this is a lambda that takes an `int&` (lookup could be changed to find the _init-capture_ but not do any member access transformation - even though this lambda ends up being not `mutable`)
3. this is a lambda that takes an `int const&` (would require lookahead, highly undesirable)
4. this is ill-formed

While there's a lot of motivation for the _trailing-return-type_, I have never seen anybody write this and do not know what the motivation for such a thing would be. (1) isn't very reasonable since the _init-capture_ is lexically closer to use and it's just as surprising to find `::x` in the _parameter-declaration-clause_ as it is in the _trailing-return-type_.

The advantage of (4) is that it guarantees that all uses of `x` in the _lambda-expression_ after the _lambda-introducer_ mean the same thing &mdash; we reject the cases up front where we are not sure what answer to give without doing lookahead. If motivation arises in the future for using captures in these contexts, we can always change the lookup in these contexts to allow such uses &mdash; rejecting now doesn't cut off that path. 

This paper proposes (4).

Note that there are potentially _two_ different *requires-clause*s in a lambda: one that is before the _decl-specifier-seq_ and one that is after. Using a capture would be ill-formed in one but valid in the other:

```cpp
double x;
[x=1]
    <decltype(x)* p> // ill-formed
    requires requires {
        *p = x;      // ill-formed
    }
    (decltype(x) q)  // ill-formed
    // now we know x is an lvalue of type int const
    noexcept(noexcept(q+x))     // ok
    -> decltype(q+x)            // ok
    requires requires { q+x; }  // ok
    {
        return q+x;             // ok
    }
```

The status quo today is that all uses here are valid, and all of them save for the last one find `::x` (the `double`) &mdash; only in the lambda's _compound-statement_ does lookup find the _init-capture_ `x` (the `int`).

## odr-used when not odr-usable

Davis Herring provides the following example:

```cpp
constexpr int read(const int &i) {return i;}

auto f() {
    constexpr int value=3;
    return [=]() -> int(*)[read(value)] {
        static int x[read(value)];
        return &x;
    };
}
```

Today, this example is ill-formed (although no compiler diagnoses it) because `value` is odr-used in the _trailing-return-type_, but it is not odr-usable ([basic.def.odr]{.sref}/9) there. It would be consistent with the theme of this paper (having the _trailing-return-type_ have the same meaning as the body) to change the rules to allow this case. Such a rule change would involve extending the reach of odr-usable to include more of the parts of the lambda (but not default arguments) but making sure to narrow the capture rules (which currently are based on odr-usable) to ensure that we don't start capturing more things. 

I'm wary of such a change because I'm very wary of touching anything related to ODR. Especially because in an example like this, we could easily make `value` not odr-used here (either by making `value` `static` or by changing `read` to not take by reference).

# Wording

This wording is based on the working draft after Davis Herring's opus [@P1787R6] was merged (i.e. [@N4878]).

The wording strategy here is as follows. We have the following scopes today:

- _lambda-introducer_ 
- _template-parameter-list_
- _requires-clause_ (#1)
- _lambda-declarator_
    - _parameter-declaration-clause_ (function parameter scope)
    - _decl-specifier-seq_
    - _noexcept-specifier_
    - _attribute-specifier-seq_
    - _trailing-return-type_
    - _requires-clause_ (#2)
        - _compound-statement_ (block scope)

We have to move the _init-capture_ to inhabit the function parameter scope, making sure to still reject cases like:

* `[x=1](int x){}` (currently rejected by [basic.scope.block]{.sref}/2, the _init-capture_ targets the _compound-statement_ and the function parameter targets the parent of that)
* `[x=1]{ int x; }` (currently rejected by [basic.scope.scope]{.sref}/4, the two declarations of `x` potentially conflict in the same scope)

We then have to change the [expr.prim.id.unqual] rule such that if an _unqualified-id_ names a local entity from a point `S` within a lambda-expression, we first consider the point `S'` that is within the _compound-statement_ of that innermost lambda. If, from `S'`, some intervening lambda (not necessary the innermost lambda from `S'`) would capture the local entity by copy then:

- if `S` is in that innermost capturing lambda's function parameter scope but not in the _parameter-declaration-clause_, then we do the class member access transformation.
- otherwise, we say the access is ill-formed.


To clarify:
```cpp
int x;
[=]<decltype(x)* p)>  // error: unqualified-id names a local entity that would be captured by copy
                      // but not from the function parameter scope
    (decltype(x) y)   // error: unqualified-id names a local entity that would be captured by copy
                      // from within the function parameter scope, but it's in the @_parameter-declaration-clause_@
    -> decltype((x))  // ok: unqualified-id names a local entity that would be captured by copy
                      // in the function parameter scope, transformed into class access. Yields int const&.
{
        return x;     // ok: lvalue of type int const
};

int j;
[=](){
    []<decltype(j)* q> // ok: the innermost lambda that would capture j by copy is the outer lambda
                       // and we are in the outer's lambda's function parameter scope, this is int*
    (decltype((j)) w)  // ok: as above, 'w' is a parameter of type int const&
    {};
};
```
 

Change [expr.prim.id.unqual]{.sref}/3 as described earlier. It currently reads:

::: bq
[3]{.pnum} The result is the entity denoted by the _unqualified-id_ ([basic.lookup.unqual]). If the entity is a local entity and naming it from outside of an unevaluated operand within the scope where the _unqualified-id_ appears would result in some intervening _lambda-expression_ capturing it by copy ([expr.prim.lambda.capture]), the type of the expression is the type of a class member access expression ([expr.ref]) naming the non-static data member that would be declared for such a capture in the closure object of the innermost such intervening _lambda-expression_.

Otherwise, the type of the expression is the type of the result.
:::

Change it to instead read (I'm trying to add bullets and parentheses to make it clear what branch each case refers to), and as a drive by fix the issue Tim Song pointed out [here](https://lists.isocpp.org/core/2020/10/9982.php):

::: bq
[3]{.pnum} The result is the entity denoted by the _unqualified-id_ ([basic.lookup.unqual]). If the entity is either a local entity or names an _init-capture_ and
the _unqualified-id_ appears in a _lambda-expression_ at program point `P`, then let `S` be _compound-expression_ of the innermost enclosing _lambda-expression_ of `P`.

If naming the local entity or _init-capture_ from outside of an unevaluated operand in `S` would refer to an entity captured by copy in some intervening _lambda-expression_ ([expr.prim.lambda.capture]), then let `E` be the innermost such intervening _lambda-expression_.

- [3.1]{.pnum} If `P` is in `E`'s function parameter scope but not its _parameter-declaration-clause_, then the type of the expression is the type of the class member access expression ([expr.ref]) naming the non-static data member that would be declared for such a capture in the closure object of `E`. 
- [3.2]{.pnum} Otherwise (if `P` either precedes `E`'s function parameter scope or is in `E`'s _parameter-declaration-clause_), the program is ill-formed.

Otherwise (if there is no such _lambda-expression_ `E` or the entity is either not local or does not name an _init-capture_), the type of the expression is the type of the result.
:::

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
    
+   [=]<decltype(x) P>{};         // error: x refers to local entity but precedes the
+                                 // lambda's function parameter scope
+   [=](decltype((x)) y){};       // error: x refers to local entity but is in lambda's
+                                 // parameter-declaration-clause
+   [=]{
+       []<decltype(x) P>{};      // ok: x is in the outer lambda's function parameter scope
+       [](decltype((x)) y){};    // ok: lambda takes a parameter of type float const&
+   };
  }
```
*- end example*]
:::

Change [expr.prim.lambda.capture]{.sref}/6:

::: bq
[6]{.pnum} An _init-capture_ inhabits the [function parameter]{.addu} scope of the _lambda-expression_’s [_compound-statement_]{.rm} [_parameter-declaration-clause_]{.addu}. An _init-capture_ without ellipsis behaves as if it declares and explicitly captures a variable of the form [...]
:::

And extend the example to demonstrate this usage (now we do have an `i` in scope for `decltype(i)` to find):

::: bq
```diff
  int x = 4;
  auto y = [&r = x, x = x+1]()->int {
              r += 2;
              return x+2;
           }();                                    // Updates ​::​x to 6, and initializes y to 7.
           
  auto z = [a = 42](int a) { return 1; };          // error: parameter and local variable have the same name
  
+ auto counter = [i=0]() mutable -> decltype(i) {  // ok: returns int
+   return i++;
+ };
```

Our earlier bad examples of _init-capture_ should still be rejected:

- `[x=1](int x){}` is now rejected by [basic.scope.scope]{.sref}/4, since we know have two declarations of `x` in the function parameter scope of the lambda.
- `[x=1]{ int x; }` is now rejected by [basic.scope.block]{.sref}/2, since the declaration `int x` targets the block scope of the _compound-statement_ of the lambda and `x=1` is a declaration whose target scope is the function parameter scope, the parent of that _compound-statement_.

Basically, we've just swapped which rule rejects which example, but both examples are still rejected.

:::

# Acknowledgements

Thanks to Davis Herring for all of his work, just in general. Thanks to Tim Song for help understand the rules.