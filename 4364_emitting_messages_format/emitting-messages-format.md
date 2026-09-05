---
title: "Emitting messages at compile time with `std::format`"
document: P4364R0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
status: progress
---

# Introduction

[@P2758R5]{.title} is still working its way through the committee, but in the meantime [@P3391R2]{.title} has been adopted for C++26. Which means that the former paper can continue to just provide the lower level interface that accepts `std::string_view` while this paper can provide the more complex formatting API that is built on top of `std::format`.

# Proposal

As suggested in the other paper, I'm proposing this API:

::: std
```cpp
template <class... Args>
constexpr void constexpr_print(format_string<Args...> fmt, Args&&... args);

template <class... Args>
constexpr void constexpr_print($tag-string$ tag, format_string<Args...> fmt, Args&&... args);

template <class... Args>
constexpr void constexpr_warning($tag-string$ tag, format_string<Args...> fmt, Args&&... args);

template <class... Args>
constexpr void constexpr_error($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
```
:::

Note that there are lower-level facilities proposed in [@P2758R5] that accept `u8string_view`, but there are no formatting functions that return `u8string`.

The implementations of each of these functions is trivial. For instance, the last one of these is:

::: std
```cpp
template <class... Args>
constexpr void constexpr_error($tag-string$ tag, format_string<Args...> fmt, Args&&... args) {
  if consteval {
    constexpr_error_str(tag, format(fmt, FWD(args)...));
  }
}
```
:::

Given that these are higher level facilities that depend on `std::format`, and one of them literally has `print` in the name, I'm proposing to put them in `<print>`.

# Wording

Add to the `<print>` synopsis in [print.syn]{.sref}:

::: {.std .wording}
```diff
namespace std {
  // [print.fun], print functions
  template<class... Args>
    void print(format_string<Args...> fmt, Args&&... args);
  template<class... Args>
    void print(FILE* stream, format_string<Args...> fmt, Args&&... args);

  template<class... Args>
    void println(format_string<Args...> fmt, Args&&... args);
  void println();
  template<class... Args>
    void println(FILE* stream, format_string<Args...> fmt, Args&&... args);
  void println(FILE* stream);

  void vprint_unicode(string_view fmt, format_args args);
  void vprint_unicode(FILE* stream, string_view fmt, format_args args);
  void vprint_unicode_buffered(FILE* stream, string_view fmt, format_args args);

  void vprint_nonunicode(string_view fmt, format_args args);
  void vprint_nonunicode(FILE* stream, string_view fmt, format_args args);
  void vprint_nonunicode_buffered(FILE* stream, string_view fmt, format_args args);

+ // [print.constexpr], emitting messages during program translation
+ template <class... Args>
+   constexpr void constexpr_print(format_string<Args...> fmt, Args&&... args);
+
+  template <class... Args>
+    constexpr void constexpr_print($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
+
+  template <class... Args>
+    constexpr void constexpr_warning($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
+
+  template <class... Args>
+    constexpr void constexpr_error($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
}
```
:::

Add a new subclause after [print.fun]{.sref} called [print.constexpr] "Emitting messages during program translation":

::: {.std .wording}
::: addu
[1]{.pnum} The facilities in this subclause are used to emit messages during program translation. The ordering and number of diagnostics emitted by the functions defined in this subclause are unspecified.

```cpp
template <class... Args>
constexpr void constexpr_print(format_string<Args...> fmt, Args&&... args);
```

[#]{.pnum} *Effects*: During constant evaluation, equivalent to `constexpr_print_str(format(fmt, std::forward<Args>(args)...))`. Otherwise, no effect.

```cpp
template <class... Args>
constexpr void constexpr_print($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
```

[#]{.pnum} *Effects*: During constant evaluation, equivalent to `constexpr_print_str(tag, format(fmt, std::forward<Args>(args)...))`. Otherwise, no effect.

```cpp
template <class... Args>
constexpr void constexpr_warning($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
```

[#]{.pnum} *Effects*: During constant evaluation, equivalent to `constexpr_warning_str(tag, format(fmt, std::forward<Args>(args)...))`. Otherwise, no effect.

```cpp
template <class... Args>
constexpr void constexpr_error($tag-string$ tag, format_string<Args...> fmt, Args&&... args);
```

[#]{.pnum} *Effects*: During constant evaluation, equivalent to `constexpr_error_str(tag, format(fmt, std::forward<Args>(args)...))`. Otherwise, no effect.
:::
:::

## Feature-test Macro

Bump the value of `__cpp_lib_compile_time_messages` in [version.syn]{.sref}.