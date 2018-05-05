# cpp_proposals

Just a bunch of proposals for standardization that I've worked on. This is probably not up to date, since I always forget to push. And yes, some python / markdown stuff notwithstanding, I just write my proposals directly in HTML. No, I don't know why.

### Accepted to Working Draft

- [p0704r0 - Fixing const-qualified pointers to members](https://rawgit.com/brevzin/cpp_proposals/master/0704_const_qual_pmfs/p0704r0.html). Added to [working draft](http://eel.is/c++draft/expr.mptr.oper#6.sentence-2), with [revised wording](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](https://rawgit.com/brevzin/cpp_proposals/master/0780_lambda_pack_capture/p0780r2.html). Added to [working draft](http://eel.is/c++draft/expr.prim.lambda#capture-17) for C++20. There still needs to be a core wording issue as a pack of references is currently specified as `...&id` instead of `&...id`.

### Rejected

- [p0321r1 - Make pointers to members callable](https://rawgit.com/brevzin/cpp_proposals/master/0312_pointers_to_members/p0312r1.html). Rejected in Toronto.
- [p0573r2 - Abbreviated Lambdas](https://rawgit.com/brevzin/cpp_proposals/master/0573_abbrev_lambdas/p0573r2.html), with Tomasz Kaminski. Rejected in Albuquerque.
- [p0644r1 - Forward without forward](https://rawgit.com/brevzin/cpp_proposals/master/0644_fwd/p0644r1.html). Rejected in Albuquerque.

### Pending

- [p0847r1 - Deducing this](https://wg21.tartanllama.xyz/deducing-this/), with Simon Brand, Gasper Asman, and Ben Deane. Work is mainly being done [here](https://github.com/TartanLlama/wg21/blob/master/deducing-this.md). Pending discussion in EWG.
- [p0848r0 - Conditionally Trivial Special Member Functions](https://rawgit.com/brevzin/cpp_proposals/master/0848_special_members/p0848r0.html), with Casey Carter. Pending discussion in EWG.
- [p0892r1 - `explicit(bool)`](https://rawgit.com/brevzin/cpp_proposals/master/0892_explicit_bool/p0892r1.html), with STL. Approved by EWG in Jacksonville. Pending core and library review in Rapperswil. 
- [p0893r1 - Chaining comparisons](https://rawgit.com/brevzin/cpp_proposals/master/0893_chain_comparisons/p0893r1.html), with Herb Sutter. Discussed by EWG in Jacksonville, returning to EWG with many changes.
- [p1061r0 - Structured bindings can introduce a Pack](https://rawgit.com/brevzin/cpp_proposals/master/1061_sb_pack/p1061r0.html), with Jonathan Wakely. Will be in pre-Rapperswil mailing, pending discussion in EWG.
