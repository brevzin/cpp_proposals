Title: Template variable template parameters
Authors: Barry Revzin &lt;barry.revzin@gmail.com&gt;
Date: 2018-05-03
Audience: EWG
Document-Number: D1065R0

# Introduction

C++ currently has three different _kinds_ of template parameters:

* type parameters

        :::c++
        template <typename T> struct A { };
        template <class T> struct B { };

* non-type (value) parameters

        :::c++
        template <int I> struct C { };
        template <auto V> struct D { }; // C++17
        template <Literal X> struct E { }; // C++20

* template parameters

        :::c++
        template <template <typename> class Z> struct F { };

There has been quite a bit of growth in the last few standards of just what kinds of values and types are allowed to be used in non-type parameters, as a result of [allowing constant evaluation for all non-type template arguments](https://wg21.link/n4198), [template auto](https://wg21.link/p0127), and [class types in non-type template parameters](https://wg21.link/p0732). But the actual _kinds_ of allowable parameters has stayed fixed. 

However, there have been recent developments in other parts of the language that suggest that this list is incomplete. Before C++14, we only had three kinds of things: types, values, and templates. Now, we have two more:

* In C++14, [Variable templates](https://wg21.link/n3651) and their many uses in the [standard library](https://wg21.link/n3932)

        :::c++
        template <typename T>
        inline constexpr bool is_object_v = std::is_object<T>::value;


* In C++20, [Concepts](https://wg21.link/p0734) and their many uses in the [Ranges TS](https://wg21.link/p0651)

        :::c++
        template <typename T>
        concept EqualityComparable = requires (T a, T b) {
            { a == b } -> bool;
        };

Neither of these can currently be used as template arguments. That is, we can use `is_object_v<int>` as a non-type template argument into a template parameter that can accept a `bool`... but we cannot use `is_object_v` itself. There is no kind of template parameter that currently accepts variable templates. Nor is there a kind of template parameter that can accept a concept like `EqualityComprable` directly.

# Proposal

This paper proposes the introduction of two new kinds of template parameters, that can be used correspondingly by their respective kinds of template arguments: _template variable template parameters_ and _template concept parameters_.

## Template variable template parameters

A _template variable template parameter_ is a template parameter that can accept a variable template. That is:

    :::c++
    template <template <typename > bool UnaryTypeTrait>
    struct Printer {
        template <typename T>
        void maybe_print(T value) {
            if constexpr (UnaryTypeTrait<T>) {
                std::cout << value;
            }
        }
    };
    
    Printer<std::is_arithmetic_v> p;
    p.maybe_print(4);        // prints 4
    p.maybe_print("hello"s); // doesn't print anything

`Printer` is a class template that has a single template parameter that accepts a `bool` variable template that consists of a single template parameter. The only way to achieve such a construction today is to wrap the variable template in a class template that has a `value` static data member that needs to be accessed. As variable templates are becoming more common, this is an undesirable indirection.

The allowable types for the variable template parameter would follow the usual rules for other non-type template parameters. They could refer to other template parameters and they could be `auto`:

    :::c++
    template <typename T, template <int > T Constant>
    struct X { };
    
    template <template <int > auto Constant>
    struct Y { };
    
    template <int I>
    inline constexpr std::integral_constant<int, I> int_{};
    
    X<int, int_> x; // ok, T is int and Constant is a variable int template 
    Y<int_> y;      // ok, Constant is a variable template that accepts an int
    
    
## Template concept parameters

Concepts are very similar to `inline constexpr bool`  variable templates, with a few notable exceptions (they can't be specialized, always evaluate as prvalues, and get special abilities with regards to subsumption and partial ordering). But they really are variable templates, so they should be used as template parameters as well.

Without the ability to pass concepts as template parameters, we have no direct way of creating a type erasure library built on concepts:

    :::c++
    namespace std {
        template <template <typename> concept ...Concepts>
        class basic_any { ... };
    
        template <typename Signature>
        using function = basic_any<Invocable<Signature>, CopyConstructible>;
    }
    
While it isn't clear at this point what we could put into the `...` section above, it certainly requires the ability to declare `Concepts` as a template parameter pack of template concept parameters. 

# Acknowledgements

Thanks to [Eric Niebler](https://twitter.com/ericniebler/status/992179279550017537) for asking the question that led to this paper.