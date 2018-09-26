<pre class='metadata'>
Title: Deducing `this`
Status: D
ED: http://wg21.link/P0847
Shortname: D0847
Level: 1
Date: 2018-03-05
Editor: Gašper Ažman, gasper dot azman at gmail dot com
Editor: Simon Brand, simon at codeplay dot com
Editor: Ben Deane, ben at elbeno dot com
Editor: Barry Revzin, barry dot revzin at gmail dot com
Group: wg21
Audience: EWG
Markup Shorthands: markdown yes
Default Highlight: C++
Abstract: We propose a new mechanism for specifying or deducing the value category of an instance of a class. In other words, a way to tell from within a member function whether the object it's invoked on is an lvalue or an rvalue, and whether it is const or volatile.
</pre>

# Revision History

## Changes since r0

[P0847R0](https://wg21.link/p0847r0) was presented in Rapperswil in June 2018 using an already adjusted syntax from the one used in the paper, using `this Self&& self` to indicate the explicit object parameter rather than `Self&& this self`. EWG took [two direction polls][rap.p0847r0] there:

> *If an explicitly named (e.g. `self`) object parameter, should `this` be implicitly or explicitly usable in member function body?*

>  <table style="font-size=8px"><tr><th>SF</th><th>F</th><th>N</th><th>A</th><th>SA</th></tr><tr><td>0</td><td>2</td><td>9</td><td>14</td><td>12</td></table>

This poll is fully adopted in this revision - changing the behavior of explicit object parameter functions from modeling member functions to modeling non-member `friend`s.

> *Encourage putting this-type identifier stuff in usual cv-ref qualifier location?*

>
    :::cpp
    template <typename Self>
    void foo(int) Self&& self;

>  <table><tr><th>SF</th><th>F</th><th>N</th><th>A</th><th>SA</th></tr><tr><td>9</td><td>11</td><td>10</td><td>5</td><td>2</td></table>

This revision as presented adopts a slight variant of the direction suggested by this poll (without an identifier - see [parsing issues](#parsing-issues)), and is discussed at length in an [alternative solution](#alternative-solution). The syntax change is:

<table style="width:100%">
<tr>
<th style="width:50%">
As presented in Rapperswil
</th>
<th style="width:50%">
This proposal
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    struct optional {
      template <typename Self>
      constexpr like_t<Self&&, T> operator*(this Self&&) {
        return forward_like<Self>(*this).m_value;
      }
    };
</td>
<td>
    :::cpp
    template <typename T>
    struct optional {
      template <typename Self>
      constexpr like_t<Self&&, T> operator*() Self&& {
        return forward<Self>(*this).optional::m_value;
      }
    };
</td>
</tr>
</table>

This new revision also proposes a slight change in template deduction rules specific to the new explicit member type. 

# Motivation

In C++03, member functions could have *cv*-qualifications, so it was possible to have scenarios where a particular class would want both a `const` and non-`const` overload of a particular member (Of course it was possible to also want `volatile` overloads, but those are less common). In these cases, both overloads do the same thing - the only difference is in the types accessed and used. This was handled by either simply duplicating the function, adjusting types and qualifications as necessary, or having one delegate to the other. An example of the latter can be found in Scott Meyers' ["Effective C++"][meyers.effective], Item 3:

    :::cpp
    class TextBlock {
    public:
      const char& operator[](std::size_t position) const {
        // ...
        return text[position];
      }

      char& operator[](std::size_t position) {
        return const_cast<char&>(
          static_cast<const TextBlock&>(*this)[position]
        );
      }
      // ...
    };

Arguably, neither the duplication or the delegation via `const_cast` are great solutions, but they work.

In C++11, member functions acquired a new axis to specialize on: ref-qualifiers. Now, instead of potentially needing two overloads of a single member function, we might need four: `&`, `const&`, `&&`, or `const&&`. We have three approaches to deal with this: we implement the same member four times, we can have three of the overloads delegate to the fourth, or we can have all four delegate to a helper, private static member function. One example might be the overload set for `optional<T>::value()`. The way to implement it would be something like:

<table style="width:100%">
<tr>
<th style="width:33%">
Quadruplication
</th>
<th style="width:33%">
Delegation to 4th
</th>
<th style="width:33%">
Delegation to helper
</th>
</tr>
<tr>
<td>
    ::cpp
    template <typename T>
    class optional {
        // ...
        constexpr T& value() & {
            if (has_value()) {
                return this->m_value;
            }
            throw bad_optional_access();
        }

        constexpr const T& value() const& {
            if (has_value()) {
                return this->m_value;
            }
            throw bad_optional_access();
        }

        constexpr T&& value() && {
            if (has_value()) {
                return std::move(this->m_value);
            }
            throw bad_optional_access();
        }

        constexpr const T&&
        value() const&& {
            if (has_value()) {
                return std::move(this->m_value);
            }
            throw bad_optional_access();
        }
        // ...
    };
</td>
<td>
    :::cpp
    template <typename T>
    class optional {
        // ...
        constexpr T& value() & {
            return const_cast<T&>(
                static_cast<optional const&>(
                    *this).value());
        }

        constexpr const T& value() const& {
            if (has_value()) {
                return this->m_value;
            }
            throw bad_optional_access();
        }

        constexpr T&& value() && {
            return const_cast<T&&>(
                static_cast<optional const&>(
                    *this).value());
        }

        constexpr const T&&
        value() const&& {
            return static_cast<const T&&>(
                value());
        }
        // ...
    };
</td>
<td>
    :::cpp
    template <typename T>
    class optional {
        // ...
        constexpr T& value() & {
            return value_impl(*this);
        }

        constexpr const T& value() const& {
            return value_impl(*this);
        }

        constexpr T&& value() && {
            return value_impl(std::move(*this));
        }

        constexpr const T&&
        value() const&& {
            return value_impl(std::move(*this));
        }

    private:
        template <typename Opt>
        static decltype(auto)
        value_impl(Opt&& opt) {
            if (!opt.has_value()) {
                throw bad_optional_access();
            }
            return std::forward<Opt>(opt).m_value;
        }


        // ...
    };
</td>
</tr>
</table>

It's not like this is a complicated function. Far from. But more or less repeating the same code four times, or artificial delegation to avoid doing so, is the kind of thing that begs for a rewrite. Except we can't really. We *have* to implement it this way. It seems like we should be able to abstract away the qualifiers. And we can... sort of. As a non-member function, we simply don't have this problem:

    :::cpp
    template <typename T>
    class optional {
        // ...
        template <typename Opt>
        friend decltype(auto) value(Opt&& o) {
            if (o.has_value()) {
                return std::forward<Opt>(o).m_value;
            }
            throw bad_optional_access();
        }
        // ...
    };

This is great - it's just one function, that handles all four cases for us. Except it's a non-member function, not a member function. Different semantics, different syntax, doesn't help.

There are many, many cases in code-bases where we need two or four overloads of the same member function for different `const`- or ref-qualifiers. More than that, there are likely many cases that a class should have four overloads of a particular member function, but doesn't simply due to laziness by the developer. We think that there are sufficiently many such cases that they merit a better solution than simply: write it, then write it again, then write it two more times.

# Proposal

We propose a new way of declaring a member function that will allow for deducing the type and value category of the class instance parameter, while still being invocable as a member function. 

Today, member functions can optionally have a *cv-qualifier* and can optionally have a *ref-qualifier*. We propose extending this to allow an explicit type name as well. This type name can simply be the class type, but more importantly it can be a function template parameter, in which case it will be deduced from the object parameter that the member function was invoked on. 

We believe that the ability to write *cv-ref qualifier*-aware member functions without duplication will improve code maintainability, decrease the likelihood of bugs, and allow users to write fast, correct code more easily. The added ability to deduce derived types additionally will bring in new idioms not previously available to us. 

A brief example demonstrating how to write `optional::value()` and `optional::operator->()` in just two functions with no duplication (instead of six) with this proposal:

    :::cpp
    template <typename T>
    struct optional {
        template <typename Self>
        constexpr auto&& value() Self&& {
            if (!this->optional::has_value()) {
                throw bad_optional_access();
            }
            
            return forward<Self>(*this).optional::m_value;
        }
        
        template <typename Self>
        constexpr auto operator->() Self {
            return addressof(this->optional::m_value);
        }
    };

What follows is a description of how explicit member object types affect all the important language constructs: name lookup, type deduction, overload resolution, and so forth. 

This is a strict extension to the language; all existing syntax remains valid.

## Name lookup: candidate functions

Today, when either invoking a named function or an operator (including the call operator) on an object of class type, name lookup will include both static and non-static member functions found by regular class lookup. Non-static member functions are treated as if there were an implicit object parameter whose type is an lvalue or rvalue reference to *cv* `X` (where the reference and *cv* qualifiers are determined based on the function's qualifiers) which binds to the object on which the function was invoked. 

For non-static member functions with the new **explicit** member object type, lookup will work the same way as other member functions today, except rather than implicitly determining the type of the object parameter based on the *cv*- and *ref*-qualifiers of the member function, these are explicitly determined by the provided type. The following examples illustrate this concept.

<table style="width:100%">
<tr>
<th style="width:50%">
C++17
</th>
<th style="width:50%">
With Explicit Type
</th>
</tr>
<tr>
<td>
    :::cpp
    struct X {
        // implicit object has type X&
        void foo();

        // implicit object has type X const&
        void foo() const;

        // implicit object has type X&&
        void bar() &&;
    };
</td>
<td>
    :::cpp
    struct X {
        // explicit object has type X&
        void ex_foo() X;

        // explicit object has type X const&
        void ex_foo() X const;

        // explicit object type X&&
        void ex_bar() X&&;
    };
</td>
</tr>
</table>

The overload resolution rules for this new set of candidate functions remains unchanged - we're simply being explicit rather than implicit about the object parameter. Given a call to `x.ex_foo()`, overload resolution would select the first `ex_foo()` overload if `x` isn't `const` and the second if it is. The behaviors of the two columns as proposed are exactly equivalent.


## Type deduction

One of the main motivations of this proposal is to deduce the *cv*-qualifiers and value category of the class object, so the explicit member type needs to be deducible from the object that the member function is invoked on:

    :::cpp
    struct X {
        template <typename Self>
        void foo(int i) Self&&;
    };

    X x;
    x.foo(4);            // Self deduces as X&
    std::move(x).foo(2); // Self deduces as X

A common source of duplication of member functions revolves solely around wanting non-`const` and `const` overloads of a member function - that otherwise do exactly the same thing. To solve this problem, we propose a slight change to template deduction rules such that when deducing an explicit member type, we _do not_ drop the *cv*-qualifiers. In other words:

    :::cpp
    struct Y {
    public:
        Y(int i) : i(i) { }
        template <typename Self>
        auto& get() Self {
            return this->i;
        }
    private:
        int i;
    };
    
    void ex(Y& y, Y const& cy) {
        y.get();             // deduces Self as Y, returns int&
        cy.get();            // deduces Self as Y const, not Y, returns int const&
        
        std::move(y).get();  // deduces Self as Y, returns int&
        std::move(cy).get(); // deduces Self as Y const, returns int const&
    }
    
This mimics today's behavior where a trailing `const` qualifier does not mean `const&`, it just means `const`. Without such a change to template deduction, `Self` would always just deduce as `Y` (and hence be pointless), `Self&` would deduce as `Y&` or `Y const&` but not allow binding to rvalues, and `Self&&` would give us different functions for lvalues and rvalues - which is unnecessary and leads to code bloat.

Since the explicit member type is deduced from the object the function is called on, this has the interesting effect of possibly deducing _derived_ types, which can best be illustrated by the following example:    
    
    :::cpp
    struct B {
        int i = 0;

        template <typename Self>
        auto&& get() Self&& {
            // NB: specifically this->i, see next section as to why
            return this->i;
        }
    };

    struct D : B {
        // shadows B::i
        double i = 3.14;
    };

    B b{};
    B const cb{};
    D d{};

    b.get();            // #1
    cb.get();           // #2
    d.get();            // #3
    std::move(d).get(); // #4

The proposed behavior of these calls is:

 1. `Self` is deduced as `B&`, this call returns an `int&` to `B::i`
 2. `Self` is deduced as `B const&`, this calls returns an `int const&` to `B::i`
 3. `Self` is deduced as `D&`, this call returns a `double&` to `D::i`
 4. `Self` is deduced as `D`, this call returns a `double&` to `D::i`

When we deduce the explicit member type, we don't just deduce the *cv*- and *ref*-qualifiers. We may also get a derived type. This follows from the normal template deduction rules. In `#3`, for instance, the object parameter is an lvalue of type `D`, so `Self` deduces as `D&`.

## Name lookup: within member functions with explicit member type

So far, we've only considered how member functions with explicit member types get found with name lookup and how they deduce that parameter. Now let's move on to how the bodies of these functions actually behave. 

The model we are proposing is that member functions with explicit member types behave more like _non-member_ `friend` functions than normal non-static member functions from the perspective of lookup. In other words, everything in the class is accessible - but you cannot access them unqualified, only through an object. What object do we have? `*this`. 

Today, `this` is always a `const` pointer to *cv-qualifier* class type. With this proposal, for member functions with explicit member type, `this` becomes a `const` pointer to `remove_reference_t<T>`:

    :::cpp
    struct X {
        void a();            // this is a X* const
        void b() const;      // this is a X const* const
        void c() &&;         // this is a X* const (ref-qualifiers don't count)
    
        void d() X;          // this is a X* const (same as a)
        void e() X&&;        // this is a X* const (same as c)
        
        template <typename S>
        void f() S;          // this is a S* const
        template <typename S>
        void g() S&;         // this is a S* const
        template <typename S>
        void h() S&&;        // this is a remove_reference_t<S>* const
    };

For some of these member functions, `this` might not point to an `X` - it might point to a type derived from `X` and it might be dependent!

It is this new dependent possibility for `this` and potential confusion that could arise from that that led to the skewed result from one of the polls cited earlier.
    
Consider a slightly expanded version of the previous example:

    :::cpp
    struct B {
        int i = 0;

        template <typename Self>
        auto&& f1() Self&& { return i; }

        template <typename Self>
        auto&& f2() Self&& { return this->i; }

        template <typename Self>
        auto&& f3() Self&& { return forward<Self>(*this).i; }

    struct D : B {
        double i = 3.14;
    };


Consider invoking each of these functions with an lvalue of type `D`. As described in the previous section, `Self` in each case will be `D&`. 

We propose that each of these behave as follows:

- `f1` is ill-formed due to the inability to find `i`. Remember the model here is non-member `friend`s, so any non-static access must be through an object, and our only object is `*this`.
- `f2` returns an lvalue reference to `D::i`, because `this` is a `D* const`.
- `f3` behaves the same as `f2` here, since we're invoking on an lvalue. If we were invoking on an rvalue of type `D`, then this would `f2` would behave the same by `f3` would now return an rvalue reference to `D::i`.

But what if I really wanted to return `B::i`? How do I do that? Today, we have two options. You could either cast `this` to the appropriately qualified pointer to `B` (or `*this` to the appropriately qualified reference to `B`), or you could use a more explicit member access syntax:

    :::cpp
    template <typename Self>
    auto&& f4() Self&& {
        // a forwarding reference to B::i
        return static_cast<like_t<Self, B>&&>(*this).i;
    }
    
    template <typename Self>
    auto&& f5() Self&& {
        // lvalue reference to B::i (const or non-const)
        return this->B::i;
    }
    
    template <typename Self>
    auto&& f6() Self&& {
        // forwarding reference to B::i
        return forward<Self>(*this).B::i;
    }    

This is admittedly tricky, so we encourage authors to come up with a syntax where in we can deduce `B` directly (instead of deducing `D` in these cases).

## Writing the function pointer types for such functions

The proposed change allows for deducing the object parameter's value category and *cv*-qualifiers. But the member functions themselves are otherwise the same as you could express today, and their types do not change.

In other words, given:

    :::cpp
    struct Y {
        int f(int, int) const&;
        int g(int, int) Y const&;
    };
    
`Y::f` and `Y::g` are equivalent from a signature standpoint, so both `&Y::f` and `&Y::g` have the type `int(Y::*)(int, int) const&`.

Deduction doesn't change this either - it's just that we add the ability to deduce the class type as well as normal argument types:

    :::cpp
    struct Z {
        template <typename Self, typename T>
        void h(T&&) Self&&;
    };
    
    struct DZ : Z { };
    
    Z z;
    z.h(1); // the pointer to the member function invoked by this
            // expression is void (Z::*)(int&&) &

    auto pmf = &Z::h<Z&, int>;
    (z.*pmf)(1); // same as above
            
    DZ const dz;
    dz.h(2.0);  // the pointer to member function invoked by this 
                // expression is void (DZ::*)(double&&) const&
                
    auto pmf2 = &Z::h<DZ const&, double>
    (dz.*pmf2)(2.0); // same as above

                
## Teachability Implications

A natural extension of having trailing *cv-* and *ref-qualifiers* to non-static member functions is providing an explicit type that those qualifers refer to, instead of the implied class type. This keeps all of the qualifiers in the same place. The ability to deduce this type follows once we have a place where we can name it. 

While having `this` become dependent is novel, requiring all access through `this` and forbidding "free" member access ensures that these functions will still be clear to write and understand. 

## Can `static` member functions have an explicit object type?

Since `static` functions can also be invoked on objects, it raises the question of whether we could deduce that object as well (even if the function, being `static`, would not take a binding to it). 

But no. Static member functions currently do not have an implicit `this` parameter, and therefore have no reason to provide an explicit type for one. 

## Interplays with capturing `[this]` and `[*this]` in lambdas

Providing an explicit member type for lambdas still works, and even has some use, but the rules for what `this` means in a lambda today still apply: `this` can only ever refer to a captured member pointer of an outer member function, and never be a pointer to the lambda instance itself:

    :::cpp
    struct X {
        int x, y;

        auto getter() const
        {
            return [*this]<typename Self>() Self&& {
                return x       // still refers to X::x
                    + this->y; // still refers to X::y
            };
        }
    };

If other language features play with what `this` means, they are completely orthogonal and do not have interplays with this proposal. However, it should be obvious that developers have great potential for introducing hard-to-read code if they are at all changing the meaning of `this` in function bodies, especially in conjunction with this proposal.

## Translating code to use explicit member types

The most common qualifier overload sets for member functions are:

1. `const` and non-`const`
2. `&`, `const&`, `&&`, and `const&&`
3. `const&` and `&&`

Some examples:

<table>
<tr>
<th>
</th>
<th>
Today
</th>
<th>
Proposed
</th>
</tr>
<tr>
<td>
`const` and non-`const`
</td>
<td>
    :::cpp
    struct foo {
        void bar();
        void bar() const;
    };
</td>
<td>
    :::cpp
    struct foo {
        template <typename Self>
        void bar() Self;
    }
</td>
</tr>
<td>
`&`, `const&`, <br />`&&`, and `const&&`
</td>
<td>
    :::cpp
    struct foo {
      void quux() &;
      void quux() const&;
      void quux() &&;
      void quux() const&&;
    };
</td>
<td>
    :::cpp
    struct foo {
        template <typename Self>
        void quux(Self&&);
    };
</td>
</tr>
<tr>
<td>
Just `const&` and `&&`
</td>
<td>
    :::cpp
    struct foo {
        void baz() const&;
        void baz() &&;
    };
</td>
<td>
    :::cpp
    struct foo {
        template <typename Self>
        void baz() Self&&;
    };
</td>
</tr>
</table>

The first two cases are neatly and exactly handled. For the third case, there is no direct equivalent - but such situations are typically just laziness on the part of the library developer rather than having a meaningful foundation; they are also simple to disable with a requires clause, now that the object type can be captured.

## Parsing issues

With the addition of a new type name after the *parameter-declaration-clause*, we potentially run into a clash with the existing *virt-specifier*s. In other words, this:

    :::cpp
    struct B {
        virtual B* override() = 0;
    };
    
    struct override : B {
        override* override() override override; // #1
        override* override() override;          // #2
        override* override();                   // #3
    };

The same problem would occur with `final`. 

Dealing with `#3` is easy - there is nothing to do. For `#1` and `#2`, the rule we propose is that we try to parse the explicit member type _first_, if possible. That is, for `#2`, this is a member function with an explicit member type `override` (that happens to override `B::override`) but does not actually have a *virt-specifier*. For `#1` then, the first use of `override` is the explicit object type and the second use would be the *virt-specifier* `override`. This changes the meaning of `#2` from what it is today, but in practice we don't think anybody actually writes this seriously so it is unlikely to break real code.

As a result, this would be ill-formed:

    :::cpp
    struct D : B {
        D* override() override D; // error
    };

because `override` cannot be the explicit member type, so it's treated as the *virt-specifier*, and then `D` cannot go in that spot.

But both of these would be okay, with `override const` being the explicit member type:

    :::cpp
    struct override : B {
        override* override() override const override; // ok
        override* override() const override override; // ok
    };
    
In the presented design, the type name is something that can be looked up - it's going to be the class name or a template parameter name or something. So adopting new context-sensitive keywords is also unlikely to cause a problem.

However, the design briefly discussed in Rapperswil would have allowed an arbitrary trailing identifier. That is:

    :::cpp
    struct X {
        template <typename Self>
        auto& foo() Self&& self {
            return self.i; // <== self.i instead of this->i
        }
        int i;
    };
    
    struct override : B {
        override* override() override override override; // #4
    };
    
We feel that this would be grabbing too much real estate with minimal benefit - as it would constrain further evolution of the standard too much and make it more difficult to use. If we adopted the ability to provide an arbitrary identifier to name the object parameter, we would have to preferentially parse this identifier. Let's say we then added a new context-sensitive keyword, like `super`. A user might try to write:

    :::cpp
    struct Y {
        // intending to use the new context-sensitive keyword but
        // really is providing a name to the object parameter
        void a() super;
        
        // same
        void b() Y super;
        
        // okay, these finally use the keyword as desired - the user
        // has to provide an identifier, even if they don't want one
        void c() _ super;
        void d() Y _ super;
    };

Without an arbitrary identifier, `a()` and `b()` both treat `super` as the context-sensitive keyword as likely intended, with the only edge case being in the scenario where you have a type named `super`. 
    
## Alternative solution

The initial revision approached the problem of deducing the object parameter with the introduction of an explicit object parameter rather than an explicit member type. The explicit object parameter fits more closely with many programmers' mental model of the `this` pointer being the first parameter to member functions "under the hood" and is comparable to usage in other languages, e.g. Python and Rust. The explicit member type is more consistent with the member functions we have today where the *cv-* and *ref-qualifiers* are trailing rather than leading. 

The explicit parameter approach gave us the ability to have recursive lambdas without a new language feature:

    ::cpp
    auto fib = [](this auto const& self, int n) {
        if (n < 2) return n;
        return self(n-1) + self(n-2);
    };
    
While this proposal lets us give a name to the lambda's _type_, it does not let us give a name to the lambda _instance_. So such recursion would still require a new language feature, such as [P0839](https://wg21.link/p0839).

However, [P0839](https://wg21.link/p0839) would prevent us from being able to just deduce `const`-ness, as this proposal does. 

Additionally, a named object parameter is arguably less confusing than having `this` potentially refer to a different type - and makes it less surprising that we cannot directly access our members without said parameter. Here is a comparison between this proposal and the R0 proposal adjusted for requiring explicit access for the initial `optional` example. They are pretty similar:

<table style="width:100%">
<tr>
<th style="width:50%">
Explicit member type
</th>
<th style="width:50%">
Explicit object parameter
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    struct optional {
        template <typename Self>
        constexpr auto&& value() Self&& {
            if (!this->optional::has_value()) {
                throw bad_optional_access();
            }
            
            return forward<Self>(*this).optional::m_value;
        }
       


 
 
        template <typename Self>
        constexpr auto operator->() Self {
            return addressof(this->optional::m_value);
        }




        
        template <typename Self, typename F>
        constexpr auto and_then(F&& f) Self&& {
            using val = decltype((
                forward<Self>(*this).optional::m_value));
            using result = invoke_result_t<F, val>;

            static_assert(
              is_optional<result>::value,
              "F must return an optional");

            return has_value()
                ? invoke(forward<F>(f),
                         forward<Self>(*this).optional::m_value
                : nullopt;
        }

    };
</td>
<td>
    :::cpp
    template <typename T>
    struct optional {
        template <typename Self>
        constexpr auto&& value(this Self&& self) {
            // access is via self, not this
            if (!self.optional::has_value()) {
                throw bad_optional_access();
            }
            
            return forward<Self>(self).optional::m_value;
        }
        
        // NB: we deduce as Self&& here. We cannot just deduce
        // 'const'-ness because "Self self" looks like a value
        // parameter
        template <typename Self>
        constexpr auto operator->(Self&& self) {
            return addressof(self.optional::m_value);
        }
        
        // NB: the function parameter is the first parameter to
        // the function but is listed second. This would be 
        // invoked as opt.and_then(f)
        // Otherwise only difference is the name self vs this
        template <typename Self, typename F>
        constexpr auto and_then(this Self&& self, F&& f) {
            using val = decltype((
                forward<Self>(self).optional::m_value));
            using result = invoke_result_t<F, val>;

            static_assert(
              is_optional<result>::value,
              "F must return an optional");

            return has_value()
                ? invoke(forward<F>(f),
                         forward<Self>(self).optional::m_value
                : nullopt;
        }
    };
</td>
</tr>
</table>

## Potential Extensions

There are a few syntactic extensions that would make it easier to deal with the case where we deduce a derived type but never actually want to use the derived type. 

Today, it is legal to use a *qualified-id* to directly access a member of a particular object in the hierarchy. But there does not exist a syntax (outside of a `static_cast`) to just get the base object. If we extend the access syntax to just allow for the naming of a type, we could get automatic qualification:

    :::cpp hl_lines="7"
    struct B {
        int i;
    
        template <typename Self>
        auto foo() Self {
            auto i = this->B::i; // legal today
            auto b = this->B; // illegal today, proposed to have obvious meaning
        };
    };
    
    struct D : B { };
    
    void ex(B& b, D const& cd) {
        b.foo();  // deduces Self as B, 'b' has type B*
        cd.foo(); // deduces Self as D const, 'b' has type B const*
    }

We could also introduce a new "magic" cast that just gives us a pointer to the type we're in right now from `this`:

    :::cpp
    struct B {
        template <typename Self>
        auto foo() Self {
            // core proposal to get a B [const]*
            auto b1 = static_cast<conditional_t<is_const_v<Self>, B const, B>*>(this);
            
            // above extension to get a B [const]*
            auto b2 = this->B;
            
            // magic cast
            auto b3 = this_cast(this);
        }
    };

This problem extends further than just to `Self`, though. It is common to only want to deduce the ref-qualifier in all sorts of contexts. Any "make it easy to get the base class pointer"-style feature suffers extra instantiations when we only really want the instantiations for the base class. A complementary feature could be proposed that constrains *deduction* (as opposed to removing candidates once they are deduced, as with `requires`, with the following straw-man syntax:

    ::cpp
    struct B {
        template <typename Self : B>
        auto front(this Self&& self) {

        }
    };
    struct D : B { };

    // also works for free functions
    template <typename T : B>
    void foo(T&& x) {
       static_assert(std::is_same_v<B, std::remove_reference_t<T>>);
    }
    
    foo(B{}); // calls foo<B>
    foo(D{}); // also calls foo<B>

This would create a function template that may only generate functions that take a `B`, ensuring that, when they participate in overload resolution, we don't generate additional instantiations. Such a proposal would change how templates participate in overload resolution, however, and is not to be attempted haphazardly.

# Real-World Examples

## Deduplicating Code

This proposal can de-duplicate and de-quadruplicate a large amount of code. In each case, the single function is only slightly more complex than the initial two or four, which makes for a huge win. What follows are a few examples of how repeated code can be reduced.

The particular implementation of optional is Simon's, and can be viewed on [GitHub][brand.optional], and this example includes some functions that are proposed in [P0798](https://wg21.link/p0798), with minor changes to better suit this format:

<table style="width:100%">
<tr>
<th style="width:50%">
C++17
</th>
<th style="width:50%">
This proposal
</th>
</tr>
<tr>
<td>
    :::cpp
    class TextBlock {
    public:
      const char& operator[](std::size_t position) const {
        // ...
        return text[position];
      }

      char& operator[](std::size_t position) {
        return const_cast<char&>(
          static_cast<const TextBlock&>
            (this)[position]
        );
      }
      // ...
    };
</td>
<td>
    :::cpp
    class TextBlock {
    public:
      template <typename Self>
      auto& operator[](std::size_t position) Self {
        // ...
        return this->text[position];
      }
      // ...
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      constexpr T* operator->() {
        return std::addressof(this->m_value);
      }

      constexpr const T*
      operator->() const {
        return std::addressof(this->m_value);
      }
      // ...
    };
</td>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      template <typename Self>
      constexpr auto operator->() Self {
        return std::addressof(this->m_value);
      }
      // ...
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      constexpr T& operator*() & {
        return this->m_value;
      }

      constexpr const T& operator*() const& {
        return this->m_value;
      }

      constexpr T&& operator*() && {
        return std::move(this->m_value);
      }

      constexpr const T&&
      operator*() const&& {
        return std::move(this->m_value);
      }

      constexpr T& value() & {
        if (has_value()) {
          return this->m_value;
        }
        throw bad_optional_access();
      }

      constexpr const T& value() const& {
        if (has_value()) {
          return this->m_value;
        }
        throw bad_optional_access();
      }

      constexpr T&& value() && {
        if (has_value()) {
          return std::move(this->m_value);
        }
        throw bad_optional_access();
      }

      constexpr const T&& value() const&& {
        if (has_value()) {
          return std::move(this->m_value);
        }
        throw bad_optional_access();
      }
      // ...
    };
</td>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      template <typename Self>
      constexpr like_t<Self, T>&& operator*() Self&& {
        return forward_like<Self>(*this).m_value;
      }

      template <typename Self>
      constexpr like_t<Self, T>&& value() Self&& {
        if (this->has_value()) {
          return forward_like<Self>(*this).m_value;
        }
        throw bad_optional_access();
      }
      // ...
    };
</td>
</tr>
<tr>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      template <typename F>
      constexpr auto and_then(F&& f) & {
        using result =
          invoke_result_t<F, T&>;
        static_assert(
          is_optional<result>::value,
          "F must return an optional");

        return has_value()
            ? invoke(forward<F>(f), **this)
            : nullopt;
      }

      template <typename F>
      constexpr auto and_then(F&& f) && {
        using result =
          invoke_result_t<F, T&&>;
        static_assert(
          is_optional<result>::value,
          "F must return an optional");

        return has_value()
            ? invoke(forward<F>(f),
                     std::move(**this))
            : nullopt;
      }

      template <typename F>
      constexpr auto and_then(F&& f) const& {
        using result =
          invoke_result_t<F, const T&>;
        static_assert(
          is_optional<result>::value,
          "F must return an optional");

        return has_value()
            ? invoke(forward<F>(f), **this)
            : nullopt;
      }

      template <typename F>
      constexpr auto and_then(F&& f) const&& {
        using result =
          invoke_result_t<F, const T&&>;
        static_assert(
          is_optional<result>::value,
          "F must return an optional");

        return has_value()
            ? invoke(forward<F>(f),
                     std::move(**this))
            : nullopt;
      }
      // ...
    };
</td>
<td>
    :::cpp
    template <typename T>
    class optional {
      // ...
      template <typename Self, typename F>
      constexpr auto and_then(F&& f) Self&& {
        using val = decltype((
            forward<Self>(*this).m_value));
        using result = invoke_result_t<F, val>;

        static_assert(
          is_optional<result>::value,
          "F must return an optional");

        return has_value()
            ? invoke(forward<F>(f),
                     forward<Self>(self).m_value)
            : nullopt;
      }
      // ...
    };
</td>
</table>

Keep in mind that there are a few more functions in P0798 that have this lead to this explosion of overloads, so the code difference and clarity is dramatic.

For those that dislike returning auto in these cases, it is very easy to write a metafunction that matches the appropriate qualifiers from a type. Certainly simpler than copying and pasting code and hoping that the minor changes were made correctly in every case.

## CRTP, without the C, R, or even T

Today, a common design pattern is the Curiously Recurring Template Pattern. This implies passing the derived type as a template parameter to a base class template, as a way of achieving static polymorphism. If we wanted to just outsource implementing postfix incrementing to a base, we could use CRTP for that. But with explicit object parameters that deduce to the derived objects already, we don't need any curious recurrence. We can just use standard inheritance and let deduction just do its thing. The base class doesn't even need to be a template:


<table style="width:100%">
<tr>
<th style="width:50%">
C++17
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename Derived>
    struct add_postfix_increment {
        Derived operator++(int) {
            auto& self = static_cast<Derived&>(*this);

            Derived tmp(self);
            ++self;
            return tmp;
        }
    };

    struct some_type : add_postfix_increment<some_type> {
        some_type& operator++() { ... }
    };
</td>
<td>
    :::cpp
    struct add_postfix_increment {
        template <typename Self>
        Self operator++(int) Self {
            Self tmp(self);
            ++self;
            return tmp;
        }
    };



    struct some_type : add_postfix_increment {
        some_type& operator++() { ... }
    };
</td>
</tr>
</table>

The example at right isn't much shorter, but it is certainly simpler.

### Builder pattern

However, once we start to do any more with CRTP, it can get increasingly complex very fast... whereas with this proposal, it stays remarkably simple.

Let's say we have a builder that does a lot of things. We might start with:

    :::cpp
    struct Builder {
      Builder& a() { /* ... */; return *this; }
      Builder& b() { /* ... */; return *this; }
      Builder& c() { /* ... */; return *this; }
    };

    Builder().a().b().a().b().c();    

But now, we want to create a specialized builder that has new operations `d()` and `e()`. This specialized builder needs new member functions, and we don't want to burden existing users with them. But we also want `Special().a().d()` to work - so we need to use CRTP to _conditionally_ return either a `Builder&` or a `Special&`:

<table style="width:100%">
<tr>
<th style="width:50%">
C++ today
</th>
<th>
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename D=void>
    class Builder {
      using Derived = conditional_t<is_void_v<D>, Builder, D>;
      Derived& self() {
        return *static_cast<Derived*>(this);
      }
      
    public:
      Derived& a() { /* ... */; return self(); }
      Derived& b() { /* ... */; return self(); }
      Derived& c() { /* ... */; return self(); }
    };
    
    struct Special : Builder<Special> {
      Special& d() { /* ... */; return *this; }
      Special& e() { /* ... */; return *this; }
    };
    
    Builder().a().b().a().b().c();
    Special().a().d().e().a();
</td>
<td>
    :::cpp
    struct Builder {
        template <typename Self>
        Self& a() Self&& { /* ... */; return self; }
        
        template <typename Self>
        Self& b() Self&& { /* ... */; return self; }        
        
        template <typename Self>
        Self& c() Self&& { /* ... */; return self; }        
    };
    
    struct Special : Builder {
        Special& d() { /* ... */; return *this; }
        Special& e() { /* ... */; return *this; }
    };
    
    Builder().a().b().a().b().c();
    Special().a().d().e().a();
</td>
</tr>
</table>

The code on the right is dramatically easier to understand and more accessible to more programmers than the code on the left.

But what, there's more.

What if we add a _super_-specialized builder, that is a more special form of `Special`? Now we need `Special` to itself opt-in to CRTP so it knows which type to pass to `Builder` so that everything in the hierarchy can return the correct type. It's roughly at this point that people just give up and start gently weeping. But with this proposal, no problem!
    
<table style="width:100%">
<tr>
<th style="width:50%">
C++ today
</th>
<th>
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    template <typename D=void>
    class Builder {
    protected:
      using Derived = conditional_t<is_void_v<D>, Builder, D>;
      Derived& self() {
        return *static_cast<Derived*>(this);
      }
      
    public:
      Derived& a() { /* ... */; return self(); }
      Derived& b() { /* ... */; return self(); }
      Derived& c() { /* ... */; return self(); }
    };
    
    template <typename D=void>
    struct Special
      : Builder<conditional_t<is_void_v<D>,Special<D>,D>
    {
      using Derived = typename Special::Builder::Derived;
      Derived& d() { /* ... */; return this->self(); }
      Derived& e() { /* ... */; return this->self(); }
    };
    
    struct Super : Special<Super>
    {
        Super& f() { /* ... */; return *this; }
    };
    
    Builder().a().b().a().b().c();
    Special().a().d().e().a();
    Super().a().d().f().e();
</td>
<td>
    :::cpp
    struct Builder {
        template <typename Self>
        Self& a() Self&& { /* ... */; return self; }
        
        template <typename Self>
        Self& b() Self&& { /* ... */; return self; }        
        
        template <typename Self>
        Self& c() Self&& { /* ... */; return self; }        
    };
    
    struct Special : Builder {
        template <typename Self>
        Self& d() Self&& { /* ... */; return self; }
        
        template <typename Self>
        Self& e() Self&& { /* ... */; return self; }
    };
    
    struct Super : Special {
        Super& f() Self&& { /* ... */; return *this; }
    };
    
    Builder().a().b().a().b().c();
    Special().a().d().e().a();
    Super().a().d().f().e();
</td>
</tr>
</table>  

That is just so much easier on the right. There are simply so many situations where this idiom, if available, would give programmers an easier, accessible solution to problems that they just cannot easily solve today.

### A new syntax for opt-in

Today, with [P0515](https://wg21.link/p0515), the way that you opt-in to a defaulted comparison is via:

    :::cpp
    struct A {
        int i;
        char c;
        double d;
        
        auto operator<=>(A const&) const = default;
    };
    
This isn't that much to type, and it's a massive improvement over the C++17 implementation of equivalent functionality. But in larger classes, it's easy to lose these kinds of important declarations in the noise. We could use this new non-CRTP version of CRTP to provide a novel annotation for types, which would lead to even less typing for this kind of opt-in while also ensuring that the annotation that is provided is much more visible. 

<table style="width:100%">
<tr>
<th style="width:50%">
C++ today
</th>
<th>
Proposed
</th>
</tr>
<tr>
<td>
    :::cpp
    struct A {
        int i;
        char c;
        double d;
        
        auto operator<=>(A const&) const = default;
    };
</td>
<td>
    :::cpp
    struct Ord {
        template <typename Self>
        auto operator<=>(Self const& rhs) Self const& = default;
    };
    
    struct A
        : Ord // <== first thing we write
    {
        int i;
        char c;
        double d;
        
        // no other declarations
    };
</td>
</tr>
</table>

## Forwarding Lambdas

This proposal allows you to store values in a lambda and then either move or copy them out, depending on the lambda is invoked. We could already do this today for class types:

    :::cpp
    template <typename T>
    struct Holder {
        T value;
        
        T&        operator()() &       { return value; }
        T const&  operator()() const&  { return value; }
        T&&       operator()() &&      { return move(value); }
        T const&& operator()() const&& { return move(value); }
    };
    
But now you can do this with a lambda too:

    :::cpp
    auto holder = [value=...]<typename Self>() Self&& {
        return static_cast<decltype((value))>(value);
    };


## SFINAE-friendly callables

A seemingly unrelated problem to the question of code quadruplication is that of writing these numerous overloads for function wrappers, as demonstrated in [P0826](https://wg21.link/p0826). Consider what happens if we implement `std::not_fn()`, as currently specified:

    :::cpp
    template <typename F>
    class call_wrapper {
        F f;
    public:
        // ...
        template <typename... Args>
        auto operator()(Args&&... ) &
            -> decltype(!declval<invoke_result_t<F&, Args...>>());

        template <typename... Args>
        auto operator()(Args&&... ) const&
            -> decltype(!declval<invoke_result_t<const F&, Args...>>());

        // ... same for && and const && ...
    };

    template <typename F>
    auto not_fn(F&& f) {
        return call_wrapper<std::decay_t<F>>{std::forward<F>(f)};
    }

As described in the paper, this implementation has two pathological cases: one in which the callable is SFINAE-unfriendly (which would cause a call to be ill-formed, when it could otherwise work), and one in which overload is deleted (which would cause a call to fall-back to a different overload, when it should fail):

    :::cpp
    struct unfriendly {
        template <typename T>
        auto operator()(T v) {
            static_assert(std::is_same_v<T, int>);
            return v;
        }

        template <typename T>
        auto operator()(T v) const {
            static_assert(std::is_same_v<T, double>);
            return v;
        }
    };

    struct fun {
        template <typename... Args>
        void operator()(Args&&...) = delete;

        template <typename... Args>
        bool operator()(Args&&...) const { return true; }
    };

    std::not_fn(unfriendly{})(1); // static assert!
                                  // even though the non-const overload is viable and would be the best
                                  // match, during overload resolution, both overloads of unfriendly have
                                  // to be instantiated - and the second one is a hard compile error.

    std::not_fn(fun{})();         // ok!? Returns false
                                  // even though we want the non-const overload to be deleted, the const
                                  // overload of the call_wrapper ends up being viable - and the only viable
                                  // candidate.

Gracefully handling SFINAE-unfriendly callables is **not solvable** in C++ today. Preventing fallback can be solved by the addition of yet another four overloads, so that each of the four *cv*/ref-qualifiers leads to a pair of overloads: one enabled and one `deleted`.

This proposal solves both problems by simply allowing `this` to be deduced. The following is a complete implementation of `std::not_fn`:

    :::cpp
    template <typename F>
    struct call_wrapper {
        F f;

        template <typename Self, typename... Args>
        auto operator()(Args&&... args) Self&&
            -> decltype(!invoke(forward_like<Self>(this->f), forward<Args>(args)...))
        {
            return !invoke(forward_like<Self>(this->f), forward<Args>(args)...);
        }
    };

    template <typename F>
    auto not_fn(F&& f) {
        return call_wrapper<decay_t<F>>{forward<F>(f)};
    }

    not_fn(unfriendly{})(1); // ok
    not_fn(fun{})();         // error

Here, there is only one overload with everything deduced together. The first example now works correctly. `Self` gets deduced as `call_wrapper<unfriendly>`, and the one `operator()` will only consider `unfriendly`'s non-`const` call operator. The `const` one is simply never considered, so does not have an opportunity to cause problems. The call works. 

The second example now fails correctly. Previously, we had four candidates: the two non-`const` ones were removed from the overload set due to `fun`'s non-`const` call operator being `delete`d, and the two `const` ones which were viable. But now, we only have one candidate. `Self` gets deduced as `call_wrapper<fun>`, which requires `fun`'s non-`const` call operator to be well-formed. Since it is not, the call is an error. There is no opportunity for fallback since there is only one overload ever considered. 

As a result, this singular overload then has precisely the desired behavior: working, for `unfriendly`, and not working, for `fun`.

Note that this could also be implemented as a lambda completely within the body of `not_fn`:

    :::cpp
    template <typename F>
    auto not_fn(F&& f) {
        return [f=forward<F>(f)]<typename Self, typename... Args>(auto&&.. args) Self&&
            -> decltype(!invoke(forward_like<Self>(f), forward<Args>(args)...))
        {
            return !invoke(forward_like<Self>(f), forward<Args>(args)...);
        };
    }
    

# Acknowledgements

The authors would like to thank:

- Jonathan Wakely, for bringing us all together by pointing out we were writing the same paper, twice
- Chandler Carruth for a lot of feedback and guidance around design issues
- Graham Heynes, Andrew Bennieston, Jeff Snyder for early feedback regarding the meaning of `this` inside function bodies
- Amy Worthington, Jackie Chen, Vittorio Romeo, Tristan Brindle, Agustín Bergé, Louis Dionne, and Michael Park for early feedback
- Guilherme Hartmann for his guidance with the implementation

[rap.p0847r0]: "RAP Wiki notes, P0847R0 - June 2018"
[meyers.effective]: https://www.aristeia.com/books.html "Effective C++, Third Edition||Scott Meyers||2005"
[brand.optional]: https://github.com/TartanLlama/optional "Simon Brand's implementation of optional<T>"
