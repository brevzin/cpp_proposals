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

[@P1061R9] introduced the ability to declare packs inside of structured bindings and, furthermore, even proposed the ability to do so outside of any templated context:

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

Both of these options are bad. Turning the whole function into a template opens up to the potential of multiple instantiations, if you want to put the definition in a header you have to explicitly instantiate the template in the source file, and so forth. Wrapping the contents in an immediately invoked, generic lambda is... better. It avoids many of the problems of the unnecessary template. But it introduces a new function scope, which interacts badly if the body of the function wants to _conditionally_ return.

For example:

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

Alternatively, you could preemptively make your entire body `return [&]<class=void>(){ ... }();` even if only a small part of it wants to introduce a pack.

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

## Wording

Extend the grammar for statement in [stmt.pre]{.sref}:

::: std
```diff
  $statement$:
    $labeled-statement$
    $attribute-specifier-seq$@~opt~@ $expression-statement$
    $attribute-specifier-seq$@~opt~@ $compound-statement$
+   $attribute-specifier-seq$@~opt~@ $template-block$
    $attribute-specifier-seq$@~opt~@ $selection-statement$
    $attribute-specifier-seq$@~opt~@ $iteration-statement$
    $attribute-specifier-seq$@~opt~@ $jump-statement$
    $declaration-statement$
    $attribute-specifier-seq$@~opt~@ $try-block$
```
:::

And a corresponding new clause after [stmt.block]{.sref}, call it [stmt.template]:

::: std
::: addu
```
$template-block$:
  $compound-statement$
```

A *template block* introduces an explicit template region ([temp.pre]) encompassing the block scope introduced by the `$compound-statement$`.

::: example
```cpp
struct Point { int x, y; };

int magnitude(Point p) {
  template {
    auto [...good] = p; // OK, within explicit template region
    return (good * good + ...);
  }

  auto [...bad] = p; // error: p is not a templated entity
}
```
:::
:::
:::

Mark all entities within an an explicit template region as templated, in [temp.pre]{.sref}:

::: std
[8]{.pnum} An entity is *templated* if it is

* [#.#]{.pnum} a template,
* [#.#]{.pnum} an entity defined ([basic.def]) or created ([class.temporary]) in a templated entity,
* [#.#]{.pnum} a member of a templated entity,
* [#.#]{.pnum} an enumerator for an enumeration that is a templated entity, [or]{.rm}
* [#.#]{.pnum} the closure type of a lambda-expression ([expr.prim.lambda.closure]) appearing in the declaration of a templated entity[.]{.rm} [, or]{.addu}

::: addu
* [#.#]{.pnum} an entity defined or created within an explicit template region ([stmt.template]).
:::

[A local class, a local or block variable, or a friend function defined in a templated entity is a templated entity.]{.note}
:::

Which needs a point of instantiation at the end of [temp.point]{.sref}:

::: {.std .ins}
[*]{.pnum} For an explicit template region, the point of instantiation immediately follows the closing brace of the `$compound-statement$` of the `$template-block$`.
:::

## Feature-Test Macro

Introduce a new `__cpp_template_block` to [cpp.predefined]{.sref}:

::: std
```diff
+ __cpp_template_block 2025XXL
```
:::