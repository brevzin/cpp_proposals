---
title: "Additional format specifiers for `time_point`"
document: P2945R1
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Revision History

From [@P2945R0] to R1: Removing any option to change the meaning of existing code. The current proposal is a pure extension.

# Introduction

`std::chrono::time_point` has a lot of format specifiers. You can peruse the table [here](https://eel.is/c++draft/tab:time.format.spec). This makes it very convenient for the user to format their `time_point` however they want: the date in either the correct (`2023-07-08`{.op}) or incorrect (`07/08/2023`{.op}) numeric formats, spelling out the month (`July` or `Jul`), presenting the time in 24-hour notation or using `AM` or `PM`, etc. This is all very useful.

But there's a few format specifier that I believe are missing that I would like to propose:

<table>
<tr><th>desired output</th><th>proposed</th><th>current workaround</th></tr>
<tr>
<td>1688830834295314673</t>
<td>`std::format("{:%s}", tp)`</td>
<td>`std::format("{:%Q}", tp.time_since_epoch())`<br/>or `std::format("{}", tp.time_since_epoch().count())`</td>
</tr>
<tr>
<td>15:40:34</td>
<td>`std::format("{:%H:%M:%.0S}", tp)`</td>
<td>`std::format("{:%H:%M:%S}", std::chrono::time_point_cast<std::chrono::seconds>(tp))`</td>
</tr>
<tr>
<td>15:40:34.295</td>
<td>`std::format("{:%H:%M:%.3S}", tp)`</td>
<td>`std::format("{:%H:%M:%S}", std::chrono::time_point_cast<std::chrono::milliseconds>(tp))`</td>
</tr>
<tr>
<td>15:40:34.295314</td>
<td>`std::format("{:%H:%M:%.6S}", tp)`</td>
<td>`std::format("{:%H:%M:%S}", std::chrono::time_point_cast<std::chrono::microseconds>(tp))`</td>
</tr>
</table>

In addition to proposing `%.$n$S` (for seconds with `$n$` decimal digits), this paper also proposes `%.$n$T` to mean `%H:%M:%.$n$S`.

# Why More Specifiers

First, it is simply much more convenient for the user to write something like `%.0T` if what they want is `15:40:34` then it is for them to write that rather verbose cast expression to convert their `time_point` into `seconds` duration.

Second, `%.0T`{.op} ensures that they don't actually have to care about the underlying duration of their `time_point`, this will consistently produce the same output regardless of whether it's `time_point<system_clock, seconds>` or `time_point<system_clock, milliseconds>` or `time_point<system_clock, nanoseconds>`.

Third, specifiers can nest in a way that those workarounds don't. For example, it is straightforward to implement formatting for `Optional<T>` such that it supports all of `T`'s format specifiers, so that `Optional<int>(42)` can format using `{:#x}`{.op} as `Some(0x2a)`. If `time_point` has the necessary specifiers then an `Optional<time_point>` can be formatted as desired simply using `time_point`'s specifiers. Otherwise, the workaround requires calling `Optional::transform`. The same argument can be made for ranges: use the underlying specifier, or have to resort to using `std::views::transform`. This certainly becomes inconvenient for the user pretty rapidly, but it also brings up a question of safety - something I'm going to call the capture problem [^naming].

Fourth, the user may only even have access to provide the specifier and may not even have the ability to touch the `time_point` _at all_. This is the case in some logging libraries (like `spdlog`) where the user-facing API can provide a specifier for how the message is formatted, but never even sees the `time_point` object.

[^naming]: This probably already exists in the literature under a much more suitable name, so I'm hoping by giving it a bad name somebody simply points me to the correct one later.

## The Capture Problem

Consider:

::: bq
```cpp
void print_names(std::vector<std::string> const& names) {
    std::println("{}", names);
}
```
:::

Here, the formatting is all done immediately. `std::println` doesn't need to (and doesn't) copy `names` and there are no lifetime issues here at all. Now consider a slight variation:

::: bq
```cpp
void log_names(std::vector<std::string> const& names) {
    LOG("{}", names);
}
```
:::

Here, `LOG` is a stand-in for your favorite logging framework. Some of these do foreground logging (and so wouldn't need to copy `names`), some of these do background logging (and so would have to, and do, copy `names`), some of these do it conditionally. Either way, the above still works, because the logger simply does the right thing with the object.

Let's say we don't want to log the names, but rather want to log some... transformed version thereof:

::: bq
```cpp
void log_transformed_names(std::vector<std::string> const& names) {
    LOG("{}", names | std::views::transform(f));
}
```
:::

One example of this being wanting to provide a custom delimiter, which currently doesn't exist as a specifier:

::: bq
```cpp
void log_transformed_names(std::vector<std::string> const& names) {
    LOG("{}", fmt::join(", and ", names));
}
```
:::

Now we have a problem. If `LOG` is doing background logging (whether always or conditionally) and it copies the result of `fmt::join` and that's... just a view. We're not copying the underlying `names` and now we have a potential problem with lifetimes. This could now dangle.

Or it could be fine, if we're doing foreground logging! So we have this problem where if we're doing foreground logging, we definitely want to just log the `view` without any additional work. Whereas if we're doing background logging, we probably want to eagerly construct a `vector` out of it (or eagerly do the formatting for this argument) to avoid any lifetime issues.

We have no way of "just doing the right thing" here. We have no way using the `view` if that's good enough or collecting the elements otherwise, nor do we have a way of signaling when using the `view` might dangle.

Note that this is not specific to ranges and views at all, I'm just using it as a simple example.

Allowing more common logic into the specifiers simply avoids this problem. We don't have to figure out how and when to wrap arguments, since the logic entirely lives in the specifier. Here, I'm not trying to solve the general capture problem, I'm only trying to help a little bit more in the context of `time_point`s.

## Why not use precision?

Rather than having a prefixed specifier to do millisecond precision, like `%3S`{.op} or `%.3S`{.op}, could we use the already-existing `$precision$` specifier and write something like `{:.3%H:%M:%S}`{.op}? We could, but I think this would be a poor choice.

First, this is what the standard has to say about `$precision$` for a `$chrono-format-spec$` (in [time.format]{.sref}/1):

::: bq
[1]{.pnum} [...] Giving a `$precision$` specification in the `$chrono-format-spec$` is valid only for types that are specializations of `std​::​chrono​::​duration` for which the nested `$typedef-name$` `rep` denotes a floating-point type. For all other types, an exception of type `format_error` is thrown if the `$chrono-format-spec$` contains a `$precision$` specification.
:::

That's it. So, first, `$precision$` is only meaningful for floating-point types, which makes it useless here because even `double` doesn't have enough precision for nanoseconds, so `system_clock::rep` is practically speaking going to be an integral type (even though all we specify about it is that it's signed). Second, we don't... actually say what `$precision$` does anywhere.

But even assuming that it does what we might think of as "the obvious thing", consider the difference between the two choices of specifier:

|using `$precision$`|modifying `%S`|
|-|-|
|`"{:.3%Y-%m-%d %H:%M:%S}"`|`"{:%Y-%m-%d %H:%M:%.3S}"`|

The version on the left just has this weirdly dangling `.3`, that only applies to the much later `%S`. That's not really how any of the other specifiers behave and is needlessly harder to understand. It would also meant that you can provide a `$precision$` without providing any specifier that makes use of it, which is just a pointless thing to allow.

## Existing Practice in Other Languages

### UNIX

In the UNIX `date` program, `%S` is always an integer number of seconds and `%s` is seconds since epoch. If you want decimal digits, you can add a width to `%N` (which defaults to `9`):

|specifier|example|
|-|-|
|`+%T`{.op}|`09:40:34`{.op}|
|`+%T.%N`{.op}|`09:40:34.295314673`{.op}|
|`+%T.%3N`{.op}|`09:40:34.295`{.op}|
|`+%s`{.op}|`1688830834`{.op}|
|`+%s%N`{.op}|`1688830834295314673`{.op}|

### C

`tm` has no subsecond field, but in `strftime`, `%S` is an integral number of seconds and `%s` is seconds since epoch.

### Python

In `datetime.strftime`, `%S` is always an integer number of seconds and `%s` is seconds since epoch. `%f` exists to print microseconds, as in `%S.%f` or `%s%f`, but there is no way to get any other precision.

### Rust

In Rust in the [`chrono` crate](https://docs.rs/chrono/latest/chrono/format/strftime/index.html), `%S` is always an integer number of seconds, `%s` is seconds since epoch. The following specifiers exist to get subsecond precision:

|specifier|example|
|-|-|
|`%f`{.op}|`026490000`{.op}|
|`%.f`{.op}|`.026940`{.op}|
|`%.3f`{.op}|`.026`{.op}|
|`%.6f`{.op}|`.026490`{.op}|
|`%.9f`{.op}|`.026490000`{.op}|
|`%3f`{.op}|`026`{.op}|
|`%6f`{.op}|`026490`{.op}|
|`%9f`{.op}|`026490000`{.op}|

### Ruby

In [Ruby](https://docs.ruby-lang.org/en/master/strftime_formatting_rdoc.html), `%S` is always an integer number of seconds, `%s` is seconds since epoch, and the following specifiers can give you subsecond precision:

|specifier|example|
|-|-|
|`%L`{.op}|`323`{.op}|
|`%N`{.op}|`323091400`{.op}|
|`%3N`{.op}|`323`{.op}|
|`%6N`{.op}|`323091`{.op}|
|`%9N`{.op}|`323091400`{.op}|
|`%24N`{.op}|`323091400000000000000000`{.op}|

I feel like yoctoseconds is probably not a unit people are going to use very often, but there it is.

### C++ Logging Libraries

Despite broadly using `{fmt}`, [spdlog](https://github.com/gabime/spdlog/wiki/3.-Custom-formatting) actually uses its own approach for `time_point` formatting specifically to allow full control over the timestamp. As I mentioned [earlier](#why-more-specifiers), `spdlog` never exposes the `time_point` — only the ability to format it with a custom specifier. While it also uses `%S` as an integer number of seconds, it instead uses `%E` as seconds since epoch. For the fractional part, it provides distinct specifiers for milliseconds, microseconds, and nanoseconds:

|specifier|example|
|-|-|
|`%e`{.op}|`678`{.op}|
|`%f`{.op}|`056789`{.op}|
|`%F`{.op}|`256789123`{.op}|

Similarly, [Quill](https://quillcpp.readthedocs.io/en/latest/formatters.html), uses `strftime` as its model and thus has `%S` as two-digit integral seconds and `%s` as seconds since epoch, and additionally provides `%Qms`, `%Qus`, and `%Qns` for the fractional parts.

### Standard C++, `<chrono>`, and `<format>`

As you can see above, there's a pretty impressive consensus on what a few specifiers mean. To everybody:

* `%S` is a specifier that gives you a two-digit, integer number of seconds from `00`{.op} to `59`{.op}, and
* `%s` is a specifier that gives you the integer number of seconds since epoch (everyone except `spdlog`, which uses `%E` for this).

To everyone, that is, except C++20's approach to chrono formatting. Why is that? Our wording comes from [@P1361R2], which itself comes from [@P0355R7]. That paper, since R0, has always defined `%S` in the same way. The specific words changed over time, but even [@P0355R0] states:

::: bq
* If `%S` or `%T` appears in the format string and the argument `tp` has precision finer than seconds, then seconds are formatted as a decimal floating point number with a fixed format and a precision matching that of the precision of `tp`. The character for the decimal point is localized according to the `locale`.
:::

P0355 describes itself as proposing "`strftime`-like formatting" but offers no explanation that I can find for why it differs from `strftime` in this case [^differs]. Nor can I find any evidence that this difference was noted in either LEWG or LWG any of the times this paper was discussed.

[^differs]: You could argue that it doesn't actually differ from `strftime` in the sense that in both cases, `%S` formats all the sub-minute time - it's just that C did not have any subsecond precision. I don't find this argument particularly compelling - `%S` went from always printing a two-digit integer number of seconds to printing decimals.

I consider this an unfortunate design choice, but it's the one we have.

# Proposal

This paper proposes that we:

* Add several new specifiers (`%s`, `%f`, `%Q`, and `%q`)
* Add modifiers to some existing ones (`%S` and `%T`)

I'll go through these in turn.

## `%s`, `%S`, and `%T`

I propose that we:

* Add `%s` to be the number of ticks since epoch in the `time_point`'s units.
* Allow both `%S` and `%s` to be prefixed with a precision to indicate how many subsecond digits to include. For formatting milliseconds, this would look like `%.3S`{.op} for the former and `%.3s`{.op} for the latter.
* Extend `%T` in the same way that we extend `%S`, so that `%.9T`{.op} means `%H:%M:%.9S`{.op}.

Proposed examples (which assume that `system_clock::time_point` has nanosecond resolution):

<table>
<tr><th>desired output</th><th>proposed</th></tr>
<tr>
<td>1688830834295314673</t>
<td>`std::format("{:%s}", tp)`</td>
</tr>
<tr>
<td>1688830834</t>
<td>`std::format("{:%.0s}", tp)`</td>
</tr>
<tr>
<td>15:40:34</td>
<td>`std::format("{:%H:%M:%.0S}", tp)`</td>
</tr>
<tr>
<td>15:40:34.295</td>
<td>`std::format("{:%H:%M:%.3S}", tp)`</td>
</tr>
<tr>
<td>15:40:34.295314</td>
<td>`std::format("{:%H:%M:%.6S}", tp)`</td>
</tr>
</table>

## `%f`

`%f` is a new specifier that gives you the fractional seconds, with or without the decimal point, with customizable precision. The meaning for `%f`, specifically, is as follows:

<table>
<tr><th /><th colspan="3">precision</th></tr>
<tr><th>specifier</th><th>`seconds(1)`</th><th>`milliseconds(1234)`</th><th>`nanoseconds(1234567)`</th></tr>
<tr><td>`%.f`{.op}</td><td>empty string</td><td>`.234`</td><td>`.001234567`</td></tr>
<tr><td>`%.0f`{.op}</td><td>empty string</td><td>empty string</td><td>empty string</td></tr>
<tr><td>`%.3f`{.op}</td><td>`.000`</td><td>`.234`</td><td>`.001`</td></tr>
<tr><td>`%.5f`{.op}</td><td>`.00000`</td><td>`.23400`</td><td>`.00123`</td></tr>
<tr><td>`%f`{.op}</td><td>empty string</td><td>`234`</td><td>`001234567`</td></tr>
<tr><td>`%0f`{.op}</td><td>empty string</td><td>empty string</td><td>empty string</td></tr>
<tr><td>`%3f`{.op}</td><td>`000`</td><td>`234`</td><td>`001`</td></tr>
<tr><td>`%5f`{.op}</td><td>`00000`</td><td>`23400`</td><td>`00123`</td></tr>
</table>

## Exotic durations

There's one more thing that needs to be touched on here. For `sys_time<nanoseconds>`, it's pretty clear what `%s` should mean (nanoseconds since epoch) and what `%.0s`{.op} would mean (seconds since epoch). But `nanoseconds` (and similar units like `microseconds` or `seconds`) aren't the only durations. We also have to consider other ones.

The next most obvious one to consider is... everyone say it with me now... [microfortnights](https://en.wikipedia.org/wiki/FFF_system).

A microfornight is, as the name suggests, one millionth of a fortnight - which is 14 days. In more familiar units, a microfortnight is equal to 1.2096 seconds.

Following the rules that we have today, formatting 1 microfortnight using `%S` would yield `01.2096`. Or, to pick a more interesting time point that is more than a minute since epoch, formatting 123 microfortnights (148.7808 seconds) using `%S` would yield `28.7808`. The question is: what should `%s` print for 123 microfortnights? I think the appropriate answer is `1487808`. That is: while `%S` prints in seconds modulo 60, `%s` prints in the unit that would avoid any decimals - in this case in units of 100μs - and without any modulo. And then explicitly providing a precision would affect the number of "decimal" points that are present. This makes `%.0s`{.op} always seconds since epoch, regardless of underlying precision.

That is, a table of examples for formatting `sys_time(microfortnights(123))` would be:

|specifier|example|
|-|-|
|`%S`{.op}|`28.7808` (C++20 status quo)|
|`%.0S`{.op}|`28`|
|`%.3S`{.op}|`28.780`|
|`%.6S`{.op}|`28.780800`|
|`%s`{.op}|`1487808`|
|`%.0s`|`148`|
|`%.3s`|`148780`|
|`%.4s`|`1487808`|
|`%.6s`|`148780800`|

## `%Q` and `%q`

Separate from the question of what `%S` and `%s` should do: are there any other specifiers that we should add? One that I have not noted here is a specifier to simply format `tp.time_since_epoch().count()`. We have such a thing for `duration`s (`%Q`) but not for `time_point`s. In the proposal right now, `%s` achieves this for the SI units - but not for microfortnights.

When I originally set out to write this paper, my intent was to propose `%Q`. But given the uniformity of treating `%s` as (sub)seconds since epoch, that seemed like a better choice to handle formatting nanoseconds since epoch. This makes `%Q` for `time_point` seem less motivated. But I think we should consider extending `%Q` to work for `time_point` for broadly the reasons described above.

## Implementation Experience

I've implemented part of this in a fork of `{fmt}`, you can find a diff [here](https://github.com/fmtlib/fmt/compare/master...brevzin:fmt:p2945). That just implements the suggested changes to `%S` and the addition of `%s`, but that's basically all the work — `%f` is just part of the formatting of `%S`, and `%Q` and `%q` simply treat the `time_point` as its underlying `duration`.

# Wording

Add `f` and `s` to the options for `$type$` and add support for precision modifiers in [time.format]{.sref}:

::: bq
```diff
  $modifier$: one of
-   E O
+   E O @[*tp-precision*]{.diffins}@

+ $tp-precision$:
+   .@~opt~@ $nonnegative-integer$@~opt~@
+   .@~opt~@ { $arg-id$@~opt~@ }

  $type$: one of
-   a A b B c C d D e F g G h H I j m M n
+   a A b B c C d D e F @[f]{.diffins}@ g G h H I j m M n
-   p q Q r R S t T u U V w W x X y Y z Z %
+   p q Q r R S @[s]{.diffins}@ t T u U V w W x X y Y z Z %
```
:::

The rest of the wording adds and modifies entries in the conversion specifier table in [time.format]{.sref}.

## The `%f` specifier

::: bq
:::addu
|Specifier|Replacement|
|-|-|
|`%f`|Sub-seconds as a decimal number. The format is a decimal floating-point number with a fixed format and precision matching that of the precision of the input (or to the `$tp-precision$` if provided or otherwise to microseconds precision if the conversion to floating-point decimal seconds cannot be made within 18 fractional digits). The localized decimal point is included if the `.` appears in the `$tp-precision$`.|
:::
:::

## The `%Q` and `%q` specifiers

::: bq
|Specifier|Replacement|
|-|-|
|`%q`|The duration's unit suffix as specified in [time.duration.io]{.sref}. [If the type being formatted is a specialization of `time_point`, then the unit suffix of the underlying duration.]{.addu}|
|`%Q`|The duration's [or time_point's]{.addu} numeric value (as if extracted via `.count()` [or .`time_since_epoch().count()`, respectively]{.addu})|
:::

## The `%S`, `%s`, and `%T` specifiers

::: bq
|Specifier|Replacement|
|-|-|
|`%S`|Seconds as a decimal number. If the number of seconds is less than `10`, the result is prefixed with `0`. If the precision of the input cannot be exactly represented with seconds, then the format is a decimal floating-point number with a fixed format and a precision matching that of the precision of the input (or to a microseconds precision if the conversion to floating-point decimal seconds cannot be made within 18 fractional digits). The character for the decimal point is localized according to the locale. The modified command %OS produces the locale's alternative representation. [The modified command `%$tp-precision$S` instead uses `$tp-precision$` as the precision for the input. The `$tp-precision$` must include a `.`.]{.addu}|
|[`%s`]{.addu}|[Duration since epoch as a decimal number in the precision of the input (or in microseconds if the conversion to floating-point decimal seconds cannot be made within 18 fractional digits). The modified command `%$tp-precision$s` instead uses `$tp-precision$` as the precision of the input. The localized decimal point is included if the `.` appears in the `$tp-precision$`.]{.addu}|
|`%T`|Equivalent to `%H:%M:%S`. [The modified command `%$tp-precision$T` is equivalent to `%H:%M:%$tp-precision$S`.]{.addu}|
:::

## Feature-test Macro

Since the chrono formatters were still handled by `__cpp_lib_format`, let's just bump that one in [version.syn]{.sref}:

::: std
```diff
- #define __cpp_lib_format 202311L // also in <format>
+ #define __cpp_lib_format 2024XXL // also in <format>
```
:::

