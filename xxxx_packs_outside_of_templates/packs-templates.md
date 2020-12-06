---
title: "Packs outside of Templates"
document: DxxxxR0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

There are several papers currently in-flight which introduce or rely on a facility that C++ does not currently have: the ability to use packs outside of templates. Those papers are:

- [@P1061R1] Structured Bindings can introduce a Pack
- [@P1858R2] Generalized pack declaration and usage
- [@P1240R1] Scalable Reflection in C++
- [@P2237R0] Metaprogramming

The first two of these are papers in which I am an author and deal exclusively with being able to introduce and use packs in more places (as the titles might suggest). The latter two are broader papers about introducing reflection into C++, in which packs play a smaller role overall, though the question comes up.

Here is a short example which uses facilities from all four papers and demonstrates the issue:

```{.cpp .numberLines}
template <typename... Ts>
struct simple_tuple {
    // from P1858
    Ts... elems;
};

int g(int);

void f(simple_tuple<int, int> xs) {
    // from P1061
    auto& [...a] = xs;
    int sum_squares = (0 + ... + a * a);
    
    // from P1858
    int product = (1 * ... * g(xs.elems));
    
    // from P1240, construct a reflection-range of the template parameters
    // this should be a vector containing two reflections of 'int'
    constexpr auto params = std::meta::parameters_of(reflexpr(decltype(xs)));
    
    // from P1240: decltype(ys) is simple_tuple<int, int>
    simple_tuple<typename(...params)> ys = xs;
    
    // from P2237: decltype(zs) is simple_tuple<int, int>
    simple_tuple<|params|...> zs = xs;
}
```

`f` here is _not_ a template, yet we see five places where we're using a pack in some way (on lines 9, 10, 13, 20, and 23).

While the two reflection-based usages look similar, there's a significant distinction between the two. In [@P1240R1], the `typename(...range)` transformation may look like a pack expansion, but it is not. We do not have access to each underlying element of the pack to be able to do further transformations on. But with [@P2237R0], we do have a proper pack expansion, and can make such transformations we please. For example, if we wanted to make a `simple_tuple<int&, int&>` that refers to the underlying elements of `xs`, that involves taking the template parameters range (`params` from the above example) and adding an lvalue reference to each type. This is how we would do it with the two papers:

```cpp
// with P1240
constexpr auto refs = params
                    | std::views::transform(std::meta::add_lvalue_reference);
simple_tuple<typename(...refs)> ys_ref{a...};

// with P2237
simple_tuple<|params|&...> zs_ref{a...};
```

# Paper Response

[@P1061R1] was last seen in Belfast, where Evolution approved the direction 12-5-2-0-1.

[@P1858R2] was discussed discussed on a telecon in October 2020. That paper is larger in scope and consists basically of three different sections, but the only relevant section as far as this paper is concerned is the question of exploring packs in more places (as in the example earlier). That section was received very favorably: 9-13-2-1-1. Other papers of the paper had less support (pack indexing slightly less at 7-11-6-2-0, and packifying much less so at 3-7-8-6-1), but the ability to simply have packs in more places is fundamental in that paper and already directly exposes having packs outside of templates.

In both cases, the source of the opposition is the same: the question about the implementation effort (and to a lesser extent, the specification effort) necessary to make this work. Implementations currently only have their pack-related machinery present in template contexts (not surprisingly, as that is the only place where packs have been able to appear since their inception), and it would take a large amount of work to be able to support the kind of ad hoc pack operations as demonstrated in the above example.

[@P1240R1] avoids this problem by only allowing very specific kinds of expansion, as demonstrated earlier. If we have a reflection-range of types, we can explode it into a template with those specific types as parameters. But that's it. [@P2237R0] does not have this restriction, and so runs into the same issue as my papers: implementation complexity.

# What Do We Do About It?

I think there are three choices we can make about how to address this problem, but we have to make the decision holistically.

1. We allow packs outside of templates, understanding the necessary effort to make that work.

2. We allow packs outside of templates, but do not allow arbitrary patterns in pack expansions. This is the [@P1240R1] approach. This means that neither of the fold-expressions on lines 12 or 15 would be valid, but perhaps you could write `(0 + ... + a)` and `(1 * ... * xs.elems)`.

3. We do not allow packs outside of templates at all. If you want to use packs, you need to have a template.

I think we should choose Option 1 (my preference here is probably not surprising, given that I've written two papers on extending pack usage). It's useful functionality to have, and from a language perspective, it seems to make a lot of sense. Such examples arguably have very clear meaning.

Not all code is templates. People do still, on occasion, write functions. Some problems lead themselves very well to pack expansion and we should give users all the tools available to solve their problems.

Barring that, I am highly skeptical of Option 2. Allowing some-but-not-all pack functionality outside of templates seems like it could either lead to a set of rules that would be hard to understand, or it leads to the kinds of workarounds that look silly at best:

```cpp
// we disallow this
int sum_squares_bad = (0 + ... + a * a);

// ... but allow this?
int sum_good = (0 + ... + a);

// ... and this?
int sum_squares_good = [](auto... b){
    return (0 + ... + b * b);
}(a...);
```

Then again, Option 3 leads to precisely the same kind of workarounds:

```cpp
// we disallow this
int product_bad = (1 * ... * xs.elems);

// ... but allow this
int product_good = [](auto& xs){
    // need to disambiguate here, since now
    // xs.elems is dependent
    return (1 * ... * xs. ...elems);
}(xs);
```

Which are precisely the sorts of workarounds that [@P1061R1] and [@P1858R2] sought to avoid to begin with. On the plus side, we would not need such workarounds in templates &mdash; just non-templates.

# Proposal

This paper isn't a proposal in of itself. We already have four different proposals in front of us solving four different problems. 

But those proposals between them expose a common new language feature: the ability to have and use packs outside of templates. 

The problem is: this is a difficult piece of functionality for implementations.

The question we have to answer is: Does the benefit of packs outside of templates (arguably fewer rules to think about and more straightforward code to write) outweigh the cost (large implementation effort)? Or is the trade-off not there &mdash; we would rather invest implementation effort on _other_ features even if it means that users will have to write the kinds of silly workarounds presented earlier? 

While I have my personal preference (easy for me to claim, as not an implementor), I do think it would be valuable for Evolution to provide clear direction on this question earlier rather than later.
