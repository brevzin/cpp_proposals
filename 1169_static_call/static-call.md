---
title: "static `operator()`"
document: P1169R4
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Casey Carter
      email: <casey@carter.net>
toc: true
status: accepted
---

# Revision History

Since [@P1169R3], wording.

Since [@P1169R2], added missing feature-test macro and updated wording to include [@LWG3617].

[@P1169R1] was approved for electronic polling by EWG, but two issues came up that while this paper does not *change* are still worth commenting on: can [static lambdas still have capture](#static-lambdas-with-capture) and can whether or not stateless lambdas be `static` be [implementation-defined](#can-the-static-ness-of-lambdas-be-implementation-defined)?

[@P1169R0] was presented to EWGI in San Diego, where there was no consensus to pursue the paper. However, recent discussion has caused renewed interest in this paper so it has been resurfaced. R0 of this paper additionally proposed implicitly changing capture-less lambdas to have static function call operators, which would be an breaking change. That part of this paper has been changed to instead allow for an explicit opt-in to static. Additionally, this language change has been implemented.

# Motivation

The standard library has always accepted arbitrary function objects - whether to be unary or binary predicates, or perform arbitrary operations. Function objects with call operator templates in particular have a significant advantage today over using overload sets since you can just pass them into algorithms. This makes, for instance, `std::less<>{}` very useful.

As part of the Ranges work, more and more function objects are being added to the standard library - the set of Customization Point Objects (CPOs). These objects are Callable, but they don't, as a rule, have any members. They simply exist to do what Eric Niebler termed the ["Std Swap Two-Step"](http://ericniebler.com/2014/10/21/customization-point-design-in-c11-and-beyond/). Nevertheless, the call operators of all of these types are non-static member functions. Because _all_ call operators have to be non-static member functions.

What this means is that if the call operator happens to not be inlined, an extra register must be used to pass in the `this` pointer to the object - even if there is no need for it whatsoever. Here is a [simple example](https://godbolt.org/z/ajTZo2):

::: bq
```cpp
struct X {
    bool operator()(int) const;
    static bool f(int);
};

inline constexpr X x;

int count_x(std::vector<int> const& xs) {
    return std::count_if(xs.begin(), xs.end(),
#ifdef STATIC
    X::f
#else
    x
#endif
    );
}
```
:::

`x` is a global function object that has no members that is intended to be passed into various algorithms. But in order to work in algorithms, it needs to have a call operator - which must be non-static. You can see the difference in the generated asm btween using the function object as intended and passing in an equivalent static member function:


::: cmptable
### Non-static call operator
```nasm
count_x(std::vector<int, std::allocator<int> > const&):
        push    r12
        push    rbp
        push    rbx
        sub     rsp, 16
        mov     r12, QWORD PTR [rdi+8]
        mov     rbx, QWORD PTR [rdi]
        @[mov     BYTE PTR [rsp+15], 0]{.addu}@
        cmp     r12, rbx
        je      .L5
        xor     ebp, ebp
.L4:
        mov     esi, DWORD PTR [rbx]
        @[lea     rdi, [rsp+15]]{.addu}@
        call    X::operator()(int) const
        cmp     al, 1
        sbb     rbp, -1
        add     rbx, 4
        cmp     r12, rbx
        jne     .L4
        add     rsp, 16
        mov     eax, ebp
        pop     rbx
        pop     rbp
        pop     r12
        ret
.L5:
        add     rsp, 16
        xor     eax, eax
        pop     rbx
        pop     rbp
        pop     r12
        ret
```

### Static member function
```nasm
count_x(std::vector<int, std::allocator<int> > const&):
        push    r12
        push    rbp
        push    rbx
        mov     r12, QWORD PTR [rdi+8]
        mov     rbx, QWORD PTR [rdi]
        cmp     r12, rbx
        je      .L5
        xor     ebp, ebp
.L4:
        mov     edi, DWORD PTR [rbx]
        call    X::f(int)
        cmp     al, 1
        sbb     rbp, -1
        add     rbx, 4
        cmp     r12, rbx
        jne     .L4
        mov     eax, ebp
        pop     rbx
        pop     rbp
        pop     r12
        ret
.L5:
        pop     rbx
        xor     eax, eax
        pop     rbp
        pop     r12
        ret
```
:::

Even in this simple example, you can see the extra zeroing out of `[rsp+15]`, the extra `lea` to move that zero-ed out area as the object parameter - which we know doesn't need to be used. This is wasteful, and seems to violate the fundamental philosophy that we don't pay for what we don't need.

The typical way to express the idea that we don't need an object parameter is to declare functions `static`. We just don't have that ability in this case.

# Proposal

The proposal is to just allow the ability to make the call operator a static member function, instead of requiring it to be a non-static member function. We have many years of experience with member-less function objects being useful. Let's remove the unnecessary object parameter overhead. There does not seem to be any value provided by this restriction.

There are other operators that are currently required to be implemented as non-static member functions - all the unary operators, assignment, subscripting, conversion functions, and class member access. We do not believe that being able to declare any of these as static will have as much value, so we are not pursuing those at this time. We're not aware of any use-case for making any of these other operators static, while the use-case of having stateless function objects is extremely common.

## Overload Resolution

There is one case that needs to be specially considered when it comes to overload resolution, which did not need to be considered until now:

::: bq
```cpp
struct less {
    static constexpr auto operator()(int i, int j) -> bool {
        return i < j;
    }

    using P = bool(*)(int, int);
    operator P() const { return operator(); }
};

static_assert(less{}(1, 2));
```
:::

If we simply allow `operator()` to be declared `static`, we'd have two candidates here: the function call operator and the surrogate call function. Overload resolution between those candidates would work as considering between:

::: bq
```cpp
operator()(@*contrived-parameter*@, int, int);
@*call-function*@(bool(*)(int, int), int, int);
```
:::

And currently this is ambiguous because [over.match.best.general]{.sref}/1.1 stipulates that the conversion sequence for the contrived implicit object parameter of a static member function is neither better nor worse than _any other conversion sequence_. This needs to be reined in slightly such that the conversion sequence for the contrived implicit object parameter is neither better nor worse than any _standard_ conversion sequence, but still better than user-defined or ellipsis conversion sequences. Such a change would disambiguate this case in favor of the call operator.


## Lambdas

A common source of function objects whose call operators could be static but are not are lambdas without any capture. Had we been able to declare the call operator static when lambdas were originally introduced in the language, we would surely have had a lambda such as:

::: bq
```cpp
auto four = []{ return 4; };
```
:::

desugar into:

::: bq
```cpp
struct __unique {
    static constexpr auto operator()() { return 4; };

    using P = int();
    constexpr operator P*() { return operator(); }
};

__unique four{};
```
:::

Rather than desugaring to a type that has a non-static call operator along with a conversion function that has to return some other function.

However, we can't simply change such lambdas because this could break code. There exists code that takes a template parameter of callable type and does `decltype(&F::operator())`, expecting the resulting type to be a pointer to member type (which is the only thing it can be right now). If we change captureless lambdas to have a static call operator implicitly, all such code would break for captureless lambdas. Additionally, this would be a language ABI break. While lambdas shouldn't show up in your ABI anyway, we can't with confidence state that such code doesn't exist nor that such code deserves to be broken.

Instead, we propose that this can be opt-in: a lambda is allowed to be declared `static`, which will then cause the call operator (or call operator template) of the lambda to be a static member function rather than a non-static member function:

::: bq
```cpp
auto four = []() static { return 4; };
```
:::

We then also need to ensure that a lambda cannot be declared `static` if it is declared `mutable` (an inherently non-static property) or has any capture (as that would be fairly pointless, since you could not access any of that capture).

### Static lambdas with capture

Consider the situation where a lambda may need to capture something (for lifetime purposes only) but does not otherwise need to reference it. For instance:

::: bq
```cpp
auto under_lock = [lock=std::unique_lock(mtx)]() static { /* do something */; };
```
:::

The body of this lambda does not use the capture `lock` in any way, so there isn't anything that inherently prevents this lambda from having a `static` call operator. The rule from R1 of this paper was basically:

> A `static` lambda shall have no *lambda-capture*.

But could instead be:

> If a lambda is `static`, then any *id-expression* within the body of the lambda that would be an odr-use of a captured entity is ill-formed.

However, we feel that the value of the teachability of "Just make stateless lambdas `static`" outweights the value of supporting holding capturing variables that the body of the lambda does not use. This restriction could be relaxed in the future, if it proves overly onerous (much as we are here relaxing the restriction that call operators be non-static member functions).

This aspect was specifically polled during the telecon, and the outcome was:

|SF|F|N|A|SA|
|--|-|-|-|--|
| 0|3|4|5| 0|

### Can the `static`-ness of lambdas be implementation-defined?

Another question arose during the telecon about whether it is feasible or desirable to make it implementation-defined as to whether or not the call operator of a capture-less lambda is `static`.

The advantage of making it implementation-defined is that implementations could, potentially, add a flag that would allow users to treat all of their capture-less lambdas as `static` without the burden of adding this extra annotation (had call operators been allowed to be static before C++11, surely capture-less lambdas would have been implicitly `static`) while still making this sufficiently opt-in as to avoid ABI-breaking changes.

The disadvantage of making it implementation-defined is that this is a fairly important property of how a lambda behaves. Right now, the observable properties of a lambda are specified and portable. The implementation freedom areas are typically not observable to the programmer. The static-ness of the operator is observable, so making that implementation-defined or unspecified seems antithetical to the design of lambdas. The rationale for doing something like this (i.e. avoiding a sea of seemingly-pointless `static` annotations when the compiler should be able to Just Do It), but it seems rather weird that a property like that wouldn't be portable.

## Deduction Guides

Consider the following, assuming a version of `less` that uses a static call operator:

::: bq
```cpp
template <typename T>
struct less {
    static constexpr auto operator()(T const& x, T const& y) -> bool {
        return x < y;
    };
};

std::function f = less<int>{};
```
:::

This will not compile with this change, because `std::function`'s deduction guides only work with either function pointers (which does not apply) or class types whose call operator is a non-static member function. These will need to be extended to support call operators with function type (as they would for [@P0847R6] anyway).

## Prior References

This idea was previously referenced in [@EWG88], which reads:

::: quote
In c++std-core-14770, Dos Reis suggests that `operator[]` and `operator()` should both be allowed to be static. In addition to that, he suggests that both should allow multiple parameters. It's well known that there's a possibility that this breaks existing code (`foo[1,2]` is valid, the thing in brackets is a comma-expression) but there are possibilities to fix such cases (by requiring parens if a comma-expression is desired). EWG should discuss whether such unification is to be strived for.

Discussed in Rapperswil 2014. EWG points out that there are more issues to consider here, in terms of other operators, motivations, connections with captureless lambdas, who knows what else, so an analysis paper is requested.
:::

There is a separate paper proposing multi-argument subscripting [@P2128R3] already, with preexisting code such as `foo[1, 2]` already having been deprecated.

## Implementation Experience

The language changes have been implemented in EDG.

# Wording

## Language Wording

Add `static` to the grammar of [expr.prim.lambda.general]{.sref}:

::: bq
```diff
  $lambda-specifier$:
    consteval
    constexpr
    mutable
+   static
```
:::

Change [expr.prim.lambda.general]{.sref}/4:

::: bq
[4]{.pnum} A _lambda-specifier-seq_ shall contain at most one of each _lambda-specifier_ and shall not contain both `constexpr` and `consteval`. If the _lambda-declarator_ contains an explicit object parameter ([dcl.fct]), then no _lambda-specifier_ in the _lambda-specifier-seq_ shall be `mutable` [or `static`]{.addu}. [The _lambda-specifier-seq_ shall not contain both `mutable` and `static`. If the _lambda-specifier-seq_ contains `static`, there shall be no _lambda-capture_]{.addu}.
:::

Change [expr.prim.lambda.closure]{.sref}/5:

::: bq
[5]{.pnum} The function call operator or operator template is [a static member function or static member function template ([class.static.mfct]) if the *lambda-expression*'s *parameter-declaration-clause* is followed by `static`. Otherwise, it is a non-static member function or member function template ([class.mfct.non-static]) that is]{.addu} declared `const` ([class.mfct.non.static]) if and only if the _lambda-expression_'s _parameter-declaration-clause_ is not followed by `mutable` and the _lambda-declarator_ does not contain an explicit object parameter. It is neither virtual nor declared `volatile`. Any *noexcept-specifier* specified on a *lambda-expression* applies to the corresponding function call operator or operator template. An *attribute-specifier-seq* in a *lambda-declarator* appertains to the type of the corresponding function call operator or operator template. The function call operator or any given operator template specialization is a `constexpr` function if either the corresponding *lambda-expression*'s *parameter-declaration-clause* is followed by `constexpr`, or it satisfies the requirements for a `constexpr` function.
:::

Add a note to [expr.prim.lambda.closure]{.sref}/8 and /11 indicating that we could just return the call operator. The wording as-is specifies the behavior of the return here, and returning the call operator already would be allowed, so no wording change is necessary. But the note would be helpful:

::: bq
[8]{.pnum} The closure type for a non-generic *lambda-expression* with no *lambda-capture* whose constraints (if any) are satisfied has a conversion function to pointer to function with C++ language linkage having the same parameter and return types as the closure type's function call operator.
The conversion is to “pointer to `noexcept` function” if the function call operator has a non-throwing exception specification.
[If the function call operator is a static member function, then the value returned by this conversion function is the address of the function call operator. Otherwise, the]{.addu} [The]{.rm} value returned by this conversion function is the address of a function `F` that, when invoked, has the same effect as invoking the closure type's function call operator on a default-constructed instance of the closure type. `F` is a constexpr function if the function call operator is a constexpr function and is an immediate function if the function call operator is an immediate function.

[11]{.pnum} [If the function call operator template is a static member function template, then the value returned by any given specialization of this conversion function template is the address of the corresponding function call operator template specialization. Otherwise, the]{.addu} [The]{.rm} value returned by any given specialization of this conversion function template is the address of a function `F` that, when invoked, has the same effect as invoking the generic lambda's corresponding function call operator template specialization on a default-constructed instance of the closure type. F is a constexpr function if the corresponding specialization is a constexpr function and F is an immediate function if the function call operator template specialization is an immediate function.
:::

Change [over.match.best.general]{.sref}/1 to drop the static member exception and remove the bullets and the footnote:

::: bq
[1]{.pnum} Define ICS^i^(`F`) as [follows:]{.rm}

* [1.1]{.pnum} [If `F` is a static member function, ICS^1^(`F`) is defined such that ICS^1^(`F`) is neither better nor worse than ICS^1^(`G`) for any function `G`, and, symmetrically, ICS^1^(`G`) is neither better nor worse than ICS^1^(`F`);^117^ otherwise,]{.rm}
* [1.2]{.pnum} [let ICS^i^(`F`) denote]{.rm} the implicit conversion sequence that converts the i^th^ argument in the list to the type of the i^th^ parameter of viable function `F`. [over.best.ics] defines the implicit conversion sequences and [over.ics.rank] defines what it means for one implicit conversion sequence to be a better conversion sequence or worse conversion sequence than another.
:::

Add to [over.best.ics.general]{.sref} a way to compare this static member function case:

::: bq
[*]{.pnum} [When the parameter is the implicit object parameter of a static member function, the implicit conversion sequence is a standard conversion sequence that is neither better nor worse than any other standard conversion sequence.]{.addu}
:::

Change [over.oper]{.sref} paragraph 6 and introduce bullets to clarify the parsing. `static void operator()() { }` is a valid function call operator that has no parameters with this proposal, so needs to be clear that the "has at least one parameter" part refers to the non-member function part of the clause.

::: bq
[6]{.pnum} An operator function shall either

* [6.1]{.pnum} be a [non-static]{.rm} member function or
* [6.2]{.pnum} be a non-member function that has at least one parameter whose type is a class, a reference to a class, an enumeration, or a reference to an enumeration.

It is not possible to change the precedence, grouping, or number of operands of operators. The meaning of the operators `=`, (unary) `&`, and `,` (comma), predefined for each type, can be changed for specific class and enumeration types by defining operator functions that implement these operators. Operator functions are inherited in the same manner as other base class functions.
:::

Change [over.call]{.sref} paragraph 1:

::: bq
[1]{.pnum} A _function call operator function_ is a function named `operator()` that is a [non-static]{.rm} member function with an arbitrary number of parameters.
:::

## Library Wording

Change the deduction guide for `function` in [func.wrap.func.con]{.sref}/16-17. [This assumes the wording change in [@LWG3617]. This relies on the fact that `f.operator()` would be valid for a static member function, but not an explicit object member function - which like other non-static member functions you can't just write `x.f` you can only write `x.f(args...)`.]{.ednote}:

::: bq
```cpp
template <class F> function(F) -> function<@_see below_@>;
```

[15]{.pnum} *Constraints*: `&F​::​operator()` is well-formed when treated as an unevaluated operand and [either]{.addu}

* [#.#]{.pnum} [`F::operator()` is a non-static member function and ]{.addu} `decltype(​&F​::​operator())` is either of the form `R(G​::​*)(A...) cv &@~opt~@ noexcept@~opt~@` or of the form `R(*)(G $cv$ $ref$@~opt~@, A...) noexcept@~opt~@` for a type `G` [, or]{.addu}

* [#.#]{.pnum} [`F::operator()` is a static member function and `decltype(​&F​::​operator())` is of the form `R(*)(A...) noexcept@~opt~@`.]{.addu}

[16]{.pnum} *Remarks*: The deduced type is `function<R(A...)>`.

:::

Change the deduction guide for `packaged_task` in [futures.task.members]{.sref}/7-8 in the same way:

::: bq
```cpp
template <class F> packaged_task(F) -> packaged_task<@_see below_@>;
```

[7]{.pnum} *Constraints*: `&F​::​operator()` is well-formed when treated as an unevaluated operand and [either]{.addu}

* [#.#]{.pnum} [`F::operator()` is a non-static member function and ]{.addu} `decltype(​&F​::​operator())` is either of the form `R(G​::​*)(A...) cv &@~opt~@ noexcept@~opt~@` or of the form `R(*)(G $cv$ $ref$@~opt~@, A...) noexcept@~opt~@` for a type `G` [, or]{.addu}

* [#.#]{.pnum} [`F::operator()` is a static member function and `decltype(​&F​::​operator())` is of the form `R(*)(A...) noexcept@~opt~@`.]{.addu}

[8]{.pnum} *Remarks*: The deduced type is `packaged_task<R(A...)>`.
:::

## Feature-test macro

Add to [cpp.predefined]{.sref}/table 19:

::: bq
[`__cpp_static_call_operator`]{.addu}
:::

with the appropriate value. This allows define function objects or lambdas to have conditionally static call operators when possible.


---
references:
    - id: LWG3617
      citation-label: LWG3617
      title: "`function`/`packaged_task` deduction guides and deducing `this`"
      author:
        - family: Barry Revzin
      issued:
        year: 2021
      URL: https://cplusplus.github.io/LWG/issue3617
---
