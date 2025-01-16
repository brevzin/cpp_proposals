---
title: "Permitting pointers to immediate functions to persist"
document: P3603R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
---

# Introduction

Jiang An submitted a very interesting bug report to [libstdc++](https://gcc.gnu.org/bugzilla/show_bug.cgi?id=118434) (and [libc++](https://github.com/llvm/llvm-project/issues/118560)) in January 2025. It dealt with visiting a `std::variant` with a consteval lambda.

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
            &visit<0, 0>,
            &visit<0, 1>,
            &visit<1, 0>,
            &visit<1, 1>
        };
    }

    static constexpr std::array fptrs = get_array();
};

template <class R, class F, class V0, class V1>
constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
    using Impl = binary_vtable_impl<R, F, V0, V1>;
    return Impl::fptrs[v0.index * 2 + v1.index]((F&&)f, v0, v1);
}

consteval auto func(const Variant<int, long>& v1, const Variant<int, long>& v2) {
    return visit<int>([](auto x, auto y) consteval { return x + y; }, v1, v2);
}

static_assert(func(Variant<int, long>{42}, Variant<int, long>{1729}) == 1771);
```
:::

Here, the lambda `[](auto x, auto y) consteval { return x + y; }` is `consteval`. It is invoked in multiple instantiations of `binary_vtable_impl<...>::visit<...>`, which causes those `constexpr` functions to escalate into `consteval` functions, due to [@P2564R3]{.title} (otherwise the invocation would already be ill-formed). `get_array()` is returning an array of 4 function pointers into different instantiations of those functions, which are all `consteval` — and that array is stored as the `static constexpr` data member `fptrs`.

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

# Relaxing the Rule

We have the rule that constituent values cannot point to an immediate function (previously, this was the "permitted result of a constant expression" rule) to avoid leaking immediate functions to runtime. In the simplest case, we need to reject this:

::: std
```cpp
consteval int add(int x, int y) { return x + y; }

constexpr auto ptr = add;
```
:::

If that initialization were allowed to succeed, then `ptr` is a totally normal `int(*)(int, int)` and nothing prevents me from calling it at runtime. Defeating the purpose of the `consteval` specifier.

The reduced code I showed _cannot_ work. It needs to remain ill-formed, because otherwise nothing stops you from calling `binary_vtable_impl<...>::fptrs[0]` at runtime. It's just a function pointer. However, it wouldn't make for much of an interesting paper if I showed some code that doesn't work and concluded by simply saying it cannot work. Let's consider a slightly different implementation:

::: cmptable
### `static constexpr` data member
```cpp
template <class R, class F, class V0, class V1>
struct binary_vtable_impl {
    template <int I, int J>
    static constexpr auto visit(F&& f,
                                V0 const& v0,
                                V1 const& v1) -> R {
        return f(v0.template get<I>(),
                 v1.template get<J>());
    }

    static constexpr auto get_array() {
        return std::array{
            &visit<0, 0>,
            &visit<0, 1>,
            &visit<1, 0>,
            &visit<1, 1>
        };
    }

    static constexpr std::array fptrs = get_array();
};

template <class R, class F, class V0, class V1>
constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
    using Impl = binary_vtable_impl<R, F, V0, V1>;

    return Impl::fptrs[v0.index * 2 + v1.index]((F&&)f, v0, v1);
}
```

### `static constexpr` local variable
```cpp
template <class R, class F, class V0, class V1>
struct binary_vtable_impl {
    template <int I, int J>
    static constexpr auto visit(F&& f,
                                V0 const& v0,
                                V1 const& v1) -> R {
        return f(v0.template get<I>(),
                 v1.template get<J>());
    }

    static constexpr auto get_array() {
        return std::array{
            &visit<0, 0>,
            &visit<0, 1>,
            &visit<1, 0>,
            &visit<1, 1>
        };
    }
};



template <class R, class F, class V0, class V1>
constexpr auto visit(F&& f, V0 const& v0, V1 const& v1) -> R {
    using Impl = binary_vtable_impl<R, F, V0, V1>;
    static constexpr std::array fptrs = Impl::get_array();
    return fptrs[v0.index * 2 + v1.index]((F&&)f, v0, v1);
}
```
:::

The one on the right only became valid in C++23 — this was [@P2647R1]{.title} — while the one on the left was valid in C++17. But everything else is the same. `visit` is still a `static constexpr` function templated on the variant indices. We still have `get_array()`. However, instead of `fptrs` being a `static constexpr` data member of `binary_vtable_impl`, it is declared locally inside of the namespace-scope `visit`. Does this matter?

Well, not yet. This is still ill-formed (although gcc [accepts](https://godbolt.org/z/o5zejor6z) the one on the right), for the same exact reason — we're initializing a `constexpr` variable with something that is not a constant expression.

But there's a big difference. On the left, `binary_vtable_impl<...>::fptrs[0]` could be invoked at runtime. It would leak, so it must be rejected. But, on the right, the local `fptrs` _cannot_ be invoked at runtime. It does not leak, so it need not be rejected. We could relax the rule to allow the local `fptrs` declaration.

## Didn't You Already Propose Something Like This?

In [@P3032R2]{.title}, I did propose something similar. Let's put both ideas together in one short example:

::: std
```cpp
consteval int add(int x, int y) { return x + y; }

constexpr std::vector<int> v_outer = {1, 2, 3};
constexpr auto f_outer = add;

consteval void immediate() {
  constexpr std::vector<int> v_inner = {1, 2, 3};
  constexpr auto f_inner = add;
}
```
:::

P3032 proposed allowing `v_inner` even though `v_outer` would still be invalid. But that was intended to be a stop-gap, we always wanted `v_outer` to _also_ be valid (and [@P3554R0]{.title} attempts to do that), it's just that making `v_inner` valid was easier.

This case is different though. Here, `f_outer` _cannot_ be valid while `f_inner` can be. It's not a question of choosing which parts to allow, it's that we fundamentally must reject one — but do not have to reject both.

# Proposal

Permit the initialization of a `constexpr` variable in an immediate function to have constituent values that refer or point to immediate functions. With the adoption of [@P2996R9], this would also include consteval-only types.

Change [expr.const]{.sref}:

::: std
[6]{.pnum} A variable `v` is *constant-initializable* if

* [6.1]{.pnum}  [either]{.addu} the full-expression of its initialization is a constant expression when interpreted as a *constant-expression* [or `v` is in an immediate function context and the full-expression of its initialization is an immediate constant expression when interpreted as a *constant-expression*]{.addu},

    [Within this evaluation, `std​::​is_constant_evaluated()` ([meta.const.eval]) returns `true`.]{.note2}

    and
* [6.2]{.pnum} immediately after the initializing declaration of `v`, the object or reference `x` declared by `v` is constexpr-representable, and
* [6.3]{.pnum} if `x` has static or thread storage duration, `x` is constexpr-representable at the nearest point whose immediate scope is a namespace scope that follows the initializing declaration of `v`.
:::

and

::: std
::: addu
[x]{.pnum} An *immediate constant expression* is either a glvalue core constant expression that refers to an object or a function, or a prvalue core constant expression whose value satisfies the following constraints:

* [x.1]{.pnum} each constituent reference refers to an object or a function,
* [x.2]{.pnum} no constituent value of scalar type is an indeterminate value ([basic.indet]), and
* [x.3]{.pnum} no constituent value of pointer type has an invalid pointer value ([basic.compound]).
:::

[22]{.pnum} A *constant expression* is either a glvalue [immediate]{.addu} core constant expression [that refers to an object or a non-immediate function]{.rm} [does not refer to an immediate function]{.addu}, or a prvalue [core]{.rm} [immediate]{.addu} constant expression whose value satisfies the following constraints:

* [22.1]{.pnum} [each constituent reference refers to an object or a non-immediate function]{.rm} [no constituent reference refers to an immediate function]{.addu},
* [22.2]{.pnum} [no constituent value of scalar type is an indeterminate value ([basic.indet])]{.rm},
* [22.3]{.pnum} no constituent value of pointer type is a pointer to an immediate function [or an invalid pointer value ([basic.compound])]{.rm}, and
* [22.4]{.pnum} no constituent value of pointer-to-member type designates an immediate function.
:::

## Feature-Test Macro

## Feature-test Macro

Bump `__cpp_consteval` in [cpp.predefined]{.sref}:

::: bq
```diff
- __cpp_­consteval @[202406L]{.diffdel}@
+ __cpp_­consteval @[20XXXXL]{.diffins}@
```
:::

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
  - id: P2996R9
    citation-label: P2996R9
    title: "Reflection for C++26"
    author:
      - family: Wyatt Childers
      - family: Peter Dimov
      - family: Dan Katz
      - family: Barry Revzin
      - family: Andrew Sutton
      - family: Faisal Vali
      - family: Daveed Vandevoorde
    issued:
      - year: 2025
        month: 1
        day: 12
    URL: https://wg21.link/p2996r9

---
