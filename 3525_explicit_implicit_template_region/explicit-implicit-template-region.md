---
title: "Explicit Implicit Template Regions"
document: P3525R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

[@P1061R9] introduced the ability to declare packs inside of structured bindings and, further more, even proposed the ability to do so outside of any templated context:

::: std
```cpp
struct Point { int x, y; };

// not a template
auto sum(Point p) -> int {
  // yet here is a pack
  auto [... parts] = p;

  // that I can fold over
  return (... + parts);
}
```
:::

This is useful (and implemented! and worded!), but it has some surprising consequences. In order for the feature to work in the ways that users would expect, everything after the declaration of `parts` above must become, implicitly, a template. See that paper for a more detailed description with examples. Templates have different rules than non-templates in a variety of ways, but there is not much of a marker to differentiate this.

As such, the part of that proposal that allowed packs outside of templates was ripped out and [@P1061R10] was adopted in the Wrocław meeting, requiring packs in structured bindings to be declared inside of a template context. This is simpler in a way, but requires users to just... arbitrarily make their code into a template.

For instance, you want to write the above code, but you cannot. So you have to do something like this:

::: cmptable
### A Template (for no reason)
```cpp
template <class>
auto sum(Point p) -> int {
  auto [... parts] = p;
  return (... + parts);
}
```

### A Generic Lambda (for no reason)
```cpp
auto sum(Point p) -> int {
  return [&]<class T=void>(){
    auto [... parts] = p;
    return (... + parts);
  }();
}
```
:::

Both of these options are bad. Turning the whole function into a template opens up to the potential of multiple instantiations, if you want to put the definition in a header you have to explicitly instantiate the template in the source file, and so forth. Wrapping the contents in an immediately invoked, generic lambda is... better. It avoids many of the problems of the unnecessary template. But it introduces a new function scope, which interacts badly if the body of the function wants to _conditionally_ return. For instance, if I want to write something like:

::: std
```cpp
auto some_function(Point p) -> bool {
  if (/* some condition */) {
    // what I want to write is this
    auto [... parts] = p;
    if (foo(parts...)) {
      return false;
    }

    // but I would have to write something like... this?
    auto ret = [&]<class T=void>() -> optional<bool> {
      auto [...parts] = p;
      if (foo(parts...)) {
        return false;
      }
      return nullopt;
    }();
    if (ret) return *ret;
  } else {
    // do some other thing
  }
}
```
:::

In general, I want to be able to write code that directly expresses user intent. Not come up with workarounds for not being able to do so.

# Proposal

The [@P1061R9] design introduced the concept of an _implicit_ template region. Let's just add an _explicit_ implicit template region. We can copy the syntactic idea of a `consteval` block as introduced in [@P3289R0]:

::: std
```cpp
auto sum(Point p) -> int {
  auto [... bad_parts] = p; // error: not in a template

  template {
    auto [... good_parts] = p; // OK, in a template (explicitly)
    return (... + good_parts);
  }
}
```
:::

We still have to be explicit about being in a template, but we can do so in a significantly more light-weight way: the template region is localized to the function body (as in [@P1061R9]) and without introducing an extra function scope that interferes with returns and coroutines (also as in [@P1061R9]).

I believe this addresses all the implementor concerns with the original design. It also provides a path forward to eventually removing the block, if so desired. It additionally provides a path forward to answering more complicated questions with other language features (like member packs [@P3115R0]).
