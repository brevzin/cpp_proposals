---
title: "On the Naming of Packs"
document: P2994R1
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: abandoned
---

<style type="text/css">
span.yellow {
    background-color: #ffff00;
}
</style>

# Revision History

Changed the proposal because it doesn't... actually work.

# Introduction

C++11 introduced variadic templates, which dramatically changed the way we write all sorts of code. Even if the only thing you can do with a parameter pack was expand it. C++17 extended this support to fold-expressions (and there's ongoing work to extend fold-expressions in a few directions [@P2355R1] or [@circle-recurrence]), but we still don't have any operations that apply to the pack itself - as opposed to consuming the whole thing.

It's tricky to add support for this, because we have to be careful that the syntax for applying an operation on the pack itself is distinct from the syntax from applying that operation on an element, otherwise we could end up with ambiguities. For example, with indexing, we cannot use `pack[0]` to be the first element in the pack, because something like `f(pack + pack[0]...)` is already a valid expression that cannot mean adding the first element of the pack to every element in the pack.

As such, all the ongoing efforts to add more pack functionality choose a distinct syntax. Consider the following table, where `elem` denotes a single variable and `pack` denotes a function parameter pack:

<table>
<tr><th/><th>Single Element</th><th>Pack</th></tr>
<tr><th style="vertical-align:middle;">Indexing [@P2662R2]</th><td>
```cpp
elem[0]
```
</td><td>
```cpp
pack...[0]
```
</td></tr>
<tr><th style="vertical-align:middle;">Expansion Statement[^exp] [@P1306R1]</th><td style="vertical-align:middle;">
```cpp
template for (auto x : elem)
```
</td><td >
One of:
```cpp
template for (auto x : {pack...})
template for ... (auto x : pack)
for ... (auto x : pack)
```
</td></tr>
<tr><th style="vertical-align:middle;">Reflection [@P1240R2]</th><td>
```
^elem
```
</td><td style="vertical-align:middle;">🤷</td></tr>
<tr><th style="vertical-align:middle;">Splice [@P1240R2]</th><td>
```cpp
[: elem :]
```
</td><td>
```cpp
... [: pack :] ...
```
</td></tr>
</table>

As you can see, the syntaxes on the right are distinct from the syntaxes on the left - which is important for those syntaxes to be viable.

What is unfortunate, in my opinion, is that the syntaxes on the right are also all distinct from each other. We don't have orthogonality here - knowing how to perform an operation on an element doesn't necessarily inform you of the syntax to perform that operation on a pack. There's an extra `...` that you have to type, but where does it go? It depends.

Additionally, while the indexing syntax here, `pack...[0]`, works just fine for indexing, it can't be generalized to the other operations:

* `template for (auto x : pack...)` looks like a pack expansion, which would then suggest that `template for (auto x : a, b)` is valid too. But this leads us to conflicting meanings: are we iterating over each element listed (as we want for a pack) or are we iterating over the sub-elements of the one element listed (as we want for a tuple). These conflict over what `template for (auto x : a)` mean.
* `^pack...` is a pack expansion of a reflection, as in `vector{^pack...}` producing a `vector<meta::info>` with the reflections over all the elements of `pack`.`

While having all the functionality available to us in the various proposals would be great, I think we can do better.

Note that lambda capture isn't included in the above table, since `[pack...]` is basically a regular pack expansion and `[...pack1=pack2]` is introducing a new pack, which is a slightly different operation than what I'm talking about here.

[^exp]: For expansion statements, even though we've agreed on the `template for` syntax, there does not appear to be a published document that uses that syntax. Also, the last revision doesn't have support for expanding over a pack due to the lack of syntax - the three options presented here are various ideas that have come up in various conversations with people.

# An Idea

I would like to suggest that rather than coming up with a bespoke syntax for every pack operation we need to do - that we instead come up with a bespoke syntax for _naming a pack_ [^name] and use _that_ syntax for each operation. This gives us orthogonality and consistency.

[^name]: What I mean by naming a pack here is basically using the pack as an operand in some function directly, as opposed to being part of a pack expansion. I think of this as naming the pack itself. Maybe referring to this problem as having a pack operand might be more to the point.

Thankfully, we don't even really need to come up with what the syntax should be for naming a pack - we already kind of have it: `...pack`. This is basically the way that packs are introduced in various templates and lambda init-capture already.

Applying that syntax to each of the facilities presented in the previous section:


<table>
<tr><th/><th>Single Element</th><th>Pack (Previous)</th><th>Pack (Proposed)</th></tr>
<tr><th style="vertical-align:middle;">Indexing</th><td>
```cpp
elem[0]
```
</td><td>
```cpp
pack...[0]
```
</td><td>
```cpp
@[\.\.\.pack]{.yellow}@[0]
```
</td></tr>
<tr><th style="vertical-align:middle;">Expansion Statement</th><td style="vertical-align:middle;">
```cpp
template for (auto x : elem)
```
</td><td >
One of:
```cpp
template for (auto x : {pack...})
template for ... (auto x : pack)
for ... (auto x : pack)
```
</td><td style="vertical-align:middle;">
```cpp
template for (auto x : @[\.\.\.pack]{.yellow}@)
```
</td></tr>
<tr><th style="vertical-align:middle;">Reflection</th><td>
```
^elem
```
</td><td style="vertical-align:middle;">🤷</td><td>
```cpp
^@[\.\.\.pack]{.yellow}@
```
</td></tr>
<tr><th style="vertical-align:middle;">Splice</th><td>
```cpp
[: elem :]
```
</td><td>
```cpp
... [: pack :] ...
```
</td><td>
```cpp
[: @[\.\.\.pack]{.yellow}@ :] ...
```
</td></tr>
</table>

Here, all the syntaxes in the last column are the same as the syntaxes in the first column, except using `...pack` instead of `elem`. These syntaxes are all distinct and unambiguous.

It is pretty likely that many people will prefer at least one syntax in the second column to its corresponding suggestion in the third column (I certainly do). But, on the whole, I think the consistency and orthogonality outweigh these small preferences.

Incidentally, this syntax also avoids the potential ambiguity mentioned in [@P2662R2] for its syntax proposal (that having a parameter of `T...[1]` is valid syntax today, which with this proposal for indexing into the pack would instead be spelled `...T[1]` which is not valid today), although given that two compilers don't even support that syntax today makes this a very minor, footnote-level advantage.

Or at least, I thought that this was a pretty good idea. But it does run into one very unfortunate problem. What does this mean:

::: bq
```cpp
template for (auto x : ...pack[0])
```
:::

There are two possible interpretations:

1. This is a tuple-expansion over the tuple that is the 1st element from `pack` (`...pack[0]` is indexing into `pack`)
2. This is a pack-expansion over the unexpanded pack expression `pack[0]` (i.e. `p[0]` for each `p` in `pack`).

Now, you could come up with some way to disambiguate one of these over the other, where the other syntax requires parentheses. But requiring a disambiguation completely defeats the purpose of coming up with unique syntax! We can't have nice things because there are no nice things.


As a result, since I don't think there actually is a viable uniform syntax option, we're left with several different choices for each operation.

## Pack Indexing

1. `pack...[0]`
1. `...pack[0]`

Here, given the lack of uniform syntax option, there doesn't seem to be any benefit to change from what's in the working draft. The only advantage I thought (2) had was the potential for uniformity. Pack indexing is a kind of expansion in a way, so the existing syntax makes sense.

## Pack Expansion

1. `template for (auto x : ...pack)`
1. `template for (auto x : {pack...})`
1. `template for ... (auto x : pack)`
1. `for ... (auto x : pack)`

Here, I think sticking with `template for` is better than not. Both the tuple-expansion and pack-expansion forms do expansion, both of which might do template instantiation, so having `template for` for both is sensible. The only question is where the dots go.

I think it's also important to discuss order of evaluation here, since:

::: cmptable
### Expansion
```cpp
template for (auto x : ...f(pack)) {
  g(x);
}
```

### Equivalent
```cpp
g(f(pack...[0]));
g(f(pack...[1]));
g(f(pack...[2]));
// ...
```
:::

That is, the expression being pack-expanded over is not evaluated `$N$` times up front, it is evaluated on demand for each iteration. The syntax in (2) - where we write `{pack...}` might suggest otherwise, which is otherwise my only reason to prefer (1) over (2), but it's a weak preference. I prefer (1) and (2) over (3) because the `...` are attached to the pack being expanded.

## Range Splicing

1. `[: ...r :]...`
1. `...[: r :]...`

Here, we need different syntax for splicing a constexpr range of reflections than splicing a single element. And it seems much better to disambiguate the two using the same splice syntax (`[:` and `:]`) but with added ellipses rather than introducing a new kind of splice syntax.

It depends on how you think about splicing a range which syntax works out better. Personally, I prefer `[: ... r :]` since conceptually it seems like we're unpacking the range into the splice operator - rather than splicing the range itself.

# Proposal

This paper proposes that:

* the syntax for pack indexing remain as `pack...[0]`
* the syntax for pack expansion be `template for (auto x : ...pack)`
* the syntax for range splicing be `[: ... r :]` (which produces an unexpanded pack)

This gives us as least a little bit of uniformity, probably as much as we can get.

Since this revision no longer suggests any changes to anything currently in the standard, there is no wording to provide.

---
references:
    - id: circle-recurrence
      citation-label: circle-recurrence
      title: "`[recurrence]`"
      author:
        - family: Sean Baxter
      issued:
        - year: 2023
          month: 05
          day: 02
      URL: https://github.com/seanbaxter/circle/blob/master/new-circle/README.md#recurrence
---
