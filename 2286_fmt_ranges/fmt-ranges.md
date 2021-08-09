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

Should `std::map<int, int>{@@{1, 2}, {3, 4}}` be printed as `[(1, 2), (3, 4)]` (as follows directly from the two previous choices) or as `{1: 2, 3: 4}` (which makes the *association* clearer in the printing)? Both Python and Rust print their associating containers this latter way.

The same question holds for sets as well as maps, it's just a question for whether `std::set<int>{1, 2, 3}` prints as `[1, 2, 3]` (i.e. as any other range of `int`) or `{1, 2, 3}`? 

If we print `map`s as any other range of pairs, there's nothing left to do. If we print `map`s as associations, then we additionally have to answer the question of how user-defined associative containers can get printed in the same way. Hold onto this thought for a minute.

### `char` and `string` (and other string-like types) in ranges or tuples

Should `pair<char, string>('x', "hello")` print as `(x, hello)` or `('x', "hello")`? Should `print<char, string>('y', "with\n\"quotes\"")` print as:

::: bq
```
(y, with
"quotes")
```
:::

or

::: bq
```
('y', "with
"quotes"")
```
:::

or

::: bq
```
('y', "with\n\"quotes\"")
```
:::

While `char` and `string` are typically printed unquoted, it is quite common to print them quoted when contained in tuples and ranges (as Python, Rust, and `fmt` do). But there is a difference between `fmt` and Python/Rust when it comes to embedded quotes:

* `fmt` simply surrounds strings with "s and doesn't escape any internal characters, so has the second implementation above there (with the extra newline)
* Rust does escape internal strings , so prints as `('y', "with\n\"quotes\"")` (the Rust implementation of `Debug` for `str` can be found [here](https://doc.rust-lang.org/src/core/fmt/mod.rs.html#2073-2095) which is implemented in terms of [`escape_debug_ext`](https://doc.rust-lang.org/src/core/char/methods.rs.html#405-419)).
* Python sort of does and sort of doesn't, owing to the fact that you can have strings with single or double quotes, but it definitely escapes the `\n` either way.

Escaping seems like the most desirable behavior. Following Rust's behavior, we escape `\t`, `\r`, `\n`, `\\`, `"` (for `string` types only), `'` (for `char` types only), and extended graphemes (if Unicode). 

Also, `std::string` isn't the only string-like type: if we decide to print strings quoted, how do users opt in to this behavior?

### Customization

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
std::vector<std::pair<int, int>> v = {@@{1, 2}, {3, 4}};
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

## Wording

The wording here is grouped by functionality added rather than linearly going through the standard text. 

### Concept `formattable`

First, we need to define a user-facing concept. We need this because we need to constrain `formatter` specializations on whether the underlying elements of the `pair`/`tuple`/range are formattable, and users would need to do the same kind of thing for their types. This is tricky since formatting involves so many different types, so this concept will never be perfect, so instead we're trying to be good enough:

Change [format.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...

  // [format.formatter], formatter
  template<class T, class charT = char> struct formatter;
  
  // [format.parse.ctx], class template basic_format_parse_context
  template<class charT> class basic_format_parse_context;
  using format_parse_context = basic_format_parse_context<char>;
  using wformat_parse_context = basic_format_parse_context<wchar_t>;

+ // [format.formattable], formattable
+ template<class T, class charT>
+   concept formattable = @*see below*@;

  // ...
}
```
:::

Add a clause [format.formattable] under [format.formatter]{.sref} and likely after [formatter.requirements]{.sref}

::: bq
::: addu
[1]{.pnum} Let `@*fmt-iter-for*@<charT>` be an implementation-defined type that models `output_iterator<const charT&>` ([iterator.concept.output]).
```
template<class T, class charT>
concept @*formattable-impl*@ =
    semiregular<formatter<T, charT>> &&
    requires (formatter<T, charT> f,
              const T t,
              basic_format_context<@*fmt-iter-for*@<charT>, charT> fc,
              basic_format_parse_context<charT> pc) {
        { f.parse(pc) } -> same_as<basic_format_parse_context<charT>::iterator>;
        { f.format(t, fc) } -> same_as<@*fmt-iter-for*@<charT>>;
    };

template<class T, class charT>
concept formattable = @*formattable-impl*@<remove_cvref_t<T>, charT>;
```

[2]{.pnum} A type `T` and a character type `charT` model `formattable` if `formatter<T, charT>` meets the *Formatter* requirements ([formatter.requirements]).
:::
:::

### Additional `formatter` specializations in `<format>`

Change [format.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...

  // [format.formatter], formatter
+ template<class T>
+   inline constexpr bool enable_formatting_as_string = @*see below*@;
  
  template<class T, class charT = char> struct formatter;

+ // [format.range], range formatter
+ template<class R, class charT>
+   concept @*default-formattable-range*@ =     // exposition only
+     ranges::input_range<const R> && formattable<ranges::range_reference_t<const R>, charT>
+     || ranges::input_range<R> && ranges::view<R> && copyable<R> && formattable<ranges::range_reference_t<R>, charT>
+
+ template<class charT, @*default-formattable-range*@<charT>, R>
+   struct formatter<R, charT>;
  
  // ...
}
```
:::

Add to... somewhere:

::: bq
::: addu
```
template<class T>
  inline constexpr bool enable_formatting_as_string = false;
  
template<class charT, class traits, class Allocator>
  inline constexpr bool enable_formatting_as_string<basic_string<charT, traits, Allocator>> = true;
  
template<class charT, class traits>
  inline constexpr bool enable_formatting_as_string<basic_string_view<charT, traits>> = true;  
```

[*]{.pnum} *Remarks*: Pursuant to [namespace.std], users may specialize `enable_formatting_as_string` to `true` for any cv-unqualified program-defined type `T` which models `@*default-formattable-range*@<charT>`. [*Note*: Users may do so to ensure that a program-defined string type formats as `"hello"` rather than as `['h', 'e', 'l', 'l', 'o']` *-end note*].
:::
:::

Add the new clause [format.range]:

::: bq
::: addu
```
template<class charT>
inline constexpr auto @*format-maybe-quote*@ = // exposition only
    []<class T>(const T& t){
      if constexpr (is_same_v<T, charT>) {
        return format(@*STATICALLY-WIDEN*@<charT>("'{}'"), t);
      } else if constexpr (enable_formatting_as_string<T>) {
        return format(@*STATICALLY-WIDEN*@<charT>("\"{}\""), t);
      } else {
        return format(@*STATICALLY-WIDEN*@<charT>("{}"), t);
      }
    };

template<class charT, @*default-formattable-range*@<charT>, R>
  struct formatter<R, charT> {
    template <class ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);
        
    template <class FormatContext>
      typename FormatContext::iterator
        format(const R& range, FormatContext& ctx);
  };
```

```
template <class ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```
[1]{.pnum} Let `T` denote the type `ranges::range_reference_t<const R>` if `const R` models `ranges::range` and `ranges::range_reference_t<R>` otherwise. 

[2]{.pnum} *Throws*: `format_error` if `ctx` does not refer to an empty *format-spec* or anything that `formatter<remove_cvref_t<T>, charT>().parse(ctx)` throws.

[3]{.pnum} *Returns*: `ctx.begin()`.

```
template <class FormatContext>
  typename FormatContext::iterator
    format(const R& range, FormatContext& ctx);
```
[4]{.pnum} *Effects*: Let `r` be `range` if `const R` models `ranges::range` and `views::all(range)` otherwise. Writes the following into `ctx.out()`:

- [4.1]{.pnum} `@*STATICALLY-WIDEN*@<charT>("[")`
- [4.2]{.pnum} for each element, `e`, of the range `r`:
    * [4.2.1]{.pnum} `@*format-maybe-quote*@<charT>(e)`
    * [4.2.2]{.pnum} `@*STATICALLY-WIDEN*@<charT>(", ")`, unless `e` is the last element of `r`
- [4.3]{.pnum} `@*STATICALLY-WIDEN*@<charT>("]")`

[5]{.pnum} *Returns*: an iterator past the end of the output range
:::
:::

### The join formatter

Change [format.syn]{.sref}:

::: bq
```diff
namespace std {
  // ...

  // [format.error], class format_error
  class format_error;
  
+ // [format.join], a join formatter
+ template <ranges::input_range V, class charT>
+   requires ranges::view<V> &&
+            formattable<ranges::range_reference_t<V>, charT>
+ class @*format-join-impl*@; // exposition only
+
+ template <ranges::input_range V, class charT>
+   requires ranges::view<V> &&
+            formattable<ranges::range_reference_t<V>, charT>
+ struct formatter<@*format-join-impl*@<V, charT>, charT>;
+
+
+ template <ranges::input_range R>
+   requires formattable<ranges::range_reference_t<R>, char>
+ constexpr @*format-join-impl*@<ranges::ref_view<remove_reference_t<R>>, char>
+   format_join(R&& range, string_view sep);
+
+ template <ranges::input_range R>
+   requires formattable<ranges::range_reference_t<R>, wchar_t>
+ constexpr @*format-join-impl*@<ranges::ref_view<remove_reference_t<R>>, wchar_t>
+   format_join(R&& range, wstring_view sep);
}
```
:::

Add a new clause [format.join]:

::: bq
::: addu
[1]{.pnum} The function template `format_join` is a convenient utility to provide a custom *format-spec* to apply to each element when formatting a range, along with a custom delimiter. 

[2]{.pnum} [*Example*:
```
cout << format("{:02x}", format_join(vector{10,20,30,40,50,60}, ":")); // prints 0a:14:1e:28:32:3c
```
-*end example*]

```
template <ranges::input_range V, class charT>
  requires ranges::view<V> &&
           formattable<ranges::range_reference_t<V>, charT>
class @*format-join-view*@ {                               // exposition only
  V @*view*@;                                              // exposition only
  basic_string_view<charT> @*sep*@;                        // exposition only
  
public:
  constexpr @*format-join-view*@(V v, basic_string_view<charT> s);
};
```

```
constexpr @*format-join-view*@(V v, basic_string_view<charT> s)
```

[3]{.pnum} *Effects*: Direct-non-list-initializes `@*view*@` with `std::move(v)` and `@*sep*@` with `s`.

```
template <ranges::input_range V, class charT>
  requires ranges::view<V> &&
           formattable<ranges::range_reference_t<V>, charT>
class formatter<@*format-join-impl*@<V, charT>, charT> {
  formatter<remove_cvref_t<ranges::range_reference_t<V>>, charT> @*fmt*@;  // exposition only

public:
  template <typename ParseContext>
    constexpr typename ParseContext::iterator
      parse(ParseContext& ctx);
      
  template <typename FormatContext>
    typename FormatContext::iterator
      format(const @*format-join-impl*@<V, charT>& j, FormatContext& ctx);
};
```

```
template <typename ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```
[4]{.pnum} *Effects*: Equivalent to `return @*fmt*@.parse(ctx);`
  
```
template <typename FormatContext>
  typename FormatContext::iterator
    format(const @*format-join-impl*@<V, charT>& j, FormatContext& ctx);
```

[5]{.pnum} *Effects*: Write the following into `ctx.out()`:

* [3.1]{.pnum} For each element `e` of `j.@*view*@`:
    * [3.1.1]{.pnum} `@*fmt*@.format(e, ctx)`
    * [3.1.2]{.pnum} `j.@*sep*@`, unless `e` is the last element of `j.@*view*@`
    
[6]{.pnum} *Returns*: an iterator past the end of the output range

```
template <ranges::input_range R>
  requires formattable<ranges::range_reference_t<R>, char>
constexpr @*format-join-view*@<ranges::ref_view<remove_reference_t<R>>, char>
  format_join(R&& range, string_view sep);
  
template <ranges::input_range R>
  requires formattable<ranges::range_reference_t<R>, wchar_t>
constexpr @*format-join-view*@<ranges::ref_view<remove_reference_t<R>>, wchar_t>
  format_join(R&& range, wstring_view sep);  
```

[7]{.pnum} *Effects*: Equivalent to `return {ranges::ref_view(range), sep};`

```
```
:::
:::

### Formatter for `pair`

Add to [utility.syn]{.sref}

::: bq
```diff
namespace std {
  // ...

  // [pairs], class template pair
  template<class T1, class T2>
    struct pair;
  
+ template<class charT, formattable<charT> T1, formattable<charT> T2>
+   struct formatter<pair<T1, T2>, charT>;
+
+ template <formattable<char> T1, formattable<char> T2>
+   constexpr @*see below*@ format_join(const pair<T1, T2>& p, string_view sep);
+
+ template <formattable<wchar_t> T1, formattable<wchar_t> T2>
+   constexpr @*see below*@ format_join(const pair<T1, T2>& p, wstring_view sep);

  // ...  
};
```
:::

Add a new subclause [pair.format] under [pairs]{.sref}

::: bq
::: addu
```
template<class charT, formattable<charT> T1, formattable<charT> T2>
  struct formatter<pair<T1, T2>, charT> {
    template <typename ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);
        
    template <typename FormatContext>
      typename FormatContext::iterator
        format(const pair<T1, T2>& p, FormatContext& ctx);    
  };
```

```
template <typename ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```   

[1]{.pnum} *Throws*: `format_error` if `ctx` does not refer to an empty *format-spec* or anything that `formatter<remove_cvref_t<T1>, charT>().parse(ctx)` or `formatter<remove_cvref_t<T2>, charT>().parse(ctx)` throws.

[2]{.pnum} *Returns*: `ctx.begin()`.

```
template <typename FormatContext>
  typename FormatContext::iterator
    format(const pair<T1, T2>& p, FormatContext& ctx);    
```

[3]{.pnum} *Effects*: Writes the following into `ctx.out()`:

* [3.1]{.pnum} `@*STATICALLY-WIDEN*@<charT>("(")`
* [3.2]{.pnum} `@*format-maybe-quote*@<charT>(p.first)`
* [3.3]{.pnum} `@*STATICALLY-WIDEN*@<charT>(", ")`
* [3.4]{.pnum} `@*format-maybe-quote*@<charT>(p.second)`
* [3.5]{.pnum} `@*STATICALLY-WIDEN*@<charT>(")")`

[4]{.pnum} *Returns*: an iterator past the end of the output range

:::
:::

### Formatter for `tuple`

Add to [tuple.syn]{.sref}

::: bq
```diff
#include <compare>              // see [compare.syn]

namespace std {
  // [tuple.tuple], class template tuple
  template<class... Types>
    class tuple;
  
+ template<class charT, formattable<charT>... Types>
+   struct formatter<tuple<Types...>, charT>;
+
+ template <formattable<char>... Types>
+   constexpr @*see below*@ format_join(const tuple<Types...>& t, string_view sep);
+
+ template <formattable<wchar_t>... Types>
+   constexpr @*see below*@ format_join(const tuple<Types...>& t, wstring_view sep);

  // ...  
};
```
:::

Add a new subclause [tuple.format] under [tuple]{.sref}

::: bq
::: addu
```
template<class charT, formattable<charT>... Types>
  struct formatter<tuple<Types...>, charT> {
    template <typename ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);
        
    template <typename FormatContext>
      typename FormatContext::iterator
        format(const tuple<Types...>& t, FormatContext& ctx);    
  };
```

```
template <typename ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```   

[1]{.pnum} *Throws*: `format_error` if `ctx` does not refer to an empty *format-spec* or anything that `formatter<remove_cvref_t<T>, charT>().parse(ctx)` throws for each type `T` in `Types...`.

[2]{.pnum} *Returns*: `ctx.begin()`.

```
template <typename FormatContext>
  typename FormatContext::iterator
    format(const tuple<Types...>& t, FormatContext& ctx);    
```

[3]{.pnum} *Effects*: Writes the following into `ctx.out()`:

* [3.1]{.pnum} `@*STATICALLY-WIDEN*@<charT>("(")`
* [3.2]{.pnum} For each element `e` in the tuple `t`:`

    * [3.2.1]{.pnum} `@*format-maybe-quote*@<charT>(e)`
    * [3.2.2]{.pnum} `@*STATICALLY-WIDEN*@<charT>(", ")`, unless `e` is the last element of `t`
* [3.3]{.pnum} `@*STATICALLY-WIDEN*@<charT>(")")`

[4]{.pnum} *Returns*: an iterator past the end of the output range

:::
:::

### Formatter for `vector<bool>::reference`

Add to [vector.syn]{.sref}

::: bq
```diff
namespace std {
  // [vector], class template vector
  template<class T, class Allocator = allocator<T>> class vector;

  // ...

  // [vector.bool], class vector<bool>
  template<class Allocator> class vector<bool, Allocator>;
  
+ template<class R>
+   inline constexpr bool @*is-vector-bool-reference*@ = @*see below*@; // exposition only

+ template<class R, class charT> requires @*is-vector-bool-reference*@<R>
+   struct formatter<R, charT>;
```
:::

Add to [vector.bool] at the end:

::: bq
::: addu
```
template<class R>
  inline constexpr bool @*is-vector-bool-reference*@ = @*see below*@;
```
[8]{.pnum} The variable template `@*is-vector-bool-reference*@<T>` is `true` if `T` denotes the type `vector<bool, Alloc>::reference` for some type `Alloc` if `vector<bool, Alloc>` is not a program-defined specialization.

```
template<class R, class charT> requires @*is-vector-bool-reference*@<R>
  class formatter<R, charT> {
    formatter<bool, charT> @*fmt*@;     // exposition only
  
  public:
    template <class ParseContext>
      constexpr typename ParseContext::iterator
        parse(ParseContext& ctx);
        
    template <class FormatContext>
      typename FormatContext::iterator
        format(const R& ref, FormatContext& ctx);
  };
```

```
template <class ParseContext>
  constexpr typename ParseContext::iterator
    parse(ParseContext& ctx);
```

[9]{.pnum} *Effects*: Equivalent to `return @*fmt*@.parse(ctx);`
    
```
template <class FormatContext>
  typename FormatContext::iterator
    format(const R& ref, FormatContext& ctx);
```

[10]{.pnum} *Effects*: Equivalent to `return @*fmt*@.format(ref, ctx);`
:::
:::

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