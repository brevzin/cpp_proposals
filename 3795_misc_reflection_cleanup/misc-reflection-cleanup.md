---
title: "Miscellaneous Reflection Cleanup"
document: P3795R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: reflection
---

# Introduction

At the Sofia meeting, [@P2996R13]{.title}, [@P3394R4]{.title}, [@P3293R3]{.title}, [@P3491R3]{.title}, [@P3096R12]{.title}, and [@P3560R2]{.title} were all adopted. Because these were all (somewhat) independent papers that were adopted at the same time, there were a few inconsistencies that were introduced. Some gaps in coverage. Some inconsistent APIs. This papers just seeks to correct a bunch of those little issues.

# Proposal

This isn't really one proposal, per se, as much as it is a number of very small proposals.

## Missing Predicates

[@P2996R13] introduces a lot of unary predicates to help identify what a reflection represents. But there are a few simple ones that are missing:

* `is_inline(r)` — for identifying inline namespaces, and variables and functions declares inline (implicitly or explicitly)
* `is_constexpr(r)` - for identifying functions and variables declared `constexpr`
* `is_consteval(r)` - for identifying functions (and maybe eventually variables) declared `consteval`

These are all pretty simple predicates.

Notably, I'm suggesting `is_consteval` and not `is_immediate_function`. That's because you always know, even from inside of a function, whether or not it is _declared_ `consteval`. But some constexpr functions can escalate — so what answer do you give from inside of a function that you're asking about? Moreover, we don't even have a good way of erroring in that context, since by making such a check either non-constant (or throw), we might cause the function to escalate, which may change its behavior. It's a complicated question, that needs to be carefully considered, whereas these three predicates are simple.

## Scope identification

[@P2996R13]'s mechanism for access checking relies on `std::meta::access_context`. However, that facility exposes another interesting bit of functionality — albeit in a very indirect way. You can identify what class you're currently in:

::: std
```cpp
consteval auto current_class(std::meta::info scope = std::meta::access_context::current().scope())
  -> std::meta::info
{
    while (true) {
        if (is_type(scope)) {
            return scope;
        }

        if (is_namespace(scope)) {
            throw std::meta::exception(/* ... */);
        }

        scope = parent_of(scope);
    }
}
```
:::

Given that this is a useful (and occasionally asked for) piece of information, we should just provide it directly, rather than having this API proliferate.

The same is true for `current_function` and `current_namespace`. The only open question in my mind is whether there are situations in which you'd want something like a `nearest_enclosing_class_or_namespace`.

## `data_member_spec` API

The current API for `std::meta::define_aggregate` requires calls to `std::meta::data_member_spec`, which currently looks like this:

::: std
```cpp
consteval info data_member_spec(info type, data_member_options options);
```
:::

When we originally proposed this API, we allowed the _name_ of a non-static data member to be omitted. Doing so was akin to asking the implementation to create a unique name for you. However, that  changed in [@P2996R8] such that if `options.name` were not provided then the data member had to be an unnamed bit-field (which meant that `options.width` had to be provided). That means that while we originally envisioned it being possible to implement `tuple` like this:

::: std
```cpp
consteval {
  define_aggregate(
      ^^storage,
      {data_member_spec(^^Ts)...}
  );
}
```
:::

That's no longer actually possible. You always have to provide _some_ member of `options`. So `tuple` looks more like this (note that you're allowed to have multiple mambers named `_` now):

::: std
```cpp
consteval {
  define_aggregate(
      ^^storage,
      {data_member_spec(^^Ts, {.name="_"})...}
  );
}
```
:::

As such, it looks a little odd to have the type off by itself like that. It's true that you _always_ need to provide a type, but why is it special? Let's face it, approximately nobody is going to be creating unnamed bit-fields, so the name is practically always present too.

Let's just make the API more uniform:

::: std
```diff
  struct data_member_options {
    struct $name-type$ { // exposition only
      // ...
    };

+   info type;
    optional<$name-type$> name;
    optional<int> alignment;
    optional<int> bit_width;
    bool no_unique_address = false;
  };

- consteval info data_member_spec(info type, data_member_options options);
+ consteval info data_member_spec(data_member_options options);
```
:::

Additionally, while adding _attributes_ is a difficult question, adding _annotations_ is actually much simpler. With the adoption of [@P3394R4], we should also allow you to add annotations to your generated members.

So the full change is really:

::: std
```diff
  struct data_member_options {
    struct $name-type$ { // exposition only
      // ...
    };

+   info type;
    optional<$name-type$> name;
    optional<int> alignment;
    optional<int> bit_width;
    bool no_unique_address = false;
+   vector<info> annotations = {};
  };

- consteval info data_member_spec(info type, data_member_options options);
+ consteval info data_member_spec(data_member_options options);
```
:::

## Annotations on Function Parameters

[@P3394R4]{.title} and [@P3096R12]{.title} were independent proposals adopted at the same time. The former gave us annotations, but the latter gave us the ability to introspect function parameters. Neither could really, directly add support for adding annotations onto function parameters. So, as a result, we don't have them.

But there isn't any particular reason why we shouldn't support this:

::: std
```cpp
auto f([[=1]] int x) -> void;

constexpr auto a = annotations_of(
  parameters_of(^^f)[0]
)[0];
static_assert([: constant_of(a) :] == 1);
```
:::

## Missing metafunctions

[@P1317R2]{.title} was also adopted in Sofia. It added three new type traits, but none of us were aware of that paper, much less expected it to be adopted, so we neglected to add consteval metafunction equivalents to those three type traits:

::: std
```cpp
consteval bool is_applicable_type(info fn, info tuple);
consteval bool is_nothrow_applicable_type(info fn, info tuple);
consteval info apply_result(info fn, info tuple);
```
:::

## Inconsistent Error-Handling API

[@P2996R13]'s approach to error-handling was to add a "Constant When" specification to every function. Failing to meet that condition resulted in failing to be a constant expression.

Every other paper in the reflection constellation followed that same path.

However, [@P3560R2] changes the error-handling approach to instead throw an object of type `std::meta::exception`. Its wording changed most of the functions in [@P2996R13] — but it both did not change the error-handling for the type traits and it also neglected to be clairvoyant enough to change the error-handling for all of the other functions added by all of the other reflection papers.

That is:

|Paper|Functions|
|-|-|
[@P3394R4]|`annotations_of` and `annotations_of_with_type`|
|[@P3293R3]|`subobjects_of`|
|[@P3491R3]|`reflect_constant_array`|
|[@P3096R12]|`parameters_of`, `variable_of`, and `return_type_of`|

All of these functions should just `throw` as well. That's a pretty straightforward wording change.

# Wording

TBD

