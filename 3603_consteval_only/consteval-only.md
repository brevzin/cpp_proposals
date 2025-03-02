---
title: "Consteval-only Values and Consteval Variables"
document: P3603R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
toc: true
tag: constexpr
---

# Abstract

This paper formalizes the concept of _consteval-only value_ and uses it to introduce consteval variables — variables that can only exist at compile time and are never code-gen. Consteval variables can then be used to solve some concrete problems we have today — like variant visitation.

# Introduction

C++ today already has an *informal* notion of a value that is only allowed to exist during compile time. Currently, this is ill-formed:

::: std
```cpp
consteval int add(int x, int y) {
    return x + y;
}

constexpr auto ptr = add; // error
```
:::

We cannot "persist" a pointer to immediate function like this — `add` is a value that is only allowed to exist at compile time. This is because we cannot invoke `ptr` at runtime, and if we allowed this, `ptr` would just be a regular old `int(*)(int, int)`. The original addition of `consteval` functions — [@P1073R3]{.title} — already had this rule.

It's not just that you cannot create a `constexpr` variable whose value is `add`, you also cannot use it as a template argument, cannot have it as a member of a struct that's used in either way, etc.

Similarly, we cannot *really* persist reflections [@P2996R10]{.title}:

::: std
```cpp
constexpr auto refl = ^^int;
```
:::

We cannot allow you to do anything with `refl` at runtime in the same way we cannot do anything with `ptr` at runtime. But we enforce these requirements very differently:

* we disallow `ptr` through what used to be the "permitted result" rule
* we allow `refl` but ensure that all expressions involving `refl` are constant, by way of consteval-only types and immediate escalation from [@P2564R3]{.title}.

The status quo from the Reflection design is that we can handle these differently because we can differentiate based on type. `ptr` is just a function pointer, `refl` is a `std::meta::info` — we can ensure that expressions involving the latter are constant, but we can't tell from a given function pointer whether we need that machinery or not.

What if we did things a little bit differently?

## Consteval Variables

We currently have `consteval` functions (which can only exist at compile time) but we do not have `consteval` variables. What if we did?

We would have to say that a `consteval` variable could only exist at compile time, which means all uses of it must be constant. We already have these kinds of rules in the language, so it is straightforward to extend them to cover this case as well. That is, we would expect:

::: std
```cpp
consteval int add(int x, int y) {
    return x + y;
}

// error: as before, cannot persist a pointer to immediate function
constexpr auto p1 = add;

// OK, p2 is a consteval variable. it does not exist at runtime
consteval auto p2 = add;

int main(int argc, char**) {
    // error: p2 is a consteval variable, so expressions using it must be
    // constant — and this is not.
    return p2(argc, 1);
}
```
:::

This is already pretty nice. We could never initialize something like `p2` today (including as part of a struct, etc.), and this would allow us to.

Another important benefit of `consteval` variables is that they are _guaranteed_ to not occupy space at runtime. You just don't hit issues [like this](https://www.reddit.com/r/cpp/comments/1i36ahd/is_this_an_msvc_bug_or_am_i_doing_something_wrong/). `constexpr` variables, even if never accessed at runtime, may occupy space anyway. It's just QoI. But in the same way that `consteval` functions _cannot_ lead to codegen, `consteval` variables _cannot_ either. That's a pretty nice benefit.

But how do we distinguish between what is allowed for `p1` and what is allowed for `p2`? We simply need to introduce...

## Consteval-Only Values

As mentioned earlier, we already have an implicit notion of consteval-only value in the language with how we treat immediate functions today. Let's make that more explicit, and also account for the consteval variables we're introducing. This isn't quite Core-precise wording, but it should convey the idea we need:

::: std
An expression has a _consteval-only value_ if:

* any constituent part of it either points to or refers to a consteval variable,
* any constituent part of it either points to or refers to an immediate function, or
* any constituent part of it has consteval-only type.
:::

For instance, an `$id-expression$` naming an immediate function is a consteval-only value (like `add`), `^^int` is a consteval-only value, `members_of(^^something)` is a consteval-only value, `p2` in the above example is a consteval-only value (doubly so — both because it is a `consteval` variable and because it is a pointer to immediate function), etc.

Our rules around immediate-escalating expressions already presuppose the existence of a consteval-only value, this term just allows us to be more explicit about it:

::: std
[25]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and [it is]{.rm} either

* [25.1]{.pnum} [a potentially-evaluated *id-expression* that denotes an immediate function that]{.rm} [it has consteval-only value and it]{.addu} is not a subexpression of an immediate invocation, or
* [25.2]{.pnum} [it is]{.addu} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.
:::

This isn't just a way to clean up the specification a little. It also has some other interesting potential...

# Motivating Example: Variant Visitation

Jiang An submitted a very interesting bug report to [libstdc++](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=118434) (and [libc++](https://github.com/llvm/llvm-project/issues/118560)) in January 2025, which is also now [@LWG4197]. It dealt with visiting a `std::variant` with a consteval lambda.

Here is a short reproduction of it, with a greatly reduced `variant` implementation that gets us to the point:

::: std
```cpp
#include <array>

template <class T, class U>
struct Variant {
    union {
        T t;
        U u;
    };
    int index;

    constexpr Variant(T t) : t(t), index(0) { }
    constexpr Variant(U u) : u(u), index(1) { }

    template <int I> requires (I < 2)
    constexpr auto get() const -> auto const& {
        if constexpr (I == 0) return t;
        else if constexpr (I == 1) return u;
    }
};

template <class R, class F, class V0, class V1>
struct binary_vtable_impl {
    template <int I, int J>
    static constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
        return f(v0.template get<I>(), v1.template get<J>());
    }

    static constexpr auto get_array() {
        return std::array{
            std::array{
                &visit<0, 0>,
                &visit<0, 1>
            },
            std::array{
                &visit<1, 0>,
                &visit<1, 1>
            }
        };
    }

    static constexpr std::array fptrs = get_array();
};

template <class R, class F, class V0, class V1>
constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
    using Impl = binary_vtable_impl<R, F, V0, V1>;
    return Impl::fptrs[v0.index][v1.index]((F&&)f, v0, v1);
}

consteval auto func(const Variant<int, long>& v1, const Variant<int, long>& v2) {
    return visit<int>([](auto x, auto y) consteval { return x + y; }, v1, v2);
}

static_assert(func(Variant<int, long>{42}, Variant<int, long>{1729}) == 1771);
```
:::

Here, the lambda `[](auto x, auto y) consteval { return x + y; }` is `consteval`. It is invoked in multiple instantiations of `binary_vtable_impl<...>::visit<...>`, which causes those `constexpr` functions to escalate into `consteval` functions, due to [@P2564R3] (otherwise the invocation would already be ill-formed). `get_array()` is returning a two-dimensional array of 4 function pointers into different instantiations of those functions, which are all `consteval` — and that array is stored as the `static constexpr` data member `fptrs`.

That is ill-formed.

Initialization of a `constexpr` variable (like `binary_vtable_impl<...>::fptrs` in this case) must be a constant expression, which must satisfy (from [expr.const]{.sref}/22, and note that this wording has changed a lot recently):

::: std
[22]{.pnum} A *constant expression* is either a glvalue core constant expression that refers to an object or a non-immediate function, or a prvalue core constant expression whose value satisfies the following constraints:

* [22.1]{.pnum} each constituent reference refers to an object or a non-immediate function,
* [22.2]{.pnum} no constituent value of scalar type is an indeterminate value ([basic.indet]),
* [22.3]{.pnum} no constituent value of pointer type is a pointer to an immediate function or an invalid pointer value ([basic.compound]), and
* [22.4]{.pnum} no constituent value of pointer-to-member type designates an immediate function.
:::

This code breaks that rule. We have pointers that point to immediate functions, hence we do not have a constant expression, hence we do not have a validly initialized `constexpr` variable.

What do we do now?

## Consteval Variable Escalation

Importantly, `fptrs` is a `static constexpr` variable that is a templated entity, and its initializer — `get_array()` — has consteval-only value. Today, we reject this initialization for the same reason that we rejected the initialization of `ptr` earlier: if that initialization were allowed to succeed, we have regular function pointers, and nothing prevents me from invoking them at runtime. Which would defeat the purpose of the `consteval` specifier.

However.

What if, instead of rejecting the example, the fact that the initializer were a consteval-only value instead led to the escalation of `fptrs` to be `consteval` variable instead of a `constexpr` one? This follows the principle set out in [@P2564R3] — there, we had a specialization of a `constexpr` function template that would otherwise be ill-formed, so we make it `consteval`.

We could do the same here! `fptrs` is a `static constexpr` variable in a class template that is initialized to a consteval-only value, so let's escalate it to be a `consteval` variable instead of a `constexpr` one. If we do that, then we have to examine its usage within `visit`, copied here again for convenience:

::: std
```cpp
template <class R, class F, class V0, class V1>
constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
    using Impl = binary_vtable_impl<R, F, V0, V1>;
    return Impl::fptrs[v0.index][v1.index]((F&&)f, v0, v1);
}
```
:::

`fptrs` being a `consteval` variable means that the invocation there has to be immediate-escalating. This causes the specialization of `visit` to become a `consteval` function following the same rules as in [@P2564R3]. At which point, everything just works.

Put differently — as a `constexpr` variable, `fptrs` was not allowed to be initialized with pointers to immediate functions. But as a `consteval` variable, it can be — since we escalate all invocations of those pointers! Everything just... works, and requires no code changes on the part of the library implementation.

# Other Design Questions

There are a few other design questions to discuss.

## Mutability

One question we have to address is, given:

::: std
```cpp
consteval int v = 0;
```
:::

What is `decltype(v)`? In Daveed's original proposal in [@P0596R1]{.title}, `v` was an `int` that was actually possible to mutate during constant evaluation time. Having compile-time mutable variables would be quite useful to solve some problems, although it is not without its share of complexity — specifically when such mutation is allowed to happen.

While I do think it would be quite valuable to have compile-time mutable variables, I am not pursuing those in this paper for three reasons:

1. They are complicated,
2. I think inherently having a variable declared `consteval` that is mutable is just confusing from a keyword standpoint. It's one thing to have `constinit` — which at least is simply `const`ant `init`ialized. But `consteval` seems a bit strong, and
3. Given that `constexpr` variables can escalate to `consteval` ones, it is important that they don't change types. `constexpr` is `int const`, so `consteval` should be too.

We can always add consteval mutable variables in the future by allowing the declaration:

::: std
```cpp
consteval mutable int v = 0;
static_assert(v == 0); // ok
consteval {
  ++v;                 // ok, mutable
}
static_assert(v == 1); // ok, observed mutation
```
:::

Alternatively, because of the potential future of consteval mutable variables, we could enforce that variables declared `consteval` must also be declared `const`. That restriction can be relaxed later. Note that this rule would only be for variables _declared_ `consteval`, not those which escalate:

::: std
```cpp
consteval int a = 0; // ill-formed
consteval int const b = 0; // ok
```
:::

## Rules around `constexpr` Variables

Let's quickly consider:

::: std
```cpp
consteval int add(int x, int y) {
    return x + y;
}

constexpr auto a = add;
consteval auto b = add;

constexpr auto c = ^^int;
consteval auto d = ^^int;
```
:::

The status quo is that `a` is ill-formed (as already mentioned) and `c` is proposed okay. `b` and `d` are obviously okay. Is that the right set of rules? There are other alternatives:

* **`c` is ill-formed**. This might be a little surprising to propose, but it actually has merit. If you want a variable that only exists at compile time, declare it `consteval`. Just because we _can_ come up with a set of rules that allows `c` (by way of having consteval-only type) but not `a` doesn't mean that we should. Rejecting `c` can also provide a clear error message that it should be `consteval` instead and makes for a simpler set of rules.
* **`a` is well-formed**. We can achieve this by having consteval variable escalation apply for all variables, not just templated ones. But when we were discussing [@P2564R3], we didn't do escalation for regular functions then — if you have a function that must be `consteval`, we decided that you should explicitly mark it as such. The same principle should apply here — if `a` has to be `consteval` (and it does), then it should be explicitly labeled as such.

We think the right answer is that only the `consteval` declarations above should be valid. The only way to keep `c` valid would require consteval-only types, but...

## Do we still need consteval-only types?

[@P2996R10] introduces the notion of consteval-only type — basically any type that has a `std::meta::info` in it somewhere — to ensure that reflections exist only at compile time. This paper provides an alternative approach to solve the same problem: extend consteval-only to include values of type `std::meta::info`.

This broadly accomplishes the same thing (and would necessitate having `c` be ill-formed in the above example), there are a few cases where the suggested rules would differ though. For example:

::: std
```cpp
// variant<info, int> is a "consteval-only type"
// but v does not have "consteval-only value"
constexpr std::variant<std::meta::info, int> v = 42;

struct C {
    std::meta::info const* p;
};
// C is a "consteval-only type"
// but c does not have "consteval-only value"
auto c = C{.p=nullptr};
```
:::

On the whole, it's definitely important to ensure that reflections do not persist to runtime and do not lead to codegen. These cases don't _actually_ have reflections in them though. So perhaps we don't need them the concept of consteval-only type after all.

[@P3421R0]{.title} is another paper in this space that also seems like what it is really trying to do is come up with a way to produce consteval-only values. Perhaps a consteval destructor would be a way to signal that.

## Consteval-only Allocation

Consider:

::: std
```cpp
#include <vector>
consteval std::vector<int> a = {1, 2, 3};
consteval int* p = new int(4);
```
:::

The issue we're trying to solve with non-transient allocation ([@P1974R0]{.title}, [@P2670R1]{.title}, and [@P3554R0]{.title}) relies upon dealing with persistence. How do we persist the constant allocation into runtime in a way that is reliably coherent.

But [@P3032R2]{.title} already recognized that there are situations in which a constexpr variable will _not_ persist into runtime, so such allocations _could_ be allowed. The rule suggested in that paper was `constexpr` variables in immediate function contexts. But `consteval` variables allow for a much clearer, more general approach to the problem: an allocation in an initializer of a `consteval` variable could simply leak — even `p` could be allowed. We would have to adopt the rule suggested in P3032 — that any mutation through the allocation after initialization is disallowed (which we can enforce since the variables live entirely at compile time).

The `consteval` specifier also makes clear that these variables would exist only at compile time, and thus there is no jarring code movement difference that the P3032 rule led to — where you can move a declaration from one context to another and that changes its validity.

Note that this also would help address a usability issue with [@P1306R3]{.title}, where we could say that:

::: std
```cpp
template for (consteval info r : members_of(type))
```
:::

desugars into declaring the underlying range `consteval`, which seems like a fairly tidy way to resolve that the allocation issue.

Consteval-only allocation can always be adopted later, it is not strictly essential to this proposal, and we're already late.


# Proposal

This paper proposes:

1. introducing the notion of consteval-only value,
2. introducing consteval variables,
3. allowing certain constexpr variables (those with consteval-only value) to escalate to consteval variables.

Currently, the only kind of consteval-only value is a pointer (or reference) to immediate function. This paper directly also adds consteval variables. With the adoption of [@P2996R10], consteval-only values will extend to include values of type `std::meta::info` (and thus variables of that type will escalate to `consteval`). We won't need consteval-only types.

## Wording

Change [expr.const]{.sref}

::: std

[6]{.pnum} A variable `v` is *constant-initializable* if

* [6.1]{.pnum}  [either]{.addu} the full-expression of its initialization is a constant expression when interpreted as a *constant-expression* [or `v` is an immediate variable and the full-expression of its initialization is an immediate constant expression when interpreted as a *constant-expression*]{.addu},
    [Within this evaluation, `std​::​is_constant_evaluated()` ([meta.const.eval]) returns `true`.]{.note2}
    and
* [6.2]{.pnum} immediately after the initializing declaration of `v`, the object or reference `x` declared by `v` is constexpr-representable, and
* [6.3]{.pnum} if `x` has static or thread storage duration, `x` is constexpr-representable at the nearest point whose immediate scope is a namespace scope that follows the initializing declaration of `v`.

[...]

::: addu
[x]{.pnum} A value is *consteval-only* if it satisfies any of the following:

* [x.1]{.pnum} any constituent reference refers to an immediate function or an immediate variable,
* [x.2]{.pnum} any constituent pointer points to an immediate function or an immediate variable, or
* [x.3]{.pnum} any constituent value of pointer-to-member type designates an immediate function.

[With the adoption of P2996, this would have to be extended to also account for any references to, pointers to, or values of type `std::meta::info`.]{.draftnote}

[y]{.pnum} An *immediate constant expression* is either a glvalue core constant expression that refers to an object or a function, or a prvalue core constant expression whose value satisfies the following constraints:

* [y.1]{.pnum} each constituent reference refers to an object or a function,
* [y.2]{.pnum} no constituent value of scalar type is an indeterminate value ([basic.indet]), and
* [y.3]{.pnum} no constituent value of pointer type has an invalid pointer value ([basic.compound]).
:::

[22]{.pnum} A *constant expression* is either a glvalue [immediate]{.addu} core constant expression [that refers to an object or non-immediate function]{.rm} [whose object does not have consteval-only value]{.addu}, or a prvalue [core]{.rm} [immediate]{.addu} constant expression [whose value satisfies the following constraints]{.rm} [that does not have consteval-only value.]{.addu}

::: rm
* [22.1]{.pnum} each constituent reference refers to an object or a non-immediate function,
* [22.2]{.pnum} no constituent value of scalar type is an indeterminate value ([basic.indet]),
* [22.3]{.pnum} no constituent value of pointer type is a pointer to an immediate function or an invalid pointer value ([basic.compound]), and
* [22.4]{.pnum} no constituent value of pointer-to-member type designates an immediate function.
:::

[...]

[25]{.pnum} An expression or conversion is *immediate-escalating* if it is not initially in an immediate function context and it is either

* [#.1]{.pnum} a potentially-evaluated [*id-expression* that denotes an immediate function]{.rm} [expression that has consteval-only value]{.addu} that is not a subexpression of an immediate invocation, or
* [#.2]{.pnum} an immediate invocation that is not a constant expression and is not a subexpression of an immediate invocation.

[26]{.pnum} An *immediate-escalating* function is [...]

[27]{.pnum} An *immediate function* is [...]

::: addu
[z]{.pnum} An *immediate variable* is:

* [z.1]{.pnum} a variable declared with the `consteval` specifier, or
* [z.2]{.pnum} a variable that results from the instantiation of a templated entity declared with the `constexpr` specifier whose initialization results in a consteval-only value.
:::
:::

Change [dcl.constexpr]{.sref} to account for `consteval` variables:

::: std
[1]{.pnum} The `constexpr` [and `consteval`]{.addu} specifier[s]{.addu} shall be applied only to the definition of a variable or variable template, a structured binding declaration, or the declaration of a function or function template. [The `consteval` specifier shall be applied only to the declaration of a function or function template.]{.rm}
A function or static data member declared with the `constexpr` or `consteval` specifier on its first declaration is implicitly an inline function or variable ([dcl.inline]).
If any declaration of a function or function template has a `constexpr` or `consteval` specifier, then all its declarations shall contain the same specifier.

[...]

[6]{.pnum} A `constexpr` [or `consteval`]{.addu} specifier used in an object declaration declares the object as `const`.
Such an object shall have literal type and shall be initialized.
A `constexpr` [or `consteval`]{.addu} variable shall be constant-initializable ([expr.const]).
A `constexpr` [or `consteval`]{.addu} variable that is an object, as well as any temporary to which a constexpr reference is bound, shall have constant destruction.

:::

## Feature-test Macro

Bump `__cpp_consteval` in [cpp.predefined]{.sref}:

::: bq
```diff
- __cpp_­consteval @[202406L]{.diffdel}@
+ __cpp_­consteval @[20XXXXL]{.diffins}@
```
:::

# Acknowledgments

An earlier draft revision of the paper proposed something much narrower — simply allowing pointers to immediate functions to persist, if those exists as part of `static constexpr` variables in immediate functions. Richard Smith suggested that we generalize this further. That suggestion led us to the much better design that this paper now proposes. Thank you, Richard.

---
references:
  - id: P3554R0
    citation-label: P3554R0
    title: "Non-transient allocation with `std::vector` and `std::basic_string`"
    author:
      - family: Peter Dimov
      - family: Barry Revzin
    issued:
      - year: 2025
        month: 1
        day: 5
    URL: https://wg21.link/p3554r0
  - id: LWG4197
    citation-label: LWG4197
    title: "Complexity of `std::visit` with immediate functions"
    author:
      - family: Jiang An
    issued:
      - year: 2025
        month: 1
        day: 26
    URL: https://wg21.link/lwg4197
---
