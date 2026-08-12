---
title: "Extending constant template parameter support by customizing `std::meta::reflect_constant`"
document: P4340R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
tag: reflection
---

# Introduction

This is a follow-up to [@P2484R0]{.title} and [@P3380R1]{.title} (the latter of which starts with a useful reading list), and is a new solution to that problem building upon three insights:

* Richard Smith's initial recognition that this is a serialization/deserialization problem in order to avoid ODR issues,
* Faisal Vali's idea that reflection provides a good answer for how to serialize arbitrary elements, and
* Dan Katz's experimentation that eventually led to the `std::define_static_meow` functions in [@P3491R3]{.title}

The goal here is to allow types to opt-in to being usable as constant template parameters in a way that's forward-looking to containers as well. This proposal will also make the following standard library types usable: `std::optional`, `std::expected`, `std::variant`, `std::tuple`, `std::string_view`, `std::span`, most types in `<chrono>`, `std::complex`, the comparison categories, `std::reference_wrapper`, `std::inplace_vector`, and `std::bitset`. Additionally, string literals will also become usable.

# Design

One of the many consteval functions added to the standard library by [@P2996R13]{.title} was `std::meta::reflect_constant(v)`. This is a very interesting function in that, if `v` has class type, this returns a reflection representing _the_ template parameter object that is template-argument-equivalent to `v`.

That is, for all class types that are today usable as constant template parameters, this comparison holds (let's agree to ignore that I'm not using `std::addressof`):

::: std
```cpp
template <auto V>
constexpr void const* addr = &V;

static_assert(addr<E> == &[: std::meta::reflect_constant(E) :]);
```
:::

Ultimately, the whole problem of how to opt an arbitrary type with non-public subobjects into being usable as a constant template parameter is about this question of how to produce the template parameter object. What if we cut out the middle-man and simply allowed (forced) types to _directly_ produce that template parameter object?

Here is a very simple example:

::: std
```cpp
class Widget {
    // private, so not usable by default
    int data_;

public:
    constexpr Widget(int d) : data_(d) { }

    consteval auto reflect_constant() const -> std::meta::info;
};

template <int D> constexpr auto impl = Widget(D);

consteval auto Widget::reflect_constant() const -> std::meta::info {
    return substitute(^^impl, {std::meta::reflect_constant(data_)});
}
```
:::

`Widget` is defining how to produce a template parameter object: `reflect_constant` returns a reflection representing a static storage duration object. It is up to `Widget` to figure out how to do that, but once it does so, we have already solved all the other problems in this space:

* template-argument-equivalence is chosen by the `Widget` author based on how they define the object being returned (in this case, by comparing the `int`s).
* the serialization/deserialization process is implicit here — we serialize in `Widget::reflect_constant` (each template argument is the next element we're serializing) and we deserialize in the initialization of that variable template (where likewise each template argument is the next element we're deserializing)
* the potential ODR issues are avoided because we're defining _the_ template parameter object, so it will be _that_ object that gets used.

`std::meta::reflect_constant(v)` will then be extended to first invoke `v.reflect_constant()` for class types that opt in before falling back to the normal C++20 structural rules.

## Normalizing

Let's go through two examples where we do something non-trivial: we want to do some normalization on the way in, so that we have fewer or more predictable specializations. Previous papers in this space brought up two examples:

* a `Fraction` type that is only in lowest terms
* a `SmallString` container that doesn't care about trailing data.

Let's start with `Fraction`. We need to make sure that we serialize it in lowest terms, so the arguments we're `substitute()`-ing will have to be reduced:

::: std
```cpp
struct Frac {
    int numer;
    int denom;
    consteval auto reflect_constant() const -> std::meta::info;
};
template <int N, int D> constexpr auto interned = Frac{N, D};

consteval auto Frac::reflect_constant() const -> std::meta::info {
    int g = std::gcd(numer, denom);
    return substitute(^^interned, {
        std::meta::reflect_constant(numer / g),
        std::meta::reflect_constant(denom / g),
    });
}
```
:::

And the result we will end up with is:

::: std
```cpp
template <auto V> inline constexpr auto const& ref = V;

// the template parameter object is {1, 2}, that's all we see
static_assert(ref<Frac{2, 4}>.numer == 1);
static_assert(ref<Frac{2, 4}>.denom == 2);

// there is only one template parameter object, so these
// alternate representations have the same address
static_assert(&ref<Frac{2, 4}> == &ref<Frac{3, 6}>);
```
:::

Similarly, the `SmallString` container I showed in [@P3380R1] wanted to ensure that regardless of what path you get to the same "value" that you only see the real value, and not any trailing characters. We can do that too:

::: std
```cpp
class SmallString {
    char data_[32];
    int length_ = 0; // always <= 32

public:
    SmallString() = default;

    constexpr SmallString(std::initializer_list<char> elems)
        : data_(), length_(elems.size())
    {
        std::copy(elems.begin(), elems.end(), data_);
    }

    constexpr auto data() const -> char const* {
        return data_;
    }

    constexpr auto push_back(char c) -> void {
        assert(length_ < 31);
        data_[length_] = c;
        ++length_;
    }

    constexpr auto pop_back() -> void {
        assert(length_ > 0);
        --length_;
    }

    constexpr auto operator[](size_t idx) const -> char {
      return data_[idx];
    }

    consteval auto reflect_constant() const -> std::meta::info;
};

template <char... Cs>
inline constexpr SmallString impl = {Cs...};

consteval auto SmallString::reflect_constant() const -> std::meta::info {
    std::vector<std::meta::info> elems;
    for (size_t i = 0; i != length_; ++i) {
        elems.push_back(std::meta::reflect_constant(data_[i]));
    }
    return substitute(^^impl, elems);
}
```
:::

We simply only serialize `data[0:length]` and not all of `data[0:32]`. The result is that the trailing data gets dropped and we end up with the same object:

::: std
```cpp
constexpr auto f() -> SmallString {
    auto s = SmallString();
    s.push_back('x');
    return s;
}

constexpr auto g() -> SmallString {
    auto s = f();
    s.push_back('y');
    s.pop_back();
    return s;
}

static_assert(&ref<f()> == &ref<g()>);

// It doesn't matter that g()[1] == 'y', normalizing through the
// template parameter object gives us all trailing nulls
static_assert(ref<g()>[1] == '\0');
```
:::

As a result, this example just isn't an ODR issue, because `bad<f()>` and `bad<g()>` both evaluate the same way — they return `1`.

::: std
```cpp
template <SmallString S>
constexpr auto bad() -> int {
  if constexpr (S.data()[1] == 'y') {
    return 0;
  } else {
    return 1;
  }
}
```
:::

## Defaulting `reflect_constant`

While it's important to be able to support the kind of arbitrary selection of serialization and normalization that I've shown in the previous three examples, in practice, for a large number of types, this kind of genericity is unnecessary. In fact, a lot of time, all we want to do is have a simple member-wise (or really subobject-wise) template-argument-equivalence.

That's the C++20 rule we have today, except that rule requires that all the subobjects be `public`. That's necessary, since we can't otherwise distinguish between `std::string_view` and `std::string`, and those would definitely want to have very different rules around equivalence (the former comparing pointers, the latter comparing contents). But what if I want to both have `private` members and also have member-wise equivalence?

Take `string_view`. It's straightforward enough to implement `reflect_constant` in a way that does what we want:

::: std
```cpp
template <class T, auto... Vals>
constexpr auto impl = T(Vals...);

class string_view {
    char const* ptr_;
    size_t len_;

public:
    constexpr string_view(char const*, size_t);

    consteval auto reflect_constant() const -> std::meta::info {
        return substitute(^^impl, {
            ^^string_view,
            std::meta::reflect_constant(ptr_),
            std::meta::reflect_constant(len_),
        });
    }
};
```
:::

This works fine, and we already have the constructor that we need to support this. But this is a lot of ceremony for what is basically the default behavior: serialize and then deserialize all of my members. Since that is a very reasonable default behavior, we should just allow `default`-ing this. This does mirror very nicely the way that other defaulted functions behave:

::: std
```cpp
class string_view {
    char const* ptr_;
    size_t len_;

public:
    constexpr string_view(char const*, size_t);

    consteval auto reflect_constant() const -> std::meta::info = default;
};
```
:::

We'll want defaulting behavior for `reflect_constant` to be similar to defaulting behavior for other functions, like the copy constructor and the comparison operators: for dependent types, if one of the subobjects isn't usable as a constant template parameter (whether it's not C++20 structural or doesn't customize `reflect_constant`), the resulting customization is `delete`d rather than being ill-formed. That means that for a class template like `std::tuple`, we can simply `default` the implementation and not have to worry about providing suitable constraints.

Note that a consequence of being able to `default` this function is that you should also be able to `delete` it, which means that a type could (if so desired) simply opt out of being usable as a constant template parameter.

Also, the actual implementation details of defaulting `reflect_constant` would differ from those of explicitly providing it. If it is user-provided, the implementation will of course invoke and use that result as the template parameter object. But if it is defaulted, then effectively the implementation just tracks an extra bit on the type that tells it to ignore the usual access rule for C++20 class types as non-type template parameters. It wouldn't have to change the rules for template-argument-equivalence for that case.

## Concrete Details

Currently, the rule is that `f<x>` and `f<y>` are the same specialization when `x` and `y` are template-argument-equivalent. For which the relevant rule from [temp.type]{.sref} for class types is:

::: std
[2]{.pnum} Two values are *template-argument-equivalent* if they are of the same type and

* [2.1]{.pnum} [...]
* [2.10]{.pnum} they are of union type and either they both have no active member or they have the same active member and their active members are template-argument-equivalent, or
* [2.11]{.pnum} [...], or
* [2.12]{.pnum} they are of class type and their corresponding direct subobjects and reference members are template-argument-equivalent.
:::

This proposal extends the those two bullets such that two values `x` and `y` of class type `T` (including union type) are template-argument equivalent if `T` provides a non-default `reflect_constant` customization and `x.reflect_constant() == y.reflect_constant()` (i.e. are reflections representing the same object).

In order for that to be viable, we need to impose strict (albeit obvious) restrictions on the return type of `reflect_constant`. For a class type `T`, `T::reflect_constant` must return a reflection representing either an object or a variable. Letting: `O` be either that object or the object defined by that variable:

* `O` has static storage duration,
* `O` is usable in constant expressions,
* `O` has the same linkage as `T` (with internal linkage we would end up with a TU-local specialization),
* `O` has type `T const` (this also guards against inheriting a customization — `Base`'s implementation would have to return a `Base const` object, which would be incompatible with the rules for a `Derived`).

`reflect_constant` must also be deterministic (same value in, same object out) and idempotent (given a value `x`, `x.reflect_constant()` and `[: x.reflect_constant() :].reflect_constant()` must be the same object). Idempotence we can actually check as part of the implementation, in the same way that template variable construction does an extra copy right now.

We'll have the same rules for the shape of the customization point for defaulting reasons as we have for defaulting the comparison operators. That is, it must be:

* `consteval`
* non-static member function,
* which returns `std::meta::info` or `auto`,
* have no non-object parameter, and
* the object parameter must have type either `T` or `T const&`.

Note that a type with `mutable` members is non-structural, regardless of any customization point.

## Standard Library Extensions

There is one standard library type that I'm aware of that would actually provide a bespoke implementation of `reflect_constant` (see below), but there are quite a few for whom `default`ing is the correct behavior. For the class templates below, `default`ing will do the right thing by opting in those types whose subobjects are usable as constant template parameters and not opting in those types whose subobjects are not usable (note that `std::pair` and `std::array` are absent from this list, but that is only because they are already usable as constant template parameters):

* `std::optional<T>`
* `std::expected<T, E>`
* `std::variant<Ts...>`
* `std::tuple<Ts...>`
* `std::basic_string_view<charT, Traits>` (all specializations)
* `std::span<T, Extent>` (all specializations)
* `std::reference_wrapper<T>` (all specializations)
* A lot of `<chrono>` types:
  * `duration<Rep, Period>`
  * `time_point<Clock, Duration>`
  * `day`
  * `month`
  * `year`
  * `weekday`
  * `weekday_indexed`
  * `weekday_last`
  * `month_day`
  * `month_day_last`
  * `month_weekday`
  * `month_weekday_last`
  * `year_month`
  * `year_month_day`
  * `year_month_day_last`
  * `year_month_weekday`
  * `year_month_weekday_last`
* The comparison categories — `std::partial_ordering`, `std::weak_ordering`, `std::strong_ordering`
* `std::bitset<N>`

For all of these types and class templates, adding the single line:

::: std
```cpp
consteval auto reflect_constant() const -> std::meta::info = default;
```
:::

Does the right thing and opts those types (or suitable specializations of those class templates) into being used as constant template parameters, with the correct semantics.

The only standard library type that we would add a non-defaulted `reflect_constant` to is `std::inplace_vector<T, N>`. Its customization point would look the same as `std::vector`'s (defined below).

## Future Extensions with Containers

Notably absent from the above list are allocating containers, most significantly `std::vector<T>` and `std::string`. That's because we do not yet have non-transient allocation and so cannot support them yet.

However, once we _do_ have non-transient allocation, this paper provides a design that lets us easily provide constant template parameter support for them. Here is `vector` for instance:

::: std
```cpp
template <class T, auto& R>
inline constexpr auto impl = T(std::from_range, R);

template <class T>
class vector {
public:
    consteval auto reflect_constant() const -> std::meta::info
        requires std::meta::is_structural_type(^^T)
    {
        return substitute(^^impl, {
            ^^vector,
            std::meta::reflect_constant_array(*this),
        });
    }
};
```
:::

Note that this implicitly normalizes capacity — as long as the elements are the same, we will choose the same specialization of `impl`, which means we will end up with the same object (which will have whatever capacity it has).

If for some reason, you actually _do_ want capacity to participate in all of this, that's easy too — just also pass in `capacity()` as an additional serialization argument.

## String Literals

One other type to cover not only isn't in the standard library, but it isn't even a class type: string literals. While `char const*` is usable as a template _parameter_, we cannot currently pass a string literal as a template _argument_.

The reason for this is that we do not ensure that `"hello"` is the same exact pointer across all translation units (and we will not start doing this now), and the result is that given a class template like:

::: std
```cpp
template <char const* P>
struct X { };
```
:::

There is no guarantee that `X<"hello">` is the same _type_ in every translation unit.

Now, [@P0424R2]{.title} already proposed the solution to this problem. Except that paper was dropped in favor of the more general [@P0732R2]{.title} in the C++20 timeframe. But we still cannot pass in string literals, so I'm resurrecting the ideas behind the original paper.

In modern C++26 reflection terms, the way we need this to work is that when we pass a string literal as a template argument:

::: std
```cpp
X<"hello">
```
:::

That it needs to evaluate instead as:

::: std
```cpp
X<[: std::meta::reflect_constant_string("hello") :]>
```
:::

And this now ensures that, across all translation units, `X<"hello">` is the same type. This is really the only way to ensure this property, and is the requirement for this to be allowed to work, so we should do it.

Note that it's not quite _that_ simple, because:

::: std
```cpp
constexpr char const* a = "holdup";
constexpr char const* b = a + 3;
constexpr char const* c = "dup";

X<b> dup1;
X<c> dup2;
```
:::

Do `dup1` and `dup2` have the same type? I would argue no, they do not. The transformation I expect is more like this:

::: std
```cpp
template <char... Cs>
inline constexpr char const lit[] = {Cs..., '\0'};

X<lit<'h', 'o', 'l', 'd', 'u', 'p'> + 3> dup1;
X<lit<'d', 'u', 'p'>> dup2;
```
:::

That is, just because the string contents of `a + 3` and `c` are the same (`"dup"`), the pointers themselves are different, and template-argument-equivalence should preserve that. This is essential for another issue to bring up...

## Normalization II

I talked about normalization earlier from the perspective of wanting to mutate an object down to its canonical representation, and only use that canonical representation as a template parameter object.

But there's another perspective of normalization to consider: simple aggregation. What happens here:

::: std
```cpp
struct Stuff {
    std::string_view s;
    int x;
};

template <Stuff S> int f();

int r = f<Stuff{.s="hello"sv, .x=1}>();
```
:::

`Stuff` has all-`public` members, all of whom are usable as constant template parameters, so this should just work without `Stuff` having to do anything. But in order for this to work:

* the `char const*` that the `string_view` owns, because it's a string literal, needs to be transformed through the string literal process described above
* then, the `string_view` needs to go through its (defaulted) `reflect_constant` opt-in

This all needs to basically... just happen as part of the template parameter object construction process. Otherwise composition completely breaks down.

This is also why the string literal rule has to have the form I described above. Imagine if `std::string_view`'s storage looked like this:

::: std
```cpp
class string_view {
    char const* begin_;
    char const* end_;
};
```
:::

In the above initialization, `string_view("hello"sv)` would have two pointers into the same string literal — so that needs to transform into `(p, p + 5)` where `p` points to a static storage duration array. It *cannot* turn into two pointers into *different* arrays. That would be horribly broken.

## Naming

The name I'm choosing for the customization point here is `reflect_constant`, since we're basically customizing `std::meta::reflect_constant`. There is precedent for using regular identifiers in language customization points already (`begin`, `end`, `get`, plus all the coroutine stuff). But with `= default`, it's possible that this might just be a bit too much — and we'll want to use a name that is decidedly not just an identifier.

The first paper in my constant template parameter trilogy, [@P2848R0]{.title}, used the name `operator template()`. Richard Smith also suggests the name `operator<>()`. Both are perfectly fine, and express what they do reasonably enough. Here are what those three (well, four, if I show `reflect_constant` both with and without an explicit return type) might look like in the wild in their typical usage (i.e. `default`ed):

::: std
```cpp
class string_view {
  char const* ptr_;
  size_t size_;

public:
  consteval auto reflect_constant() const -> std::meta::info = default;
  consteval auto reflect_constant() const = default;
  consteval operator template() const = default;
  consteval operator<>() const = default;
}
```
:::

In the original paper, `operator template()` was not invokable, which was something that some people didn't like, since it meant that you had to contrive some other way to test the function directly if that's what you wanted to do. In this case, `operator template()` would still be a customization point for `std::meta::reflect_constant()`, so there would still be an easy way to test the implication directly.

So those options exist as well, if preferred. I have a mild preference for sticking with `reflect_constant`, simply because it matches the reflection function name, but only mild, and would be quite happy with any name whatsoever as long as it means we get the feature.

## Implementation Experience

This has been implemented in [my fork](https://github.com/brevzin/llvm-project/commit/750c1763c183d9fb1bc0e6bb1c6a4adde11c9094) of Clang, and you can see it on [compiler explorer](https://compiler-explorer.com/z/WbYjs5EaK).