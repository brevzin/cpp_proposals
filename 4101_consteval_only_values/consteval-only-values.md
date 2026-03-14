---
title: "Consteval-only Values for C++26"
document: P4101R0
date: today
audience: EWG, CWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Peter Dimov
      email: <pdimov@gmail.com>
toc: true
status: progress
tag: reflection
---

# Introduction

The Reflection design from [@P2996R13] was based on a model of having consteval-only types to prevent reflections from leaking to runtime. But we've run into issues and limitations with that approach, so we propose that, for C++26, we change instead to a consteval-only value model. This solves the same problems, but has additional benefits.

# Issues with Consteval-only Types

The consteval-only type model is simple. Any type with `std::meta::info` in it somewhere is consteval-only. This includes chasing pointers and references, function types, class members, and pointer-to-member types. There has been some desire voiced to simply allow `info` to persist to runtime, and having it simply be an empty type in which nothing is observable, but up until recently, there weren't really any problems expressed with the model itself.

However, Jakub Jelinek pointed out in January 2026 that whether a type is consteval-only is not actually a static property. This led to [@CWG3150]{.title}:

::: quote
A class type is consteval-only depending on its member types. However, a class type may be incomplete, and thus the question cannot be answered where needed.

For example,

::: std
```cpp
struct S;
void f(S*);   // #1
struct S {    // #2
  std::meta::info x;
};
```
:::

Does the class definition at #2 make the function declaration #1 retroactively ill-formed? What if #1 and #2 are not mutually reachable?
:::

This is a problem. Note also that class incompleteness might apply to members — rather than `S` being incomplete, `S` could have a member `U*` where `U` is incomplete. We cannot say definitively whether a type is consteval-only or not.

Discussing this issue led to the discovery of a closely-related problem:

::: std
```cpp
template <class T> struct S;
void f(S<int>*);
```
:::

Consteval-only types include function types. Does the above need to instantiate `S` to determine whether `S` is consteval-only, because the completeness of `S<int>` affects the semantics of the problem? Currently, we do _not_ need to instantiate `S` there, and indeed lots of code relies on this lack of instantiation.

## Consteval-only Types are Abstract Types

One suggested approach of how to resolve these issues (the incompleteness issue and the extra-instantiation issue) was to borrow from the closest analogue we have to this problem in C++ today: abstract class types. [@P0929R2]{.title} opened with:

::: quote
Subclause 13.4 [class.abstract] paragraph 3 states:

> An abstract class shall not be used as a parameter type, as a function return type, or as the type of an explicit conversion. Pointers and references to an abstract class can be declared.

This is troublesome, because it requires checking a property only known once a type is complete at a position (declaring a function) where the rest of the language permits incomplete types to appear.

In practice, this introduces spooky "backward-in-time" errors:

::: std
```cpp
struct S;
S f();   // #1, ok

// lots of code

struct S { virtual void f() = 0; };  // makes #1 retroactively ill-formed
```
:::
:::

Which is pretty familiar!

That paper refined the ways in which we handle abstract class type checking by only checking for abstract-ness at the point of definition. The declaration of `f()` above there is fine, but the definition of `f()` would require (and check) that `S` is not abstract.

Note, however, that pointers and references to abstract class types are not themselves abstract class types. This led to the question...

## Must Pointers and References to Consteval-only Types be Consteval-only?

Consider the following example:

::: std
```cpp
constexpr std::meta::info r = ^^int;

/* not constexpr */ std::meta::info const* p = &r;
constexpr std::meta::info const* q = &r;
```
:::

`r` does not exist at runtime, so the declaration of `p` needs to be ill-formed, while the declaration of `q` is fine. If `std::meta::info const*` is a consteval-only type, then the declaration of `p` is rejected by our consteval-only type rule in [basic.types.general]{.sref}/12:

::: std
[12]{.pnum} [...] Every object of consteval-only type shall be

* [12.8]{.pnum} the object associated with a constexpr variable or a subobject thereof,
* [#.#]{.pnum} a template parameter object ([temp.param]) or a subobject thereof, or
* [#.#]{.pnum} an object whose lifetime begins and ends during the evaluation of a core constant expression.
:::

The declaration of `q` is fine.

But if `std::meta::info const*` is no longer a consteval-only type, what happens? The reasoning gets surprising complicated.

For `p`, `&r` is an immediate-escalating expression (because `r` has consteval-only type), which means it has to be in an immediate function context, which means it has to be manifestly constant-evaluated, which means that the full-initialization has to be a constant expression. But now we violate what used to be called the "permitted result of a constant expression" rule in [expr.const]{.sref}/21:

::: std
[21]{.pnum} A _constant expression_ is either

* [21.1]{.pnum} a glvalue core constant expression [...]
  or
* [21.2]{.pnum} a prvalue core constant expression whose result object ([basic.lval]) satisfies the following constraints:
  * [#.#.1]{.pnum} [...]
  * [#.#.5]{.pnum} unless the value is of consteval-only type,
    * [#.#.#.1]{.pnum} no constituent value of pointer-to-member type points to a direct member of a consteval-only class type,
    * [#.#.#.#]{.pnum} no constituent value of pointer type points to or past an object whose complete object is of consteval-only type, and
    * [#.#.#.#]{.pnum} no constituent reference refers to an object whose complete object is of consteval-only type.
:::

Status quo, we don't go into 21.2.5 because `std::meta::info const*` is a consteval-only type. But it weren't, we do, and we violate 21.2.5.2 because `&r` points to an object whose complete type is consteval-only. Which, great, `p` is rejected.

_However_, `q` is _also_ rejected for the exact same reason. The declaration of `q` is also ill-formed. And this is a real problem because:

::: std
```cpp
constexpr std::meta::info arr[] = {^^int, ^^char};
constexpr std::span<std::meta::info const> s = arr;
```
:::

Status quo: this is fine. And it is very important that this work because this is our workaround for the lack of non-transient constexpr allocation via `std::define_static_array()`, so approximately everybody who uses reflection will run into needing this.

But if `std::info const*` is no longer a consteval-only type, then `std::span<std::meta::info const>` wouldn't be either, and again we violate the "permitted result" rule because we have a non-consteval-only type with a constituent value of pointer type that points to an object of consteval-only type.

## What is the Conclusion?

The conclusion here is that in the consteval-only type model, we simply have to follow all the type edges. Pointers, references, functions, class subobjects, etc. But once we do that, we simply do not know if we have a way to properly reject invalid uses in the presence of incomplete types. I don't think we even know how to solve this problem.

Note for interest that the Zig programming language has consteval-only types in the exact way that we want to define here (comptime-only in their parlance), but they don't have incomplete types in the same way, so they don't run into this issue.

# Consteval-only Values

There is a different approach to restricting certain values to not persist until runtime: enforce the rules at a _value_ level instead of a _type_ level. It's not objects of type `std::meta::info` that must live at compile-time, it's that reflection values must live at compile-time.

C++ today already has a notion of a value that is only allowed to exist during compile time: consteval functions. We already used to the fact that our constant rules are value-based rather than type based:

::: std
```cpp
consteval auto add(int x, int y) -> int { return x + y; }
constexpr auto sub(int x, int y) -> int { return x - y; }

constexpr auto p = add; // error
constexpr auto q = sub; // ok
```
:::

We already explored this idea in [@P3603R1]{.title}, but what if we generalized the notion we have today and combined it with reflections? Informally:

::: std
An expression has _consteval-only value_ if:

1. it is a reflection,
2. it has a constituent pointer/reference to an immediate function, or
3. it has a constituent pointer/reference to an immediate variable.

An _immediate variable_ is a `constexpr` variable whose initialization produces a consteval-only value.
:::

And then we split our term "constant expression" into two because a `constexpr` variable can be initialized with consteval-only value (this escalates it to an immediate variable) but a regular variable cannot be (because otherwise our consteval-only value would not be very consteval-only).

Going back to our earlier example:

::: std
```cpp
constexpr std::meta::info r = ^^int;

/* not constexpr */ std::meta::info const* p = &r;
constexpr std::meta::info const* q = &r;
```
:::

The new way of thinking about these declarations is:

* For `r`: `^^int` is a consteval-only value (it is a reflection), so it must be part of a constant expression. `r` is declared `constexpr`, so it is allowed to have consteval-only value. It becomes an immediate variable.
* For `p`: `&r` is a consteval-only value (it is a pointer to an immediate variable), so it must be a part of a constant expression. `p` is not declared `constexpr`, so its initializer is not allowed to have consteval-only value, so this is ill-formed.
* For `q`: As above, `&r` is a consteval-only value, but because `q` is declared `constexpr`, it is allowed to have consteval-only value. It becomes an immediate variable.

Similar reasoning shows that this other example, both `arr` and `s` are valid declarations and become immediate variables.

::: std
```cpp
constexpr std::meta::info arr[] = {^^int, ^^char};
constexpr std::span<std::meta::info const> s = arr;
```
:::

## As a Solution to CWG 3150

Because values (unlike types) during constant evaluation are always complete, we mostly avoid any issues arising from incomplete types. This side-steps the issue from [@CWG3150].

There are still some cases that would become IFNDR though:

::: std
```cpp
// TU #1
struct S;
extern S const s;
S const* p = &s;

// TU #2
struct S { std::meta::info r; };
constexpr S s = {.r = ^^int};
```
:::

In TU `#2`, `s` is an immediate variable. It will not persist to runtime. Attempting to use its address in TU `#1` just won't link, because there won't be a definition. But that's okay.

But the value approach also means that we don't even have to think about the instantiation issue:

::: std
```
template <class T> struct S;
void f(S<int>*);
```
:::

If `S<int>` has a member of type `std::meta::info` and that member is initialized with `^^int`, then that object will necessarily have to live at compile-time, and you just won't be able to invoke a non-`constexpr` function with a pointer to it. It might be a bug to have not declared it `constexpr`, but that simply makes it useless — not problematic.

## Differences in Treatment Between Consteval-only Types and Consteval-only Values

If we transition to having our model be based on consteval-only _values_ rather than consteval-only _types_, mostly the same issues are diagnosed — if for different reasons. But there are a few notable instances where the consteval-only type rule does diagnose a misuse but the consteval-only value does not:

::: std
```cpp
// consteval-only type: this is ill-formed
// consteval-only value: ok: x is null, it doesn't actually hold a reflection value
std::meta::info x;

// consteval-only type: this is ill-formed for several reasons: f needs to
// be consteval and bit_cast mandates that neither the source nor destination
// type is a consteval-only type.
// consteval-only value: this is... simply useless
auto f(std::meta::info r) -> u64 { return std::bit_cast<u64>(r); }
```
:::

There are other examples in this vein: the consteval-only type rule rejects some code that the consteval-only value rule allows to be well-formed but useless.

Some other examples might be potentially-useful:

::: std
```cpp
// consteval-only type: this is ill-formed because v has consteval-only type
// consteval-only value: there is no actual reflection here, so this is fine
auto v = std::variant<std::meta::info, int>(42);

// similar
struct C { std::meta::info const* p; };
auto c = C{.p = nullptr};
```
:::

## Ancillary Benefits of Consteval-only Values

The consteval-only value rule gives us a way to keep reflections at compile-time with a simpler, more-easily-enforceable rule than the consteval-only type rule. But the consequences aren't just that some useless code is allowed to hang around. It also provides us significant ancillary benefits.

### Variant Visitation

P3603 was motivated by the [variant visitation example](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3603r1.html#motivating-example-variant-visitation):

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

    // This constexpr variable will be initialized to
    // an array of pointers to consteval functions.
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

Note that there are no consteval-only types in this example, but it is representative of the case where the `variant` being visited actually has reflections in it. This example is ill-formed today. The linked paper has a longer description, but basically `fptrs` is a `constexpr` variable that is initialized to an array of pointers to consteval functions. That is disallowed today.

But with the consteval-only value rule, that's fine — it just becomes an immediate variable. That causes usage of it to escalate: the particular specialization of `visit` becomes a `consteval` function, at which point everything just works. This means that variant visitation with existing implementations using variants containing `std::meta::info` or otherwise visiting with a `consteval` function start to work without any code changes required.

### Consteval Functions as Constant Template Arguments

Similarly, use of `consteval` functions will explode with C++26 with the adoption of Reflection. And one such use will be as constant template parameters. [This example](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2025/p3603r1.html#motivating-example-immediate-functions) also was noted in P3603R1:

::: std
```cpp
consteval auto always_false() -> bool { return false; }

static_assert(std::not_fn(always_false)());   // OK
static_assert(std::not_fn<always_false>()()); // ill-formed
```
:::

Status quo is this is ill-formed (although gcc accepts). With this change, the example becomes well-formed with the desired semantics, with no library code change necessary.

### Consteval-only Allocation (Future Work)

Not proposed in this paper, but consider:

::: std
```cpp
template for (constexpr std::meta::info m : members_of(type, ctx)) { ... }
```
:::

This internally constructs a `constexpr` variable of type `std::vector<std::meta::info>`, which requires allocation (unless `type` happens to have no members — which again, not to belabor the point, but this rule is based on the _value_ of the range, not its type). Now, non-transient `constexpr` allocation is a problem that we're trying to solve anyway — but in this case, the values we're talking about are consteval-only values. That allocation _cannot_ persist to runtime anyway. And if it cannot persist to runtime, then there's no actual non-transient allocation problem to be solved.

The consteval-only value model gives us a clear path to simply accepting the above formulation, without having to either wait for a general solution or require the user to wrap every call in `define_static_array`. We are not proposing this for C++26 though.

# Proposal

We propose to _replace_ the existing consteval-only type model with a consteval-only value model, where a consteval-only value is defined as being:

1. a reflection,
2. a constituent pointer or reference to an immediate function, or
3. a constituent pointer or reference to an immediate variable

where an immediate variable is a `constexpr` variable whose initialization produces a consteval-value. Note that unlike [@P3603R1], this paper does not propose the ability to _explicitly_ declare an immediate variable. Explicit `consteval` variables will be a C++29 feature.

All of the relevant rules in [expr.const]{.sref} around escalation will change to consider consteval-only values and immediate variables instead of consteval-only types. The type trait `is_consteval_only` will be removed. The other use of consteval-only type is the mandates on `bit_cast`, which can change to add `std::meta::info` to the category of types with constexpr-unknown representation — [@CWG2765]{.title} — since pointers and references are already rejected, there likewise isn't any issue here with incompleteness.

---
references:
    - id: CWG3150
      citation-label: CWG3150
      title: Incomplete consteval-only class types
      author:
        - family: Jakub Jelinek
      issued:
        - year: 2026
          month: 01
          day: 19
      URL: https://cplusplus.github.io/CWG/issues/3150.html
---