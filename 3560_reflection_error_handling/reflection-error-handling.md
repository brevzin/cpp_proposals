---
title: "Error Handling in Reflection"
document: P3560R1
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

# Revision History

Since [@P3560R0], after discussion in an SG16 telecon, we changed the proposed API to be able to have a `string_view` constructor and `string` accessor. See [here](#post-sg16-telecon)

# Introduction

In [@P2996R9]{.title}, we had to answer the question of what the error handling mechanism should be. We considered four options:

1. Returning an invalid reflection (similar to `NaN` for floating point)
2. Returning a `std::expected<T, E>` for some reflection-specific error type `E`
3. Failing to be a constant expression
4. Throwing an exception of type `E`, for some type `E`.

Option (1) doesn't work well, because not all reflection functions return `std::meta::info`. Some (such as `members_of`) return `vector<info>`, some (such as `identifier_of`) return `string_view`, and `extract<T>` even returns `T`. A `NaN` reflection doesn't solve the problem.

Option (2) places a heavy syntactic burden on user code, because `std::expected` needs to be unwrapped manually, without help from the language.

Option (3) doesn't provide any means for user code to recover from an error.

At the time we had to make the decision, option (4) was essentially equivalent to (3), because throwing an exception wasn't a constant expression, so we settled on option (3). However, since the adoption of [@P3068R6]{.title}, that has changed, and option (4) has become viable.

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

## Encoding

What encoding should we use for the string describing the error, and what character type?

The encoding is left unspecified in the runtime case (`std::exception::what()`), which is generally regarded as a defect ([@LWG4087]). Since we are designing a new component, we should not repeat that mistake, and specify the encoding of `meta::exception::what()`.

Since the string describing the error can be constructed from components coming from multiple sources, it should use an encoding that can represent any of these substrings. That is, it should use UTF-8.

The principled way to reflect this fact in the type system is to use `u8string_view`. However, there are strong, purely pragmatic, arguments in favor of using `string_view` instead.

`char8_t` has nearly zero support in the standard library, which makes it _very_ inconvenient to use. Suppose, for example, that we are writing a function `member_of(info x, size_t i)` that returns `members_of(x)[i]`:

::: std
```cpp
consteval info member_of(info x, size_t i, source_location where = source_location::current())
{
    auto v = members_of(x);

    if( i >= v.size() )
    {
        throw meta::exception( u8"invalid member index", ^^member_of, where );
    }

    return v[i];
}
```
:::

Further suppose that we want to provide a more descriptive error string, e.g. `"152 is not a valid member index"`, where 152 is the value of `i`.

There's basically no way to easily do that today. We can't use `std::format` to create a `u8string`, there is no `std::to_u8string`, there is even no equivalent of `to_chars` that would produce a `char8_t` sequence.

In contrast, if the constructor took `std::string_view`, we could have used any of these.

So maybe we should just use `string_view`? But that's not consistent with the current state of [@P2996R8]. It did start out using `string_view` everywhere, and had to be rewritten to supply additional `u8string_view` interfaces, for good reasons.

Consider, for instance, `std::meta::identifier_of(x)`. It can fail for two reasons: if the entity to which `x` refers has no associated identifier, or if it does, but that identifier is not representable in the literal encoding.

We are changing these failures from hard errors (not a constant expression) to throwing `meta::exception`. A sketch implementation of `identifier_of`, then, would look like this:

::: std
```cpp
consteval string_view identifier_of(info x)
{
    if( !has_identifier(x) )
    {
        throw meta::exception(u8"entity has no identifier", ^^identifier_of, ...);
    }

    auto id = u8identifier_of(x);

    if( !is_representable(id) )
    {
        throw meta::exception(u8"identifier '"s + id + u8"'is not representable", ^^identifier_of, ...);
    }

    // convert id to the literal encoding and return it
}
```
:::

For quality of implementation reasons, we want to include the identifier in the error description string we pass to the exception constructor, so that the subsequent error message will say `"the identifier 'риба' is not representable"` and not just `"identifier not representable"`.

There is no way to do that if we take and return `string_view` from the exception constructor and `what()`. Since the failure is caused by the identifier not being representable in the literal encoding, it trivially follows that we can't put it into an error string that uses the literal encoding.

That is why we believe that the currently proposed interface, taking and returning `u8string_view`, is the path to take in order to maintain consistency with the current design of [@P2996R8], which is the result of extensive discussions in SG16.

(The fact that the standard library doesn't provide adequate support for `char8_t` strings is fixable by... adding that support.)

What if, however, we follow [@P2996R8] even more closely and provide two constructors for `meta::exception`, one taking `u8string_view` and one taking `string_view`?

This is possible, and will allow users to take advantage of present standard library facilities for string manipulation when constructing the exception. We don't propose it here for two reasons. One, it allows writing code that isn't quite correct, even though it would probably work on all tested platforms.

Consider this hypothetical user function:

::: std
```cpp
consteval auto user_fn(info x)
{
    // ...

    info y = member_of(x, i);

    if( is_private(y) )
    {
        throw meta::exception(std::format("member {} is private", identifier_of(y)), ^^user_fn, ...);
    }

    // ...
}
```
:::

If the identifier of `y` is not representable in the literal encoding, this function will throw an exception that says that the identifier is not representable, instead of the correct one saying that the member is private.

Since in the common case the literal encoding is UTF-8, this mistake is going to go undetected.

Whether this is a problem worth worrying about is up for debate. The principled approach is to only take `u8string_view`, forcing correct user code whether the user likes it or not. A less principled but considerably more pragmatic approach would prefer ease of use over forced (over-)correctness and add the `string_view` constructor.

But the second reason for our not proposing the additional `string_view` constructor is that it's easy to add at a later date.

## Post-SG16 Telecon

After the SG16 telecon on February 5th, 2025, which discussed the question of encoding above and whether we want `string_view` or `u8string_view` construction and access, we think the more usable API might be more like:

::: std
```cpp
namespace std::meta {

class exception
{
private:
  u8string $what_$;         // exposition only
  info $from_$;             // exposition only
  source_location $where_$; // exposition only

public:
  consteval exception(u8string_view what, info from,
    source_location where = source_location::current()) noexcept
    : $what_$(what), $from_$(from), $where_$(where) {}

  consteval exception(string_view what, info from,
    source_location where = source_location::current()) noexcept
    : $what_$($ordinary-to-u8$(what)), $from_$(from), $where_$(where) {}

  consteval u8string_view u8what() const noexcept {
    return $what_$;
  }
  consteval string what() const noexcept {
    return $u8-to-ordinary$($what_$);
  }

  // ...
};

}
```
:::

where `what()` fails to be constant if it cannot transcode. It would be nice if we had at least `$u8-to-ordinary$` and `$ordinary-to-u8$` already specified and present but, well, today is better than tomorrow.

This gives us a maximally usable API — since the standard library has plenty of support for `string` formatting and that can be used here, the conversion from ordinary to UTF-8 is fine. It does still mean that attempting to call `what()` could fail, but... so be it.


## Single or Multiple Types

We are proposing a single exception type. The runtime analogy is `std::system_error` as opposed to a hierarchy of exception types.

This in principle makes user code that wishes to inspect the failure reason and do different things depending on it less convenient to write. It would have to look like this

::: std
```cpp
catch( meta::exception const& x )
{
    if( x.from() == ^^identifier_of )
    {
        // handle errors originating from identifier_of
    }
    else if( x.from() == ^^members_of )
    {
        // handle errors originating from members_of
    }
    // ...
}
```
:::

instead of, hypothetically, like this

::: std
```cpp
catch( meta::identifier_exception const& x )
{
    // handle errors originating from identifier_of
}
catch( meta::members_exception const& x )
{
    // handle errors originating from members_of
}
// ...
```
:::

(exception type names are illustrative.)

We don't propose an exception hierarchy here. Designing a proper exception hierarchy is not something we can realistically do in the C++26 timeframe.
It's not as straightforward as just using an exception per function because functions can fail for multiple reasons, and client code may well wish to distinguish between these.

Furthermore, an exception hierarchy can be designed at a later date, with the functions changed to throw an appropriate type derived from the currently proposed `meta::exception`.
Code written against this proposal will continue to work unmodified, and new code would be able to use more specific catch clauses.

# Recoverable or Unrecoverable

We went through the proposed API in [@P2996R8] and we think that all of the library functions should be recoverable — that is failing to meet the requirements of the function should be an exception rather than constant evaluation failure — with a single exception, `std::meta::define_aggregate`.

`define_aggregate` isn't likely to be used from a context from which recovery is meaningful, and even if it were, for meaningful recovery we would have to guarantee that the partial effects of a failure have been rolled back (as a definition containing some of the members may already have been produced at the point where the error is detected.)
We don't believe that imposing this requirement is warranted or worth the cost.

The rest of the library functions are straightforwardly fallible, so the ability to recover from them is desirable.

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

  consteval exception(string_view what, info from,
    source_location where = source_location::current()) noexcept;

  exception(exception const&) = default;
  exception(exception&&) = default;

  exception& operator=(exception const&) = default;
  exception& operator=(exception&&) = default;

  consteval u8string_view u8what() const noexcept;
  consteval string what() const noexcept;
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
consteval exception(string_view what, info from,
    source_location where = source_location::current()) noexcept;
```

[#]{.pnum} *Effects*: Initializes `$what_$` with `what`, transcoded from the ordinary literal encoding to UTF-8, `$from_$` with `from` and `$where_$` with `where`.

```cpp
consteval u8string_view u8what() const noexcept;
```

[#]{.pnum} *Returns*: `$what_$`.

```cpp
consteval string what() const noexcept;
```

[#]{.pnum} *Constant When*: `$what_$` can be represented in the ordinary literal encoding.

[#]{.pnum} *Returns*: `$what_$`, converted to the ordinary literal encoding.

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

Replace the error handling in this subclause:

::: std
```cpp
consteval string_view identifier_of(info r);
consteval u8string_view u8identifier_of(info r);
```

[#]{.pnum} Let *E* be UTF-8 if returning a `u8string_view`, and otherwise the ordinary literal encoding.

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `has_identifier(r)` is `true` and the identifier that would be returned (see below) is representable by `$E$`.
:::

## [meta.reflection.queries]

Replace the error handling in this subclause:

::: std
```cpp
consteval info type_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents a value, object, variable, function that is not a constructor or destructor, enumerator, non-static data member, bit-field, direct base class relationship, or data member description.
:::

::: std
```cpp
consteval info object_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents either an object with static storage duration ([basic.stc.general]), or a variable associated with, or referring to, such an object.
:::

::: std
```cpp
consteval info value_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` is a reflection representing

* [#.#]{.pnum} a value,
* [#.#]{.pnum} an enumerator, or
* [#.#]{.pnum} an object or variable `$X$` such that the lifetime of `$X$` has not ended, the type of `$X$` is a structural type, and either `$X$` is usable in constant expressions from some point in the evaluation context or the lifetime of `$X$` began in the manifestly constant-evaluated expression currently under evaluation ([expr.const]), ([temp.type]).
:::

::: std
```cpp
consteval info parent_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents a variable, structured binding, function, enumerator, class, class member, bit-field, template, namespace or namespace alias (other than `::`), type alias, or direct base class relationship.
:::

::: std
```cpp
consteval info template_of(info r);
consteval vector<info> template_arguments_of(info r);
```
[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `has_template_arguments(r)` is `true`.
:::

## [meta.reflection.member.queries]

Replace the error handling in this subclause:

::: std
```cpp
consteval vector<info> members_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` is a reflection representing either a class type that is complete from some point in the evaluation context or a namespace.
:::

::: std
```cpp
consteval vector<info> bases_of(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` is a reflection representing a complete class type.
:::

::: std
```cpp
consteval vector<info> static_data_members_of(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

::: std
```cpp
consteval vector<info> nonstatic_data_members_of(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

::: std
```cpp
consteval vector<info> enumerators_of(info type_enum);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type_enum)` represents an enumeration type and `has_complete_definition(dealias(type_enum))` is `true`.
:::

::: std
```cpp
consteval vector<info> get_public_members(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

::: std
```cpp
consteval vector<info> get_public_bases(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

::: std
```cpp
consteval vector<info> get_public_static_data_members(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

::: std
```cpp
consteval vector<info> get_public_nonstatic_data_members(info type);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(type)` represents a complete class type.
:::

## [meta.reflection.layout]

Replace the error handling in this subclause:

::: std
```cpp
consteval member_offset offset_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents a non-static data member, unnamed bit-field, or direct base class relationship other than a virtual base class of an abstract class.
:::

::: std
```cpp
consteval size_t size_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.
:::

::: std
```cpp
consteval size_t alignment_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(r)` is a reflection representing a type, object, variable, non-static data member that is not a bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.
:::

::: std
```cpp
consteval size_t bit_size_of(info r);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `dealias(r)` is a reflection of a type, object, value, variable of non-reference type, non-static data member, unnamed bit-field, direct base class relationship, or data member description. If `dealias(r)` represents a type `$T$`, there is a point within the evaluation context from which `$T$` is not incomplete.
:::

## [meta.reflection.extract]

Replace the error handling in this subclause:

::: std
```cpp
template <class T>
  consteval T $extract-ref$(info r); // exposition only
```

[#]{.pnum} [`T` is a reference type.]{.note}

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `r` represents a variable or object of type `U` that is usable in constant expressions from some point in the evaluation context and `is_convertible_v<remove_reference_t<U>(*)[], remove_reference_t<T>(*)[]>` is `true`.
:::

::: std
```cpp
template <class T>
  consteval T $extract-member-or-function$(info r); // exposition only
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless the following conditions are met:]{.addu}

- [#.#]{.pnum} If `r` represents a non-static data member of a class `C` with type `X`, [then when]{.rm} `T` is `X C::*` and `r` does not represent a bit-field.
- [#.#]{.pnum} Otherwise, if `r` represents an implicit object member function of class `C` with type `F` or `F noexcept`, [then when]{.rm} `T` is `F C::*`.
- [#.#]{.pnum} Otherwise, `r` represents a function, static member function, or explicit object member function of function type `F` or `F noexcept`, [then when]{.rm} [and]{.addu} `T` is `F*`.
:::

::: std
```cpp
template <class T>
  consteval T $extract-val$(info r); // exposition only
```

[#]{.pnum} Let `U` be the type of the value that `r` represents.

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless the following conditions are met:]{.addu}

  - [#.#]{.pnum} `U` is a pointer type, `T` and `U` are similar types ([conv.qual]), and `is_convertible_v<U, T>` is `true`,
  - [#.#]{.pnum} `U` is not a pointer type and the cv-unqualified types of `T` and `U` are the same, or
  - [#.#]{.pnum} `U` is a closure type, `T` is a function pointer type, and the value that `r` represents is convertible to `T`.
:::

## [meta.reflection.substitute]

Replace the error handling in this subclause:

::: std
```cpp
template <reflection_range R = initializer_list<info>>
consteval bool can_substitute(info templ, R&& arguments);
```
[1]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `templ` represents a template and every reflection in `arguments` represents a construct usable as a template argument ([temp.arg]).
:::

::: std
```cpp
template <reflection_range R = initializer_list<info>>
consteval info substitute(info templ, R&& arguments);
```

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `can_substitute(templ, arguments)` is `true`.
:::

## [meta.reflection.result]

Replace the error handling in this subclause:

::: std
```cpp
template <typename T>
  consteval info reflect_value(const T& expr);
```

[#]{.pnum} *Mandates*: `T` is a structural type that is neither a reference type nor an array type.

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless any]{.addu} [Any]{.rm} value computed by `expr` having pointer type, or every subobject of the value computed by `expr` having pointer or reference type, is the address of or refers to an object or function that

  - [#.#]{.pnum} is a permitted result of a constant expression ([expr.const]),
  - [#.#]{.pnum} is not a temporary object ([class.temporary]),
  - [#.#]{.pnum} is not a string literal object ([lex.string]),
  - [#.#]{.pnum} is not the result of a `typeid` expression ([expr.typeid]), and
  - [#.#]{.pnum} is not an object associated with a predefined `__func__` variable ([dcl.fct.def.general]).
:::

::: std
```cpp
template <typename T>
  consteval info reflect_object(T& expr);
```

[#]{.pnum} *Mandates*: `T` is not a function type.

[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless]{.addu} `expr` designates an object or function that

  - [#.#]{.pnum} is a permitted result of a constant expression ([expr.const]),
  - [#.#]{.pnum} is not a temporary object ([class.temporary]),
  - [#.#]{.pnum} is not a string literal object ([lex.string]),
  - [#.#]{.pnum} is not the result of a `typeid` expression ([expr.typeid]), and
  - [#.#]{.pnum} is not an object associated with a predefined `__func__` variable ([dcl.fct.def.general]).
:::

## [meta.reflection.define.aggregate]

Replace the error handling in this subclause:

::: std
```cpp
consteval info data_member_spec(info type,
                                data_member_options options);
```
[#]{.pnum} [*Constant When*]{.rm} [*Throws*]{.addu}: [`meta::exception` unless the following conditions are met:]{.addu}

- [#.#]{.pnum} `dealias(type)` represents a type `cv $T$` where `$T$` is either an object type or a reference type;
- [#.#]{.pnum} if `options.name` contains a value, then:
  - [#.#.#]{.pnum} `holds_alternative<u8string>(options.name->$contents$)` is `true` and `get<u8string>(options.name->$contents$)` contains a valid identifier when interpreted with UTF-8, or
  - [#.#.#]{.pnum} `holds_alternative<string>(options.name->$contents$)` is `true` and `get<string>(options.name->$contents$)` contains a valid identifier when interpreted with the ordinary literal encoding;
- [#.#]{.pnum} otherwise, if `options.name` does not contain a value, then `options.bit_width` contains a value;
- [#.#]{.pnum} if `options.alignment` contains a value, it is an alignment value ([basic.align]) not less than `alignment_of(type)`; and
- [#.#]{.pnum} if `options.bit_width` contains a value `$V$`, then
  - [#.#.#]{.pnum} `is_integral_type(type) || is_enumeration_type(type)` is `true`,
  - [#.#.#]{.pnum} `options.alignment` does not contain a value,
  - [#.#.#]{.pnum} `options.no_unique_address` is `false`, and
  - [#.#.#]{.pnum} if `$V$` equals `0` then `options.name` does not contain a value.
:::

## Feature-Test Macro

Bump `__cpp_lib_reflection` in [version.syn]{.sref} (which isn't there yet) to some new value:

::: std
```diff
+ #define __cpp_lib_reflection 2025XXL // also in <meta>
```
:::