Just a bunch of proposals for standardization that I've worked on. This is
probably not up to date, since I always forget to push. Rendered
[here](https://brevzin.github.io/cpp_proposals).

As of pre-Cologne 2019, I use [mpark/wg21](https://github.com/mpark/wg21) to
make all of my papers.

### Accepted to Working Draft

- [p0704r1 - Fixing const-qualified pointers to members](0704_const_qual_pmfs/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](0780_lambda_pack_capture/p0780r2.html). There is a core issue (CWG 2378) as a pack of references is currently specified as `...&id` instead of `&...id`.
- [p0892r2 - `explicit(bool)`](0892_explicit_bool/p0892r2.html), with STL. 
- [p1185r2 - `<=> != ==`](118x_spaceship/p1185r2.html).

### Approved by (L)EWG

- [p0848r2 - Conditionally Trivial Special Member Functions](0848_special_members/p0848r2.html), with Casey Carter. Approved in San Diego with new design..
- [p1065r1 - constexpr `INVOKE`](1065_constexpr_invoke/p1065r1.html), with Tomasz Kamiński.
- [p1186r2 - When do you actually use `<=>`?](118x_spaceship/d1186r2.html).
  Approved in San Diego and rejected by Core there, new design approved in Kona
  and this is the wording for that. The library part was moved to p1614.
- [p1614r1 - The Mothership Has Landed](118x_spaceship/p1614r1.html). The one
  library wording paper for everything `<=>` related (including many papers that
  are not mine).

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
- [p1061r0 - Structured bindings can introduce a Pack](1061_sb_pack/p1061r0.html), with Jonathan Wakely.
- [p1170r0 - Overload sets as function parameters](1170_overload_sets/p1170r0.html), with Andrew Sutton.
- [p1630r0 - Spaceship needs a tuneup](118x_spaceship/p1630r0.html). A paper addressing several `<=>`-
  related issues that have come up. Will be in the pre-Cologne mailing.
