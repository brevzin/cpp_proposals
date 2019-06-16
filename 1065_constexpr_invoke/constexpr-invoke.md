---
title: "`constexpr` _`INVOKE`_"
document: P1065R1
audience: LWG
date: today
author:
	- name: Tomasz Kamiński
	  email: <tomaszkam@gmail.com>
	- name: Barry Revzin
	  email: <barry.revzin@gmail.com>
---

# Revision History

Since [@P1065R0], just wording changes to correctly describe what it means for
things `bind` to be `constexpr` and also including `bind_front()`.

# Motivation

Currently, one of the most important utility functions in the standard libary,
`std::invoke()`, is not `constexpr`. Even though `std::apply()` and
`std::visit()`, both of which rely on <code>*INVOKE*</code>, are both `constexpr`.
The standard library thus finds itself in an odd state where `std::invoke()` is
and is not `constexpr`. 

The reason that `std::invoke()` is not `constexpr` has some interesting history
associated with it. But at this point, it is simply history, and there is no
further blocker to making this change. This proposal resolves [@LWG2894] but also 
goes one step further and addresses various other <code>*INVOKE*</code>-related
machinery.

# History

Our tale beings in April, 2015 with [@llvm23141], which presented this code which
broke in clang in C++14 (but had compiled in C++11 mode) due to the introduction
of a `constexpr __invoke` (which ended up breaking range-v3):

```cpp
#include <functional>
#include <type_traits>

struct Fun
{
  template<typename T, typename U>
  void operator()(T && t, U && u) const
  {
	static_assert(std::is_same<U, int &>::value, "");
  }
};

int main()
{
	std::bind(Fun{}, std::placeholders::_1, 42)("hello");
}
```

as well as the similar [@llvm23135], which was about this program:

```cpp
template<typename T>
int f(T x)
{
	return x.get();
}

template<typename T>
constexpr int g(T x)
{
	return x.get();
}

int main() {

  // O.K. The body of `f' is not required.
  decltype(f(0)) a;

  // Seems to instantiate the body of `g'
  // and results in an error.
  decltype(g(0)) b;

  return 0;
}
```
    
In both cases the fundamental issue was eager instantiation of the body, which
doesn't actually seem necessary to determine the results here. In neither example
is the return type deduced.

These are incarnations of [@CWG1581], which dealt with the question of when,
exactly, are `constexpr` functions defined. In the broken programs above, the
`constexpr` functions (the non-`const` call operator of the binder object being
returned in the first case and `g()` in the second) were eagerly instantiated,
triggering hard compile errors, in cases where the program ultimately would not
have required their instantiation. 

Thankfully, this difficult problem has been resolved by the adoption of
[@P0859R0] in Albuquerque, 2017. As a result, both of the above programs are
valid. 

This issue was the blocker for having a `constexpr std::invoke()` due to this
eager instantiation issue - which no longer exists. 

# Proposal

This proposal adds `constexpr` to the following <code>*INVOKE*</code>-related
machinery: `invoke()`, `reference_wrapper<T>`, `not_fn()`, `bind()`,
`bind_front()`, and `mem_fn()`. The remaining non-`constexpr` elements of the
library that are <code>*INVOKE*</code>-adjacent are `function<Sig>`,
`packaged_task<Sig>`, `async()`, `thread`, and `call_once()`.

This proposal resolves [@LWG2894], [@LWG2957], and [@LWG3023]. The last is
addressed by guaranteeing that call wrappers that are produced by `not_fn()` and
`bind()` have the same type if their state entities have the same type (note
that this guarantee does not imply any restriction on implementors). Thus the
types of `f1`, `f2`, `f3`, and `f4` in the following example are now guaranteed
to be the same:

```cpp
auto func = [](std::string) {};
std::string s("foo");
auto f1 = std::bind(func, s);
auto f2 = std::bind(std::as_const(func), std::as_const(s));
auto f3 = std::bind(func, std::string("bar"));
auto f4 = std::bind(std::move(func), std::move(s));
```

The wording uses the phrase "shall be constexpr functions" in a couple places.
We don't seem to have a way to say that in Library, see also [@LWG2833] and
[@LWG2289].

## Wording

<style>
code span.co { color: #898887; }
</style>

Add `constexpr` to several places in the synopsis in 20.14.1 [functional.syn]

::: bq

```diff
namespace std {
  // [func.invoke], invoke
  template<class F, class... Args>
-   invoke_result_t<F, Args...> invoke(F&& f, Args&&... args)
+   @[constexpr]{.diffins}@ invoke_result_t<F, Args...> invoke(F&& f, Args&&... args)
      noexcept(is_nothrow_invocable_v<F, Args...>);
	  
  // [refwrap], reference_wrapper
  template<class T> class reference_wrapper;

- template<class T> reference_wrapper<T> ref(T&) noexcept;
- template<class T> reference_wrapper<const T> cref(const T&) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<T> ref(T&) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<const T> cref(const T&) noexcept;
  template<class T> void ref(const T&&) = delete;
  template<class T> void cref(const T&&) = delete;

- template<class T> reference_wrapper<T> ref(reference_wrapper<T>) noexcept;
- template<class T> reference_wrapper<const T> cref(reference_wrapper<T>);
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<T> ref(reference_wrapper<T>) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<const T> cref(reference_wrapper<T>) noexcept;	  
  
  // [arithmetic.operations], arithmetic operations
  // ...
  
  // [comparisons], comparisons
  // ...

  // [logical.operations], logical operations
  // ...

  // [bitwise.operations], bitwise operations
  // ...

  // [func.identity], identity
  // ...

  // [func.not.fn], function template not_fn
- template<class F> @_unspecified_@ not_fn(F&& f);
+ template<class F> @[constexpr]{.diffins} _unspecified_@ not_fn(F&& f);

  // [func.bind.front], function template bind_front
- template<class F, class... Args> @_unspecified_@ bind_front(F&&, Args&&...);
+ template<class F, class... Args> @[constexpr]{.diffins} _unspecified_@ bind_front(F&&, Args&&...);
 
  // [func.bind], bind
  template<class T> struct is_bind_expression;
  template<class T> struct is_placeholder;

- template<class F, class... BoundArgs>
-   @_unspecified_@ bind(F&&, BoundArgs&&...);
- template<class R, class F, class... BoundArgs>
-   @_unspecified_@ bind(F&&, BoundArgs&&...);
+ template<class F, class... BoundArgs>
+   @[constexpr]{.diffins} _unspecified_@ bind(F&&, BoundArgs&&...);
+ template<class R, class F, class... BoundArgs>
+   @[constexpr]{.diffins} _unspecified_@ bind(F&&, BoundArgs&&...);

  namespace placeholders {
    // M is the implementation-defined number of placeholders
    @_see below_@ _1;
    @_see below_@ _2;
               .
               .
               .
    @_see below_@ _M;
  }

  // [func.memfn], member function adaptors
  template<class R, class T>
-   @_unspecified_@ mem_fn(R T::*) noexcept;
+   @[constexpr]{.diffins} _unspecified_@ mem_fn(R T::*) noexcept;

  // ...	
}
```

:::

The definition of the *simple call wrapper* (used only for `mem_fn`) is changed
to be a refinement of *perfect forwarding call wrapper*, instead of *argument
forwarding call wrapper*. These make the invocation operator conditionally
`constexpr` and `noexcept`. In addition we state explicitly the copy/move
constructor/assignment of simple call wrapper is core constant expression.
[ *Note*: The definition of simple call wrapper is still required to guarantee
assignability. ]

The requirement of copy/move operation to be defined in terms of state entities
is now extended to any argument forwarding call wrapper (as we define them for
`not_fn` and `bind`).

Apply following changes to 20.14.3 [func.require]:

> [3]{.pnum} Every call wrapper ([func.def]) [meets the]{.addu} [is]{.rm}
> *Cpp17MoveConstructible* [and *Cpp17Destructible* requirements]{.addu}.
> [A]{.rm} [An]{.addu} *argument forwarding call wrapper* is a call wrapper that
> can be called with an arbitrary argument list and delivers the arguments to
> the wrapped callable object as references. This forwarding step delivers
> rvalue arguments as rvalue references and lvalue arguments as lvalue
> references. [A *simple call wrapper* is an argument forwarding call wrapper
> that is *Cpp17CopyConstructible* and *Cpp17CopyAssignable* and whose copy
> constructor, move constructor, copy assignment operator, and move assignment
> operator do not throw exceptions.]{.rm} [ *Note*: In a typical implementation,
> argument forwarding call wrappers have an overloaded function call operator of
> the form
> ```diff
>  template<class... UnBoundArgs>
>-   R operator()(UnBoundArgs&&... unbound_args) @_cv-qual_@;
>+   @[constexpr]{.diffins}@ R operator()(UnBoundArgs&&... unbound_args) @_cv-qual_@;
> ```
> —*end note*]  
> 
> [4]{.pnum} A *perfect forwarding call wrapper* is an argument forwarding call
> wrapper that forwards its state entities to the underlying call expression.
> This forwarding step delivers a state entity of type `T` as *cv* `T&` when the
> call is performed on an lvalue of the call wrapper type and as *cv* `T&&`
> otherwise, where *cv* represents the cv-qualifiers of the call wrapper and
> where *cv* shall be neither `volatile` nor `const volatile`.  
> 
> [5]{.pnum} A *call pattern* defines the semantics of invoking a perfect
> forwarding call wrapper. A postfix call performed on a perfect forwarding call
> wrapper is expression-equivalent ([defns.expression-equivalent]) to an
> expression `e` determined from its call pattern cp by replacing all
> occurrences of the arguments of the call wrapper and its state entities with
> references as described in the corresponding forwarding steps.  
> 
> [a]{.pnum} [A *simple call wrapper* is a perfect forwarding call wrapper that
> meets the *Cpp17CopyConstructible* and *Cpp17CopyAssignable* and whose copy
> constructor, move constructor, and assignment operator are constexpr functions
> which do not throw exceptions.]{.addu}
> 
> [6]{.pnum} The copy/move constructor of [a perfect]{.rm} [an argument]{.addu}
> forwarding call wrapper has the same apparent semantics as if memberwise
> copy/move of its state entities were performed ([class.copy.ctor]). [ *Note*:
> This implies that each of the copy/move constructors has the same
> *exception-specification* as the corresponding implicit definition and is
> declared as `constexpr` if the corresponding implicit definition would be
> considered to be constexpr. —*end note* ]
> 
> [7]{.pnum} [Perfect]{.rm} [Argument]{.addu} forwarding call wrappers returned
> by a given standard library function template have the same type if the types
> of their corresponding state entities are the same.

Add `constexpr` to `std::invoke()` in 20.14.4 [func.invoke]

::: bq
```diff
  template<class F, class... Args>
-   invoke_result_t<F, Args...> invoke(F&& f, Args&&... args)
+   @[constexpr]{.diffins}@ invoke_result_t<F, Args...> invoke(F&& f, Args&&... args)
      noexcept(is_nothrow_invocable_v<F, Args...>);
```	
:::

Add `constexpr` to `std::reference_wrapper<T>` in 20.14.5 [refwrap]

::: bq
```diff
namespace std {
  template<class T> class reference_wrapper {
  public:
    // types
    using type = T;

    // construct/copy/destroy
    template<class U>
-     reference_wrapper(U&&) noexcept(@_see below_@);
-   reference_wrapper(const reference_wrapper& x) noexcept;
+     @[constexpr]{.diffins}@ reference_wrapper(U&&) noexcept(@_see below_@);
+   @[constexpr]{.diffins}@ reference_wrapper(const reference_wrapper& x) noexcept;

    // assignment
-   reference_wrapper& operator=(const reference_wrapper& x) noexcept;
+   @[constexpr]{.diffins}@ reference_wrapper& operator=(const reference_wrapper& x) noexcept;

    // access
-   operator T& () const noexcept;
-   T& get() const noexcept;
+   @[constexpr]{.diffins}@ operator T& () const noexcept;
+   @[constexpr]{.diffins}@ T& get() const noexcept;

    // invocation
    template<class... ArgTypes>
-     invoke_result_t<T&, ArgTypes...> operator()(ArgTypes&&...) const;
+     @[constexpr]{.diffins}@ invoke_result_t<T&, ArgTypes...> operator()(ArgTypes&&...) const;
  };
  template<class T>
    reference_wrapper(T&) -> reference_wrapper<T>;
}
```
:::
	
And its corresponding subsections, 20.14.5.1 [refwrap.const]

::: bq
```diff
  template<class U>
-   reference_wrapper(U&& u) noexcept(@_see below_@);
+   @[constexpr]{.diffins}@ reference_wrapper(U&& u) noexcept(@_see below_@);

  [...]

- reference_wrapper(const reference_wrapper& x) noexcept;
+ @[constexpr]{.diffins}@ reference_wrapper(const reference_wrapper& x) noexcept;
```
:::

20.14.5.2 [refwrap.assign]

::: bq
```diff
- reference_wrapper& operator=(const reference_wrapper& x) noexcept;
+ @[constexpr]{.diffins}@ reference_wrapper& operator=(const reference_wrapper& x) noexcept;
```
:::

20.14.5.3 [refwrap.access]

::: bq
```diff
- operator T& () const noexcept;
+ @[constexpr]{.diffins}@ operator T& () const noexcept;

  [...]

- T& get() const noexcept;
+ @[constexpr]{.diffins}@ T& get() const noexcept;
```
:::

20.14.5.4 [refwrap.invoke]

::: bq
```diff
  template<class... ArgTypes>
-   invoke_result_t<T&, ArgTypes...>
+   @[constexpr]{.diffins}@ invoke_result_t<T&, ArgTypes...>
      operator()(ArgTypes&&... args) const;
```
:::

and its helper functions, 20.14.5.5 [refwrap.helpers]

::: bq
```diff
- template<class T> reference_wrapper<T> ref(T& t) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<T> ref(T& t) noexcept;
```
[2]{.pnum} _Returns_: `reference_wrapper<T>(t)`.
```diff
- template<class T> reference_wrapper<T> ref(reference_wrapper<T> t) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<T> ref(reference_wrapper<T> t) noexcept;
```
[3]{.pnum} _Returns_: `ref(t.get())`.
```diff
- template<class T> reference_wrapper<const T> cref(const T& t) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<const T> cref(const T& t) noexcept;
```
[4]{.pnum} _Returns_: `reference_wrapper <const T>(t)`.
```diff
- template<class T> reference_wrapper<const T> cref(reference_wrapper<T> t) noexcept;
+ template<class T> @[constexpr]{.diffins}@ reference_wrapper<const T> cref(reference_wrapper<T> t) noexcept;
```
[5]{.pnum} _Returns_: `cref(t.get())`.
:::

Add `constexpr` to `std::not_fn()` in 20.14.12 [func.not.fn]:

::: bq
```diff
- template<class F> @_unspecified_@ not_fn(F&& f);
+ template<class F> @[constexpr]{.diffins} _unspecified_@ not_fn(F&& f);
```
:::

Add `constexpr` to `std::bind_front()` in 20.14.13 [func.bind.front]:

::: bq
```diff
  template<class F, class... Args>
-   @_unspecified_@ bind_front(F&& f, Args&&... args);
+   @[constexpr]{.diffins} _unspecified_@ bind_front(F&& f, Args&&... args);
```
:::

Apply the following changes to `std::bind()` in 20.14.14.3 [func.bind.bind], merging `bind` and `bind<R>`:

::: bq

[1]{.pnum} In the text that follows:

- [1.0]{.pnum}[`g` is a value of the result of a `bind` invocation,]{.addu}
- [1.1]{.pnum} `FD` is the type `decay_t<F>`,
- [1.2]{.pnum `fd` is [an lvalue of type `FD` constructed from
`std::forward<F>(f)`,]{.rm} [a target object of `g` ([func.def]) of type `FD`
direct-non-list-initialized with `std::forward<F>(f)`,]{.addu}
- [1.3]{.pnum} <code>T<sub>i</sub></code> is the `i`th type in the template
parameter pack `BoundArgs`,
- [1.4]{.pnum} <code>TD<sub>i</sub></code> is the type
<code>decay_t&lt;T<sub>i</sub>&gt;</code>,
- [1.5]{.pnum} <code>t<sub>i</sub></code> is the `i`th argument in the function
parameter pack `bound_args`,
- [1.6]{.pnum} <code>td<sub>i</sub></code> is [an lvalue of type
<code>TD<sub>i</sub></code> constructed from
<code>std::forward&lt;T<sub>i</sub>&gt;(t<sub>i</sub>)</code>,]{.rm} [a bound
argument entity of `g` ([func.def]) of type <code>TD<sub>i</sub></code>
direct-non-list-initialized with <code>std::forward&lt;T<sub>i</sub>&gt;(t<sub>i</sub>)</code>,]{.addu}
- [1.7]{.pnum} <code>U<sub>j</sub></code> is the `j`th deduced type of the
`UnBoundArgs&&...` parameter of the argument forwarding call wrapper, and
- [1.8]{.pnum} <code>u<sub>j</sub></code> is the `j`th argument associated with
<code>U<sub>j</sub></code>.

```diff
  template<class F, class... BoundArgs>
-   @_unspecified_@ bind(F&& f, BoundArgs&&... bound_args);
+   @[constexpr]{.diffins} _unspecified_@ bind(F&& f, BoundArgs&&... bound_args);
+ template<class R, class F, class... BoundArgs>
+   @[constexpr]{.diffins} _unspecified_@ bind(F&& f, BoundArgs&&... bound_args);
```

[2]{.pnum} [*Requires*]{.rm} [*Mandates*]{.addu}: `is_constructible_v<FD, F>`
[shall be]{.rm} [is]{.addu} `true`. For each <code>T<sub>i</sub></code> in
`BoundArgs`, <code>is_constructible_v&lt;TD<sub>i</sub>, T<sub>i</sub>&gt;</code>
[shall be]{.rm} [is]{.addu} `true`.

[2a]{.pnum} [*Expects*: `FD` and each <code>TD<sub>i</sub></code> meets the of
*Cpp17MoveConstructible* and *Cpp17Destructible* requirements.]{.addu}
<code>INVOKE(fd, w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub>)</code>
([func.require]) [shall be]{.rm} [is]{.addu} a valid expression for some values
<code>w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub></code>, where `N` has the
value `sizeof...(bound_args)`. [The cv-qualifiers *cv* of the call wrapper `g`,
as specified below, shall be neither `volatile` nor `const volatile`.]{.rm}

[3]{.pnum} *Returns*: An argument forwarding call wrapper `g` ([func.require]).
[A program that attempts to invoke a volatile-qualified `g` is ill-formed. When
`g` is not volatile-qualified,]{.addu} [The effect of]{.rm} [invocation]{.addu}
<code>g(u<sub>1</sub>, u<sub>2</sub>, …, u<sub>M</sub>)</code> [shall be]{.rm}
[is expression-equivalent ([defns.expression-equivalent]) to]{.addu}
[<code>INVOKE(fd, std::forward&lt;V<sub>1</sub>&gt;(v<sub>1</sub>),
std::forward&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …,
std::forward&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code>]{.rm}
[<code>INVOKE(static_cast&lt;V<sub>fd</sub>&gt;(v<sub>fd</sub>),
static_cast&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), static_cast&lt;V<sub>2</sub>&gt;(v<sub>2</sub>),
…, static_cast&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code> for the first
overload, and <code>INVOKE&lt;R&gt;(static_cast&lt;V<sub>fd</sub>&gt;(v<sub>fd</sub>),
static_cast&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), static_cast&lt;V<sub>2</sub>&gt;(v<sub>2</sub>),
…, static_cast&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code> for the second
overload,]{.addu} where the values and types of [the target argument <code>v<sub>fd</sub></code>
and of]{.addu} the bound arguments <code>v<sub>1</sub>, v<sub>2</sub>, …, v<sub>N</sub></code>
are determined as specified below. [The copy constructor and move constructor of
the argument forwarding call wrapper shall throw an exception if and only if the
corresponding constructor of `FD` or of any of the types <code>TD<sub>i</sub></code>
throws an exception.]{.rm}

[4]{.pnum} *Throws*: [Nothing unless the construction of `fd` or of one of the
values <code>td<sub>i</sub></code> throws an exception.]{.rm} [Any exception
thrown by the initialization of the state entities of `g`.]{.addu}

[5]{.pnum} [*Remarks*: The return type shall satisfy the *Cpp17MoveConstructible*
requirements. If all of `FD` and <code>TD<sub>i</sub></code> satisfy the
*Cpp17CopyConstructible* requirements, then the return type shall satisfy the
*Cpp17CopyConstructible* requirements. [*Note*: This implies that all of
<code>FD</code> and <code>TD<sub>i</sub></code> are *Cpp17MoveConstructible*.
—*end note*]]{.rm}

[5a]{.pnum} [[*Note*: If all of `FD` and <code>TD<sub>i</sub></code> meet the
requirements of *Cpp17CopyConstructible*, then the return type meets the
requirements of *Cpp17CopyConstructible*. -*end note*]]{.addu}

::: rm
```
template<class R, class F, class... BoundArgs>
  @_unspecified_@ bind(F&& f, BoundArgs&&... bound_args);
```

[6]{.pnum} *Requires*: `is_constructible_v<FD, F>` shall be `true`. For each
<code>T<sub>i</sub></code> in `BoundArgs`, <code>is_constructible_v&lt;TD<sub>i</sub>,
T<sub>i</sub>&gt;</code> shall be true. <code>INVOKE(fd, w<sub>1</sub>,
w<sub>2</sub>, …, w<sub>N</sub>)</code> ([func.require]) shall be a valid
expression for some values <code>w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub></code>,
where `N` has the value `sizeof...(bound_args)`. The cv-qualifiers *cv* of the
call wrapper `g`, as specified below, shall be neither `volatile` nor
`const volatile`.

[7]{.pnum} *Returns*: An argument forwarding call wrapper g ([func.require]).
The effect of <code>g(u<sub>1</sub>, u<sub>2</sub>, …, u<sub>M</sub>)</code>
shall be <code>INVOKE&lt;R&gt;(fd, std::forward&lt;V<sub>1</sub>&gt;(v<sub>1</sub>),
std::forward&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …, std::forward&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code>
where the values and types of the bound arguments
<code>v<sub>1</sub>, v<sub>2</sub>, …, v<sub>N</sub></code> are determined as
specified below. The copy constructor and move constructor of the argument
forwarding call wrapper shall throw an exception if and only if the corresponding
constructor of `FD` or of any of the types <code>TD<sub>i</sub></code> throws an
exception.

[8]{.pnum} *Throws*: Nothing unless the construction of `fd` or of one of the
values <code>td<sub>i</sub></code> throws an exception.

[9]{.pnum} *Remarks*: The return type shall satisfy the *Cpp17MoveConstructible*
requirements. If all of `FD` and <code>TD<sub>i</sub></code> satisfy the
*Cpp17CopyConstructible* requirements, then the return type shall satisfy the
*Cpp17CopyConstructible* requirements. [*Note*: This implies that all of
<code>FD</code> and <code>TD<sub>i</sub></code> are *Cpp17MoveConstructible*.
—*end note*]
:::

:::

Define <code>v<sub>fd</sub></code> and add reference to the *cv*-qualifies in
20.14.14.3 [func.bind.bind]/10: 

::: bq

[10]{.pnum} The values of the *bound arguments* <code>v<sub>1</sub></code>,
<code>v<sub>2</sub></code>, ..., <code>v<sub>N</sub></code> and their
corresponding types <code>V<sub>1</sub></code>, <code>V<sub>2</sub></code>, ...,
<code>V<sub>N</sub></code> depend on the types <code>TD<sub><i>i</i></sub></code>
derived from the call to bind and the cv-qualifiers *cv* of the call wrapper `g`
as follows:

- [10.1]{.pnum} if <code>TD<sub><i>i</i></sub></code> is `reference_wrapper<T>`, [...]
- [10.2]{.pnum} if the value of <code>is_bind_expression_v&lt;TD<sub><i>i</i></sub>&gt;</code>
is `true`, the argument is <code>[td<sub><i>i</i></sub>]{.rm}
[static_cast&lt;TD<sub><i>i</i></sub> <i>cv</i> &&gt;]{.addu}(std::forward&lt;U<sub>j</sub>&gt;(u<sub>j</sub>)...)</code>
and its type <code>V<sub><i>i</i></sub></code> is
<code>invoke_result_t&lt;TD<sub><i>i</i></sub> <i>cv</i> &, U<sub><i>j</i></sub>...&gt;&&</code>;
- [10.3]{.pnum} if the value `j` of [...] 
- [10.4]{.pnum} otherwise, [...]

[11]{.pnum} [The value of the <i>target argument</i> <code>v<sub>fd</sub></code>
is <code>fd</code> and its corresponding type <code>V<sub>fd</sub></code> is
<code>FD <i>cv</i> &</code>.]{.addu}
:::

Add constant requirement to the placeholders in 20.14.14.4 [func.bind.place]/1:

> [1]{.pnum} All placeholder types [meet the]{.addu} [shall be]{.rm}
*Cpp17DefaultConstructible* and *Cpp17CopyConstructible*[ requirements]{.addu},
and their default constructors and copy/move constructors [are constexpr
functions which do]{.addu} [shall]{.rm} not throw exceptions. It is
implementation-defined whether placeholder types [meet the]{.addu} [are]{.rm}
*Cpp17CopyAssignable* [requirements, but if so, their]{.addu}
[. *Cpp17CopyAssignable* placeholders']{.rm} copy assignment operators [are
constexpr functions which do]{.addu} [shall]{.rm} not throw exceptions.
  
Add `constexpr` to `std::mem_fn()` in 20.14.15 [func.memfn]

::: bq
```diff
- template<class R, class T> @_unspecified_@ mem_fn(R T::* pm) noexcept;
+ template<class R, class T> @[constexpr]{.diffins} _unspecified_@ mem_fn(R T::* pm) noexcept;
```
:::

> [1]{.pnum} *Returns*: A simple call wrapper `fn` [such that the expression
<code>fn(t, a<sub>2</sub>, …, a<sub>N</sub>)</code> is equivalent to
<code>INVOKE(pm, t, a<sub>2</sub>, …, a<sub>N</sub>)</code> ([func.require]).]{.rm}
[with call pattern `invoke(pmd, call_args...)`, where `pmd` is the target object
of `fn` of type `R T::*` direct-non-list-initialized with `pm`, and `call_args`
is an argument pack used in a function call expression ([expr.call]) of `pm`.]{.addu}

# Acknowledgements

Thanks to Casey Carter and Agustín Bergé for going over the history of issues
surrounding `constexpr invoke` and suggesting that this proposal be written.
Thanks to Daniel Krügler, Tim Song and,  Casey Carter for help on the wording.

---
references:
  - id: llvm23141
	citation-label: llvm23141
	title: "`std::bind` const-qualifying bound arguments captured by value when compiled as C++14"
	author:
	  - family: Eric Niebler
	issued:
	  - year: 2015
	URL: https://bugs.llvm.org/show_bug.cgi?id=23141
  - id: llvm23135
	citation-label: llvm23135
	title: "[C++11/14] Body of constexpr function templates instantiated too eagerly in unevaluated operands"
	author:
	  - family: Gonzalo BG
	issued:
	  - year: 2015
	URL: https://bugs.llvm.org/show_bug.cgi?id=23135
---

