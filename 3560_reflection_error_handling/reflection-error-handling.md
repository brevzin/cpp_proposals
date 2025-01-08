---
title: "Error Handling in Reflection"
document: P3560R0
date: today
audience: EWG, LEWG
author:
    - name: Peter Dimov
      email: <pdimov@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
tag: reflection
---

# Introduction

In [@P2996R8], we had to answer the question of what the error handling mechanism should be. We considered four options:

1. Returning an invalid reflection (similar to `NaN` for floating point)
2. Returning a `std::expected<T, E>` for some reflection-specific error type `E`
3. Failing to be a constant expression
4. Throwing an exception of type `E`, for some type `E`.

Option (1) doesn't work well, because not all reflection functions return `std::meta::info`. Some (such as `members_of`) return `vector<info>`, some (such as `identifier_of`) return `string_view`, and `extract<T>` even returns `T`. A `NaN` reflection doesn't solve the problem.

Option (2) places a heavy syntactic burden on user code, because `std::expected` needs to be unwrapped manually, without help from the language.

Option (3) doesn't provide any means for user code to recover from an error.

At the time we had to make the decision, option (4) was essentially equivalent to (3), because throwing an exception wasn't a constant expression, so we settled on option (3). However, since the adoption of [@P3068R6], that has changed, and option (4) has become viable.

Using exceptions to signal errors doesn't suffer from the problem with option (1), because it's a strategy that can be used regardless of the return type. It also doesn't require syntactic changes to the user code.

Ordinarily, for runtime functions, exception handling might be avoided for reasons of binary size and runtime overhead; it also imposes the requirement that the API can't be used with exceptions disabled (which is nonstandard, but nevertheless highly popular.)

However, none of these objections apply to exceptions used at compile time. They have no binary footprint, don't affect the run time, and there is no reason for a compiler to not allow them even in "no exceptions" mode (because they are entirely contained to program compilation.)

Therefore, we believe that we need to adopt option (4) as the error handling strategy for reflection functions.

# Exception Type

To signal errors via throwing an exception, we need to settle on an exception type (or types) which to throw.

Since these exceptions will never escape to runtime, we don't need to be concerned with deriving their type(s) from `std::exception`. However, it would be desirable for the exceptions to carry enough information for error recovery (when caught), enough information for high quality error messages (when uncaught), and for them to be suitable for error handling in user `constexpr` and `consteval` functions as well, in addition to standard ones.

To that end, we propose the following exception type:

::: std
```cpp
namespace std::meta {

class exception
{
public:
    consteval exception(u8string_view what,
                        info from,
                        source_location where = source_location::current());

    consteval u8string_view what() const;
    consteval info from() const;
    consteval source_location where() const;
};

}
```
:::

`exception::what()` is a string describing the error; `exception::from()` is a reflection of the function (or function template) from a call to which the error originated; and `exception::where()` is the source location of the call to that function.

For example, the following function

::: std
```cpp
consteval auto f()
{
    return members_of(^^int);
}
```
:::

will throw an exception of type `std::meta::exception` for which `what()` will return (for example) `u8"invalid reflection operand"`, `from()` will return `^^std::meta::members_of`, and `where()` will return a `std::source_location` object pointing at the call to `members_of` inside `f`.

Suppose a user wishes to write a `consteval` function that only accepts class type reflections. It would be possible to use `std::meta::exception` to signal errors as follows:

::: std
```cpp
consteval auto user_fn(info type, source_location where = source_location::current())
{
    if( !is_class_type(type) )
    {
        throw std::meta::exception(u8"not a class type", ^^user_fn, where);
    }

    // carry on
}
```
:::

# Recoverable or Unrecoverable

We went through the proposed API in [@P2996R8] and we think that all of the library functions should be recoverable — that is failing to meet the requirements of the function should be an exception rather than constant evaluation failure. All of them except for two:

* `std::meta::data_member_spec`
* `std::meta::define_aggregate`

These are exceedingly unlikely to be used in a context in which recovering is meaningful, and we think it makes the most sense for those to remain hard errors. But many of the rest are more generic or straightforwardly fallible, so the ability to recover from them is desirable.

# Proposed Wording

The wording here introduces a new type `std::meta::exception` and defines it.

Otherwise it's pretty rote changing all the error handling from something of the form "*Constant When*: `$C$`" to "*Throws*: `meta::exception` unless `$C$`".

## [meta.reflection.synop]

Add to the synopsis in [meta.reflection.synop:]

::: std
```diff
namespace std::meta {
  using info = decltype(^^::);

+ // [meta.reflection.exception], class exception
+ class exception;

  // ...
}
```
:::


## [meta.reflection.exception]

Add a new subclause as follows:

**Class exception, [meta.reflection.exception]**

::: std
::: addu
```cpp
class exception
{
private:
  u8string $what_$;         // exposition only
  info $from_$;             // exposition only
  source_location $where_$; // exposition only

public:
  consteval exception(u8string_view what, info from,
    source_location where = source_location::current()) noexcept;

  exception(exception const&) = default;
  exception(exception&&) = default;

  exception& operator=(exception const&) = default;
  exception& operator=(exception&&) = default;

  consteval u8string_view what() const noexcept;
  consteval info from() const noexcept;
  consteval source_location where() const noexcept;
};
```

[1]{.pnum} Reflection functions throw exceptions of type `std::meta::exception` to signal an error. `std::meta::exception` is a consteval-only type.

```cpp
consteval exception(u8string_view what, info from,
    source_location where = source_location::current()) noexcept;
```

[#]{.pnum} *Effects*: Initializes `$what_$` with `what`, `$from_$` with `from` and `$where_$` with `where`.

```cpp
consteval u8string_view what() const noexcept;
```

[#]{.pnum} *Returns*: `$what_$`.

```cpp
consteval info from() const noexcept;
```

[#]{.pnum} *Returns*: `$from_$`.

```cpp
consteval source_location where() const noexcept;
```

[#]{.pnum} *Returns*: `$where_$`.

:::
:::


## [meta.reflection.operators]

Replace the error handling in this subclause:

::: std
```cpp
consteval operators operator_of(info r);
```

[2]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents an operator function or operator function template.

[#]{.pnum} *Returns*: The value of the enumerator from `operators` whose corresponding `$operator-function-id$` is the unqualified name of the entity represented by `r`.

```cpp
consteval string_view symbol_of(operators op);
consteval u8string_view u8symbol_of(operators op);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless the]{.addu} [The]{.rm} value of `op` corresponds to one of the enumerators in `operators`.

[#]{.pnum} *Returns*: `string_view` or `u8string_view` containing the characters of the operator symbol name corresponding to `op`, respectively encoded with the ordinary literal encoding or with UTF-8.
:::


## [meta.reflection.names]

...

## [meta.reflection.queries]

...

## [meta.reflection.member.queries]

...

## [meta.reflection.layout]

...

## [meta.reflection.extract]

...

## [meta.reflection.substitute]

...

## [meta.reflection.result]

...