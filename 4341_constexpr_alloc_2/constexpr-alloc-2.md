---
title: "Another take on non-transient constexpr allocation"
document: P4341R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: constexpr
highlighting:
  keywords:
    cpp:
      - immutable_if_constexpr
status: progress
---

# Abstract

This papers proposes a new approach to solving the problem of non-transient constexpr allocation. The result is variables of types like `std::vector<T>` and `std::string` are able persist as `constexpr` variables and, with [@P4340R0]{.title}, be usable as constant template parameters.

# Introduction

Relevant reading:

* [@P0784R5]{.title}: The original paper that led to allowing allocation during constant evaluation. This revision proposed a facility named `std::mark_immutable_if_constexpr`, but was eventually pulled.
* [@P1974R0]{.title}: A very thorough description of the problem we have to solve in this space, which proposed a new `propconst` qualifier as a solution to the problem. Revised in [@P1974R1]{.title} to provide a rule for when allocations can persist.

The two designs laid out above basically agree on the rule for persistence:

::: std
Given a constexpr variable `V` whose initialization has an allocation `A`, that allocation is allowed to persist if evaluating the destructor of `V` is a constant subexpression and

* that evaluation would free `A` and
* an object stored in `A` may not be read during that evaluation if `A` is reachable as mutable.

Reads of objects in `A` are not constant expressions if `A` as reachable as mutable.
:::

The difference between the two is: how do we determine if `A` is reachable as mutable? I'm going to refer to this difference between the models as, with apologies to the Motherland, trust OR verify?

* **Trust**: The [@P0784R5] model (`std::mark_immutable_if_constexpr`) simply _trusts_ users to mark allocations properly. If they mark their allocations incorrectly, undefined behavior can occur at runtime.

* **Verify**: The [@P1974R0] model (`propconst`), by construction, ensures that any allocations that ends up being allowed to persist and can be read as constant is sound. No undefined behavior can result (outside of explicit `const_cast` shenanigans, which... sure, fine, whatever. `const_cast` is Latin for "not our fault").

## The `propconst` model

Now, in general, I think everyone would easily prefer a sound model which just works over trusting C++ programmers to do the right thing in an increasingly complex language. However, soundness isn't without cost.

The `propconst` qualifier model does achieve soundness, but it has a very large cost: we are solving a narrow problem (non-transient constexpr allocation) with a very, very global hammer (a new type qualifier). That requires us to truly *globally* understand what the impacts of allowing T propconst as a type are, in every context that accepts a type.

Now, I'm not going to claim that all of these problems that we'd have to work through are unsolvable. I'm not even going to claim that they're necessarily hard. But what I think is clear is that a lot of work will have to go into making sure the rest of the language and library properly deals with the large scale of impact that will happen. And the added benefit of what we'll be able to do as a result of such work is not really commensurate.

## The marking model

On the other hand, the marking (trust) model sounds unsafe and risky. But it actually has exceedingly narrow impact, both in terms of feature scope (we only care about non-transient allocations, so the solution is to mark those allocation) and overall impact. Which makes it immediately more appealing to me.

Now, I used to think that the only way we would run into undefined behavior with the marking model is through *mis-marking*. But I don't think mis-marking is actually a very big deal at all. It's actually not very many types that need to be marked — only types that both manage their own memory *and also* you want to use them as `constexpr` variables (including as constant template parameters). Moreover, the rule for when to mark allocations is actually fairly straightforward: mark if you're deep-const.

* containers should already be deep-const, those mark their allocations
* smart pointers are shallow-const, so only mark if the underlying type is const.

The standard library would mark (or not mark) all of its own types properly, and frankly just `std::vector<T>` and `std::string` alone cover the majority of the value of the feature (which is why we proposed [@P3554R0]{.title}), and it's easy enough for custom versions of those two types to do the right thing too by copying what we do.

As a result, initially I was going to simply propose `std::mark_immutable_if_constexpr(p)` as the design, as originally proposed in [@P0784R5]. I implemented that approach, wrote examples, and started writing a paper. I thought such mis-marking examples weren't a huge concern, like this one:

::: std
```cpp
template <class T>
class bad_unique_ptr {
    T* ptr;

public:
    explicit constexpr bad_unique_ptr(T* p) : ptr(p) { }
    constexpr ~bad_unique_ptr() { std::mark_immutable_if_constexpr(ptr); }

    constexpr auto get() const -> T* { return ptr; }
};

auto main() -> int {
    constexpr auto p = bad_unique_ptr<bad_unique_ptr<int>>(
      new bad_unique_ptr<int>(new int(42))
    );

    p->reset(new int(43)); // oops
}
```
:::

Yes, the above would be undefined behavior at runtime, but it's an easy problem to avoid.

However, after implementing it and doing some experimentation, I realized that mis-marking isn't the only potential source of undefined behavior in this model. We also have *interior pointers*. Consider these examples:

::: std
```cpp
struct A {
    std::vector<int> v;
    int* p;
};

constexpr A a = []{
    std::vector<int> v = {1, 2, 3};
    int* p = v.data();
    return A{.v=std::move(v), .p=p};
}();

struct B {
    int i;
    int* p;

    constexpr B(int i) : i(i), p(&this->i) { }
    constexpr B(B const& rhs) : i(rhs.i), p(&i) { }
};

constexpr std::vector<B> bs = {1, 2, 3};

auto main() -> int {
    ++a.p[0];      // oops
    ++bs[0].p[0];  // oops
}
```
:::

Both of these would be rejected by the `propconst` model, since the allocations here are reachable as mutable. But the marking model effectively whitelists the entire allocation, it's not member-by-member. In both cases here, we allow the allocation — even though we can subvert that.

With mis-marking, it's easy to point to whose fault it is and how to fix it. Just don't mark. With the above examples, it's not so easy? Just don't use interior pointers inside types that want to persist allocations? Not a very compelling rule.

## An alternative marking model

In [@P2670R0]{.title}, I tried to propose a significantly narrower form of `propconst`, such that we still get verification but we don't have to have such a global hammer with far-reaching impact. That doesn't pan out because of how exactly we would need to be able to specify how we propagate `const`-ness on a specifier level (see [@P2670R1] for the `vector` and `Matrix` examples). At this point, I've become convinced that if we really want a verification-based model for ensuring that we have no undefined behavior, we have to go with a `propconst` type qualifier — which as described above, I don't think we should do.

But instead of introducing a `propconst` specifier to try to still give us the behavior of the `propconst` qualifier, what if we instead introduced an `immutable_if_constexpr` specifier that gave us the behavior of `std::mark_immutable_if_constexpr(p)`?

`std::mark_immutable_if_constexpr(p)` globally blesses the allocation that `p` points to, but doesn't give us any pathing differentiation of how we got there. It solves a lot of problems for us, but gives us no mechanism to reject `A` or `B` in the above example.

But if instead of we individually blessed paths to an allocation, by way of adding a specifier on non-static data members, we could get both. That is, a simplified version of `vector` and `unique_ptr` would look like:

::: std
```cpp
template <class T>
class vector {
  immutable_if_constexpr T* begin_;
  immutable_if_constexpr T* end_;
  immutable_if_constexpr T* capacity_;

public:
  constexpr ~vector() {
    delete [] begin_;
  }
};

template <class T>
class unique_ptr {
  immutable_if_constexpr(std::is_const_v<T>) T* p_;

public:
  constexpr ~unique_ptr() {
    delete p_;
  }
};
```
:::

We still have the rule about reachable-as-mutable. It's just that we consider any path from a data member marked `immutable_if_constexpr` as if it were immutable. We're not _verifying_ that it's actually only ever reachable as const, we're *trusting* that the marking is correct.

# Proposal

The concrete design I'm proposing is roughly as follows.

At the end of the initialization of a constexpr variable `V`, before the hypothetical destruction runs, for each surviving allocation `A`, we walk through every pointer and reference into `A` stored anywhere in the graph (in `V` itself or in any surviving allocation) and classify each such path as **blessed** or **unblessed**:

* A path is **blessed** if the pointer/reference is stored in (a subobject of) a data member declared `immutable_if_constexpr(true)` OR if the pointer/reference's declared pointee/referent type is const-qualified and not a class type with `mutable` members.
* Otherwise, the path is **unblessed**.

Then `A` is classified:

|Paths into `A`|Result|
|-|-|
|all blessed, >=1 via an `immutable_if_constexpr` member|immutable|
|all blessed, but only via const-typed paths|mutable persistence|
|some unblessed, none via `immutable_if_constexpr`|mutable persistence|
|mixed: some path via `immutable_if_constexpr`, some unblessed|ill-formed (conflicting intent)

The rest of the persistence rules are:

* During the synthesized destruction of `V`, `A` must be freed. Nothing leaks.
* The persisted image is the end-of-initialization state; destruction-time writes are discarded.
* An object in a surviving allocation may be read if and only if the allocation was classified as immutable.
* A successfully persisted allocation retains its identity and is promoted to static storage; no destructor is invoked at runtime. Pointers and references into such storage are usable as constant template parameters. The identity rule is a function of `(V, allocation index, offset)` and would require V to have linkage. This would probably require an ABI extension.
* Contents of immutable allocations are usable in later constant expressions; contents of mutable ones are not. Runtime writes to immutable allocations, and any independent deallocation, are UB.
* Persisted allocations of consteval variables have consteval-only address.

## Why composition

The compositional aspect here (blessing distributes through members) is necessary to handle a particular. Let's say I want to write a deep-const type `C` that off-shores its allocation onto `std::unique_ptr<int>`. Something like this:

::: std
```cpp
class C {
    std::unique_ptr<int> p_;

public:
    constexpr C(int i): p_(new int(i)) { }
    constexpr auto get() -> int& { return *p_; }
    constexpr auto get() const -> int const& { return *p_; }
};

constexpr auto c = C(42);
static_assert(c.get() == 42);
```
:::

It's worth walking through that that implementation would look like in each of the three models. In all cases, the constructor and getters look the same, so I'm not going to repeat them:

::: std
```cpp
// with propconst, we use the type system to push
// deep const-ness into unique_ptr itself
class C1 {
    std::unique_ptr<int propconst> p_;
};

// with the way I'd implemented mark_immutable_if_constexpr
// we mark in the destructor. unique_ptr wouldn't here, because
// it's non-const, but we can do it ourselves
class C2 {
    std::unique_ptr<int> p_;

public:
    constexpr ~C2() { std::mark_immutable_if_constexpr(p_.get()); }
};

// with the specifier, it's just up to us to mark
class C3 {
    immutable_if_constexpr std::unique_ptr<int> p_;
};
```
:::

## Examples

You can see all of these on [compiler explorer](https://compiler-explorer.com/z/YMT6GWx4T).

Ex 1: This is still ill-formed, because p leaks memory.

::: std
```cpp
constexpr int* p = new int(1); // error
```
:::

Ex 2: The allocation is fine and persists, but we can't read through it as constant:

::: std
```cpp
constexpr std::unique_ptr<int> p(new int(2)); // ok
static_assert(*p == 2); // error
void bump() { ++*p; } // ok! mutable at runtime
```
:::

Ex 3: Now, we can read through it as constant, because it will have been marked:

::: std
```cpp
constexpr unique_ptr<int const> p(new int(3)); // ok
static_assert(*p == 3); // ok
```
:::

Ex 4: This is not okay, because destroying the outer unique_ptr will try to read through its allocation when it destroys the inner one:

::: std
```cpp
constexpr unique_ptr<unique_ptr<int>> p(new unique_ptr<int>(new int(4))); // error
```
:::

Ex 5: But this recursion works fine:

::: std
```cpp
constexpr vector<string> v = {"this", "is", "so", "cool"};
static_assert(v[1] == "is");
```
:::

Ex 6: Marking the inner unique_ptr const is fine, it allows the allocation to persist, but the innermost int read isn't constant.

::: std
```cpp
constexpr unique_ptr<unique_ptr<int> const> p(
    new unique_ptr<int> const(new int(6)));    // ok
static_assert((*p).get() != nullptr);          // ok: outer allocation marked
static_assert(**p == 6);                       // error: inner allocation unmarked
int& r = **p;                                  // ok: and mutable at runtime!
```
:::

Ex 7: Here is a mixed example:

::: std
```cpp
constexpr auto v = []{
    vector<unique_ptr<int>> v;
    v.push_back(make_unique<int>(1));
    v.push_back(make_unique<int>(2));
    return v;
}();                                    // ok
static_assert(v.size() == 2);           // ok: buffer marked
static_assert(v[0] != nullptr);         // ok: the unique_ptr objects are readable
static_assert(*v[0] == 1);              // error: pointees unmarked
void f() { *v[1] = 20; }                // ok: pointees runtime-mutable
```
:::

Ex 8: `std::map` would work fine:

::: std
```cpp
constexpr std::map<std::string_view, int> m = {{"one",1},{"two",2}}; // ok
static_assert(m.at("two") == 2);                                     // ok
```
:::

Ex 9: `std::shared_ptr` could not work because of the control block (the destructor must read the control block), so it could not be marked, even for `const`:

::: std
```cpp
constexpr shared_ptr<int const> sp = make_shared<int>(1);  // error, and rightly so
```
:::

Ex 10: Constant template parameter usage is not content based.

::: std
```cpp
template <int const* P> struct X { };
constexpr vector<int> v1 = {1, 2, 3};
constexpr vector<int> v2 = {1, 2, 3};

X<v1.data()> x1; // ok
X<v2.data()> x2; // ok
static_assert(type_of(^^x1) != type_of(^^x2)); // ok: these have different types
```
:::

Ex 11: Persisted allocations are permitted results:

::: std
```cpp
constexpr string s = "Some sufficiently long string as to definitely allocate";
constexpr string_view sv = s; // ok
```
:::

Ex 12: Expanding over `members_of` works:

::: std
```cpp
struct Point { int x, y; };
auto loop(Point const& p) -> void {
    constexpr auto ctx = meta::access_context::current();
    template for (constexpr meta::info r : nonstatic_data_members_of(^^Point, ctx)) {
        // look ma, no define_static_array
        (void)p.[:r:];
    }
}
```
:::

Ex 13: These interior pointer examples are rejected, since they would other expose mutable writes into immutable allocations:

::: std
```cpp
struct A {
    std::vector<int> v;
    int* p;
};

constexpr A a = []{
    std::vector<int> v = {1, 2, 3};
    int* p = v.data();
    return A{.v=std::move(v), .p=p};
}(); // error

struct B {
    int i;
    int* p;

    constexpr B(int i) : i(i), p(&this->i) { }
    constexpr B(B const& rhs) : i(rhs.i), p(&i) { }
};

constexpr std::vector<B> bs = {1, 2, 3}; // error
```
:::

Ex. 14: Deep const adaptation

::: std
```cpp
class C {
    immutable_if_constexpr std::unique_ptr<int> p_;

public:
    constexpr C(int i): p_(new int(i)) { }
    constexpr auto get() -> int& { return *p_; }
    constexpr auto get() const -> int const& { return *p_; }
};

constexpr auto c = C(42);
static_assert(c.get() == 42); // ok
```
:::

## Implementation Experience

Implemented in clang, here is the [the patch](https://github.com/brevzin/llvm-project/commit/eeaef62808baf7fe5d2c49e0110ca4e3b0a99ffa) (this commit actually both removes the `std::mark_immutable_if_constexpr` implementation and adds the `immutable_if_constexpr` specifier one) You can see what the opt-ins are for `vector`, `string`, and `unique_ptr`.