Title: Conditionally Trivial Special Member Functions
Document-Number: D0848R1
Authors: Barry Revzin, barry dot revzin at gmail dot com
Authors: Casey Carter, casey at carter dot net
Audience: EWG

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

TBD
