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

In both cases, the source of opposition was implementation concern, along two axes:

* implementation effort (and to a lesser extent, specification effort)
* compilation latency

The first bullet point speaks for itself, but the second bears some elaboration. Right now, in order to handle packs properly, implementations have to cache expressions as they go. Since a `...` can appear arbitrarily late in an expression, and is only a suffix rather than a prefix, compilers just don't know if something will eventually become a pack expansion or not. This machinery only has to exist in templates today, since that's the only place where we can get pack expansions today.

But if we added packs outside of templates, suddenly every function anywhere could potentially have a pack expansion in it, and compilers need to be able to handle that, which means more work. Even existing C++20 code that does not use any of these features could see its compile times increase. 

[@P1240R1] avoids this problem by only allowing very specific kinds of expansion, as demonstrated earlier. Notably, those specific kinds of expansions have identifiable prefixes, so that compilers can know in advance that a pack expansion is coming, and won't have additional overhead for parsing regular functions that don't use the new features. 

If we have a reflection-range of types, with [@P1240R1] we can explode it into a template with those specific types as parameters. But that's it. Any deviation from specific prescribes shapes requires more work from the user.

[@P2237R0] does not have this restriction, and so runs into the same issue as my papers: implementation complexity and added compilation latency. 

# What Do We Do About It?

I think there are three choices we can make about how to address this problem, but we have to make the decision holistically.

1. We allow packs outside of templates, understanding the necessary cost (both in time and latency) to make that work.

2. We allow packs outside of templates, but come up with a pack-expansion prefix or something to that effect. The prefixed expansion would be valid in both templates and non-templates, but only the suffixed expansion would be valid in templates. And encourage people to start using prefixed expansion for compiler latency purposes. 

3. We do not allow packs outside of templates at all. If you want to use packs, you need to have a template.

I think we should choose Option 1 (my preference here is probably not surprising, given that I've written two papers on extending pack usage). It's useful functionality to have, and from a language perspective, it seems to make a lot of sense. Such examples arguably have very clear meaning.

Not all code is templates. People do still, on occasion, I am told, write functions. Some problems lead themselves very well to pack expansion and we should give users all the tools available to solve their problems.

Barring that, Option 2 would kind of be jarringly weird. The obvious choice is probably just prefix ellipsis, so we end up with like:

```cpp
void some_func(int, int);

void f(simple_tuple<int, int> xs) {
    some_func(... xs.elems);
}

template <typename... Ts>
void g(Ts... elems) {
    some_func(elems ...);
}
```

Same idea, two syntaxes? Delightful. Way better than one syntax, two ideas though. 

Option 3 leads silly looking workarounds:

```cpp
void f(simple_tuple<int, int> xs) {
    // we disallow this
    int product_bad = (1 * ... * xs.elems);

    // ... but allow this
    int product_good = [](auto& xs){
        // need to disambiguate here, since now
        // xs.elems is dependent
        return (1 * ... * xs. ...elems);
    }(xs);
}
```

Which are precisely the sorts of workarounds that [@P1061R1] and [@P1858R2] sought to avoid to begin with. On the plus side, we would not need such workarounds in templates &mdash; just non-templates.

# Proposal

This paper isn't a proposal in of itself. We already have four different proposals in front of us solving four different problems. 

But those proposals between them expose a common new language feature: the ability to have and use packs outside of templates. 

The problem is: this is a difficult piece of functionality for implementations.

The question we have to answer is: Does the benefit of packs outside of templates (arguably fewer rules to think about and more straightforward code to write) outweigh the cost (large implementation effort and likely compile-time performance hit)? Or is the trade-off not there &mdash; we would rather invest implementation effort on _other_ features even if it means that users will have to write the kinds of silly workarounds presented earlier? 

While I have my personal preference (easy for me to claim, as not an implementor), I do think it would be valuable for Evolution to provide clear direction on this question earlier rather than later.
