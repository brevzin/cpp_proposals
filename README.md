# cpp_proposals

Just a bunch of proposals for standardization that I've worked on. This is probably not up to date, since I always forget to push. And yes, some python / markdown stuff notwithstanding, I just write my proposals directly in HTML. No, I don't know why.

- [p0321r1 - Make pointers to members callable](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/0312r1_pointers_to_members.html). Rejected in Toronto.
- [p0573r2 - Abbreviated Lambdas](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/0573r2_abbrev_lambda.html), with Tomasz Kaminski. Rejected in Albuquerque.
- [p0644r1 - Forward without forward](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/0644r1_forward_without_forward.html). Rejected in Albuquerque.
- [p0704r0 - Fixing const-qualified pointers to members](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/0704r0_const_qualified_pmfs.html). Added to [working draft](http://eel.is/c++draft/expr.mptr.oper#6.sentence-2), with [revised wording](http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2017/p0704r1.html).
- [p0780r2 - Allow pack expansion in lambda init-capture](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/d0780r2.html). Added to [working draft](http://eel.is/c++draft/expr.prim.lambda#capture-17) for C++20. There still needs to be a core wording issue as a pack of references is currently specified as `...&id` instead of `&...id`.
- [p0847r0 - Deducing this](https://wg21.tartanllama.xyz/deducing-this/), with Simon Brand, Gasper Asman, and Ben Deane. Work is mainly being done [here](https://github.com/TartanLlama/wg21/blob/master/deducing-this.md). Pending discussion in EWG in ~Jacksonville~ Rapperswil.
- [p0848r0 - Conditionally Trivial Special Member Functions](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/0848r0_special_members.html), with Casey Carter. Pending discussion in EWG in ~Jacksonville~ Rapperswil.
- [p0892r1 - `explicit(bool)`](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/d0892r1.html), with STL. Approved by EWG in Jacksonville. Pending core and library review in Rapperswil. 
- [p0893r1 - Chaining comparisons](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/d0893r1.html), with Herb Sutter. Discussed by EWG in Jacksonville, returning to EWG with many changes.
- [p1061r0 - Structured bindings can introduce a Pack](http://htmlpreview.github.io/?https://github.com/BRevzin/cpp_proposals/blob/master/1061r0_sb_pack.html), with Jonathan Wakely. Will be in pre-Rapperswil mailing, pending discussion in EWG in Rapperswil.
