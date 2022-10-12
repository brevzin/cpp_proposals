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

# More Functionality for Packs

There are three things that we can't easily do with packs, but should be able to:

1. indexing
2. slicing (i.e. producing another pack out of the original one)
3. iterating (covered separately by [@P1306R1])

## Indexing

The issue with indexing is largely a choice of syntax. We can't use `$pack$[0]` because of potential ambiguity if this appears in a pack expansion: do we want the first element of the pack, or do we want the first element of every element of the pack?

That leaves introducing some new syntax that is distinct. I think the options are here `$pack$.[0]` (as [@N4235] suggested) or `$pack$...[0]` (as [@P1858R2] suggested).

A different option would be something like `$OBJECT$($pack$)[0]`, where `$OBJECT$` is some kind of annotation that indicates treating the pack as an object, which is then indexed into like a normal object. I'm not sure that this would avoid the issues of ambiguity if this expression itself appears in a pack expansion (e.g. imagine wanting to add the first element of a pack to every element of the pack) and I can't think of an approach that I would consider better than the early ones that just use one or three dots.

Between `$pack$.[0]` and `$pack$...[0]`, I think the latter is better for two reasons:

1. It does seem like operations with packs should just use `...`, this one is no different
2. It looks like shorthand for `tuple($pack$...)[0]`. The latter isn't valid yet (would require constexpr function parameters to be valid), but the similarity between the two expressions seems compelling to me anyway.

Note that this is a frequently desired utility, and there is a whole talk at CppNow specifically about how to efficiently index into a pack: [The Nth Element: A Case Study](https://www.youtube.com/watch?v=LfOh0DwTP00) [^indexing]. Also note that Circle implements this syntax.

[^indexing]: Spoiler alert, having a dedicated language feature doesn't just look much better, it also compiles tremendously faster.

### Type Indexing

Pack indexing shouldn't just work for a pack of values, it should also work for a pack of types. That is:

::: bq
```cpp
template <typename... Ts>
auto first(Ts... ts) -> Ts...[0] {
  return ts...[0];
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

I like the C# approach:

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

* `x[from:to]`
* `x[from..to]`

The advantage of the former is that it uses less periods. But also that it also can be extended by another argument as `x[from:to:stride]` (e.g. `x[::2]` takes every other element).

The advantage of the latter is that it's probably more viable to in a for loop (e.g. `for (int i : 0..10)`, since having the extra colon would be fairly awkward) and it can be extended by simply adding more groupings (e.g. `x[0..4, 8..10]` or `x[1, 2, 8..]`) [^dslice]. Although the interesting downside here is `x[1, 3..]`: does this mean indices 1, 3, 4, 5, 6, ... (as it does in D) or 1, 3, 5, 7, 9, ... (as it does in Haskell)? The latter is probably both more useful and confusing.

[^dslice]: D's slice overloading is [fairly involved](https://dlang.org/spec/operatoroverloading.html#slice) but it's worth a look.

Regardless of which syntax to choose, the arguments about [indexing from the back](#indexing-from-the-back) still apply, as well as the syntax for how to slice a pack. If we're going to index into a pack via `$pack$...[0]` then we should likewise slice a pack via `$pack$...[1:]` (or `$pack$...[1..]`). And what we end up with, at this point is... still a pack. So it would need to be expanded. For instance, one (not-great) way of writing `sum` might be:

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

`ts...[0]` is the first element of the pack. Then, `ts...[1..]` is a slice that starts from the second element and goes to the end - which is still a pack, so it is then expanded into `sum`.

Slicing, while not as important as indexing, still is an operation that regularly comes up, so I think it would be important to support. Whichever syntax we choose for slicing a pack could also be used to slice other objects as well. If I have some `s` that is a `span<T>`, `s[1..]` could conceivably be made to work (and evaluate basically as `s.subspan(1)`, which we have today).

## Summary

The two pack operations suggested here both have similar syntax:

* `$pack$...[0]` gives me the first element
* `$pack$...[1..]` gives me every element starting with the second, and is still a pack

I think this syntax works pretty well, and is consistent with how both packs and indexing work today.

Importantly, `$pack$` in the above should be an identifier that denotes a pack, not an arbitrary expression. Partially this avoids the question of: in `f(x)...[0]`, how many times is `f` invoked? BUt also because it's not strictly necessary anyway. If I want to invoke `f` on the first element, I could do `f(x...[0])`. If I wanted to invoke `f` on all the elements starting from the second, I don't need `f(x)...[1..]...`, I can just `f(x...[1..])...` It's the same amount of characters in both cases anyway, so there's really no reason to have to get complicated here.

# More Functionality for Tuples

