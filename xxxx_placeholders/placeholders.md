---
title: "Exploring Placeholders"
document: DxxxxR0
date: today
audience: EWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Placeholders in Pipelines

[@P2011R0] proposed a new, non-overloadable binary operator `|>` such that the expression

::: bq
```cpp
x |> f(y)
```
:::

was defined to be evaluated as:

::: bq
```cpp
f(x, y)
```
:::

without any intermediate `f(y)` expression. The rules of this operator were fairly simple: the right-hand side had to be a call expression, and the left-hand expression was inserted as the first argument into the right-hand call. 

[@P2011R1] introduced the possibility of a different variant of this idea, where instead of unconditionally inserting the left-hand expression as the first argument of the right-hand call expression, the user would specify _where_ it was inserted into with a placeholder. For the purposes of this paper, I will use `$` as the placeholder. As such

::: bq
```cpp
x |> f($, y)
```
:::

was defined to be evaluated as:

::: bq
```cpp
f(x, y)
```
:::

while also allowing

::: bq
```cpp
x |> f(y, $)
```
:::

to be evaluated as

::: bq
```cpp
f(y, x)
```
:::

In a telecon [@pipeline-minutes], EWG indicated preference for exploring the user of placeholders in pipelining:

::: bq
We're generally interested in some pipeline operator, assuming my preferred placeholder syntax and precedence is chosen.

|SF|F|N|A|SA|
|-|-|-|-|-|
|11|8|3|2|0|

Pipeline operator should have a placeholder, with design to be determined.

|SF|F|N|A|SA|
|-|-|-|-|-|
|6|12|4|1|1|

Pipeline operator should not have a placeholder.

|SF|F|N|A|SA|
|-|-|-|-|-|
|1|3|10|6|2|
:::

The advantage of placeholders in pipelines is significantly added flexibility: you can express any operation that you want to express, regardless of the order of parameters that the function takes. There are numerous examples in the standard library where the most likely candidate for the expression to put on the left-hand side of a pipe expression is not actually the first parameter of the function:

* In Ranges, `r1 |> zip_transform(f, $, r2)`. The function is the first parameter, but range pipelines are going to be built up of ranges.
* Similarly, `some_tuple |> apply(f, $)` and `some_variant |> visit(f, $)` fall into the same boat: the function is the first parameter, but the "subject" of the operation is what you want to put on the left.

Moreover, pipelines with placeholders are even more flexible than that: there is no reason to limit the right-hand side to a call expression. For instance, the expression `(co_await GetString(i)).size()` (from [@P0973R0]) has awkward parenthesization based on operator precedence. This could instead be rewritten as `GetString(i) |> co_await $ |> $.size()`. That is: pipelines with placeholders can effectively introduce postfix `co_await` or `co_yield`. They became a mechanism for controlling operator precedence. 

Obviously there are questions about pipeline placeholders that need to be answered, such as where and how the placeholder needs to appear. Can it appear in an unevaluated operand? In the body of a lambda? More than once?

Put in a pin in those questions for now, and let's just carefully consider the expression:

::: bq
```cpp
std::tuple(1, 2) |> std::apply(std::plus(), $)
```
:::

In particular, what role does the `std::apply(std::plus(), $)` part serve? I don't want to call it a sub-expression since it's not technically a distinct operand here. But conceptually, it at least kind of is. And as such the role that it serves here is quite similar to:

::: bq
```cpp
std::bind(std::apply, std::plus(), _1)(std::tuple(1, 2))
```
:::

With two very notable differences:

* The `bind` expression is limited to solely functions, which the placeholder pipeline is not (as illustrated earlier). But more than that, the `bind` expression is limited to the kinds of functions we can pass as parameters, which `std::apply` (being a function template) is not. 
* The `bind` expression has to capture any additional arguments (the bound arguments) because, as a library facility, it is not knowable when those arguments will actually be used. In this case, capturing `std::plus()` is basically free as it's an empty type anyway. But that could be real cycles that need to be spent. With the placeholder expression, we don't need to capture anything, since we know we're immediately evaluating the expression.

That said, the not-quite-expression `std::apply(std::plus(), $)` is conceptually a lot like a unary function. So what if we keep exploring that route.

# Placeholder Lambdas

There are basically three approaches that languages take to lambdas (some languages do more than one of these). 

1. Direct language support for partial application
2. Placeholder expressions
3. Abbreviated function declarations

I'll illustrate what I mean here by using a simple example: how do you write a lambda that is a unary predicate which checks if its parameter is a negative integer? 

For the languages that support partial application, that looks like:

|Expression|Language|
|-|-|
|`(<0)`|Haskell|
|`(<) 0`|F#|

For the languages that provide abbreviated function declarations, we basically have a section that introduces names followed by a body that uses those names:

|Expression|Language|
|-|-|
|`|e| e < 0`|Rust|
|`e -> e < 0`|Java|
|`e => e < 0`|C#, JavaScript, Scala|
|`\e -> e < 0`|Haskell|
|`{ |e| e < 0 }`|Ruby|
|`{ e in e < 0 }`|Swift|
|`{ e -> e < 0 }`|Kotlin
|`fun e -> e < 0`|F#, OCaml|
|`lambda e: e < 0`|Python|
|`fn e -> e < 0 end`|Elixir|
|`[](int e){ return e < 0; }`|C++|
|`func(e int) bool { return e < 0 }`|Go|

On the plus side, C++ is not the longest.

But the interesting case I wanted to discuss here is those languages that support placeholder expressions:

|Expression|Language|
|-|-|
|`_ < 0`|Scala|
|`#(< % 0)`|Clojure|
|`&(&1 < 0)`|Elixir|
|`{ $0 < 0 }`|Swift|
|`{ it < 0 }`|Kotlin|

There's a lot of variety in these placeholder lambdas. Some languages number their parameters and let you write whatever you want (Swift starts numbering at `0`, Elixir at `1`, Clojure also at `1` but also provides `%` as a shorthand), Kotlin only provides the special variable `it` to be the first parameter.

Scala is unique in that it only provides `_`, but that placeholder refers to a different parameter on each use. So `_ > _` is a binary predicate that checks if the first parameter is greater than the second. 

Now, for this particular example, we also have library solutions available to us, and have for quite some time. There are several libraries *just in Boost* that allow for either `_1 < 0` or `_ < 0` to mean the same thing as illustrated above. Having placeholder lambdas is quite popular precisely because there's no noise; when writing a simple expression, having to deal with the ceremony of introducing names and dealing with the return type is excessive. For instance, to implement [@P2321R1]'s `zip`, you need to dereference all the iterators in a tuple:

::: cmptable
### Regular Lambda
```cpp
tuple_transform([](auto& it) -> decltype(auto) { return *it; }, current_)
```

### Placeholder Lambda
```cpp
tuple_transform(*_, current_)
```
:::

The C++ library solutions are fundamentally limited to operators though. You can make `_1 == 0` work, but you can't really make `_1.id() == 0` or `f(_1)` work. As a language feature, having member functions work is trivial. But having non-member functions work is... not.

In the table of languages that support placeholder lambdas, four of them have _bounded_ expressions: there is punctuation to mark where the lambda expression begins and ends. This is due to a fundamental ambiguity: what does `f($)` mean? It could either mean invoking `f` with a unary lambda that returns its argument (i.e. `f(e => e)`) or it could mean a unary lambda that invokes `f` on its argument (i.e. `e => f(e)`). How do you know which one is which? 

Scala, which does not have bounded placeholder expressions, takes an interesting direction here:

|Expression|Meaning|
|-|-|
|`f(_)`|`e => f(e)`|
|`f(_, x)`|`e => f(e, x)`|
|`f(_ + 2)`|`e => f(e + 2)`|
|`f(1, 2, g(_))`|`f(1, 2, e => g(e))`|
|`f(1, _)`|`e => f(1, e)`|
|`f(1, _ + 1)`|`f(1, e => e + 1)`|

I'm pretty sure we wouldn't want to go that route in C++, where we come up with some rule for what constitutes the bounded expression around the placeholder.

But also more to the point, when the placeholder expression refers to (odr-uses) a variable, we need to capture it, and we need to know _how_ to capture it.

So a C++ approach to placeholder lambdas might be: a *lambda-introducer*, followed by some token to distinguish it from a regular lambda (e.g. `:` or `=>`), followed by a placeholder expression. That is, the placeholder lambda for the negative example might be `[]: $1 < 0` or just `[]: $ < 0`. At 9 characters, this is substantially shorter than the C++ lambda we have to write today (26 characters), and this is about as short as a C++ lambda gets (the dereference example would be 7 characters as compared to 46). And while it's longer than the various library approaches (Boost.HOF's `_ < 0` is just 5), it would have the flexibility to do anything. Probably good enough. 

For more on the topic of placeholder lambdas in C++, consider `vector<bool>`'s blog posts "Now I Am Become Perl" [@vector-bool] and his macro-based solution to the problem [@vector-bool-macro].

Getting back to the topic at hand, we had:

::: bq
```cpp
std::tuple(1, 2) |> std::apply(std::plus(), $)
```
:::

And now we're saying that perhaps:

::: bq
```cpp
auto apply_plus = []: std::apply(std::plus(), $);
```
:::

Could the lambda that does that.

Which actually suggests that the model for pipelines in placeholders might be that `x |> f` simply evaluates as `f(x)`, except that we allow `f` to be a *placeholder-expression* which is allowed to omit the capture introducer (since we know we are not capturing in this context and we have the `|>`s as our bounds to avoid ambiguity). 

This is a pretty cool model (and is in fact how F# defines its `|>`, it's merely inverted function application) in that it's pretty easy to understand while also giving us placeholder lambdas as a drive-by language feature. 

Except...

# March of the Placeholders

One of the library features that the Ranges library provides is the ability to do partial function composition, as a library feature. Let's say, for instance, that we want to send some swag to all of our employees named Tim. We could find their locations like so:

::: bq
```cpp
employees | views::filter([](auto&& e){ return e.name() == "Tim"; })
          | views::transform([](auto&& e) { return e.address(); })
```
:::

But maybe we actually frequently need to locate the Tims. So we can produce a new range adaptor closure object to hold onto and use for future compositions like so:

::: bq
```cpp
auto tim_locations = views::filter([](auto&& e){ return e.name() == "Tim"; })
                   | views::transform([](auto&& e) { return e.address(); })
employees | tim_locations
```
:::

Part of the goal of `|>` as a language feature is to obviate the need for any library `|` machinery. So the question is, how do we do this partial function with `|>`? Let's start with just getting all the Tims. We start out by writing:

::: bq
```cpp
auto tims = [](auto&& r){
    return r |> views::filter($, [](auto&& e){ return e.name() == "Tim"; });
};
```
:::

But this is (a) much longer than the existing `views::filter(/* that lambda */)` while also (b) not being SFINAE-friendly. So that's a no-go. But maybe we need to combine it with the placeholder lambdas, to make both parts of this much shorter:

::: bq
```cpp
auto tims = []: $@~a~@ |> views::filter($@~b~@, []: $@~c~@.name() == "Tim");
```
:::

Alright, what's going on here? We have three uses of `$` here, that actually refer to three different things. I took the liberty of adding subscripts to them to be able to describe what they mean a little better:

* `$@~a~@`: this is the parameter to the lambda that we're declaring. This will be some kind of `viewable_range`, the input to our `filter`.
* `$@~b~@`: this refers to the left-hand side of the `|>` expression, it's a placeholder for the pipeline rather than the lambda
* `$@~c~@`: this is the parameter of the predicate being passed to `filter`

Now, `$@~a~@` and `$@~b~@` actually refer to the same expression here (literally: `$@~b~@` simply refers to `$@~a~@`), so this could be reduced to:

::: bq
```cpp
auto tims = []: views::filter($@~ab~@, []: $@~c~@.name() == "Tim");
```
:::

But if we then add the `transform` on top of these to find the locations of all of our Tims, that would end up being:

::: bq
```cpp
auto tims_locations = []: views::filter($@~a~@, []: $@~c~@.name() == "Tim") |> views::transform($@~d~@, []: $@~e~@.address());
```
:::

Where, to add clarity:

* `$@~d~@`: this refers to the left-hand side of the `|>` expression, which is the `filter` view expression
* `$@~e~@`: this is the parameter of the unary projection that we're passing into transform

Now, this is certainly shorter than what we started with and, importantly, avoids quite a bit of library machinery. Doing this entirely in the language doesn't just lead to terser code, it will almost certainly lead to better compile times and certainly lead to improved diagnostics on incorrect usages. 

As described in the original pipeline paper, this also easily extends to arbitrary algorithms without any additional library work:

::: bq
```cpp
auto tims_salaries = []: views::filter($, []: $.name() == "Tim")
                      |> ranges::foldl($, 0.0, std::plus(), []: $.salary())
```
:::

Now, if we adopted placeholder lambdas along the lines presented here, but instead kept the non-placeholder pipelines, the above would look like:

::: bq
```cpp
auto tims_locations = []: $@~a~@ |> views::filter([]: $@~c~@.name() == "Tim") |> views::transform([]: $@~e~@.address());
```
:::

Because in this world, placeholders _only_ appear in placeholder-lambdas (as opposed to also appearing in pipelines), it's *may* be possible to potentially reduce the above further to (since we have no capture and are using no non-member functions, so the bounds are clear):

::: bq
```cpp
auto tims_locations = $@~a~@ |> views::filter($@~c~@.name() == "Tim") |> views::transform($@~e~@.address());
```
:::

The introducer would be impossible to take away if `|>` took a placeholder, since given the code `e |> views::transform($, $.address())`, how do you know that `$.address()` is intended to be a placeholder-lambda by itself, rather than intending to evaluate as `views::transform(e, e.address())` (which is obviously nonsense in this particular example, but could be a perfectly reasonable expression in a different context). 

So the takeaway here is... this is *quite* messy. A pipeline operator with a placeholder is very flexible, and naturally lends itself to being defined in terms of a placeholder lambda. And placeholder lambdas themselves are very useful (the numerous libraries in existence which allow them is testament to that). But somehow, combining a placeholder pipeline with a placeholder lambda leads to code that might be quite a bit worse than if we had the pipeline with no placeholder (as originally proposed) but still adopted placeholder lambdas. 

---
references:
    - id: pipeline-minutes
      citation-label: pipeline-minutes
      title: "P2011R1 Telecon - September 2020"
      author:
        - family: EWG
      issued:
        - year: 2020
      URL: https://wiki.edg.com/bin/view/Wg21summer2020/EWG-P2011R1-10-Sep-2020
    - id: vector-bool
      citation-label: vector-bool
      title: "Now I Am Become Perl"
      author:
        - family: Colby Pike
      issued:
        - year: 2018
      URL: https://vector-of-bool.github.io/2018/10/31/become-perl.html
    - id: vector-bool-macro
      citation-label: vector-bool-macro
      title: "A Macro-Based Terse Lambda Expression"
      author:
        - family: Colby Pike
      issued:
        - year: 2021
      URL: https://vector-of-bool.github.io/2021/04/20/terse-lambda-macro.html
---