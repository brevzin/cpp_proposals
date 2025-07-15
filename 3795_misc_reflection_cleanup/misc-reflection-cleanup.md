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

## Specifying Error-Handling More Precisely

Currently, [@P3560R2] specifies nothing about the contents of the exceptions being thrown on failure from various reflection functions. That's largely as it should be — we really don't need to specify the message, for instance.

But one thing that `std::meta::exception` gives you is a `from()` accessor that tells you which operation failed. And that we _should_ specify — providing some front-matter sentence that says that when a function or function template `F` in `<meta>` is specified to throw a `meta::exception`, that `from()` is `^^F`.

# Wording

The rest of the wording will be provided later.

The synopsis change for [meta.type.synop] is:

::: std
```diff
#include <initializer_list>

namespace std::meta {
  using info = decltype(^^::);

  // [meta.reflection.operators], operator representations
  enum class operators {
    $see below$;
  };
  using enum operators;
  consteval operators operator_of(info r);
  consteval string_view symbol_of(operators op);
  consteval u8string_view u8symbol_of(operators op);

  // [meta.reflection.names], reflection names and locations
  consteval bool has_identifier(info r);

  consteval string_view identifier_of(info r);
  consteval u8string_view u8identifier_of(info r);

  consteval string_view display_string_of(info r);
  consteval u8string_view u8display_string_of(info r);

  consteval source_location source_location_of(info r);

  // [meta.reflection.queries], reflection queries
  consteval info type_of(info r);
  consteval info object_of(info r);
  consteval info constant_of(info r);

  consteval bool is_public(info r);
  consteval bool is_protected(info r);
  consteval bool is_private(info r);

  consteval bool is_virtual(info r);
  consteval bool is_pure_virtual(info r);
  consteval bool is_override(info r);
  consteval bool is_final(info r);

  consteval bool is_deleted(info r);
  consteval bool is_defaulted(info r);
  consteval bool is_user_provided(info r);
  consteval bool is_user_declared(info r);
  consteval bool is_explicit(info r);
  consteval bool is_noexcept(info r);
+ consteval bool is_inline(info r);
+ consteval bool is_constexpr(info r);
+ consteval bool is_consteval(info r);

  consteval bool is_bit_field(info r);
  consteval bool is_enumerator(info r);
  consteval bool is_annotation(info r);

  consteval bool is_const(info r);
  consteval bool is_volatile(info r);
  consteval bool is_mutable_member(info r);
  consteval bool is_lvalue_reference_qualified(info r);
  consteval bool is_rvalue_reference_qualified(info r);

  consteval bool has_static_storage_duration(info r);
  consteval bool has_thread_storage_duration(info r);
  consteval bool has_automatic_storage_duration(info r);

  consteval bool has_internal_linkage(info r);
  consteval bool has_module_linkage(info r);
  consteval bool has_external_linkage(info r);
  consteval bool has_c_language_linkage(info r);
  consteval bool has_linkage(info r);

  consteval bool is_complete_type(info r);
  consteval bool is_enumerable_type(info r);

  consteval bool is_variable(info r);
  consteval bool is_type(info r);
  consteval bool is_namespace(info r);
  consteval bool is_type_alias(info r);
  consteval bool is_namespace_alias(info r);

  consteval bool is_function(info r);
  consteval bool is_conversion_function(info r);
  consteval bool is_operator_function(info r);
  consteval bool is_literal_operator(info r);
  consteval bool is_special_member_function(info r);
  consteval bool is_constructor(info r);
  consteval bool is_default_constructor(info r);
  consteval bool is_copy_constructor(info r);
  consteval bool is_move_constructor(info r);
  consteval bool is_assignment(info r);
  consteval bool is_copy_assignment(info r);
  consteval bool is_move_assignment(info r);
  consteval bool is_destructor(info r);

  consteval bool is_function_parameter(info r);
  consteval bool is_explicit_object_parameter(info r);
  consteval bool has_default_argument(info r);
  consteval bool has_ellipsis_parameter(info r);

  consteval bool is_template(info r);
  consteval bool is_function_template(info r);
  consteval bool is_variable_template(info r);
  consteval bool is_class_template(info r);
  consteval bool is_alias_template(info r);
  consteval bool is_conversion_function_template(info r);
  consteval bool is_operator_function_template(info r);
  consteval bool is_literal_operator_template(info r);
  consteval bool is_constructor_template(info r);
  consteval bool is_concept(info r);

  consteval bool is_value(info r);
  consteval bool is_object(info r);

  consteval bool is_structured_binding(info r);

  consteval bool is_class_member(info r);
  consteval bool is_namespace_member(info r);
  consteval bool is_nonstatic_data_member(info r);
  consteval bool is_static_member(info r);
  consteval bool is_base(info r);

  consteval bool has_default_member_initializer(info r);

  consteval bool has_parent(info r);
  consteval info parent_of(info r);

  consteval info dealias(info r);

  consteval bool has_template_arguments(info r);
  consteval info template_of(info r);
  consteval vector<info> template_arguments_of(info r);
  consteval vector<info> parameters_of(info r);
  consteval info variable_of(info r);
  consteval info return_type_of(info r);

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
  consteval vector<info> members_of(info r, access_context ctx);
  consteval vector<info> bases_of(info type, access_context ctx);
  consteval vector<info> static_data_members_of(info type, access_context ctx);
  consteval vector<info> nonstatic_data_members_of(info type, access_context ctx);
  consteval vector<info> subobjects_of(info type, access_context ctx);
  consteval vector<info> enumerators_of(info type_enum);

  // [meta.reflection.layout], reflection layout queries
  struct member_offset;
  consteval member_offset offset_of(info r);
  consteval size_t size_of(info r);
  consteval size_t alignment_of(info r);
  consteval size_t bit_size_of(info r);

  // [meta.reflection.extract], value extraction
  template<class T>
    consteval T extract(info);

  // [meta.reflection.substitute], reflection substitution
  template<class R>
    concept reflection_range = $see below$;

  template<reflection_range R = initializer_list<info>>
    consteval bool can_substitute(info templ, R&& arguments);
  template<reflection_range R = initializer_list<info>>
    consteval info substitute(info templ, R&& arguments);

  // [meta.reflection.result], expression result reflection
  template<class T>
    consteval info reflect_constant(const T& value);
  template<class T>
    consteval info reflect_object(T& object);
  template<class T>
    consteval info reflect_function(T& fn);

  // [meta.reflection.array], promoting to static storage arrays
  template<ranges::input_range R>
    consteval info reflect_constant_string(R&& r);

  template<ranges::input_range R>
    consteval info reflect_constant_array(R&& r);

  // [meta.reflection.define.aggregate], class definition generation
  struct data_member_options;
- consteval info data_member_spec(info type, data_member_options options);
+ consteval info data_member_spec(data_member_options options);
  consteval bool is_data_member_spec(info r);
  template<reflection_range R = initializer_list<info>>
    consteval info define_aggregate(info type_class, R&&);

  // associated with [meta.unary.cat], primary type categories
  consteval bool is_void_type(info type);
  consteval bool is_null_pointer_type(info type);
  consteval bool is_integral_type(info type);
  consteval bool is_floating_point_type(info type);
  consteval bool is_array_type(info type);
  consteval bool is_pointer_type(info type);
  consteval bool is_lvalue_reference_type(info type);
  consteval bool is_rvalue_reference_type(info type);
  consteval bool is_member_object_pointer_type(info type);
  consteval bool is_member_function_pointer_type(info type);
  consteval bool is_enum_type(info type);
  consteval bool is_union_type(info type);
  consteval bool is_class_type(info type);
  consteval bool is_function_type(info type);
  consteval bool is_reflection_type(info type);

  // associated with [meta.unary.comp], composite type categories
  consteval bool is_reference_type(info type);
  consteval bool is_arithmetic_type(info type);
  consteval bool is_fundamental_type(info type);
  consteval bool is_object_type(info type);
  consteval bool is_scalar_type(info type);
  consteval bool is_compound_type(info type);
  consteval bool is_member_pointer_type(info type);

  // associated with [meta.unary.prop], type properties
  consteval bool is_const_type(info type);
  consteval bool is_volatile_type(info type);
  consteval bool is_trivially_copyable_type(info type);
  consteval bool is_trivially_relocatable_type(info type);
  consteval bool is_replaceable_type(info type);
  consteval bool is_standard_layout_type(info type);
  consteval bool is_empty_type(info type);
  consteval bool is_polymorphic_type(info type);
  consteval bool is_abstract_type(info type);
  consteval bool is_final_type(info type);
  consteval bool is_aggregate_type(info type);
  consteval bool is_consteval_only_type(info type);
  consteval bool is_signed_type(info type);
  consteval bool is_unsigned_type(info type);
  consteval bool is_bounded_array_type(info type);
  consteval bool is_unbounded_array_type(info type);
  consteval bool is_scoped_enum_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_constructible_type(info type, R&& type_args);
  consteval bool is_default_constructible_type(info type);
  consteval bool is_copy_constructible_type(info type);
  consteval bool is_move_constructible_type(info type);

  consteval bool is_assignable_type(info type_dst, info type_src);
  consteval bool is_copy_assignable_type(info type);
  consteval bool is_move_assignable_type(info type);

  consteval bool is_swappable_with_type(info type_dst, info type_src);
  consteval bool is_swappable_type(info type);

  consteval bool is_destructible_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_trivially_constructible_type(info type, R&& type_args);
  consteval bool is_trivially_default_constructible_type(info type);
  consteval bool is_trivially_copy_constructible_type(info type);
  consteval bool is_trivially_move_constructible_type(info type);

  consteval bool is_trivially_assignable_type(info type_dst, info type_src);
  consteval bool is_trivially_copy_assignable_type(info type);
  consteval bool is_trivially_move_assignable_type(info type);
  consteval bool is_trivially_destructible_type(info type);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_constructible_type(info type, R&& type_args);
  consteval bool is_nothrow_default_constructible_type(info type);
  consteval bool is_nothrow_copy_constructible_type(info type);
  consteval bool is_nothrow_move_constructible_type(info type);

  consteval bool is_nothrow_assignable_type(info type_dst, info type_src);
  consteval bool is_nothrow_copy_assignable_type(info type);
  consteval bool is_nothrow_move_assignable_type(info type);

  consteval bool is_nothrow_swappable_with_type(info type_dst, info type_src);
  consteval bool is_nothrow_swappable_type(info type);

  consteval bool is_nothrow_destructible_type(info type);
  consteval bool is_nothrow_relocatable_type(info type);

  consteval bool is_implicit_lifetime_type(info type);

  consteval bool has_virtual_destructor(info type);

  consteval bool has_unique_object_representations(info type);

  consteval bool reference_constructs_from_temporary(info type_dst, info type_src);
  consteval bool reference_converts_from_temporary(info type_dst, info type_src);

  // associated with [meta.unary.prop.query], type property queries
  consteval size_t rank(info type);
  consteval size_t extent(info type, unsigned i = 0);

  // associated with [meta.rel], type relations
  consteval bool is_same_type(info type1, info type2);
  consteval bool is_base_of_type(info type_base, info type_derived);
  consteval bool is_virtual_base_of_type(info type_base, info type_derived);
  consteval bool is_convertible_type(info type_src, info type_dst);
  consteval bool is_nothrow_convertible_type(info type_src, info type_dst);
  consteval bool is_layout_compatible_type(info type1, info type2);
  consteval bool is_pointer_interconvertible_base_of_type(info type_base, info type_derived);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_invocable_type(info type, R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval bool is_invocable_r_type(info type_result, info type, R&& type_args);

  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_type(info type, R&& type_args);
  template<reflection_range R = initializer_list<info>>
    consteval bool is_nothrow_invocable_r_type(info type_result, info type, R&& type_args);

  // associated with [meta.trans.cv], const-volatile modifications
  consteval info remove_const(info type);
  consteval info remove_volatile(info type);
  consteval info remove_cv(info type);
  consteval info add_const(info type);
  consteval info add_volatile(info type);
  consteval info add_cv(info type);

  // associated with [meta.trans.ref], reference modifications
  consteval info remove_reference(info type);
  consteval info add_lvalue_reference(info type);
  consteval info add_rvalue_reference(info type);

  // associated with [meta.trans.sign], sign modifications
  consteval info make_signed(info type);
  consteval info make_unsigned(info type);

  // associated with [meta.trans.arr], array modifications
  consteval info remove_extent(info type);
  consteval info remove_all_extents(info type);

  // associated with [meta.trans.ptr], pointer modifications
  consteval info remove_pointer(info type);
  consteval info add_pointer(info type);

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