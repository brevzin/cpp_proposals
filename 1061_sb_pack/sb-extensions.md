Title: Structured Bindings can introduce a Pack
Document-Number: D1061R1
Date: 2018-05-01
Authors: Barry Revzin, barry dot revzin at gmail dot com
Authors: Jonathan Wakely, jonathan dot wakely at gmail dot com
Audience: EWG

# Motivation

Function parameter packs and tuples are conceptually very similar. Both are heterogeneous sequences of objects. Some problems are easier to solve with a parameter pack, some are easier to solve with a `tuple`. Today, it's trivial to convert a pack to a `tuple`, but it's somewhat more involved to convert a `tuple` to a pack. You have to go through [`std::apply()`](https://wg21.link/n3915):

    :::c++
    std::tuple<A, B, C> tup = ...;
    std::apply([&](auto&&... elems){
        // now I have a pack
    }, tup);

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
    :::c++
    template <class P, class Q>
    auto dot_product(P p, Q q) {
        return std::apply([&](auto... p_elems){
            return std::apply([&](auto... q_elems){
                return (... + (p_elems * q_elems));
            }, q)
        }, p);
    }

</td>
<td>
    :::c++
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
</td>
</tr>
</table>

Regardless of which option you dislike the least, both are limited to only `std::tuple`s. We don't have the ability to do this at all for any of the other kinds of types that can be used in a [structured binding declaration](https://wg21.link/p0144) - because we need to explicit list the correct number of identifiers, and we might not know how many there are.

# Proposal

We propose to extend the structured bindings syntax to allow the user to introduce a pack as the last identifier, following the usual rules of pack declarations (must be trailing, and packs are introduced with leading `...`):

    :::c++
    std::tuple<X, Y, Z> f();
    auto [x,y,z] = f();          // OK today
    auto [...xs] = f();          // proposed: xs is a pack of length three containing an X, Y, and a Z
    auto [x, ...rest] = f();     // proposed: x is an X, rest... is a pack of length two
    auto [x,y,z, ...rest] = f(); // proposed: rest... is an empty pack
    auto [x, ...rest, z] = f();  // ill-formed: non-trailing pack. This is for consistency with, for instance, function parameter packs


If we additionally add the structured binding customization machinery to `std::integer_sequence`, this could greatly simplify generic code:

<table style="width:100%">
<tr><th></th><th>Today</th><th>Proposed</th></tr>
<tr>
<td>
Implementing<br/>`std::apply()`
</td>
<td>
    :::c++
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
</td>
<td>
    :::c++
    template <class F, class Tuple>
    constexpr decltype(auto) apply(F &&f, Tuple &&t)
    {
        auto&& [...elems] = t;
        return std::invoke(std::forward<F>(f),
            forward_like<Tuple, decltype(elems)>(elems)...);
    }
</td>
</tr>
<tr>
<td>
`dot_product()`,<br/>nested
</td>
<td>

    :::c++
    template <class P, class Q>
    auto dot_product(P p, Q q) {
        return std::apply([&](auto... p_elems){
            return std::apply([&](auto... q_elems){
                return (... + (p_elems * q_elems));
            }, q)
        }, p);
    }

</td>
<td>

    :::c++
    template <class P, class Q>
    auto dot_product(P p, Q q) {
        // no indirection!
        auto&& [...p_elems] = p;
        auto&& [...q_elems] = q;
        return (... + (p_elems * q_elems));
    }

</td></tr>
<tr>
<td>
`dot_product()`,</br>`index_sequence`
</td>
<td>

    :::c++
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

</td>
<td>

    :::c++
    template <class P, class Q>
    auto dot_product(P p, Q q) {
        // no helper function necessary!
        auto [...Is] = std::make_index_sequence<
            std::tuple_size_v<P>>{};
        return (... + (std::get<Is>(p) * std::get<Is>(q)));
    }
</td></tr>
</table>

Not only are these implementations more concise, but they are also more functional. I can just as easily use `apply()` with user-defined types as I can with `std::tuple`:

    :::c++
    struct Point {
        int x, y, z;
    };

    Point getPoint();
    double calc(int, int, int);

    double result = std::apply(calc, getPoint()); // ill-formed today, ok with proposed implementation

# Wording

Add a new grammar option for *simple-declaration* to 10 [dcl.dcl]:

<blockquote>
<i>simple-declaration:</i><br />
<div style="margin-left: 2em;"><i>decl-specifier-seq</i> <i>init-declarator-list<sub>opt</sub></i> ;<br /></div>
<div style="margin-left: 2em;"><i>attribute-specifier-seq</i> <i>decl-specifier-seq</i> <i>init-declarator-list</i> ;</div>
<div style="margin-left: 2em;"><i>attribute-specifier-seq<sub>opt</sub></i> <i>decl-specifier-seq</i> <i>ref-qualifier<sub>opt</sub></i> [ <i>identifier-list</i> ] <i>initializer</i> ;</div>
<div style="margin-left: 2em;"><ins><i>attribute-specifier-seq<sub>opt</sub></i> <i>decl-specifier-seq</i> <i>ref-qualifier<sub>opt</sub></i> [ <i>identifier-list</i>,<sub>opt</sub> ...<i>identifier</i> ] <i>initializer</i> ;</ins></div>
</blockquote>

Expand 10 [dcl.dcl] paragraph 8:

> A _simple-declaration_ with an _identifier-list_ <ins>or an _identifier_ with preceding ellipsis</ins> is called a structured binding declaration ([dcl.struct.bind]). The _decl-specifier-seq_ shall contain only the _type-specifier_ `auto` and _cv-qualifiers_. The _initializer_ shall be of the form "= _assignment-expression_", of the form "{ _assignment-expression_ }", or of the form "( _assignment-expression_ )", where the _assignment-expression_ is of array or non-union class type.

Change 11.5 [dcl.struct.bind] paragraph 1:

<blockquote>
A structured binding declaration introduces <del>the identifiers v<sub>0</sub>, v<sub>1</sub>, v<sub>2</sub>, ... of the <i>identifier-list</i> as</del> names ([basic.scope.declarative]) of <i>structured bindings</i>. <ins>If the declaration contains an <i>identifier-list</i>, the declaration introduces the identifiers v<sub>0</sub>, v<sub>1</sub>, v<sub>2</sub>, ... of the <i>identifier-list</i> as names. If the declaration contains an <i>identifier</i> with preceding ellipsis, the declaration introduces a <i>structured binding pack</i> ([temp.variadic]).</ins> Let <i>cv</i> denote the <i>cv-qualifiers</i> in the <i>decl-specifier-seq</i>. 
</blockquote>

Introduce a new paragraph after [dcl.struct.bind] paragraph 1, introducing the term "structured binding size":

<blockquote>
<ins>The <i>structured binding size</i> of a type <code>E</code> is the required number of names that need to be introduced by the structured binding declaration, as defined below. If there is no structured binding pack, then the number of elements in the <i>identifier-list</i> shall be equal to the structured binding size. Otherwise, the number of elements of the structured binding pack is the structured binding size less the number of elements in the <i>identifier-list</i>.
</blockquote>

Change [dcl.struct.bind] paragraph 2 to define a structured binding size:

<blockquote>
If <code>E</code> is an array type with element type <code>T</code>, <del>the number of elements in the <i>identifier-list</i></del> <ins>the structured binding size of <code>E</code></ins> shall be equal to the number of elements of <code>E</code>. <del>Each <i>v<sub>i</sub></i></del> <ins>The <i>i</i><sup>th</sup> <i>identifier</i></ins> is the name of an lvalue that refers to the element <i>i</i> of the array and whose type is <code>T</code>; the referenced type is <code>T</code>.
</blockquote>

Change [dcl.struct.bind] paragraph 3 to define a structured binding size:

<blockquote>
Otherwise, if the <i>qualified-id</i> <code>std::tuple_size&lt;E></code> names a complete type, the expression <code class="language-cpp">std::tuple_size&lt;E>::value</code> shall be a well-formed integral constant expression and the <del>number of elements in the <i>identifier-list</i></del> <ins>structured binding size of <code>E</code></ins> shall be equal to the value of that expression. [...] <del>Each <i>v<sub>i</sub></i></del> <ins>The <i>i</i><sup>th</sup> <i>identifier</i></ins> is the name of an lvalue of type <code class="">T<sub>i</sub></code> that refers to the object bound to <code class="">r<sub>i</sub></code>; the referenced type is <code class="">T<sub>i</sub></code>.
</blockquote>

Change [dcl.struct.bind] paragraph 4 to define a structured binding size:

<blockquote>
Otherwise, all of <code>E</code>'s non-static data members shall be direct members of <code>E</code> or of the same base class of <code>E</code>, well-formed when named as <code>e.name</code> in the context of the structured binding, <code>E</code> shall not have an anonymous union member, and the <del>number of elements in the <i>identifier-list</i></del> <ins>structured binding size of <code>E</code></ins> shall be equal to the number of non-static data members of <code>E</code>. Designating the non-static data members of <code>E</code> as <code class="">m<sub>0</sub>, m<sub>1</sub>, m<sub>2</sub>, . . .</code> (in declaration order), <del>each <code class="">v<sub>i</i></code></del> <ins>the <i>i</i><sup>th</sup> <i>identifier</i></ins> is the name of an lvalue that refers to the member <code class="">m<sub>i</sub></code> of <code>e</code> and whose type is <i>cv</i> <code class="">T<sub>i</sub></code>, where <code class="">T<sub>i</sub></code> is the declared type of that member; the referenced type is <i>cv</i> <code class="">T<sub>i</sub></code>. The lvalue is a bit-field if that member is a bit-field.
</blockquote>

Add a new clause to 17.6.3 [temp.variadic], after paragraph 3:

<blockquote>
<ins>A <i>structured binding pack</i> is an <i>identifier</i> that introduces zero or more <i>structured binding</i>s ([dcl.struct.bind]). <i>[ Example</i>

```
auto foo() -> int(&)[2];
auto [...a] = foo();          // a is a structured binding pack containing 2 elements
auto [b, c, ...d] = foo();    // d is a structured binding pack containing 0 elements
auto [e, f, g, ...h] = foo(); // error: too many identifiers
```

<ins><i>-end example]</i></ins></blockquote>

In 17.6.3 [temp.variadic], change paragraph 4:

> A *pack* is a template parameter pack, a function parameter pack, <del>or</del> an *init-capture* pack<ins>, or a structured binding pack</ins>. The number of elements of a template parameter pack or a function parameter pack is the number of arguments provided for the parameter pack. The number of elements of an *init-capture* pack is the number of elements in the pack expansion of its *initializer*.

In 17.6.3 [temp.variadic], paragraph 5 (describing pack expansions) remains unchanged.

In 17.6.3 [temp.variadic], add a bullet to paragraph 8:

<blockquote>
Such an element, in the context of the instantiation, is interpreted as follows:
<ul>
<li> if the pack is a template parameter pack, the element is a template parameter ([temp.param]) of the corresponding kind (type or non-type) designating the 
<i>i</i><sup>th</sup> corresponding type or value template argument;
<li> if the pack is a function parameter pack, the element is an <i>id-expression</i> designating the  <i>i</i><sup>th</sup> function parameter that resulted from instantiation of the function parameter pack declaration; otherwise
<li> if the pack is an <i>init-capture</i> pack, the element is an <i>id-expression</i> designating the variable introduced by the <i>i</i><sup>th</sup>th <i>init-capture</i> that resulted from instantiation of the <i>init-capture</i> pack<del>.</del> <ins>; otherwise</ins>
<li><ins> if the pack is a structured binding pack, the element is an <i>id-expression</i> designating the <i>i</i><sup>th</sup> structured binding that resulted from the structured binding declaration.</ins>
</ul>
</blockquote>

# Acknowledgements

Thanks to Michael Park and Tomasz Kamiński for their helpful feedback.
