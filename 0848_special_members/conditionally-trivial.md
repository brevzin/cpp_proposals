Title: Conditionally Trivial Special Member Functions
Document-Number: D0848R1
Authors: Barry Revzin, barry dot revzin at gmail dot com
Authors: Casey Carter, casey at carter dot net
Audience: CWG

# Introduction and Revision History

For a complete motivation for this proposal, see [P0848R0](https://wg21.link/p0848). In brief, it is important for certain class template instantiations to propagate the triviality from their template parameters - that is, we want `wrapper<T>` to be trivially copyable if and only if `T` is copyable. In C++17, this is possible, yet is extremely verbose. The introduction of Concepts provides a path to make this propagation substantially easier to write, but the current definition of trivially copyable doesn't quite suffice for what we want.

Consider:

    :::cpp
    template <typename T>
    concept C = /* ... */;

    template <typename T>
    struct X {
        // #1
        X(X const&) requires C<T> = default;
        
        // #2
        X(X const& ) { /* ... */ }
    };

According to the current working draft, both `#1` and `#2` are copy constructors. The current definition for trivially copyable requires that _each_ copy constructor be either deleted or trivial. That is, we always consider both copy constructors, regardless of `T` and `C<T>`, and hence no instantation of `X` is ever trivially copyable. 

R0 of this paper, as presented in San Diego in November 2018, proposed to change the rules for trivially copyable such that we only consider the _best viable candidate_ amongst the copy constructors given a synthesized overload resolution. That is, depending on `C<T>`, we either consider only `#2` (because `#1` wouldn't be viable) or only `#1` (because it would be more constrained than `#2` and hence a better match). However, EWG considered this to be confusing as trivially copyable is a property of a type and adding overload resolution simply adds more questions about context (e.g. do we consider accessibility?). EWG requested a new mechanism to solve this problem.

# Proposal

The following arguments and definitions are focused specifically on the copy constructor, but also apply similarly to the default constructor, the move constructor, the copy and move assignment operators, and the destructor.

## Special member function candidates

In the current working draft, the definition of copy constructor is, from \[class.copy.ctor\]:

> A non-template constructor for class `X` is a copy constructor if its first parameter is of type `X&`, `const X&`,
`volatile X&` or `const volatile X&`, and either there are no other parameters or else all other parameters
have default arguments (9.2.3.6).

By this definition, both `#1` and `#2` in the previous example are copy constructors - regardless of `C<T>`. Instead, we could say that both of these functions are simply _candidates_ to be copy constructors. For a given *cv*-qualification, a class can have multiple copy constructor candidates - but only have one copy constructor: the most constrained candidate of the candidates whose constraints are satisfied. If there is no most constrained candidate (that is, either there is no candidate or there is an ambiguity), then there is no copy constructor.

With this approach, `#1` and `#2` are both copy constructor candidates for the signature `X(X const&)`. For a given instantiation, if `C<T>` is not satisfied, there is only one candidate whose constraints are met and hence `#2` is _the_ copy constructor. If `C<T>` is satisfied, then `#1` is the most constrained candidate and hence it is _the_ copy constructor. 

Using the example from R0:

    :::cpp
    template <typename T>
    struct optional {
        // #1
        optional(optional const&)
            requires TriviallyCopyConstructible<T> && CopyConstructible<T>
            = default;
            
        // #2
        optional(optional const& rhs)
                requires CopyConstructible<T>
           : engaged(rhs.engaged)
        {
            if (engaged) {
                new (value) T(rhs.value);
            }
        }
    };
    
We have two copy constructor candidates: `#1` and `#2`. For `T=unique_ptr<int>`, neither candidate has its constraints satisfied, so there is no copy constructor. For `T=std::string`, only `#2` has its constraints satisfied, so it is the copy constructor. For `T=int`, both candidates have their constraints satisfied and `#1` is more constrained than `#2`, so `#1` is the copy constructor.

With the introduction of the notion of copy constructor candidates, and reducing the meaning of "copy constructor" to be the most constrained candidate, no change is necessary to the definition of trivially copyable; requiring that each copy constructor be trivial or deleted becomes the correct definition - and meets the requirements of making it easy to propagate triviality.

## Standard Impact

One important question is: now that we have two terms, copy constructor candidate and copy constructor, what do we have to change throughout the standard? In the latest working draft, [N4778](https://wg21.link/n4778), there are 79 uses of the term _copy constructor_. 27 of those are in the core language (several of which are in notes or example), and 52 in the library.

Whenever the library uses "copy constructor," it really means this new refined definition of _the_ copy constructor. Indeed, it's even more specific than that as the library only cares about the most constrained copy constructor candidate which takes an lvalue reference to `const`. Hence, no library wording needs to change all.

Of the 27 core language uses:

- 2 are seemingly unnecessary: \[class.temporary\]/3 and \[expr.call\]/12 contain a complete redefinition of what it means for a type to be trivially copyable.
- 2 refer to the new, proposed notion of copy constructor as the most constrained candidate
- 12 refer to what this paper proposes to call a copy constructor candidate, mostly in \[class.copy.ctor\] but also when describing the copyability of lambdas.
- 11 are non-normative, appearing in comments in examples or notes

In other words, introducing this notion of a copy constructor candidate and the copy constructor would primarily require simply changing \[class.copy.ctor\], which in the working draft describes the rules for what a copy constructor is and when it would be generated. Just about everything outside of that section would not only not need to change, but arguably be silently improved - we typically only care about the most constrained copy constructor candidate and now the wording would actually say that.

# Wording

Relative to N4791. Due to potential confusion with overload resolution, we are using the term "prospective" rather than the term "candidate" throughout.

## Default constructor

Change 10.3.4.1 [class.default.ctor], paragraph 1:

> A _<ins>prospective</ins> default constructor_ for a class `X` is a constructor of class `X` for which each parameter that is not a function parameter pack has a default argument (including the case of a constructor with no parameters). If there is no user-declared constructor for class `X`, a non-explicit constructor having no parameters is implicitly declared as defaulted (9.4). An implicitly-declared <ins>prospective</ins> default constructor is an inline public member of its class. 

> <ins>A prospective default constructor is a _default constructor_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective default constructor whose constraints are satisfied (if any).</ins>

Change 10.3.4.1 [class.default.ctor], paragraph 2:

> A defaulted <ins>prospective</ins> default constructor for class `X` is defined as deleted if:

## Copy and move constructor

Introduce the concept of prospective copy constructor in 10.3.4.2 [class.copy.ctor], paragraph 1:

> A non-template constructor for class `X` is a <ins>prospective</ins> copy constructor if its first parameter is of type `X&`, `const X&`, `volatile X&` or `const volatile X&`, and either there are no other parameters or else all other parameters have default arguments (9.2.3.6). 

> <ins>A prospective copy constructor is a _copy constructor_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective copy constructor that has the same first parameter type and whose constraints are satisfied (if any).</ins>
> 
> <ins> [ *Note*: a class can have multiple copy constructors, provided they have different signatures *-end note*] </ins>

Introduce the concept of prospective move constructor in 10.3.4.2 [class.copy.ctor], paragraph 2:

> A non-template constructor for class X is a <ins>prospective</ins> move constructor if its first parameter is of type `X&&`, `const X&&`, `volatile X&&`, or `const volatile X&&`, and either there are no other parameters or else all other
parameters have default arguments (9.2.3.6).

> <ins>A prospective move constructor is a _move constructor_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective move constructor
that has the same first parameter type and whose constraints are satisfied (if any).</ins>
> 
> <ins> [ *Note*: a class can have multiple move constructors, provided they have different signatures *-end note*] </ins>

Reduce the cases where we implicitly generate special members in 10.3.4.2 [class.copy.ctor], paragraph 6:

> If the class definition does not explicitly declare a <ins>prospective</ins> copy constructor, a non-explicit one is declared implicitly. If the class definition declares a <ins>prospective</ins> move constructor or <ins>prospective</ins> move assignment operator, the implicitly declared <ins>prospective</ins> copy constructor is defined as deleted; otherwise, it is defined as defaulted (9.4). The latter case is deprecated if the class has a user-declared <ins>prospective</ins> copy assignment operator or a user-declared <ins>prospective</ins> destructor (D.5). <ins>[ *Note:* An implicitly declared prospective copy constructor is a copy constructor. *-end note* ]</ins>

Change 10.3.4.2 [class.copy.ctor], paragraph 7:

> The implicitly-declared <ins>prospective</ins> copy constructor for a class `X` will have the form
> 
    :::cpp
    X::X(const X&)
> if each potentially constructed subobject of a class type M (or array thereof) has a copy constructor whose first
parameter is of type `const M&` or `const volatile M&`. Otherwise, the implicitly-declared <ins>prospective</ins> copy constructor will have the form
> 
    :::cpp
    X::X(X&)

Change 10.3.4.2 [class.copy.ctor], paragraph 8:

> If the definition of a class `X` does not explicitly declare a <ins>prospective</ins> move constructor, a non-explicit one will be implicitly declared as defaulted if and only if  
> 
— X does not have a user-declared <ins>prospective</ins> copy constructor,  
— X does not have a user-declared <ins>prospective</ins> copy assignment operator,  
— X does not have a user-declared <ins>prospective</ins> move assignment operator, and  
— X does not have a user-declared <ins>prospective</ins> destructor.
> [*Note:* <ins>An implicitly declared propsective move constructor is a move constructor.</ins> When <del>the</del> <ins>a</ins> <ins>prospective</ins> move constructor is not implicitly declared or explicitly supplied, expressions that otherwise would have invoked the move constructor may instead invoke a copy constructor. *—end note*]

Change 10.3.4.2 [class.copy.ctor], paragraph 9:

> The implicitly-declared <ins>prospective</ins> move constructor for class `X` will have the form
> 
    :::cpp
    X::X(X&&)
    
Change 10.3.4.2 [class.copy.ctor], paragraph 10:
    
> An implicitly-declared <ins>prospective</ins> copy/move constructor is an inline public member of its class. A defaulted <ins>prospective</ins> copy/move constructor for a class `X` is defined as deleted (9.4.3) if X has:
> 
> - [...]
> - for the <ins>prospective</ins> copy constructor, a non-static data member of rvalue reference type.  
> 
> A defaulted <ins>prospective</ins> move constructor that is defined as deleted is ignored by overload resolution (11.3, 11.4).

Change 10.3.4.2 [class.copy.ctor], paragraph 11:

> A <ins>prospective</ins> copy/move constructor for class X is _trivial_ if it is not user-provided and if:
> 
> - [...]  
> - [...]  
> - [...]  
> 
> otherwise the <ins>prospective</ins> copy/move constructor is _non-trivial_.

Change 10.3.4.2 [class.copy.ctor], paragraph 12:

> A <ins>prospective</ins> copy/move constructor that is defaulted and not defined as deleted is implicitly defined when it is odr-used (6.2), when it is needed for constant evaluation (7.7), or when it is explicitly defaulted after its first
declaration.

Change 10.3.4.2 [class.copy.ctor], paragraph 13:

> Before <del>the</del> <ins>a</ins> defaulted <ins>prospective</ins> copy/move constructor for a class is implicitly defined, all non-user-provided copy/move constructors for its potentially constructed subobjects shall have been implicitly defined.

Change 10.3.4.2 [class.copy.ctor], paragraph 14:

> The implicitly-defined <ins>prospective</ins> copy/move constructor for a non-union class X performs a memberwise copy/move of its bases and members. [Note: Default member initializers of non-static data members are ignored. See also
the example in 10.9.2. —end note] The order of initialization is the same as the order of initialization of
bases and members in a user-defined constructor (see 10.9.2). Let `x` be either the parameter of the <ins>prospective copy</ins> constructor or, for the <ins>prospective</ins> move constructor, an xvalue referring to the parameter. Each base or non-static data member is copied/moved in the manner appropriate to its type:
> 
> - if the member is an array, each element is direct-initialized with the corresponding subobject of `x`;  
> - if a member `m` has rvalue reference type `T&&`, it is direct-initialized with `static_cast<T&&>(x.m)`;  
> - otherwise, the base or member is direct-initialized with the corresponding base or member of `x`.  
> 
> Virtual base class subobjects shall be initialized only once by the implicitly-defined <ins>prospective</ins> copy/move constructor (see 10.9.2).

Change 10.3.4.2 [class.copy.ctor], paragraph 15: 

> The implicitly-defined <ins>prospective</ins> copy/move constructor for a union X copies the object representation (6.7) of X.

## Copy and move assignment

Change 10.3.5 [class.copy.assign], paragraph 1:

> A user-declared <ins>prospective</ins> copy assignment operator `X::operator=` is a non-static non-template member function of class `X` with exactly one parameter of type `X`, `X&`, `const X&`, `volatile X&`, or `const volatile X&`.

> <ins>A prospective copy assignment operator is a _copy assignment operator_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective copy assignment operator
that has the same parameter type and whose constraints are satisfied (if any).</ins> 

[*Note:* An overloaded assignment operator must be declared to have only one parameter; see 11.5.3. *—end note]* [*Note:* More than one form of <ins>prospective</ins> copy assignment operator may be declared for a class. *—end note*] [*Note:* If a class `X` only has a <ins>prospective</ins> copy assignment operator with a parameter of type `X&`, an expression of type `const X` cannot be assigned to an object of type `X`. [*Example:*

Change 10.3.5 [class.copy.assign], paragraph 2:

> If the class definition does not explicitly declare a <ins>prospective</ins> copy assignment operator, one is declared implicitly. If the class definition declares a <ins>prospective</ins> move constructor or <ins>prospective</ins> move assignment operator, the implicitly declared <ins>prospective</ins> copy assignment operator is defined as deleted; otherwise, it is defined as defaulted (9.4). The latter case is deprecated if the class has a user-declared <ins>prospective</ins> copy constructor or a user-declared <ins>prospective</ins> destructor (D.5). The implicitly-declared <ins>prospective</ins> copy assignment operator for a class `X` will have the form
> 
    :::cpp
    X& X::operator=(const X&)
> if
> 
> - each direct base class `B` of `X` has a copy assignment operator whose parameter is of type `const B&`,
`const volatile B&`, or `B`, and  
> - for all the non-static data members of `X` that are of a class type `M` (or array thereof), each such class
type has a copy assignment operator whose parameter is of type `const M&`, `const volatile M&`, or
`M`. 
> 
> Otherwise, the implicitly-declared <ins>prospective</ins> copy assignment operator will have the form
> 
    :::cpp
    X& X::operator=(X&)
    
Change 10.3.5 [class.copy.assign], paragraph 3:

> A user-declared <ins>prospective</ins> move assignment operator `X::operator=` is a non-static non-template member function of class `X` with exactly one parameter of type `X&&`, `const X&&`, `volatile X&&`, or `const volatile X&&`.

> <ins>A prospective move assignment operator is a _move assignment operator_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective move assignment operator
that has the same parameter type and whose constraints are satisfied (if any).</ins> 

> [*Note:* An overloaded assignment operator must be declared to have only one parameter; see 11.5.3. *—end note*] [*Note:* More than one form of <ins>prospective</ins> move assignment operator may be declared for a class. *—end note*]

Change 10.3.5 [class.copy.assign], paragraph 4:

> If the definition of a class X does not explicitly declare a <ins>prospective</ins> move assignment operator, one will be implicitly declared as defaulted if and only if
> 
> - X does not have a user-declared <ins>prospective</ins> copy constructor,  
> - X does not have a user-declared <ins>prospective</ins> move constructor,  
> - X does not have a user-declared <ins>prospective</ins> copy assignment operator, and  
> - X does not have a user-declared <ins>prospective</ins> destructor.
> 
> [*Example*: The class definition
> 
    :::cpp
    struct S {
      int a;
      S& operator=(const S&) = default;
    };
> will not have a default <ins>prospective</ins> move assignment operator implicitly declared because <del>the</del> <ins>a</ins> <ins>prospective</ins> copy assignment operator has been user-declared. <del>The</del> <ins>A</ins> <ins>prospective</ins> move assignment operator may be explicitly defaulted.
> 
    :::cpp
    struct S {
      int a;
      S& operator=(const S&) = default;
      S& operator=(S&&) = default;
    };
> —*end example*]

Change 10.3.5 [class.copy.assign], paragraph 5: 
> The implicitly-declared <ins>prospective</ins> move assignment operator for a class X will have the form
> 
    :::cpp
    X& X::operator=(X&&);

Change 10.3.5 [class.copy.assign], paragraph 6:
> The implicitly-declared <ins>prospective</ins> copy/move assignment operator for class X has the return type `X&`; it returns the object for which the assignment operator is invoked, that is, the object assigned to. An implicitly-declared <ins>prospective</ins> copy/move assignment operator is an inline public member of its class.

Change 10.3.5 [class.copy.assign], paragraph 7:
> A defaulted <ins>prospective</ins> copy/move assignment operator for class X is defined as deleted if X has:
> 
> - a variant member with a non-trivial corresponding assignment operator and X is a union-like class, or  
> - a non-static data member of const non-class type (or array thereof), or  
> - a non-static data member of reference type, or  
> - a direct non-static data member of class type M (or array thereof) or a direct base class M that cannot be copied/moved because overload resolution ([over.match]), as applied to find M's corresponding assignment operator, results in an ambiguity or a function that is deleted or inaccessible from the defaulted assignment operator.
> 
> A defaulted <ins>prospective</ins> move assignment operator that is defined as deleted is ignored by overload resolution ([over.match], [over.over]).

Change 10.3.5 [class.copy.assign], paragraph 8:
> Because a <ins>prospective</ins> copy/move assignment operator is implicitly declared for a class if not declared by the user, a base class copy/move assignment operator is always hidden by the corresponding <ins>prospective</ins> assignment operator of a derived class ([over.ass]). A using-declaration ([namespace.udecl]) that brings in from a base class an assignment operator with a parameter type that could be that of a <ins>prospective</ins> copy/move assignment operator for the derived class is not considered an explicit declaration of such a <ins>prospective</ins> operator and does not suppress the implicit declaration of the derived class <ins>prospective</ins> operator; the operator introduced by the using-declaration is hidden by the implicitly-declared <ins>prospective</ins> operator in the derived class.

Change 10.3.5 [class.copy.assign], paragraph 9:
> A <ins>prospective</ins> copy/move assignment operator for class X is trivial if it is not user-provided and if:
> 
> - class X has no virtual functions ([class.virtual]) and no virtual base classes ([class.mi]), and  
> - the assignment operator selected to copy/move each direct base class subobject is trivial, and  
> - for each non-static data member of X that is of class type (or array thereof), the assignment operator selected to copy/move that member is trivial;  
> 
> otherwise the <ins>prospective</ins> copy/move assignment operator is non-trivial.

Change 10.3.5 [class.copy.assign], paragraph 10:
> A <ins>prospective</ins> copy/move assignment operator for a class X that is defaulted and not defined as deleted is implicitly defined when it is odr-used ([basic.def.odr]) (e.g., when it is selected by overload resolution to assign to an object of its class type), when it is needed for constant evaluation ([expr.const]), or when it is explicitly defaulted after its first declaration. The implicitly-defined <ins>prospective</ins> copy/move assignment operator is constexpr if
> 
> - X is a literal type, and
> - the assignment operator selected to copy/move each direct base class subobject is a constexpr function, and
> - for each non-static data member of X that is of class type (or array thereof), the assignment operator selected to copy/move that member is a constexpr function.

Change 10.3.5 [class.copy.assign], paragraph 11:
> Before the defaulted <ins>prospective</ins> copy/move assignment operator for a class is implicitly defined, all non-user-provided copy/move assignment operators for its direct base classes and its non-static data members shall have been implicitly defined. [*Note: An implicitly-declared <ins>prospective</ins> copy/move assignment operator has an implied exception specification ([except.spec]). —*end note*]

Change 10.3.5 [class.copy.assign], paragraph 12: 
> The implicitly-defined <ins>prospective</ins> copy/move assignment operator for a non-union class X performs memberwise copy/move assignment of its subobjects. The direct base classes of X are assigned first, in the order of their declaration in the base-specifier-list, and then the immediate non-static data members of X are assigned, in the order in which they were declared in the class definition. Let x be either the parameter of the function or, for the move operator, an xvalue referring to the parameter. Each subobject is assigned in the manner appropriate to its type:
> 
> - [...]  
> 
> It is unspecified whether subobjects representing virtual base classes are assigned more than once by the implicitly-defined <ins>prospective</ins> copy/move assignment operator.

Change 10.3.5 [class.copy.assign], paragraph 13:
> The implicitly-defined <ins>prospective</ins> copy assignment operator for a union X copies the object representation ([basic.types]) of X.

## Destructor

Change 10.3.6 [class.dtor], paragraph 1:

> In a declaration of a <ins>prospective</ins> destructor, the declarator is a function declarator (9.2.3.5) of the form [...] A <ins>prospective</ins> destructor shall take no arguments (9.2.3.5). Each *decl-specifier* of the *decl-specifier-seq* of a <ins>prospective</ins> destructor declaration (if any) shall be `friend`, `inline`, or `virtual`.

> <ins>A prospective destructor is a _destructor_ if</ins>
> 
> - <ins>all of its constraints (if any) are satisfied, and</ins>
> - <ins>it is at least as constrained as ([temp.constr.order]) every other prospective destructor whose constraints are satisfied (if any).</ins> 

Change 10.3.6 [class.dtor], paragraph 4:

> If a class has no user-declared <ins>prospective</ins> destructor, a <ins>prospective</ins> destructor is implicitly declared as defaulted (9.4). An implicitly-declared <ins>prospective</ins> destructor is an inline public member of its class.

Change 10.3.6 [class.dtor], paragraph 5:

> A defaulted <ins>prospective</ins> destructor for a class X is defined as deleted if: [...]

Change 10.3.6 [class.dtor], paragraph 6:

> A <ins>prospective</ins> destructor is trivial if it is not user-provided and if:  
> 
> - the <ins>prospective</ins> destructor is not virtual,  
> - all of the direct base classes of its class have trivial destructors, and
> - for all of the non-static data members of its class that are of class type (or array thereof), each such
class has a trivial destructor.
> 
> Otherwise, the <ins>prospective</ins> destructor is non-trivial.

Change 10.3.6 [class.dtor], paragraph 7:

> A <ins>prospective</ins> destructor that is defaulted and not defined as deleted is implicitly defined when it is odr-used (6.2) or when it is explicitly defaulted after its first declaration.

Change 10.3.6 [class.dtor], paragraph 8:

> Before the defaulted <ins>prospective</ins> destructor for a class is implicitly defined, all the non-user-provided destructors for its base classes and its non-static data members shall have been implicitly defined.

Change 10.3.6 [class.dtor], paragraph 10:

> A <ins>prospective</ins> destructor can be declared `virtual` (10.6.2) or pure `virtual` (10.6.3); if <ins>the destructor of a class is `virtual` and</ins> any objects of that class or any derived class are created in the program, the destructor shall be defined. If a class has a base class with a virtual destructor, its destructor (whether user- or implicitly-declared) is virtual.

Add a note to 10.3.6 [class.dtor], paragraph 12:

> A program is ill-formed if a destructor that is potentially invoked is deleted or not accessible from
the context of the invocation. <ins>[ *Note*: this can occur if a class has multiple prospective destructors and there is no most constrained destructor candidate, so there is no destructor *-end note* ]</ins>

Change 10.3.6 [class.dtor], paragraph 13:

> At the point of definition of a virtual <ins>prospective</ins> destructor (including an implicit definition (10.3.6)), the non-array deallocation function is determined as if for the expression `delete this` appearing in a non-virtual destructor of the destructor’s class (see 7.6.2.5). If the lookup fails or if the deallocation function has a deleted
definition (9.4), the program is ill-formed.

# Acknowledgments

Thanks to Gaby dos Reis, Daveed Vandevoorde, and Jonathan Wakely for helping bring us to this design. Thanks to Jens Maurer for the wording wizardry. 