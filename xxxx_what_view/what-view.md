---
title: "What is a `view`?"
document: DxxxxR0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
    - name: Tim Song
      email: <t.canens.cpp@gmail.com>
toc: true
---

# Introduction

C++20 Ranges introduced two main concepts for dealing with ranges: `range` and `view`. These notions were introduced way back in the original paper, "Ranges for the Standard Library" [@N4128] (though under different names than what we have now - what we now know as `range` and `view` were originally specified as `Iterable` and `Range`[^1]):

::: quote
[A Range] type is one for which we can call `begin()` and `end()` to yield an iterator/sentinel pair. (Sentinels are described below.) The [Range] concept says nothing about the type’s constructibility or assignability. Range-based standard algorithms are constrained using the [Range] concept.

[...]

The [View] concept is modeled by lightweight objects that denote a range of elements they do not own. A pair of iterators can be a model of [View], whereas a `vector` is not. [View], as opposed to [Range], requires copyability and assignability. Copying and assignment are required to execute in constant time; that is, the cost of these operations is not proportional to the number of elements in the Range.

The [View] concept refines the [Range] concept by additionally requiring following valid expressions for an object `o` of type `O`:

```cpp
// Constructible:
auto o1 = o;
auto o2 = std::move(o);
O o3; // default-constructed, singular
// Assignable:
o2 = o1;
o2 = std::move(o1);
// Destructible
o.~O();
```

The [View] concept exists to give the range adaptors consistent and predictable semantics, and memory and performance characteristics. Since adaptors allow the composition of range objects, those objects must be efficiently copyable (or at least movable). The result of adapting a [View] is a [View]. The result of adapting a container is also a [View]; the container – or any [Range] that is not already a [View] – is first converted to a [View] automatically by taking the container’s `begin` and `end`.
:::

The paper really stresses two points throughout:

* views are lightweight objects that refer to elements they do not own [^2]
* views are O(1) copyable and assignable

This design got muddled a bit when views ceased to require copyability, as a result of "Move-only Views" [@P1456R1]. As the title suggests, this paper relaxed the requirement that views be copyable, and got us to the set of requirements we have now in [range.view]{.sref}:

* views are O(1) move constructible, move assignable, and destructible
* views are either O(1) copy constructible/assignable or not copy constructible/assignable

But somehow absent from the discussion is: why do we care about views and range adaptors being cheap to copy and assign and destroy? This isn't just idle navel-gazing either, [@LWG3452] points out that requiring strict O(1) destruction has implications for whether `std::generator` [@P2168R3] can be a `view`. What can go wrong in a program that annotates a range as being a `view` despite not meeting these requirements? 

The goal of this paper is to provide good answers to these questions.


[^1]: This is why they're called _range_ adaptors rather than _view_ adaptors, perhaps that should change as well?
[^2]: except `views::single`