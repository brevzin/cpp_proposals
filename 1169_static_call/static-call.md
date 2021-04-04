---
title: "static `operator()`"
document: P1169R1
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: barry.revzin@gmail.com
    - name: Casey Carter
      email: <casey@carter.net>
toc: true
---

# Revision History

[@P1169R0] was presented to EWGI in San Diego, where there was no consensus to pursue the paper. However, recent discussion has caused renewed interest in this paper so it has been resurfaced. R0 of this paper additionally proposed implicitly changing capture-less lambdas to have static function call operators, which would be an ABI breaking change. That part of this paper has been changed to instead allow for an explicit opt-in to static. Additionally, this language change has been implemented. 

# Motivation

The standard library has always accepted arbitrary function objects - whether to be unary or binary predicates, or perform arbitrary operations. Function objects with call operator templates in particular have a significant advantage today over using overload sets since you can just pass them into algorithms. This makes, for instance, `std::less<>{}` very useful.

As part of the Ranges work, more and more function objects are being added to the standard library - the set of Customization Point Objects (CPOs). These objects are Callable, but they don't, as a rule, have any members. They simply exist to do what Eric Niebler termed the ["Std Swap Two-Step"](http://ericniebler.com/2014/10/21/customization-point-design-in-c11-and-beyond/). Nevertheless, the call operators of all of these types are non-static member functions. Because _all_ call operators have to be non-static member functions. 

What this means is that if the call operator happens to not be inlined, an extra register must be used to pass in the `this` pointer to the object - even if there is no need for it whatsoever. Here is a [simple example](https://godbolt.org/z/ajTZo2):

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

If we simply allow `operator()` to be declared `static`, we'd have two candidates here: the function call operator and the surrogate call function. Overload resolution between those candidates would work as considering between:

```cpp
operator()(@*contrived-parameter*@, int, int);
@*call-function*@(bool(*)(int, int), int, int);
```

And currently this is ambiguous because [over.match.best.general]{.sref}/1.1 stipulates that the conversion sequence for the contrived implicit object parameter of a static member function is neither better nor worse than _any other conversion sequence_. This needs to be reined in slightly such that the conversion sequence for the contrived implicit object parameter is neither better nor worse than any _standard_ conversion sequence, but still better than user-defined or ellipsis conversion sequences. Such a change would disambiguate this case in favor of the call operator.


## Lambdas

A common source of function objects whose call operators could be static but are not are lambdas without any capture. Had we been able to declare the call operator static when lambdas were originally introduced in the language, we would surely have had a lambda such as:

```cpp
auto four = []{ return 4; };
```

desugar into:

```cpp
struct __unique {
    static constexpr auto operator()() { return 4; };
    consetxpr auto operator std::add_pointer_t<int()>() { return operator(); }
};

__unique four{};
```

Rather than desugaring to a type that has a non-static call operator along with a conversion function that has to return some other function. 

However, we can't simply change such lambdas because this would be a language ABI break. While lambdas shouldn't show up in your ABI anyway, we can't with confidence state that such code doesn't exist nor that such code deserves to be broken.

Instead, we propose that this can be opt-in: a lambda is allowed to be declared `static`:

```cpp
auto four = []() static { return 4; };
```

Ensuring that a lambda cannot be declared `static` if it is declared `mutable` (an inherently non-static property) or has any capture (as that would be fairly pointless, since you could not access any of that capture).

## Deduction Guides

Consider the following, assuming a version of `less` that uses a static call operator:

```cpp
template <typename T>
struct less {
    static constexpr auto operator()(T const& x, T const& y) -> bool {
        return x < y;
    };
};

std::function f = less<int>{};
```

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

# Language Wording

Change [expr.prim.lambda.general]{.sref}/3:

::: bq
[3]{.pnum} In the *decl-specifier-seq* of the *lambda-declarator*, each *decl-specifier* shall be one of `mutable`, [`static`,]{.addu} `constexpr`, or `consteval`. [The *decl-specifier-seq* shall not contain both `mutable` and `static`. If the *decl-specifier-seq* contains `static`, there shall be no *lambda-capture*.]{.addu}
:::

Change [expr.prim.lambda.closure]{.sref}/4:

::: bq
[4]{.pnum} The function call operator or operator template is  [a static member function or static member function template ([class.static.mfct]) if the *lambda-expression*'s *parameter-declaration-clause* is followed by `static`. Otherwise, it is a non-static member function or member function template ([class.mfct.non-static]) that is]{.addu} declared `const` ([class.mfct.non-static]) if and only if the *lambda-expression*'s *parameter-declaration-clause* is not followed by `mutable`. It is neither virtual nor declared `volatile`. Any *noexcept-specifier* specified on a *lambda-expression* applies to the corresponding function call operator or operator template. An *attribute-specifier-seq* in a *lambda-declarator* appertains to the type of the corresponding function call operator or operator template. The function call operator or any given operator template specialization is a `constexpr` function if either the corresponding *lambda-expression*'s *parameter-declaration-clause* is followed by `constexpr`, or it satisfies the requirements for a `constexpr` function. 
:::

Add a note to [expr.prim.lambda.closure]{.sref}/7 and /10 indicating that we could just return the call operator. The wording as-is specifies the behavior of the return here, and returning the call operator already would be allowed, so no wording change is necessary. But the note would be helpful:

::: bq
[7]{.pnum} The closure type for a non-generic *lambda-expression* with no *lambda-capture* whose constraints (if any) are satisfied has a conversion function to pointer to function with C++ language linkage having the same parameter and return types as the closure type's function call operator.
The conversion is to “pointer to `noexcept` function” if the function call operator has a non-throwing exception specification.
The value returned by this conversion function is the address of a function `F` that, when invoked, has the same effect as invoking the closure type's function call operator on a default-constructed instance of the closure type. `F` is a constexpr function if the function call operator is a constexpr function and is an immediate function if the function call operator is an immediate function. [ [*Note*: if the function call operator is a static member function, the conversion function may return the address of the function call operator. -*end note*] ]{.addu}

[10]{.pnum} The value returned by any given specialization of this conversion function template is the address of a function `F` that, when invoked, has the same effect as invoking the generic lambda's corresponding function call operator template specialization on a default-constructed instance of the closure type. F is a constexpr function if the corresponding specialization is a constexpr function and F is an immediate function if the function call operator template specialization is an immediate function. [ [*Note*: if the function call operator template is a static member function template, the conversion function may return the address of a specialization of the function call operator template. -*end note*] ]{.addu}
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

Change [over.oper]{.sref} paragraph 6:

::: bq
[6]{.pnum} An operator function shall either be a [non-static]{.rm} member function or be a non-member function that has at least one parameter whose type is a class, a reference to a class, an enumeration, or a reference to an enumeration. It is not possible to change the precedence, grouping, or number of operands of operators. The meaning of the operators `=`, (unary) `&`, and `,` (comma), predefined for each type, can be changed for specific class and enumeration types by defining operator functions that implement these operators. Operator functions are inherited in the same manner as other base class functions.
:::

Change [over.call]{.sref} paragraph 1:

::: bq
[1]{.pnum} A _function call operator function_ is a function named `operator()` that is a [non-static]{.rm} member function with an arbitrary number of parameters.
:::

## Library Wording

Change the deduction guide for `function` in [func.wrap.func.con]{.sref}/14-15:

::: bq
```cpp
template <class F> function(F) -> function<@_see below_@>;
```

[14]{.pnum} *Constraints*: `&F​::​operator()` is well-formed when treated as an unevaluated operand and `decltype(​&F​::​operator())` is [either]{.addu} of the form `R(G​::​*)(A...) cv &@~opt~@ noexcept@~opt~@` for a class type `G` [or of the form `R(*)(A...) noexcept@~opt~@`]{.addu}.

[15]{.pnum} *Remarks*: The deduced type is `function<R(A...)>`.
:::

Change the deduction guide for `packaged_task` in [futures.task.members]{.sref}/7-8 in the same way (it's nearly the same wording today):

::: bq
```cpp
template <class F> packaged_task(F) -> packaged_task<@_see below_@>;
```

[7]{.pnum} *Constraints*: `&F​::​operator()` is well-formed when treated as an unevaluated operand and `decltype(​&F​::​operator())` is [either]{.addu} of the form `R(G​::​*)(A...) cv &@~opt~@ noexcept@~opt~@` for a class type `G` [or of the form `R(*)(A...) noexcept@~opt~@`]{.addu}.

[8]{.pnum} *Remarks*: The deduced type is `packaged_task<R(A...)>`.
:::
