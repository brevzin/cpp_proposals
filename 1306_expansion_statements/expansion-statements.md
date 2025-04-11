---
title: "Expansion Statements"
document: P1306R4
date: today
audience: CWG
author:
    - name: Dan Katz
      email: <dkatz85@bloomberg.net>
    - name: And Others
toc: true
---

# Revision History

This revision: Rewrote the prose.

[@P1306R3] Expansion over a range requires a constant expression. Added support for break and continue
control flow during evaluation.

[@P1306R2] Adoption of `template for` syntax. Added support for init-statement, folded pack expansion into
new expansion-init-list mechanism. Updated reflection code to match P2996. Minor updates to wording:
updated handling of switch statements, work around lack of general non-transient constexpr
allocation, eliminated need for definition of an "intervening statement", rebased onto working draft,
updated feature macro value, fixed typos. Addressed CWG review feedback

[@P1306R1] Adopted a unified syntax for different forms of expansion statements. Further refinement of semantics
to ensure expansion can be supported for all traversable sequences, including ranges of input iterators.
Added discussion about `break` and `continue` within expansions.

[@P1306R0] superceded and extended [@P0589R0]{.title} to work with more destructurable
objects (e.g., classes, parameter packs). Added a separate constexpr-for variant that a) makes the loop
variable a constant expression in each repeated expansion, and b) makes it possible to expand constexpr
ranges. The latter feature is particularly important for static reflection.

# Introduction

This paper proposes a new kind of statement that enables the compile-time repetition of a statement for
each element of a tuple, array, class, range, or brace-delimited list of expressions. Existing methods for
iterating over a heterogeneous container inevitably leverage recursively instantiated templates to allow
some part of the repeated statement to vary (e.g., by type or constant) in each instantiation.
While such behavior can be encapsulated in a single library operation (e.g., Boost.Hana’s `for_each`) or,
potentially in the future, using the `[:expand(...):]` construct built on top of [@P2996R10]{.title} reflection facilities,
there are several reasons to prefer language support:

First, repetition is a fundamental building block of algorithms, and should be expressible directly without complex template instantiation strategies.

Second, such repetition should be as inexpensive as possible. Recursively instantiating templates generates a large
number of specializations, which can consume significant compilation time and memory resources.

Third, library-based approaches rely on placing the repeated statements in a lambda body, which changes the
semantics of something like a `return` statement — and makes coroutines unusable.

Lastly, "iteration" over destructurable classes effectively requires language support to implement correctly.

Here are some basic usage examples:

::: cmptable
### Today
```cpp
void print_all(std::tuple<int, char> xs) {
  hana::for_each(xs, [&](auto elem){
    std::println("{}", elem);
  });
}
```

### Proposed
```cpp
void print_all(std::tuple<int, char> xs) {
  template for (auto elem : xs) {
    std::println("{}", elem);
  }
}
```

---

```cpp
template <class... Ts>
void print_all(Ts... xs) {
  hana::for_each(std::tie(xs...), [&](auto elem){
    std::println("{}", elem);
  });
}
```

```cpp
template <class... Ts>
void print_all(Ts... xs) {
  template for (auto elem : {xs...}) {
    std::println("{}", elem);
  }
}
```

---

```cpp
template <class T>
void print_all(T const& v) {
  [: expand(nsdms(^^T)) :] >> [&]<auto e>{
    std::println(".{}={}", identifier_of(e), v.[:e:]);
  };
}
```

```cpp
template <class T>
void print_all(T const& v) {
  template for (constexpr auto e :
                define_static_array(nsdms(^^T))) {
    std::println(".{}={}", identifier_of(e), v.[:e:]);
  }
}
```
:::

For the last row, `expand` is demonstrated in [@P2996R10], `define_static_array()` comes from [@P3491R1]{.title} (although can be implemented purely on top of p2996) and works around non-transient allocation (more on this later), and `nsdms(type)` is just shorthand for `nonstatic_data_members_of(type, std::meta::access::unprivileged())` just to help fit.

# Design

The proposed design allows iterating over:

* expression lists,
* anything destructurable via structured bindings (i.e. tuple-like), or
* ranges with compile-time size

The expansion statement

::: std
```cpp
template for ($init-statement$@~opt~@ $for-range-declaration$ : $expansion-initializer$) $statement$
```
:::

will determine an _expansion size_ based on the `$expansion-initializer$` and then _expand_ into:

::: std
```cpp
{
    $init-statement$@~opt~@
    $additional-expansion-declarations$@~opt~@; // depends on expansion kind

    {
        $for-range-declaration$ = $get-expr$(0);
        $statement$
    }

    {
        $for-range-declaration$ = $get-expr$(1);
        $statement$
    }

    // ... repeated up to ...

    {
        $for-range-declaration$ = $get-expr$($expansion-size$ - 1);
        $statement$
    }

}
```
:::


The mechanism of determining the `$additional-expansion-declarations$` (if any), the expansion size, and `$get-expr$` depends on the `$expansion-initializer$`.

## Expansion over Expression Lists

If `$expansion-initializer$` is of the form `{ $expression-list$ }`, then:

  * there are no `$additional-expansion-declarations$`
  * the expansion size is the number of `$expression$`s in the `$expression-list$` (possibly 0), and
  * `$get-expr$(i)` is the `i`th `$expression$` in the `$expression-list$`.

For example:

::: cmptable
### Code
```cpp
template <typename... Ts>
void print_all(Ts... elems) {
  template for (auto elem : {elems...}) {
    std::println("{}", elem);
  }
}
```

### Expands Into
```cpp
template <typename... Ts>
void print_all(Ts... elems) {
  {
    {
      auto elem = elems...[0];
      std::println("{}", elem);
    }

    {
      auto elem = elems...[1];
      std::println("{}", elem);
    }

    {
      auto elem = elems...[2];
      std::println("{}", elem);
    }
  }
}
```
:::

Approximately anyway. The `$expression-list$` need not be a simple pack expansion for which pack indexing applies, that's just for illustration purposes.

An earlier revision of this paper did not have dedicated syntax for expansion over packs. The syntax for the above example was originally proposed as:

::: std
```cpp
template <typename... Ts>
void print_all(Ts... elems) {
  template for (auto elem : elems) { // just elems
    std::println("{}", elem);
  }
}
```
:::

This was pointed out by Richard Smith to be ambiguous [on the EWG reflector](https://lists.isocpp.org/ext/2019/07/10770.php). Consider:

::: std
```cpp
template <typename... Ts>
void fn(Ts... vs) {
  ([&](auto p){
    template for (auto& v : vs) {
      // ...
    }
  }(vs), ...);
}
```
:::

Consider the call `fn(array{1, 2, 3, 4}, array{1, 3, 5, 7}, array{2, 4, 6, 8})`. It is far from clear whether the expansion statement containing `vs` expands over:

* each of the three `array` arguments (once for each invocation of the lambda), or
* each of the four `int` elements (of a different `array` for each invocation of the lambda).

Initially, support for pack iteration was dropped from the proposal entirely, but it was added back using the `$expansion-init-list$` syntax in [@P1306R2].

In addition to avoiding ambiguity, it is also broadly more useful than simply expanding over a pack since it allows ad hoc expressions. For instance, can add prefixes, suffixes, or even multiple packs: `{0, xs..., 1, ys..., 2}` is totally fine.

## Expansion over Ranges

If `$expansion-initializer$` is a single expression that is a range, then:

  * `$addition-expansion-declarations$` is:

    ```cpp
    constexpr@~opt~@ auto&& $__range$ = $expansion-initializer$;
    constexpr@~opt~@ auto $__begin$ = $begin-expr$; // see [stmt.ranged]
    ```

    where the `constexpr` specifier is present when the `$for-range-declaration$` is declared with `constexpr`.

  * the expansion size is `$end-expr$ - $__begin$`. This expression must be a constant expression. It is possible for this to be the case even if `$__begin$` is not `constexpr`, but expansion statements over ranges in general are really only useful if the loop element is `constexpr`.
  * `$get-expr$(i)` is `*($__begin$ + i)`.

For example:

::: cmptable
### Code
```cpp
void f() {
  template for (constexpr int I : std::array{1, 2, 3}) {
    static_assert(I < 4);
  }
}
```

### Expands Into
```cpp
void f() {
  {
    constexpr auto&& $__range$ = std::array{1, 2, 3};
    constexpr auto $__begin$ = $__range$.begin();
    constexpr auto $__expansion-size$ = $__range$.end() - $__begin$; // 3

    {
      constexpr int I = *($__begin$ + 0);
      static_assert(I < 4);
    }

    {
      constexpr int I = *($__begin$ + 1);
      static_assert(I < 4);
    }

    {
      constexpr int I = *($__begin$ + 2);
      static_assert(I < 4);
    }
  }
}
```
:::

Note that the `$__range$` variable is declared `constexpr` here. As such, all the usual rules for `constexpr` variables apply. Including the restriction on non-transient allocation.

Consider:

::: std
```cpp
template <typename T>
void print_members(T const& v) {
    template for (constexpr auto r : nonstatic_data_members_of(^^T)) {
      std::println(".{}={}", identifier_of(r), v.[:r:]);
    }
}
```
:::

Examples like this feature prominently in [@P2996R10]. And at first glance, this seems fine. The compiler knows the length of the vector returned by `members_of(^^T)`, and can expand the body for each element. However, the expansion in question
more or less requires a constexpr vector, which the language is not yet equipped to handle.

We at first attempted to carve out a narrow exception from [expr.const] to permit non-transient constexpr
allocation in this very limited circumstance. Although the wording seemed reasonable, our
implementation experience with Clang left us less than optimistic for this approach: The architecture of
Clang's constant evaluator really does make every effort to prevent dynamic allocations from surviving
the evaluation of a constant expression (certainly necessary to produce a "`constexpr vector`"). After
some wacky experiments that amounted to trying to "rip the constant evaluator in half" (i.e., separating
the "evaluation state", whereby dynamically allocated values are stored, from the rest of the metadata
pertaining to an evaluation), we decided to fold: as of the [@P1306R3] revision, we instead propose restricting
expansion over iterable expressions to only cover those that are constant expression.

In other words — the desugaring described above (which is similar to the desugaring for the C++11 range-based `for` statement) — is what you get. No special cases.

Regrettably, this makes directly expanding over `members_of(^^T)` ill-formed for C++26 – but all is
not lost: By composing `members_of` with the `define_static_array` function from [@P3491R1]{.title}
we obtain a `constexpr` `span` containing the same reflections from `members_of`:

::: std
```cpp
template <typename T>
void print_members(T const& v) {
    template for (constexpr auto r : define_static_array(nonstatic_data_members_of(^^T))) {
      std::println(".{}={}", identifier_of(r), v.[:r:]);
    }
}
```
:::

This works fine, since we no longer require non-transient allocation. We're good to go.

This yields the same expressive power, at the cost of a few extra characters and a bit more memory that
must be persisted during compilation. It's a much better workaround than others we have tried (e.g., the
expand template), and if (when?) WG21 figures out how to support non-transient constexpr allocation,
the original syntax should be able to "just work".

## Expansion over Tuples

If `$expansion-initializer$` is a single expression that is a range, then:

  * the expansion size is the _structured binding size_ of the `$expansion-initializer$` ([dcl.struct.bind]{.sref})
  * `$addition-expansion-declarations$` is:

    ```cpp
    constexpr@~opt~@ auto&& [$__v$@~0~@, $__v$@~0~@, ..., $__v$@~expansion_size-1~@] = $expansion-initializer$;
    ```

  * `$get-expr$(i)` is `$__v$@~i~@` if either the referenced type is an lvalue reference or the `$expansion-initializer$` is an lvalue. Otherwise, `std::move($__v$@~i~@)`.


For example:

::: cmptable
### Code
```cpp
auto tup = std::make_tuple(0, 'a');
template for (auto& elem : tup) {
  elem += 1;
}
```

### Desugars Into
```cpp
auto tup = std::make_tuple(0, 'a');
{
  auto&& [$__v$@~0~@, $__v$@~1~@] = tup;

  {
    auto& elem = $__v$@~0~@;
    elem += 1;
  }

  {
    auto& elem = $__v$@~1~@;
    elem += 1;
  }
}
```
:::

## Prioritizing Range over Tuple

Most types can either be used as a range or destructured, but not both. And even some that can be used in both contexts have equivalent meaning in both — C arrays and `std::array`.

However, it is possible to have types that have different meanings with either interpretation. That means that, for a given type, we have to pick one interpretation. Which should we pick?

One such example is `std::ranges::subrange(first, last)`. This could be:

* as a range, from `[first, last)`.
* as a tuple, specifically the iterators `first` and `last` (i.e. always size 2).

Another such example is a range type that just happens to have all public members. `std::views::empty<T>` isn't going to have any non-static data members at all, so it's tuple-like (with size 0) and also a range (with size 0), so that one amusingly works out the same either way.

But any other range whose members happen to be public probably wants to be interpreted as a range. Moreover, the structured binding rule doesn't actually require _public_ members, just _accessible_ ones. So there are some types that might be only ranges externally but could be both ranges and tuples internally.

In all of these cases, it seems like the obviously desired interpretation is as a range. Which is why we give priority to the range interpretation over the tuple interpretation.

Additionally, given a type that can be interpreted both ways, it easy enough to force the tuple interpretation if so desired:

::: std
```cpp
template <class T>
constexpr auto into_tuple(T const& v) {
    auto [...parts] = v;
    return std::tie(parts...);
}
```
:::


## `break` and `continue`

Earlier revisions of the paper did not support `break` or `continue` within expansion statements. There was previously concern that users would expect such statement to exercise control over the code generation / expansion process at translation time, rather than over the evaluation of the statement.

Discussions with others have convinced us that this will not be an issue, and to give the keywords their most obvious meaning: `break` jumps to just after the end of the last expansion, whereas `continue` jumps to the start of the next
expansion (if any).

## Expansion over Types

There are regular requests to support expanding over _types_ directly, rather than expressions:

::: std
```cpp
template <typename... Ts>
void f() {
    // strawman syntax
    template for (typename T : {Ts...}) {
        do_something<T>();
    }
}
```
:::

Something like this would be difficult to support directly since you can't tell that the declaration is just a type rather than an unnamed variable. But with Reflection coming, there's less motivation to come up with a way to address this problem directly since we can just iterate in the value domain:

::: std
```cpp
template <typename... Ts>
void f() {
    template for (constexpr auto r : {^^Ts...}) {
        using T = [:r:];
        do_something<T>();
    }
}
```
:::

## Implementation experience

Bloomberg's Clang/P2996 fork (available [on Godbolt](https://godbolt.org/z/Yjv1dM4eK)) implements all features proposed by this paper. Expansion statements are enabled with the `-fexpansion-statements` flag (or with `-freflection-latest`).


# Proposed wording

## [basic.scope.pdecl] Point of declaration {-}

Update paragraph 11 to specify the locus of an expansion statement:

::: std
[11]{.pnum} The locus of a `$for-range-declaration$` of a range-based `for` statement ([stmt.range]) [or expansion statement ([stmt.expand])]{.addu} is immediately after the `$for-range-initializer$` [or `$expansion-initializer$`]{.addu}.

:::

## [basic.scope.block] Block scope {-}

Update paragraph 1.1 to include expansion statements:

::: std
- [1.1]{.pnum} selection[,]{.addu} [or]{.rm} iteration[, or expansion]{.addu} statement ([stmt.select], [stmt.iter] [, [stmt.expand]]{.addu})

:::

## [class.temporary] Temporary objects {-}

Update paragraph 7 to extend the lifetime of temporaries created by `$expansion-initializer$`s:

::: std
[7]{.pnum} The fourth context is when a temporary object other than a function parameter object is created in the `$for-range-initializer$` of a range-based `for` statement [or the `$expansion-initializer$` of an expansion statement]{.addu}. If such a temporary object would otherwise be destroyed at the end of the `$for-range-initializer$` or `$expansion-initializer$` full-expression, the object persists for the lifetime of the reference initialized by the `$for-range-initializer$` [or for the evaluation of the `$expansion-statement$`]{.addu}.

:::

## [stmt.pre] Preamble {-}

Add a production for expansion statements to `$statement$`:

::: std
[1]{.pnum} Except as indicated, statements are executed in sequence.

```diff
  $statement$:
      $labeled-statement$
      $attribute-specifier-seq$@~_opt_~@ $expression-statement$
      $attribute-specifier-seq$@~_opt_~@ $compound-statement$
      $attribute-specifier-seq$@~_opt_~@ $selection-statement$
      $attribute-specifier-seq$@~_opt_~@ $iteration-statement$
+     $attribute-specifier-seq$@~_opt_~@ $expansion-statement$
      $attribute-specifier-seq$@~_opt_~@ $jump-statement$
      $declaration-statement$
      $attribute-specifier-seq$@~_opt_~@ $try-block$
```
:::

Extend "substatement" to cover expansion statements in paragraph 2:

::: std
[2]{.pnum} A _substatement_ of a `$statement$` is one of the following:

- [#.#]{.pnum} for a `$labeled-statement$`, its `$statement$`,
- [#.#]{.pnum} for a `$compound-statement$`, any `$statement$` of its `$statement-seq$`,
- [#.#]{.pnum} for a `$selection-statement$`, any of its `$statement$`s or `$compound-statement$`s (but not its `$init-statement$`), [or]{.rm}
- [#.#]{.pnum} for an `$iteration-statement$`, its `$statement$` (but not an `$init-statement$`)[.]{.rm}[, or]{.addu}
- [[#.#]{.pnum} for an `$expansion-statement$`, its `$statement$` (but not an `$init-statement$`).]{.addu}

:::

Extend "enclose" to cover expansion statements in paragraph 3:

::: std
[3]{.pnum} A `$statement$` `S1` _encloses_ a `$statement$` `S2` if

- [#.#]{.pnum} `S2` is a substatement of `S1`,
- [#.#]{.pnum} `S1` is a `$selection-statement$`[,]{.addu} [or]{.rm} `$iteration-statement$`[, or `$expansion-statement$`]{.addu} and `S2` is the `$init-statement$` of `S1`,
- [#.#]{.pnum} [...]

:::

## [stmt.label] Labeled statement {-}

Add a new paragraph to the end of [stmt.label]:

::: std
[[4]{.pnum} An identifier label shall not occur in an `$expansion-statement$` ([stmt.expand]).]{.addu}

:::

## [stmt.ranged] The range-based `for` statement {-}

Add the following paragraph to the end of [stmt.ranged]:

::: std
[[3]{.pnum} An expression is _iterable_ if, when treated as a `$for-range-initializer$` ([stmt.iter.general]), expressions `$begin-expr$` and `$end-expr$` can be determined as specified above, and if they are of the form `begin($range$)` and `end($range$)` then argument-dependent lookup finds at least one function or function template for each.]{.addu}

:::

## 8.6+ [stmt.expand] Expansion statements

Insert this section after [stmt.iter]{.sref} (and renumber accordingly).

::: std
::: addu
**Expansion statements   [stmt.expand]**

[1]{.pnum} Expansion statements specify compile-time repetition, with substitutions, of their substatement.

```
$expansion-statement$:
    template for ( $init-statement$@~_opt_~@ $for-range-declaration$ : $expansion-initializer$ ) $statement$

$expansion-initializer$:
    $expression$
    $expansion-init-list$

$expansion-init-list$:
    { $expression-list$ }
```

[An `$init-statement$` ends with a semicolon.]{.note}

[#]{.pnum} The `$statement$` of an `$expansion-statement$` is a control-flow-limited statement ([stmt.label]).

[#]{.pnum} Each `$decl-specifier$` in the `$decl-specifier-seq$` of a `$for-range-declaration$` in an `$expansion-statement$` shall be either a `$type-specifier$` or `constexpr`.

[#]{.pnum} If the `$expansion-initializer$` is iterable ([stmt.ranged]), the type of the `$initializer$` for the `$for-range-declaration$` is `decltype(*I)` where `I` is an iterator into the range. Otherwise, if the `$expansion-initializer$` is not an `$expansion-init-list$`, then it shall be destructurable expression ([dcl.struct.bind]). [The name declared by a `$for-range-declaration$` for an `$expansion-statement$` is value-dependent ([temp.dep.constexpr]) if the `$expansion-initializer$` is iterable, and is otherwise type-dependent ([temp.dep.expr]) if declared wiht a placeholder type.]{.note}

[#]{.pnum} For the purpose of name lookup and instantiation, the `$for-range-declaration$` and the `$statement$` of the `$expansion-statement$` are together considered a template definition.

[#]{.pnum} An `$expansion-statement$` is _expanded_ (as described below) if its `$expansion-initializer$` is neither type-dependent nor a value-dependent iterable expression. Expansion entails the repetition of a statement for each element of the `$expansion-initializer$`. Each repetition is called an _expansion_ and is an instantiation ([temp.spec]) of the `$for-range-declaration$` (including its implied initialization) together with the `$statement$`.

[#]{.pnum} If the `$expansion-initializer$` is an `$expansion-init-list$`, the `$expansion-statement$` is expanded once for each expression in the contained `$expression-list$`; the expansion is equivalent to:

```cpp
{
  $init-statement$
  {  // @i^th^@ repetition of the substatement
    $for-range-declaration$ = $get-expr$@~_i_~@ ;
    $statement$
  }
}
```
where `$get-expr$@~_i_~@` is the _i_^th^ `$expression$` in the `$expansion-init-list$`.

[#]{.pnum} Otherwise, if the `$expansion-initializer$` is an iterable expression ([stmt.ranged]) and the `$expansion-initializer$` does not have array type, the `$expansion-statement$` is expanded once for each element in the range computed by the `$expansion-initializer$`; the expansion is equivalent to:
```cpp
{
  $init-statement$
  static constexpr auto&& range = $expansion-initializer$ ;
  // @i^th^@ repetition of the substatement
  static constexpr auto iter@~_i_~@ = $get-expr$@~_i_~@ ;
  {
    $for-range-declaration$ = *iter@~_i_~@ ;
    $statement$
  }
}
```
where `$get-expr$@~_0_~@` is the expression:
```cpp
begin($range$)
```
and every subsequent `$get-expr$@~_i_~@` is the expression:
```cpp
$get-expr$@~_i-1_~@ + 1
```
for all `i` in the range [0, _N_], for _N_ such that:
```cpp
$get-expr$@~_N_~@ == $end-expr$
```
where `$end-expr$` is the expression:
```cpp
end($range$) @.@
```

[The expansion is ill-formed if `range` is not a constant expression ([expr.const])]{.note}

[9]{.pnum} Otherwise, the `$expansion-initializer$` shall be destructurable. The `$expansion-statement$` is expanded once for each element of the `$identifier-list$` of a structured binding declaration of the form
```cpp
auto&& [u@~1~@, u@~2~@, ..., u@~_n_~@] = $expansion-initializer$ ;
```
where _n_ is the number of elements required in a valid `$identifier-list$` for such a structured binding declaration. Each expansion is equivalent to:
```cpp
{
  $init-statement$
  static $constexpr-specifier$@~_opt_~@ auto&& $seq$ = $expansion-initializer$ ;
  {  // @i^th^@ repetition of the substatement
    $for-range-declaration$ = $get-expr$@~_i_~@ ;
    $statement$
  }
}
```
where `$get-expr$@~_i_~@` is the `$initializer$` for the `$i$@^th^@` `$identifier$` in the corresponding structured binding declaration. The `$constexpr-specifier$` is present in the declaration of `$seq$` if `constexpr` appears in the `$for-range-declaration$`. The name `$seq$` is used for exposition only.

::: example
```cpp
struct S { int i; short s; };
consteval long f(S s) {
  long result = 0;
  template for (auto x : s) {
    result += x;
  }
  return result;
}
static_assert(f(S{1, 2}) == 3);
```
:::

::: example
```cpp
consteval int f(auto const&... Containers) {
  int result = 0;
  template for (auto const& c : {Containers...}) {
    result += c[0];
  }
  return result;
}
constexpr int c1[] = {1, 2, 3};
constexpr int c2[] = {4, 3, 2, 1};
static_assert(f(c1, c2) == 5);
```
:::

[The following example assumes the changes proposed by P2996R11 and P3491.]{.ednote}

::: example
```cpp
template <typename T> consteval std::optional<int> f() {
  constexpr auto statics = std::define_static_array(
      std::meta::static_data_members_of(
          ^^T,
          std::meta::access_context::current()));
  template for (constexpr std::meta::info s : statics)
    if (std::meta::identifier_of(s) == "ClsId")
      return [:s:];
  return std::nullopt;
}
struct Cls { static constexpr int ClsId == 14; };
static_assert(f<Cls>().value() == 14);
```
:::

:::
:::

## [stmt.break] The `break` statement {-}

Modify paragraph 1 to allow `break` in expansion statements:

::: std
[1]{.pnum} A `break` statement shall be enclosed by ([stmt.pre]) an `$iteration-statement$` ([stmt.iter])[, an `$expansion-statement$` ([stmt.expand]),]{.addu} or a `switch` statement ([stmt.switch]). The `break` statement causes termination of the smallest such enclosing statement; control passes to the statement following the terminated statement, if any.

:::

## [stmt.cont] The `continue` statement

[We recommend the phrase "continuation portion" in lieu of "loop-continuation portion" to emphasize that an expansion statement is not a loop.]{.ednote}

Modify paragraph 1 to allow `continue` in expansion statements:

::: std

[1]{.pnum} A `continue` statement shall be enclosed by ([stmt.pre]) an `$iteration-statement$` ([stmt.iter]) [or an `$expansion-statement$` ([stmt.expand])]{.addu}. The `continue` statement causes control to pass to the [loop]{.rm} continuation portion of the smallest such enclosing statement, that is, to the end of the loop [or expansion]{.addu}. More precisely, in each of the statements
<table><tr>

<td>
```cpp
while (foo) {
  {
    // ...
  }
contin: ;
}
```
</td>

<td>
```cpp
do {
  {
    // ...
  }
contin: ;
} while (foo);
```
</td>

<td>
```cpp
for (;;) {
  {
    // ...
  }
contin: ;
}
```
</td>

<td>
::: addu
```cpp
template for (auto e : foo) {
  {
    // ...
  }
contin: ;
}
```
:::
</td>

</tr></table>
a `continue` not contained in an enclosing iteration [or expansion]{.addu} statement is equivalent to  `goto contin`.

:::

## [dcl.struct.bind] Structured binding declarations

Add the following paragraph to the end of [dcl.struct.bind]:

::: std
::: addu
[9]{.pnum} An expression `$E$` is _destructurable_ if the declaration
```cpp
auto [$identifier-list$] = $E$;
```
is valid for some `$identifier-list$`.

:::
:::

## [dcl.attr.fallthrough] Fallthrough attribute

Update paragraph 1 to discuss expansion statements:

::: std
[1]{.pnum} The `$attribute-token$` `fallthrough` may be applied to a null statement; such a statement is a fallthrough statement. No `$attribute-argument-clause$` shall be present. A fallthrough statement may only appear within an enclosing `switch` statement ([stmt.switch]). The next statement that would be executed after a fallthrough statement shall be a labeled statement whose label is a case label or default label for the same `switch` statement and, if the fallthrough statement is contained in an iteration statement [or expansion statement]{.addu}, the next statement shall be part of the same execution of the substatement of the innermost enclosing iteration statement [or the same expansion of the innermost enclosing expansion statement]{.addu}. The program is ill-formed if there is no such statement.

:::

## [temp.res.general] General {-}

Update paragraph 6.1 to permit early checking of expansion statements in dependent contexts.

::: std
[6]{.pnum} The validity of a templated entity may be checked prior to any instantiation.

[Knowing which names are type names allows teh syntax of every template to be checked in this way.]{.note3}

The program is ill-formed, no diagnostic required, if

- [#.#]{.pnum} no valid specialization, ignoring `$static_assert-declaration$`s that fail ([dcl.pre]), can be generated for a templated entity or a substatement of a constexpr if statement ([stmt.if]) [or expansion statement]{.addu} within a templated entity and the innermost enclosing template is not instantiated, or

- [#.#]{.pnum} [...]

:::

## [temp.dep.expr] Type-dependent expressions {-}

Add the following case to paragraph 3 (and renumber accordingly):

::: std
[3]{.pnum} An `$id-expression$` is type-dependent if it is a `$template-id$` that is not a concept-id and is dependent; or if its terminal name is

- [#.#]{.pnum} [...]
- [#.10]{.pnum}  a `$conversion-function-id$` that specifies a dependent type, or
- [[#.10+]{.pnum} a name introduced by a `$for-range-declaration$` that contains a placeholder type and is declared in an `$expansion-statement$` ([stmt.expand]) for which the `$expansion-initializer$` is not an iterable expression ([stmt.ranged]), or]{.addu}
- [#.11]{.pnum} dependent

or if it names [...]
:::

## [temp.dep.constexpr] Value-dependent expressions {-}

Add the following case to paragraph 2 (and renumber accordingly):

::: std
[2]{.pnum} An `$id-expression$` is value-dependent if

- [#.#]{.pnum} [...]
- [#.3]{.pnum} it is the name of a constant template parameter,
- [[#.3+]{.pnum} it is a name introduced by a `$for-range-declaration$` declared in an `$expansion-statement$` for which the `$expansion-initializer$` is an iterable expression,]{.addu}
- [#.4]{.pnum} [...]

:::

## [cpp.predefined] Predefined macro names {-}

Add the following entry to Table 22:

::: std
<center>Table 22 — Feature-test macros [tab:cpp.predefined.ft]</center>

<table>
<tr><td><center>Macro name</center></td><td><center>Value</center></td></tr>
<tr><td><center>...</center></td><td><center>...</center></td></tr>
<tr><td><center>[`__cpp_expansion_statements`]{.addu}</center></td><td><center>[202506L]{.addu}</center></td></tr>
</tr></table>
:::