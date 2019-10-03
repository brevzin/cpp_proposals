---
title: _`forwarding-range`_`<T>` is too subtle
document: D1870R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

One of the concepts introduces by Ranges is _`forwarding-range`_. The salient aspect of what makes a _`forwarding-range`_ is stated in [\[range.range\]](http://eel.is/c++draft/range.range):

> the validity of iterators obtained from the object denoted by `E` is not tied to the lifetime of that object.

clarified more in the subsequent note:

> *[ Note*: Since the validity of iterators is not tied to the lifetime of an object whose type models _`forwarding-range`_, a function can accept arguments of such a type by value and return iterators obtained from it without danger of dangling.
*— end note ]*

For example, `std::vector<T>` is not a _`forwarding-range`_ because any iterator into a `vector` is of course dependent on the lifetime of the `vector` itself. On the other hand, `std::string_view` _is_ a _`forwarding-range`_ because it does not actually own anything - any iterator you get out of it has its lifetime tied to some other object entirely.

But while `span` and `subrange` each model _`forwarding-range`_, not all views do. For instance, `transform_view` would not because its iterators' validity would be tied to the unary function that is the actual transform. You could increment those iterators, but you couldn't dereference them. Likewise, `filter_view`'s iterator validity is going to be based on its predicate.

Really, a _`forwarding-range`_ is quite a rare creature [^1].

Value category also plays into this. Notably, _lvalue_ ranges all model _`forwarding-range`_ -- the "object" in question in this case is an lvalue reference, and the validity of iterators into a range are never going to be tied to the lifetime of some reference to that range. For instance, `std::vector<T>` is not a _`forwarding-range`_, but `std::vector<T>&` is. The only question is about _rvalue_ ranges. If I have a function that either takes a range by forwarding reference or by value, I have to know what I can do with it.

Ranges uses this in two kinds of places:

- Many algorithms return iterators into a range. Those algorithms conditionally return either `iterator_t<R>` or `dangling` based on whether or not `R` satisfies _`forwarding-range`_ (because if `R` did not, then such iterators would not be valid, so they are not returned). This type is called `safe_iterator_t<R>` and appears over 100 times in [\[algorithms\]](http://eel.is/c++draft/algorithms).
- Range adapters can only be used on rvalue ranges if they satisfy either _`forwarding-range`_ or they decay to a `view`. The former because you may need to keep iterators into them past their lifetime, and the latter because if you can cheaply copy it than that works too. This higher-level concept is called `viewable_range`, and every range adapter depends on it.

That is, _`forwarding-range`_ is a very important concept. It is used practically everywhere. It also conveys a pretty subtle and very rare feature of a type: that its iterators can outlive it. Syntactically, there is no difference between a `range`, a `view`, and a _`forwarding-range`_, so the question is - how does a type declare itself to have this feature?

# Naming

The name _`forwarding-range`_ is problematic. There is a concept `std::forward_range` which is completely unrelated. A fairly common first response is that it has something to do with forwarding iterators. But the name actually comes from the question of whether you can use a forwarding reference `range` safely.

However, coming up with a good name for it is very difficult. The concept has to refer to the range, but the salient aspect really has more to do with the iterators. Words that seem relevant are detachable, untethered, unfettered, nondangling. But then applying them to the range ends up being a mouthful: `range_with_detachable_iterators`. Granted, this concept isn't _directly_ used in too many places so maybe a long name is fine.

The naming direction this proposal takes is to use the name `safe_range`, based on the existence of `safe_iterator` and `safe_subrange`. It still doesn't seem like a great name though, but at least all the relevant library things are similarly named. 

Also the concept is still exposition-only, despite being a fairly important concept that people may want to use in their own code. This can be worked around:

```cpp
template<class R>
concept my_forwarding_range = std::range<R>
    && std::same_as<std::safe_iterator_t<R>, std::iterator_t<R>>;
```

But this seems like the kind of thing the standard library should provide directly.


# Opting into _`forwarding-range`_

Types must opt into _`forwarding-range`_, and this is done by having non-member `begin()` and `end()` overloads that must take the type by either value or rvalue reference. At first glance, it might seem like this is impossible to do in the language but Ranges accomplishes this through the clever[^2] use of poison-pill overload:

```cpp
namespace __begin {
    // poison pill
    template <typename T> void begin(T&&) = delete;

    template <typename R>
    concept has_non_member = requires (R&& r) {
            begin(std::forward<R>(r));
        };
}

namespace N {
    struct my_vector { /* ... */ };
    auto begin(my_vector const&);
}
```

Does `N::my_vector` satisfy the concept `__begin::has_non_member`? It does not. The reason is that the poison pill candidate binds an rvalue reference to the argument while the ADL candidate binds an lvalue reference, and tiebreaker of rvalue reference to lvalue reference happens much earlier than non-template to template. The only way to have a better match than the poison pill for rvalues is to either have a function/function template that takes its argument by value or rvalue reference, or to have a function template that takes a constrained forwarding reference. 

This is a pretty subtle design decision - why did we decide to use the existence of non-member overloads as the opt-in?

## History

This design comes from [@stl2.547], with the expressed intent:

> Redesign begin/end CPOs to eliminate deprecated behavior and to force range types to opt in to working with rvalues, thereby giving users a way to detect that, for a particular range type, iterator validity does not depend on the range's lifetime.

which led to [@P0970R1], which describes the earlier problems with `ranges::begin()` thusly:

::: bq
[1]{.pnum} For the sake of compatibility with `std::begin` and ease of migration, `std::ranges::begin` accepted rvalues and treated them the same as `const` lvalues. This behavior was deprecated because it is fundamentally unsound: any iterator returned by such an overload is highly likely to dangle after the full-expression that contained the invocation of `begin`.

[2]{.pnum} Another problem, and one that until recently seemed unrelated to the design of `begin`, was that algorithms that return iterators will wrap those iterators in `std::ranges::dangling<>` if the range passed to them is an rvalue.  This ignores the fact that for some range types — `std::span`, `std::string_view`, and P0789’s `subrange`, in particular — the iterator’s validity does not depend on the range’s lifetime at all. In the case where an rvalue of one of the above types is passed to an algorithm, returning a wrapped iterator is totally unnecessary.

[3]{.pnum} The author believed that to fix the problem with `subrange` and `dangling` would require the addition of a new trait to give the authors of range types a way to say whether its iterators can safely outlive the range. That felt like a hack.
:::

This paper was presented in Rapperswil 2018, partially jointly with [@P0896R1], and as far as I can tell from the minutes, this subtlety was not discussed.

## Why `ranges::begin()` on an rvalue?

The design of _`forwarding-range`_ is based on the ability to invoke `ranges::begin()` on an rvalue. But what is the actual motivation of doing such a thing? Why would I want to forward a range into `begin()`? Even in contexts of algorithms taking `range`s by forwarding reference, we could just call `begin()` on the lvalue range that we get passed in. It's not like any iterator transformations are performed - we get the same iterator either way (and the cases in which we would _not_ get the same iterator are errors, such types would fail the expression-equivalence requirement).

The machinery for `ranges::begin()` being invocable on an rvalue is entirely driven by the desire to detect iterator validity exceeding range lifetime. 

## Issues with overloading

In [@stl2.592], Eric Niebler points out that the current wording has the non-member `begin()` and `end()` for `subrange` taking it by rvalue reference instead of by value, meaning that `const subrange` doesn't count as a _`forwarding-range`_. But there is a potentially broader problem, which is that overload resolution will consider the `begin()` and `end()` functions for `subrange` even in contexts where they would be a worse match than the poison pill (i.e. they would involve conversions), and some of those contexts could lead to hard instantiation errors. So Eric suggests that the overload should be:

```cpp
friend constexpr I begin(same_as<subrange> auto r) { return r.begin(); }
```

Of the types in the standard library that should model _`forwarding-range`_, three of the four should take the same treatment (only `iota_view` doesn't need to worry). That is, in order to really ensure correctness by avoiding any potential hard instantiation errors, we have to write non-member `begin()` and `end()` function templates that constrain their argument via `same_as<R>`?

## How many mechanisms do we need?

At this point, we have three concepts in Ranges that have some sort of mechanism to opt-in/opt-out:

- _`forwarding-range`_: provide a non-member `begin()`/`end()` that take their argument by value or rvalue reference (but really probably a constrained function template)
- `view`: opt-in via the `enable_view` type trait
- `sized_range`: opt-out via the `disable_sized_range` trait (itself problematic due to the double negative, see [@P1871R0])

I don't think we need different mechanisms for each trait. I know Eric and Casey viewed having to have a type trait as a hack, but it's a hack around not having a language mechanism to express opt-in. It's still the best hack we have, that's the easiest to understand, that's probably more compiler-efficient as well (overload resolution is expensive!)

## Hard to get correct

Now that MSVC's standard library implementation is open source, we can take a look at how they went about implementing the opt-in for _`forwarding-range`_ in their implementation of `basic_string_view` [@msvc.basic_string_view]:

```cpp
#ifdef __cpp_lib_concepts
    _NODISCARD friend constexpr const_iterator begin(const basic_string_view& _Right) noexcept {
        // non-member overload that accepts rvalues to model the exposition-only forwarding-range concept
        return _Right.begin();
    }
    _NODISCARD friend constexpr const_iterator end(const basic_string_view& _Right) noexcept {
        // Ditto modeling forwarding-range
        return _Right.end();
    }
#endif // __cpp_lib_concepts
```

Note that these overloads take their arguments by reference-to-`const`. But the non-member overloads need to take their arguments by either value or rvalue reference, otherwise the poison pill is a better match, as described earlier. So at this moment, `std::string_view` fails to satisfy _`forwarding-range`_. If even Casey can make this mistake, how is anybody going to get it right?

# Proposal

The naming direction this proposal takes is to use the name `safe_range`, based on the existence of `safe_iterator` and `safe_subrange`. If a alternate name is preferred, the wording can simply be block replaced following the naming convention proposed in [@P1871R0]. The proposal has four parts:

- Trait: introduce a new variable template `enable_safe_range`, with default value `false.`
- Concept: rename the concept _`forwarding-range`_ to `safe_range`, make it non-exposition only, and have its definition be based on the type trait. Replace all uses of _`forwarding-range`_ with `safe_range` as appropriate.
- CPO: Have `ranges::begin()` and `ranges::end()`, and their const and reverse cousins, _only_ allow lvalues. Some of the other CPOs that invoke `begin()` and `end()` have to be adjusted to ensure that they only propagate lvalues too.
- Library opt-in: Have the library types which currently opt-in to _`forwarding-range`_ by providing non-member `begin` and `end` instead specialize `enable_safe_range`, and remove those overloads non-member overloads.

## Wording

Change 21.4.2 [string.view.template] to remove the non-member `begin`/`end` overloads that were the old opt-in and opt-in to `safe_range` [Can't just provide a specialization for `enable_safe_range` since we cannot forward-declare it]{.ednote}:

::: bq
```diff
template<class charT, class traits = char_traits<charT>>
class basic_string_view {

- friend constexpr const_iterator begin(basic_string_view sv) noexcept { return sv.begin(); }
- friend constexpr const_iterator end(basic_string_view sv) noexcept { return sv.end(); }

};
```

[1]{.pnum} In every specialization `basic_string_view<charT, traits>`, the type `traits` shall meet the character traits requirements ([char.traits]).
[ *Note*: The program is ill-formed if `traits​::​char_type` is not the same type as `charT`.
— *end note*
 ]
 
[1*]{.pnum} [All specializations of `basic_string_view` model `safe_range` ([range.range])]{.addu}
:::

Change 22.7.3.1 [span.overview] to remove the non-member `begin`/`end` overloads that were the old opt-in and add the new specialization to opt-in [Can't just provide a specialization for `enable_safe_range` since we cannot forward-declare it]{.ednote}:

::: bq
[1]{.pnum} A `span` is a view over a contiguous sequence of objects, the storage of which is owned by some other object.

[2]{.pnum} All member functions of `span` have constant time complexity.

[3]{.pnum} [All specializations of `span` model `safe_range` ([range.range])]{.addu}

```diff
namespace std {
  template<class ElementType, size_t Extent = dynamic_extent>
  class span {
    [...]
    
-   friend constexpr iterator begin(span s) noexcept { return s.begin(); }
-   friend constexpr iterator end(span s) noexcept { return s.end(); }

  private:
    pointer data_;    // exposition only
    index_type size_; // exposition only
  };
  
  template<class Container>
    span(const Container&) -> span<const typename Container::value_type>;
}  
```
:::

Change 24.2 [ranges.syn] to introduce the new trait and the new non-exposition-only concept:

::: bq
```diff
#include <initializer_list>
#include <iterator>

namespace std::ranges {
  [ ... ]

  // [range.range], ranges  
  template<class T>
    concept range = @_see below_@;

+ template <std::range T>
+   inline constexpr bool enable_safe_range = false;
+
+ template<class T>
+   concept safe_range = @_see below_@;

  [ ... ]    
  
  // [range.dangling], dangling iterator handling
  struct dangling;

  template<range R>
-   using safe_iterator_t = conditional_t<@[_forwarding-range_]{.diffdel}@<R>, iterator_t<R>, dangling>;
+   using safe_iterator_t = conditional_t<@[safe_range]{.diffins}@<R>, iterator_t<R>, dangling>;

  template<range R>
    using safe_subrange_t =
-     conditional_t<@[_forwarding-range_]{.diffdel}@<R>, subrange<iterator_t<R>>, dangling>;
+     conditional_t<@[safe_range]{.diffins}@<R>, subrange<iterator_t<R>>, dangling>;
      
  [...]
}
```
:::

Change the definitions of `ranges::begin()`, `ranges::end()`, and their `c` and `r` cousins, to only allow lvalues, and then be indifferent to member vs non-member (see also [@stl2.429]). That is, the poison pill no longer needs to force an overload taking a value or rvalue reference, it now only needs to force ADL - see also [@LWG3247]). Some of these changes aren't strictly necessary (e.g. the changes to `ranges::cbegin` explicitly call out being ill-formed for rvalues, but this could've been inherited from `ranges::begin`), they are made simply to make the specification easier to read. 

Change. 24.3.1 [range.access.begin]:

::: bq
[1]{.pnum} The name `ranges​::​begin` denotes a customization point object. The expression `ranges​::​​begin(E)` for some subexpression `E` is expression-equivalent to:

- [1.0]{.pnum} [If `E` is an rvalue, `ranges::begin(E)` is ill-formed.]{.addu}
- [1.1]{.pnum} [Otherwise, ]{.addu} `E + 0` if `E` is [an lvalue]{.rm} of array type ([basic.compound]).
- [1.2]{.pnum} Otherwise, [if `E` is an lvalue,]{.rm} _`decay-copy`_`(E.begin())` if it is a valid expression and its type `I` models `input_or_output_iterator`.
- [1.3]{.pnum} Otherwise, _`decay-copy`_`(begin(E))` if it is a valid expression and its type `I` models `input_or_output_iterator` with overload resolution performed in a context that includes the declaration[s]{.rm}:

    ```
    template<class T> void begin(T&&) = delete;
    template<class T> void begin(initializer_list<T>&&) = delete;
    ```
    and does not include a declaration of `ranges​::​begin`.

- [1.4]{.pnum} Otherwise, `ranges​::​begin(E)` is ill-formed. [ Note: This case can result in substitution failure when `ranges​::​begin(E)` appears in the immediate context of a template instantiation. *— end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​begin(E)` is a valid expression, its type models `input_or_output_iterator`. — *end note* ]
:::

Change 24.3.2 [range.access.end] similarly:

::: bq
[1]{.pnum} The name `ranges​::​end` denotes a customization point object. The expression `ranges​::​end(E)` for some subexpression `E` is expression-equivalent to:

- [1.0]{.pnum} [If `E` is an rvalue, `ranges::end(E)` is ill-formed.]{.addu}
- [1.1]{.pnum} [Otherwise, ]{.addu} `E + extent_v<T>` if E is [an lvalue]{.rm} of array type ([basic.compound]) `T`.
- [1.2]{.pnum} Otherwise, [if E is an lvalue,]{.rm} _`decay-copy`_`(E.end())` if it is a valid expression and its type `S` models `sentinel_for<decltype(ranges::begin(E))>`.
- [1.3]{.pnum} Otherwise, _`decay-copy`_`(end(E))` if it is a valid expression and its type `S` models `sentinel_for<decltype(ranges::begin(E))>` with overload resolution performed in a context that includes the declaration[s]{.rm}:

    ```
    template<class T> void end(T&&) = delete;
    template<class T> void end(initializer_list<T>&&) = delete;
    ```
    and does not include a declaration of `ranges​::​end`.

- [1.4]{.pnum} Otherwise, `ranges​::​end(E)` is ill-formed. [ *Note*: This case can result in substitution failure when `ranges​::​end(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​end(E)` is a valid expression, the types `S` and `I` of `ranges​::​end(E)` and `ranges​::​begin(E)` model `sentinel_for<S, I>`. — *end note* ]
:::

Change 24.3.3 [ranges.access.cbegin]:

::: bq
[1]{.pnum} The name `ranges​::​cbegin` denotes a customization point object.
The expression `ranges​::​​cbegin(E)` for some subexpression `E` of type `T` is expression-equivalent to:

- [1.1]{.pnum} `ranges​::​begin(static_cast<const T&>(E))` if `E` is an lvalue.
- [1.2]{.pnum} Otherwise, [`ranges​::​begin(static_cast<const T&&>(E))`]{.rm} [`ranges::cbegin(E)` is ill-formed]{.addu}.

[2]{.pnum} [ *Note*: Whenever `ranges​::​cbegin(E)` is a valid expression, its type models `input_or_output_iterator`. - *end note* ]
:::

Change 24.3.4 [ranges.access.cend]:

::: bq
[1]{.pnum} The name `ranges​::​cend` denotes a customization point object.
The expression `ranges​::​​cend(E)` for some subexpression `E` of type `T` is expression-equivalent to:

- [1.1]{.pnum} `ranges​::​end(static_cast<const T&>(E))` if `E` is an lvalue.
- [1.2]{.pnum} Otherwise, [`ranges​::end(static_cast<const T&&>(E))`]{.rm} [`ranges::cend(E)` is ill-formed]{.addu}.

[2]{.pnum} [ *Note*: Whenever `ranges​::​cend(E)` is a valid expression, the types `S` and `I` of `ranges​::​cend(E)` and `ranges​::​cbegin(E)` model `sentinel_for<S, I>`. - *end note* ]
:::

Change 24.3.5 [ranges.access.rbegin]:

::: bq
[1]{.pnum} The name `ranges​::​rbegin` denotes a customization point object. The expression `ranges​::​​rbegin(E)` for some subexpression `E` is expression-equivalent to:

- [1.0]{.pnum} [If `E` is an rvalue, `ranges::rbegin(E)` is ill-formed.`]{.addu}
- [1.1]{.pnum} [Otherwise]{.addu} [If `E` is an lvalue]{.rm}, _`decay-copy`_`(E.rbegin())` if it is a valid expression and its type `I` models `input_or_output_iterator`.
- [1.2]{.pnum} Otherwise, _`decay-copy`_`(rbegin(E))` if it is a valid expression and its type `I` models `input_or_output_iterator` with overload resolution performed in a context that includes the declaration:

    ```
    template<class T> void rbegin(T&&) = delete;
    ```
    and does not include a declaration of `ranges​::​rbegin`.

- [1.3]{.pnum} Otherwise, `make_reverse_iterator(ranges​::​end(E))` if `both ranges​::​begin(E)` and `ranges​::​end(​E)` are valid expressions of the same type `I` which models `bidirectional_iterator` ([iterator.concept.bidir]).
- [1.4]{.pnum} Otherwise, `ranges​::​rbegin(E)` is ill-formed. [ Note: This case can result in substitution failure when `ranges​::​rbegin(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​rbegin(E)` is a valid expression, its type models `input_or_output_iterator`. — *end note* ]
:::
 
Change 24.3.6 [range.access.rend]:

::: bq
[1]{.pnum} The name `ranges​::​rend` denotes a customization point object. The expression `ranges​::​rend(E)` for some subexpression `E` is expression-equivalent to:

- [1.0]{.pnum} [If `E` is an rvalue, `ranges::rend(E)` is ill-formed.`]{.addu}
- [1.1]{.pnum} [Otherwise]{.addu} [If `E` is an lvalue]{.rm}, _`decay-copy`_`(E.rend())` if it is a valid expression and its type `S` models
`sentinel_for<decltype(ranges::rbegin(E))>`.
- [1.2]{.pnum} Otherwise, _`decay-copy`_`(rend(E))` if it is a valid expression and its type `S` models `sentinel_for<decltype(ranges::rbegin(E))>`.
with overload resolution performed in a context that includes the declaration:

    ```diff
    template<class T> void rend(T&&) = delete;
    ```
    and does not include a declaration of `ranges​::​rend`.

- [1.3]{.pnum} Otherwise, `make_reverse_iterator(ranges​::​begin(E))` if both `ranges​::​begin(E)` and `ranges​::​​end(E)` are valid expressions of the same type `I` which models `bidirectional_iterator` ([iterator.concept.bidir]).
- [1.4]{.pnum} Otherwise, `ranges​::​rend(E)` is ill-formed. [ *Note*: This case can result in substitution failure when `ranges​::​rend(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​rend(E)` is a valid expression, the types `S` and `I` of `ranges​::​rend(E)` and `ranges​::​rbegin(E)` model `sentinel_for<S, I>`. — *end note* ]
:::

Change 24.3.7 [ranges.access.crbegin]:

::: bq
[1]{.pnum} The name `ranges​::​crbegin` denotes a customization point object.
The expression `ranges​::​​crbegin(E)` for some subexpression `E` of type `T` is expression-equivalent to:

- [1.1]{.pnum} `ranges​::​rbegin(static_cast<const T&>(E))` if `E` is an lvalue.
- [1.2]{.pnum} Otherwise, [`ranges​::​rbegin(static_cast<const T&&>(E))`]{.rm} [`ranges::crbegin(E)` is ill-formed]{.addu}.

[2]{.pnum} [ *Note*: Whenever `ranges​::​crbegin(E)` is a valid expression, its type models `input_or_output_iterator`. - *end note* ]
:::

Change 24.3.8 [ranges.access.crend]:

::: bq
[1]{.pnum} The name `ranges​::​crend` denotes a customization point object.
The expression `ranges​::​​crend(E)` for some subexpression `E` of type `T` is expression-equivalent to:

- [1.1]{.pnum} `ranges​::​rend(static_cast<const T&>(E))` if `E` is an lvalue.
- [1.2]{.pnum} Otherwise, [`ranges​::rend(static_cast<const T&&>(E))`]{.rm} [`ranges::crend(E)` is ill-formed]{.addu}.

[2]{.pnum} [ *Note*: Whenever `ranges​::​crend(E)` is a valid expression, the types `S` and `I` of `ranges​::​crend(E)` and `ranges​::​crbegin(E)` model `sentinel_for<S, I>`. - *end note* ]
:::

For `ranges::size`, `ranges::empty`, and `ranges::data`, we want to allow them to be invoked on rvalues that satisfy `safe_range`. The specification this needs to ensure that `ranges::begin` and `ranges::end` are still only called on lvalues. `ranges::cdata` requires no changes.

Change 24.3.9 [range.prim.size]:

::: bq
[1]{.pnum} The name `size` denotes a customization point object.
[The expression `ranges​::​size(E)` for some subexpression `E` with type `T`]{.rm} [Given a subexpression `E` with type `T` and an lvalue `t` that denotes the same object as `E`, the expression `ranges::size(E)`]{.addu} is expression-equivalent to:

- [1.1]{.pnum} _`decay-copy`_`(extent_v<T>)` if `T` is an array type ([basic.compound]).
- [1.2]{.pnum} Otherwise, if `disable_sized_range<remove_cv_t<T>>` ([range.sized]) is `false`:

    - [1.2.1]{.pnum} _`decay-copy`_`(E.size())` if it is a valid expression and its type `I` is integer-like ([iterator.concept.winc]).
    - [1.2.2]{.pnum} Otherwise, _`decay-copy`_`(size(E))` if it is a valid expression and its type `I` is integer-like with overload resolution performed in a context that includes the declaration:

        ```
        template<class T> void size(T&&) = delete;
        ```
            

        and does not include a declaration of `ranges​::​size`.
- [1.3]{.pnum} Otherwise, [if `decltype((E))` models `safe_range` ([range.range]),]{.addu} <code><em>make-unsigned-like</em>(ranges::end([E]{.rm} [t]{.addu}) - ranges::begin([E]{.rm} [t]{.addu}))</code> ([range.subrange]) if it is a valid expression and the types `I` and `S` of <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> and <code>ranges​::​end([E]{.rm} [t]{.addu})</code> (respectively) model both `sized_sentinel_for<S, I>` ([iterator.concept.sizedsentinel]) and `forward_iterator<I>`. [However, `E` is evaluated only once.]{.rm}
- [1.4]{.pnum} Otherwise, `ranges​::​size(E)` is ill-formed. [ *Note*: This case can result in substitution failure when `ranges​::​size(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​size(E)` is a valid expression, its type is integer-like. - *end note* ]
:::

Change 24.3.10 [range.prim.empty]:

::: bq
[1]{.pnum} The name `empty` denotes a customization point object.
[The expression `ranges​::​empty(E)` for some subexpression `E`]{.rm} [Given a subexpression `E` and an lvalue `t` that denotes the same object as `E`, the expression ``ranges::empty(E)`]{.addu} is expression-equivalent to:

- [1.1]{.pnum} `bool((E).empty())` if it is a valid expression.
- [1.2]{.pnum} Otherwise, `(ranges​::​size(E) == 0)` if it is a valid expression.
- [1.3]{.pnum} Otherwise, [if `decltype((E))` models `safe_range` ([range.range]),]{.addu} [`EQ`, where `EQ` is]{.rm} <code>[bool]{.dt}(ranges​::​begin([E]{.rm} [t]{.addu}) == ranges​::​end([E]{.rm} [t]{.addu}))</code> [except that `E` is only evaluated once, if `EQ`]{.rm} [if it]{.addu} is a valid expression and the type of <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> models `forward_iterator`.
- [1.4]{.pnum} Otherwise, `ranges​::​empty(E)` is ill-formed. [ *Note*: This case can result in substitution failure when `ranges​::​empty(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​empty(E)` is a valid expression, it has type `bool`. - *end note*  ]
:::

Change 24.3.11 [range.prim.data]:
[In this wording, in order for `decltype((E))` to model `safe_range`, it must necessarily follow that `ranges::begin(t)` is valid so we don't need extra wording to check it again.]{.ednote}

::: bq
[1]{.pnum} The name `data` denotes a customization point object.
[The expression `ranges​::​data(E)` for some subexpression `E`]{.rm} [Given a subexpression `E` and an lvalue `t` that denotes the same object as `E`, the expression `ranges::data(E)`]{.addu} is expression-equivalent to:

- [1.1]{.pnum} If `E` is an lvalue, _`decay-copy`_`(E.data())` if it is a valid expression of pointer to object type.
- [1.2]{.pnum} Otherwise, [if `decltype((E))` models `safe_range` ([range.range]) and the type of]{.addu} [if]{.rm} <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> [is a valid expression whose type]{.rm} models `contiguous_iterator`, <code>to_address(ranges​::​begin([E]{.rm} [t]{.addu}))</code>.
- [1.3]{.pnum} Otherwise, `ranges​::​data(E)` is ill-formed. [ *Note*: This case can result in substitution failure when `ranges​::​data(E)` appears in the immediate context of a template instantiation. — *end note* ]

[2]{.pnum} [ *Note*: Whenever `ranges​::​data(E)` is a valid expression, it has pointer to object type. — *end note* ]
:::

Change 24.4.2 [range.range]:

::: bq
[1]{.pnum} The `range` concept defines the requirements of a type that allows iteration over its elements by providing an iterator and sentinel that denote the elements of the range.

```diff
- template<class T>
-   concept range-impl =          // exposition only
-     requires(T&& t) {
-       ranges::begin(std::forward<T>(t));        // sometimes equality-preserving (see below)
-       ranges::end(std::forward<T>(t));
-     };
- 
- template<class T>
-   concept range = range-impl<T&>;
- 
- template<class T>
-   concept forwarding-range =    // exposition only
-     range<T> && range-impl<T>;

+ template<class T>
+   concept range =
+     requires(T& t) {
+       ranges::begin(t);                         // sometimes equality-preserving (see below)
+       ranges::end(t);
+     }; 
+
+ template<class T>
+   concept safe_range =
+     range<T> && (is_lvalue_reference_v<T> || enable_safe_range<remove_cvref_t<T>>);
```

[2]{.pnum} The required expressions [`ranges​::​begin(std​::​forward<T>(t))` and `ranges​::​end(std​::​forward<​T>(t))` of the _`range-impl`_ concept]{.rm} [`ranges::begin(t)` and `ranges::end(t)` of the `range` concept]{.addu} do not require implicit expression variations ([concepts.equality]).


[3]{.pnum} Given an expression `E` such that `decltype((E))` is `T` [and an lvalue `t` that denotes the same object as `E`]{.addu}, `T` models [_`range-impl`_]{.rm} [`range`]{.addu} only if

- [3.1]{.pnum} <code>[ranges​::​begin([E]{.rm} [t]{.addu}), ranges​::​end([E]{.rm} [t]{.addu}))</code> denotes a range ([iterator.requirements.general]),
- [3.2]{.pnum} both <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> and <code>ranges​::​end([E]{.rm} [t]{.addu})</code> are amortized constant time and non-modifying, and
- [3.3]{.pnum} if the type of <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> models `forward_iterator`, <code>ranges​::​begin([E]{.rm} [t]{.addu})</code> is equality-preserving.

[4]{.pnum} [ *Note*: Equality preservation of both `ranges​::​begin` and `ranges​::​end` enables passing a range whose iterator type models `forward_iterator` to multiple algorithms and making multiple passes over the range by repeated calls to `ranges​::​begin` and `ranges​::​end`.
Since `ranges​::​begin` is not required to be equality-preserving when the return type does not model `forward_iterator`, repeated calls might not return equal values or might not be well-defined; `ranges​::​begin` should be called at most once for such a range. - *end note* ]


[5]{.pnum} Given an expression `E` such that `decltype((E))` is `T` and an lvalue `t` that denotes the same object as `E`, `T` models [_`forwarding-range`_]{.rm} [`safe_range`]{.addu} only if [the validity of iterators obtained from the object denoted by `t` is not tied to the lifetime of that object.]{.addu}

::: rm
- [5.1]{.pnum} `ranges​::​begin(E)` and `ranges​::​begin(t)` are expression-equivalent,
- [5.2]{.pnum} `ranges​::​end(E)` and `ranges​::​end(t)` are expression-equivalent, and
- [5.3]{.pnum} the validity of iterators obtained from the object denoted by `E` is not tied to the lifetime of that object.
:::

[6]{.pnum} [ *Note*: Since the validity of iterators is not tied to the lifetime of an object whose type models [_`forwarding-range`_]{.rm} [`safe_range`]{.addu}, a function can accept arguments of such a type by value and return iterators obtained from it without danger of dangling. — *end note* ]

```diff
+ template<class>
+   inline constexpr bool enable_safe_range = true;
```

[6*]{.pnum} [*Remarks*: Pursuant to [namespace.std], users may specialize `enable_safe_range` for *cv*-unqualified program-defined types.
Such specializations shall be usable in constant expressions ([expr.const]) and have type `const bool`.]{.addu}

[7]{.pnum} [ Example: Specializations of class template `subrange` model [_`forwarding-range`_]{.rm} [`safe_range`]{.addu}. `subrange` [provides non-member rvalue overloads of `begin` and `end` with the same semantics as its member lvalue overloads]{.rm} [specializes `enable_safe_range` to `true`]{.addu}, and `subrange`'s iterators - since they are “borrowed” from some other range - do not have validity tied to the lifetime of a subrange object. — *end example*  ]
:::

Change 24.4.5 [range.refinements], the definition of the `viewable_range` concept:

::: bq
[4]{.pnum} The `viewable_range` concept specifies the requirements of a `range` type that can be converted to a `view` safely.

```diff
  template<class T>
    concept viewable_range =
-     range<T> && (@[_forwarding-range_]{.diffdel}@<T> || view<decay_t<T>>);
+     range<T> && (@[safe_range]{.diffins}@<T> || view<decay_t<T>>);
```
:::

Change 24.5.3 [range.subrange], to use `safe_range` instead of _`forwarding-range`_, to remove the non-member `begin`/`end` overloads that were the old opt-in, and to add a specialization for `enable_safe_range` which is the new opt-in:

::: bq
[1]{.pnum} The `subrange` class template combines together an iterator and a sentinel into a single object that models the `view` concept.
Additionally, it models the `sized_range` concept when the final template parameter is `subrange_kind​::​sized`.

```diff
namespace std::ranges {

  template<input_or_output_iterator I, sentinel_for<I> S = I, subrange_kind K =
      sized_sentinel_for<S, I> ? subrange_kind::sized : subrange_kind::unsized>
    requires (K == subrange_kind::sized || !sized_sentinel_for<S, I>)
  class subrange : public view_interface<subrange<I, S, K>> {
  
    template<@_not-same-as_@<subrange> R>
-     requires @[_forwarding-range_]{.diffdel}@<R> &&
+     requires @[safe_range]{.diffins}@<R> &&
        convertible_to<iterator_t<R>, I> && convertible_to<sentinel_t<R>, S>
    constexpr subrange(R&& r) requires (!StoreSize || sized_range<R>);

-   template<@[_forwarding-range_]{.diffdel}@ R>
+   template<@[safe_range]{.diffins}@ R>
      requires convertible_to<iterator_t<R>, I> && convertible_to<sentinel_t<R>, S>
    constexpr subrange(R&& r, @_make-unsigned-like-t_@(iter_difference_t<I>) n)
      requires (K == subrange_kind::sized)
        : subrange{ranges::begin(r), ranges::end(r), n}
    {}  
  
-   friend constexpr I begin(subrange&& r) { return r.begin(); }
-   friend constexpr S end(subrange&& r) { return r.end(); }
  };


- template<@[_forwarding-range_]{.diffdel}@ R>
+ template<@[safe_range]{.diffins}@ R>
    subrange(R&&) ->
      subrange<iterator_t<R>, sentinel_t<R>,
               (sized_range<R> || sized_sentinel_for<sentinel_t<R>, iterator_t<R>>)
                 ? subrange_kind::sized : subrange_kind::unsized>;

- template<@[_forwarding-range_]{.diffdel}@ R>
+ template<@[safe_range]{.diffins}@ R>
    subrange(R&&, @_make-unsigned-like-t_@(range_difference_t<R>)) ->
      subrange<iterator_t<R>, sentinel_t<R>, subrange_kind::sized>;
      
  template<size_t N, class I, class S, subrange_kind K>
    requires (N < 2)
  constexpr auto get(const subrange<I, S, K>& r);
  
+ template<input_or_output_iterator I, sentinel_for<I> S, subrange_kind K>
+   inline constexpr bool enable_sized_range<subrange<I, S, K>> = true;
}
```
:::

Change the name of the concept in 24.5.3.1 [range.subrange.ctor]:

::: bq
```diff
  template<@_not-same-as_@<subrange> R>
-   requires @[_forwarding-range_]{.diffdel}@<R> &&
+   requires @[safe_range]{.diffins}@<R> &&
             convertible_to<iterator_t<R>, I> && convertible_to<sentinel_t<R>, S>
  constexpr subrange(R&& r) requires (!StoreSize || sized_range<R>);
```
:::

Change the name of the concept in 24.5.4 [range.dangling]:

::: bq
[1]{.pnum} The tag type `dangling` is used together with the template aliases `safe_iterator_t` and `safe_subrange_t` to indicate that an algorithm that typically returns an iterator into or subrange of a `range` argument does not return an iterator or subrange which could potentially reference a range whose lifetime has ended for a particular rvalue `range` argument which does not model [_`forwarding-range`_]{.rm} [`safe_range`]{.addu} ([range.range]).

[2]{.pnum} [ *Example*: [...] 

The call to `ranges​::​find` at `#1` returns `ranges​::​dangling` since `f()` is an rvalue `vector`; the `vector` could potentially be destroyed before a returned iterator is dereferenced. However, the calls at `#2` and `#3` both return iterators since the lvalue vec and specializations of `subrange` model [_`forwarding-range`_]{.rm} [`safe_range`]{.addu}. — *end example* ]
:::

[^1]: There is a hypothetical kind of range where the range itself owns its data by `shared_ptr`, and the iterators _also_ share ownership of the data. In this way, the iterators' validity isn't tied to the range's lifetime not because the range doesn't own the elements (as in the `span` case) but because the iterators _also_ own the elements. I'm not sure if anybody has ever written such a thing.
[^2]: I intend this as a positive, not as being derogatory. 

# Acknowledgements

Thanks to Eric Niebler and Casey Carter for going over this paper with me, and correcting some serious misconceptions earlier drafts had. Thanks to Tim Song and Agustín Bergé for going over the details. 

---
references:
    - id: stl2.429
      citation-label: stl2.429
      title: "Consider removing support for rvalue ranges from range access CPOs"
      author:
        - family: Casey Carter
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/429
    - id: stl2.547
      citation-label: stl2.547
      title: "Redesign begin/end CPOs to eliminate deprecated behavior"
      author:
        - family: Eric Niebler
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/547
    - id: stl2.592
      citation-label: stl2.592
      title: "`const subrange<I,S,[un]sized>` is not a _`forwarding-range`_"
      author:
        - family: Eric Niebler
      issued:
        - year: 2018
      URL: https://github.com/ericniebler/stl2/issues/592
    - id: P1871R0
      citation-label: P1871R0
      title: "Should concepts be enabled or disabled?"
      author:
        - family: Barry Revzin
      issued:
        - year: 2019
      URL: https://wg21.link/p1871r0
    - id: msvc.basic_string_view
      citation-label: msvc.basic_string_view
      title: "non-member `begin()`/`end()` for `basic_string_view`"
      issued:
        -year: 2019
      URL: https://github.com/microsoft/STL/blame/92508bed6387cbdae433fc86279bc446af6f1b1a/stl/inc/xstring#L1207-L1216
---