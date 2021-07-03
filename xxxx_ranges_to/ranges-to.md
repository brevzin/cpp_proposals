---
title: "Range-based container operations and `ranges::to`"
document: DxxxxR0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

From C++98 through C++17, C++ has had one iterator model that all containers within (and without) the standard library used to great effect. A typical container would look like this:

::: bq
```cpp
struct Container {
    template <typename Iterator>
    Container(Iterator, Iterator);
    
    template <typename Iterator>
    void assign(Iterator, Iterator);
    
    template <typename Iterator>
    void insert(const_iterator, Iterator, Iterator);
};
```
:::

Obviously these function templates should have some sort of constraint, but the idea is clear. You can construct any kind of container using any kind of iterator pair - as long as the iterator gave you the right kind of thing. 

But C++20 changes the iterator model in some very big ways:

* a range may have its sentinel type differ from its iterator type, which makes none of the above operations valid.
* the C++20 iterator categories improve upon the C++17 iterator categories, so an iterator that is only an input iterator in C++17 can be a random access iterator in C++20. This can be a huge pessimization if code isn't updated to reflect the new categories.
* we want range-based APIs, not iterator-based APIs - both because this is more convenient and because it can provide more performance. The canonical example here is `std::list<T>` is a `sized_range`, but a pair of `std::list<T>::iterators` cannot give you the size in `O(1)`.

Our existing model, where containers have an iterator-pair constructor, an iterator-pair `assign`, and an iterator-pair `insert`, just doesn't cut it in a C++20 world. And this inadequacy is especially prominent now that we're discussing how to make `ranges::to` ([@P1206R3]) work. Given an arbitrary C++20 range (which may not be a `common` range and may not even have valid C++17 iterators), the best you can do to make something like this work:

::: bq
```cpp
auto v = some_range | ranges::to<std::vector>();
```
:::

is to do this:

::: bq
```cpp
std::vector<T> v;
v.reserve(std::ranges::distance(some_range));
std::ranges::copy(some_range, std::back_inserter(v));
```
:::

which leaves a lot to be desired. Both because we need to formalize when the library can call `reserve` on a user-defined type and which kind of output iterator to provide (sometimes it's `back_inserter` and sometimes it's `inserter`), but also because even once we deal with these issues, the above implementation isn't really as efficient as could be. `ranges::copy` through the `back_inserter` basically boils down to:

::: bq
```cpp
std::vector<T> v;
v.reserve(std::ranges::distance(some_range));

auto f = std::ranges::begin(some_range);
auto l = std::ranges::end(some_range);
for (; f != l; ++f) {
    v.push_back(*f);
}
```
:::

And that `push_back` will do a branch every time, to see if the `vector` has to be reallocated. Even though it won't have to be, since we just `reserve`-ed up front. But the library doesn't know that we did that. If we were *inside* `vector`, we wouldn't have to do this at all:

::: bq
```cpp
template <typename T>
class vector {
    // simplified to avoid allocators
    T* begin_;
    T* end_;
    T* capacity_;
    
public:
    template <typename R>
    explicit vector(from_range_t, R&& r) {
        if constexpr (std::ranges::forward_range<R>) {
            size_t d = std::ranges::distance(r);
            begin_ = std::allocator<T>{}.allocate(d);
            end_ = std::ranges::uninitialized_copy(r, begin_);
            capacity_ = end_;
        } else {
            // have to do the slow thing, no way around it
        }
    }
};
```
:::

Here, there's no extra branches in `uninitialized_copy`, we know we're not going to have to allocate again - we're just copying into a pointer. Collecting an arbitrary range pipeline into a `vector` is going to be a pretty common operation, so we should make it as efficient as we can!

How do we do that?

## Range-based container operations

Let's start by introducing range-based container operations. It's tempting to want to do that this way:

::: bq
```diff
struct Container {
    template <typename Iterator>
    Container(Iterator, Iterator);
    
+   template <typename Range>
+   explicit Container(Range&&);
    
    template <typename Iterator>
    void assign(Iterator, Iterator);
    
+   template <typename Range>
+   void assign(Range&&);
    
    template <typename Iterator>
    void insert(const_iterator, Iterator, Iterator);
    
+   template <typename Range>
+   void insert(const_iterator, Range&&);
};
```
:::

But we unforuntately run into the following problem:

::: bq
```cpp
std::list<int> xs = {1, 2, 3};

// this would use our new range-based constructor
// and give us a vector<int> containing {1, 2, 3}
std::vector v(xs);

// this would prefer the existing initializer_list
// constructor and give us a vector<list<int>>
// containing a single list
std::vector w{xs};
```
:::

And there's not much we can do there, since existing code that does something like `vector{"hello"s}` may very intentionally want to produce a `vector<string>` containing a single `string` with the value `"hello"` rather than producing a `vector<char>` containing 5 `char`s. This is an ambiguity we have to avoid.

The easiest way to avoid ambiguities is to simply give them different names. We can't rename the constructor, so we introduce a tag type:

::: bq
```diff
+ namespace std {
+   struct from_range_t { explicit from_range_t() = default; };
+   inline constexpr from_range_t from_range;
+ }

struct Container {
    template <typename Iterator>
    Container(Iterator, Iterator);
    
+   template <typename Range>
+   explicit Container(std::from_range_t, Range&&);
    
    template <typename Iterator>
    void assign(Iterator, Iterator);
    
+   template <typename Range>
+   void assign_range(Range&&);
    
    template <typename Iterator>
    void insert(const_iterator, Iterator, Iterator);
    
+   template <typename Range>
+   void insert_range(const_iterator, Range&&);
};
```
:::

Thus:

::: bq
```cpp
std::list<int> xs = {1, 2, 3};

// this would use our new range-based constructor
// and give us a vector<int> containing {1, 2, 3}
std::vector a(std::from_range, xs);

// same
std::vector b{std::from_range, xs};

// this is a vector of list<int>
std::vector c{xs};

// and this is ill-formed
std::vector d(xs);
```
:::

For the sequence containers (mainly `vector`, but this also applies to `deque` and others as well), there's one more range-based operation which is useful:

::: bq
```diff
struct Container {
    template <typename Iterator>
    void insert(const_iterator, Iterator, Iterator);
+   template <typename Range>
+   void insert_range(const_iterator, Range&&);

    void push_back(value_type const&);
    void push_back(value_type&&);
+   template <typename Range>
+   void push_back_range(Range&&);
};
```
:::

Here, `c.push_back_range(r)` is roughly equivalent to `c.insert_range(c.end(), r)` just shorter to type. Which is already kind of nice in that it's a fairly common thing to write. But it also has one other benefit: fewer type requirements. `insert` requires the type to be copy-assignable, but `push_back` only requires copy-constructible - and the range equivalents of these member functions would work the same way. 

## Designing `ranges::to`

With the standard library containers now having range-based operations, and thus providing a model for how containers outside of the standard library should behave, we can now really answer the question of how `ranges::to` can work. And the range-based container operations offer an easy answer to this question (and one which is really the standard C++ answer for the question of how should an algorithm construct a `C` from some other type `R`): try the constructor first.

That is, `ranges::to<C>(r, args...)`, for some type cv-unqualified type `C` that is not a `view`, should evaluate the first one of these expressions that is valid:

1. `C(r, args...)`
2. `C(std::from_range, r, args...)`
3. If `C` is a range of ranges, and `r` is a range of ranges, then let `r2c` be `r | transform(to<range_value_t<C>>)` in the following:
    a. `C(r2c, args...)`
    b. `C(std::from_range, r2c, args...)`

And that's already... pretty good! This would cover collecting any pipeline into a standard library container (the third step there handles the case of trying to converting something like a range of range of `T` into a `vector<vector<T>>`). But what it would not yet do is provide a way of collecting a pipeline into some user-defined container that adheres to the C++98-through-C++17 notion of generic iterator-pair construction but not yet this new notion of generic range construction. To still work with legacy containers, we need some kind of fallback that is not based on these constructors. 

There really are only three fallbacks that we can try:

* use the iterator-pair constructor, if our range is actually a common range whose iterators are C++17 iterators. A version of that is using the iterator-pair constructor if `views::common(r)` is valid, to force common-ness, although this would be fairly expensive, and we would want to avoid that.
* use the `ranges::copy` solution presented earlier, with some kind of insertion iterator.
* provide an ADL customization point, separate from the constructor customization point already offered.

The first option there would be:

::: bq
```cpp
template <range C, common_range R, typename... Args>
    requires constructible_from<C, iterator_t<R>, iterator_t<R>, Args...>
          && @*cpp17-input-iterator*@<iterator_t<R>>
auto to(R&& r, Args&&... args) -> C {
    return C(ranges::begin(R), ranges::end(r), FWD(args)...);
}
```
:::

and the second option there might look something like this:

::: bq
```cpp
template <typename C, typename I>
concept @*push-back-from*@ = requires (C c, I it) {
    c.push_back(*it);
};

template <typename C, typename I>
concept @*insert-from*@ = requires (C c, I it) {
    c.insert(std::ranges::end(c), *it);
};

template <range C, range R, typename... Args>
    requires constructible_from<C, Args...>
          && (@*push-back-from*@<C, iterator_t<R>>
              || @*insert-from*@<C, iterator_t<R>>)
auto to(R&& r, Args&&... args) -> C {
    C c(FWD(args)...);
    if constexpr (requires (size_t s) { c.reserve(s); }) {
        c.reserve(ranges::distance(r));
    }
    
    auto out_iterator = [&]{
        if constexpr (@*push-back-from*@<C, iterator_t<R>>) {
            return back_inserter(c);
        } else {
            return inserter(c, ranges::end(c));
        }
    }();
    ranges::copy(r, out_iterator);
    
    return c;
}
```
:::

This is... okay, but at least importantly it allows the efficient solution for standard library containers and also provides a path for external containers to do the same for themselves. 