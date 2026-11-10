---
title: |-
  Extending constant template parameter support\
  via `operator template()`
document: P4340R1
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
tag: reflection
---

# Revision History {- .unlisted .collapsed}

Since [@P4340R0], reworked the customization point to `operator template` and added wording. `operator template` is now implicitly declared for all C++20 structural types, it cannot be named, and idempotence is diagnosed.

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

    consteval operator template() const;
};

template <int D> constexpr auto impl = Widget(D);

consteval Widget::operator template() const {
    return substitute(^^impl, {std::meta::reflect_constant(data_)});
}
```
:::

`Widget` is defining how to produce a template parameter object: `operator template` returns a reflection representing a static storage duration object. It is up to `Widget` to figure out how to do that, but once it does so, we have already solved all the other problems in this space:

* template-argument-equivalence is chosen by the `Widget` author based on how they define the object being returned (in this case, by comparing the `int`s).
* the serialization/deserialization process is implicit here — we serialize in `Widget::operator template` (each template argument is the next element we're serializing) and we deserialize in the initialization of that variable template (where likewise each template argument is the next element we're deserializing)
* the potential ODR issues are avoided because we're defining _the_ template parameter object, so it will be _that_ object that gets used.

`std::meta::reflect_constant(v)` will then be extended to first invoke `v.operator template()` for class types that opt in before falling back to the normal C++20 structural rules. This function is otherwise not utterable by the user. More on this in the [naming](#naming) section.

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
    consteval operator template() const;
};
template <int N, int D> constexpr auto interned = Frac{N, D};

consteval Frac::operator template() const {
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
        assert(length_ < 32);
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

    consteval operator template() const;
};

template <char... Cs>
inline constexpr SmallString impl = {Cs...};

consteval SmallString::operator template() const {
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

As a result, the example below just isn't an ODR issue, because `bad<f()>` and `bad<g()>` both evaluate the same way — they return `1`.

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

## Defaulting `operator template`

While it's important to be able to support the kind of arbitrary selection of serialization and normalization that I've shown in the previous three examples, in practice, for a large number of types, this kind of genericity is unnecessary. In fact, a lot of time, all we want to do is have a simple member-wise (or really subobject-wise) template-argument-equivalence.

That's the C++20 rule we have today, except that rule requires that all the subobjects be `public`. That's necessary, since we can't otherwise distinguish between `std::string_view` and `std::string`, and those would definitely want to have very different rules around equivalence (the former comparing pointers, the latter comparing contents). But what if I want to both have `private` members and also have member-wise equivalence?

Take `string_view`. It's straightforward enough to implement `operator template` in a way that does what we want:

::: std
```cpp
template <class T, auto... Vals>
constexpr auto impl = T(Vals...);

class string_view {
    char const* ptr_;
    size_t len_;

public:
    constexpr string_view(char const*, size_t);

    consteval operator template() const {
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

    consteval operator template() const = default;
};
```
:::

We'll want defaulting behavior for `operator template` to be similar to defaulting behavior for other functions, like the copy constructor and the comparison operators: for dependent types, if one of the subobjects isn't usable as a constant template parameter (whether it's not C++20 structural or doesn't customize `operator template`), the resulting customization is `delete`d rather than being ill-formed. That means that for a class template like `std::tuple`, we can simply `default` the implementation and not have to worry about providing suitable constraints.

And, like the copy constructor, `operator template` is implicitly declared. If a class doesn't declare one, it gets one anyway: it is deleted if any subobject is not `public` or not structural, and defaulted otherwise. That lets the language define a structural class type simply as a literal type with no `mutable` subobjects whose `operator template` isn't deleted. The only difference between the implicit declaration and writing `= default` yourself is the access check. Writing `= default` is how a class with `private` members says that member-wise equivalence is correct for it.

Note that a consequence of being able to `default` this function is that you should also be able to `delete` it, which means that a type could (if so desired) simply opt out of being usable as a constant template parameter (including types that would otherwise be usable by C++20 rules).

Also, the actual implementation details of defaulting `operator template` would differ from those of explicitly providing it. If it is user-provided, the implementation will of course invoke and use that result as the template parameter object. But if it is defaulted, then effectively the implementation just tracks an extra bit on the type that tells it to ignore the usual access rule for C++20 class types as non-type template parameters. It wouldn't have to change the rules for template-argument-equivalence for that case.

## Concrete Details

Currently, the rule is that `f<x>` and `f<y>` are the same specialization when `x` and `y` are template-argument-equivalent. For which the relevant rule from [temp.type]{.sref} for class types is:

::: std
[2]{.pnum} Two values are *template-argument-equivalent* if they are of the same type and

* [2.1]{.pnum} [...]
* [2.10]{.pnum} they are of union type and either they both have no active member or they have the same active member and their active members are template-argument-equivalent, or
* [2.11]{.pnum} [...], or
* [2.12]{.pnum} they are of class type and their corresponding direct subobjects and reference members are template-argument-equivalent.
:::

This proposal does not change those bullets. Instead, for a class type `T` (including union type) that provides a non-default `operator template` customization, `f<x>` and `f<y>` are the same specialization when `x.operator template() == y.operator template()` (i.e. they are reflections representing the same object). The template parameter object _is_ the object returned by `operator template`, so template-argument-equivalence compares those objects, never `x` and `y` themselves. And because `operator template` must be deterministic and idempotent (see below), two objects returned by it have template-argument-equivalent values only if they are the same object, so the existing rules suffice.

In order for that to be viable, we need to impose strict (albeit obvious) restrictions on the return type of `operator template`. For a class type `T`, `T::operator template` must return a reflection representing either an object or a variable. Letting: `O` be either that object or the object defined by that variable:

* `O` has static storage duration,
* `O` is usable in constant expressions,
* `O` has type `T const` (this also guards against inheriting a customization — `Base`'s implementation would have to return a `Base const` object, which would be incompatible with the rules for a `Derived`).

`operator template` must also be deterministic (same value in, same object out) and idempotent (given a value `x`, `x.operator template()` and `[: x.operator template() :].operator template()` must be the same object — although note that this function isn't directly invokable, I'm just presenting this as exposition). Idempotence we can actually check as part of the implementation (and is enforced in the wording), in the same way that template variable construction does an extra copy right now.

We'll have the same rules for the shape of the customization point for defaulting reasons as we have for defaulting the comparison operators. That is, it must be:

* `consteval`
* non-static member function,
* have no non-object parameter, and
* the object parameter must have type either `T` or `T const&`.

Note that a type with `mutable` members is non-structural, regardless of any customization point.

## Standard Library Extensions

There is one standard library type that I'm aware of that would actually provide a bespoke implementation of `operator template` (see below), but there are quite a few for whom `default`ing is the correct behavior. For the class templates below, `default`ing will do the right thing by opting in those types whose subobjects are usable as constant template parameters and not opting in those types whose subobjects are not usable (note that `std::pair` and `std::array` are absent from this list, but that is only because they are already usable as constant template parameters):

* `std::optional<T>`
* `std::expected<T, E>`
* `std::variant<Ts...>`
* `std::tuple<Ts...>`
* `std::basic_string_view<charT, Traits>` (all specializations)
* `std::span<T, Extent>` (all specializations)
* `std::reference_wrapper<T>` (all specializations)
* `std::complex<T>` (all specializations)
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
consteval operator template() const = default;
```
:::

Does the right thing and opts those types (or suitable specializations of those class templates) into being used as constant template parameters, with the correct semantics. Although this isn't how it's worded — the wording specifies the resulting semantics, not the spelling.

`std::inplace_vector<T, N>` is different: only the elements in [`0`, `size()`) should matter, as with `SmallString` above. An implementation could get that behavior with a custom `operator template` that looks the same as `std::vector`'s (defined below). It could also use a defaulted one, if the storage past `size()` consists of objects that are not within their lifetime. Which is better depends on how efficiently those holes can be mangled, which I think is still an open issue in Itanium. But when it comes to specification, we shouldn't be concerned about how to implement it specifically — what we specify is the behavior: only [`0`, `size()`) participates.

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
    consteval operator template() const
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
* then, the `string_view` needs to go through its (defaulted) `operator template` opt-in

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

In [@P4340R0], the name I chose for the customization point was `reflect_constant`, since it was conceptually customizing `std::meta::reflect_constant`. There was precedent for using regular identifiers in language customization points already (`begin`, `end`, `get`, plus all the coroutine stuff), so this seemed alright. Although with `= default`, it's possible that this was a bit too much.

However, there are some real issues with attempting to use the name `reflect_constant` that would be improved by simply... not using a real identifier:

* for custom definitions, the implementation frequently wants to use `std::meta::reflect_constant`. Using that unqualified (whether by relying on ADL, a using directive, or simply being in namespace `std::meta`) would now find the customization point we're implementing, not the real function. I did run into this bug.
* we're not _really_ customizing `std::meta::reflect_constant` per se, so the premise wasn't quite right.
* with reflection queries, you usually don't want to find this customization point, and I hit issues with reflection code accidentally finding it. This first led me to allow the customization point to be declared `private` (an earlier idea required public), but even better would be to call this a _special member function_. Since it really is _quite_ special, and I don't think we want to have a special member function called `reflect_constant`
* there is no reason to allow its use outside of the declaration. With a regular name like `reflect_constant`, you have to deal with using `x.reflect_constant()` (not useful, since `std::meta::reflect_constant(x)` would do the same thing and work in more contexts) and `using Base::reflect_constant;` (which is wrong).

Instead, this revision changes the name of the customization point to `operator template()` — the name proposed in the original paper in the constant template parameter trilogy, [@P2484R0]{.title}. Richard Smith had also suggested `operator<>()`, but that seems more cryptic, and plus `operator< <>` is technically valid, so I don't think we need _more_ punctuation here. `operator template` is a pretty good name, since this is the operator that turns our value into a template arguemnt. It fits. The spelling also lets us avoid a return type, like conversion functions, so it's just less to type too.

Now, in that original discussion, some people didn't like that `operator template()` wasn't directly invokable, but we now have a way of getting that result directly anyway via `std::meta::reflect_constant`. So we're not missing any functionality, and restricting the use of `operator template` just prevents misuses.

## Implementation Experience

This has been implemented in [my fork](https://github.com/brevzin/llvm-project/commit/750c1763c183d9fb1bc0e6bb1c6a4adde11c9094) of Clang (updated for R1 in [this commit](https://github.com/brevzin/llvm-project/commit/0301b9344c195c8e922631a38f50058a426f18dc)), and you can see it on [compiler explorer](ce:impl).

# Wording

The wording introduces a _template-argument function_: a special member function named `operator template`, specified in a new subclause [class.template.arg]. Like a copy constructor, it is declared implicitly if a class doesn't declare one, and its implicit declaration and defaulted definition encode the C++20 requirements on structural class types. A structural class type is then simply a literal class type with no mutable subobjects that has an eligible template-argument function. Normalization, both of string literals and of values whose type has a user-provided  template-argument function, happens when the value of a constant template parameter is determined. Finally, the wording goes through all of the standard library types in [the list](#standard-library-extensions) and makes them suitably structural based on their semantics, not on any particular spelling of `operator template`.

## Language

A template-argument function, like a conversion function, has no type in its `$decl-specifier-seq$`. Change [dcl.type.general]{.sref}/3:

::: std
[3]{.pnum} Except in a declaration of a constructor, destructor, [or]{.rm} conversion function[, or template-argument function ([class.template.arg])]{.addu}, at least one `$defining-type-specifier$` that is not a `$cv-qualifier$` shall appear in a complete `$type-specifier-seq$` or a complete `$decl-specifier-seq$`.
:::

Change [special]{.sref}/1, /3, and /5:

::: std
[1]{.pnum} Default constructors ([class.default.ctor]), copy constructors, move constructors ([class.copy.ctor]), copy assignment operators, move assignment operators ([class.copy.assign]), [and]{.rm} prospective destructors ([class.dtor])[, and template-argument functions ([class.template.arg])]{.addu} are *special member functions*. [...]

[3]{.pnum} [The special member functions affect the way objects of class type are created, copied, moved, and destroyed, [and]{.rm} how values can be converted to values of other types[, and how values are used as constant template arguments]{.addu}. Often such special member functions are called implicitly.]{.note}

[5]{.pnum} Two special member functions are of the same kind if

* [5.1]{.pnum} they are both default constructors,
* [5.2]{.pnum} they are both copy or move constructors with the same first parameter type, [or]{.rm}
* [5.3]{.pnum} they are both copy or move assignment operators with the same first parameter type and the same `$cv-qualifier$`s and `$ref-qualifier$`, if any[.]{.rm}[, or]{.addu}
* [5.4]{.pnum} [they are both template-argument functions.]{.addu}
:::

Change [dcl.fct.def.default]{.sref}/2 and /5:

::: std
[2]{.pnum} The program is ill-formed if an explicitly defaulted special member function `F@~1~@` of class `C` differs from the corresponding special member function `F@~2~@` that would have been implicitly declared other than as follows:

* [2.1]{.pnum} if `F@~1~@` is an assignment operator, it may have the `&` `$ref-qualifier$`;
* [2.2]{.pnum} [...]
* [2.3]{.pnum} `F@~1~@` and `F@~2~@` may have differing exception specifications;
* [2.4]{.pnum} [...]; [and]{.rm}
* [2.5]{.pnum} if `F@~2~@` has a non-object parameter of type "lvalue reference to `C`", the corresponding non-object parameter of `F@~1~@` may be of type "lvalue reference to `const C`"; in this case only, `F@~1~@` is defined as deleted if it is explicitly defaulted on its first declaration and the program is ill-formed otherwise[.]{.rm}[; and]{.addu}
* [2.6]{.pnum} [if `F@~2~@` is a template-argument function, `F@~1~@` may have an explicit object parameter of type `C` or of type "lvalue reference to `const C`", in which case the type of `F@~1~@` would differ from the type of `F@~2~@` in that the type of `F@~1~@` has an additional parameter.]{.addu}

[3]{.pnum} [...]

[4]{.pnum} [...]

[5]{.pnum} Explicitly-defaulted functions and implicitly-declared functions are collectively called *defaulted* functions, and the implementation shall provide implicit definitions for them ([class.ctor], [class.dtor], [class.copy.ctor], [class.copy.assign]<!-- -->[, [class.template.arg]]{.addu}) as described below, including possibly defining them as deleted. [...]
:::

Add `template` to the grammar of `$operator$` in [over.oper.general]{.sref}/1, and add a paragraph after [over.oper.general]{.sref}/6:

::: std
[1]{.pnum} A declaration whose `$declarator-id$` is an `$operator-function-id$` shall declare a function or function template or an explicit instantiation or specialization of a function template. A function so declared is an *operator function*. [...]
```
$operator-function-id$:
    operator $operator$

$operator$: one of
    new      delete   new[]    delete[] co_await ()       []       ->       ->*
    ~        !        +        -        *        /        %        ^        &
    |        =        +=       -=       *=       /=       %=       ^=       &=
    |=       ==       !=       <        >        <=       >=       <=>      &&
    ||       <<       >>       <<=      >>=      ++       --       ,        @[template]{.addu}@
```

[6]{.pnum} The `co_await` operator is described completely in [expr.await]. The attributes and restrictions found in the rest of [over.oper] do not apply to it unless explicitly stated in [expr.await].

::: addu
[6+]{.pnum} The template-argument function, `operator template`, is described completely in [class.template.arg]. The attributes and restrictions found in the rest of [over.oper] do not apply to it unless explicitly stated in [class.template.arg].
:::
:::

Add a new subclause after [class.dtor]{.sref}:

::: {.std .wording}
::: addu
**Template-argument functions   [class.template.arg]**

[1]{.pnum} A *template-argument function* of a class `C` is a non-static member function of `C` whose name is the `$operator-function-id$` `operator template` and whose object parameter has type `const C&` or `C`, other than a function template or a specialization of one. The declarator of a template-argument function shall consist only of its `$declarator-id$` followed by its `$parameters-and-qualifiers$`. The `$operator-function-id$` `operator template` shall be used only in the `$declarator-id$` of a declaration of a template-argument function. [For example, it cannot be used in an expression, as the operand of a `$reflect-expression$`, in a `$using-declarator$`, or in a `$template-id$`.]{.note}

[#]{.pnum} The declared return type of a template-argument function is `std::meta::info`. No `$decl-specifier$` in its `$decl-specifier-seq$` shall be a `$defining-type-specifier$`. A user-declared template-argument function shall be declared `consteval` and shall have no non-object parameters.

[#]{.pnum} A template-argument function shall not be named by an expression ([basic.def.odr]). [A template-argument function is invoked only as specified in this subclause, including by `std::meta::reflect_constant` ([meta.reflection.result]). It cannot be called or have its address taken, even by way of a `$splice-expression$`, but `std::meta::members_of` can represent it ([meta.reflection.member.queries]).]{.note}

[#]{.pnum} If the class is not an anonymous union and does not have a user-declared template-argument function, one is declared implicitly. If the class has a direct base class or a direct non-static data member that is not public, the implicitly-declared template-argument function is defined as deleted; otherwise, it is defaulted ([dcl.fct.def]). The implicitly-declared template-argument function for a class `C` is an inline public member of `C` and has the form
```cpp
consteval C::operator template() const
```

[An explicitly-defaulted template-argument function differs from an implicitly-declared one in that access is not considered: it is not defined as deleted because of a non-public direct base class or non-static data member.]{.note}

[#]{.pnum} A class shall not have more than one eligible ([special]) template-argument function.

[#]{.pnum} A template-argument function that is defined as defaulted shall be defined as defaulted on its first declaration.

[#]{.pnum} Let `C` be a class whose eligible template-argument function `F` is user-provided. When the template parameter object for an object `x` of type `$cv$ C` is determined ([temp.arg.nontype]), `F` is invoked with `x` as its object argument, without regard to access control; let `$R$` be the result of that invocation. If the template parameter object is determined as part of an evaluation `$X$` during the evaluation of an expression as a core constant expression (for example, by a call to `std::meta::reflect_constant`), the invocation is evaluated as part of `$X$`. Otherwise, the invocation is evaluated as a manifestly constant-evaluated expression that appears at the point at which the template argument from which the value of the constant template parameter is being determined ([temp.arg.nontype]) appears. [This determines the evaluation context ([expr.const.reflect]) of the invocation, and so the behavior of functions used for reflection ([meta.reflection]) during it.]{.note} The program is ill-formed if that invocation is not a constant expression ([expr.const]). Otherwise, the program is ill-formed unless `$R$` represents either an object or a variable; let `$O$` be that object or the object declared by that variable. The program is ill-formed unless

* [#.#]{.pnum} `$O$` is the object declared by a variable with static storage duration, or a subobject of such an object,
* [#.#]{.pnum} `$O$` is usable in constant expressions,
* [#.#]{.pnum} `$O$` is a const object ([basic.type.qualifier]) of type `C` or `const C`,
* [#.#]{.pnum} the complete object of `$O$` ([intro.object]) is not TU-local ([basic.link]) unless `x` is TU-local, and
* [#.#]{.pnum} the invocation of `F` with `$O$` as its object argument is a constant expression whose result represents either `$O$` or a variable that declares `$O$`.

`$O$` is the template parameter object ([temp.param]) for `x`.

[#]{.pnum} For any two objects `x` and `y` of a class type `C` whose eligible template-argument function is user-provided, if the corresponding direct subobjects and reference members of `x` and `y` are template-argument-equivalent ([temp.type]), the template parameter object for `x` shall be the same object as the template parameter object for `y`; no diagnostic required. [A user-provided template-argument function need not be injective: objects whose subobjects are not template-argument-equivalent can have the same template parameter object.]{.note}

[#]{.pnum} A defaulted template-argument function of a class `C` is defined as deleted if any direct base class of `C`, any non-static data member of `C` other than an anonymous union member, or any variant member of `C` has a type that is neither a structural type ([temp.param]) nor a (possibly multidimensional) array of structural types.

[#]{.pnum}

::: example
```cpp
struct A {                                     // A::operator template is implicitly declared
  int i;                                       // and defaulted
};

class B {                                      // B::operator template is implicitly declared
  int i;                                       // and defined as deleted: B::i is not public
};

class C {
  int i;
public:
  consteval operator template() const = default;  // OK, access is not considered
};

struct D {
  consteval operator template() const = default;  // defined as deleted: B is not a structural type
private:
  B b;
};

struct E : C { };                              // E::operator template is implicitly declared
                                               // and defaulted; C is a structural type

struct F {                                     // F::operator template is implicitly declared
  union { int i; float f; };                   // and defaulted; the anonymous union's members
};                                             // are variant members of F

struct Mod {                                   // values are equivalent modulo 7
  int v;
  consteval operator template() const;
};
template<int V> constexpr Mod canonical = {V};
consteval Mod::operator template() const {
  return substitute(^^canonical, {std::meta::reflect_constant((v % 7 + 7) % 7)});
}

template<Mod m> struct X { };
static_assert(std::is_same_v<X<Mod{3}>, X<Mod{10}>>);  // OK, the template parameter object
                                                       // for both is canonical<3>

struct Bad {
  int v;
  consteval operator template() const { return ^^canonical<0>; }
};
template<Bad b> struct Y { };
Y<Bad{1}> y;                                   // error: canonical<0> has type const Mod,
                                               // not const Bad

struct Year {                                  // the template parameter object is a subobject
  int y;                                       // of the variable jan1<y>
  consteval operator template() const;
};
struct Date { Year year; int month, day; };
template<int N> constexpr Date jan1 = {{N}, 1, 1};
consteval Year::operator template() const {
  auto r = substitute(^^jan1, {std::meta::reflect_constant(y)});
  return std::meta::reflect_object(std::meta::extract<const Date&>(r).year);
}
template<Year y> constexpr const Year* addr = &y;
static_assert(addr<Year{2024}> == &jan1<2024>.year);

struct G {
  operator template() const;                   // error: not consteval
};

struct H {
  consteval operator template();               // error: object parameter has type H&
};

struct I {
  consteval std::meta::info operator template() const;  // error: return type specified
};

struct J {
  consteval operator template() const;
};
consteval J::operator template() const = default;  // error: not defaulted on first declaration

struct K : A {
  using A::operator template;                  // error: operator template named by a using-declarator
};

consteval std::meta::info f(A a) {
  return a.operator template();                // error: operator template named in an expression
}
constexpr std::meta::info r = ^^A::operator template;  // error: operator template named in an expression
constexpr std::meta::info s = std::meta::reflect_constant(A{1});  // OK
```
:::
:::
:::

Change the definition of structural type in [temp.param]{.sref}/12:

::: std
[12]{.pnum} A *structural type* is one of the following:

* [12.1]{.pnum} a scalar type, or
* [12.2]{.pnum} an lvalue reference type, or
* [12.3]{.pnum} a literal class type [with the following properties:]{.rm} [that has no mutable subobjects and has an eligible template-argument function ([class.template.arg]).]{.addu}
  * [12.3.1]{.pnum} [all base classes and non-static data members are public and non-mutable and]{.rm}
  * [12.3.2]{.pnum} [the types of all base classes and non-static data members are structural types or (possibly multidimensional) arrays thereof.]{.rm}

:::

Change [temp.param]{.sref}/13. A template parameter object of a class type whose eligible template-argument function is user-provided can be the object declared by a variable, or a subobject of such an object, so not all template parameter objects are distinct from all other objects. Also, a template parameter object is template-argument-equivalent to the normalized template argument, not to the template argument as written:

::: std
[13]{.pnum} Certain constructs refer to template parameter objects, which are [distinct]{.rm} objects with static storage duration and non-volatile const type. [A template parameter object of a class type whose eligible template-argument function is user-provided is determined by that function ([class.template.arg]) and can be the object declared by a variable or a subobject of such an object; other template parameter objects are distinct from all other objects.]{.addu} No two [such]{.rm} [template parameter]{.addu} objects have template-argument-equivalent values ([temp.type]). An `$id-expression$` naming a constant template parameter of class type `T` denotes the template parameter object of type `const T`, which is template-argument-equivalent to the corresponding template argument after it has been converted to the type of the template parameter [and normalized]{.addu} ([temp.arg.nontype]).
:::

Change [temp.arg.nontype]{.sref}/2-5. Paragraphs 4 and 5 become bullets of the new paragraph that introduces `v`; their text is otherwise unchanged:

::: std
[2]{.pnum} The value of a constant template parameter `$P$` of (possibly deduced) type `T` is determined from its template argument `$A$` as follows. If `T` is not a class type and `$A$` is not a `$braced-init-list$`, `$A$` shall be a converted constant expression ([expr.const.const]) of type `T`; the value of `$P$` is [`$A$` (as converted)]{.rm} [the normalized value (see below) of `$A$` (as converted)]{.addu}.

[3]{.pnum} Otherwise, a temporary variable
```diff
- constexpr T v = $A$;
+ constexpr T u = $A$;
```
is introduced. [The lifetime of `v` ends immediately after initializing it and any template parameter object (see below). For each such variable, the `$id-expression$` `v` is termed a *candidate initializer*.]{.rm}

::: addu
[a]{.pnum} If `T` is a class type whose eligible template-argument function is user-provided, `$P$` denotes the template parameter object ([class.template.arg]) for `u`. The lifetime of `u` ends immediately after that template parameter object is determined. [The template parameter object is not initialized from `u`.]{.note}

[b]{.pnum} Otherwise, a temporary variable `v` of type `const T` is introduced whose value is the normalized value (see below) of `u`. The lifetimes of `u` and `v` end immediately after initializing `v` and any template parameter object (see below). For each such variable `v`, the `$id-expression$` `v` is termed a *candidate initializer*.
:::

* [b.1]{.pnum} If `T` is a class type, a template parameter object ([temp.param]) exists that is constructed so as to be template-argument-equivalent to `v`; `$P$` denotes that template parameter object. `$P$` is copy-initialized from an unspecified candidate initializer that is template-argument-equivalent to `v`. If, for the initialization from any candidate initializer,
  * [b.1.1]{.pnum} the initialization would be ill-formed, or
  * [b.1.2]{.pnum} the full-expression of an invented `$init-declarator$` for the initialization would not be a constant expression when interpreted as a `$constant-expression$` ([expr.const.const]), or
  * [b.1.3]{.pnum} the initialization would cause `$P$` to not be template-argument-equivalent ([temp.type]) to `v`,

  the program is ill-formed.
* [b.2]{.pnum} Otherwise, the value of `$P$` is that of `v`.

::: addu
[c]{.pnum} For a string literal object `$S$` ([lex.string]), let `$P$@~_S_~@` be the template parameter object of the same type as `$S$` whose elements are template-argument-equivalent to the corresponding elements of `$S$`. [`$P$@~_S_~@` is the object represented by `std::meta::reflect_constant_string($S$)` ([meta.define.static]). It is a potentially non-unique object ([intro.object]).]{.note}

[d]{.pnum} The *normalized value* of an object, value, or reference `x` of type `T` is:

* [d.1]{.pnum} If `T` is a pointer type and `x` points to or past the end of an object `$o$` that is a string literal object `$S$` or a subobject of `$S$`, the pointer value that points to or past the end of, respectively, the object that corresponds to `$o$` within `$P$@~_S_~@`.
* [d.2]{.pnum} Otherwise, if `T` is a reference type and `x` refers to an object `$o$` that is a string literal object `$S$` or a subobject of `$S$`, a reference to the object that corresponds to `$o$` within `$P$@~_S_~@`.
* [d.3]{.pnum} Otherwise, if `T` is an array type, the array value whose elements are the normalized values of the corresponding elements of `x`.
* [d.4]{.pnum} Otherwise, if `T` is a class type whose eligible template-argument function ([class.template.arg]) is user-provided, the value of an object of type `T` copy-initialized from an lvalue designating the template parameter object for `x` ([class.template.arg]). The program is ill-formed if that copy-initialization would be ill-formed or if the full-expression of an invented `$init-declarator$` for it would not be a constant expression.
* [d.5]{.pnum} Otherwise, if `T` is a union type, `x` if `x` has no active member; otherwise, the value that has the same active member as `x`, whose value is the normalized value of the active member of `x`.
* [d.6]{.pnum} Otherwise, if `T` is a class type, the value whose direct subobjects and reference members have the normalized values of the corresponding direct subobjects and reference members of `x`.
* [d.7]{.pnum} Otherwise, `x`.

[Normalization replaces each pointer into, or reference to, a string literal object or subobject thereof with a pointer into, or reference to, an object whose identity is the same in every translation unit, and replaces each subobject of a class type whose eligible template-argument function is user-provided with a copy of the template parameter object for that subobject.]{.note}

::: example
```cpp
template<const char* p> struct X { };
X<"hello"> x1;
X<std::define_static_string("hello")> x2;    // same type as x1

constexpr const char* a = "holdup";
X<a + 3> x3;
X<"dup"> x4;                                // not the same type as x3

template<const char& r> struct W { };
W<"hello"[1]> w1;
W<std::define_static_string("hello")[1]> w2;  // same type as w1

struct Frac {
  int num, den;
  consteval operator template() const;
};
template<int N, int D> constexpr Frac interned = {N, D};
consteval Frac::operator template() const {
  int g = std::gcd(num, den);
  return substitute(^^interned, {std::meta::reflect_constant(num / g),
                                 std::meta::reflect_constant(den / g)});
}

template<Frac F> struct Y { };
Y<Frac{2, 4}> y1;                           // the template parameter object is interned<1, 2>
Y<interned<3, 6>> y2;                       // same type as y1

struct Stuff {
  Frac f;
  const char* s;
};
template<Stuff S> struct Z { };
Z<Stuff{{2, 4}, "hi"}> z1;
Z<Stuff{{1, 2}, std::define_static_string("hi")}> z2;   // same type as z1
```
:::
:::
:::

Remove [temp.arg.nontype]{.sref}/6.2. By the time this check applies, pointers into and references to string literal objects have been normalized. A template parameter object determined by a user-provided template-argument function is not normalized, but it is an ordinary variable, no different from any other object a reference template parameter can refer to:

::: std
[6]{.pnum} For a constant template parameter of reference or pointer type, or for each non-static data member of reference or pointer type in a constant template parameter of class type or subobject thereof, the reference or pointer value shall not refer or point to (respectively):

* [6.1]{.pnum} a temporary object ([class.temporary]),
* [6.2]{.pnum} [a string literal object ([lex.string]),]{.rm}
* [6.3]{.pnum} the result of a `typeid` expression ([expr.typeid]),
* [6.4]{.pnum} a predefined `__func__` variable ([dcl.fct.def.general]), or
* [6.5]{.pnum} a subobject ([intro.object]) of one of the above.

:::

Change the note in [temp.arg.nontype]{.sref}/9:

::: std
[9]{.pnum}

::: note
A `$string-literal$` ([lex.string]) is [not]{.rm} an acceptable `$template-argument$` for a constant template parameter of [non-class]{.rm} [pointer or reference]{.addu} type[; the resulting pointer value or reference is normalized to point into or refer to a template parameter object]{.addu}.

::: example
```diff
  template<class T, T p> class X {
    ...
  };

- X<const char*, "Studebaker"> x; // error: string literal object as template-argument
- X<const char*, "Knope" + 1> x2; // error: subobject of string literal object as template-argument
+ X<const char*, "Studebaker"> x; // OK
+ X<const char*, "Knope" + 1> x2; // OK
+ X<const char*, "Knope" + 1> x3; // OK, same type as x2

  const char p[] = "Vivisectionist";
  X<const char*, p> y;            // OK

  struct A {
    constexpr A(const char*) {}
  };

  X<A, "Pyrophoricity"> z;        // OK, string-literal is a constructor argument to A
```
:::
:::
:::

[[temp.type]{.sref} needs no change. Every value compared there has already been normalized. For a class type whose eligible template-argument function is user-provided, determinism ([class.template.arg]/8) and idempotence ([class.template.arg]/7.5) mean that two template parameter objects have template-argument-equivalent values exactly when they are the same object.]{.draftnote}

[[basic.link]{.sref} needs no change. The template argument for a constant template parameter of class type whose eligible template-argument function is user-provided designates its template parameter object, which can be the object declared by a variable or a subobject of such an object. [class.template.arg]/7.4 ensures that the complete object is TU-local only when the template argument already was.]{.draftnote}

## Reflection library

`is_special_member_function` ([meta.reflection.queries]{.sref}) is specified in terms of [special]{.sref}, so it returns `true` for a template-argument function without any change. The implicitly-declared template-argument function is returned by `members_of`, so update the counts in the example in [meta.reflection.member.queries]{.sref}:

::: std
```diff
  class B {};

  struct S : B {
  private:
    class I;
  public:
    int m;
  };

- static_assert(                                                  // 6 special members,
-   members_of(^^S, access_context::current()).size() == 7);      // 1 public member,
+ static_assert(                                                  // 7 special members,
+   members_of(^^S, access_context::current()).size() == 8);      // 1 public member,
                                                                  // does not include base

  static_assert(                                                  // all of the above,
-   members_of(^^S, access_context::unchecked()).size() == 8);    // as well as a reflection
+   members_of(^^S, access_context::unchecked()).size() == 9);    // as well as a reflection
                                                                  // representing S::I
```
:::

Add one row, for `op_template`, to the end of the table of the enumerators of `operators` (Table [tab:meta.reflection.operators]) in [meta.reflection.operators]{.sref}. The existing rows are unchanged:

::: std
|Constant|Corresponding `$operator-function-id$`|Operator symbol name|
|-|-|-|
|[...]|[...]|[...]|
|`op_plus_plus`|`operator++`|`++`|
|`op_minus_minus`|`operator--`|`--`|
|`op_comma`|`operator,`|`,`|
|[`op_template`]{.addu}|[`operator template`]{.addu}|[`template`]{.addu}|
:::

A template-argument function cannot be named by an expression ([class.template.arg]/3), so `extract` must not produce a pointer to one. Change [meta.reflection.extract]{.sref}/7:

::: std
```cpp
template<class T>
  consteval T $extract-member-or-function$(info r);       // exposition only
```

[7]{.pnum} *Throws*: `meta::exception` unless

* [7.1]{.pnum} `r` represents a non-static data member with type `X`, that is not a bit-field, that is a direct member of class `C`, `T` and `X C::*` are similar types ([conv.qual]), and `is_convertible_v<X C::*, T>` is `true`;
* [7.2]{.pnum} `r` represents an implicit object member function [that is not a template-argument function ([class.template.arg])]{.addu} with type `F` or `F noexcept` that is a direct member of a class `C`, and `T` is `F C::*`; or
* [7.3]{.pnum} `r` represents a non-member function, static member function, or explicit object member function [that is not a template-argument function]{.addu} of function type `F` or `F noexcept`, and `T` is `F*`.
:::

Change the specification of `template_arguments_of` in [meta.reflection.queries]{.sref}:

::: std
```cpp
consteval vector<info> template_arguments_of(info r);
```

[59]{.pnum} *Returns*: A `vector` containing reflections of the template arguments of the template specialization represented by `r`, in the order in which they appear in the corresponding template argument list. For a given template argument `$A$`, its corresponding reflection `$R$` is determined as follows:

* [59.1]{.pnum} If `$A$` denotes a type or type alias, [...]
* [59.2]{.pnum} Otherwise, if `$A$` denotes a class template, variable template, concept, or alias template, [...]
* [59.3]{.pnum} Otherwise, `$A$` is a constant template argument ([temp.arg.nontype]). Let `$P$` be the corresponding template parameter.
  * [59.3.1]{.pnum} If `$P$` has reference type, then `$R$` is a reflection representing the object or function referred to by [`$A$`]{.rm} [`$P$` as determined from `$A$` ([temp.arg.nontype])]{.addu}.
  * [59.3.2]{.pnum} Otherwise, if `$P$` has class type, then `$R$` represents the corresponding template parameter object.
  * [59.3.3]{.pnum} Otherwise, `$R$` is a reflection representing the value of [`$A$`]{.rm} [`$P$` as determined from `$A$` ([temp.arg.nontype])]{.addu}.
:::

`std::meta::reflect_constant` is specified in terms of `template_arguments_of(^^TCls<$V$>)[0]`, so it picks up both the user-provided template-argument function and normalization from the core wording. What remains is to update the example in [meta.reflection.result]{.sref}:

::: std
```cpp
template<class T>
  consteval info reflect_constant(T expr);
```

[1]{.pnum} *Mandates*: `is_copy_constructible_v<T>` is `true` and `T` is a cv-unqualified structural type ([temp.param]) that is not a reference type.

[2]{.pnum} Let `$V$` be:

* [2.1]{.pnum} if `T` is a class type, then an object that is template-argument-equivalent to the value of `expr`;
* [2.2]{.pnum} otherwise, the value of `expr`.

[3]{.pnum} Let `TCls` be the invented template:
```cpp
template<T P> struct TCls;
```

[4]{.pnum} *Returns*: `template_arguments_of(^^TCls<$V$>)[0]`. [This is a reflection of an object for class types, and a reflection of a value otherwise.]{.note}

[5]{.pnum} *Throws*: `meta::exception` unless the `$template-id$` `TCls<$V$>` would be valid.

[6]{.pnum}

::: example
```diff
  template<auto D>
    struct A { };

  struct N { int x; };
  struct K { char const* p; };

  constexpr info r1 = reflect_constant(42);
  static_assert(is_value(r1));
  static_assert(r1 == template_arguments_of(^^A<42>)[0]);

  constexpr info r2 = reflect_constant(N{42});
  static_assert(is_object(r2));
  static_assert(r2 == template_arguments_of(^^A<N{42}>)[0]);

  constexpr info r3 = reflect_constant(K{nullptr});
- constexpr info r4 = reflect_constant(K{"ebab"});        // error: constituent pointer
-                                                         // points to string literal
+ constexpr info r4 = reflect_constant(K{"ebab"});        // OK
+ static_assert(extract<K>(r4).p == define_static_string("ebab"));
+
+ struct Frac {
+   int num, den;
+   consteval operator template() const;
+ };
+ template<int N, int D> constexpr Frac interned = {N, D};
+ consteval Frac::operator template() const {
+   int g = std::gcd(num, den);
+   return substitute(^^interned, {std::meta::reflect_constant(num / g),
+                                  std::meta::reflect_constant(den / g)});
+ }
+
+ constexpr info r5 = reflect_constant(Frac{2, 4});
+ static_assert(is_object(r5));
+ static_assert(r5 == reflect_object(interned<1, 2>));
+ static_assert(r5 == template_arguments_of(^^A<Frac{3, 6}>)[0]);
```
:::
:::

## Standard library types

[Every class now has a template-argument function. For a standard library class, whether that function is implicitly declared, explicitly defaulted, or user-provided should probably be unspecified, as should its exact signature. Users get at this functionality through `std::meta::reflect_constant`, and implementations may need to explicitly default the function on internal helper types as well. This probably needs a sentence in [member.functions]{.sref}.]{.draftnote}

Following the existing wording for `pair` ([pairs.pair]{.sref}) and `array` ([array.overview]{.sref}), the library types are specified by stating when they are structural types and what template-argument-equivalence means for them, rather than by adding a defaulted `operator template` to every synopsis. Explicitly defaulting only exempts the direct subobjects from the access requirement ([class.template.arg]/4, /9), so implementations will also need to opt in their internal storage types, and this approach leaves them free to do so. `inplace_vector` is specified slightly differently (see below).

### `optional`

Add a paragraph to the end of [optional.optional.general]{.sref}:

::: std
::: addu
[3]{.pnum} `optional<T>` is a structural type ([term.structural.type]) if `T` is a structural type. Two values `x` and `y` of type `optional<T>` are template-argument-equivalent ([temp.type]) if and only if either `x.has_value()` and `y.has_value()` are both `false`, or they are both `true` and `x.$val$` and `y.$val$` are template-argument-equivalent.
:::
:::

Add a paragraph to the end of [optional.optional.ref.general]{.sref}:

::: std
::: addu
[3]{.pnum} `optional<T&>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `optional<T&>` are template-argument-equivalent ([temp.type]) if and only if either `x.has_value()` and `y.has_value()` are both `false`, or they are both `true` and `*x` and `*y` refer to the same object.
:::
:::

### `variant`

Add a paragraph to the end of [variant.variant.general]{.sref}:

::: std
::: addu
[5]{.pnum} `variant<Types...>` is a structural type ([term.structural.type]) if each type in `Types` is a structural type. Two values `x` and `y` of type `variant<Types...>` are template-argument-equivalent ([temp.type]) if and only if `x.index() == y.index()` is `true` and either `x.valueless_by_exception()` is `true` or the objects referred to by `get<$I$>(x)` and `get<$I$>(y)` are template-argument-equivalent, where `$I$` is `x.index()`.
:::
:::

### `tuple`

Add a paragraph to the end of [tuple.tuple.general]{.sref}:

::: std
::: addu
[2]{.pnum} `tuple<Types...>` is a structural type ([term.structural.type]) if each type in `Types` is a structural type. Two values `x` and `y` of type `tuple<Types...>` are template-argument-equivalent ([temp.type]) if and only if, for every `$I$` in the range [`0`, `sizeof...(Types)`):

* [2.1]{.pnum} if `Types...[$I$]` is a reference type, then `get<$I$>(x)` and `get<$I$>(y)` refer to the same object;
* [2.2]{.pnum} otherwise, the objects referenced by `get<$I$>(x)` and `get<$I$>(y)` are template-argument-equivalent.
:::
:::

### `expected`

Add a paragraph to the end of [expected.object.general]{.sref}:

::: std
::: addu
[4]{.pnum} `expected<T, E>` is a structural type ([term.structural.type]) if `T` and `E` are both structural types. Two values `x` and `y` of type `expected<T, E>` are template-argument-equivalent ([temp.type]) if and only if either `x.has_value()` and `y.has_value()` are both `true` and `x.$val$` and `y.$val$` are template-argument-equivalent, or they are both `false` and `x.$unex$` and `y.$unex$` are template-argument-equivalent.
:::
:::

Add a paragraph to the end of [expected.void.general]{.sref}:

::: std
::: addu
[4]{.pnum} `expected<T, E>` is a structural type ([term.structural.type]) if `E` is a structural type. Two values `x` and `y` of type `expected<T, E>` are template-argument-equivalent ([temp.type]) if and only if either `x.has_value()` and `y.has_value()` are both `true`, or they are both `false` and `x.$unex$` and `y.$unex$` are template-argument-equivalent.
:::
:::

### `reference_wrapper`

Add a paragraph to the end of [refwrap.general]{.sref}:

::: std
::: addu
[4]{.pnum} `reference_wrapper<T>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `reference_wrapper<T>` are template-argument-equivalent ([temp.type]) if and only if `x.get()` and `y.get()` refer to the same object or function.
:::
:::

### Comparison categories

Add a paragraph to the end of [cmp.categories.pre]{.sref}:

::: std
::: addu
[5]{.pnum} The types `partial_ordering`, `weak_ordering`, and `strong_ordering` are structural types ([term.structural.type]). Two values `x` and `y` of the same such type are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

### `bitset`

Add a paragraph to the end of [template.bitset.general]{.sref}:

::: std
::: addu
[14]{.pnum} `bitset<N>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `bitset<N>` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

### `basic_string_view`

Add a paragraph to the end of [string.view.template.general]{.sref}:

::: std
::: addu
[5]{.pnum} `basic_string_view<charT, traits>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `basic_string_view<charT, traits>` are template-argument-equivalent ([temp.type]) if and only if `x.data()` and `y.data()` are template-argument-equivalent and `x.size() == y.size()` is `true`.
:::
:::

### `span`

Add a paragraph to the end of [span.overview]{.sref}:

::: std
::: addu
[6]{.pnum} `span<ElementType, Extent>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `span<ElementType, Extent>` are template-argument-equivalent ([temp.type]) if and only if `x.data()` and `y.data()` are template-argument-equivalent and `x.size() == y.size()` is `true`.
:::
:::

### `inplace_vector`

For `inplace_vector`, only the elements in the range [`0`, `size()`) are part of its value. The elements past `size()` must not affect which template parameter object a template argument names. There are two ways an implementation can get that behavior:

* a defaulted template-argument function, if the storage past `size()` consists of objects that are not within their lifetime, so that template-argument-equivalence ignores them, or
* a user-provided template-argument function that only serializes [`0`, `size()`), like `SmallString` and `vector` in the design discussion.

The choice depends on mangling. With a defaulted function, the mangled name of a specialization encodes the template parameter object's entire storage, including the holes past `size()`. Whether that's acceptable depends on how compactly those holes can be mangled. With a user-provided function, the mangled name refers to the returned object instead, and that object's own name encodes only the serialized elements. That is an implementation decision, so the wording specifies only the behavior. It doesn't state when two `inplace_vector` values are template-argument-equivalent, because with a user-provided function that relation compares the returned objects, so it isn't meaningful for arbitrary values. Instead, it says when two template arguments determine the same template parameter object, which holds for either implementation.

Add a paragraph to the end of [inplace.vector.overview]{.sref}:

::: std
::: addu
[6]{.pnum} `inplace_vector<T, N>` is a structural type ([term.structural.type]) if `T` is a structural type. Two constant template arguments of type `inplace_vector<T, N>` whose values are `x` and `y` determine the same template parameter object ([temp.arg.nontype]) if and only if `x.size() == y.size()` is `true` and the objects referred to by `x[i]` and `y[i]` are template-argument-equivalent for each `i` in the range [`0`, `x.size()`).
:::
:::

### `complex`

Add a paragraph to the end of [complex]{.sref}:

::: std
::: addu
[2]{.pnum} `complex<T>` is a structural type ([term.structural.type]). Two values `x` and `y` of type `complex<T>` are template-argument-equivalent ([temp.type]) if and only if `x.real()` and `y.real()` are template-argument-equivalent and `x.imag()` and `y.imag()` are template-argument-equivalent.
:::
:::

### `<chrono>`

Add a paragraph to the end of [time.duration.general]{.sref}:

::: std
::: addu
[7]{.pnum} `duration<Rep, Period>` is a structural type ([term.structural.type]) if `Rep` is a structural type. Two values `x` and `y` of type `duration<Rep, Period>` are template-argument-equivalent ([temp.type]) if and only if `x.count()` and `y.count()` are template-argument-equivalent.
:::
:::

Add a paragraph to the end of [time.point.general]{.sref}:

::: std
::: addu
[2]{.pnum} `time_point<Clock, Duration>` is a structural type ([term.structural.type]) if `Duration` is a structural type. Two values `x` and `y` of type `time_point<Clock, Duration>` are template-argument-equivalent ([temp.type]) if and only if `x.time_since_epoch()` and `y.time_since_epoch()` are template-argument-equivalent.
:::
:::

Add a paragraph to the end of [time.cal.day.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `day` is a structural type ([term.structural.type]). Two values `x` and `y` of type `day` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.month.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `month` is a structural type ([term.structural.type]). Two values `x` and `y` of type `month` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.year.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.wd.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `weekday` is a structural type ([term.structural.type]). Two values `x` and `y` of type `weekday` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.wdidx.overview]{.sref}:

::: std
::: addu
[4]{.pnum} `weekday_indexed` is a structural type ([term.structural.type]). Two values `x` and `y` of type `weekday_indexed` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.wdlast.overview]{.sref}:

::: std
::: addu
[4]{.pnum} `weekday_last` is a structural type ([term.structural.type]). Two values `x` and `y` of type `weekday_last` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.md.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `month_day` is a structural type ([term.structural.type]). Two values `x` and `y` of type `month_day` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph after paragraph 3 of [time.cal.mdlast]{.sref}:

::: std
::: addu
[a]{.pnum} `month_day_last` is a structural type ([term.structural.type]). Two values `x` and `y` of type `month_day_last` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.mwd.overview]{.sref}:

::: std
::: addu
[4]{.pnum} `month_weekday` is a structural type ([term.structural.type]). Two values `x` and `y` of type `month_weekday` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.mwdlast.overview]{.sref}:

::: std
::: addu
[4]{.pnum} `month_weekday_last` is a structural type ([term.structural.type]). Two values `x` and `y` of type `month_weekday_last` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.ym.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year_month` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year_month` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.ymd.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year_month_day` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year_month_day` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.ymdlast.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year_month_day_last` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year_month_day_last` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.ymwd.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year_month_weekday` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year_month_weekday` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

Add a paragraph to the end of [time.cal.ymwdlast.overview]{.sref}:

::: std
::: addu
[3]{.pnum} `year_month_weekday_last` is a structural type ([term.structural.type]). Two values `x` and `y` of type `year_month_weekday_last` are template-argument-equivalent ([temp.type]) if and only if `x == y` is `true`.
:::
:::

## Feature-test macros

Bump `__cpp_nontype_template_args` in [cpp.predefined]{.sref}:

::: std
```diff
- __cpp_nontype_template_args 201911L
+ __cpp_nontype_template_args 2026XXL
```
:::

Add a new library feature-test macro to [version.syn]{.sref} for the library types above. The name is a placeholder:

::: std
```diff
+ #define __cpp_lib_constant_template_args 2026XXL // also in <optional>, <expected>, <variant>, <tuple>, <string_view>, <span>, <functional>, <complex>, <compare>, <bitset>, <inplace_vector>, <chrono>
```
:::

# Acknowledgements

Thanks to Tim Song as usual for discussing all the design issues and for helping to make the paper better. Thanks to Dan Katz for an example that was deliberately left out of this paper, for everyone's safety. Thanks to Tomasz Kamiński for help with the wording and discussions of non-uniquness and allowing a template parameter object to be a subobject.
