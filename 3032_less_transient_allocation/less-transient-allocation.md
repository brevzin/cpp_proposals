---
title: "Less transient constexpr allocation"
document: D3032R2
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

<style type="text/css">
div.std blockquote { color: #000000; background-color: #F1F1F1;
    border: 1px solid #D1D1D1;
    padding-left: 0.5em; padding-right: 0.5em; }

div.std.ins blockquote {
    color: #000000; background-color: #C8FFC8;
    border: 1px solid #B3EBB3;
  }

div.ins > div.example {
    color: #000000; background-color: #C8FFC8;
    border: 1px solid #B3EBB3;
  }

div.std div.sourceCode { background-color: inherit; margin-left: 1em; }

div.std blockquote del { text-decoration: line-through;
    color: #000000; background-color: #FFC8EB;
    border: none; }

code del { border: 1px solid #ECB3C7; }


</style>

# Revision History

Since [@P3032R1], fixed wording and extended the feature.

Since [@P3032R0], fixed wording, added feature-test macro.

# Introduction

C++20 introduced constexpr allocation, but in a limited form: any allocation must be deallocated during that constant evaluation.

The intent of the rule is that no constexpr allocation persists to runtime. For more on why we currently need to avoid that, see Jeff Snyder's [@P1974R0] and also [@P2670R1].

But the rule cited above does slightly more than prevent constexpr allocation to persist until runtime. The goal of this paper is to allow more examples of allocations that _do not_ persist until runtime, that nevertheless are still rejected by the C++23 rules.

For the purposes of this paper, we'll consider the example of wanting to get the number of enumerators of a given enumeration. While the specific example is using reflection ([@P2996R1]), there isn't anything particularly reflection-specific about the example - it just makes for a good example. All you need to know about reflection to understand the example is that `^E` gives you an object of type `std::meta::info` and that this function exists:

::: std
```cpp
namespace std::meta {
  using info = /* ... */;
  consteval vector<info> enumerators_of(info);
}
```
:::

With that, let's go through several attempts at trying to get the _number_ of enumerators of a given enumeration `E` as a constant:

<table>
<tr><th/><th>Attempt</th><th>Result</th></tr>
<tr><td style="text-align: center;vertical-align: middle">1</td><td>
```cpp
int main() {
    constexpr int r1 = enumerators_of(^E).size();
    return r1;
}
```
</td><td>✅. This one is valid - because `r1` is a `constexpr` variable, it's initializer starts a constant evaluation - which includes the entire expression.
The temporary `vector` is destroyed at the end of that expression, so it doesn't persist outside of any constant evaluation.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">2</td><td>
```cpp
constexpr int f2() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r2 = f2();
    return r2;
}
```
</td><td>❌. This one is invalid.

The same idea about initializing `r2` as mentioned in the previous example is valid - but because `f2` is a `constexpr` function, the invocation of `enumerators_of(^E)` is not in an immediate function context - so it needs to be a constant expression on its own. That is, we start a new constant evaluation within the original one - but this constant evaluation is just the expression `enumerators_of(^E)`. It is not the full expression `enumerators_of(^E).size()`.

As a result, the temporary vector returned by `enumerators_of(^E)` persists outside of its constant expression in order to invoke `.size()` on it, which is not allowed.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">3</td><td>
```cpp
consteval int f3() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r3 = f();
    return r3;
}
```
</td><td>✅. Both this row and the next row are subtle refinements of the second row that make it valid.

The only difference between this and the previous row is that `f2` was `constexpr` but `f3` is `consteval`. This distinction matters, because now `enumerators_of(^E)` is no longer an immediate invocation - it is now in an immediate function context. As a result, the only thing that matters is that the entire expression `enumerators_of(^E).size()` is constant - and the temporary `vector<info>` does not persist past that.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">4</td><td>
```cpp
template<class E> constexpr int f4() {
    return enumerators_of(^E).size();
}

int main() {
    constexpr int r4 = f4<E>();
    return r4;
}
```
</td><td>✅. Here `f4` is a `constexpr` function *template*, whereas `f2` was a regular `constexpr` function. This matters because of [@P2564R3] - the fact that `enumerators_of(^E).size()` isn't a constant expression now causes `f4` to become a `consteval` function template - and thus we're in the same state that we were in `f3`: it's not `enumerators_of(^E)` that needs to be a core constant expression but rather all of `enumerators_of(^E).size()`.
</td></tr>
<tr><td style="text-align: center;vertical-align: middle">5</td><td>
```cpp
consteval int f5() {
    constexpr auto es = enumerators_of(^E);
    return es.size();
}

int main() {
    constexpr int r5 = f();
    return r5;
}
```
</td><td>❌. Even though `f5` is `consteval`, we are still explicitly starting a new constant evaluation within `f5` by declaring `es` to be `constexpr`. That allocation persists past that declaration - *even though* it does not persist past `f5`, which by being `consteval` means that it does not persist until runtime.
</td></tr>
</table>

Three of these rows are valid C++23 programs (modulo the fact that they're using reflection), but `2` and `5` are invalid - albeit for different reasons:

* in the 2nd row, we are required to consider `enumerators_of(^E)` as a constant expression all by itself - even if `enumerators_of(^E).size()` is definitely a constant expression.
* in the 5th row, we are required to consider `es` as a non-transient constexpr allocation - even though it definitely does not persist until runtime, and thus does not actually cause any of the problems that non-transient constexpr allocation has to address.

## Immediate-escalating expressions

The wording in [@P2564R3] introduced the term *immediate-escalating expression* in [expr.const]{.sref}:

::: bq
[17]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and it is either

* [17.1]{.pnum} a potentially-evaluated *id-expression* that denotes an immediate function that is not a subexpression of an immediate invocation, or
* [17.2]{.pnum} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.
:::

In the second example:

::: std
```cpp
constexpr int f2() {
    return enumerators_of(^E).size();
}
```
:::

The expression `enumerators_of(^E)` is immediate-escalating - it is an immediate invocation (`enumerators_of` is a `consteval` function) that is not a constant expression (because the temporary vector persists outside of this expression). This is what causes `f4` to become a `consteval` function template.

But `enumerators_of(^E).size()` is not an immediate invocation (it simply has a subexpression that is an immediate invocation). However, if we were to define it as an immediate invocation  - then it would not be an immediate-escalating expression anymore because it is actually a constant expression. And that would be enough to fix this example (as well as `f4` which would then itself not escalate to `consteval` since it wouldn't need to).

Put differently: instead of escalating `enumerators_of(^E)` up to the nearest function, which we then try to make `consteval` (and fail in the case of `f2` because `constexpr` functions are not *immediate-escalating*), we only need to escalate up to the nearest enclosing expression that could be a constant expression.

## Transient allocations

The wording in [expr.const]{.sref} for rejecting non-transient allocations rejects an expression `E` as being a core constant expressions if `E` evaluates:

::: bq
* [5.18]{.pnum} a *new-expression* ([expr.new]), unless the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is deallocated within the evaluation of `E`;
* [5.20]{.pnum} a call to an instance of `std​::​allocator<T>​::​allocate` ([allocator.members]), unless the allocated storage is deallocated within the evaluation of `E`;
:::

That is - an allocation within `E` has to be transient to `E`. However, the rule we really want is that a constant allocation is transient to constant evaluation. In the fifth example:

::: std
```cpp
consteval int f5() {
    constexpr auto es = enumerators_of(^E);
    return es.size();
}
```
:::

The allocation in `enumerators_of(^E)` isn't transient to that expression, but it is definitely destroyed within `f5`, which is `consteval`. That's important: if `f5` were `constexpr`, we'd have access to that allocation at runtime.

We can loosen the restriction such that an allocation within `E` must be deallocated within `E` or, if `E` is in an immediate function context, the end of that context. This would be the end of the `if consteval { }` block or the end of the `consteval` function. Such a
loosening would allow `f5` above, but not if it's `constexpr`, and not if `es` were also declared `static`.

# Proposal

There are two separate potential changes here, that would each make one of the attempts above well-formed:

1. we could [escalate](#immediate-escalating-expressions) expressions to larger expressions, so that `enumerators_of(^E).size()` becomes a constant expression, or
2. we could extend the notation of [transient allocation](#transient-allocations) to include the full immediate context instead of just the constant evaluation

The second of these is straightforward to word and provides a lot of value - since now particularly in the context of reflection you can declare a `constexpr vector<info>` inside a `consteval` function and use those contents as a constant expression. The first of these is complicated to word and does not provide as much value, as it is a limitation that is fairly easy to work around: either declare a local `constexpr` variable, or change the function to be `consteval` or a template.

As such, this paper only proposes extending the notion of transience.

## Constant Expression vs Core Constant Expression

Right before plenary in Tokyo, Hubert Tong pointed out an important omission in the wording of this paper: it completely failed to solve the problem.

While the wording relaxes the rules for a *core constant expression*, it did not touch two other important rules: the definition of a *constant expression* and the requirements for the initialization of a `constexpr` variable.

Specifically, the existing rule in [dcl.constexpr]{.sref}/6 requires that:

::: std
[6]{.pnum} ... In any constexpr variable declaration, the full-expression of the initialization shall be a constant expression ([expr.const]).
:::

where the term "constant expression" is defined in [expr.const]{.sref}/14:

::: std
[14]{.pnum} A *constant expression* is either a glvalue core constant expression that refers to an entity that is a permitted result of a constant expression (as defined below), or a prvalue core constant expression whose value satisfies the following constraints:

* [14.#]{.pnum} if the value is an object of class type, each non-static data member of reference type refers to an entity that is a permitted result of a constant expression,
* [14.#]{.pnum} if the value is an object of scalar type, it does not have an indeterminate value ([basic.indet]),
* [14.#]{.pnum} if the value is of pointer type, it contains the address of an object with static storage duration, the address past the end of such an object ([expr.add]), the address of a non-immediate function, or a null pointer value,
* [14.#]{.pnum} if the value is of pointer-to-member-function type, it does not designate an immediate function, and
* [14.#]{.pnum} if the value is an object of class or array type, each subobject satisfies these constraints for the value.

An entity is a *permitted result of a constant expression* if it is an object with static storage duration that either is not a temporary object or is a temporary object whose value satisfies the above constraints, or if it is a non-immediate function.
:::

Attempting to declare a local `constexpr` variable to point to some allocation would violate this rule - we do not meet the requirements set out above.

However, before we go about trying to figure out how to relax the rule to allow allocations in automatic storage duration `constexpr` variables in immediate function contexts - Richard Smith pointed out another issue. This time not so much a *mistake* as a missed opportunity: allocations aren't the only example of results that are not permitted today but could be allowed if they're entirely within an immediate function context. For instance, taking a pointer to an immediate function. We have to prevent that from leaking to runtime, but if we're in a `consteval` function - there's nothing to prevent:

::: std
```cpp
consteval void f() {}
consteval void g() {
  // Ought to be valid, but isn't a constant expression, because
  // compile-time-only state escapes... into a compile-time-only context.
  constexpr auto *p = f;
  p();
}
```
:::

So now we have multiple ways in which we need to relax this rule. How do we go about doing it? We could be very precise in carving out specifically what we need - but this has a cost. We could fail to carve out enough, and have to keep refining the rule. But more importantly, the status quo is that we have two clear terms with clear usage: *core constant expression* and *constant expression*. Any attempt to introduce a third term in between them simply adds complexity. Is it worth doing so?

Let's say that instead we go all the way. If an automatic storage `constexpr` variable is declared in an immediate function context, its initializer does *not* have to be a constant expression - it only has to be a core constant expression. This allows the allocation examples that were the original motivation of the paper, and this allows the immediate function example that Richard brought up. It does also allow some weird cases:

::: std
```cpp
consteval int f(int n) {
  constexpr int &r = n; // ill-formed, becomes well-formed
  return r;
}

struct S {
  constexpr S() {}
  int i;
};

consteval void g() {
  constexpr S s; // ill-formed, becomes well-formed
}
```
:::

Both of these cases are... odd. They are rejected today for being an invalid permitted result (`n` doesn't have static storage duration) and indeterminate (`s.i` isn't initialized), respectively. And allowing them isn't great. But also any attempt to actually use `r` and `s` here in a constant expression won't work anyway. So we're not losing anything in terms of correctness.

I think on the whole it's better to stick with the simpler and easier-to-understand rule, even as it allows some odd and pointless code.

## Mutation

Consider the following example:

::: std
```cpp
consteval void f(int n) {
    constexpr int* a = new int(n); // ill-formed
    constexpr int* b = new int(1); // #1
    int c[*b];                     // #2
    ++*b;                          // #3
    int d[*b];                     // #4
    delete b;
}
```
:::

The declaration of `a` is already ill-formed, so we don't have to do anything here.

Now, if the declaration of `c` is ill-formed (at `#2`), then we lose the point of declaring the local `constexpr` variable. We really do want it to be usable as a constant expression.

However, at the very least the declaration of `d` has to be ill-formed - this cannot be valid code that both declares an `int[1]` and an `int[2]`. There are two ways we can get there:

1. We can reject `#1` as being insufficiently constant. This gets into the issues that `propconst` was trying to solve [@P1974R0].
2. We can reject `#3` for doing mutation.

It would be nice to not have to go full `propconst` just to solve this particular issue. We're entirely within the realm of the constant evaluator, so this problem is just simpler than having to deal with constexpr allocation that leaks to runtime. And we very nearly already have wording to reject `#3`, that's [expr.const]{.sref}/5.16:

::: std
* [5.16]{.pnum} a modification of an object ([expr.ass], [expr.post.incr], [expr.pre.incr]) unless it is applied to a non-volatile lvalue of literal type that refers to a non-volatile object whose lifetime began within the evaluation of `E`;
:::

It's just that here, `*b` did actually begin its lifetime within `E` (the call to `f`), so we don't violate this rule. We should simply extend this rule to be able to reject this case.


## Incomplete Prior Wording

Also pointed out by Richard, the original wording changing [expr.const]{.sref}/5 as follows:

::: std
[5.18]{.pnum} a *new-expression* ([expr.new]), unless the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and the allocated storage is deallocated [either]{.addu} within the evaluation of `E` [or, if `E` is in an immediate function context, within that context]{.addu};
:::

Richard pointed out this example, asking if it's valid:

::: std
```cpp
consteval void f(bool b) {
  constexpr int *p = new int;
  if (b) delete p;
}
```
:::

Noting that it's impossible to tell - it depends on `b`, which the constant evaluator does not know. Instead he suggests this wording:

::: std
[5.18]{.pnum} a *new-expression* ([expr.new]), unless the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and [either]{.addu} the allocated storage is deallocated within the evaluation of `E`[, or `E` is in an immediate function context]{.addu};
:::

He points out that the actual call to `f` still has to be a constant expression, and so this leak rule still applies there. Neither leaks-to-runtime nor compile-time leaks are possible. So this wording change is more correct.

Richard also points out that this allows this nonsensical function, but if you can't observe a leak, does it really leak?

::: std
```cpp
consteval void f() {
  if (false) { constexpr int *p = new int; }
}
```
:::

## Wording

Change [expr.const]{.sref}/5:

::: std
[5]{.pnum} An expression E is a *core constant expression* unless the evaluation of `E`, following the rules of the abstract machine ([intro.execution]), would evaluate one of the following:

* [5.1]{.pnum} [...]
* [5.16]{.pnum} a modification of an object ([expr.ass], [expr.post.incr], [expr.pre.incr]) unless it is applied to a non-volatile lvalue of literal type that refers to a non-volatile object whose lifetime began within the evaluation of `E`;
* [5.16b]{.pnum} [a modification of an object ([expr.ass], [expr.post.incr], [expr.pre.incr]) whose lifetime began within the evaluation of the initializer for a constexpr variable `V`, unless `E` occurs within the initialization or destruction of `V` or of a temporary object whose lifetime is extended to that of `V`;]{.addu}

::: ins
::: example
```
constexpr int f(int n) {
    constexpr int* p = new int(1); // #1
    ++n;      // ok, lifetime of n began within E
    ++*p;     // error: modification of object whose lifetime began within
              // initializer of constexpr variable at #1
    delete p; // ok
    constexpr int q = []{ // #2
        int i = 0;
        ++i;  // ok: modification of an object whose lifetime begin within E
              // this E occurs within the initialization of constexpr variable
              // declared at #2
        return i;
    }();
    return n + q;
}
```
:::
:::

* [5.17]{.pnum} [...]
* [5.18]{.pnum} a *new-expression* ([expr.new]), unless the selected allocation function is a replaceable global allocation function ([new.delete.single], [new.delete.array]) and [either `E` is in an immediate function context or]{.addu} the allocated storage is deallocated within the evaluation of `E`{.addu};

::: ins
::: example
```
constexpr int f() {
    constexpr int* i = new int(1);  // error: allocation is neither deallocated within this
    return *i;                      // evaluation nor within an immediate function context
}

consteval int o() {
    constexpr int* n = new int(21); // ok, #1
    int a = *n;
    delete n;                       // #2
    return a;
}

consteval int e() {
    constexpr int* r = new int(2022); // ok, #3
    return *r;
}

static_assert(o() == 21);   // ok, because allocation at #1 is deallocated at #2
static_assert(e() == 2022); // error: allocation at #3 is not deallocated
```
:::
:::

* [5.19]{.pnum} a *delete-expression* ([expr.delete]), unless it deallocates a region of storage allocated within the evaluation of `E`;
* [5.20]{.pnum} a call to an instance of `std​::​allocator<T>​::​allocate` ([allocator.members]), unless [either `E` is in an immediate function context or]{.addu} the allocated storage is deallocated within the evaluation of `E`;
* [5.21]{.pnum} a call to an instance of `std​::​allocator<T>​::​deallocate` ([allocator.members]), unless it deallocates a region of storage allocated within the evaluation of `E`;
* [5.22]{.pnum} [...]
:::

Change [dcl.constexpr]{.sref}/6:

::: std
[6]{.pnum} A `constexpr` specifier used in an object declaration declares the object as const. Such an object shall have literal type and shall be initialized. In any `constexpr` variable declaration, [either]{.addu}

* [6.1]{.pnum} the full-expression of the initialization shall be a constant expression ([expr.const]) [, or]{.addu}
* [6.2]{.pnum} [the variable shall have automatic storage duration, be declared within an immediate function context, and the full-expression of the initialization shall be a core constant expression ([expr.const])]{.addu}.

[Except for an automatic storage duration variable declared in an immediate function context, a]{.addu} [A]{.rm} `constexpr` variable that is an object, as well as any temporary to which a `constexpr` reference is bound, shall have constant destruction.

::: example4
```diff
  struct pixel {
    int x, y;
  };
  constexpr pixel ur = { 1294, 1024 };    // OK
  constexpr pixel origin;                 // error: initializer missing

+ consteval int f() {
+   constexpr pixel* q = new pixel{3, 4}; // ok
+   int result = q->x + q->y;
+   delete q;
+   return result;
+ }
+
+ constexpr void g() {
+   constexpr pixel* p = new pixel{1, 2}; // error: not a constant expression
+   delete p;
+   constexpr auto pf = f; // error: not a constant expression
+ }
+
+ consteval int h() {
+   constexpr auto pf = f; // ok
+   return pf();
+ }
```
:::
:::

## Feature-Test Macro

Bump the value of `__cpp_constexpr` in [cpp.predefined]{.sref}:

::: std
```diff
- __cpp_constexpr @[202306L]{.diffdel}@
+ __cpp_constexpr @[2024XXL]{.diffins}@
```
:::

# Acknowledgements

Thank you to Peter Dimov for being Peter Dimov and coming up with all of these examples.

Thank you to Hubert Tong for noticing that the wording was wrong and Richard Smith for helping to fix it.

---
references:
  - id: P2747R1
    citation-label: P2747R1
    title: "`constexpr`` placement new"
    author:
      - family: Barry Revzin
    issued:
      - year: 2023
    URL: https://wg21.link/p2747r1
---
