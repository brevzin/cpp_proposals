---
title: "String Interpolation with Template Strings"
document: P3951R0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Introduction

The `std::format` approach to formatting offers many significant benefits over the prior `<iostream>`s approach that need not be revisited here. However, `<iostream>` does still have one significant advantage: ordering. It is easy to see at a glance with a long `std::cout` statement which pieces are to be formatted in which order. With `std::format`, as the amount of replacement fields increases, it becomes increasingly difficult to ensure that they are all correctly ordered.

The solution to this problem is string interpolation: the ability to put the expression to be formatted inside of the format string. This lets us preserve all of the advantages of `std::format`, while also regaining ordering. String interpolation is a wildly popular language feature due to the ease with which it allows users to express complex ideas. It's not surprising that a huge number of modern languages support this to some degree or another. A non-exhaustive list includes: C#, D, Elixir, F#, Groovy, Kotlin, JavaScript, Perl, PHP, Python, Ruby, Rust, Scala, Swift, and VB.

## Prior Work

There have been two prior WG21 papers pursuing string interpolation as a C++ language feature: [@P1819R0]{.title} and [@P3412R3]{.title}. The two proposals are quite different, so let's consider an example to work through the details:

::: std
```cpp
auto get_result() -> int { return 42; }

auto example() -> void {
    auto interp = f"The result is {get_result()}\n";     // #1
    std::print(interp);                                  // #2
    std::print(f"The result is {get_result()}\n");       // #3
}
```
:::

In P1819, the `interp` is an object that is roughly equivalent to:

::: std
```cpp
auto interp = [&](auto&& f) -> decltype(auto) {
    return f("The result is ", get_result(), "\n");
};
```
:::

So line `#1` does nothing. The call to `get_result()` does not happen yet. Instead, the library would provide new overloads of `std::print` and friends so that in line `#2`, the library would invoke `interp` with the appropriate function to do the printing. The call to `get_result()` happens at that point. Line `#3` does the same things as lines `#1` and `#2`, just together.

In P3412, the behavior is very different. `interp` is already a `std::string`, which is evaluated as:

::: std
```cpp
auto interp = std::format("The result is {}\n", get_result());
```
:::

This makes the call in line `#2` ill-formed, since `std::print` cannot accept a `std::string`. However, the call in line `#3` is valid — by way of a change to overload resolution that recognizes this case as special and instead evaluates the call directly as:

:::std
```cpp
std::print("The result is {}\n", get_result());
```
:::

In short, P1819 gives us an object (that doesn't evaluate any of the expressions) while P3412 gives us either a `std::string` or an argument list, depending on context.

Of the two, I think P1819 is significantly better. We get a simple object that can allow for a wide variety of potential functionality. It has two big problems though. The first is that it evaluates lazily and stores its data opaquely — which leads to more surprising behavior, the potential for dangling references, and arbitrarily limited usage. The other is its breakup into pieces doesn't actually play very well with `std::format` — where we would want there to be a format string and we don't have one. The original motivation for the lambda approach was ease of use — get all the expressions in one convenient format. But the language has evolved since 2019. We have both reflection and packs in structured bindings now, so we don't need the lambda approach anymore.

On the other hand, P3412 is a very complex design, with an overload resolution mechanism that is very strongly coupled to the current implementation strategy of formatting. What if we someday get `constexpr` function parameters and it turns out to be better to implement `basic_format_string<char, Args...>` as taking a `constexpr string_view` instead of it being a `consteval` constructor? Would we need to change how string interpolation is defined too? All this complexity buys us is the ability to create a `std::string` in a single character. However, I don't think that's even a good goal for C++ — we shouldn't hide an operation as costly as string formatting in a single character, and we shouldn't tie the language so tightly to this particular library (as this proposal does via `__format__`).

Instead, this paper proposes an idea much closer to the P1819 model.

## Prior Art in Python

Python 3.6 introduced literal string interpolation (`f"..."`) in [@PEP-498], which was later extended in Python 3.14 by template strings (`t"..."`) in [@PEP-750]. The former directly produces a `string`, while the latter gives a template string — an object with enough information in it to be formatted later. This is similar to Rust's `format_args!`, which gives you a completely opaque object (unlike Python's which is completely specified).

Both languages gives you a facility to take an interpolated string and produce an object for future work (similar to P1819).

# Design

This paper proposes that we introduce string interpolation for C++ following the same idea as Python's template strings. We'll have to come up with a better name than "template string" for this, but for now I'm going to stick with it. A template string literal will eagerly evaluate all of the expressions and produce a new object with all of the relevant pieces, such that it will be suitable for both formatting APIs (like `std::format` and `std::print`) and other APIs that have nothing to do with formatting.

Our example from earlier:

::: std
```cpp
auto get_result() -> int { return 42; }

auto example() -> void {
    auto interp = t"The result is {get_result()}\n";     // #1
    std::print(interp);                                  // #2
    std::print(t"The result is {get_result()}\n");       // #3
}
```
:::

will evaluate as:

::: std
```cpp
struct $Template$ {
    static constexpr char const* fmt = "The result is {}\n";

    static constexpr char const* strings[] = {"The result is ", "\n"};
    static constexpr std::interpolation interpolations[] = {{"get_result()", "{}", 0, 1}};

    int $_0$;
};

auto example() -> void {
    auto interp = $Template${get_result()};
    std::print(interp);
    std::print($Template${get_result()});
}
```
:::

Let's go through all of these pieces in order. A template string will generate an instance of a not-necessarily-unique type (unlike a lambda expression, which always has a unique type) by parsing the replacement fields of the string literal. The parsing logic here is very basic and does not need to understand very much about either the format specifier mini-language or C++ expressions more broadly. The interpolation type will have four public pieces of information in it:

* `fmt` is a static data member that contains a format string,
* `strings` is a static array of string literals that are just the string parts,
* `interpolations` is a static array of the [interpolation information](#interpolation-information), and then
* one non-static data member for each expression. The type of the member for expression `$E$` is `decltype(($E$))`, and the name of the member is unspecified.

With this structure, we can easily add additional overloads to the format library to handle template strings:

::: std
```cpp
template <TemplateString S>
auto print(S&& s) -> void {
    auto& [...pieces] = s;
    std::print(s.fmt(), pieces...);
}
```
:::

Note that there is no difference in handling between lines `#1-2` and line `#3` in the example (like the P1819 design and unlike the P3412 one). A template string is just an object, that contains within it all the relevant information.

The rest of the paper will go through details on first on how the parsing works and then into other examples to help motivate the structure.

Keep in mind that since a template string object is _just an object_, where most of the information are static data members, this ends up being a very embedded-friendly design too.

## Parsing

A template string is conceptually an alternating sequence of string literals and interpolations. The string literal parts are just normal string literals — we look ahead until we find a non-escaped `{` to start the next replacement field (`"{{"` in a format string is used to print the single character `'{'`). A replacement-field comes in two forms:

::: std
```cpp
$replacement-field$:
  { $expr$ }
  { $expr$ : $format-spec$ }
```
:::

C++ expressions can be arbitrary complicated. Notably, they can also include `:` or `}`. both of which are significant in formatting and indicate the end of the expression. So how do we know when we're done with the `$expr$` part here?

One approach would be to simply limit the kinds of expressions that can appear in template strings. Rust, for instance, _only_ supports identifiers. That obviously makes parsing quite easy, but it also is very limiting. On the other extreme, supporting _all_ expressions can easily lead to undecipherable code. I think on balance, supporting only identifiers is far too restrictive. But once you start adding what other kinds of expressions to allow (surely, at least class member access), it quickly becomes too difficult to keep track of what is allowed (indexing? function calls? splices?) and ironically makes both the implementation more difficult (to enforce what is and isn't allowed) and harder to understand for the user (to know which expressions are and aren't allowed).

I think it's best to simply allow anything in the expression (as Python does) and trust the user to refactor their expressions to be as legible as they desire. This allows us to take a very simple approach: we simply lex `$expr$` as a balanced token sequence (just counting `{}`s, `()`s, and `[]`s), so that colons and braces inside of any of the bracket kinds are treated as part of an expression. But the first `:` or `}` encountered when we're at a brace depth of zero means we're done with `$expr$`.

Here are some examples of template string formatting calls and how they would be evaluated:

|template string|evaluated as|
|-|-|
|`std::format(t"x={x}")`|`std::format("x={}", x)`|
|`std::format(t"x={[]{ return 42; }()}")`|`std::format("x={}", []{ return 42; }())`|
|`std::format(t"x={co_await f(x) + g(y) / z}")`|`std::format("x={}", co_await f(x) + g(y) / z)`|
|`std::format(t"x={a::b}")`|`std::format("x={::b}", a)`|
|`std::format(t"x={(a::b)}")`|`std::format("x={}", (a::b))`|
|`std::format(t"x={cond ? a : b}")`|`std::format("x={:b}", cond ? a)` ❌|
|`std::format(t"x={(cond ? a : b)}")`|`std::format("x={}", (cond ? a : b))`|

In order to format expressions that use scoping or the conditional operator, you'll just have to write parentheses. That seems easy enough to both understand and use. Otherwise, anything goes.

## Parsing Nested Expressions

Consider the template string:

::: std
```cpp
t"{name:>{width}}"
```
:::

There are two expressions here: `name` and `width`. Or rather, we know `name` is an expression, but how do we know that `width` is? In the format model, the format specifiers can be _anything_. There really are no rules — as long as the type's formatter can handle it. Having nested braces in a format specifier commonly refers to another argument, but it need not actually mean that.

In my CppCon 2022 talk [The Surprising Complexity of Formatting Ranges](https://youtu.be/EQELdyecZlU?t=2397), I work through an example of how one might add underlying specifiers to `std::pair` and `std::tuple`, [where](https://godbolt.org/z/vPfE7er3M):

::: std
```cpp
int main() {
    auto elems = std::tuple(10, 20, 30);
    fmt::print("{}\n", elems);                  // (10, 20, 30)
    fmt::print("{:{x}{#x}{-^4}}\n", elems);     // (a, 0x14, -30-)
    fmt::print("{:{x}{}{x}}\n", elems);         // (a, 20, 1e)
}
```
:::

There, `{x}` doesn't refer to the expression `x`, it was just chosen as notational convenience. So how can we get this to work?

::: std
```cpp
fmt::print(t"{elems:{x}{}{x}}\n");
```
:::

The short answer is: we cannot. We need to make sense of the template string literal separate from type information, and even if we had type information, it's not like we have a way for the `formatter` API to signal when it's expecting an expression. We just have to make a choice up front for what to do here. I think we have three options:

1. We could explicitly opt _in_ to a nested expression. That is, maybe something like `t"{elems:{x}{}${x}}"` signals that the first `{x}` is just a string but the second `{x}` is actually the expression `x`. So in the above example, we simply wouldn't use the `$`{.op}.
2. We could explicitly opt _out_ of being a nested expression with added escaping. Which in this case would be `t"{elems:{{x}}{{}}{{x}}}"`.
3. We could say that in a format specifier, encountering a `{` _always_ begins an expression that ends at `:` or `}` (same as top-level) and that there is neither opt-in nor opt-out. Meaning that the approach to format specifiers that I demonstrated in that talk wouldn't work, and would instead have to be `t"{elems:{:x}{}{:x}}"`.

I think the third option here is the best. It is at most a minor burden on users as I doubt this approach is in widespread use, and allows for a design that is as simple as possible.

Getting back to our original example:

::: std
```cpp
t"{name:>{width}}"
```
:::

This would parse as two expressions: `name` and `width`, with a single corresponding format string `"{:>{}}"`. If we simply stored the expressions and the format string, that would be straightforward: we just have two [data members](#data-members). But we can do better than that. But before we get into the [interpolation information](#interpolation-information), I'll talk about the data members.

## Data Members

For any expression, `$E$`, including nested expressions, a non-static data member will be generated (and then initialized from `$E$`) having type `decltype(($E$))`. This ensures that we get the right type, but also that we're not copying anything unnecessarily. For instance:

::: std
```cpp
auto verb() -> std::string;

auto example(std::string const& name, std::string relation) -> void {
    auto tmpl = t"Hello, my name is {name}. You killed my {relation}. Prepare to {verb()}.";
}
```
:::

The object `tmpl` will have three members:

* the first, corresponding to `name`, has type `std::string const&`
* the second, corresponding to `relation`, has type `std::string&`
* the third, corresponding to `verb()`, has type `std::string`

These are all public, non-static data members. If we want to copy all of the members (e.g. because we want to serialize them), we can easily do so. It's just that there is no need for the template string itself to do so directly. Note that the call to `verb()` happens immediately and the result is stored in `tmpl`. We are not lazily holding onto the expression `verb()`.

## Interpolation Information

A template string will have a `static constexpr` data member of type `std::interpolation`, which is a simple aggregate with four members:

::: std
```cpp
struct interpolation {
    char const* expression;
    char const* fmt;
    size_t index;
    size_t count;
};
```
:::

`expression` is a string literal that is the stringified version of the expression. `fmt` is the full format specifier with the expression removed, that you would need in order to format this specific expression. Then, we need both `index` and `num` in order to support nested replacement expressions, as we just went through. That gives us both the first non-static data member and the amount of non-static data members this interpolation is associated with.

The template string literal `t"{name:>{width}}"` needs to generate the object

::: std
```cpp
struct $Template$ {
    static constexpr char const* fmt = "{:>{}}";
    static constexpr char const* strings[] = {"", ""};
    static constexpr interpolation interpolations[] = {{
        .expression = "name",
        .fmt = "{:>{}}",
        .index = 0,
        .count = 2
    }};

    std::string const& $_0$;
    int const& $_1$;
};
```
:::


Why do we go through the trouble of proving `strings` and `interpolations`?  Consider again our example from earlier:

::: std
```cpp
auto tmpl = t"Hello, my name is {name}. You killed my {relation}. Prepare to {verb()}.";
```
:::

I already showed how we could print this normally, which didn't use either of those two arrays:

::: std
```cpp
template <TemplateString S>
auto print(S&& s) -> void {
    auto& [...pieces] = s;
    std::print(s.fmt, pieces...);
}
```
:::

But we could do a lot more with this object other than just print it. We could write a function to automatically highlight the interpolations green and bold, which is supported by the `fmt` library but not the standard. That's straightforward, since we're not tied into any particular library, and we have all the information we need:

::: std
```cpp
template <TemplateString S>
auto highlighted_print(S&& s) -> void {
    auto& [...exprs] = s;

    template for (constexpr auto [str, interp] : std::views::zip(s.strings, s.interpolations)) {
        fmt::print(str);

        constexpr auto [...I] = std::make_index_sequence<interp.count>();
        fmt::print(fmt::emphasis::bold | fg(fmt::color::green),
                   interp.fmt,
                   exprs...[interp.index + I]...);
    }

    fmt::print(s.strings[std::size(s.strings) - 1]);
}
```
:::

Or we could turn it into the JSON object `{"name": "Inigo Montoya", "relation": "father", "verb()": "die"}`:

::: std
```cpp
template <TemplateString S>
auto into_json(S&& s) -> boost::json::object {
    auto& [...exprs] = s;

    boost::json::object o;
    template for (constexpr auto interp : s.interpolations) {
        o[interp.expression] = exprs...[interp.index];
    }
    return o;
}
```
:::

The important thing is to expose all the relevant information to users to let them do whatever they want with it. Note that `highlighted_print` uses the `fmt`, `index`, and `count` fields of the interpolation, since it is formatting all of them, but not the `expression` field. Meanwhile, `into_json` uses only `expression` and `index` — it doesn't need any of the format specifier logic, since it isn't actually doing formatting.

Having both `interp.index` and `interp.count` is a little clunky, especially since `interp.count` will almost always be `1`. But I think it's better to put the clunkiness there and maintain the trivial formatting implementations (where you can just unpack the template string object).

## The `TemplateString` Concept

In these examples, I've been using this `TemplateString` concept to identify a template string object. The question is, what does that concept look like? This one I'm not sure about yet. It can't be a built-in, since users might need to create one of these objects. Consider a logger:

::: std
```cpp
log::info(t"Got a trade for in {symbol}: {side} {qty} @ {price}");
```
:::

The expressions here are all cheap to copy, but formatting is expensive — so I might want to serialize all of the data into a background thread to do my formatting there. I don't want to just copy the template string object, since it might have references. With reflection, I can create a new type that has only value members, and then keep the same `fmt`, `strings`, and `interpolations`:

::: std
```cpp
template <TemplateString S, class F>
auto map(S s, F f) {
    struct Base;
    consteval {
        vector<meta::info> specs;
        for (meta::info m : nonstatic_data_members_of(^^S)) {
            specs.push_back(data_member_spec({
                .type=invoke_result(^^F, {type_of(m)}),
                .name=identifier_of(m),
            }));
        }
        define_aggregate(^^Base, specs);
    }
    struct R : Base {
        static constexpr char const* fmt = S::fmt;
        static constexpr auto const& strings = S::strings;
        static constexpr auto const& interpolations = S::interpolations;
    };

    auto& [...pieces] = s;
    return R{{f(pieces)...}};
}
```
:::

I'd want to make sure this `R` here is also considered a template string for all of these purposes. There are a few attributes here, not really sure which would be best:

* an attribute
* an annotation
* structural conformance: simply check for the presence of `fmt`, `strings`, and `interpolations`?

## Implementation Experience

I implemented this in Clang, on top of the p2996 reflection branch. Code can be found in my fork in the `template-strings`{.op} branch [here](https://github.com/brevzin/llvm-project/tree/template-strings). I'm sure there are better ways to do some of what I did, but on the whole, the implementation is completely standalone. The only difference between this paper and the implementation is that instead of introducing the type `std::interpolation`, I just made `interpolation` a nested class of each template string type, just for convenience.

This does raise the question of how `std::interpolation` should be defined. It does make sense for it to be one type, rather than the distinct type per template string that I implemented. But should this facility really require a new header? Maybe it's a sufficiently trivial type (an aggregate with no member functions and just four data members, each of scalar type) that the compiler can just generate it? Maybe we don't care about additional headers because `import std;` anyway?

---
references:
    - id: PEP-498
      citation-label: PEP-498
      title: Literal String Interpolation
      author:
        - family: Eric V. Smith
      issued:
        - year: 2015
          month: 08
          day: 01
      URL: https://peps.python.org/pep-0498/
    - id: PEP-750
      citation-label: PEP-750
      title: Template Strings
      author:
        - family: Jim Baker
        - family: Guido van Rossum
        - family: Paul Everitt
        - family: Koudai Aono
        - family: Lysandros Nikolaou
        - family: Dave Peck
      issued:
        - year: 2024
          month: 07
          day: 08
      URL: https://peps.python.org/pep-0750/
---