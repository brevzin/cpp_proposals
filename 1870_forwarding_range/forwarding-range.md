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

clarified more more in the subsequent note:

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

The design of _`forwarding-range`_ is based on the ability to invoke `ranges::begin()` on an rvalue. But what is the actual motivation of doing such a thing? Why would I want to forward a range into `begin()`? Even in contexts of algorithms taking `range`s by forwarding reference, we could just call `begin()` on the lvalue range that we get passed in. It's not like any iterator transformations are performed - we get the same iterator either way (and the cases in which we would _not_ get the same iterator are part of the motivation for this paper).

The machinery for `ranges::begin()` being invocable on an rvalue seems entirely driven by the desire to detect iterator validity exceeding range lifetime. 

## Issues with overloading

In [@stl2.592], Eric Niebler points out that the current wording has the non-member `begin()` and `end()` for `subrange` taking it by rvalue reference instead of by value, meaning that `const subrange` doesn't count as a _`forwarding-range`_. But there is a potentially broader problem, which is that overload resolution will consider the `begin()` and `end()` functions for `subrange` even in contexts where they would be a worse match than the poison pill (i.e. they would involve conversions), and some of those contexts could lead to hard instantiation errors. So Eric suggests that the overload should be:

```cpp
friend constexpr I begin(same_as<subrange> auto r) { return r.begin(); }
```

Of the types in the standard library that should model _`forwarding-range`_, three of the four should take the same treatment (only `iota_view` doesn't need to worry). That is, in order to really ensure correctness, we have to write non-member `begin()` and `end()` function templates that constrain their argument via `same_as<R>`?

## How many mechanisms do we need?

At this point, we have three concepts in Ranges that have some sort of mechanism to opt-in/opt-out:

- _`forwarding-range`_: provide a non-member `begin()`/`end()` that take their argument by value or rvalue reference (but really probably a constrained function template)
- `view`: opt-in via the `enable_view` type trait
- `sized_range`: opt-out via the `disable_sized_range` trait (itself problematic due to the double negative)

I don't think we need different mechanisms for each trait. I know Eric and Casey viewed having to have a type trait as a hack, but it's a hack around not having a langauge mechanism to express opt-in. It's still the best hack we have, that's the easiest to understand, that's probably more compiler-efficient as well (overload resolution is expensive!)

# Proposal

The naming direction this proposal takes is to use the name `safe_range`, based on the existence of `safe_iterator` and `safe_subrange`. Having these three closely-related notions have closely-related names makes sense.

## Trait

Introduce a new trait:

```cpp
template <std::range T>
    inline constexpr bool enable_safe_range = false;
```

## Concept

Rename _`forwarding-range`_ to `safe_range` and make it non-exposition only and have its definition use the type trait:

```cpp
template<class T>
  concept safe_range =
    range<T> && (is_lvalue_reference_v<T> || enable_safe_range<remove_cvref_t<T>>);
```

Replace all the uses of _`forwarding-range`_`<T>` with `safe_range<T>`.

## CPO

Change the definitions of `ranges::begin()` and `ranges::end()` to only allow lvalues, and then be indifferent to member vs non-member (see also [@stl2.429]). That is (the poison pill no longer needs to force an overload taking a value or rvalue reference, it now only needs to force ADL - see also [@LWG3247]):

::: bq
[1]{.pnum} The name `ranges​::​begin` denotes a customization point object. The expression `ranges​::​​begin(E)` for some subexpression `E` is expression-equivalent to:

- [1.0]{.pnum} [If `E` is an rvalue, `ranges::begin(E)` is ill-formed.]{.addu}
- [1.1]{.pnum}`E + 0` if `E` is [an lvalue]{.rm} of array type ([basic.compound]).
- [1.2]{.pnum} Otherwise, [if `E` is an lvalue,]{.rm} _`decay-copy`_`(E.begin())` if it is a valid expression and its type `I` models `input_or_output_iterator`.
- [1.3]{.pnum} Otherwise, _`decay-copy`_`(begin(E))` if it is a valid expression and its type `I` models `input_or_output_iterator` with overload resolution performed in a context that includes the declaration[s]{.rm}:

::: bq
```diff
- template<class T> void begin(T&&) = delete;
- template<class T> void begin(initializer_list<T>&&) = delete;
+ void begin();
```
:::

and does not include a declaration of `ranges​::​begin`.

- [1.4]{.pnum} Otherwise, `ranges​::​begin(E)` is ill-formed. [ Note: This case can result in substitution failure when `ranges​::​begin(E)` appears in the immediate context of a template instantiation. *— end note* ]
:::

And similarly for `ranges::end()` and `ranges::c?r?{begin,end}`.

## Library opt-in

And lastly, add specializations of `enable_safe_range` for all specializations of `basic_string_view`, `span`, and `subrange`. Remove the non-member `begin()` and `end()` functions for each of these.

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
---