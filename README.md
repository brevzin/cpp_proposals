Just a bunch of proposals for standardization that I've worked on. This is
probably not up to date, since I always forget to push. Rendered
[here](https://brevzin.github.io/cpp_proposals).

As of pre-Cologne 2019, I use [mpark/wg21](https://github.com/mpark/wg21) to
make all of my papers.

### Accepted to Working Draft

#### C++20

- [p0704r1 - Fixing const-qualified pointers to members](0704_const_qual_pmfs/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](0780_lambda_pack_capture/p0780r2.html). There is a core issue (CWG 2378) as a pack of references is currently specified as `...&id` instead of `&...id`.
- [p0848r3 - Conditionally Trivial Special Member Functions](0848_special_members/p0848r3.html), with Casey Carter.
- [p0892r2 - `explicit(bool)`](0892_explicit_bool/p0892r2.html), with STL.
- [p1065r2 - constexpr `INVOKE`](1065_constexpr_invoke/p1065r2.html), with Tomasz Kamiński.
- ![][~spaceship] [p1185r2 - `<=> != ==`](118x_spaceship/p1185r2.html). Splitting `==` and `<=>`.
- ![][~spaceship] [p1186r3 - When do you actually use `<=>`?](118x_spaceship/p1186r3.html). A helper feature to make it easier to adopt `<=>`.
- ![][~spaceship] [p1187r0 - A type trait for `std::compare_3way()`'s type](118x_spaceship/p1187r0.html).
- ![][~spaceship] [p1188r0 - Library utilities for `<=>`](118x_spaceship/p1188r0.html).
- ![][~spaceship] [p1189r0 - Adding `<=>` to library](118x_spaceship/p1189r0.html).
- ![][~spaceship] [p1614r2 - The Mothership Has Landed](118x_spaceship/p1614r2.html). The one library wording paper for everything `<=>` related (including many papers that are not mine).
- ![][~spaceship] [p1630r1 - Spaceship needs a tuneup](118x_spaceship/p1630r1.html). A paper addressing several `<=>`-related issues that have come up.
- ![][~ranges] [p1870r1 - `forwarding-range<T>` is too subtle](1870_forwarding_range/p1870r1.html).
- ![][~ranges] [p1871r1 - Concept traits should be named after concepts](1871_enable_sized_range/p1871r1.html).
- ![][~spaceship] [p1946r0 - Allow defaulting comparisons by value](1946_dflt_value_comparisons/p1946r0.html).
- ![][~spaceship] [p1959r0 - Remove `std::weak_equality` and `std::strong_equality`](1959_remove_equality/p1959r0.html).
- [p2095r0 - Resolve lambda init-capture pack grammar (CWG2378)](2095_lambda_pack_cwg/p2095r0.html).

#### DRs against C++20 (or earlier)

- ![][~ranges] [p2017r1 - Conditionally borrowed ranges](2017_safe_range/p2017r1.html).
- [p2036r3 - Change scope of lambda _trailing-return-type_](2036_lambda_scope/p2036r3.html).
- [p2162r2 - Inheriting from `std::variant`](2162_inherit_variant/p2162r2.html).
- ![][~ranges] [p2210r2 - Superior String Splitting](2210_string_split/p2210r2.html).
- ![][~constexpr] [p2231r1 - Missing `constexpr` in `std::optional` and `std::variant`](2231_constexpr_optional_variant/p2231r1.html).
- ![][~ranges] [p2325r3 - Views should not be required to be default constructible](2325_views_default/p2325r3.html).
- ![][~ranges] [p2415r2 - What is a `view`?](2415_what_view/p2415r2.html)
- [p2493r0 - Missing feature test macros for C++20 core papers](2493_core_feature_test/p2493r0.html)

#### C++23

- [p0847r7 - Deducing this](0847_deducing_this/p0847r7.html), with Simon Brand, Gašper Ažman, and Ben Deane.
- ![][~constexpr] [p1938r3 - `if consteval`](1938_if_consteval/p1938r3.html), with Richard Smith, Andrew Sutton, and Daveed Vandevoorde.
- ![][~ranges] [p2387r3 - Pipe support for user-defined range adaptors](2387_ranges_pipes/p2387r3.html).
- ![][~ranges] [p2441r2 - `views::join_with`](2441_join_with/p2441r2.html)

### Pending Plenary

- [p1169r4 - `static operator()`](1169_static_call/p1169r4.html), with Casey Carter.
- ![][~constexpr] [p2280r4 - Using unknown references in constant expressions](2280_unknown_reference/p2280r4.html).
- ![][~ranges] [p2322r6 - `ranges::fold`](2322_fold/p2322r6.html).
- ![][~constexpr] [p2448r2 - Relaxing some `constexpr` restrictions](2448_relax_constexpr/p2448r2.html).
- [p2508r2 - Expose `std::basic-format-string<charT, Args...>`](2508_expose_format_string/p2508r2.html)

### Pending CWG/LWG Review

- ![][~ranges] [p2278r3 - `cbegin` should always return a constant iterator](2278_cbegin/p2278r3.html).
- ![][~ranges] [p2286r8 - Formatting Ranges](2286_fmt_ranges/p2286r8.html).
- ![][~ranges] [p2446r2 - `views::as_rvalue`](2446_move_view/p2446r2.html).

### Pending EWG/LEWG Voting

- ![][~ranges] [p2520r0 - `move_iterator<T*>` should be a random access iterator](2520_move_iterator/p2520r0.html).

### Pending EWG/LEWG Review

- ![][~constexpr] [p2484r0 - Extending support for class types as non-type template parameters](2484_extend_cnttp/p2484r0.html)
- ![][~ranges] [p2550r1 - `ranges::copy` should say `output_iterator` somewhere](2550_algo_output_iterator/p2550r1.html).
- ![][~constexpr] [p2564r0 - `consteval` needs to propagate up](2564_consteval_patch/p2564r0.html).

### Ongoing Plans

- ![][~ranges] [p2214r2 - A Plan for C++23 Ranges](2214_ranges_plan/p2214r2.html), with Conor Hoekstra and Tim Song.

#### Figure out packs outside of templates?

- [p1061r2 - Structured bindings can introduce a Pack](1061_sb_pack/p1061r2.html), with Jonathan Wakely.
- [p1858r2 - Generalized pack declarations and usage](1858_generalized_packs/p1858r2.html).
- [p2277r0 - Packs outside of templates](2277_packs_outside_of_templates/p2277r0.html).
- [p2120r0 - Simplified structured bindings protocol with pack aliases](1858_generalized_packs/p2120r0.html). Splitting off the structured bindings part from p1858.

#### Concepts

- [p1900r0 - Concepts-adjacent problems](1900_concepts/p1900r0.html).
- [p2279r0 - We need a language mechanism for customization points](2279_static_polymorphism/p2279r0.html).

#### Other

- [d2011r2 - A pipeline-rewrite operator](2011_pipeline/d2011r2.html), with Colby Pike.
- [p2287r1 - Designated-initializers for Base Classes](2287_designated_base/p2287r1.html).
- [p2481r0 - Forwarding reference to specific type/template](2481_forward_ref/forward-ref.md).

### Rejected

- [p0321r1 - Make pointers to members callable](0312_pointers_to_members/p0312r1.html). Rejected in Toronto.
- [p0573r2 - Abbreviated Lambdas](0573_abbrev_lambdas/p0573r2.html), with Tomasz Kamiński. Rejected in Albuquerque.
- [p0644r1 - Forward without forward](0644_fwd/p0644r1.html). Rejected in Albuquerque.
- [p0893r1 - Chaining comparisons](0893_chain_comparisons/p0893r1.html), with Herb Sutter. Rejected in San Diego.

### On Hold
- [p1170r0 - Overload sets as function parameters](1170_overload_sets/p1170r0.html), with Andrew Sutton. Need to reconsider the design.
- [p2089r0 - Function parameter constraints are fragile](2089_param_constraints/p2089r0.html). The proposal this responds to was tabled.


[~ranges]: https://img.shields.io/badge/-ranges-brightgreen
[~constexpr]: https://img.shields.io/badge/-constexpr-blueviolet
[~spaceship]: https://img.shields.io/badge/-%3C%3D%3E-yellow
