---
title: "Formatting Ranges"
document: P2286R4
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
toc-depth: 4
---

# Revision History

Since [@P2286R3], several major changes:

* Removed the special `pair`/`tuple` parsing for individual elements. This proved complicated and illegible.
* Renaming `format_as_debug` to `set_debug_format` (since it's not actually _formatting_ anything, it's just setting up)

Since [@P2286R2], several major changes:

* This paper assumes the adoption of [@P2418R0], which affects how [non-`const`-iterable views](#how-to-support-those-views-which-are-not-const-iterable) are handled. This paper now introduces two concepts (`formattable` and `const_formattable`) instead of just one.
* Extended discussion and functionality for various [representations](#what-representation), including how to quote strings properly and how to format associative ranges.
* Introduction of format specifiers of all kinds and discussion of how to make them work more broadly.
* Removed the wording, since the priority is the design.

Since [@P2286R1], adding a sketch of wording.

[@P2286R0] suggested making all the formatting implementation-defined. Several people reached out to me suggesting in no uncertain terms that this is unacceptable. This revision lays out options for such formatting.

# Introduction

[@LWG3478] addresses the issue of what happens when you split a string and the last character in the string is the delimiter that you are splitting on. One of the things I wanted to look at in research in that issue is: what do _other_ languages do here?

For most languages, this is a pretty easy proposition. Do the split, print the results. This is usually only a few lines of code.

Python:

::: bq
```python
print("xyx".split("x"))
```
outputs
```
['', 'y', '']
```
:::

Java (where the obvious thing prints something useless, but there's a non-obvious thing that is useful):

::: bq
```java
import java.util.Arrays;

class Main {
  public static void main(String args[]) {
    System.out.println("xyx".split("x"));
    System.out.println(Arrays.toString("xyx".split("x")));
  }
}
```
outputs
```
[Ljava.lang.String;@76ed5528
[, y]
```
:::

Rust (a couple options, including also another false friend):

::: bq
```rust
use itertools::Itertools;

fn main() {
    println!("{:?}", "xyx".split('x'));
    println!("[{}]", "xyx".split('x').format(", "));
    println!("{:?}", "xyx".split('x').collect::<Vec<_>>());
}
```
outputs
```
Split(SplitInternal { start: 0, end: 3, matcher: CharSearcher { haystack: "xyx", finger: 0, finger_back: 3, needle: 'x', utf8_size: 1, utf8_encoded: [120, 0, 0, 0] }, allow_trailing_empty: true, finished: false })
[, y, ]
["", "y", ""]
```
:::

Kotlin:

::: bq
```kotlin
fun main() {
    println("xyx".split("x"));
}
```
outputs
```
[, y, ]
```
:::

Go:

::: bq
```go
package main
import "fmt"
import "strings"

func main() {
    fmt.Println(strings.Split("xyx", "x"));
}
```
outputs
```
[ y ]
```
:::

JavaScript:

::: bq
```js
console.log('xyx'.split('x'))
```
outputs
```
[ '', 'y', '' ]
```
:::

And so on and so forth. What we see across these languages is that printing the result of split is pretty easy. In most cases, whatever the print mechanism is just works and does something meaningful. In other cases, printing gave me something other than what I wanted but some other easy, provided mechanism for doing so.

Now let's consider C++.

::: bq
```cpp
#include <iostream>
#include <string>
#include <ranges>
#include <format>

int main() {
    // need to predeclare this because we can't split an rvalue string
    std::string s = "xyx";
    auto parts = s | std::views::split('x');

    // nope
    std::cout << parts;

    // nope (assuming std::print from P2093)
    std::print("{}", parts);


    std::cout << "[";
    char const* delim = "";
    for (auto part : parts) {
        std::cout << delim;

        // still nope
        std::cout << part;

        // also nope
        std::print("{}", part);

        // this finally works
        std::ranges::copy(part, std::ostream_iterator<char>(std::cout));

        // as does this
        for (char c : part) {
            std::cout << c;
        }
        delim = ", ";
    }
    std::cout << "]\n";
}
```
:::

This took me more time to write than any of the solutions in any of the other languages. Including the Go solution, which contains 100% of all the lines of Go I've written in my life.

Printing is a fairly fundamental and universal mechanism to see what's going on in your program. In the context of ranges, it's probably the most useful way to see and understand what the various range adapters actually do. But none of these things provides an `operator<<` (for `std::cout`) or a formatter specialization (for `format`). And the further problem is that as a user, I can't even do anything about this. I can't just provide an `operator<<` in `namespace std` or a very broad specialization of `formatter` - none of these are program-defined types, so it's just asking for clashes once you start dealing with bigger programs.

The only mechanisms I have at my disposal to print something like this is either

1. nested loops with hand-written delimiter handling (which are tedious and a bad solution), or
2. at least replace the inner-most loop with a `ranges::copy` into an output iterator (which is more differently bad), or
3. Write my own formatting library that I _am_ allowed to specialize (which is not only bad but also ridiculous)
4. Use `fmt::format`.

## Implementation Experience

That's right, there's a fourth option for C++ that I haven't shown yet, and that's this:

::: bq
```cpp
#include <ranges>
#include <string>
#include <fmt/ranges.h>

int main() {
    std::string s = "xyx";
    auto parts = s | std::views::split('x');

    fmt::print("{}\n", parts);
    fmt::print("<<{}>>\n", fmt::join(parts, "--"));
}
```
outputting
```
[[], ['y'], []]
<<[]--['y']--[]>>
```
:::

And this is great! It's a single, easy line of code to just print arbitrary ranges (include ranges of ranges).

And, if I want to do something more involved, there's also `fmt::join`, which lets me specify both a format specifier and a delimiter. For instance:

::: bq
```cpp
std::vector<uint8_t> mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
fmt::print("{:02x}\n", fmt::join(mac, ":"));
```
outputs
```
aa:bb:cc:dd:ee:ff
```
:::

`fmt::format` (and `fmt::print`) solves my problem completely. `std::format` does not, and it should.

# Proposal Considerations

The Ranges Plan for C++23 [@P2214R1] listed as one of its top priorities for C++23 as the ability to format all views. Let's go through the issues we need to address in order to get this functionality.

## What types to print?

The standard library is the only library that can provide formatting support for standard library types and other broad classes of types like ranges. In addition to ranges (both the conrete containers like `vector<T>` and the range adaptors like `views::split`), there are several very commonly used types that are currently not printable.

The most common and important such types are `pair` and `tuple` (which ties back into Ranges even more closely once we adopt `views::zip` and `views::enumerate`). `fmt` currently supports printing such types as well:

::: bq
```cpp
fmt::print("{}\n", std::pair(1, 2));
```
outputs
```
(1, 2)
```
:::

Another common and important set of types are `std::optional<T>` and `std::variant<Ts...>`. `fmt` does not support printing any of the sum types. There is not an obvious representation for them in C++ as there might be in other languages (e.g. in Rust, an `Option<i32>` prints as either `Some(42)` or `None`, which is also the same syntax used to construct them).

However, the point here isn't necessarily to produce the best possible representation (users who have very specific formatting needs will need to write custom code anyway), but rather to provide something useful. And it'd be useful to print these types as well. However, given that `optional` and `variant` are both less closely related to Ranges than `pair` and `tuple` and also have less obvious representation, they are less important.

## What representation?

There are several questions to ask about what the representation should be for printing. I'll go through each kind in turn.

### `vector` (and other ranges)

Should `std::vector<int>{1, 2, 3}` be printed as `{1, 2, 3}` or `[1, 2, 3]`? At the time of [@P2286R1], `fmt` used `{}`s but changed to use `[]`s for consistency with Python ([400b953f](https://github.com/fmtlib/fmt/commit/400b953fbb420ff1e47565303c64223445a51955)).

Even though in C++ we initialize `vector`s (and, generally, other containers as well) with `{}`s while Python's uses `[1, 2, 3]` (and likewise Rust has `vec![1, 2, 3]`), `[]` is typical representationally so seems like the clear best choice here.

### `pair` and `tuple`

Should `std::pair<int, int>{4, 5}` be printed as `{4, 5}` or `(4, 5)`? Here, either syntax can claim to be the syntax used to initialize the `pair`/`tuple`. `fmt` has always printed these types with `()`s, and this is also how Python and Rust print such types. As with using `[]` for ranges, `()` seems like the common representation for tuples and so seems like the clear best choice.

### `map` and `set` (and other associative containers)

Should `std::map<int, int>{@{@1, 2}, {3, 4}}` be printed as `[(1, 2), (3, 4)]` (as follows directly from the two previous choices) or as `{1: 2, 3: 4}` (which makes the *association* clearer in the printing)? Both Python and Rust print their associating containers this latter way.

The same question holds for sets as well as maps, it's just a question for whether `std::set<int>{1, 2, 3}` prints as `[1, 2, 3]` (i.e. as any other range of `int`) or `{1, 2, 3}`?

If we print `map`s as any other range of pairs, there's nothing left to do. If we print `map`s as associations, then we additionally have to answer the question of how user-defined associative containers can get printed in the same way. Hold onto this thought for a minute.

### `char` and `string` (and other string-like types) in ranges or tuples

Should `pair<char, string>('x', "hello")` print as `(x, hello)`{.x} or `('x', "hello")`{.x}? Should `pair<char, string>('y', "with\n\"quotes\"")` print as:

::: bq
```
(y, with
"quotes")
```
:::

or

::: bq
```
('y', "with\n\"quotes\"")
```
:::

While `char` and `string` are typically printed unquoted, it is quite common to print them quoted when contained in tuples and ranges (as Python, Rust, and `fmt` do). Rust escapes internal strings, so prints as `('y', "with\n\"quotes\"")` (the Rust implementation of `Debug` for `str` can be found [here](https://doc.rust-lang.org/src/core/fmt/mod.rs.html#2073-2095) which is implemented in terms of [`escape_debug_ext`](https://doc.rust-lang.org/src/core/char/methods.rs.html#405-419)). Following discussion of this paper and this design, Victor Zverovich implemented in this `fmt` as well.

Escaping seems like the most desirable behavior. Following Rust's behavior, we escape `\t`, `\r`, `\n`, `\\`, `"` (for `string` types only), `'` (for `char` types only), and extended graphemes (if Unicode).

Also, `std::string` isn't the only string-like type: if we decide to print strings quoted, how do users opt in to this behavior for their own string-like types? And `char` and `string` aren't the only types that may desire to have some kind of _debug_ format and some kind of regular format, how to differentiate those?

Moreover, it's all well and good to have the default formatting option for a range or tuple of strings to be printing those strings escaped. But what if users want to print a range of strings *unescaped*? I'll get back to this.

### Format Specifiers

One of (but hardly the only) the great selling points of `format` over iostreams is the ability to use specifiers. For instance, from the `fmt` documentation:

::: bq
```cpp
fmt::format("{:<30}", "left aligned");
// Result: "left aligned                  "
fmt::format("{:>30}", "right aligned");
// Result: "                 right aligned"
fmt::format("{:^30}", "centered");
// Result: "           centered           "
fmt::format("{:*^30}", "centered");  // use '*' as a fill char
// Result: "***********centered***********"
```
:::

Earlier revisions of this paper suggested that formatting ranges and tuples would accept no format specifiers, but there indeed are quite a few things we may want to do here (as by Tomasz Kamiński and Peter Dimov):

* Formatting a range of pairs as a map (the `key: value` syntax rather than the `(key, value)` one)
* Formatting a range of chars as a string (i.e. to print `hello`{.x} or `"hello"`{.x} rather than `['h', 'e', 'l', 'l', 'o']`{.x})

But these are just providing a specifier for how we format the range itself. How about how we format the elements of the range? Can I conveniently format a range of integers, printing their values as hex? Or as characters? Or print a range of chrono time points in whatever format I want? That's fairly powerful.

The problem is how do we actually *do that*. After a lengthy discussion with Peter Dimov, Tim Song, and Victor Zverovich, this is what we came up with. I'll start with a table of examples and follow up with a more detailed explanation.

Instead of writing a bunch of examples like `print("{:?}\n", v)`, I'm just displaying the format string in one column (the `"{:?}"` here) and the argument in another (the `v`):

|Format String|Contents|Formatted Output|
|-|---|---|
|`{:}`{.x}|`42`|`42`{.x}|
|`{:#x}`{.x}|`42`|`0x2a`{.x}|
|`{}`{.x}|`"h\tllo"s`|`h    llo`{.x}|
|`{:?}`{.x}|`"h\tllo"s`|`"h\tllo"`{.x}|
|`{}`{.x}|`vector{"h\tllo"s, "world"s}`|`["h\tllo", "world"]`{.x}|
|`{:}`{.x}|`vector{"h\tllo"s, "world"s}`|`["h\tllo", "world"]`{.x}|
|`{::}`{.x}|`vector{"h\tllo"s, "world"s}`|`[h    llo, world]`{.x}|
|`{:*^14}`{.x}|`vector{"he"s, "wo"s}`|`*["he", "wo"]*`{.x}|
|`{::*^14}`{.x}|`vector{"he"s, "wo"s}`|`[******he******, ******wo******]`{.x}|
|`{}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`['H', '\t', 'l', 'l', 'o']`{.x}|
|`{::}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[H,     , l, l, o]`{.x}|
|`{::c}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[H,     , l, l, o]`{.x}|
|`{::?c}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`['H', '\t', 'l', 'l', 'o']`{.x}|
|`{::d}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[72, 9, 108, 108, 111]`{.x}|
|`{::#x}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[0x48, 0x9, 0x6c, 0x6c, 0x6f]`{.x}|
|`{:s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`H    llo`{.x}|
|`{:?s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`"H\tllo"`{.x}|
|`{}`{.x}|`pair{42, "h\tllo"s}`|`(42, "h\tllo")`{.x}|
|`{}`{.x}|`vector{pair{42, "h\tllo"s}}`|`[(42, "h\tllo")]`{.x}|
|`{:m}`{.x}|`vector{pair{42, "h\tllo"s}}`|`{42: "h\tllo"}`{.x}|
|`{:m:}`{.x}|`vector{pair{42, "h\tllo"s}}`|`{42: h    llo}`{.x}|
|`{}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[['a'], ['b', 'c']]`{.x}|
|`{::?s}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`["a", "bc"]`{.x}|
|`{:::d}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[[97], [98, 99]]`{.x}|

### Explanation of Added Specifiers

#### The debug specifier `?`

`char` and `string` and `string_view` will start to support the `?` specifier. This will cause the character/string to be printed as quoted (characters with `'` and strings with `"`) and all characters to be escaped, as [described earlier](char-and-string-and-other-string-like-types-in-ranges-or-tuples).

This facility will be generated by the formatters for these types providing an addition member function (on top of `parse` and `format`):

::: bq
```cpp
void set_debug_format();
```
:::

Which other formatting types may conditionally invoke when they parse a `?`. For instance, since the intent is that range formatters print escaped by default, the logic for a simple range formatter that accepts no specifiers might look like this (note that this paper is proposing something more complicated than this, this is just an example):

::: bq
```cpp
template <typename V>
struct range_formatter {
    std::formatter<V> underlying;

    template <typename ParseContext>
    constexpr auto parse(ParseContext& ctx) {
        // ensure that the format specifier is empty
        if (ctx.begin() != ctx.end() && *ctx.begin() != '}') {
            throw std::format_error("invalid format");
        }

        // ensure that the underlying type can parse an empty specifier
        auto out = underlying.parse(ctx);

        // conditionally format as debug, if the type supports it
        if constexpr (requires { underlying.set_debug_format(); }) {
            underlying.set_debug_format();
        }
        return out;
    }

    template <typename R, typename FormatContext>
        requires std::same_as<std::remove_cvref_t<std::ranges::range_reference_t<R>>, V>
    constexpr auto format(R&& r, FormatContext& ctx) {
        auto out = ctx.out();
        *out++ = '[';
        auto first = std::ranges::begin(r);
        auto last = std::ranges::end(r);
        if (first != last) {
            // have to format every element via the underlying formatter
            ctx.advance_to(std::move(out));
            out = underlying.format(*first, ctx);
            for (++first; first != last; ++first) {
                *out++ = ',';
                *out++ = ' ';
                ctx.advance_to(std::move(out));
                out = underlying.format(*first, ctx);
            }
        }
        *out++ = ']';
        return out;
    }
};
```
:::

#### Range specifiers

Range format specifiers come in two kinds: specifiers for the range itself and specifiers for the underlying elements of the range. They must be provided in order: the range specifiers (optionally), then if desired, a colon and then the underlying specifier (optionally). For instance:

|specifier|meaning|
|-|----|
|`{}`{.x}|No specifiers|
|`{:}`{.x}|No specifiers|
|`{:<10}`|The whole range formatting is left-aligned, with a width of 10|
|`{:*^20}`|The whole range formatting is center-aligned, with a width of 20, padded with `*`s|
|`{:m}`{.x}|Apply the `m` specifier to the range (which must be a range of pair or 2-tuple)|
|`{::d}`{.x}|Apply the `d` specifier to each element of the range|
|`{:?s}`{.x}|Apply the `?s` specifier to the range (which must be a range of char)|

There are only a few top-level range-specific specifiers proposed:

* `s`: for ranges of char, only: formats the range as a string.
* `?s` for ranges of char, only: same as `s` except will additionally quote and escape the string
* `m`: for ranges of `pair`s (or `tuple`s of size 2) will format as `{k1: v1, k2: v2}` instead of `[(k1, v1), (k2, v2)]` (i.e. as a `map`).
* `e`: will format without the `[]`s. This will let you, for instance, format a range as `a, b, c` or `{a, b, c}` or `(a, b, c)` or however else you want, simply by providing the desired format string.

Additionally, ranges will support the same fill/align/width specifiers as in _std-format-spec_, for convenience and consistency.

If no element-specific formatter is provided (i.e. there is no inner colon - an empty element-specific formatter is still an element-specific formatter), the range will be formatted as debug. Otherwise, the element-specific formatter will be parsed and used.

To revisit a few rows from the earlier table:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`['H', '\t', 'l', 'l', 'o']`{.x}|
|`{::}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[H,     , l, l, o]`{.x}|
|`{::?c}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`['H', '\t', 'l', 'l', 'o']`{.x}|
|`{::d}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[72, 9, 108, 108, 111]`{.x}|
|`{::#x}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`[0x48, 0x9, 0x6c, 0x6c, 0x6f]`{.x}|
|`{:s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`H    llo`{.x}|
|`{:?s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`"H\tllo"`{.x}|
|`{}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[['a'], ['b', 'c']]`{.x}|
|`{::?s}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`["a", "bc"]`{.x}|
|`{:::d}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[[97], [98, 99]]`{.x}|

The second row is not printed quoted, because an empty element specifier is provided. We assume that if the user explicitly provides a format specifier (even if it's empty), that they want control over what they're doing. The third row is printed quoted again because it was explicitly asked for using the `?c` specifier, applied to each character.

The last row, `:::d`, is parsed as:

||top level outer vector||top level inner vector||inner vector each element|
|-|-|-|-|-|-|
|`:`|(none)|`:`|(none)|`:`|`d`|

That is, the `d` format specifier is applied to each underlying `char`, which causes them to be printed as integers instead of characters.

Note that you can provide both a fill/align/width specifier to the range itself as well as to each element:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`vector<int>{1, 2, 3}`|`[1, 2, 3]`{.x}|
|`{::*^5}`{.x}|`vector<int>{1, 2, 3}`|`[**1**, **2**, **3**]`{.x}|
|`{:o^17}`{.x}|`vector<int>{1, 2, 3}`|`oooo[1, 2, 3]oooo`{.x}|
|`{:o^29:*^5}`{.x}|`vector<int>{1, 2, 3}`|`oooo[**1**, **2**, **3**]oooo`{.x}|

#### Dynamic Delimiter for Ranges

Let's say I have a `vector<uint8_t>` that I wish to format as a MAC address. That is, I want to print every element with `"02x"`, delimited with `":"` (rather than the default `", "`), and without the surrounding square brackets.

I showed an example of how to do this earlier using `{fmt}`:

::: bq
```cpp
std::vector<uint8_t> mac = {0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
fmt::print("{}\n", mac);                     // [170, 187, 204, 221, 238, 255]
fmt::print("{:02x}\n", fmt::join(mac, ":")); // aa:bb:cc:dd:ee:ff
```
:::

However, if we're going to add more support for adding specifiers to ranges, that suggests a potential alternate avenue. We could add a dynamic delimiter in the same way that we support dynamic width in other contexts. That is:

::: bq
```cpp
fmt::print("{:ed{}:02x}", mac, ":"); // aa:bb:cc:dd:ee:ff
```
:::

Here, `":ed{}"` are the specifiers for the top-level vector, and then `":02x"` are the specifiers for the underlying element. The `e` specifier (for empty brackets) avoids printing the `[]`s and then the `d` specifier (for delimiter) is followed by which argument to get the delimiter out of (`{}` for auto-numbering, could also have been `{1}` in this example).

Perhaps `e` is implicit with `d`, perhaps not.

The question is, there are ultimately two ways that we could format this mac address as a result of this paper:

::: bq
```cpp
fmt::print("{:02x}\n", fmt::join(mac, ":")); // aa:bb:cc:dd:ee:ff
fmt::print("{:ed{}:02x}", mac, ":");         // aa:bb:cc:dd:ee:ff
```
:::

Do we want to pursue:

1. Just `fmt::join`?
2. Just dynamic delimiter?
3. Both?

The dynamic delimiter approach is more cryptic. The `join` approach arguably has the advantage of making it more clear what the delimiter is and how it's used, whereas in the dynamic delimiter approach it's just... wherever. Note that I'm not proposing any way of adding a static delimiter for the same reason that this paper is no longer proposing custom pair/tuple specifiers (see the [pair/tuple section](#pair-and-tuple-specifiers)).

But the dynamic delimiter approach does have the advantage that it supports more functionality. If I want to center-align the mac address and pad it with asterisks like I've been doing with every other example (for instance), that's just more specifiers as compared with another call to `format`:

::: bq
```cpp
fmt::print("{:*^23ed{}:02x}", mac, ":");                              // ***aa:bb:cc:dd:ee:ff***
fmt::print("{:*^23}\n", fmt::format("{:02x}", fmt::join(mac, ":")));  // ***aa:bb:cc:dd:ee:ff***
```
:::

And the other advantage is that it's one less thing to have to specify. And part of the problem there is what to name `fmt::join`? This paper has been using the name `std::format_join`. Is this one of those cases that Bjarne likes to point out as people want more syntax because it's simply novel, or is this one of those cases where the terser syntax is just inscrutable and unnecessary?

To be honest, I'm not sure what the right answer is here.

#### Pair and Tuple Specifiers

This is the hard part.

To start with, we for consistency will support the same fill/align/width specifiers as usual.

For ranges, we can have the underlying element's `formatter` simply parse the whole format specifier string from the character past the `:` to the `}`. The range doesn't care anymore at that point, and what we're left with is a specifier that the underlying element should understand (or not).

But for `pair`, it's not so easy, because format strings can contain _anything_. Absolutely anything. So when trying to parse a format specifier for a `pair<X, Y>`, how do you know where `X`'s format specifier ends and `Y`'s format specifier begins? This is, in general, impossible.

In [@P2286R3], this paper used Tim's insight to take a page out of `sed`'s book and rely on the user providing the specifier string to actually know what they're doing, and thus provide their own delimiter. `pair` will recognize the first character that is not one of its formatters as the delimiter, and then delimit based on that. This previous revision had proposed the following:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`pair(10, 1729)`|`(10, 1729)`{.x}|
|`{:}`{.x}|`pair(10, 1729)`|`(10, 1729)`{.x}|
|`{::#x:04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|
|`{:|#x|04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|
|`{:Y#xY04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|

The last three rows are equivalent, the difference is which character is used to delimit the specifiers: `:` or `|` or `Y`.

This approach, while technically functional, still leaves something to be desired. For one thing, these examples are already difficult to read and I haven't even shown any additional nesting. We're using to nested parentheses, brackets, or braces, but there's nothing visually nested here. And it's not even clear how to do something like that anyway. Several people expressed a desire to have a delimiter language that at least has some concept of nesting built-in - such as naturally-nesting punctuation like`()`s, `[]`s, or `{}s` (Unicode has plenty of other pairs of open/close characters. I could revisit my Russian roots with `«` and `»`, or use something prettier like `⦕` and `⦖`).

The point, ultimately, is that it is difficult to comme up with a format specifier syntax that works _at all_ in the presence of types that can use arbitrary characters in their specifiers. Like formatting `std::chrono::system_clock::now()`:

|Format String|Formatted Output|
|-|----|
|`{}`|`2021-10-24 20:33:37`{.x}|
|`{:%Y-%m-%d}`|`2021-10-24`{.x}|
|`{:%H:%M:%S}`|`20:33:37`{.x}|
|`{:%H hours, %M minutes, %S seconds}`|`20 hours, 33 minutes, 37 seconds`{.x}|

Because there is reasonable concern about the complexity of the initially proposed solution, and because there doesn't seem to be a lot of demand for actually being able to do this, in contrast to the very clear and present demand of being able to format pairs and tuples simply by default - this revision of this paper is withdrawing this part of the proposal in an effort to get the rest of the paper in for C++23.

To summarize: `std::pair` and `std::tuple` will only support:

* the fill/align/width specifiers from _std-format-spec_
* the `?` specifier, to format as debug (which is a no-op, since it will always format as debug, since there is no opt-out provided)
* the `m` specifier, only valid for `pair` or 2-tuple, to format as `k: v` instead of `(k, v)`


It will additionally provide the function:

::: bq
```cpp
void set_debug_format();
```
:::

and `pair` and `tuple` will provide the function:

::: bq
```cpp
void set_map_format();
```
:::

which for `tuple` of size other than 2 will throw an exception (since you cannot format those as a map). To clarify the map specifier:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`|`pair(1, 2)`|`(1, 2)`|
|`{:m}`|`pair(1, 2)`|`1: 2`|
|`{:m}`|`tuple(1, 2)`|`1: 2`|
|`{}`|`tuple(1)`|`(1)`|
|`{:m}`|`tuple(1)`|exception or compile error|
|`{}`|`tuple(1, 2, "3"s)`|`(1, 2, "3")`|
|`{:m}`|`tuple(1, 2, "3"s)`|exception or compile error|


## Implementation Challenges

I implemented the range and pair/tuple portions of this proposal on top of libfmt. I chose to do it on top so that I can easily share the implementation [@fmt-impl], as such I could not implement `?` support for strings and char, though that is not a very interesting part of this proposal (at least as far as implementability is concerned). There were two big issues that I ran into that are worth covering.

### Wrapping `basic_format_context` is not generally possible

In order to be able to provide an arbitrary type's specifiers to format a range, you have to have a `formatter<V>` for the underlying type and use that specific `formatter` in order to `parse` the format specifier and then `format` into the given context. If that's all you're doing, this isn't that big a deal, and I showed a simplified implementation of `range_formatter<V>` [earlier](#the-debug-specifier).

However, if you additionally want to support fill/pad/align, then the game changes. You can't format into the provided context - you have to format into _something else_ first and then do the adjustments later. Adding padding support ends up doing something more like this:

::: cmptable
### No padding
```cpp
template <typename R, typename FormatContext>
constexpr auto format(R&& r, FormatContext& ctx) {
    auto out = ctx.out();
    *out++ = '[';
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first != last) {
        ctx.advance_to(std::move(out));
        out = underlying.format(*first, ctx);
        for (++first; first != last; ++first) {
            *out++ = ',';
            *out++ = ' ';
            ctx.advance_to(std::move(out));
            out = underlying.format(*first, ctx);
        }
    }
    *out++ = ']';
    return out;
}
```

### With padding
```cpp
template <typename R, typename FormatContext>
constexpr auto format(R&& r, FormatContext& ctx) {
    // fmt has a dynamically growing buffer: memory_buffer
    // and a type-erased iterator into it: appender
    fmt::memory_buffer buf;
    fmt::basic_format_context<fmt::appender, char>
      bctx(fmt::appender(buf), ctx.args(), ctx.locale());

    auto out = bctx.out();
    *out++ = '[';
    auto first = std::ranges::begin(r);
    auto last = std::ranges::end(r);
    if (first != last) {
        bctx.advance_to(std::move(out));
        out = underlying.format(*first, bctx);
        for (++first; first != last; ++first) {
            *out++ = ',';
            *out++ = ' ';
            bctx.advance_to(std::move(out));
            out = underlying.format(*first, bctx);
        }
    }
    *out++ = ']';

    // at this point, we formatted our range into buf, so
    // now we need to format buf into the *real* context,
    // ctx.out(), with fill/pad/align. That part isn't
    // interesting for our purposes here
    return $write-padded-aligned$(ctx.out(), buf);
}
```
:::

It's mostly the same - we format into `bctx` instead of `ctx` and then `write` into `ctx` later using the `specs` that we already parsed. The code seems straightforward enough, except...

First, we don't even expose a way to construct `basic_format_context` so can't do this at all (there's no specified constructor for it in [format.context]{.sref}). Nor do we expose a way of constructing an iterator type for formatting into some buffer. And if we could construct these things, the real problem hits when we try to construct this new context. We need some kind of `fmt::basic_format_context<???, char>`, and we need to write into some kind of dynamic buffer, so `fmt::appender` is the appropriate choice for iterator. But the issue here is that `fmt::basic_format_context<Out, CharT>` has a member `fmt::basic_format_args<basic_format_context>` - the underlying arguments are templates _on the context_. We can't just... change the `basic_format_args` to have a different context, this is a fairly fundamental attachment in the design.

The _only_ type for the output iterator that I can support in this implementation is precisely `fmt::appender`.

This seems like it'd be _extremely_ limiting.

Except it turns out that `{fmt}` uses exactly this iterator in a whole lot of places. `fmt::print`, `fmt::format`, `fmt::format_to`, `fmt::format_to_n`, `fmt::vformat`, etc., all only use this one iterator type. This is because of [@P2216R3]'s efforts to reduce code bloat by type erasing the output iterator.

However, there is one part of `{fmt}` that uses a different iterator type, which the above implementation fails on:

::: bq
```cpp
fmt::format("{:::d}", vector{vector{'a'}, vector{'b', 'c'}});              // ok: [[97], [98, 99]]
fmt::format(FMT_COMPILE("{:::d}"), vector{vector{'a'}, vector{'b', 'c'}}); // ill-formed
```
:::

The latter fails because there the initial output iterator type is `std::back_insert_iterator<std::string>`. This is a different iterator type from `fmt::appender`, so we get a mismatch in the types of the `basic_format_args` specializations, and cannot compile the construction of `bctx`.

This can be worked around (I just need to know what the type of the buffer needs to be, in the usual case it's `fmt::memory_buffer` and here it becomes `std::string`, that's fine), but it means we really need to nail down what the requirements of the `formatter` API are. One of the things we need to do in this paper is provide a `formattable` concept. From a previous revision of that paper, dropping the `char` parameter for simplicity, that looks like:

::: bq
```cpp
template <class T>
concept $formattable-impl$ =
    std::semiregular<fmt::formatter<T>> &&
    requires (fmt::formatter<T> f,
              const T t,
              fmt::basic_format_context<char*, char> fc,
              fmt::basic_format_parse_context<char> pc)
    {
        { f.parse(pc) } -> std::same_as<fmt::basic_format_parse_context<char>::iterator>;
        { f.format(t, fc) } -> std::same_as<char*>;
    };

template <class T>
concept formattable = $formattable-impl$<std::remove_cvref_t<T>>;
```
:::

I use `char*` as the output iterator, but my `range_formatter<V>` cannot support `char*` as an output iterator type at all. Do `formatter` specializations need to support any output iterator type? If so, how can we implement fill/align/pad support in `range_formatter`?

The simplest approach would be to state that there actually is only one output iterator type that need be support per character type. But this would prohibit the approach `{fmt}` uses to process the format string at compile time, as well as any potential future optimizations. This just seems like a non-starter.

A different approach would be to introduce a new API that allows the implementation to produce a new context for us. That approach could look like this:

::: bq
```cpp
template <typename V, typename FormatContext>
constexpr auto format(V&& value, FormatContext& ctx) -> typename FormatContext::iterator
{
    // ctx here is a basic_format_context<OutIt, CharT>, for some output iterator
    // and some character type

    // can use a vector<CharT>, basic_string<CharT>, or some custom buffer like
    // fmt::buffer, user's choice
    vector<CharT> buf;

    // The job of the retargeted_format_context class template is to produce
    // a new specialization of basic_format_context for the provided iterator
    // that simply does The Right Thing (TM).
    // We do not need bctx here to be specifically (w)format_context, just some
    // specialization of basic_format_context that is definitely going to write
    // into buf (regardless of buf's type).
    retargeted_format_context rctx(ctx, std::back_inserter(buf));
    auto& bctx = rctx.context();

    // format into bctx...
}
```
:::

There is one fundamental limitation here that is sort of inherent in the design. If the user-defined types want to reference some other argument (i.e. something like dynamic width or dynamic precision) but what that other argument to _also_ be a user-defined type (rather than just an integer or `string_view`/`char const*`), they basically cannot. User-defined types are type erased as `handle` (see [format.arg]{.sref}), and `handle` can only be formatted with a `(w)format_parse_context` - which only the implementation would have access to.

However, if we ignore user-defined types entirely, it is straightforward to convert all the other `format_arg`s from one context to another, since we know everything about all of those types and they are all cheap to copy.

The implementation approach I used is as follows:

::: bq
```cpp
// effectively a tagged version of fmt::appender, solely for
// specializing on top of
template <typename Old>
struct custom_appender : appender {
    using appender::appender;
};

// specialization of basic_format_args for use with custom_appender
// This ended up being easier than specializing basic_format_context.
// This specialization is only used in the context of retargeted_format_context.
//
// Note that here we hold a reference to the original basic_format_args: we don't
// have to make a copy, and we only produce a new basic_format_arg if actually
// requiried by the formatting. This means we don't have to pay for anything that
// we don't use
template <typename Old>
struct basic_format_args<basic_format_context<custom_appender<Old>, char>> {
    using old_args = basic_format_args<basic_format_context<Old, char>>;
    using new_context = basic_format_context<custom_appender<Old>, char>;
    using format_arg = basic_format_arg<new_context>;
    using size_type = int;

    old_args const& orig_args;

    basic_format_args(old_args const& orig)
        : orig_args(orig)
    { }

    constexpr auto get(int id) const -> format_arg {
        return visit_format_arg([]<typename T>(T const& arg) -> format_arg {
            // I'm not sure when this is ever monostate, but for any user-defined
            // types (i.e. handle), we can't produce a valid format_arg for them
            // so we insetad return no format arg
            if constexpr (std::same_as<T, typename old_args::format_arg::handle>
                        or std::same_as<T, monostate>) {
                return format_arg();
            } else {
                // ... but for all the other types, this is a cheap copy
                // T is bool, char, some integral type, some floating point
                // type, char const*, string_view, or void const*
                return detail::make_arg<new_context>(arg);
            }
        }, orig_args.get(id));
    }

    // These next two functions are {fmt}-specific, since std:: doesn't
    // have argument names. But if it did, as you can see, these calls are
    // pretty straightforward
    constexpr auto get(fmt::string_view name) const -> format_arg {
        int id = orig_args.get_id(name);
        return id >= 0 ? get(id) : format_arg();
    }

    constexpr auto get_id(fmt::string_view name) const -> int {
        return orig_args.get_id(name);
    }
};

// In the case where we do need to retarget, we build a new context using
// custom_appender<Context::iterator>, which will use the specialization of
// basic_format_args defined above (no new basic_format_args are created)
template <typename Context, typename OutputIt>
struct retargeted_format_context {
    detail::iterator_buffer<OutputIt, char> buffer;

    using iterator = custom_appender<typename Context::iterator>;
    using new_context = basic_format_context<iterator, char>;
    new_context erased_ctx;

    retargeted_format_context(Context& ctx, OutputIt it)
        : buffer(it)
        , erased_ctx(iterator(buffer),
                        basic_format_args<new_context>(ctx.args()),
                        ctx.locale())
    { }

    auto context() -> new_context& {
        return erased_ctx;
    }

    // in fmt, this iterator is buffered, so we need to flush it
    void flush() {
        (void)buffer.out();
    }
};

// In the "happy" case (i.e. we're just using fmt::print), we don't need to do
// any of this, the args are already the correct type so copying them is fine.
// All we need to do is create a new context
template <typename CharT, typename OutputIt>
struct retargeted_format_context<basic_format_context<OutputIt, CharT>, OutputIt>
{
    basic_format_context<OutputIt, CharT> ctx;

    retargeted_format_context(basic_format_context<OutputIt, CharT>& ctx, OutputIt it)
        : ctx(it, ctx.args(), ctx.locale())
    { }

    auto context() -> basic_format_context<OutputIt, CharT>& { return ctx; }

    void flush() { }
};
```
:::

You can see this in the implementation I shared [@fmt-impl], on lines 65-140.

We don't strictly need to provide `retargeted_format_context` just to format ranges (the implementation would do something like this internally). But if users want to be able to solve this problem (e.g. fill/pad/align for a user-defined type where all you have is `formatter<T>` for unknown `T`) for any of their own types, they'll need to do something like this as well, so this functionality should be provided to let them do that.

### Manipulating `basic_format_parse_context` to search for sentinels

Even though this paper is no longer proposing complex `pair` and `tuple` support, it's still useful to discuss one of the examples that could have been supported:

::: bq
```cpp
fmt::format("{:|#x|*^10}", std::pair(42, "hello"s));
```
:::

In order for this to work, the `formatter<int>` object needs to be passed a context that just contains the string `"#x"` and the `formatter<string>` object needs to be passed a context that just contains the string `"*^10"` (or possibly `"*^10}"`). This is because `formatter<T>::parse` must consume the whole context. That's the API.

But `basic_format_parse_context` does not provide a way for you to take a slice of it, and we can't just construct a new object because of the dynamic argument counting support. Not just _any_ context, but _specifically that one_.

Tim's suggested design for how to even do specifiers for `pair` also came with a suggested implementation: use a `sentry`-like type that temporarily modifies the context and restores it later. The use of this type looks like this:

::: bq
```cpp
auto const delim = *begin++;
ctx.advance_to(begin);
tuple_for_each_index(underlying, [&](auto I, auto& f){
    auto next_delim = std::find(ctx.begin(), end, delim);
    if constexpr (I + 1 < sizeof...(Ts)) {
        if (next_delim == end) {
            throw fmt::format_error("ran out of specifiers");
        }
    }

    end_sentry _(ctx, next_delim);
    auto i = f.parse(ctx);
    if (i != next_delim && *i != '}') {
        throw fmt::format_error("this is broken");
    }

    if (next_delim != end) {
        ++i;
    }
    ctx.advance_to(i);
});
```
:::

This ensures that each element of the `pair`/`tuple` only sees its part of the whole parse string, which is the only part that it knows what to do anything with.

Without something like this in the library, it'd be impossible to do this sort of complex specifier parsing. You could support ranges (there, we only have one underlying element, so it parses to the end), but not pair or tuple. We _could_ say that since pair and tuple are library types, the library should just Make This Work, but there are surely other examples of wanting to do this sort of thing and it doesn't feel right to not allow users to do it too.

As with `retargeted_format_context`, if we adopted the `pair`/`tuple` specifiers design, we wouldn't have to expose something like this in the standard library. The implementation would need to do it internally and it could do whatever it needs to do to get it done. But it's still useful functionality to be able to export to users. And especially if we're not going to adopt arbitrary pair/tuple specifiers, I think it's important to give users the tools to experiment with them.

This design space is, thankfully, slightly easier than the previous problem: this is basically what you have to do. Not much choice, I don't think.

### Parsing of alignment, padding, and width

The first two issues in this section are serious implementation issues that require design changes to `<format>`. This one doesn't *require* changes, and this paper won't propose changes, but it's worth pointing out nevertheless. Alignment, padding, and width are the most common and fairly universal specifiers. But we don't provide a public API to actually parse them.

When implementing this in `fmt`, I just took advantage of `fmt`'s implementation details to make this a lot easier for myself: a type (`dynamic_format_specs<char>`) that holds all the specifier results, a function that understands those to let you write a padded/aligned string (`write`), and several parsing functions that are well designed to do the right thing if you have a unique set of specifiers you wish to parse (the appropriately-named `parse_align` and `parse_width`).

These don't have to be standardized, as nothing in these functions is something that a user couldn't write on their own. And this paper is big enough already, so it, again, won't propose anything in this space. But it's worth considering for the future.

## How to support those views which are not `const`-iterable?

In a previous revision of this paper, this was a real problem since at the time `std::format` accepted its arguments by `const Args&...`

However, [@P2418R2] was speedily adopted specifically to address this issue, and now `std::format` accepts its arguments by `Args&&...` This allows those views which are not `const`-iterable to be mutably passed into `format()` and `print()` and then mutably into its formatter. To support both `const` and non-`const` formatting of ranges without too much boilerplate, we can do it this way:

::: bq
```cpp
template <formattable V>
struct range_formatter {
    template <typename ParseContext>
    constexpr auto parse(ParseContext&);

    template <range R, typename FormatContext>
        requires same_as<remove_cvref_t<range_reference_t<R>>, V>
    constexpr auto format(R&&, FormatContext&);
};

template <range R> requires formattable<range_reference_t<R>>
struct formatter<R> : range_formatter<range_reference_t<R>>
{ };
```
:::

`range_formatter` allows reducing unnecessary template instantiations. Any range of `int` is going to `parse` its specifiers the same way, there's no need to re-instantiate that code n times. Such a type will also help users to write their own formatters.

## What additional functionality?

There’s three layers of potential functionality:

1. Top-level printing of ranges: this is `fmt::print("{}", r)`;

2. A format-joiner which allows providing a a custom delimiter: this is `fmt::print("{:02x}", fmt::join(r, ":"))`. This revision of the paper allows providing a format specifier and removed in the brackets in the top-level case too, as in `fmt::print("{:e:02x}", r)`, but does not allow for providing a custom delimiter.

3. A more involved version of a format-joiner which takes a delimiter and a callback that gets invoked on each element. fmt does not provide such a mechanism, though the Rust itertools library does:

::: bq
```rust
let matrix = [[1., 2., 3.],
              [4., 5., 6.]];
let matrix_formatter = matrix.iter().format_with("\n", |row, f| {
                                f(&row.iter().format_with(", ", |elt, g| g(&elt)))
                              });
assert_eq!(format!("{}", matrix_formatter),
           "1, 2, 3\n4, 5, 6");
```
:::

This paper suggests the first two and encourages research into the third.

## `format` or `std::cout`?

Just `format` is sufficient.

## What about `vector<bool>`?

Nobody expected this section.

The `value_type` of this range is `bool`, which is formattable. But the `reference` type of this range is `vector<bool>::reference`, which is not. In order to make the whole type formattable, we can either make `vector<bool>::reference` formattable (and thus, in general, a range is formattable if its `reference` types is formattable) or allow formatting to fall back to constructing a `value_type` for each `reference` (and thus, in general, a range is formattable if either its `reference` type or its `value_type` is formattable).

For most ranges, the `value_type` is `remove_cvref_t<reference>`, so there’s no distinction here between the two options. And even for `zip` [@P2321R2], there’s still not much distinction since it just wraps this question in tuple since again for most ranges the types will be something like `tuple<T, U>` vs `tuple<T&, U const&>`, so again there isn’t much distinction.

`vector<bool>` is one of the very few ranges in which the two types are truly quite different. So it doesn’t offer much in the way of a good example here, since `bool` is cheaply constructible from `vector<bool>::reference`. Though it’s also very cheap to provide a formatter specialization for `vector<bool>::reference`.

Rather than having the library provide a default fallback that lifts all the `reference` types to `value_type`s, which may be arbitrarily expensive for unknown ranges, this paper proposes a format specialization for `vector<bool>::reference`. Or, rather, since it’s actually defined as `vector<bool, Alloc>::reference`, this isn’t necessarily feasible, so instead this paper proposes a specialization for `vector<bool, Alloc>` at top level.

# Proposal

The standard library will provide the following utilities:

* A `formattable` concept.
* A `range_formatter<V>` that uses a `formatter<V>` to `parse` and `format` a range whose `reference` is similar to `V`. This can accept a specifier on the range (align/pad/width as well as string/map/debug/empty) and on the underlying element (which will be applied to every element in the range).
* A `tuple_formatter<Ts...>` that uses a `formatter<T>` for each `T` in `Ts...` to `parse` and `format` either a `pair`, `tuple`, or `array` with appropriate elements. This can accepted a specifier on the tuple-like (align/pad/width) as well as a specifier for each underlying element (with a custom delimiter).
* A `retargeted_format_context` facility that allows the user to construct a new `(w)format_context` with a custom output iterator.
* An `end_sentry` facility that allows the user to manipulate the parse context's range, for generic parsing purposes.

The standard library should add specializations of `formatter` for:

* any type `R` that is a `range` whose `reference` is `formattable`, which inherits from `range_formatter<remove_cvref_t<ranges::range_reference_t<R>>>`
* `pair<T, U>` if `T` and `U` are `formattable`, which inherits from `tuple_formatter<remove_cvref_t<T>, remove_cvref_t<U>>`
* `tuple<Ts...>` if all of `Ts...` are `formattable`, which inherits from `tuple_formatter<remove_cvref_t<Ts>...>`

Additionally, the standard library should provide the following more specific specializations of `formatter`:

* `vector<bool, Alloc>` (which formats as a range of `bool`)
* all the associative maps (`map`, `multimap`, `unordered_map`, `unordered_multimap`) if their respective key/value types are `formattable`. This accepts the same set of specifiers as any other range, except by _default_ it will format as `{k: v, k: v}` instead of `[(k, v), (k, v)]`
* all the associative sets (`sets`, `multiset`, `unordered_set`, `unordered_multiset`) if their respective key/value types are `formattable`. This accepts the same set of specifiers as any other range, except by _default_ it will format as `{v1, v2}` instead of `[v1, v2]`

Formatting for `string`, `string_view`, and `char`/`wchar_t` will gain a `?` specifier, which causes these types to be printed as escaped and quoted if provided. Ranges and tuples will, by default, print their elements as escaped and quoted, unless the user provides a specifier for the element.

The standard library should also add a utility `std::format_join` (or any other suitable name, knowing that `std::views::join` already exists), following in the footsteps of `fmt::join`, which allows the user to provide more customization in how ranges and tuples get formatted. Even though this paper allows you to provide a specifier for each element in the range, it does not let you change the delimiter in the specifier (that's... a bit much), so `fmt::join` is still a useful and necessary facility for that.

## Wording

None yet, since spent all my time on implementation but nevertheless wanted to get this paper out sooner.

---
references:
    - id: P2286R2
      citation-label: P2286R2
      title: "Formatting Ranges"
      author:
        - family: Barry Revzin
      issued:
        - year: 2021
      URL: https://wg21.link/p2286r2
    - id: P2418R0
      citation-label: P2418R0
      title: "Add support for `std::generator`-like types to `std::format`"
      author:
        - family: Victor Zverovich
      issued:
        - year: 2021
      URL: https://wg21.link/p2418r0
    - id: fmt-impl
      citation-label: fmt-impl
      title: "Implementation for range formatting on top of `{fmt}`"
      author:
        - family: Barry Revzin
      issued:
        - year: 2021
      URL: https://godbolt.org/z/Kf5G5e8xc
---
