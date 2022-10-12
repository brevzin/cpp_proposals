---
title: "Syntax Choices for Generalized Pack Declaration and Usage"
document: P2671R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

[@P1858R2] proposed a lot of new facilities for working with packs and tuples. This paper is intended to be a companion paper to that one (it's kind of like an R3), but is focused solely on syntax decisions.

That paper roughly provided three kinds of facilities:

1. allowing declaring packs in more places
2. allowing more functionality for packs (indexing and slicing)
3. allowing more functionality for tuples (indexing and unpacking)

These facilities had [varying levels](https://github.com/cplusplus/papers/issues/612#issuecomment-702259945) of support, and I think it's worth exploring the various syntax options in more detail.

# Packs in More Places

[@P1858R2] proposed allowing pack declarations in more contexts: more importantly allowing non-static data member packs:

::: bq
```cpp
template <typename... Ts>
struct tuple {
    Ts... elems; // proposed
};

template <typename... Ts>
void foo(Ts... ts) {
  using... Pointers = T*; // proposed
  Pointers... ps = &ts;   // proposed
}
```
:::

Here, the syntax is basically already chosen for us. The syntax for member pack declaration has to be that, and its semantics follow pretty straightforwardly from all the other pack rules.

The local pack declarations are more interesting in that we have the question of what goes on the right-hand-side:

1. `auto... xs = ys;` (right-hand side is an unexpanded pack)
2. `auto... xs = ys...;` (right-hand side is an expanded pack)
3. `auto... xs = {ys...};` (right-hand side is an expanded pack within braces)

Here, we have precedent from [@P0780R2] for option 1, since in lambdas we'd write `[...xs=ys]{}`. Option 2 here is a bit jarringly different, although option 3 is attractive for its ad hoc introduction of pack literals. The original paper [briefly discussed this](https://brevzin.github.io/cpp_proposals/1858_generalized_packs/p1858r2.html#what-about-stdpair) and it also has interesting interplay with what to do about expansion statements [@P1306R1] [^exp].

[^exp]: There, the facility was trying to support iterating over both packs and tuples, but it can be ambiguous in some contexts. If the direction were to iterate over `tuple` as a tuple and `{pack...}` as a pack, we could get both pieces of functionality with any ambiguity and without having to construct a tuple out of the pack.

I think we have to support option 1, and we should seriously consider option 3.

## Disambiguating Nested Packs

Once we allow declaring a member pack, we have to deal with the problem of disambiguation. Since if I pass an instance of the earlier `tuple` to a function template, and that template wants to expand the `elems` member, there needs to be some marker for that:

::: bq
```cpp
template <typename Tuple>
auto sum_tuple(Tuple tuple) -> int {
    // this can't work as-is
    return (tuple.elems + ...);
}
```
:::

[@P1858R2] proposed leading ellipsis for this (because once you start dealing with dots, everything is dots):

::: bq
```cpp
template <typename Tuple>
auto sum_tuple(Tuple tuple) -> int {
    return (tuple. ...elems + ...);  // ok
}
```
:::

There aren't too many other options here - we need some marker to either put in front or behind `elems`. Using the word `pack` might be nice, as in `tuple.pack elems...` - that's probably a viable context-sensitive parse, since that is currently nonsense. Something longer like `tuple.packname elems...` seems a bit much, or perhaps something like `tuple.pack!(elems)...`?

# More Functionality for Packs

There are three things that we can't easily do with packs, but should be able to:

1. indexing
2. slicing (i.e. producing another pack out of the original one)
3. iterating (covered separately by [@P1306R1])

## Indexing

The issue with indexing is largely a choice of syntax. We can't use `$pack$[0]` because of potential ambiguity if this appears in a pack expansion: `call(x[0] + x...)` is valid syntax today, but this isn't asking for the first element of the pack `x`, it's indexing into every element of the pack `x`.

That leaves introducing some new syntax that is distinct. I think the options are here `$pack$.[0]` (as [@N4235] suggested) or `$pack$...[0]` (as [@P1858R2] suggested). Between `$pack$.[0]` and `$pack$...[0]`, I think the latter is better for two reasons:

1. It does seem like operations with packs should just use `...`, this one is no different
2. It looks like shorthand for `tuple($pack$...)[0]`. The latter isn't valid yet (would require constexpr function parameters to be valid), but the similarity between the two expressions seems compelling to me anyway.

An alternative would be some way to annotate `$pack$` as being a pack to avoid the above ambiguity. Like `$OBJECT$($pack$)` or `$pack$.$into_object$()`. But we need some kind of syntax to be _definitely_ unambiguous and also _distinct_ grammatically, which neither of those really allow for. We would need some sort of token.

One option that does work is `$pack$![0]`. The `!` cannot appear after an identifier today. This could be a postfix operator that can only follow the name of a pack, and must precede an indexing operation. There are a few other tokens that could be used here as well, but there's something kind of nice about `elems!` meaning "no, actually, this `elems` thing as a whole!" that I kind of like. This also provides an answer for expansion statements: we always expand an object, and if we want to expand a pack we have to first turn it into an object - that's `$pack$!`.

Between `$pack$...[0]` and `$pack$![0]`, the former has the benefit of (some kind of) consistency with existing pack facilities, while the latter has the benefit of using fewer dots. There are a lot of dots when dealing with packs and it would be nice to have fewer of them. The downside though is that introducing more tokens pushes us further on the path to Perl, so there's no free lunch here either.

Lastly, there could hypothetically be some named reflection function which probably would look like `std::pack_index<0>($pack$...)`. But this is just a guess, as [@P1240R2] doesn't mention anything related. I think there's a lot of value in using indexing syntax to do indexing, so I would strongly prefer a dedicated language feature for this case.

Note that this is a frequently desired utility, and there is a whole talk at CppNow specifically about how to efficiently index into a pack: [The Nth Element: A Case Study](https://www.youtube.com/watch?v=LfOh0DwTP00) [^indexing]. Also note that Circle implements the `$pack$...[0]` syntax.

[^indexing]: Spoiler alert, having a dedicated language feature doesn't just look much better, it also compiles tremendously faster.

### Type Indexing

Pack indexing shouldn't just work for a pack of values, it should also work for a pack of types. That is:

::: cmptable
### Indexing as `...[0]`
```cpp
template <typename... Ts>
auto first(Ts... ts) -> Ts...[0] {
  return ts...[0];
}
```

### Indexing as `![0]`
```cpp
template <typename... Ts>
auto first(Ts... ts) -> Ts![0] {
  return ts![0];
}
```
:::

Packs of types follow the same principle as packs of values.

### Indexing from the Back

Once we have indexing in general, a lot of the rules more or less follow. The index needs to be within the range of the size of the pack, and if not, that's an immediate-context error (`first()` above is ill-formed, since there is no first type, but it's ill-formed in a SFINAE-friendly way).

But there's still an interesting question here: how do you return the _last_ element? Well, you could write this:

::: bq
```cpp
template <typename... Ts>
auto last(Ts... ts) -> Ts...[sizeof...(Ts) - 1] {
  return ts...[sizeof...(ts) - 1];
}
```
:::

This is... fine. This is fine. It works, it does the right thing. But it's a bit tedious.

Other languages provide a dedicated facility for counting backwards from the last element:

* Python uses `Ts...[-1]`
* D uses `Ts...[$-1]`
* C# uses `Ts...[^1]`

The problem with negative indexing is that while it's convenient most of the time, and is reasonably easy to understand and use, it does have surprising problems on the edges:

::: bq
```python
def last_n_items(xs, n):
    return xs[-n:]

last_n_items(range(10), 3) # [7, 8, 9]
last_n_items(range(10), 2) # [8, 9]
last_n_items(range(10), 1) # [9]
last_n_items(range(10), 0) # [0, 1, 2, 3, 4, 5, 6, 7, 8, 9]
```
:::

Not to mention that if you do some math to determine an index, it'd be nice if overflowing past zero would be treated as an error rather than some wildly different meaning.

The D and C# approaches don't have this issue. But `$`{.op} seems like a waste of that token, which can be put to more interesting uses (although it has prior art in regex as well, also meaning the end). The C# approach is pretty interesting. In both cases, this syntax can _only_ appear inside of indexing (or slicing) expressions. But I think such a restriction would be fine. In both cases, had the Python code used these alternate syntaxes (returning either `xs[$-n:]` or `xs[^n:]`, as appropriate), then the first three calls would be equivalent while the last call, `last_n_items(range(10), 0)`, would return an empty list -- which would be the correct answer.

I personally like the C# approach:

::: bq
```cpp
template <typename... Ts>
auto last(Ts... ts) -> Ts...[^1] {
  return ts...[^1];
}
```
:::

Circle implements Python's approach.

## Slicing

Sometimes you want one element from a pack, sometimes you want multiple. That's slicing. There are basically two syntax I've seen languages use for slicing:

* `[from:to]`
* `[from..to]`

In both cases, `from` can be omitted (implicitly meaning `0`) and `to` can be omitted (implicitly meaning the end). That is, `[:]` or `[..]` would mean to take the entire pack.

The advantage of the former is that it uses less dots. And also that it also can be extended by another argument as `x[from:to:stride]` (e.g. `[::2]` would be taking every other element, starting from the first).

The advantage of the latter is that it's probably more viable to in a for loop (e.g. `for (int i : 0..10)`, since having the extra colon would be fairly awkward) and it simply looks more like we're presenting a range [^dslice].

[^dslice]: D's slice overloading is [fairly involved](https://dlang.org/spec/operatoroverloading.html#slice), and also supports adding multiple groups. Like `x[1, 2, 8..20]`. On the one hand, this is interesting, but on the other hand with the adoption of multi-dimensional subscript operators, we're establishing a meaning for `x[1, 2]` in C++ that is at odds with interpreting this as a slice of the 2nd and 3rd elements.

Regardless of which syntax to choose, the arguments about [indexing from the back](#indexing-from-the-back) still apply, as well as the syntax for how to slice a pack. If we're going to index into a pack via `$pack$...[0]` then we should likewise slice a pack via `$pack$...[1:]` (or `$pack$...[1..]`). And what we end up with, at this point is... still a pack. So it would need to be expanded.

For instance, one (not-great) way of writing `sum` might be (demonstrated using both indexing syntax options):

::: cmptable
### Indexing with `...[0]`
```cpp
template <class... Ts>
auto sum(Ts... ts) -> int {
  if (sizeof...(Ts) == 0) {
    return 0;
  } else {
    return ts...[0] + sum(ts...[1..]...);
  }
}
```
### Indexing with `![0]`
```cpp
template <class... Ts>
auto sum(Ts... ts) -> int {
  if (sizeof...(Ts) == 0) {
    return 0;
  } else {
    return ts![0] + sum(ts![1..]...);
  }
}
```
:::

`ts...[0]` (or `ts![0]`) is the first element of the pack. Then, `ts...[1..]` (or `ts![1..]`) is a slice that starts from the second element and goes to the end - which is still a pack, so it is then expanded into `sum`.

Slicing, while not as important as indexing, still is an operation that regularly comes up, so I think it would be important to support. Whichever syntax we choose for slicing a pack could also be used to slice other objects as well. If I have some `s` that is a `span<T>`, `s[1..]` could conceivably be made to work (and evaluate basically as `s.subspan(1)`, which we have today).

Slicing also presents a good motivation for choosing `$pack$![0]` as the choice fo indexing, because if slicing a pack involves ellipsis and then expanding that pack involves another ellipsis _and also_ the slicing involves `..`, that's a tremendous amount of `.`s for a single expression.

## Summary

The two pack operations suggested here both have similar syntax. Either we expand the pack and index into it:

* `$pack$...[0]` gives me the first element
* `$pack$...[1..]` gives me every element starting with the second, and is still a pack

or we have a special syntax to identify that the pack should be treated as a distinct object and index into that:

* `$pack$![0]` gives me the first element
* `$pack$![1..]` gives me every element starting with the second, and is still a pack

I think both syntax options work pretty well, and are consistent with how both packs and indexing work today.

Importantly, `$pack$` in the above should be an identifier that denotes a pack, not an arbitrary expression. Partially this avoids the question of: in `f(x)...[0]`, how many times is `f` invoked? But also because it's not strictly necessary anyway. If I want to invoke `f` on the first element, I could do `f(x...[0])`. If I wanted to invoke `f` on all the elements starting from the second, I don't need `f(x)...[1..]...`, I can just `f(x...[1..])...` It's the same amount of characters in both cases anyway, so there's really no reason to have to get complicated here.

# More Functionality for Tuples

There are two things we can't easily do with tuples, but we should be able to:

1. unpacking
2. indexing

Indexing into a tuple is technically possible today, although the syntax at the moment is `std::get<0>(tuple)`, which is decidedly unlike any other indexing syntax in this or any other language. `boost::tuple` at least supports `tuple.get<0>()`, which puts the index last.

Unpacking is also technically possible today, by way of `std::apply`, but is extremely unergonomic to say the least.

## Tuples `<=>` Packs

The syntax for indexing into a tuple and unpacking a tuple (that is, turning a tuple into an unexpanded pack referring to the elements of the tuple) should mirror the syntax for dealing with packs. That is:

::: cmptable
### Pack Syntax
```cpp
$pack$...[0]
$pack$...[..]...
$pack$...[1..]...
```
### Tuple Syntax
```cpp
$tuple$.[0]
$tuple$.[..]...;
$tuple$.[1..]...;
```
---

```cpp
$pack$...[0]
$pack$...[:]...
$pack$...[1:]...
```

```cpp
$tuple$.[0];
$tuple$.[:]...;
$tuple$.[1:]...;
```

---

```cpp
$pack$![0]
$pack$![..]...
$pack$![1..]...
```

```cpp
$tuple$.[0];
$tuple$~...;
$tuple$.[1..]...;
```
:::

A few things to note here. While the slicing syntax for the "whole slice" (whether `[..]` or `[:]`) is not especially useful if you're starting from a pack, it is, on the other hand, the most useful thing when dealing with a tuple. Since, when you're unpacking a tuple into a function, typically you'll want to unpack the entire tuple.

Given a syntax for unpacking a tuple, like `$tuple$.[..]...`, there doesn't need to be another syntax on top of that to index into a tuple. Since, once we have a pack, we can index into the pack with `$tuple$.[..]...[0]`. But that feels a bit excessive [^dots], so having a shorthand for this case seems justified.

[^dots]: Six dots to pull one element of a tuple? That's more than a little excessive.

Now, if we have a dedicated postfix operator (`!`) to treat a pack as an object, then it might make sense to have a mirrored postfix operator (`~`) to treat an object as a pack. The same pros and cons here apply: this reduces the number of dots you have to write (which themselves hinder comprehension), but it increases the amount of punctuation required and brings us closer to Perl (which itself hinders comprehension). But if `$tuple$~` gives us a pack (which would be quite useful, as the common case of wanting to unpack a tuple is indeed to unpack the _entire_ tuple, so having a short marker for this is quite nice), then how would you index into that resulting pack? Well, that would have to be `$tuple$~![0]` and `$tuple$~![1..]...`. But that's just awkward (we take our tuple, turn it into a pack, then turn it back into an object?), so I think it's worth still resorting to `$tuple$.[0]` in this case anyway. But `$tuple$~` is worth considering nevertheless.

Here's a concrete example of the difference between the `...[0]` and `![0]` indexing syntaxes for a tuple implementation that also does unpacking (imagine a `product` function taking a pack of integers and returning its product), using the terser `$tuple$~` form to unpack a tuple:

::: cmptable
### Indexing with `...[0]`
```cpp
template <typename... Ts>
class tuple {
  Ts... elems;
public:
  tuple(Ts... ts) : elems(ts)... { }

  using ...tuple_element = Ts;

  template <size_t I>
  auto get() const& -> Ts...[I] const& {
    return elems...[I];
  }
};

int main() {
  tuple vals(1, 2, 3, 4);
  assert(vals.[0] + vals.[1] + vals.[2] + vals.[3] == 10);

  assert(product(vals.[..]...) == 24);
  assert(product(vals.[2..]...) == 6);
}
```

### Indexing with `![0]`
```cpp
template <typename... Ts>
class tuple {
  Ts... elems;
public:
  tuple(Ts... ts) : elems(ts)... { }

  using ...tuple_element = Ts;

  template <size_t I>
  auto get() const& -> Ts![I] const& {
    return elems![I];
  }
};

int main() {
  tuple vals(1, 2, 3, 4);
  assert(vals.[0] + vals.[1] + vals.[2] + vals.[3] == 10);

  assert(product(vals~...) == 24);
  assert(product(vals.[2..]...) == 6);

}
```
:::

## Unified Operations

Treating a tuple as a pack is closely tied in with slicing a pack, so considering these operations together makes a lot of sense. I don't think they are meaningfully separable, as they have to inform each other.

And part of the value of having a dedicated syntax for pack/tuple indexing, pack slicing, and tuple unpacking, is precisely that these syntaxes can mirror each other... and the indexing operations for packs and tuples share a syntax with the indexing operations for other kinds. The slice syntax could be used in other contexts (perhaps `1..3` can be an expression of value `std::slice(1, 3, 1)` [^std_slice]?).

One of the push-backs against these facilities is that a hypothetical reflection facility could address these use-cases. And that's probably true, we could add different functions for each of these cases. But then we'd end up with differently named functions - losing the symmetry. I think that would be unfortunate.

## Nested Packs

[@P1858R2] had a whole section on [nested pack expansions](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2020/p1858r2.html#nested-pack-expansions). The model that paper suggested was that the `.[:]` operator, when applied to a tuple, would add a layer of packness. In the normal case, we go from "not a pack" to "a pack," but if we have a pack of tuples, we could then go to a pack of packs. The paper contained this example:

::: bq
```cpp
template <typename... Ts>
void foo(Ts... e) {
    bar(e.[:]... ...);
}

// what does this do?
foo(xstd::tuple{1}, xstd::tuple{2, 3});
```
:::

And suggested that this calls `bar(1, 2, 3)`.

That's... cool. But it adds an unbelievable level of complexity that I don't think is really worth it, and surely necessitates a better syntax. Like having proper list comprehensions? So I don't think the above example should be valid, just for the forseeable future.

[^std_slice]: Yeah, `std::slice` exists.

## Table of Disambiguation

[@P1858R2] included a table of the various kinds of member-access expansions that could occur of the form `e.f`, where either `e` or `f` could be a pack, a tuple, or a simple object. Adjusted for the syntax presented here, and removing support for nested packs, that table now looks as follows.

If we use `...[0]` for pack indexing and `.[..]` for tuple unpacking:

<table>
<tr><th /><th>`e` is a Pack</th><th>`e` is a Tuple</th><th>`e` is not expanded</th></tr>
<tr>
<th>`f` is a Pack</th>
<td style="text-align:center">not possible</td>
<td style="text-align:center">not possible</td>
<td>`foo(e. ...f...);`</td>
</tr>
<tr>
<th>`f` is a Tuple</th>
<td style="text-align:center">not possible</td>
<td style="text-align:center">not possible</td>
<td>`foo(e.f.[..]...);`</td>
</tr>
<tr>
<th>`f` is not expanded</th>
<td>`foo(e.f...);`</td>
<td>`foo(e.[..].f...);`</td>
<td>`foo(e.f);`</td>
</table>

If we use `![0]` for pack indexing and `~` for tuple unpacking:

<table>
<tr><th /><th>`e` is a Pack</th><th>`e` is a Tuple</th><th>`e` is not expanded</th></tr>
<tr>
<th>`f` is a Pack</th>
<td style="text-align:center">not possible</td>
<td style="text-align:center">not possible</td>
<td>`foo(e.pack!(f)...);`</td>
</tr>
<tr>
<th>`f` is a Tuple</th>
<td style="text-align:center">not possible</td>
<td style="text-align:center">not possible</td>
<td>`foo(e.f~...);`</td>
</tr>
<tr>
<th>`f` is not expanded</th>
<td>`foo(e.f...);`</td>
<td>`foo(e~.f...);`</td>
<td>`foo(e.f);`</td>
</table>

With either choice of syntax, the table is significantly reduced, since we no longer have to deal with the question of layering packs.

# Proposal

I think the right set of functionality to have is:

* the ability to declare member packs, alias packs, and local variable packs
* the ability to index and slice into a pack, including from the back
* the ability to index into and unpack a tuple

I think there are two good choices of syntax here for indexing, slicing, and unpacking:

<table>
<tr><th/><th>Option 1</th><th>Option 2</th></tr>
<tr><th>Pack Indexing (first element)</th><td>`$pack$...[0]`</td><td>`$pack$![0]`</td></tr>
<tr><th>Pack Indexing (last element)</th><td>`$pack$...[^1]`</td><td>`$pack$![^1]`</td></tr>
<tr><th>Pack Slicing</th><td>`$pack$...[1..]...`</td><td>`$pack$![1..]...`</td></tr>
<tr><th>Tuple Indexing</th><td>`$tuple$.[0]`</td><td>`$tuple$.[0]`</td></tr>
<tr><th>Tuple Unpacking</th><td>`$tuple$.[1..]...`</td><td>`$tuple$.[1..]...`</td></tr>
<tr><th>Full Tuple Unpacking</th><td>`$tuple$.[..]...`</td><td>`$tuple$~...`</td></tr>
<tr><th>Dependent Member Pack</th><td>`obj. ...elems`</td><td>`obj.pack!(elems)`</td></tr>
</table>

This proposal deals exclusively with syntax. The semantics of what some of these options mean (in particular, how does tuple indexing evaluate) was discussed in [@P1858R2].
