---
title: "Member `visit`"
document: P2637R3
date: today
audience: LWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: accepted
---

# Revision History

Since [@P2637R2], wording changes. Added note on feature-test macro after LWG discussion.

Since [@P2637R1], deprecating `std::visit_format_arg` and thus adding a feature-test macro.

Since [@P2637R0], dropped `apply`, added member `visit<R>` to `basic_format_arg`, and added support for types privately inheriting from `std::variant` for member `visit` and `visit<R>`

# Introduction

The standard library currently has two free function templates for variant visitation: `std::visit` and `std::visit_format_arg`. The goal of this paper is to add member function versions of each of them, simply for ergonomic reasons. This paper adds no new functionality that did not exist before.

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

In an [LEWG telecon](https://wiki.edg.com/bin/view/Wg21telecons2023/P2637#Library-Evolution-2023-04-11), since adding a member `visit` to `basic_format_arg` is just strictly better than `std::visit_format_arg`, there's no reason to preserve the existing one (which was only added as a non-member function for consistency with `std::visit`), so LEWG preferred to deprecate `std::visit_format_arg` as part of this paper with a vote of:

|SF|F|N|A|SA|
|-|-|-|-|-|
|1|8|5|1|1|

Because `std::visit_format_arg` is being deprecated, now we do require a feature-test macro for this change - which previously wouldn't have needed one.

## Implementation

In each case, the implementation is simple: simply redirect to the corresponding non-member function. Member `visit`, for instance:

::: bq
```cpp
template <class... Types>
class variant {
public:
  template <int=0, class Self, class Visitor>
  constexpr auto visit(this Self&& self, Visitor&& vis) -> decltype(auto) {
    return std::visit(std::forward<Visitor>(vis), (copy_cvref_t<Self, variant>&&)self);
  }

  template <class R, class Self, class Visitor>
  constexpr auto visit(this Self&& self, Visitor&& vis) -> decltype(auto) {
    return std::visit<R>(std::forward<Visitor>(vis), (copy_cvref_t<Self, variant>&&)self);
  }
};
```
:::

`copy_cvref_t<A, B>` is a metafunction that simply pastes the const/ref qualifiers from `A` onto `B`. It will be added by [@P1450R3].

The C-style cast here is deliberate because `variant` might be a private base of `Self`. This is a case that `std::visit` does not support, but LEWG preferred if member `visit` did.

There is also an extra leading `int=0` template parameter for the overload that just calls `std::visit` (rather than `std::visit<R>`). This is because, unlike with the non-member functions, an ambiguity would otherwise arise if you attempted to do:

```cpp
using State = std::variant<A, B, C>;

State state = /* ... */;
state = std::move(state).visit<State>(f);
```

With non-member `visit`, there's no real possible ambiguity because the function goes first. But here, unless we protect the non-return-type taking overload, `Self` could deduce as `State`, which would then be a perfectly valid overload. And this pattern isn't rare either - so it's important to support. The added `int=0` parameter ensures that only the first overload is viable for `v.visit(f)` and only the second is viable for `v.visit<R>(f)`.

# Wording

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
+     constexpr decltype(auto) visit(this Self&&, Visitor&&);
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
  constexpr decltype(auto) visit(this Self&& self, Visitor&& vis);
```

[9]{.pnum} Let `V` be `$OVERRIDE_REF$(Self&&, $COPY_CONST$(remove_reference_t<Self>, variant))` ([forward]).

[#]{.pnum} *Constraints*: The call to `visit` does not use an explicit `$template-argument-list$` that begins with a type `$template-argument$`.

[#]{.pnum} *Effects*: Equivalent to: `return std::visit(std::forward<Visitor>(vis), (V)self);`

```
template<class R, class Self, class Visitor>
  constexpr R visit(this Self&& self, Visitor&& vis);
```

[#]{.pnum} Let `V` be `$OVERRIDE_REF$(Self&&, $COPY_CONST$(remove_reference_t<Self>, variant))` ([forward]).

[#]{.pnum} *Effects*: Equivalent to: `return std::visit<R>(std::forward<Visitor>(vis), (V)self);`
:::
:::

Remove from [format.syn]{.sref} (this is now deprecated):

::: bq
```diff
namespace std {
  // ...
  // [format.arguments], arguments
  // [format.arg], class template basic_format_arg
  template<class Context> class basic_format_arg;

- template<class Visitor, class Context>
-   decltype(auto) visit_format_arg(Visitor&& vis, basic_format_arg<Context> arg);
  // ...
}
```
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
+   template<class R, class Visitor>
+     R visit(this basic_format_arg arg, Visitor&& vis);

  };
}
```
:::

And:

::: bq
```
explicit operator bool() const noexcept;
```
[7]{.pnum} *Returns*: `!holds_­alternative<monostate>(value)`.

::: addu
```
template<class Visitor>
  decltype(auto) visit(this basic_format_arg arg, Visitor&& vis);
```

[#]{.pnum} *Effects*: Equivalent to: `return arg.value.visit(std::forward<Visitor>(vis));`

```
template<class R, class Visitor>
  R visit(this basic_format_arg arg, Visitor&& vis);
```

[#]{.pnum} *Effects*: Equivalent to: `return arg.value.visit<R>(std::forward<Visitor>(vis));`
:::
:::

And remove [format.arg]{.sref}/13:

::: bq
::: rm
```
template<class Visitor, class Context>
  decltype(auto) visit_format_arg(Visitor&& vis, basic_format_arg<Context> arg);
```
[13]{.pnum} *Effects*: Equivalent to: `return visit(std​::​forward<Visitor>(vis), arg.value);`
:::
:::

Add a new clause in [depr]{.sref}: [depr.format]:

::: bq
::: addu
[1]{.pnum} The header `<format>` has the following addition:
```
namespace std {
  template<class Visitor, class Context>
    decltype(auto) visit_format_arg(Visitor&& vis, basic_format_arg<Context> arg);
}
```

```
template<class Visitor, class Context>
  decltype(auto) visit_format_arg(Visitor&& vis, basic_format_arg<Context> arg);
```
[2]{.pnum} *Effects*: Equivalent to: `return visit(std​::​forward<Visitor>(vis), arg.value);`
:::
:::

## Feature-test macro

Bump both `__cpp_lib_variant` and `__cpp_lib_format` in [version.syn]{.sref}

::: bq
```diff
- #define __cpp_lib_format @[202207L]{.diffdel}@ // also in <format>
+ #define __cpp_lib_format @[2023XXL]{.diffins}@ // also in <format>

- #define __cpp_lib_variant @[202106L]{.diffdel}@ // also in <variant>
+ #define __cpp_lib_variant @[2023XXL]{.diffins}@ // also in <variant>
```
:::

[There are three formatting papers that are going to be moved at the same time: P2510 (Formatting Pointers), P2757 (Type-checking Format Args), and this one. They should end up with three consecutive values for `__cpp_lib_format` (not all the same one): `202304L`, `202305L`, and `202306L`, respectively]{.ednote}


---
references:
    - id: P2637R0
      citation-label: P2637R0
      title: "Member `visit` and `apply`"
      author:
        - family: Barry Revzin
      issued:
        - year: 2022
          month: 09
          day: 17
      URL: http://www.open-std.org/jtc1/sc22/wg21/docs/papers/2022/p2637r0.html
---
