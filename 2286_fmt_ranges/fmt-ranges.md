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
|`{::*^10}`{.x}|`vector{"hello"s, "world"s}`|`[**hello***, **world***]`{.x}|
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
            out = underlying.format(*first, ctx));
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
|`{:m}`{.x}|Apply the `m` specifier to the range|
|`{::d}`{.x}|Apply the `d` specifier to each element of the range|
|`{:?s}`{.x}|Apply the `?s` specifier to the range|
|`{:m::#x:#x}`{.x}|Apply the `m` specifier to the range and the `:#x:#x`{.x} specifier to each element of the range|

There are only a few top-level range specifiers proposed:

* `s`: for ranges of char, only: formats the range as a string.
* `?s` for ranges of char, only: same as `s` except will additionally quote and escape the string
* `m`: for ranges of `pair`s (or `tuple`s of size 2) will format as `{k1: v1, k2: v2}` instead of `[(k1, v1), (k2, v2)]` (i.e. as a `map`). For other ranges, will format as `{a, b, c}` instead of `[a, b, c]` (i.e. as a `set`).

If no element-specific formatter is provided (i.e. there is no inner colon - an empty element-specific formatter is still an element-specific formatter), the range will be formatted as debug. Otherwise, the element-specific formatter will be parsed and used.

To revisit a few rows from the earlier table:

|Format String|Contents|Formatted Output|
|-|----|----|
|`{}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[H, e, l, l, o]`{.x}|
|`{::?c}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`['H', 'e', 'l', 'l', 'o']`{.x}|
|`{::d}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[72, 101, 108, 108, 111]`{.x}|
|`{::#x}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`[0x48, 0x65, 0x6c, 0x6c, 0x6f]`{.x}|
|`{:s}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`Hello`{.x}|
|`{:?s}`{.x}|`vector<char>{'H', 'e', 'l', 'l', 'o'}`|`"Hello"`{.x}|
|`{}`{.x}|`vector<{vector{'a'}, vector{'b', 'c'}}`|`[['a'], ['b', 'c']]`{.x}|
|`{::?s}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`["a", "bc"]`{.x}|
|`{:::d}`{.x}|`vector{vector{'a'}, vector{'b', 'c'}}`|`[[97], [98, 99]]`{.x}|

The second row is not printed quoted, because an empty element specifier is provided. The third row is printed quoted again because it was explicitly asked for using the `?c` specifier, applied to each character.

The last row, `:::d`, is parsed as:

||top level outer vector||top level inner vector||inner vector each element|
|-|-|-|-|-|-|
|`:`|(none)|`:`|(none)|`:`|`d`|

That is, the `d` format specifier is applied to each underlying `char`, which causes them to be printed as integers instead of characters.

#### Pair and Tuple Specifiers

This is the hard part.

For ranges, we can have the underlying element's `formatter` simply parse the whole format specifier string from the character past the `:` to the `}`. The range doesn't care anymore at that point, and what we're left with is a specifier that the underlying element should understand (or not).

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
|`{:m}`|`tuple(1)`|`1`{.x}|
|`{}`|`tuple(1,2,3)`|`(1, 2, 3)`{.x}|
|`{:m}`|`tuple(1,2,3)`|ill-formed|

### Customization

There are actually multiple questions when it comes to customization:

1. How do you opt in a user-defined string type to being formatted without delimiters (when printed normally) or being formatted as quoted (when as part of a range or tuple)?
2. How do you opt in a user-defined associated range to being printed as an association?
3. How do you choose to print a given range as either a quoted string or an associated container (whether opted into or not)?

These are all important questions, since even regardless of how we do customization, we need to give the user the tools to format their types the way they want, conveniently. For instance, a user might have a type like:

::: bq
```cpp
struct Foo {
    int bar;
    std::string baz;
};
```
:::

And want to format `Foo{.bar=10, .baz="Hello World"}` as the string `Foo(bar=10, baz="Hello World")`. This requires giving the user some way to choose to print the `baz` member as being quoted, rather than not. They need to have something to be able to write here:

::: bq
```cpp
template <>
struct formatter<Foo, char> {
    template <typename FormatContext>
    constexpr auto format(Foo const& f, FormatContext& ctx) {
        return format_to(ctx.out(), "Foo(bar={}, baz={})", f.bar, /* ???? */);
    }
};
```
:::

Extending this question out further: how do you format wrappers?

Let's say you have your own implementation of `Optional`, that you want to format the same way that Rust does: so that a disengaged one formats as `None` and an engaged one formats as `Some(??)`. We can start by:

::: bq
```cpp
template <formattable<char> T>
struct formatter<Optional<T>, char> {
    // we'll skip parse for now

    template <typename FormatContext>
    auto format(Optional<T> const& opt, FormatContext& ctx) {
        if (not opt) {
            return format_to(ctx.out(), "None");
        } else {
            return format_to(ctx.out(), "Some({})", *opt);
        }
    }
};
```
:::

If we had an `Optional<string>("hello")`, this would format as `Some(hello)`. Which may be fine. But what if we wanted to format it as `Some("hello")` instead? That is, take advantage of the quoting rules we just went through. What do you write instead of `*opt` to format `string`s (or `char`s or user-defined string-like types) as quoted in this context?

Put differently, how does the library implement formatting `pair` and `tuple` and arbitrary `range`s, which also would have to do this dance of conditionally quoting character and string types? Users need that functionality too, and we need to provide it.












Having a type trait ([@P2286R2] proposed `enable_formatting_as_string` as a variable template to opt into

There's basically two different approaches to customization here:

* a type trait (whether variable template or class template) that opts into the customized behavior ([@P2286R2] proposed `enable_formatting_as_string` to enable formatting as a string)
* a class template with a member view that formats its member as appropriate for the kind (string/set/map).

The difference between the two in the typical case where you just have a type that always behaves the same way is:

::: cmptable
### type trait
```cpp
namespace N {
  struct my_string { ... };
}

template <>
inline constexpr bool
std::enable_formatting_as_string<
  N::my_string> = true;
```

### formatter
```cpp
namespace N {
  struct my_string { ... };
}

namespace std {
  template <>
  struct formatter<N::my_string, char>
    : formatter<string_format_wrapper<N::my_string>, char>
  { };
}
```
:::

The latter is clearly more verbose, but has the advantage that if you want to treat a type as a string/set/map that isn't always formatted that way, the custom formatters would let you do that:

::: bq
```cpp
std::vector<std::pair<int, int>> v = {@{@1, 2}, {3, 4}};
std::print("{}\n", v);                // [(1, 2), (3, 4)]
std::print("{}\n", format_as_map(v)); // {1: 2, 3: 4}
```
:::

Which especially comes into play when you're dealing with range adaptors and just construct some range of pairs or range of range of char and wish to print that as a map or string, accordingly. For example:

::: bq
```cpp
vector<string> words = {"  some  ", " words   ", "here"};
std::print("{}\n", words);   // ["  some  ", " words  ", "here"]

auto trimmed = words | transform(views::trim_whitespace);
std::print("{}\n", trimmed); // [['s', 'o', 'm', 'e'], ['w', 'o', 'r', 'd', 's'], ['h', 'e', 'r', 'e']]
std::print("{}\n", trimmed | views::transform(format_as_string));
                             // ["some", "words", "here"]
```
:::

But perhaps the most important question here is: how do you format wrappers?

Let's say you have your own implementation of `Optional`, that you want to format the same way that Rust does: so that a disengaged one formats as `None` and an engaged one formats as `Some(??)`. We can start by:

::: bq
```cpp
template <formattable<char> T>
struct formatter<Optional<T>, char> {
    // we'll skip parse for now

    template <typename FormatContext>
    auto format(Optional<T> const& opt, FormatContext& ctx) {
        if (not opt) {
            return format_to(ctx.out(), "None");
        } else {
            return format_to(ctx.out(), "Some({})", *opt);
        }
    }
};
```
:::

If we had an `Optional<string>("hello")`, this would format as `Some(hello)`. Which is fine. But what if we wanted to format it as `Some("hello")` instead? That is, take advanage of the quoting rules we just went through. Put differently: how would the standard library implement any of the functionality I'm talking about here?

Well, we could start by being very explicit about this:

::: bq
```cpp
template <formattable<char> T>
template <typename FormatContext>
auto formatter<Optional<T>, char>::format(Optional<T> const& opt, FormatContext& ctx) {
    if (not opt) {
        return format_to(ctx.out(), "None");
    } else {
        if constexpr (same_as<T, char>) {
            return format_to(ctx.out(), "Some({})", format_as_char(*opt));
        } else if constexpr (enable_formatting_as_string<T>) {
            return format_to(ctx.out(), "Some({})", format_as_string(*opt));
        } else {
            return format_to(ctx.out(), "Some({})", *opt);
        }
    }
}
```
:::

Note that we need *both* `enable_formatting_as_string` here *and also* `format_as_string`. Because these types inherently have two different formatting rules.

I don't want every wrapper type to have to write all of that, so we clearly need another standard facility to do this. We can call it `format_as_quoted`:

::: bq
```cpp
template <formattable<char> T>
template <typename FormatContext>
auto formatter<Optional<T>, char>::format(Optional<T> const& opt, FormatContext& ctx) {
    if (not opt) {
        return format_to(ctx.out(), "None");
    } else {
        return format_to(ctx.out(), "Some({})", format_as_quoted(*opt));
    }
}
```
:::

### Summary of Fancied Formatting Facilities

In short:

1. Ranges format as `[a, b, c]`
2. Tuples and pairs format as `(x, y, z)`
3. Chars and strings when in ranges and tuples format as quoted. User-defined strings can opt in to this quoting by specializing `enable_formatting_as_string`.
4. Standard library associative containers format as `{k1: v1, k2: v2}` or `{k1, k2}`.
5. Different formatting can be opted into by wrapping a type in `format_as_char`, `format_as_string`, `format_as_set`, `format_as_map`. The function `format_as_meow` returns a `meow_format_wrapper<T>` (which contains a single member of type `T*`).
6. A generic type can be formatted as quoted by wrapping it with `format_as_quoted` (which may perform either `format_as_char`, `format_as_string`, or identity).

## What additional functionality?

There's three layers of potential functionality:

1. Top-level printing of ranges: this is `fmt::print("{}", r);`
2. A format-joiner which allows providing a format specifier for each element and a delimiter: this is `fmt::print("{:02x}", fmt::join(r, ":"))`.
3. A more involved version of a format-joiner which takes a delimiter and a callback that gets invoked on each element. `fmt` does not provide such a mechanism, though the Rust itertools library does:

    ```rust
    let matrix = [[1., 2., 3.],
              [4., 5., 6.]];
    let matrix_formatter = matrix.iter().format_with("\n", |row, f| {
                                    f(&row.iter().format_with(", ", |elt, g| g(&elt)))
                                 });
    assert_eq!(format!("{}", matrix_formatter),
               "1, 2, 3\n4, 5, 6");
    ```

This paper suggests the first two and encourages research into the third.

## What about format specifiers?

The implementation experience in `fmt` is that directly formatting ranges does _not_ support any format specifiers, but `fmt::join` supports providing a specifier per element as well as providing the delimiter and wrapping brackets.

We could add the same format specifier support for direct formatting of ranges as `fmt::join` supports, but it doesn't seem especially worthwhile. If you don't care about formatting, `"{}"` is all you need. If you do care about formatting, it's likely that you care about more than just the formatting of each individual element &mdash; you probably care about other things do. At which point, you'd likely need to use `fmt::join` anyway.

That seems like the right mix of functionality to me.

## How to support those views which are not `const`-iterable?

In earlier revisions of this paper, we had to deal with the problem of ranges that are neither `const`-iterable nor copyable and how to handle that situation (which earlier revisions answered by... uh... not handling that situation). With [@P2418R0], however, that is no longer a problem: just pass in your non-`const`-iterable range as non-`const`.

This still begs the question of if this should compile:

::: bq
```cpp
std::vector<int> ints = {1, 2, 3, 4};
auto const cannot_even = ints | views::filter([](int i){ return i % 2 != 0; });
std::print("{}\n", cannot_even);
```
:::

`cannot_even` isn't iterable, because it's `const`. But it *is* copyable and it *is* a view. Prior revisions of this paper supported this case by copying it, which is what the `fmt` library also used to do. But as of yesterday ([111de881](https://github.com/fmtlib/fmt/commit/111de881)), this situation is no longer supported. If you want to print a `const` object that is a copyable `view` that is not `const`-iterable, it is up to you to copy it to produce a non-`const` object. This has the benefit of making the copy more obvious to the user.

Alternatively, users can use the proposed equivalent of `fmt::join` to avoid the copy.

## Specifying formatters for ranges

It's quite important that a `std::string` whose value is `"hello"` gets printed as `hello` rather than something like `[h, e, l, l, o]`.

This would basically fall out no matter how we approach implementing such a thing, so in of itself is not much of a concern. However, for users who have either custom containers or want to customize formatting of a standard container for their own types, they need to make sure that they can provide a specialization which is more constrained than the standard library's for ranges. To ensure that they can do that, I think we need to be clear about the specific constraint we use when we specify this, and thus this paper proposes a user-facing concept `formattable` that other parts of this proposal will directly use.

## `format` or `std::cout`?

Just `format` is sufficient.

## What about `vector<bool>`?

Nobody expected this section.

The `value_type` of this range is `bool`, which is formattable. But the `reference` type of this range is `vector<bool>::reference`, which is not. In order to make the whole type formattable, we can either make `vector<bool>::reference` formattable (and thus, in general, a range is formattable if its reference types is formattable) or allow formatting to fall back to constructing a `value` for each `reference` (and thus, in general, a range is formattable if either its reference type or its `value_type` is formattable).

For most ranges, the `value_type` is `remove_cvref_t<reference>`, so there's no distinction here between the two options. And even for `zip`, there's still not much distinction since it just wraps this question in `tuple` since again for most ranges the types will be something like `tuple<T, U>` vs `tuple<T&, U const&>`, so again there isn't much distinction.

`vector<bool>` is one of the very few ranges in which the two types are truly quite different. So it doesn't offer much in the way of a good example here, since `bool` is cheaply constructible from `vector<bool>::reference`. Though it's also very cheap to provide a formatter specialization for `vector<bool>::reference`.

Rather than having the library provide a default fallback that lifts all the `reference` types to `value_type`s, which may be arbitrarily expensive for unknown ranges, this paper proposes a format specialization for `vector<bool>::reference`. Or, rather, since it's actually defined as `vector<bool, Alloc>::reference`, this isn't necessarily feasible, so instead this paper proposes a specialization for `vector<bool, Alloc>` at top level.


# Proposal

The standard library should add specializations of `formatter` for:

* any type `T` such that `T const` satisifies `range` and whose `reference` is formattable,
* `pair<T, U>` if `T` and `U` are formattable,
* `tuple<Ts...>` if all of `Ts...` are formattable,
* `vector<bool, Alloc>::reference` (which formats as a `bool`).

Ranges should be formatted as `[x, y, z]` while tuples should be formatted as `(a, b, c)`. `std::array` is tuple-like, but not a tuple, it's treated as a range. In the context of formatting ranges, pairs, and tuples, character types (in the [basic.fundamental]{.sref} sense) or string-like (e.g. `string`, `string_view`, controlled by `enable_formatting_as_string`) should be formatted as being quoted (characters using `'` and strings using `"`).

Formatting ranges does not support any additional format specifiers.

The standard library should also add a utility `std::format_join` (or any other suitable name, knowing that `std::views::join` already exists), following in the footsteps of `fmt::join`, which allows the user to provide more customization in how ranges and tuples get formatted.

For types like `std::generator<T>` (which are move-only, non-const-iterable ranges), users will have to either use `std::format_join` facility or use something like `ranges::ref_view` as shown earlier.


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
