---
title: Structured Bindings can introduce a Pack
document: P1061R10
date: today
audience: CWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Jonathan Wakely
      email: <cxx@kayari.org>
toc: true
---

# Revision History

R10 removes the ability to use packs outside of templates.

R9 has minor wording changes and updates the [implementation experience](#implementation-experience) section.

R8 re-adds the [namespace-scope exclusion](#namespace-scope-packs), and more wording updates. Also rebases the wording to account for [@P0609R3].

R7 attempts to word the post-Varna version.

R6 has added wording changes and adds some more complicated examples to motivate how to actually word this paper.

R5 has minor wording changes.

R4 significantly improves the wording after review in Issaquah.

R3 removes the exclusion of namespace-scope per [EWG guidance](https://github.com/cplusplus/papers/issues/294#issuecomment-1234578812).

R2 adds a section about implementation complexity, implementation experience, and wording.

R1 of this paper [@P1061R1] was presented to EWG in Belfast 2019 [@P1061R1.Minutes]
which approved the direction as presented (12-5-2-0-1).

R0 of this paper [@P1061R0] was presented to EWGI in Kona 2019 [@P1061R0.Minutes], who
reviewed it favorably and thought this was a good investment of our time
(4-3-4-1-0). The consensus in the room was that the restriction that the
introduced pack need not be the trailing identifier.

# Motivation

Function parameter packs and tuples are conceptually very similar. Both are heterogeneous sequences of objects. Some problems are easier to solve with a parameter pack, some are easier to solve with a `tuple`. Today, it's trivial to convert a pack to a `tuple`, but it's somewhat more involved to convert a `tuple` to a pack. You have to go through `std::apply()` [@N3915]:

::: std
```c++
std::tuple<A, B, C> tup = ...;
std::apply([&](auto&&... elems){
    // now I have a pack
}, tup);
```
:::

This is great for cases where we just need to call a [non-overloaded] function or function object, but rapidly becomes much more awkward as we dial up the complexity. Not to mention if I want to return from the outer scope based on what these elements have to be.

How do we compute the dot product of two `tuple`s? It's a choose your own adventure of awkward choices:

<table style="width:100%">
<tr>
<th style="width:50%">
Nested `apply()`
</th>
<th style="width:50%">
Using `index_sequence`
</th>
</tr>
<tr>
<td>
```cpp
template <class P, class Q>
auto dot_product(P p, Q q) {
    return std::apply([&](auto... p_elems){
        return std::apply([&](auto... q_elems){
            return (... + (p_elems * q_elems));
        }, q)
    }, p);
}
```
</td>
<td>
```cpp
template <size_t... Is, class P, class Q>
auto dot_product(std::index_sequence<Is...>, P p, Q, q) {
    return (... + (std::get<Is>(p) * std::get<Is>(q)));
}

template <class P, class Q>
auto dot_product(P p, Q q) {
    return dot_product(
        std::make_index_sequence<std::tuple_size<P>::value>{},
        p, q);
}
```
</td>
</tr>
</table>

Regardless of which option you dislike the least, both are limited to only `std::tuple`s. We don't have the ability to do this at all for any of the other kinds of types that can be used in a structured binding declaration [@P0144R2] - because we need to explicit list the correct number of identifiers, and we might not know how many there are.

# Proposal

We propose to extend the structured bindings syntax to allow the user to introduce a pack as (at most) one of the identifiers:

::: std
```c++
std::tuple<X, Y, Z> f();

auto [x,y,z] = f();          // OK today
auto [...xs] = f();          // proposed: xs is a pack of length three containing an X, Y, and a Z
auto [x, ...rest] = f();     // proposed: x is an X, rest is a pack of length two (Y and Z)
auto [x,y,z, ...rest] = f(); // proposed: rest is an empty pack
auto [x, ...rest, z] = f();  // proposed: x is an X, rest is a pack of length one
                             //   consisting of the Y, z is a Z
auto [...a, ...b] = f();     // ill-formed: multiple packs
```
:::


If we additionally add the structured binding customization machinery to `std::integer_sequence`, this could greatly simplify generic code:

<table style="width:100%">
<tr><th>Today</th><th>Proposed</th></tr>
<tr>
<td colspan="2">
<center>Implementing `std::apply()`</center>
</td>
</tr>
<tr>
<td>
```cpp
namespace detail {
    template <class F, class Tuple, std::size_t... I>
    constexpr decltype(auto) apply_impl(F &&f, Tuple &&t,
        std::index_sequence<I...>)
    {
        return std::invoke(std::forward<F>(f),
            std::get<I>(std::forward<Tuple>(t))...);
    }
}

template <class F, class Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
    return detail::apply_impl(
        std::forward<F>(f), std::forward<Tuple>(t),
        std::make_index_sequence<std::tuple_size_v<
            std::decay_t<Tuple>>>{});
}
```
</td>
<td>
```cpp
template <class F, class Tuple>
constexpr decltype(auto) apply(F &&f, Tuple &&t)
{
    auto&& [...elems] = t;
    return std::invoke(std::forward<F>(f),
        forward_like<Tuple, decltype(elems)>(elems)...);
}
```
</td>
</tr>
<tr>
<td colspan="2">
<center>`dot_product()`, nested</center>
</td>
</tr>
<tr>
<td>
```cpp
template <class P, class Q>
auto dot_product(P p, Q q) {
    return std::apply([&](auto... p_elems){
        return std::apply([&](auto... q_elems){
            return (... + (p_elems * q_elems));
        }, q)
    }, p);
}
```
</td>
<td>
```c++
template <class P, class Q>
auto dot_product(P p, Q q) {
    // no indirection!
    auto&& [...p_elems] = p;
    auto&& [...q_elems] = q;
    return (... + (p_elems * q_elems));
}
```
</td></tr>
<tr>
<td colspan="2">
<center>`dot_product()`, with `index_sequence`</center>
</td>
</tr>
<tr>
<td>
```cpp
template <size_t... Is, class P, class Q>
auto dot_product(std::index_sequence<Is...>, P p, Q, q) {
    return (... + (std::get<Is>(p) * std::get<Is>(q)));
}

template <class P, class Q>
auto dot_product(P p, Q q) {
    return dot_product(
        std::make_index_sequence<std::tuple_size_v<P>>{},
        p, q);
}
```
</td>
<td>
```cpp
template <class P, class Q>
auto dot_product(P p, Q q) {
    // no helper function necessary!
    auto [...Is] = std::make_index_sequence<
        std::tuple_size_v<P>>{};
    return (... + (std::get<Is>(p) * std::get<Is>(q)));
}
```
</td></tr>
</table>

Not only are these implementations more concise, but they are also more functional. I can just as easily use `apply()` with user-defined types as I can with `std::tuple`:

::: std
```cpp
struct Point {
    int x, y, z;
};

Point getPoint();
double calc(int, int, int);

double result = std::apply(calc, getPoint()); // ill-formed today, ok with proposed implementation
```
:::

## Other Languages

Python 2 had always allowed for a syntax similar to C++17 structured bindings,
where you have to provide all the identifiers:

::: std
```python
>>> a, b, c, d, e = range(5) # ok
>>> a, *b = range(3)
  File "<stdin>", line 1
    a, *b = range(3)
       ^
SyntaxError: invalid syntax
```
:::

But you could not do any more than that. Python 3 went one step further by way
 of PEP-3132 [@PEP.3132]. That proposal allowed for a single starred
identifier to be used, which would bind to all the elements as necessary:

::: std
```python
>>> a, *b, c = range(5)
>>> a
0
>>> c
4
>>> b
[1, 2, 3]
```
:::

The Python 3 behavior is synonymous with what is being proposed here. Notably,
from that PEP:

> Possible changes discussed were:
>
> - Only allow a starred expression as the last item in the exprlist. This
> would simplify the unpacking code a bit and allow for the starred expression
> to be assigned an iterator. This behavior was rejected because it would be too
>  surprising.

R0 of this proposal only allowed a pack to be introduced as the last item, which
was changed in R1.

## Implementation Burden

Unfortunately, this proposal has some implementation complexity. The
issue is not so much this aspect:

::: std
```cpp
template <typeanme Tuple>
auto sum_template(Tuple tuple) {
    auto [...elems] = tuple;
    return (... + elems);
}
```
:::

This part is more or less straightforward - we have a dependent type and we
introduce a pack from it, but we're already in a template context where dealing
with packs is just a normal thing.

The problem is this aspect:

::: std
```cpp
auto sum_non_template(SomeConreteType tuple) {
    auto [...elems] = tuple;
    return (... + elems);
}
```
:::

We have not yet in the history of C++ had this notion of packs outside of
dependent contexts. This is completely novel, and imposes a burden on
implementations to have to track packs outside of templates where they
previously had not.

However, in our estimation, this functionality is going to come to C++ in one
form or other fairly soon. Reflection, in the latest form of [@P1240R2], has
many examples of introducing packs in non-template contexts as well - through
the notion of a _reflection range_. That paper introduces several reifiers that
can manipilate a newly-introduced pack, such as:

::: std
```cpp
std::meta::info t_args[] = { ^int, ^42 };
template<typename T, T> struct X {};
X<...[:t_args:]...> x; // Same as "X<int, 42> x;".
template<typename, typename> struct Y {};
Y<...[:t_args:]...> y; // Error: same as "Y<int, 42> y;".
```
:::

As with the structured bindings example in this paper - we have a non-dependent
object outside of a template that we're using to introduce a pack.

Furthermore, unlike some of the reflection examples, and some of the more
generic pack facilities proposed in [@P1858R2], this paper offers a nice
benefit: all packs must still be declared before use. Even in the
`sum_non_template` example which, as the name suggests, is not a template in any
way, the pack `elems` needs an initial declaration. So any machinery that
implementations need to track packs doesn't need to be enabled everywhere - only
when a pack declaration has been seen.

## Removing Packs outside of Templates

The [@P1061R9] design relied upon introducing an _implicit template region_ when a structured binding pack was declared, which implicitly turns the rest of your function into a function template. That complexity, coupled with persistent opposition due to implementation complexity, led to Evolution rejecting [@P1061R9] at the Wrocław meeting.

Since R10, this paper removes support for packs outside of templates, which removes the implementor objection and the design complexity. All of the complex examples from the original approach have been removed from this paper for brevity.

## Implementation Experience

Jason Rice has implemented the [@P1061R9] design in a
[clang](https://github.com/ricejasonf/llvm-project/commits/ricejasonf/p1061). As
far as we've been able to ascertain, it works great. It was initially done early in the process, before the concept of "implicit template region" was introduced in the wording — when he was updating the implementation to account for the new rules and to make sure that all the examples in the paper compiled, he noted that "Honestly, the implicit template region vastly simplified things, and much code was deleted."

It is also [available on Compiler Explorer](https://godbolt.org/z/Tnz4e1dY9), including [the most complex example](https://godbolt.org/z/6ebbqb1Kh) in the the original paper.

# Wording

Add a drive-by fix to [expr.prim.fold]{.sref} after paragraph 3:

::: {.std .ins}
[π]{.pnum} [A fold expression is a pack expansion.]{.addu}
:::

Add a new grammar option for *simple-declaration* to [dcl.pre]{.sref} (note that this accounts for [@P0609R3] by renaming the grammar productions prefixed with `$attributed$` to `$sb$`):

::: std
```diff
- $attributed-identifier$:
-     $identifier$ $attribute-specifier-seq$@~opt~@
+ $sb-identifier$:
+     @[`...`~opt~]{.diffins}@ $identifier$ $attribute-specifier-seq$@~opt~@
+
- $attributed-identifier-list$:
-     $attributed-identifier$
-     $attributed-identifier-list$, $attributed-identifier$
+ $sb-identifier-list$:
+     $sb-identifier$
+     $sb-identifier-list$, $sb-identifier$

  $structured-binding-declaration$:
-    $attribute-specifier-seq$@~opt~@ $decl-specifier-seq$ $ref-qualifier$@~opt~@ [ @[*attributed-identifier-list*]{.diffdel}@ ]
+    $attribute-specifier-seq$@~opt~@ $decl-specifier-seq$ $ref-qualifier$@~opt~@ [ @[*sb-identifier-list*]{.diffins}@ ]
```
:::

Change [dcl.pre]{.sref}/6:

::: std
[6]{.pnum} A *simple-declaration* with a `$structured-binding-declaration$` is called a structured binding declaration ([dcl.struct.bind]). Each *decl-specifier* in the *decl-specifier-seq* shall be `static`, `thread_local`, `auto` ([dcl.spec.auto]), or a *cv*-qualifier. [The declaration shall contain at most one *sb-identifier* whose *identifier* is preceded by an ellipsis. If the declaration contains any such *sb-identifier*, it shall declare a templated entity ([temp.pre]).]{.addu}
:::

Change [dcl.struct.bind]{.sref} paragraph 1:

::: std
[1]{.pnum} A structured binding declaration introduces the <i>identifier</i>s v<sub>0</sub>, v<sub>1</sub>, v<sub>2</sub>, ...[, v<sub>N-1</sub>]{.addu} of the [<i>attribute-identifier-list-list</i>]{.rm} [`$sb-identifier-list$`]{.addu} as names ([basic.scope.declarative]) [of *structured bindings*]{.rm}. [An `$sb-identifier$` that contains an ellipsis introduces a structured binding pack ([temp.variadic]). A *structured binding* is either an `$sb-identifier$` that does not contain an ellipsis or an element of a structured binding pack.]{.addu} The optional `$attribute-specifier-seq$` of an [`$attributed-identifier$`]{.rm} [`$sb-identifier$`]{.addu} appertains to the [associated]{.addu} structured binding[s]{.addu} [so introduced]{.rm}. Let <i>cv</i> denote the <i>cv-qualifiers</i
> in the `$decl-specifier-seq$`.
:::

Introduce new paragraphs after [dcl.struct.bind]{.sref} paragraph 1, introducing
the terms "structured binding size" and SB~_i_~:

::: {.std .ins}
[1+1]{.pnum} The _structured binding size_ of `E`, as defined below, is the
number of structured bindings that need to be introduced by the structured binding
declaration. If there is no structured binding pack, then
the number of elements in the _sb-identifier-list_ shall be equal to the
structured binding size of `E`.
Otherwise, the number of non-pack elements shall be no more than the structured binding size of `E`; the number of elements of the structured
binding pack is the structured binding size of `E` less the number of non-pack elements in the
 `$sb-identifier-list$`.

[1+2]{.pnum} Let SB~_i_~ denote the _i_^th^ structured binding in the structured binding declaration after
expanding the structured binding pack, if any. [ _Note_: If there is no
structured binding pack, then SB~_i_~ denotes v~_i_~. - _end note_ ]

::: example
```
struct C { int x, y, z; };

template <class T>
void now_i_know_my() {
  auto [a, b, c] = C(); // OK, SB@~0~@ is a, SB@~1~@ is b, and SB@~2~@ is c
  auto [d, ...e] = C(); // OK, SB@~0~@ is d, the pack e (v@~1~@) contains two structured bindings: SB@~1~@ and SB@~2~@
  auto [...f, g] = C(); // OK, the pack f (v@~0~@) contains two structured bindings: SB@~0~@ and SB@~1~@, and SB@~2~@ is g
  auto [h, i, j, ...k] = C(); // OK, the pack k is empty
  auto [l, m, n, o, ...p] = C(); // error: structured binding size is too small
}
```
:::
:::

Change [dcl.struct.bind]{.sref} paragraph 3 to define a structured binding size and
extend the example:

::: std
[3]{.pnum} If `E` is an array type with element type <code>T</code>, [the number of elements in the <i>attributed-identifier-list</i> shall be]{.rm} [the structured binding size of `E` is]{.addu} equal to the number of elements of `E`. Each [<i>v<sub>i</sub></i>]{.rm} [SB~_i_~]{.addu} is the name of an lvalue that refers to the element <i>i</i> of the array and whose type is <code>T</code>; the referenced type is <code>T</code>.
[_Note_: The top-level _cv_-qualifiers of `T` are _cv_. — _end note_]

::: example
```diff
  auto f() -> int(&)[2];
  auto [ x, y ] = f();            // x and y refer to elements in a copy of the array return value
  auto& [ xr, yr ] = f();         // xr and yr refer to elements in the array referred to by f's return value

+ auto g() -> int(&)[4];

+ template <size_t N>
+ void h(int (&arr)[N]) {
+   auto [a, ...b, c] = arr;   // a names the first element of the array, b is a pack referring to the second and
+                              // third elements, and c names the fourth element
+   auto& [...e] = arr;        // e is a pack referring to the four elements of the array
+ }
+
+ void call_h() {
+   h(g());
+ }
```
:::
:::

Change [dcl.struct.bind]{.sref} paragraph 4 to define a structured binding size:

::: std
[4]{.pnum} Otherwise, if the <i>qualified-id</i> <code>std::tuple_size&lt;E></code> names a complete type, the expression <code class="language-cpp">std::tuple_size&lt;E>::value</code> shall be a well-formed integral constant expression and the [number of elements in the <i>attributed-identifier-list</i> shall be]{.rm} [structured binding size of `E` is]{.addu} equal to the value of that expression. [...] Each [<i>v<sub>i</sub></i>]{.rm} [SB~_i_~]{.addu} is the name of an lvalue of type <code class="">T<sub>i</sub></code> that refers to the object bound to <code class="">r<sub>i</sub></code>; the referenced type is <code class="">T<sub>i</sub></code>.
:::

Change [dcl.struct.bind]{.sref} paragraph 5 to define a structured binding size:

::: std
[5]{.pnum} Otherwise, all of `E`'s non-static data members shall be direct members of `E` or of the same base class of `E`, well-formed when named as <code>e.name</code> in the context of the structured binding, `E` shall not have an anonymous union member, and the [number of elements in the <i>attributed-identifier-list</i> shall be]{.rm} [structured binding size of `E` is]{.addu} equal to the number of non-static data members of `E`. Designating the non-static data members of `E` as <code class="">m<sub>0</sub>, m<sub>1</sub>, m<sub>2</sub>, . . .</code> (in declaration order), each [<code class="">v<sub>i</i></code>]{.rm} [SB~_i_~]{.addu} is the name of an lvalue that refers to the member <code class="">m<sub>i</sub></code> of `E` and whose type is <i>cv</i> <code class="">T<sub>i</sub></code>, where <code class="">T<sub>i</sub></code> is the declared type of that member; the referenced type is <i>cv</i> <code class="">T<sub>i</sub></code>. The lvalue is a bit-field if that member is a bit-field.
:::


Add a new clause to [temp.variadic]{.sref}, after paragraph 3:

::: {.std .ins}
[3+]{.pnum} A *structured binding pack* is an *sb-identifier* that introduces zero or more structured bindings ([dcl.struct.bind]).

::: example
```
auto foo() -> int(&)[2];

template <class T>
void g() {
  auto [...a] = foo();          // a is a structured binding pack containing 2 elements
  auto [b, c, ...d] = foo();    // d is a structured binding pack containing 0 elements
}
```
:::

:::

In [temp.variadic]{.sref}, change paragraph 4:

::: {.std}
[4]{.pnum} A *pack* is a template parameter pack, a function parameter pack, [or]{.rm} an *init-capture* pack[, or a structured binding pack]{.addu}. The number of elements of a template parameter pack or a function parameter pack is the number of arguments provided for the parameter pack. The number of elements of an *init-capture* pack is the number of elements in the pack expansion of its *initializer*.
:::

In [temp.variadic]{.sref}, paragraph 5 (describing pack expansions) remains unchanged.

In [temp.variadic]{.sref}, add a bullet to paragraph 8:

::: std
[8]{.pnum} Such an element, in the context of the instantiation, is interpreted as follows:

* [8.1]{.pnum} if the pack is a template parameter pack, the element is a template parameter ([temp.param]) of the corresponding kind (type or non-type) designating the
<i>i</i><sup>th</sup> corresponding type or value template argument;
* [8.2]{.pnum} if the pack is a function parameter pack, the element is an <i>id-expression</i> designating the  <i>i</i><sup>th</sup> function parameter that resulted from instantiation of the function parameter pack declaration; [otherwise]{.rm}
* [8.3]{.pnum} if the pack is an <i>init-capture</i> pack, the element is an <i>id-expression</i> designating the variable introduced by the <i>i</i><sup>th</sup>th <i>init-capture</i> that resulted from instantiation of the <i>init-capture</i> pack[.]{.rm} [; otherwise]{.addu}
* [8.4]{.pnum} [ if the pack is a structured binding pack, the element is an <i>id-expression</i> designating the <i>i</i><sup>th</sup> structured binding in the pack that resulted from the structured binding declaration.]{.addu}
:::

Add a bullet to [temp.dep.expr]{.sref}/3:

::: std
[3]{.pnum} An _id-expression_ is type-dependent if it is a _template-id_ that is not a concept-id and is dependent; or if its terminal name is

* [3.1]{.pnum} associated by name lookup with one or more declarations declared with a dependent type,
* [3.2]{.pnum} associated by name lookup with a non-type template-parameter declared with a type that contains a placeholder type,
* [3.3]{.pnum} associated by name lookup with a variable declared with a type that contains a placeholder type ([dcl.spec.auto]) where the initializer is type-dependent,
* [3.4]{.pnum} associated by name lookup with one or more declarations of member functions of a class that is the current instantiation declared with a return type that contains a placeholder type,
* [3.5]{.pnum} associated by name lookup with a structured binding declaration ([dcl.struct.bind]) whose brace-or-equal-initializer is type-dependent,
* [3.5b]{.pnum} [associated by name lookup with a pack, unless that pack is a non-type template parameter pack whose types are non-dependent,]{.addu}

  ::: addu
  ::: example
  ```cpp
  struct C { };

  void g(...); // #1

  template <typename T>
  void f() {
      C arr[1];
      auto [...e] = arr;
      g(e...); // calls #2
  }

  void g(C); // #2

  int main() {
      f<int>();
  }
  ```
  :::
  :::

* [3.6]{.pnum} associated by name lookup with an entity captured by copy ([expr.prim.lambda.capture]) in a lambda-expression that has an explicit object parameter whose type is dependent ([dcl.fct]),
* [3.7]{.pnum} the identifier `__func__` ([dcl.fct.def.general]), where any enclosing function is a template, a member of a class template, or a generic lambda,
* [3.8]{.pnum} a conversion-function-id that specifies a dependent type, or
* [3.9]{.pnum} dependent
:::

Add a carve-out for in [temp.dep.constexpr]{.sref}/4:

::: std
[4]{.pnum} Expressions of the following form are value-dependent:
```
sizeof ... ( identifier )
fold-expression
```
[unless the *identifier* is a structured binding pack whose initializer is not dependent.]{.addu}
:::

## Feature-Test Macro

Bump `__cpp_structured_bindings` in [cpp.predefined]{.sref}:

::: std
```diff
- __cpp_­structured_­bindings 201606L
+ __cpp_­structured_­bindings 2024XXL
```
:::

# Acknowledgements

Thanks to Michael Park and Tomasz Kamiński for their helpful feedback. Thanks to
Richard Smith for help with the wording. Thanks especially to Jason Rice for the
implementation.

Thanks to John Spicer, Christof Meerwald, Jens Maurer, and everyone else in Core for the wording help, mind-melting examples, and getting this paper in shape.

---
references:
  - id: P1061R0.Minutes
    citation-label: P1061R0.Minutes
    title: "Kona 2019 EWGI: P1061R0"
    author:
      - family: EWGI
    issued:
      - year: 2019
    URL: http://wiki.edg.com/bin/view/Wg21kona2019/P1061
  - id: P1061R1.Minutes
    citation-label: P1061R1.Minutes
    title: "Belfast 2020 EWG: P1061R1"
    author:
      - family: EWG
    issued:
      - year: 2019
    URL: https://wiki.edg.com/bin/view/Wg21belfast/P1061-EWG
  - id: PEP.3132
    citation-label: PEP.3132
    title: "PEP 3132 -- Extended Iterable Unpacking"
    author:
      - family: Georg Brandl
    issued:
      - year: 2007
    URL: https://www.python.org/dev/peps/pep-3132/
  - id: P0609R3
    citation-label: P0609R3
    title: "Attributes for Structured Bindings"
    author:
      - family: Aaron Ballman
    issued:
      - year: 2024
        month: 03
        day: 21
    URL: https://wg21.link/p0609r3
---
