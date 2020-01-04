---
title: "Conditionally safe ranges"
document: D2017R0
date: today
audience: LWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction and Motivation

Consider the following approach to trimming a `std::string`:

```cpp
auto trim(std::string const& s) {
    auto isalpha = [](unsigned char c){ return std::isalpha(c); };
    auto b = ranges::find_if(s, isalpha);
    auto e = ranges::find_if(s | views::reverse, isalpha).base();
    return subrange(b, e);
}
```

This is a fairly nice and, importantly, safe way to implement `trim`. The 
iterators `b` and `e` returned from `find_if` will not dangle, since they point
into the string `s` whose lifetime outlives the function. 

Except this code will not compile at the moment, either in C++20 or in
range-v3, failing on the declaration of `e`. The algorithm `find_if` is in
[alg.find]{.sref} is declared as:

```cpp
template<input_range R, class Proj = identity,
         indirect_unary_predicate<projected<iterator_t<R>, Proj>> Pred>
  constexpr safe_iterator_t<R>
    ranges::find_if(R&& r, Pred pred, Proj proj = {});
```

`R` will deduce as `reverse_view<ref_view<std::string const>>`, which does
_not_ satisfy `safe_range` (it is neither an lvalue reference, nor does
`reverse_view` currently opt-in to being a `safe_range`) hence the
return type `safe_iterator_t<R>` is the type `dangling` rather than being the
type `iterator_t<R>`. Instead of getting the reverse iterator we might have
expected, that we need to call `.base()` on, we get effectively nothing. We
are forced to rewrite the above as:

```cpp
auto trim(std::string const& s) {
    auto isalpha = [](unsigned char c){ return std::isalpha(c); };
    auto b = ranges::find_if(s, isalpha);
    auto reversed = s | views::reverse;
    auto e = ranges::find_if(reversed, isalpha).base();
    return subrange(b, e);
}
```

Which is an unnecessary code indirection. The goal of this paper is to make the
initial example just work. We clearly have a safe range that is not marked
as such, so I consider this to be a library defect. 

# History and Status Quo

Ranges introduced with it the concept _`forwarding-range`_. This was then
renamed to `safe_range` by [@P1870R1] in Belfast, but the core concept remains
the same. A range is a `safe_range` when you can safely hold onto its iterators
after the range goes out of scope. There are two kinds of safe ranges:

- lvalue references to ranges are always safe. It's not the lvalue reference
itself which owns the data, so if the reference dies, we're fine.
- ranges which must necessarily opt in to being considered safe, by way of the
new `enable_safe_range` variable template, which defaults to `false` for all
types.

There are several safe ranges which do this opt in today:

- `ref_view` is a safe_range. It's basically a reference - the iterators it
gives out are iterators to the range it refers to, which it has no ownership of.
If the `ref_view` dies, the referred to range can still be around.
- `string_view` is a safe range. Like `ref_view`, it just refers to data - the
iterators it gives out are iterators into some other containers. `span` and
`subrange` are similar.
- `empty_view` doesn't even have any data, so it's trivially safe. 
- `iota_view` works by having the iterators themselves own the "counter", so
having the iterators stick around is sufficient.

And that's it. We have six, _unconditionally_ safe ranges. All other ranges
and views in the standard library are _unconditionally_ unsafe. But this is
far too strict. As the opening example demonstrates, there are many more kinds
of safe ranges you can construct than _just_ the chosen six. 

This issue was first pointed out by Johel Ernesto Guerrero Peña in [@stl2.640].

# Proposal

Several range adapters semantically behave as if they have a single member of
some templated view type. If that underlying view type is a `safe_range`, the
range adapter itself can be transitively safe. For example, `s | views::reverse`
has the type `reverse_view<ref_view<string const>>`. This can be a `safe_range`
because `ref_view<string const>` is a `safe_range`. Likewise,
`s | views::reverse | views::take(3)` can also be a `safe_range` by extending
this logic further. 

Here is a table of all the range adapters and factories in the current working
draft, what their current `safe_range` status is, and what this paper proposes.

<table>
<tr><th>Name</th><th>Current Status</th><th>Proposed</th></tr>
<tr><td>`empty`</td><td>Safe</td><td>No change</td></tr>
<tr><td>`single`</td><td>Unsafe</td><td>No change. It's the view which holds the element, not the iterators.</th></tr>
<tr><td>`iota`</td><td>Safe</td><td>No change.</td></tr>
<tr><td>`istream`</td><td>Unsafe</td><td>No change. The iterators need to refer to parent view, which holds onto the element.</td></tr>
<tr><td>`ref`</td><td>Safe</td><td>No change.</td></tr>
<tr><td>`filter`</td><td>Unsafe</td><td>No change. The view needs to own the predicate.</td></tr>
<tr><td>`transform`</td><td>Unsafe</td><td>No change, as above.</td></tr>
<tr><td>`take`</td><td>Unsafe</td><td>[Conditionally safe, based on the underlying view. The iterators are just iterators into the underlying view (or thin wrappers thereof).]{.addu}.</td></tr>
<tr><td>`take_while`</td><td>Unsafe</td><td>No change, same as `filter`.</td></tr>
<tr><td>`drop`</td><td>Unsafe</td><td>[Conditionally safe, same as `take`]{.addu}</td></tr>
<tr><td>`drop_while`</td><td>Unsafe</td><td>No change, same as `filter`.</td></tr>
<tr><td>`join`</td><td>Unsafe</td><td>No change. This one is quite complex and iterators need to refer the `join_view`.</td></tr>
<tr><td>`split`</td><td>Unsafe</td><td>No change, as with `join`.</td></tr>
<tr><td>`counted`</td><td colspan="2">Not actually its own view, `counted(r, n)` is actually either some `subrange` or ill-formed, so it's already safe.</td></tr>
<tr><td>`common`</td><td>Unsafe</td><td>[Conditionally safe based on the underlying view. All it does is propagate iterators.]{.addu}</td></tr>
<tr><td>`reverse`</td><td>Unsafe</td><td>[Conditionally safe based on the underlying view. All it does is propagate reverse iterators.]{.addu}</td></tr>
<tr><td>`elements`/`keys`/`values`</td><td>Unsafe</td><td>[Conditionally safe based on the underlying view. This is a special case of `transform_view` where
the transform is actually encoded into the type, so it doesn't need to be
held onto by the view itself.]{.addu}</td></tr>
</table>

# Wording

Add five variable template specializations to [range.syn]{.sref}:

```diff
#include <initializer_list>
#include <iterator>

namespace std::ranges {
  // [...]
  

  // [range.take], take view
  template<view> class take_view;
  
+ template<class T>
+   inline constexpr bool enable_safe_range<take_view<T>> = enable_safe_range<T>; 

  namespace views { inline constexpr @_unspecified_@ take = @_unspecified_@; }  
  
  // [...]
  
  // [range.drop], drop view
  template<view V>
    class drop_view;
    
+ template<class T>
+   inline constexpr bool enable_safe_range<drop_view<T>> = enable_safe_range<T>; 

  namespace views { inline constexpr @_unspecified_@ drop = @_unspecified_@; }  
  
  // [...]
  // [range.common], common view
  template<view V>
    requires (!common_range<V> && copyable<iterator_t<V>>)
  class common_view;
  
+ template<class T>
+   inline constexpr bool enable_safe_range<common_view<T>> = enable_safe_range<T>;   

  namespace views { inline constexpr @_unspecified_@ common = @_unspecified_@; }

  // [range.reverse], reverse view
  template<view V>
    requires bidirectional_range<V>
  class reverse_view;
  
+ template<class T>
+   inline constexpr bool enable_safe_range<reverse_view<T>> = enable_safe_range<T>;    

  namespace views { inline constexpr @_unspecified_@ reverse = @_unspecified_@; }

  // [range.elements], elements view
  template<input_range V, size_t N>
    requires @_see below_@;
  class elements_view;
  
+ template<class T, size_t N>
+   inline constexpr bool enable_safe_range<elements_view<T, N>> = enable_safe_range<T>;  

  template<class R>
    using keys_view = elements_view<all_view<R>, 0>;
  template<class R>
    using values_view = elements_view<all_view<R>, 1>;

  namespace views {
    template<size_t N>
      inline constexpr @_unspecified_@ elements = @_unspecified_@ ;
    inline constexpr @_unspecified_@ keys = @_unspecified_@ ;
    inline constexpr @_unspecified_@ values = @_unspecified_@ ;
  }
}
```

# Implementation

This has been implemented in range-v3 [@range-v3.1405]. The PR includes the five
range adapters in this paper, along with a smattering of other range adapters
from range-v3 that can also be made conditionally safe in this manner (`const`,
`delimit`, `drop_exactly`, `indirect`, `move`, `slice`, `tail`, `trim`,
and `unbounded`).

---
references:
  - id: stl2.640
    citation-label: stl2.640
    title: "Unsafe views that are actually safe"
    author:
      - family: Johel Ernesto Guerrero Peña
    issued:
      - year: 2019
    URL: https://github.com/ericniebler/stl2/issues/640
  - id: range-v3.1405
    citation-label: range-v3.1405
    title: "Making more range adapters safe"
    author:
      - family: Barry Revzin
    issued:
      - year: 2020
    URL: https://github.com/ericniebler/range-v3/pull/1405    
---
