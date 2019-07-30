Title: Deduplicating Forwarding Overloads
Document-Number: D1500R0
Authors: Michael Park, mcypark at gmail dot com
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: EWGI

# Motivation

There are many places today where we want to deduce a specific type, but with unknown *cv-ref* qualifiers and unknown potentially derived type. Specific examples might be: `get<>` for `std::tuple`, which is currently specified as four overloads of the form:

    :::cpp
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>>& get(tuple<Ts...>&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>> const& get(tuple<Ts...> const&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>>&& get(tuple<Ts...>&&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>> const&& get(tuple<Ts...> const&&);
    
These overloads are all fundamentally the same kind of thing, but we have to write it four times in order to ensure that we handle all four *cv-ref* cases as well any potentially derived type. Ideally, what we _really_ want to write is a function template that deduces a particular type or class template with unknown qualifiers. That is, not an arbitrary forwarding reference - but a forwarding reference to a _specific type_.

This problem becomes much more apparent with the hopeful adoption of [P0847](https://wg21.link/p0847), where we run into the problem of needing to do mitigation against shadowing:

    :::cpp
    struct B {
        int i;
        auto&& get(this auto&& self) {
            return FWD(self).i;
        }
    };
    
    struct D : B {
        double i;
    };
    
    D{}.get(); // oops: rvalue reference to D::i instead of B::i
    
What we really want `B::get` to do is to take, specifically, some kind of reference to `B`. We want to deduce _just_ the qualifiers, and apply them to the member as expected, but we have no way in the language to do that today. We can't use Concepts - those cannot perform conversions, specifically derived-to-base conversions. We can only verify if either the type _is_ a specific kind or _derives from_ a specific kind, but that's not sufficient for these purposes.

To demonstrate:

    :::cpp
    void foo(Same<B> auto x);
    void bar(DerivedFrom<B> auto y);
    
    foo(D{}); // error
    bar(D{}); // ok, but y is still a D when we really want it to be a B
    
We need to not just constrain the deduction, but also convert down to the base class for these cases.

# Proposal

We propose the following syntax:

    :::cpp
    void foo(std::string auto&& s);

to mean an overload set whose parameter is a "forwarding reference to `std::string`." This has the effect of stamping out the four functions:

    :::cpp
    void foo(std::string&);
    void foo(std::string const&);
    void foo(std::string&&);
    void foo(std::string const&&);
    
This syntax is derived from the terse concepts syntax ([P1141](https://wg21.link/p1141)), where we already have as precedent the notion of abbreviated function template using the syntax `Concept auto` - here instead of a concept name we use a type name. The notable difference is that we do not propose a long form syntax here - solely the terse syntax.

## Conversion Functions

The model we're proposing for this feature is stamping out the combinatorial explosion of *cv-ref* qualifiers (all four of them!) - which has the effect of allowing conversions in non-template scenarios.

For example:

    :::cpp
    struct X {
        operator std::string() cosnt;
    };
    
    void bar(std::string auto&&);
    
    /* roughly equivalent to
    void bar(std::string&);
    void bar(std::string const&);
    void bar(std::string&&);
    void bar(std::string const&&);
    */
    
    bar(X{}); // ok, calls bar<std::string&&>

But if the proposed forwarding reference function template is still an underlying template, conversion functions would not apply:

    :::cpp
    struct Y {
        operator std::tuple<int>() const;
    };
    
    template <typename... Ts>
    void quux(std::tuple<Ts...> auto&&);
    
    /* roughly equivalent to
    template <typename... Ts> void quux(std::tuple<Ts...>&);
    template <typename... Ts> void quux(std::tuple<Ts...> const&);
    template <typename... Ts> void quux(std::tuple<Ts...>&&);
    template <typename... Ts> void quux(std::tuple<Ts...> const&&);
    */
    
    quux(Y{}); // error

## To Template or not To Template

Even though we use the `auto` keyword to declare these functions - we don't view these kinds of functions as actual templates, behaviorally. We very much consider the right model for this proposal to be stamping out the right overloads. With the Concepts terse syntax, we are deducing an unknowably many and potentially infinite amount of types. With this proposal, we have a known, finite collection of overloads. There are only eight possibilities: the Cartesian product of {`const`, non-`const`} x {`volatile`, non-`volatile`} x {`&`, `&&`}. And let's be serious, we don't really care about `volatile`.

As such, these do not have to be templates. We know at the point of declaration all the potential "instantiations" of these functions. This allows us to actually write code like:

    :::cpp
    // Person.h
    class Person {
        std::string last;
        std::string first;
    public:
        Person(std::string auto&&, std::string auto&&);
    };

and then define that constructor in a source file:

    :::cpp
    // Person.cxx
    Person::Person(std::string auto&& l, std::string auto&& f)
        : last(FWD(l))
        , first(FWD(f))
    { }
    
## Special Member Functions

Since these forwarding functions are not templates, they can count as special member functions. Meaning that:

    :::cpp
    struct S {
        S(S auto&&) = default;
    };
    
Defaults both the copy and move constructors - but for both the `const` and non-`const` cases. This has the added benefit that such a construction avoids the usual issues with forward references - since we're declaring the non-`const` verisons as well:

    :::cpp
    struct S {
        S();
    
        S(S auto&&); // #1
        
        template <typename T>
        S(T&&); // #2
    };
    
    S s;
    s s2 = s; // ok: calls #1, not #2
    
# Examples

<table style="width:100%">
<tr>
<th style="width:50%">
Today
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<th colspan="2">
Deduplication
</th>
<tr>
<td>
    :::cpp
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>>&
    get(tuple<Ts...>&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>> const&
    get(tuple<Ts...> const&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>>&&
    get(tuple<Ts...>&&);
    
    template <size_t I, typename... Ts>
    constexpr tuple_element_t<I, tuple<Ts...>> const&&
    get(tuple<Ts...> const&&);
</td>
<td>
    :::cpp
    template <size_t I, typename... Ts>
    constexpr auto get(tuple<Ts...> auto&& t)
        -> copy_cv_ref<decltype(t), tuple_element_t<I, tuple<Ts...>>;
</td>
</tr>
<tr>
<th colspan="2">
Multiple Forwarding without Combinatorial Explosion
</th>
</tr>
<tr>
<td>
    :::cpp
    class Person {
        std::string last;
        std::string first;
    public:
        // by-value for convenience, which potentially
        // triggers an extra move - but means we only
        // have to write one single function
        Person(std::string l, std::string f)
            : last(std::move(l))
            , first(std::move(f))
        { }
    };
</td>
<td>
    :::cpp
    class Person {
        std::string last;
        std::string first;
    public:
        // still one "function" - but optimal performance
        Person(std::string auto&& l, std::string auto&& f)
            : last(FWD(l))
            , first(FWD(f))
        { }
    };
</td>
</tr>
<tr>
<th colspan="2">
Shadowing mitigation
</th>
</tr>
<tr>
<td>
    :::cpp
    struct B {
        int i;
        auto&& get(this auto&& self) {
            return FWD(self).i;
        }
    };
    
    struct D : B {
        double i;
    };
    
    D{}.get(); // oops
</td>
<td>
    :::cpp
    struct B {
        int i;
        auto&& get(this B auto&& self) {
            return FWD(self).i;
        }
    };
    
    struct D : B {
        double i;
    };
    
    D{}.get(); // no shadowing
</td>
</tr>
<tr>
<th colspan="2">
Shadowing mitigation privately
</th>
</tr>
<tr>
<td>
    :::cpp
    struct B {
        int i;
        auto&& get(this auto&& self) {
            // trying to avoid shadowing problems
            // (see above)
            return FWD(self).B::i;
        }
    };
    
    struct D : private B {
        double i;
        int bar() { return get(); }
    };
    
    D{}.bar(); // error in B::get
</td>
<td>
    :::cpp
    struct B {
        int i;
        auto&& get(this B auto&& self) {
            // don't need to worry about shadowing
            return FWD(self).i;
        }
    };
    
    struct D : private B {
        int bar() { return get(); }
    };
    
    D{}.bar(); // ok
</td>
</tr>
</table>