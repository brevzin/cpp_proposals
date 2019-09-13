---
title: _`forwarding-range`_`<T>` is too subtle
document: D1858R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

One of the concepts introduces by Ranges is _`forwarding-range`_. Rather than describe the definition of that concept, I'm going to describe the intent of it and how it is used by various aspects of the Ranges design. The salient aspect of what makes a _`forwarding-range`_ is stated in [\[range.range\]](http://eel.is/c++draft/range.range):

> the validity of iterators obtained from the object denoted by `E` is not tied to the lifetime of that object.

clarified more more in the subsequent note:

> *[ Note*: Since the validity of iterators is not tied to the lifetime of an object whose type models _`forwarding-range`_, a function can accept arguments of such a type by value and return iterators obtained from it without danger of dangling.
*— end note ]*

For example, `std::vector<T>` is not a _`forwarding-range`_ because any iterator into a `vector` is of course dependent on the lifetime of the `vector` itself. On the other hand, `std::string_view` _is_ a _`forwarding-range`_ because it does not actually own anything - any iterator you get out of it has its lifetime tied to some other object entirely.

But while `span` and `subrange` each model _`forwarding-range`_, not all views do. For instance, `transform_view` would not because its iterators' validity would be tied to the unary function that is the actual transform. You could increment those iterators, but you couldn't dereference them. Likewise, `filter_view`'s iterator validity is going to be based on its predicate. Really, a _`forwarding-range`_ is quite a rare creature.

The picture is actually more complex than that though, because value category plays into this. Notably, _lvalue_ ranges all model _`forwarding-range`_ -- the "object" in question in this case is an lvalue reference, and of the validity of iterators into a range are never going to be tied to the lifetime of some reference to that range. `std::vector<T>` is not a _`forwarding-range`_, but `std::vector<T>&` is. The only question is about _rvalue_ ranges. If I have a function that either takes a range by forwarding reference or by value, I have to know what I can do with it.

Ranges uses this in two kinds of places:

- Many algorithms return iterators into a range. Those algorithms conditionally return either `iterator_t<R>` or `dangling` based on whether or not `R` satisfies _`forwarding-range`_ (because if `R` did not, then such iterators would not be valid, so they are not returned). This type is called `safe_iterator_t<R>` and appears over 100 times in [\[algorithms\]](http://eel.is/c++draft/algorithms).
- Range adapters can only be used on rvalue ranges if they satisfy either _`forwarding-range`_ or they decay to a `view`. The former because you may need to keep iterators into them past their lifetime, and the latter because if you can cheaply copy it than that works too. This higher-level concept is called `viewable_range`, and every range adapter depends on it.

That is, _`forwarding-range`_ is a very important concept. It is used practically everywhere. It also conveys a pretty subtle and very rare feature of a type: that its iterators can outlive it. Syntactically, there is no difference between a `range`, a `view`, and a _`forwarding-range`_, so the question is - how does a type declare itself to have this feature?

# Opting into _`forwarding-range`_

Since C++11, we're all generally familiar with ranges. They haven't changed _too_ much over the years. Ranges are either arrays, a type with member `begin()` and `end()`, nor a type with non-member `begin()` and `end()` that are in an associated namespace. Those functions have to return iterators. Up through C++20, all the algorithms required both iterators to have the same type - although in C++17, the range-based for statement was relaxed to allow for the new notion of a Sentinel, which could have a different type from the iterator.

I'm just repeating what everyone already knows, just so we're all on the same page. And I actually just want to repeat it one more time. Since C++11, we've had this language concept of a range that could _either_ have member _or_ non-member functions for `begin()` and `end()` and these have entirely the same semantics. If you're the kind of person that prefers to write `begin(c)` over `c.begin()`, you could go ahead and write your containers with _only_ non-member accessors and that would work without any issues.

That said, how does the C++20 concept _`forwarding-range`_`<T>` check to see if an rvalue `range` can have its iterator validity outlive the object? Try to take a look in [\[range.range\]](http://eel.is/c++draft/range.range) to see if you can figure it out. It took me a very long time to determine the answer to this for myself.

Here's our concept:

```cpp
template<class T>
  concept @_range-impl_@ =          // exposition only
    requires(T&& t) {
      ranges::begin(std::forward<T>(t));        // sometimes equality-preserving (see below)
      ranges::end(std::forward<T>(t));
    };

template<class T>
  concept range = @_range-impl_@<T&>;

template<class T>
  concept @_forwarding-range_@ =    // exposition only
    range<T> && @_range-impl_@<T>;
```

## Lvalues

Let's start with the lvalue case, since it's easier.

Consider _`forwarding-range`_`<L&>`. This calls into _`range-impl`_`<L&>` twice, and leads to us trying to invoke `ranges::begin` and `ranges::end` with an lvalue of type `L`. Ignoring arrays, `ranges::begin(E)` on an lvalue can either be (from [\[range.access.begin\]](http://eel.is/c++draft/range.access.begin)):

- _`decay-copy`_`(E.begin())` if that is a valid expression (and an iterator), or
- _`decay-copy`_`(begin(E))` if that is a valid expression (and an iterator) with some extra poison pill overloads so as to not match the preexisting `std::begin`.

That's basically what we would expect from our C++11 experience. Member or non-member `begin()`, either way works fine. Straightforward and expected.

The same process holds for `end()`, so I'll just skip it for brevity here on out.

## Rvalues

Alright, now what about rvalues? When does _`forwarding-range`_`<R>` hold? Now we check both _`range-impl`_`<R&>` and _`range-impl`_`<R>`. The former we just went through - we either need a member or non-member `begin()`. But the rvalue case is a little different. The member `begin()` was _only_ considered if our expression, `E`, was an lvalue. But now it's an rvalue. Which means we _only_ are looking for:

- _`decay-copy`_`(begin(E))` if that is a valid expression (and an iterator) with some extra poison pill overloads so as to not match the preexisting `std::begin`.

That is, _`range-impl`_`<R>` is _only_ satisfied if you have non-member `begin()`. Meaning that _`forwarding-range`_`<R>` is only satisfied if you have a non-member `begin()`.

## So...?

The conclusion that _`forwarding-range`_`<L&>` is satisfied with either member or non-member `begin()` but _`forwarding-range`_`<R>` is satisfied only with non-member `begin()` seems like an entirely uninteresting conclusion.

But let's flip the causality. 

If your type, `R`, has non-member `begin()` and `end()` (and assuming these return an iterator/sentinel), then `R` satisfies _`forwarding-range`_. Which is to say, if at some point you decided that you wanted to opt in to being a range with non-member functions instead of member functions, you have just _also_ opted in to stating that your iterators can safely outlive your range.

I find this to be incredibly subtle and quite surprising. It's imposing a lot of meaning onto a choice that up until now had no meaning associated with it at all. 

## And also...

Moreover, the current definition doesn't even work. The criteria laid out in [\[range.range\]/5](http://eel.is/c++draft/range.range#5) are:

> Given an expression `E` such that `decltype((E))` is `T` and an lvalue `t` that denotes the same object as `E`, `T` models _`forwarding-range`_ only if 
>
> - `ranges​::​begin(E)` and `ranges​::​begin(t)` are expression-equivalent,
> - `ranges​::​end(E)` and `ranges​::​end(t)` are expression-equivalent, and
> - the validity of iterators obtained from the object denoted by `E` is not tied to the lifetime of that object.

Every type in the standard library that is intended to model _`forwarding-range`_ has both the required non-member `begin()`/`end()` and also has member `begin()`/`end()` (you can see the synposes for [`span`](http://eel.is/c++draft/span.overview), [`string_view`](http://eel.is/c++draft/string.view.template), and [`subrange`](http://eel.is/c++draft/range.subrange)). Which means that `ranges::begin(E)` and `ranges::begin(t)` aren't going to be expression-equivalent since they call different functions - the former calls the non-member `begin` and the latter calls the member `begin`.

## History

The origin of this design was [@P0970R1], which describes the earlier problems with `ranges::begin()` thusly:

::: bq
[1]{.pnum} For the sake of compatibility with `std::begin` and ease of migration, `std::ranges::begin` accepted rvalues and treated them the same as `const` lvalues. This behavior was deprecated because it is fundamentally unsound: any iterator returned by such an overload is highly likely to dangle after the full-expression that contained the invocation of `begin`.

[2]{.pnum} Another problem, and one that until recently seemed unrelated to the design of `begin`, was that algorithms that return iterators will wrap those iterators in `std::ranges::dangling<>` if the range passed to them is an rvalue.  This ignores the fact that for some range types — `std::span`, `std::string_view`, and P0789’s `subrange`, in particular — the iterator’s validity does not depend on the range’s lifetime at all. In the case where an rvalue of one of the above types is passed to an algorithm, returning a wrapped iterator is totally unnecessary.

[3]{.pnum} The author believed that to fix the problem with `subrange` and `dangling` would require the addition of a new trait to give the authors of range types a way to say whether its iterators can safely outlive the range. That felt like a hack.
:::

This paper was presented in Rapperswil 2018, partially jointly with [@P0896R1], and as far as I can tell from the minutes, this subtlety was no discussed.

In my opinion, the addition of a new trait does not feel like a hack [^1]. On the contrary, I think it's absolutely essential that functionality like this be explicitly opted into. Moreover, this particular functionality is very rare - so the amount of noise generated by this extra specializations is approximately zero. 

# Proposal

Introduce a new trait, with some meaningful name:

```cpp
template <std::range T>
  struct iterators_can_outlive_range : std::false_type
  { };

template <std::range T>
  struct iterators_can_outlive_range<T&> : std::true_true
  { };
  
template <std::range T>
  struct iterators_can_outlive_range<T&&> : iterators_can_outlive_range<T>
  { };
```

Change the definition of _`forwarding-range`_ to look at this type trait:

```cpp
template<class T>
  concept @_forwarding-range_@ =    // exposition only
    range<T> && iterators_can_outlive_range<T>::value;
```

Change the definitions of `ranges::begin()` and `ranges::end()` to be agnostic to value category for considering member and non-member functions. That is:

::: bq
[1]{.pnum} The name `ranges​::​begin` denotes a customization point object. The expression `ranges​::​​begin(E)` for some subexpression `E` is expression-equivalent to:

- [1.1]{.pnum}`E + 0` if `E` is [an lvalue]{.rm} of array type ([basic.compound]).
- (1.2){.pnum} Otherwise, [if E is an lvalue,]{.rm} _`decay-copy`_`(E.begin())` if it is a valid expression and its type `I` models `input_or_output_iterator`.
- [1.3]{.pnum} Otherwise, _`decay-copy`_`(begin(E))` if it is a valid expression and its type `I` models `input_or_output_iterator` [with overload resolution performed in a context that includes the declarations:]{.rm}

::: rm
```cpp
template<class T> void begin(T&&) = delete;
template<class T> void begin(initializer_list<T>&&) = delete;
```

and does not include a declaration of `ranges​::​begin`.
:::

- [1.4]{.pnum} Otherwise, `ranges​::​begin(E)` is ill-formed. [ Note: This case can result in substitution failure when `ranges​::​begin(E)` appears in the immediate context of a template instantiation. *— end note* ]
:::

And similarly for `ranges::end()`.

And lastly, add specializations of `iterators_can_outlive_range` for `basic_string_view`, `span`, and `subrange`.

[^1]: To the extent that it is a hack, it's based on the limitation of the concepts language feature. 

---
references:
---