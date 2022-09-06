---
title: "Member `visit` and `apply`"
document: P2637R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

The standard library currently has three free function templates: `std::visit`, `std::apply`, and `std::visit_format_arg`. The goal of this paper is to add member function versions of each of them, simply for ergonomic reasons. This paper adds no new functionality that did not exist before.

## `std::visit`

`std::visit` is a variadic function template, which is the correct design since binary (and more) visitation is a useful and important piece of functionality. However, the common case is simply unary visitation. Even in that case, however, a non-member function was a superior implementation choice for forwarding const-ness and value category [^1].

[^1]: A single non-member function template is still superior to four member function overloads due to proper handling of certain edge cases. See the section on [SFINAE-friendly](https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2021/p0847r7.html#sfinae-friendly-callables) for more information.

But this decision logic changes in C++23 with the introduction of deducing `this` [@P0847R7]. Now, it is possible to implement unary `visit` as a member function without any loss of functionality. We simply gain better syntax:

::: cmptable
### Existing
```cpp
std::visit(overload{
  [](int i){ std::print("i={}\n", i); },
  [](std::string s){ std::print("s={:?}\n", s); }
}, value);
```

### Proposed
```cpp
value.visit(overload{
  [](int i){ std::print("i={}\n", i); },
  [](std::string s){ std::print("s={:?}\n", s); }
});
```
:::

## `std::apply`

`std::apply`, also added in C++17, is also a non-member function template. It takes a single _`tuple-like`_ object and a callable, and its interface otherwise mirrors `std::variant`. I am not sure why it takes the function first and the tuple second, even though the tuple is the subject of the operation.

`std::apply` originally needed to be a non-member function for one of the same reasons as `std::visit`: proper forwarding of const-ness and value category. But, as with `std::visit`, this can now easily be made a member function template. It's just that we have to add it to multiple types: `pair`, `tuple`, `array`, and `subrange`.

::: cmptable
### Existing
```cpp
int sum = std::apply(
  [](auto... args){
    return (0 + ... + args);
  },
  elements);
```

### Proposed
```cpp
int sum = elements.apply([](auto... args){
  return (0 + ... + args);
});
```
:::

## `std::visit_format_arg`

One of the components of the format library is `basic_format_arg<Context>` (see [format.arg]{.sref}), which is basically a `std::variant`. As such, it also needs to be visited in order to be used. To that end, the library provides:

::: bq
```cpp
template<class Visitor, class Context>
  decltype(auto) visit_format_arg(Visitor&& vis, basic_format_arg<Context> arg);
```
:::

But here, the only reason `std::visit_format_arg` is a non-member function was to mirror the interface for `std::visit`. There is neither multiple visitation nor forwarding of value category or const-ness here. It could always have been a member function without any loss of functionality. With deducing `this`, it can even be by-value member function.

This example is from the standard itself:

::: cmptable
### Existing
```cpp
auto format(S s, format_context& ctx) {
  int width = visit_format_arg([](auto value) -> int {
    if constexpr (!is_integral_v<decltype(value)>)
      throw format_error("width is not integral");
    else if (value < 0 || value > numeric_limits<int>::max())
      throw format_error("invalid width");
    else
      return value;
    }, ctx.arg(width_arg_id));
  return format_to(ctx.out(), "{0:x<{1}}", s.value, width);
}
```

### Proposed
```cpp
auto format(S s, format_context& ctx) {
  int width = ctx.arg(width_arg_id).visit([](auto value) -> int {
    if constexpr (!is_integral_v<decltype(value)>)
      throw format_error("width is not integral");
    else if (value < 0 || value > numeric_limits<int>::max())
      throw format_error("invalid width");
    else
      return value;
    });
  return format_to(ctx.out(), "{0:x<{1}}", s.value, width);
}
```
:::

The proposed name here is just `visit` (rather than `visit_format_arg`), since as a member function we don't need the longer name for differentiation.

## Implementation

In each case, the implementation is simple: simply redirect to the corresponding non-member function. Member `visit`, for instance:

::: bq
```cpp
template <class... Types>
class variant {
public:
  template <class Self, class Visitor>
    requires convertible_to<add_pointer_t<Self>, variant const*>
  constexpr auto visit(this Self&& self, Visitor&& vis) -> decltype(auto) {
    return std::visit(std::forward<Visitor>(vis), std::forward<Self>(self));
  }
};
```
:::

The constraint is to reject those cases where a type might inherit privately inherit from `variant`. Those cases aren't supported by `std::variant` either.

# Wording

Add to [pairs.pair]{.sref}:

::: bq
```diff
namespace std {
  template<class T1, class T2>
  struct pair {
    // ...

    constexpr void swap(pair& p) noexcept(see below);
    constexpr void swap(const pair& p) const noexcept(see below);

+   template<class Self, class F>
+     constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
  };
}
```
:::

And to [pairs.pair]{.sref} after the definitions of `swap`:

::: bq
::: addu
```
template<class Self, class F>
  constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
```

[55]{.pnum} *Effects*: equivalent to `return std::apply(std::forward<F>(f), std::forward<Self>(self));`
:::
:::


Add to [tuple.tuple]{.sref}:

::: bq
```diff
namespace std {
  template<class... Types>
  class tuple {
  public:
    // ...

    template<tuple-like UTuple>
      constexpr tuple& operator=(UTuple&&);
    template<tuple-like UTuple>
      constexpr const tuple& operator=(UTuple&&) const;

    // [tuple.swap], tuple swap
    constexpr void swap(tuple&) noexcept(see below);
    constexpr void swap(const tuple&) const noexcept(see below);

+   // [tuple.tuple.apply], calling a function with a tuple of arguments
+   template<class Self, class F>
+     constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
  };
}
```
:::

And a new clause [tuple.tuple.apply] after [tuple.swap]{.sref}:

::: bq
::: addu
```
template<class Self, class F>
  constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
```

[1]{.pnum} *Effects*: equivalent to `return std::apply(std::forward<F>(f), std::forward<Self>(self));`
:::
:::

Add to [variant.variant.general]{.sref}:

::: bq
```diff
namespace std {
  template<class... Types>
  class variant {
  public:
    // ...

    // [variant.status], value status
    constexpr bool valueless_by_exception() const noexcept;
    constexpr size_t index() const noexcept;

    // [variant.swap], swap
    constexpr void swap(variant&) noexcept(see below);

+   // [variant.visit], visitation
+   template<class Self, class Visitor>
+     constexpr $see below$ visit(this Self&&, Visitor&&);
+   template<class R, class Self, class Visitor>
+     constexpr R visit(this Self&&, Visitor&&);
  };
}
```
:::

Add to [variant.visit]{.sref}, after the definition of non-member `visit`:

::: bq
::: addu
```
template<class Self, class Visitor>
  constexpr $see below$ visit(this Self&& self, Visitor&& vis);
template<class R, class Self, class Visitor>
  constexpr R visit(this Self&& self, Visitor&& vis);
```

[9]{.pnum} *Effects*: Equivalent to `return std::visit(std::forward<Visitor>(vis), std::forward<Self>(self))` for the first form and `return std::visit<R>(std::forward<Visitor>(vis), std::forward<Self>(self))` for the second form.
:::
:::

Change the example in [format.context]{.sref}/8:

::: bq
```diff
struct S { int value; };

template<> struct std::formatter<S> {
  size_t width_arg_id = 0;

  // Parses a width argument id in the format { digit }.
  constexpr auto parse(format_parse_context& ctx) {
    auto iter = ctx.begin();
    auto get_char = [&]() { return iter != ctx.end() ? *iter : 0; };
    if (get_char() != '{')
      return iter;
    ++iter;
    char c = get_char();
    if (!isdigit(c) || (++iter, get_char()) != '}')
      throw format_error("invalid format");
    width_arg_id = c - '0';
    ctx.check_arg_id(width_arg_id);
    return ++iter;
  }

  // Formats an S with width given by the argument width_­arg_­id.
  auto format(S s, format_context& ctx) {
-   int width = visit_format_arg([](auto value) -> int {
+   int width = ctx.arg(width_arg_id).visit([](auto value) -> int {
      if constexpr (!is_integral_v<decltype(value)>)
        throw format_error("width is not integral");
      else if (value < 0 || value > numeric_limits<int>::max())
        throw format_error("invalid width");
      else
        return value;
-     }, ctx.arg(width_arg_id));
+     });
    return format_to(ctx.out(), "{0:x<{1}}", s.value, width);
  }
};

std::string s = std::format("{0:{1}}", S{42}, 10);  // value of s is "xxxxxxxx42"
```
:::

Add to [format.arg]{.sref}:

::: bq
```diff
namespace std {
  template<class Context>
  class basic_format_arg {
    // ...
  public:
    basic_format_arg() noexcept;

    explicit operator bool() const noexcept;

+   template<class Visitor>
+     decltype(auto) visit(this basic_format_arg arg, Visitor&& vis);
  };
}
```
:::

And:

::: bq
```
explicit operator bool() const noexcept;
```
[15]{.pnum} *Returns*: `!holds_­alternative<monostate>(value)`.

::: addu
```
template<class Visitor>
  decltype(auto) visit(this basic_format_arg arg, Visitor&& vis);
```

[16]{.pnum} *Effects*: Equivalent to: `return arg.value.visit(forward<Visitor>(vis));`
:::
:::

Add to [array.overview]{.sref}:

::: bq
```diff
namespace std {
  template<class T, size_t N>
  struct array {
    // ...

    // element access
    constexpr reference       operator[](size_type n);
    constexpr const_reference operator[](size_type n) const;
    constexpr reference       at(size_type n);
    constexpr const_reference at(size_type n) const;
    constexpr reference       front();
    constexpr const_reference front() const;
    constexpr reference       back();
    constexpr const_reference back() const;

    constexpr T *       data() noexcept;
    constexpr const T * data() const noexcept;

+   // function application
+   template<class Self, class F>
+     constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
  };
}
```
:::

Add to [array.members]{.sref}:

::: bq
```
constexpr void swap(array& y) noexcept(is_nothrow_swappable_v<T>);
```
[4]{.pnum} *Effects*: Equivalent to `swap_­ranges(begin(), end(), y.begin())`.

[5]{.pnum} [*Note 1*: Unlike the swap function for other containers, `array​::​swap` takes linear time, can exit via an exception, and does not cause iterators to become associated with the other container. — *end note*]

::: addu
```
template<class Self, class F>
  constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
```

[6]{.pnum} *Effects*: equivalent to `return std::apply(std::forward<F>(f), std::forward<Self>(self));`
:::
:::

Add to [range.subrange.general]{.sref}:

::: bq
```diff
namespace std::ranges {
  template<input_­or_­output_­iterator I, sentinel_­for<I> S = I, subrange_kind K =
      sized_­sentinel_­for<S, I> ? subrange_kind::sized : subrange_kind::unsized>
    requires (K == subrange_kind::sized || !sized_­sentinel_­for<S, I>)
  class subrange : public view_interface<subrange<I, S, K>> {
  public:
    // ...

    constexpr bool empty() const;
    constexpr make-unsigned-like-t<iter_difference_t<I>> size() const
      requires (K == subrange_kind::sized);

    [[nodiscard]] constexpr subrange next(iter_difference_t<I> n = 1) const &
      requires forward_­iterator<I>;
    [[nodiscard]] constexpr subrange next(iter_difference_t<I> n = 1) &&;
    [[nodiscard]] constexpr subrange prev(iter_difference_t<I> n = 1) const
      requires bidirectional_­iterator<I>;
    constexpr subrange& advance(iter_difference_t<I> n);

+   template<class Self, class F>
+     constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
  };
}
```
:::

Add to [range.subrange.access]{.sref}

::: bq
::: addu
```
template<class Self, class F>
  constexpr decltype(auto) apply(this Self&& self, F&& f) noexcept($see below$);
```

[11]{.pnum} *Effects*: equivalent to `return std::apply(std::forward<F>(f), std::forward<Self>(self));`
:::
:::
