Just a bunch of proposals for standardization that I've worked on. This is
probably not up to date, since I always forget to push. Rendered
[here](https://brevzin.github.io/cpp_proposals).

As of pre-Cologne 2019, I use [mpark/wg21](https://github.com/mpark/wg21) to
make all of my papers.

### Accepted to Working Draft

- [p0704r1 - Fixing const-qualified pointers to members](0704_const_qual_pmfs/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](0780_lambda_pack_capture/p0780r2.html). There is a core issue (CWG 2378) as a pack of references is currently specified as `...&id` instead of `&...id`.
- [p0848r3 - Conditionally Trivial Special Member Functions](0848_special_members/p0848r3.html), with Casey Carter.
- [p0892r2 - `explicit(bool)`](0892_explicit_bool/p0892r2.html), with STL. 
- [p1065r2 - constexpr `INVOKE`](1065_constexpr_invoke/p1065r2.html), with Tomasz Kamiński.
- [p1185r2 - `<=> != ==`](118x_spaceship/p1185r2.html). Splitting `==` and `<=>`.
- [p1186r3 - When do you actually use `<=>`?](118x_spaceship/p1186r3.html). A helper
  feature to make it easier to adopt `<=>`.
- [p1614r2 - The Mothership Has Landed](118x_spaceship/p1614r2.html). The one
  library wording paper for everything `<=>` related (including many papers that
  are not mine).
- [p1630r1 - Spaceship needs a tuneup](118x_spaceship/p1630r1.html). A paper addressing several `<=>`-
  related issues that have come up.
- [p1870r1 - `forwarding-range<T>` is too subtle](1870_forwarding_range/p1870r1.html).
- [p1871r1 - Concept traits should be named after concepts](1871_enable_sized_range/p1871r1.html).
- [p1946r0 - Allow defaulting comparisons by value](1946_dflt_value_comparisons/p1946r0.html).
- [p1959r0 - Remove `std::weak_equality` and `std::strong_equality`](1959_remove_equality/p1959r0.html).

### Rejected

- [p0321r1 - Make pointers to members callable](0312_pointers_to_members/p0312r1.html). Rejected in Toronto.
- [p0573r2 - Abbreviated Lambdas](0573_abbrev_lambdas/p0573r2.html), with Tomasz Kamiński. Rejected in Albuquerque.
- [p0644r1 - Forward without forward](0644_fwd/p0644r1.html). Rejected in Albuquerque.
- [p0893r1 - Chaining comparisons](0893_chain_comparisons/p0893r1.html), with Herb Sutter. Rejected in San Diego.
- [p1169r0 - static `operator()`](1169_static_call/p1169r0.html), with Casey Carter. Rejected in San Diego.

### Pending

- [p0847r2 - Deducing this](0847_deducing_this/p0847r2.html), with Simon Brand,
  Gasper Asman, and Ben Deane. R0 was discussed in EWG in Rapperswil. R1 was
  discussed in San Diego. 
- [p1061r1 - Structured bindings can introduce a Pack](1061_sb_pack/p1061r1.html), with Jonathan Wakely.
- [p1170r0 - Overload sets as function parameters](1170_overload_sets/p1170r0.html), with Andrew Sutton.
- [p1858r0 - Generalized pack declarations and usage](1858_generalized_packs/p1858r0.html).
- [p1900r0 - Concepts-adjacent problems](1900_concepts/p1900r0.html).
