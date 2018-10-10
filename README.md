Just a bunch of proposals for standardization that I've worked on. This is probably not up to date, since I always forget to push. And yes, some python / markdown stuff notwithstanding, I just write my proposals directly in HTML. No, I don't know why. Rendered [here](https://brevzin.github.io/cpp_proposals).

### Accepted to Working Draft

- [p0704r0 - Fixing const-qualified pointers to members](0704_const_qual_pmfs/p0704r0.html). Added to [working draft](http://eel.is/c++draft/expr.mptr.oper#6.sentence-2), with [revised wording](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](0780_lambda_pack_capture/p0780r2.html). Added to [working draft](http://eel.is/c++draft/expr.prim.lambda#capture-17) for C++20. There still needs to be a core wording issue as a pack of references is currently specified as `...&id` instead of `&...id`.
- [p0892r2 - `explicit(bool)`](0892_explicit_bool/p0892r2.html), with STL. Added to [working draft](http://eel.is/c++draft/dcl.fct.spec) for C++20. 

### Rejected

- [p0321r1 - Make pointers to members callable](0312_pointers_to_members/p0312r1.html). Rejected in Toronto.
- [p0573r2 - Abbreviated Lambdas](0573_abbrev_lambdas/p0573r2.html), with Tomasz Kaminski. Rejected in Albuquerque.
- [p0644r1 - Forward without forward](0644_fwd/p0644r1.html). Rejected in Albuquerque.

### Pending

- [p0847r1 - Deducing this](0847_deducing_this/p0847r1.html), with Simon Brand, Gasper Asman, and Ben Deane. R0 was discussed in EWG in Rapperswil. 
- [p0848r0 - Conditionally Trivial Special Member Functions](0848_special_members/p0848r0.html), with Casey Carter.
- [p0893r1 - Chaining comparisons](0893_chain_comparisons/p0893r1.html), with Herb Sutter.
- [p1061r0 - Structured bindings can introduce a Pack](1061_sb_pack/p1061r0.html), with Jonathan Wakely.
- [p1065r0 - constexpr `INVOKE`](1065_constexpr_invoke/d1065r0.html).
- [p1169r0 - static `operator()`](1169_static_call/p1169r0.html), with Casey Carter.
- [p1170r0 - Overload sets as function parameters](1170_overload_sets/p1170r0.html), with Andrew Sutton.
- [p1185r0 - `<=> != ==`](1185-7_spaceship/p1185r0.html).
- [p1185r0 - When do you actually use `<=>`?](1185-7_spaceship/p1186r0.html).
- [p1185r0 - A type trait for `std::compare_3way()`'s type](1185-7_spaceship/p1187r0.html).
