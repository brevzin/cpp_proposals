---
title: "Formatting Ranges"
document: P2286R3
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

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
    fmt::print("[{}]\n", fmt::join(parts, ","));
}
```
outputting
```
{@{}@, {'y'}}
[{},{'y'}]
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

The Ranges Plan for C++23 [@P2214R0] listed as one of its top priorities for C++23 as the ability to format all views. Let's go through the issues we need to address in order to get this functionality.

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
|`{}`{.x}|`"hello"s`|`hello`{.x}|
|`{:?}`{.x}|`"hello"s`|`"hello"`{.x}|
|`{}`{.x}|`vector{"hello"s, "world"s}`|`["hello", "world"]`{.x}|
|`{:}`{.x}|`vector{"hello"s, "world"s}`|`["hello", "world"]`{.x}|
|`{:?}`{.x}|`vector{"hello"s, "world"s}`|`["hello", "world"]`{.x}|
|`{:*^14}`{.x}|`vector{"he"s, "wo"s}`|`*["he", "wo"]*`{.x}|
|`{::*^14}`{.x}|`vector{"he"s, "wo"s}`|`[******he******, ******wo******]`{.x}|
|`{:}`{.x}|`42`|`42`{.x}|
|`{:#x}`{.x}|`42`|`0x2a`{.x}|
|`{}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[H, e, l, l, o]`{.x}|
|`{::?c}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::d}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[72, 101, 108, 108, 111]`{.x}|
|`{::#x}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[0x48, 0x65, 0x6c, 0x6c, 0x6f]`{.x}|
|`{:s}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`Hello`{.x}|
|`{:?s}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`"Hello"`{.x}|
|`{}`{.x}|`pair{42, "hello"s}`|`(42, "hello")`{.x}|
|`{::#x:*^10}`{.x}|`pair{42, "hello"s}`|`(0x2a, **hello***)`{.x}|
|`{:|#x|*^10}`{.x}|`pair{42, "hello"s}`|`(0x2a, **hello***)`{.x}|
|`{}`{.x}|`vector{pair{42, "hello"s}}`|`[(42, "hello")]`{.x}|
|`{:m}`{.x}|`vector{pair{42, "hello"s}}`|`{42: "hello"}`{.x}|
|`{:m::#x:*^10}`{.x}|`vector{pair{42, "hello"s}}`|`{0x2a: **hello***}`{.x}|
|`{}`{.x}|`vector<{vector{'a'}, vector{'b', 'c'}}`|`[['a'], ['b', 'c']]`{.x}|
|`{::?s}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`["a", "bc"]`{.x}|
|`{:::d}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[[97], [98, 99]]`{.x}|
|`{}`|`pair(system_clock::now(), system_clock::now())`|`(2021-10-24 20:33:37, 2021-10-24 20:33:37)`{.x}|
|`{:|%Y-%m-%d|%H:%M:%S}`|`pair(system_clock::now(), system_clock::now())`|`(2021-10-24, 20:33:37)`{.x}|

### Explanation of Added Specifiers

#### The debug specifier `?`

`char` and `string` and `string_view` will start to support the `?` specifier. This will cause the character/string to be printed as quoted (characters with `'` and strings with `"`) and all characters to be escaped, as [described earlier](char-and-string-and-other-string-like-types-in-ranges-or-tuples).

This facility will be generated by the formatters for these types providing an addition member function (on top of `parse` and `format`):

::: bq
```cpp
void format_as_debug();
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
        if constexpr (requires { underlying.format_as_debug(); }) {
            underlying.format_as_debug();
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
|-|-|
|`{}`{.x}|No specifiers|
|`{:}`{.x}|No specifiers|
|`{:<10}`|The whole range formatting is left-aligned, with a width of 10|
|`{:*^20}`|The whole range formatting is center-aligned, with a width of 20, padded with `*`s|
|`{:m}`{.x}|Apply the `m` specifier to the range|
|`{::d}`{.x}|Apply the `d` specifier to each element of the range|
|`{:?s}`{.x}|Apply the `?s` specifier to the range|
|`{:m::#x:#x}`{.x}|Apply the `m` specifier to the range and the `:#x:#x`{.x} specifier to each element of the range|

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
|`{}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[H, e, l, l, o]`{.x}|
|`{::?c}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::d}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[72, 101, 108, 108, 111]`{.x}|
|`{::#x}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[0x48, 0x65, 0x6c, 0x6c, 0x6f]`{.x}|
|`{:s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`H    llo`{.x}|
|`{:?s}`{.x}|`vector<char>{'H', '\t', 'l', 'l', 'o'}`|`"H\tllo"`{.x}|
|`{}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[['a'], ['b', 'c']]`{.x}|
|`{::?s}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`["a", "bc"]`{.x}|
|`{:::d}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[[97], [98, 99]]`{.x}|

The second row is not printed quoted, because an empty element specifier is provided. The third row is printed quoted again because it was explicitly asked for using the `?c` specifier, applied to each character.

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


#### Pair and Tuple Specifiers

This is the hard part.

To start with, we for consistency will support the same fill/align/width specifiers as usual.

But for ranges, we can have the underlying element's `formatter` simply parse the whole format specifier string from the character past the `:` to the `}`. The range doesn't care anymore at that point, and what we're left with is a specifier that the underlying element should understand (or not).

For `pair`, it's not so easy, because format strings can contain _anything_. Absolutely anything. So when trying to parse a format specifier for a `pair<X, Y>`, how do you know where `X`'s format specifier ends and `Y`'s format specifier begins? This is, in general, impossible.

But Tim's insight was to take a page out of `sed`'s book and rely on the user providing the specifier string to actually know what they're doing, and thus provide their own delimiter. `pair` will recognize the first character that is not one of its formatters as the delimiter, and then delimit based on that.

Let's start with some easy examples:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`pair(10, 1729)`|`(10, 1729)`{.x}|
|`{:}`{.x}|`pair(10, 1729)`|`(10, 1729)`{.x}|
|`{::#x:04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|
|`{:|#x|04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|
|`{:Y#xY04X}`{.x}|`pair(10, 1729)`|`(0xa, 06C1)`{.x}|

In the first two rows, there are no specifiers for the underlying elements. The last three rows each provide the same specifiers, but use a different delimiter:

||pair specifier|delimiter|`first` specifier|delimiter|`second` specifier|
|-|-|-|-|-|-|
|`:`|(none)|`:`|`#x`{.x}|`:`|`04X`{.x}|
|`:`|(none)|`|`|`#x`{.x}|`|`|`04X`{.x}|
|`:`|(none)|`Y`|`#x`{.x}|`Y`|`04X`{.x}|

If you provide the `first` specifier, you must provide all the specifiers. In other words, `::#x`{.x} would be an invalid format specifier for a `pair<int, int>`.

To demonstrate why such a scheme is necessary, and simply using `:` as a delimiter is insufficient, consider chrono formatters. Chrono format strings allow anything, including `:`. Consider trying to format `std::chrono::system_clock::now()` using various specifiers:

|Format String|Formatted Output|
|-|----|
|`{}`|`2021-10-24 20:33:37`{.x}|
|`{:%Y-%m-%d}`|`2021-10-24`{.x}|
|`{:%H:%M:%S}`|`20:33:37`{.x}|
|`{:%H hours, %M minutes, %S seconds}`|`20 hours, 33 minutes, 37 seconds`{.x}|

How could `pair` _possibly_ know when to stop parsing `first`'s specifier given... that? It can't. But if allow an arbitrary choice of delimiter, the user can pick one that won't interfere:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`pair(now(), 1729)`|`(2021-10-24 20:33:37, 1729)`{.x}|
|`{:m|%Y-%m-%d|#x}`{.x}|`pair(now(), 1729)`|`2021-10-24: 0x6c1`{.x}|

Which is parsed as:

||pair specifier|delimiter|`first` specifier|delimiter|`second` specifier|
|-|-|-|-|-|-|
|`:`|`m`|`|`|`%Y-%m-%d`|`|`|`#x`{.x}|

The above also introduces the only top-level specifier for `pair`: `m`. As with Ranges described in the previous section (and, indeed, necessary to support the Ranges functionality described there), the `m` specifier formatters pairs and 2-tuples as associations (i.e. `k: v`) instead of as a pair/tuple (i.e. `(k, v)`):

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`|`pair(1, 2)`|`(1, 2)`{.x}|
|`{:m}`|`pair(1, 2)`|`1: 2`{.x}|
|`{:m}`|`tuple(1, 2)`|`1: 2`{.x}|
|`{}`|`tuple(1)`|`(1)`{.x}|
|`{:m}`|`tuple(1)`|ill-formed|
|`{}`|`tuple(1,2,3)`|`(1, 2, 3)`{.x}|
|`{:m}`|`tuple(1,2,3)`|ill-formed|

Similarly to how in the debug specifier is handled by introducing a:

::: bq
```cpp
void format_as_debug();
```
:::

function, `pair` and `tuple` will provide a:

::: bq
```cpp
void format_as_map();
```
:::

function, that for `tuple` of size other than 2 will throw an exception (since you cannot format those as a map).

Tuple and pair will also provide the same fill/align/width specifiers as other types, again for consistency and convenience.

## Implementation Challenges

I implemented the range and pair/tuple portions of this proposal on top of libfmt. I chose to do it on top so that I can easily [share the implementation](https://godbolt.org/z/o8nfvdYxM), as such I could not implement `?` support for strings and char, though that is not a very interesting part of this proposal (at least as far as implementability is concerned). There were two big issues that I ran into that are worth covering.

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
    fmt::memory_buffer buf;
    fmt::basic_format_context<fmt::appender, char>
      bctx(appender(buf), ctx.args(), ctx.locale());

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

    return fmt::write(ctx.out(),
      fmt::string_view(buf.data(), buf.size()),
      this->specs);
}
```
:::

It's mostly the same - we format into `bctx` instead of `ctx` and then `write` into `ctx` later using the `specs` that we already parsed. The problem comes up in constructing this new context. We need some kind of `fmt::basic_format_context<???, char>`, and we need to write into some kind of dynamic buffer, so `fmt::appender` is the appropriate choice for iterator. But the issue here is that `fmt::basic_format_context<Out, CharT>` has a member `fmt::basic_format_args<basic_format_context>` - the underlying arguments are templates _on the context_. We can't just... change the `basic_format_args` to have a different context, this is a fairly fundamental attachment in the design.

The _only_ type for the output iterator that I can support in this implementation is precisely `fmt::appender`.

This seems like it'd be extremely limiting. Except it runs out that actually nearly all of libfmt uses exactly this iterator. `fmt::print`, `fmt::format`, `fmt::format_to`, `fmt::format_to_n`, `fmt::vformat`, etc., all only use this one iterator type. This is because of [@P2216R3]'s efforts to reduce code bloat by type erasing the output iterator.

However, there is one part of libfmt that uses a different iterator type, which now the above implementation fails on:

::: bq
```cpp
fmt::format("{:::d}", vector{vector{'a'}, vector{'b', 'c'}});              // ok: [[97], [98, 99]]
fmt::format(FMT_COMPILE("{:::d}"), vector{vector{'a'}, vector{'b', 'c'}}); // ill-formed
```
:::

The latter fails because now the initial output iterator type is `std::back_insert_iterator<std::string>`, and the implementation fails to compile, erroring on the construction of `bctx` because of the mismatch in the types of the `basic_format_args` specializations.

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

The simplest approach would be to state that there actually is only one output iterator type that need be support per character type. This is mostly already the case in libfmt, and seems to be how MSVC implements `<format>` as well. That is, we already have in [format.syn]{.sref}:

::: bq
```cpp
namespace std {
  // [format.context], class template basic_­format_­context
  template<class Out, class charT> class basic_format_context;
  using format_context = basic_format_context<$unspecified$, char>;
  using wformat_context = basic_format_context<$unspecified$, wchar_t>;
}
```
:::

The suggestion would be that the only contexts that need be supported are `std::format_context` and/or `std::wformat_context`. Only one context for each character type.

That reduces the problem quite a bit, but it's still not enough. We're not exposing what the buffer type needs to be, so even if I knew I only had to deal with `std::format_context`, I still wouldn't know how to construct a dynamic buffer that `std::format_context::iterator` is an extending output iterator into. That is, we need to expose/standardize `fmt::memory_buffer` (or provide it as an typedef somewhere).

If we don't require _just_ one format context per character type, we can simply throw more type erasure at the problem. Say the only allowed iterators are either (using libfmt's names) `fmt::appender` or `variant<fmt::appender, Out>`. The latter still allows support for other iterator types, while still letting other formatters use `fmt::appender` which they know how to do. This has some cost of course, but it does provide extra flexibility.

### Manipulating `basic_format_parse_context` to search for sentinels

Take a look at one of the `pair` formatting examples:

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

This design space is, thankfully, slightly easier than the previous problem: this is basically what you have to do. Not much choice, I don't think.

### Parsing of alignment, padding, and width

The first two issues in this section are serious implementation issues that require design changes to `<format>`. This one doesn't *require* changes, and this paper won't propose changes, but it's worth pointing out nevertheless. Alignment, padding, and width are the most common and fairly universal specifiers. But we don't provide a public API to actually parse them.

When implementing this in `fmt`, I just took advantage of `fmt`'s implementation details to make this a lot easier for myself: a type (`dynamic_format_specs<char>`) that holds all the specifier results, a function that understands those to let you write a padded/aligned string (`write`), and several parsing functions that are well designed to do the right thing if you have a unique set of specifiers you wish to parse (the appropriately-named `parse_align` and `parse_width`).

These don't have to be standardized, as nothing in these functions is something that a user couldn't write on their own. And this paper is big enough already, so it, again, won't propose anything in this space. But it's worth considering for the future.

---
references:
    - id: P2286R2
      citation-label: P2286R2
      title: "Formatting Ranges"
      author:
        - family: Barry Revzin
      issued:
        -year: 2021
      URL: https://wg21.link/p2286r2
    - id: P2418R0
      citation-label: P2418R0
      title: "Add support for `std::generator`-like types to `std::format`"
      author:
        - family: Victor Zverovich
      issued:
        -year: 2021
      URL: https://wg21.link/p2418r0
---
