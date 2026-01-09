---
title: "Miscellaneous Reflection Cleanup"
document: P3795R1
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: reflection
---

# Revision History

Since [@P3795R0], fixing missing wording in `annotations_of` and `data_member_spec` (including annotations), rebasing the wording. No longer pursuing the `is_inline`, `is_constexpr`, `is_consteval` functions, those have been removed from this paper.

# Introduction

At the Sofia meeting, [@P2996R13]{.title}, [@P3394R4]{.title}, [@P3293R3]{.title}, [@P3491R3]{.title}, [@P3096R12]{.title}, and [@P3560R2]{.title} were all adopted. Because these were all (somewhat) independent papers that were adopted at the same time, there were a few inconsistencies that were introduced. Some gaps in coverage. Some inconsistent APIs. This papers just seeks to correct a bunch of those little issues.

# Proposal

This isn't really one proposal, per se, as much as it is a number of very small proposals.

## Missing Predicates

[@P3795R0] proposed adding the predicates `is_inline(r)`, `is_constexpr(r)`, and `is_consteval(r)`. After LEWG discussion in Kona, with questions about what `is_constexpr(r)` actually means and whether it applies to implicitly `constexpr` functions as well as implicitly-declared ones, this proposal is simply no longer pursuing those.

## Scope Identification

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

## Missing Metafunctions

[@P1317R2]{.title} was also adopted in Sofia. It added three new type traits, but none of us were aware of that paper, much less expected it to be adopted, so we neglected to add consteval metafunction equivalents to those three type traits:

::: std
```cpp
consteval bool is_applicable_type(info fn, info tuple);
consteval bool is_nothrow_applicable_type(info fn, info tuple);
consteval info apply_result(info fn, info tuple);
```
:::

## Inconsistent Error-Handling API

This section in [@P3795R0] pointed out how some functions in `std::meta::` still had a *Constant When* specification instead of a *Throws* specification, but that was already resolved directly by the handling of [@P3560R2]. Nothing more is needed here.

## Specifying Error-Handling More Precisely

Currently, [@P3560R2] specifies nothing about the contents of the exceptions being thrown on failure from various reflection functions. That's largely as it should be — we really don't need to specify the message, for instance.

But one thing that `std::meta::exception` gives you is a `from()` accessor that tells you which operation failed. And that we _should_ specify — providing some front-matter sentence that says that when a function or function template `F` in `<meta>` is specified to throw a `meta::exception`, that `from()` is `^^F`.

# Wording

Extend what an annotation can represent in [dcl.attr.annotation]{.sref}:

::: std
[1]{.pnum} An annotation may be applied to any declaration of a type, type alias, variable, function, [function parameter,]{.addu} namespace, enumerator, `$base-specifier$`, or non-static data member.

::: addu
::: note
[*]{.pnum} An annotation on a `$parameter-declaration$` in a function definition applies to both the function parameter and the variable.

::: example
```cpp
void f([[=1]] int x);
void f([[=2]] int y) {
  constexpr info rp = parameters_of(^^f)[0];
  constexpr info ry = variable_of(rp);
  static_assert(ry == ^^y);

  static_assert(annotations_of(rp).size() == 2); // both [1, 2]
  static_assert(annotations_of(ry).size() == 1); // just [2]
}
```
:::
:::
:::

:::

Change [class.mem.general]{.sref} to extend our quintuple to a sextuple:

::: std
[32]{.pnum} A _data member description_ is a [quintuple]{.rm} [sextuple]{.addu} (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$` [, `$ANN$`]{.addu}) describing the potential declaration of a non-static data member where

- [#.#]{.pnum} `$T$` is a type,
- [#.#]{.pnum} `$N$` is an `$identifier$` or ⊥,
- [#.#]{.pnum} `$A$` is an alignment or ⊥,
- [#.#]{.pnum} `$W$` is a bit-field width or ⊥, [and]{.rm}
- [#.#]{.pnum} `$NUA$` is a boolean value[.]{.rm} [, and]{.addu}
- [#.#]{.pnum} [`$ANN$` is a sequence of reflections representing either values or template parameter objects.]{.addu}

Two data member descriptions are equal if each of their respective components are the same [entities]{.rm} [entity]{.addu}, [are]{.rm} the same identifier[s]{.rm}, [have equal values]{.rm} [the same value, the same sequence]{.addu}, or [are]{.rm} both ⊥.

:::

The synopsis change for [meta.syn]{.sref} is:

::: std
```diff
#include <initializer_list>

namespace std::meta {
  using info = decltype(^^::);

  // ...

  // [meta.reflection.access.context], access control context
  struct access_context;

  // [meta.reflection.access.queries], member accessessibility queries
  consteval bool is_accessible(info r, access_context ctx);
  consteval bool has_inaccessible_nonstatic_data_members(info r, access_context ctx);
  consteval bool has_inaccessible_bases(info r, access_context ctx);
  consteval bool has_inaccessible_subobjects(info r, access_context ctx);

+ // [meta.reflection.scope], scope identification
+ consteval info current_function();
+ consteval info current_class();
+ consteval info current_namespace();

  // [meta.reflection.member.queries], reflection member queries
  // ...

  // [meta.reflection.define.aggregate], class definition generation
  struct data_member_options;
- consteval info data_member_spec(info type, data_member_options options);
+ consteval info data_member_spec(data_member_options options);
  consteval bool is_data_member_spec(info r);
  template<reflection_range R = initializer_list<info>>
    consteval info define_aggregate(info type_class, R&&);

  // associated with [meta.unary.cat], primary type categories
  // ...

  // associated with [meta.trans.other], other transformations
  consteval info remove_cvref(info type);
  consteval info decay(info type);
  template<reflection_range R = initializer_list<info>>
    consteval info common_type(R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval info common_reference(R&& type_args);
  consteval info type_underlying_type(info type);
  template<reflection_range R = initializer_list<info>>
    consteval info invoke_result(info type, R&& type_args);
  consteval info unwrap_reference(info type);
  consteval info unwrap_ref_decay(info type);

  consteval size_t tuple_size(info type);
  consteval info tuple_element(size_t index, info type);
+ consteval bool is_applicable_type(info fn, info tuple);
+ consteval bool is_nothrow_applicable_type(info fn, info tuple);
+ consteval info apply_result(info fn, info tuple);

  consteval size_t variant_size(info type);
  consteval info variant_alternative(size_t index, info type);

  consteval strong_ordering type_order(info type_a, info type_b);

  // [meta.reflection.annotation], annotation reflection
  consteval vector<info> annotations_of(info item);
  consteval vector<info> annotations_of_with_type(info item, info type);
}
```
:::

Add to the front matter in [meta.syn]{.sref}:

::: std
[1]{.pnum} Unless otherwise specified, each function, and each specialization of any function template, specified in this header is a designated addressable function ([namespace.std]).

::: addu
[*]{.pnum} When a function or function template specialization `$F$` specified in this header throws a `meta::exception` `$E$`, `$E$.from()` is a reflection representing `$F$` and `$E$.where()` is a `source_location` representing from where the call to `$F$` originated.
:::

[2]{.pnum} The behavior of any function specified in namespace `std::meta` is implementation-defined when a reflection of a construct not otherwise specified by this document is provided as an argument.
:::

Adjust the data member description wording in [meta.reflection.names]{.sref}:

::: std
```cpp
consteval bool has_identifier(info r);
```
[1]{.pnum} *Returns* [...]

* [1.1]{.pnum} [...]
* [1.13]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]); `true` if `N` is not `⊥`. Otherwise, `false`.

```cpp
consteval string_view identifier_of(info r);
consteval u8string_view u8identifier_of(info r);
```

[2]{.pnum} Let `$E$` be [...]

[3]{.pnum} *Returns*: An NTMBS, encoded with E, determined as follows:

* [3.1]{.pnum} [...]
* [3.6]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]); a `string_view` or `u8string_view`, respectively, containing the identifier `N`.
:::

Adjust the data member description wording in [meta.reflection.queries]{.sref}:

::: std
```cpp
consteval info type_of(info r);
```
[2]{.pnum} *Returns*:

* [2.6]{.pnum} Otherwise, for a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]), a reflection of the type `$T$`.

```cpp
consteval bool is_bit_field(info r);
```

[19]{.pnum} *Returns*: `true` if `r` represents a bit-field, or if `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]) for which `$W$` is not ⊥. Otherwise, `false`.
:::

Add the new subclause [meta.reflection.scope] before [meta.reflection.access.context]{.sref}. [The wording for `$eval-point$` (p3)  and `$ctx-scope$` (p4) is moved wholesale from `access_context::current`. Paragraphs 1, 2, and 5 onwards are new]{.draftnote}:

::: std
::: addu
[1]{.pnum} The functions in this subclause retrieve information about where in the program they are invoked.

[#]{.pnum} None of the functions in this subclause is an addressable function ([namespace.std]).

[#]{.pnum} Given a program point `$P$`, let `$eval-point$($P$)` be the following program point:

* [#.#]{.pnum} If a potentially-evaluated subexpression ([intro.execution]) of a default member initializer `$I$` for a member of a class `$C$` ([class.mem.general]) appears at `$P$`, then a point determined as follows:
  * [#.#.#]{.pnum} If an aggregate initialization is using `$I$`, `$eval-point$($Q$)`, where `$Q$` is the point at which that aggregate initialization appears.
  * [#.#.#]{.pnum} Otherwise, if an initialization by an inherited constructor ([class.inhctor.init]) is using `$I$`, a point whose immediate scope is the class scope corresponding to `$C$`.
  * [#.#.#]{.pnum} Otherwise, a point whose immediate scope is the function parameter scope corresponding to the constructor definition that is using `$I$`.
* [#.#]{.pnum} Otherwise, if a potentially-evaluated subexpression of a default argument ([dcl.fct.default]) appears at `$P$`, `$eval-point$($Q$)`, where `$Q$` is the point at which the invocation of the function ([expr.call]) using that default argument appears.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a declaration `$D$`, and `$P$` appears either before the locus of `$D$` or within the trailing `$requires-clause$` of `$D$`, a point whose immediate scope is the innermost scope enclosing the locus of `$D$` that is not a template parameter scope.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a `$lambda-expression$` `$L$` whose `$lambda-introducer$` appears at point `$Q$`, and `$P$` appears either within the `$trailing-return-type$` or the trailing `$requires-clause$` of `$L$`, `$eval-point$($Q$)`.
* [#.#]{.pnum} Otherwise, if the innermost non-block scope enclosing `$P$` is the function parameter scope introduced by a `$consteval-block-declaration$` ([dcl.pre]), a point whose immediate scope is that inhabited by the outermost `$consteval-block-declaration$` `$D$` containing `$P$` such that each scope (if any) that intervenes between `$P$` and the function parameter scope introduced by `$D$` is either
  * [#.#.#]{.pnum} a block scope or
  * [#.#.#]{.pnum} a function parameter scope or lambda scope introduced by a `$consteval-block-declaration$`.
* [#.#]{.pnum} Otherwise, `$P$`.

[#]{.pnum} Given a scope `$S$`, let `$ctx-scope$($S$)` be the following scope:

* [#.#]{.pnum} If `$S$` is a class scope or a namespace scope, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a function parameter scope introduced by the declaration of a function, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a lambda scope introduced by a `$lambda-expression$` `$L$`, the function parameter scope corresponding to the call operator of the closure type for `$L$`.
* [#.#]{.pnum} Otherwise, `$ctx-scope$($S$')` where `$S$'` is the parent scope of `$S$`.

[#]{.pnum} Let `$CURRENT-SCOPE$($P$)` for a point `$P$` be a reflection representing the function, class, or namespace whose corresponding function parameter scope, class scope, or namespace scope, respectively, is `$ctx-scope$($S$)`, where `$S$` is the immediate scope of `$eval-point$($P$)`.

```cpp
consteval info current_function();
```

[#]{.pnum} An invocation of `current_function` that appears at a program point `$P$` is value-dependent ([temp.dep.contexpr]) if `$eval-point$($P$)` is enclosed by a scope corresponding to a templated entity.

[#]{.pnum} Let `$S$` be `$CURRENT-SCOPE$($P$)` where `$P$` is the point at which the invocation of `current_function` lexically appears.

[#]{.pnum} *Throws*: `meta::exception` unless `$S$` represents a function.

[#]{.pnum} *Returns*: `$S$`.

```cpp
consteval info current_class();
```

[#]{.pnum} An invocation of `current_class` that appears at a program point `$P$` is value-dependent ([temp.dep.contexpr]) if `$eval-point$($P$)` is enclosed by a scope corresponding to a templated entity.

[#]{.pnum} Let `$S$` be `$CURRENT-SCOPE$($P$)` where `$P$` is the point at which the invocation of `current_class` lexically appears.

[#]{.pnum} *Throws*: `meta::exception` unless `$S$` represents either a class or a member function.

[#]{.pnum} *Returns*: `$S$` if `$S$` represents a class. Otherwise, `parent_of($S$)`.


```cpp
consteval info current_namespace();
```

[#]{.pnum} An invocation of `current_namespace` that appears at a program point `$P$` is value-dependent ([temp.dep.contexpr]) if `$eval-point$($P$)` is enclosed by a scope corresponding to a templated entity.

[#]{.pnum} Let `$S$` be `$CURRENT-SCOPE$($P$)` where `$P$` is the point at which the invocation of `current_namespace` lexically appears.

[#]{.pnum} *Returns*: `$S$` if `$S$` represents a namespace. Otherwise, a reflection representing the nearest enclosing namespace of the entity represented by `$S$`.

:::
:::

Adjust down the now-moved wording from [meta.reflection.access.context]{.sref}:

::: std
```cpp
static consteval access_context current() noexcept;
```

[5]{.pnum} `current` is not an addressable function ([namespace.std]).

::: rm
[#]{.pnum} Given a program point `$P$`, let `$eval-point$($P$)` be the following program point:

* [#.#]{.pnum} If a potentially-evaluated subexpression ([intro.execution]) of a default member initializer `$I$` for a member of a class `$C$` ([class.mem.general]) appears at `$P$`, then a point determined as follows:
  * [#.#.#]{.pnum} If an aggregate initialization is using `$I$`, `$eval-point$($Q$)`, where `$Q$` is the point at which that aggregate initialization appears.
  * [#.#.#]{.pnum} Otherwise, if an initialization by an inherited constructor ([class.inhctor.init]) is using `$I$`, a point whose immediate scope is the class scope corresponding to `$C$`.
  * [#.#.#]{.pnum} Otherwise, a point whose immediate scope is the function parameter scope corresponding to the constructor definition that is using `$I$`.
* [#.#]{.pnum} Otherwise, if a potentially-evaluated subexpression of a default argument ([dcl.fct.default]) appears at `$P$`, `$eval-point$($Q$)`, where `$Q$` is the point at which the invocation of the function ([expr.call]) using that default argument appears.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a declaration `$D$`, and `$P$` appears either before the locus of `$D$` or within the trailing `$requires-clause$` of `$D$`, a point whose immediate scope is the innermost scope enclosing the locus of `$D$` that is not a template parameter scope.
* [#.#]{.pnum} Otherwise, if the immediate scope of `$P$` is a function parameter scope introduced by a `$lambda-expression$` `$L$` whose `$lambda-introducer$` appears at point `$Q$`, and `$P$` appears either within the `$trailing-return-type$` or the trailing `$requires-clause$` of `$L$`, `$eval-point$($Q$)`.
* [#.#]{.pnum} Otherwise, if the innermost non-block scope enclosing `$P$` is the function parameter scope introduced by a `$consteval-block-declaration$` ([dcl.pre]), a point whose immediate scope is that inhabited by the outermost `$consteval-block-declaration$` `$D$` containing `$P$` such that each scope (if any) that intervenes between `$P$` and the function parameter scope introduced by `$D$` is either
  * [#.#.#]{.pnum} a block scope or
  * [#.#.#]{.pnum} a function parameter scope or lambda scope introduced by a `$consteval-block-declaration$`.
* [#.#]{.pnum} Otherwise, `$P$`.

[#]{.pnum} Given a scope `$S$`, let `$ctx-scope$($S$)` be the following scope:

* [#.#]{.pnum} If `$S$` is a class scope or a namespace scope, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a function parameter scope introduced by the declaration of a function, `$S$`.
* [#.#]{.pnum} Otherwise, if `$S$` is a lambda scope introduced by a `$lambda-expression$` `$L$`, the function parameter scope corresponding to the call operator of the closure type for `$L$`.
* [#.#]{.pnum} Otherwise, `$ctx-scope$($S$')` where `$S$'` is the parent scope of `$S$`.
:::

[#]{.pnum} An invocation of `current` that appears at a program point `$P$` is value-dependent ([temp.dep.contexpr]) if `$eval-point$($P$)` is enclosed by a scope corresponding to a templated entity.

[#]{.pnum} *Returns*: An `access_context` whose designating class is the null reflection and whose scope [represents the function, class, or namespace whose corresponding function parameter scope, class scope, or namespace scope is `$ctx-scope$($S$)`, where `$S$` is the immediate scope of `$eval-point$($P$)` and]{.rm} [is `$CURRENT-SCOPE$($P$)` where]{.addu} `$P$` is the point at which the invocation of `current` lexically appears.
:::

Adjust the data member description wording in [meta.reflection.layout]{.sref}:

::: std
```cpp
consteval size_t size_of(info r);
```
[5]{.pnum} *Returns*:

- [#.#]{.pnum} If `r` represents a non-static data member of type `$T$` or a a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}), or

[6]{.pnum} *Throws*: `meta::exception` unless all of the following conditions are met:

- [#.#]{.pnum} `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member that is not a bit-field, direct base class relationship, or data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]) where `$W$` is ⊥.

```cpp
consteval size_t alignment_of(info r);
```

[7]{.pnum} *Returns*:

* [#.5]{.pnum} Otherwise, `r` represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]). If `$A$` is not ⊥, then the value `$A$`. Otherwise, `alignment_of(^^$T$)`.

[8]{.pnum} *Throws*: `meta::exception` unless all of the following conditions are met:

* [8.1]{.pnum} `dealias(r)` is a reflection of a type, object, variable of non-reference type, non-static data member that is not a bit-field, direct base class relationship, or data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]) where `$W$` is ⊥.

```cpp
consteval size_t bit_size_of(info r);
```

[9]{.pnum} *Returns*:

* [9.2]{.pnum} Otherwise, if r represents a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) and `$W$` is not ⊥, then `$W$`.

:::

Change [meta.reflection.annotation]{.sref}:

[This doesn't work for data member specifications, because those contain constants — not annotations yet. Is it even useful to pull them back out? They're not annotations yet. Do we have to synthesize annotations? It's useful to add annotations to generated data members — I'm not sure if it's useful to query the annotations on a pre-generated data member? There's also a drive-by cleanup of the wording around direct base class relationships, since `$base-specifier$`s aren't technically declared. ]{.draftnote}

::: std
```cpp
consteval vector<info> annotations_of(info item);
```

::: addu
[1]{.pnum} For a function `$F$`, let `$S$($F$)` be the set of declarations, ignoring any explicit instantiations, that declare either `$F$` or a templated function of which `$F$` is a specialization.
:::

::: rm
[1]{.pnum} Let `$E$` be

* [#.#]{.pnum} the corresponding `$base-specifier$` if `item` represents a direct base class relationship,
* [#.#]{.pnum} otherwise, the entity represented by `item`.
:::

[2]{.pnum} *Returns*: a `vector` containing all of the reflections `$R$` representing each annotation applying to [each declaration of `$E$` that]{.rm}:

::: addu
* [#.1]{.pnum} if `item` represents a function parameter `$P$` of a function `$F$`, then the declaration of `$P$` in each declaration of `$F$` in `$S$($F$)`,
* [#.#]{.pnum} otherwise, if `item` represents a function `$F$`, then each declaration of `$F$` in `$S$($F$)`,
* [#.#]{.pnum} otherwise, if `item` represents a direct base class relationship (`$D$`, `$B$`), then the corresponding `$base-specifier$` in the definition of `$D$`,
* [#.#]{.pnum} otherwise, each declaration of the entity represented by `item`,
:::

[such that each specified declaration]{.addu} precedes either some point in the evaluation context ([expr.const]) or a point immediately following the `$class-specifier$` of the outermost class for which such a point is in a complete-class context. For any two reflections `@*R*~1~@` and `@*R*~2~@` in the returned `vector`, if the annotation represented by `@*R*~1~@` precedes the annotation represented by `@*R*~2~@`, then `@*R*~1~@` appears before `@*R*~2~@`. If `@*R*~1~@` and `@*R*~2~@` represent annotations from the same translation unit `T`, any element in the returned `vector` between `@*R*~1~@` and `@*R*~2~@` represents an annotation from `T`.

[The order in which two annotations appear is otherwise unspecified.]{.note}

[3]{.pnum} Throws: `meta​::​exception` unless item represents a type, type alias, variable, function, [function parameter,]{.addu} namespace, enumerator, direct base class relationship, or non-static data member.

:::

Change the `data_member_spec` API in [meta.reflection.define.aggregate]{.sref} [The nested example in the note is now separate from the note]{.draftnote}:

::: std
```diff
struct data_member_options {
  struct $name-type$ { // exposition only
    template<class T> requires constructible_from<u8string, T>
      consteval $name-type$(T &&);

    template<class T> requires constructible_from<string, T>
      consteval $name-type$(T &&);

  private:
    variant<u8string, string> $contents$;    // exposition only
  };

+ info type;
  optional<$name-type$> name;
  optional<int> alignment;
  optional<int> bit_width;
  bool no_unique_address = false;
+ vector<info> annotations;
};
```

[1]{.pnum} The classes `data_member_options` and `data_member_options::$name-type$` are consteval-only types ([basic.types.general]), and are not structural types ([temp.param]).

```cpp
template <class T> requires constructible_from<u8string, T>
consteval $name-type$(T&& value);
```

[#]{.pnum} *Effects*: Initializes `$contents$` with `u8string(std::forward<T>(value))`.

```cpp
template<class T> requires constructible_from<string, T>
consteval $name-type$(T&& value);
```
[#]{.pnum} *Effects*: Initializes `$contents$` with `string(std::forward<T>(value))`.

::: note
The class `$name-type$` allows the function `data_member_spec` to accept an ordinary string literal (or `string_view`, `string`, etc.) or a UTF-8 string literal (or `u8string_view`, `u8string`, etc.) equally well.
:::

::: example
```diff
consteval void fn() {
- data_member_options o1 = {.name="ordinary_literal_encoding"};
- data_member_options o2 = {.name=u8"utf8_encoding"};
+ data_member_options o1 = {.type=^^int, .name="ordinary_literal_encoding"};
+ data_member_options o2 = {.type=^^char, .name=u8"utf8_encoding"};
}
```
:::

```diff
- consteval info data_member_spec(info type,
-                                 data_member_options options);
+ consteval info data_member_spec(data_member_options options);
```
[#]{.pnum} *Returns*: A reflection of a data member description (`$T$`, `$N$`, `$A$`, `$W$`, `$NUA$`[, `$ANN$`]{.addu}) ([class.mem.general]) where

- [#.#]{.pnum} `$T$` is the type represented by `dealias(@[options.]{.addu}@type)`,
- [#.#]{.pnum} `$N$` is either the identifier encoded by `options.name` or ⊥ if `options.name` does not contain a value,
- [#.#]{.pnum} `$A$` is either the alignment value held by `options.alignment` or ⊥ if `options.alignment` does not contain a value,
- [#.#]{.pnum} `$W$` is either the value held by `options.bit_width` or ⊥ if `options.bit_width` does not contain a value, [and]{.rm}
- [#.#]{.pnum} `$NUA$` is the value held by `options.no_unique_address`[.]{.rm} [, and]{.addu}
- [#.#]{.pnum} [`$ANN$` is the sequence of values `constant_of(r)` for each `r` in `options.annotations`.]{.addu}

[The returned reflection value is primarily useful in conjunction with `define_aggregate`; it can also be queried by certain other functions in `std::meta` (e.g., `type_of`, `identifier_of`).]{.note}

[#]{.pnum} *Throws*: `meta::exception` unless the following conditions are met:

- [#.#]{.pnum} `dealias(@[options.]{.addu}@type)` represents either an object type or a reference type;
- [#.#]{.pnum} if `options.name` contains a value, then:
  - [#.#.#]{.pnum} `holds_alternative<u8string>(options.name->$contents$)` is `true` and `get<u8string>(options.name->$contents$)` contains a valid identifier ([lex.name]) that is not a keyword ([lex.key]) when interpreted with UTF-8, or
  - [#.#.#]{.pnum} `holds_alternative<string>(options.name->$contents$)` is `true` and `get<string>(options.name->$contents$)` contains a valid identifier that is not a keyword when interpreted with the ordinary literal encoding;

  [The name corresponds to the spelling of an identifier token after phase 6 of translation ([lex.phases]). Lexical constructs like `$universal-character-name$`s [lex.universal.char] are not processed and will cause evaluation to fail. For example, `R"(\u03B1)"` is an invalid identifier and is not interpreted as `"α"`.]{.note}
- [#.#]{.pnum} if `options.name` does not contain a value, then `options.bit_width` contains a value;
- [#.#]{.pnum} if `options.bit_width` contains a value `$V$`, then
  - [#.#.#]{.pnum} `is_integral_type(@[options.]{.addu}@type) || is_enumeration_type(@[options.]{.addu}@type)` is `true`,
  - [#.#.#]{.pnum} `options.alignment` does not contain a value,
  - [#.#.#]{.pnum} `options.no_unique_address` is `false`, and
  - [#.#.#]{.pnum} if `$V$` equals `0` then `options.name` does not contain a value; [and]{.rm}
- [#.#]{.pnum} if `options.alignment` contains a value, it is an alignment value ([basic.align]) not less than `alignment_of(@[options.]{.addu}@type)`[.]{.rm} [; and]{.addu}
- [#.#]{.pnum} [for every reflection `r` in `options.annotations`, `type_of(r)` represents a non-array object type, and evaluation of `constant_of(r)` does not exit via an exception.]{.addu}

```cpp
template<reflection_range R = initializer_list<info>>
  consteval info define_aggregate(info class_type, R&& mdescrs);
```

[7]{.pnum} Let `$C$` be the class represented by `class_type` and `@$r$~$K$~@` be the `$K$`^th^ reflection value in `mdescrs`. For every `@$r$~$K$~@` in `mdescrs`, let (`@$T$~$K$~@`, `@$N$~$K$~@`, `@$A$~$K$~@`, `@$W$~$K$~@`, `@$NUA$~$K$~@`[, `@$ANN$~$K$~@`]{.addu}) be the corresponding data member description represented by `@$r$~$K$~@`.

[8]{.pnum} *Constant When*: [...]

[9]{.pnum} Produces an injected declaration `$D$` ([expr.const]) that defines `$C$` and has properties as follows:

* [9.1]{.pnum} The target scope of `$D$` is [...]
* [9.2]{.pnum} The locus of `$D$` [...]
* [9.3]{.pnum} The characteristic sequence of `$D$` [...]
* [9.4]{.pnum} If `$C$` is a specialization [...]
* [9.5]{.pnum} For each `@$r$~$K$~@`, there is a corresponding entity `@$M$~$K$~@` belonging to the class scope of `$D$` with the following properties:

  - [#.#.#]{.pnum} If `@$N$~$K$~@` is ⊥, `@$M$~$K$~@` is an unnamed bit-field. Otherwise, `@$M$~$K$~@` is a non-static data member whose name is the identifier determined by the character sequence encoded by `@$N$~$K$~@` in UTF-8.
  - [#.#.#]{.pnum} The type of `@$M$~$K$~@` is `@$T$~$K$~@`.
  - [#.#.#]{.pnum} `@$M$~$K$~@` is declared with the attribute `[[no_unique_address]]` if and only if `@$NUA$~$K$~@` is `true`.
  - [#.#.#]{.pnum} If `@$W$~$K$~@` is not ⊥, `@$M$~$K$~@` is a bit-field whose width is that value. Otherwise, `@$M$~$K$~@` is not a bit-field.
  - [#.#.#]{.pnum} If `@$A$~$K$~@` is not ⊥, `@$M$~$K$~@` has the `$alignment-specifier$` `alignas(@$A$~$K$~@)`. Otherwise, `@$M$~$K$~@` has no `$alignment-specifier$`.
  - [#.#.#]{.pnum} [`@$M$~$K$~@` has an annotation whose underlying constant ([dcl.attr.annotation]) is `r` for every reflection `r` in `@$ANN$~$K$~@`.]{.addu}
* [9.6]{.pnum} For every `@$r$~$L$~@` in `mdescrs` such that `$K$ < $L$` [...]
:::

