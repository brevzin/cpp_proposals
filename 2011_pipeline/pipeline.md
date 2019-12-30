---
title: "A pipeline-rewrite operator"
document: D2011R0
date: today
audience: EWG
author:
    - name: Colby Pike
      email: <vectorofbool@gmail.com>
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

# Abstract

This paper proposes a new non-overloadable operator `|>` such that the
expression `x |> f(y)` evaluates exactly as `f(x, y)`. There would be no
intermediate subexpression `f(y)`. 

This is notably unrelated to, and quite different from, [@P1282R0], which
proposed an overloadable `operator|>`, allowing the evaluation of the above
expression as `operator|>(x, f(y))`.

# Introduction and Motivation

While the addition of Ranges into the standard brings many great features and
concepts, the "pipeline" features could use some attention. The current
incarnation of the pipeline functionality brings a few important drawbacks:

1. The necessary amount of supporting code machinery lends itself to high
    amounts of complexity, creating a larger maintenance burden on developers
    who wish to use and write pipeline-style code.
2. The support machinery can incur large amounts of overhead when inlining
    and peephole optimizations are not enabled.
3. The support machinery places additional large burdens on the implementation
    in that it needs to parse and process large amounts of the support code
    that is needed to support the syntax.
4. Adopting the ability to add pipeline support for some algorithms might be
    impossible given fundamental ambiguities.

The goal of the "pipeline-rewrite" operator proposed herein is to solve all of
the above issues, as well as generalize the concept of "pipeline" code to work
with arbitrary functions and types, and not just those that must specifically
request it.

The addition of a "pipeline-rewrite" operator requires no API adjustments to
any existing or proposed libraries in order to support such an operator.

## Pipeline Style

C++ has object-oriented *features*, but it is not itself an object-oriented
*language*. This author attributes much of C++'s success to its ability to
support multiple paradigms simultaneously, lending the benefits of each domain
to developers when appropriate.

Not being solely object-oriented, and with the benefits of generic programming,
we have seen the proliferations of generic algorithms being implemented as
free functions. For example, where many languages have a single type to
represent a "sequence" of values, C++ permits an unlimited number of "sequence"
types tailored to the needs of their respective domain, and the generic
algorithms that operate on them work identically (provided the underlying type
meets the appropriate guarantees). In classical object-oriented languages, the
algorithms are attached to the objects themselves. For example:

```js
// Some JavaScript
const seq = [1, 2, 3, 4];
const twice = seq.map(i => i * 2);
```

Here `map` is a *member* of `seq`, despite the concept of "mapping" being
entirely abstract.

In many languages, when new sequence types are needed, they may be defined, but
can suffer from performance penalties, but even worse: The algorithms are gone!
The algorithm methods need to be re-implemented again on the new types.

The C++ standard library instead opts for generic free functions. These have
great benefits, including supporting containers of disparate types:

```c++
QList<int> integers = get_integers();
std::vector<int> twice;
std::transform(begin(integers), end(integers), back_inserter(twice), double_values);
```

Much of the standard library accepts "iterator pairs" as their representation of
a sequence. This has some benefits, such as the algorithms not needing to
know anything about the underlying container. This also has some drawbacks,
such as algorithms not being able to know anything about the underlying
container.

One of the biggest drawbacks, though, is the simple verbosity. We do not often
write application code dealing strictly with iterator pairs. Instead, we'll be
using actual concrete data structures that expose the iterator pairs that we
hand to algorithms.

Amongst many other things, Ranges defines new overloads for many of the
standard algorithms that accept range types which represent iterator pairs (or
an iterator and a sentinel, but that isn't relevant).

```c++
QList<int> integers = get_integers();
std::vector<int> twice;
ranges::transform(integers, back_inserter(twice), double_values);
```

Another idea introduced by Ranges is the composition of algorithms.

Here, we will borrow one of the most famous examples from the ranges-v3
library: The calendar printing example [@range.calendar].
We will start with a very uglified
version of the example's apex, `format_calendar`:

```c++
template <typename Calendar>
auto format_calendar(size_t months_per_line, Calendar&& cal) {
    // Group the dates by month
    auto months = by_month(cal);
    // Format the months into a range of strings
    auto month_strings = layout_months(months);
    // Group the months that belong side-by-side
    auto chunked_months = chunk(month_strings, months_per_line);
    // Transpose the rows and columns of side-by-side months
    auto transposed = transpose_months(chunked_months);
    // Ungroup the side-by-side months
    auto joined_view = view::join(transposed);
    // Join the strings of the transposed months
    return join_months(joined_view);
}
```

This code is not inscrutable, but it is far from what the original looked like.
We have a several variables that are essentially meaningless, as their names
are tautological to the spelling of their initializing expression. And because
these variables are only used in the immediately following line, we may as well
place each variable's initializer in place of the variable name in the following
call. The result is horrific, to say the least:

```c++
template <typename Calendar>
auto format_calendar(size_t months_per_line, Calendar&& cal) {
    // Join the strings of the transposed months
    return join_months(
        // Ungroup the side-by-side months
        view::join(
            // Transpose the rows and columns of side-by-side months
            transpose_months(
                // Group the months that belong side-by-side
                chunk(
                    // Format the months into a range of strings
                    layout_months(
                        // Group the dates by month
                        by_month(cal)
                    ),
                    months_per_line
                )
            )
        )
    );
}
```

(Our favorite feature of the above horror is the `months_per_line` appearing
quite distant from the function call to which it is an argument.)

While the code is frightening, it is conceptually equivalent to the prior
example. Both of these examples are very dissimilar to the code found in the
range-v3 [@range-v3] example upon which they were based.

Ranges also seeks to tackle the above problem with the idea of pipeable
objects.

Pipeline-style is an increasingly popular way to write code, especially in
functional programming languages. Ranges provides pipeline style via overloading
of the bitwise-or `|` binary operator. In the pipeline style, the value on
the left of the "pipeline" operator is conceptually "fed into" the expression
on the right, where the right-hand-side is some "partial" expression missing
the primary argument on which it operates. The actual example from range-v3
uses this syntax to produce the much more concise and readable pipeline style:

```c++
auto
format_calendar(std::size_t months_per_line)
{
    return make_pipeable([=](auto &&rng) {
        using Rng = decltype(rng);
        return std::forward<Rng>(rng)
               // Group the dates by month:
               | by_month()
               // Format the month into a range of strings:
               | layout_months()
               // Group the months that belong side-by-side:
               | chunk(months_per_line)
               // Transpose the rows and columns of the size-by-side months:
               | transpose_months()
               // Ungroup the side-by-side months:
               | view::join
               // Join the strings of the transposed months:
               | join_months();
    });
}
```

Usage of `format_calendar` also makes use of the "pipeline" syntax:

```c++
copy(dates(start_date, end_state) | format_calendar(3),
     calendar_lines);
```

Where `dates` lazily generates date objects which are fed into the
`format_calendar` algorithm.

Although the above examples use ranges, the pipeline style can be applied to
any type of objects, from integers to strings to database rows.

## Supporting `|` as an Pipeline Operator

How does `|` work in the above examples, and with Ranges in general? After
all, it's just the bitwise-or operator. The "pipeline" semantics aren't built
into the language.

The answer, of course, is to use operator overloading. To support
`transform(rng, projection)` and `rng | transform(projection)`, the
`transform` name does not correspond to a single function. It must instead
name an overload set (or, as with everything in Ranges, a single object with
multiple `operator()`
overloads). The type returned by the two overloads is radically different. The
partially-applied form intended for use with `|` stores its argument in an
object which defines the overloaded `operator|`. If a range is given as the
left-hand operand of the `|` operator, only then is the algorithm
fully-applied and ready to produce results.

Let's go through what the implementation of this machinery looks like so that
we can point out what the many issues with it are.

For the full function call, `transform(rng, projection)`, we can provide complete
constraints. We have all the information we need: we know the range, we know the
function type, and we know that the function type has to be compatible with the
range. With concepts, we just need to write those constraints out:

```cpp
template <std::ranges::viewable_range R,
          std::regular_invocable<std::range_reference_t<R>> F>
auto transform(R&& range, F&& projection)
    -> transform_view<std::ranges::all_view<R>, std::decay_t<F>>
{
    // implementation
}
```

Now the implementation of `transform_view` itself is non-trivial - but its
implementation is unrelated to the topic we're focusing on here.

Now for the partial function call, in order to support `rng | transform(f)` we
need to first support `transform(f)`. This call does not have complete
information - we just have a function, but we cannot check this function all by
itself. We can only check if a type is callable with a specific argument, we
cannot check in general if a type is callable at all. There is no constraint
that we can add on this type at all (outside of it being copyable), so we're
left to just write:

```cpp
#define FWD(x) static_cast<decltype(x)&&>(x)

template <typename F>
auto transform(F&& projection)
{
    return make_left_pipeable(
        [f=std::forward<F>(projection)](std::ranges::viewable_range auto&& r)
            -> decltype(transform(FWD(r), std::ref(f)))
        {
            return transform(FWD(r), std::ref(f));
        });
}
```

This is probably the most concise way to implement partial `transform`: we have
a utility `make_left_pipeable` which takes a lambda and returns a type that, when
left-`|`-ed invokes the lambda. This lambda has to be SFINAE-friendly, and needs
to just forward all of its arguments properly to the complete call so as to
avoid having to duplicate the constraints.

## Problems with `|` as a Pipeline Operator

There are a few facially obvious drawbacks to using `|` for pipeline semantics:

- The code required to support using `|` functionality is not simple. It adds
    overhead during compilation, and without the aide of the inliner and basic
    optimizations it can be expensive at runtime.
- Defining new range algorithms necessitates opting-in to this machinery.
    Existing code cannot make use of pipeline style.
- Supporting both pipeline style and immediate style requires algorithms to
    provide both partial and full algorithm implementations, where the partial
    implementation is mostly boilerplate to generate the partially applied
    closure object.

Those are, in of themselves, pretty bad. But these problems are all just work -
work for the programmer to write the initial `|` support to begin with (just the
one time), work for the programmer to write each additional overload to opt-in,
work for the compiler to compile all these things together, work for the
optimizer to get rid of all these things, work for the programmer to try to
figure out what's getting called where. 

But there's a bigger problem here that no amount increased programmer or compiler
throughput can solve: sometimes the partial and complete calls are ambiguous.

Specific to ranges, the "total call" syntax always takes a range followed by
some amount of helper arguments. In the `transform` case, this is a range
followed by an invocable. The "partial call" syntax drops the range, and takes
the rest of the arguments. What happens, though, when the second argument
can itself be a range? How do you know? There are several examples where
this comes up:

- `zip` is a view that takes a bunch of ranges and turns them into a range of
`tuple`s of references, it's an extremely useful range adaptor - just one that
we won't quite have in C++20. `zip(a)` is itself a valid (if odd) view: just
takes all the elements of a range and wraps them in a `tuple` of a single
element. As a result, you cannot make both `zip(a, b)` and `a | zip(b)` yield
a range of tuples, you have to pick one. range-v3's `zip` does not support the
pipeline syntax.

- `concat` is a view that takes a bunch of ranges and concatenates
them all together in a larger range. Much like `zip`, `concat(a)` is a valid
expression and so `a | concat(b)` can't work. range-v3's `concat` does not
support the pipeline syntax. 

- `transform` actually has two forms: a unary form and a binary form. C++20
adds an algorithm that is `ranges::transform(rng1, rng2, result, binary_op)`.
But `views::transform(r, f)` could be a valid expression, so there may be an
ambiguity with `a | views::transform(b, f)`, if `f` happens to be invocable both
as a unary and as a binary function. range-v3 does not have a binary transform
adapter, only a unary one.

- `views::join` in C++20 takes a range of ranges and collapses it to a single
range, taking no additional arguments. One potentially useful argument it could
take is a delimiter: `join(rng_of_rngs, delim)` could interleave the delimiter
in between each element. But can itself be a range of ranges, then you'll run
into problems with `rng | join(delim)`. range-v3 only considers the pipeable
form for non-joinable-ranges. An alternative approach might have to been to
just name them differently - `join(rng)` is unambiguous with `rng | join` and
`join_with(rng, delim)` is unambiguous with `rng | join_with(delim)`. 

Each of these are specific cases that have to be considered by the library
author, which is just a lot of extra mental overhead to have to deal with. But
it's somehow even worse than that.

In range-v3 and in C++20, the only algorithms that opt-in to the pipeline
syntax are those algorithms that take one or more ranges as input and produce
a [lazy] range as output. There is a whole other class of algorithms that does
not have this particular shape, but still would be quite useful to have pipeline
syntax for. Conor Hoekstra, in his CppNow talk entitled Algorithm Intuition
[@Hoekstra], presented a problem which boiled down to checking if a string
had at least 7 consecutive 0s or 1s. One way to solve that using the pipeline
syntax would be:

```cpp
auto dangerous_teams(std::string const& s) -> bool {
    return s
         | views::group_by(std::equal_to{})
         | views::transform(ranges::distance)
         | ranges::any_of([](std::size_t s){
                return s >= 7;
            });
}
```

At least, it would be a way of solving this problem if the `any_of` algorithm
had opted-in to the pipeline syntax. It does not. And doing so takes work - we
have to go through all of the over 100 algorithms in the standard library and
add these extra partial overloads, specify how all of them work, and then
figure out how to deal with ambiguities. This is a lot of work for a large number
of people, and still wouldn't help solve the problem for any of the algorithms
in user code. And you can still get an ambiguity:

- `accumulate` does not currently have a range-based overload in C++20, but it
needs to take an initial value as the second argument. If that initial argument
is a range that happens to be addable, that's ambiguous. One such type is
`std::string`: is `accumulate(str)` a complete call summing that string, or is
it a partial call simply providing the initial value to the accumulator. You
could work around this - either by not providing a default value for the initial
value, or not providing a default value for the binary operator (or both). But
either way, you need _some_ workaround.

Rather than committing to:

- a lot of committee time to discuss opting in algorithms to the pipeline syntax, and
- a lot of committee time to discuss the right way to handle ambiguities, and
- a lot of library implementer time to implement the ones that we adopt, and
- a lot of compiler time compling code that uses the pipeline syntax, and
- a lot of non-committee time to do the same for their own algorithms, including
having to come up with mechanisms to support the pipeline syntax for algorithms
like `zip` and `concat` too.

We would like to propose something better.

# Proposal: Rewriting Pipelines with a `|>` Operator

We propose a new, non-overloaded function call operator spelled `|>`. What it
does is simply evaluate the _postfix-expression_:

```cpp
x |> f(y)
```

as

```cpp
f(x, y)
```

Without any intermediate operands. That is, it rewrites code written in a
pipeline style into immediate function call style. This rewriting of pipeline-style
is why the name "pipeline-rewrite" was chosen. 

It's important to be clear that the above **does not** evaluate as:

```cpp
operator|>(x, f(y))
```

There is no `f(y)` operand, there is no lookup for `operator|>` (this paper
is proposing that such a declaration be ill-formed - this operator is not
overloadable).

In other words, the following program is valid:

```cpp
constexpr int add(int a, int b) {
    return a + b;
}

static_assert(1 |> add(2) == 3);
```

This is a complete program, no `#include`s or <code>[import]{.kw}</code>s, no additional library
glue necessary to make this work. The assertion directly invokes `add(1, 2)`
just as if the code had been written that way to begin with. Indeed, an
attempt at invoking a unary `add(2)` would be ill-formed!

This is similar to member function call syntax, where `c.f(x)` does not evaluate
as the expression `operator.(c, f(x))` and instead evaluates as something much
closer to `C::f(c, x)`

## Specific Proposal Details

The description above is roughly the entirety of the proposal. We introduce two
new forms of [_postfix-expression_](http://eel.is/c++draft/expr#nt:postfix-expression),
which have the grammar:

::: bq
_postfix-expression_ `|>` _primary-expression_ `(` _expression-list_ ~opt~ `)`  
_postfix-expression_ `|>` _primary-expression_
:::

And evaluate directly as function calls as a result of moving the left-hand-side
as the first argument of the call expression. For those cases where the
left-hand-side is the sole argument, the empty parentheses may be omitted (this
is the second grammar form above).

Some examples:

```cpp
namespace N {
   struct X { int i; };
   int f(X x) { return x.i; }
}

template <typename T>
void f(T);

N::X{1} |> f(); // ADL finds N::f, is 1
N::X{1} |> f;   // as above, parens optional
N::X{1} |> (f); // parens inhibit ADL, so this calls `::f`, is a void

// immediately invokes the lambda with arguments 1 and 2
1 |> [](int i, int j){ return i + j; }(2);

// immediately invokes the lambda with the argument 1
1 |> [](int i){ return i; };

template <typename T>
int g(T);

2 |> g<int>; // equivalent to g<int>(2)

// arbitrary expressions can be composed in parens
template <typename F, typename G>
auto operator>>(F f, G g) {
    return [=](auto arg){
        return g(f(arg));
    };
}

// evaluates as dbl(add1(4))
auto add1 = [](int i){ return i+1; };
auto dbl = [](int i) { return i*2; };
4 |> (add1 >> dbl)();
4 |> (add1 >> dbl)
```

Note that _postfix-expression_ s can chain after each other too, so these
also work:

```cpp
template <typename T>
auto always(T val) {
    return [=](auto&&...){ return val; };
}

// the first ()s are the pipeline rewrite call
// and the second ()s invoke the resulting function
1 |> always()();

namespace N {
    struct X { int i; };
    X add(X x, X y) {
        return X{x.i + y.i};
    }
}

// the |> and . have the same "precedence", so
// this evaluates the |> first and then the .
// on the result of that
int i = N::X{2} |> add(N::X{3}) . i;
```


All of the above works directly out of the box with this proposal.

## Further examples

### Using ``copy``

Let's look at a non-lazy ``copy`` function:

```c++
template <typename Range, typename Output>
auto copy(Range&& rng, Output out) {
    for (const auto& item : std::forward<Range>(rng)) {
        *out++ = item;
    }
    return out;
}
```

This function operates on a range as its first argument, and an output iterator
as its second argument. Usage is very simple:

```c++
std::vector<int> copies;
auto integers = get_integers();
copy(integers, back_inserter(copies));
```

We can elide the extraneous ``integers`` variable to shrink the code:

```c++
std::vector<int> copies;
copy(get_integers(), back_inserter(copies));
```

We may want to use pipeline syntax to perform the copy. Instead of using ``|``
for the pipeline style, we just use ``|>``. That would look like this:

```c++
std::vector<int> copies;
get_integers() |> copy(back_inserter(copies));
```

To make our ``copy`` algorithm work with the ``|>`` operator, we need to write
this additional code:

```c++
// (This space intentionally left blank)
```

That's right! Nothing at all!

Remember that the semantics of ``|>`` will *rewrite* the code:

```c++
// This:
get_integers() |> copy(back_inserter(copies));
// becomes this:
copy(get_integers(), back_inserter(copies));
```

That is, using ``|>`` is equivalent to the code not using the pipeline style.

### ``transform``

One of the most fundamental algorithms is ``transform``. It applies a
projection function to each element of the input range and yields the result of
that projection.

```c++
template <typename Range, typename Proj>
struct __transform_view {
    // ...
};

template <typename Range, typename Proj, typename Out>
auto transform(Range&& rng, Proj&& fn) {
    return __transform_view(rng, fn);
}
```

This algorithm is a *lazy* version of ``transform``. It will apply the
projection function to elements of ``rng`` as iterators on the
``__transform_view`` object are advanced.

Range algorithms compose. We can use this with ``copy`` to make a meaningful
program:

```c++
copy(transform(get_words(), make_uppercase), ostream_iterator<string>{cout, "\n"});
```

This code, of course, is inside-out from how evaluation is ordered. We can feed
the result of ``transform`` into ``copy`` using ``|>``:

```c++
transform(get_words(), make_uppercase)
  |> copy(ostream_iterator<string>{cout, "\n"});
```

And, without writing any additional support code, we can use ``|>`` to feed
``get_words`` into ``transform``:

```c++
get_words()
  |> transform(make_uppercase)
  |> copy(ostream_iterator<string>{cout, "\n"});
```


### A New Algorithm: ``each_as``

Ranges will be receiving a function template ``to`` that creates a concrete
range from another range. A very primitive implementation of one overload might
look like this:

```c++
template <typename Container, typename Range>
Container to(const Range& rng) {
    Container ret(rng.begin(), rng.end());
    return ret;
}
```

This simply takes a range and uses it to fill a container with the
iterator-pair constructor present on many container types. Usage looks like
this:

```c++
auto filenames = get_strings()
    |> to<vector<filesystem::path>>()
    |> transform(get_filename)
    |> to<vector<string>>();
```

However: The ``to`` algorithm, unlike ``transform``, is *eager*. It consumes
each element of the input immediately. This requires a concrete new container
type that will eagerly allocate a buffer to hold the new objects. In the above
snippet, all we are doing is obtaining the filenames of each file, and we do
not actually care about the intermediate ``std::vector``.

Note: The above example is illustrative. There are other ways to perform the
necessary transform.

What we may want it a new lazy algorithm that simply converts each range
element to a new type as they pass through. How could we define such an
algorithm?

```c++
template <typename T, typename Range>
auto each_as(Range&& rng) {
    return rng |> transform([](const auto& item) { return T(item); });
}
```

With `|>` at our disposal, there is no need to offer two overloads of
``each_as`` for the two styles. The above overload happily works with ``|>``
pipeline style:

```c++
auto filenames = get_strings()
    |> each_as<filesystem::path>()
    |> transform(get_filename)
    |> to<vector<string>>();
```

Or non-pipeline style:

```c++
auto filenames =
    each_as<filesystem::path>(get_strings())
    |> transform(get_filename)
    |> to<vector<string>>();
```


### A New Algorithm: `copy_insert`/`copy_extend`

A common operation is to collect the results of multiple computations into a
single container. We can define two new algorithms:

```c++
template <typename Range, typename Container, typename Iter>
void copy_insert(Range&& rng, Container& c, Iter it) {
    rng |> copy(inserter(c, it));
}

template <typename Range, typename Container>
void copy_extend(Range&& rng, Container& c) {
    rng |> copy_insert(c, c.end());
}
```

Again, we have ``|>`` syntax using normal functions and no special return types
or expression templates.

Using them is very simple:

```c++
// We may use pipeline style:
void collect_filenames(filesystem::path dirpath, vector<string>& fnames) {
    filesystem::directory_iterator{dirpath}
        |> copy_extend(fnames);
}

// Or we may use classical style:
void collect_filenames(filesystem::path dirpath, vector<string>& fnames) {
    copy_extend(
      filesystem::directory_iterator{dirpath},
      fnames
    );
}
```

### Not A New Algorithm: `any_of`

Of course, we can go back to Conor's example and provide a complete
implementation of it:

```cpp
auto dangerous_teams(std::string const& s) -> bool {
    return s
         |> views::group_by(std::equal_to{})
         |> views::transform(ranges::distance)
         |> ranges::any_of([](std::size_t s){
                return s >= 7;
            });
}
```

It is worth repeatedly stressing that this does _not_ require any new overloads
of `any_of` to allow this usage. The above function evaluates exactly as:

```cpp
auto dangerous_teams_rewritten(std::string const& s) -> bool {
    return ranges::any_of(
        ranges::transform(
            ranges::group_by(
                s,
                std::equal_to{}),
            ranges::distance),
        [](std::size_t s){
            return s >= 7;
        });
}
```

This rewrite isn't exactly readable, but that's not the point - nobody has to
read it. Only the compiler has to know how to evaluate these calls, and it has
no problem at all figuring out the right thing to do.

### Async Examples

Ranges isn't the only subset of C++ that would benefit from the existence of
a pipeline rewrite operator. At CppCon 2019, Eric Niebler and David Hollman presented
[A Unifying Abstraction for Async in C++](https://www.youtube.com/watch?v=tF-Nz4aRWAM),
illustrating the executors work that's been ongoing for a few years now. They
build up to the following example:

```cpp
int main() {
    auto f = async_algo(new_thread());
    auto f2 = then(f, [](int i){
        return i + rand();
    });
    printf("%d\n", sync_wait<int>(f2));
}
```

With this proposal, this could be written (with zero additional library work
on anyone's part) as:

```cpp
int main() {
    auto result =
        new_thread()
        |> async_algo
        |> then([](int i){ return i + rand(); })
        |> sync_wait<int>;
    printf("%d\n", result);
}
```

Which demonstrates the linear flow of execution quite well. 

## Precedence (in C++ and elsewhere)

It's important to point out that the notion of a pipeline rewrite operator is
not novel across programming languages, and it isn't even novel in C++.

The particular form of the operator this paper is proposing comes from the
Elixir programming language, where it is known as the pipe operator [@Elixir.Pipe].
Its semantics are most the same as are being proposed here, except that the
parentheses are mandatory. From the docs:

```elixir
iex> "Elixir rocks" |> String.upcase() |> String.split()
["ELIXIR", "ROCKS"]
```

Another language with the same operator is Hack, except its usage is slightly
more generalized than either Elixir's or what is being proposed here [@Hack.Pipe].
While the underlying idea is the same - a function call is split such that one
argument is on the left of `|>` and the rest of the call is on the right - Hack
instead stores the left-hand operand in a variable named `$$`{.x} which then must
appear on the right-hand side (but doesn't necessarily have to be the first
argument):

```php
$x = vec[2,1,3]
  |> Vec\map($$, $a ==> $a * $a)
  |> Vec\sort($$);
```

F#, Julia, and OCaml also have an operator named `|>` which all do the same thing:
they invoke the right-hand side with the left-hand side as its sole argument:
rather than `x |> f(y)` meaning `f(x, y)` as is being proposed here and as it
means in Elixir, it instead means `f(y)(x)`. In other words, given a binary
operator as proposed in [@P1282R0], these languages' version of `|>` could be
implemented as a global:

```cpp
template <typename Arg, std::invocable<Arg> F>
auto operator|>(Arg&& a, F&& f)
    -> std::invoke_result_t<F, Arg>
{
    return std::invoke(FWD(f), FWD(a));
}
```

This may be more in line with how most operators in C++ work, but it's also not
especially useful. It would make it marginally easier to implement partial
calls - you could just return a lambda from those partial calls instead of
having to have this left-pipeable type - but it's still a lot of work on
everyone's behalf to get there.

As far as C++ is concerned, it would be a third in a set of operators that
have special semantics as far as function lookup is concerned:

```cpp
x->f(y)
x .f(y)
x|>f(y)
```

None of the first two operators evaluate `f(y)` by itself and then evaluate
something else joining that result with `x`. `x->f(y)` might invoke something
named `operator->`, but once it finds a pointer, we do a single function call
to something named `f` using both arguments. It's just that while the first two
always invoke a member function, the last would always invoke a non-member
function.

# What about Unified Function Call Syntax?

Any discussion regarding pipeline rewrites, or even the pipeline syntax in
C++20 ranges, will eventually hit on Unified Function Call Syntax (UFCS). As
previously described in a blog post [@Revzin], UFCS means different things to
different people: there were quite a few proposals under this banner that had
a variety of different semantics and different applicability to this problem
space. 

In [@N4165], Herb Sutter presented two goals:

::: quote
*Enable more-generic code*: Today, generic code cannot invoke a function on a
`T` object without knowing whether the function is a member or non-member, and
must commit to one. This is long-standing known issue in C++.
:::

and

::: quote
*Enable “extension methods” without a separate one-off language feature*: The
proposed generalization enables calling non-member functions (and function
pointers, function objects, etc.) symmetrically with member functions, but
without a separate and more limited “extension methods” language feature.
Further, unlike “extension methods” in other languages which are a special-purpose
feature that adds only the ability to add member functions to an existing class,
this proposal would immediately work with calling existing library code without
any change. (See also following points.)
:::

We will address these two points in turn.

## UFCS does not enable more-generic code

The most consistent argument in favor of UFCS, regardless of proposal details,
as Herb made here and Bjarne Stroustrup made in [@N4174] and they both made
together in [@N4474] and Bjarne made again in a blog post on isocpp.org
[@Stroustrup] is this one, quoting from Bjarne's blog post:

::: quote
C++ provides two calling syntaxes, `x.f(y)` and `f(x,y)`. This has bothered me
for a long time. I discussed the problem in the context of multimethods in D&E
in 1994, referring back to a proposal by Doug Lea from 1991, and again in 2007.
Each of the syntaxes has its virtues, but they force design decisions on people,
especially library designers. When you write a library, which syntax do you use
for operations on objects passed to you by your users? The STL insists on the
traditional functional syntax, `f(x,y)`, whereas many OO libraries insist on the
dot notation, `x.f(y)`. In turn, libraries force these decisions on their users.
:::

This is a very real problem in writing generic code in C++, one which UFCS set
out to solve. Since a lot of the examples in this paper deal with Ranges already,
let's stick to Ranges.

### Fails for fundamental types

The fundamental concept in C++20 Ranges is the `std::range`
concept. With a version of UFCS in which member function call syntax could
find non-member functions (notably, not the version that was voted on in plenary
- that one allowed non-member call syntax to find member functions only), we
might expect to be able to define that concept this way:

```cpp
template <typename R>
concept ufcs_range = requires (R& rng) {
    { rng.begin() } ->  input_or_output_iterator;
    { rng.end() } -> sentinel_for<decltype(rng.begin())>;
}
```

You don't even need any language changes for this to work with all the standard
library containers - those just have member `begin` and `end` directly.
`ufcs_range<vector<int>>` is satisfies without much fuss. 

For a type that looks like:

```cpp
namespace lib {
    struct some_container { /* ... */ };
    struct some_iterator { /* ... */ };
    
    auto begin(some_container&) -> some_iterator;
    auto end(some_container&) -> some_iterator;
}
```

`ufcs_range<lib::some_container>` would be satisfied by saying that lookup for
`rng.begin()` would find the free function `lib::begin()` (let's assume for
simplicity that `some_container` has no members named `begin` or `end`) and
likewise for `rng.end()`.

This seems to work, what's the problem?

Consider `ufcs_range<lib::some_container[10]>`. Is this satisfied? C arrays have
no member functions, so the member lookup trivially fails. But the only candidate
we find with ADL for `begin` isn't viable - we don't have a candidate that can
take a C array. Consider what we have to do in order to make this work. The way
we make `begin()` and `end()` work for C arrays is completely agnostic to the
type that the array is of. It shouldn't be `lib`'s responsibility to provide this
function.

And even if it did, consider `ufcs_range<int[10]>`. Here, not only do
we have no member functions but we also have no associated namespaces in which
to look for what we need! Do we conclude that `int[10]` is not a range? The only
way for `rng.begin()` to work on a simple array of `int`s with UFCS is to have
`begin` in scope - which means either that it has to be a global function with
no intervening declarations of `begin` (not going to happen) or it's up to 
every algorithm to bring them into scope. Something like:

```cpp
namespace std {
    template <typename T, size_t N>
    auto begin(T (&arr)[N]) -> T* { return arr; }
    template <typename T, size_t N>
    auto end(T (&arr)[N])   -> T* { return arr+N; }
    
    template <typename R>
    concept ufcs_range = requires (R& rng) {
        { rng.begin() } ->  input_or_output_iterator;
        { rng.end() } -> sentinel_for<decltype(rng.begin())>;
    }    
}

template <std::range R, typename Value>
auto find(R& r, Value const& value)
{
    using std::begin, std::end;
    // UFCS logic here, regular lookup will find the
    // begin/end brought in with the using-declarations
    auto first = r.begin();
    auto last = r.end();
    // ...
}
```

But if we have to do this dance _anyway_, we didn't gain anything from UFCS at
all. We can do the exact same thing already, at the cost of a few more lines of
code (adding declarations of `begin` and `end` that invoke member functions)
and using non-member syntax instead. 

And if we don't have a generic solution that works for the fundamental types
(this example fails for raw arrays, but the general solution will fail if you
want to provide implementations for a generic algorithm for types like `int` or
`char` or pointers), then we don't have a generic solution.

### Too greedy on names

UFCS works by having the non-member fallback find a free function of the same
name with ADL. C++20 ranges introduces customization point objects that do
this for you, which at least makes it easier to write new algorithms (although
the customization point objects themselves are a chore to implement). But
this has fundamental problems, as noted in [@P1895R0]:

::: quote
1. Each one internally dispatches via ADL to a free function of the same name,
which has the effect of globally reserving that identifier (within some
constraints). Two independent libraries that pick the same name for an ADL
customization point still risk collision.

2. There is occasionally a need to write wrapper types that ought to be
transparent to customization. (Type-erasing wrappers are one such example.)
With C++20's CPOs, there is no way to generically forward customizations through
the transparent wrappers

Point (1) above is an immediate and pressing concern in the executors design,
where we would like togive platform authors the ability to directly customize
parallel algorithms. Using the C++20 CPO designwould explode the number of
uniquely-named ADL customization points from a handful to potentially hundreds,
which would create havoc for the ecosystem.

Point (2) is also a concern for executors, where platform authors would like to
decorate executor typeswith platform-specific "properties" (extra-standard
affinities and thresholds of all sorts) that can beexposed even through
transparent layers of adaptation, such as the polymorphic executor wrapper. This
need led to the properties system (P1393) which LEWG has already reviewed.
It's important to note that, although the problems in C++20 CPOs are exposed by
the executors work, theproblems are not specific to executors.
:::

The paper expresses its concerns specifically in relation to CPOs, but the
problem is really about using ADL for customization points. UFCS as a language
feature would push much harder in that direction, and it's not a good direction.

Put differently, designing generic code on top of ADL is somewhat akin to
just eschewing namespaces altogether and going back to our C roots. 

### This is a concepts problem

As argued in [@P1900R0], the problem of customization should be considered
a `concept`s problem and merits a `concept`s solution. What we are trying to do
with this paper has nothing to do with customization - we are not trying to
solve, or even address, this problem. 

Instead, what we are trying to do is address...

## UFCS does enable extension methods without a separate one-off language feature

The other argument that Herb made in favor of UFCS was in favor of adopting
extension method without a special language feature specific to them. Notably,
the argument he made in [@N4165] specifically cites the desire to avoid having
to make any library changes to start using all the functionality - which is
precisely the argument we are making with this proposal with regards to pipelines.

Herb also makes the argument that allow member call syntax to lookup non-member
functions is friendlier to developers and tools as it allows things like
autocomplete to work.

But the question we have is: why does this feature _need_ to be spelled `.`?
There are many problems with that specific choice that completely evaporate if
we simply choose an alternate spelling. There is no question about how the
candidate set should be considered, or how overload resolution should work, 
nor is there any concern about long term library work and accidentally breaking
user code by adding a private member function. The alternate spelling this paper
is proposing is `|>`. 

One example from that document is:

```cpp
FILE* file = fopen( “a.txt”, “wb” );
if (file) {
    fputs(“Hello world”, file);
    fseek(file, 9, SEEK_SET);
    fclose(file);
}
```

This proposal wouldn't allow for any help on `fputs` (would need something like
Hack's explicit `$$`{.x} token) but the rest could be written as:

```cpp
FILE* file = fopen( “a.txt”, “wb” );
if (file) {
    fputs(“Hello world”, file);
    file |> fseek(9, SEEK_SET);
    file |> fclose();
}
```

## 1 Syntax, 2 Meanings

Fundamentally, a problem with UFCS is that it would be yet another example in
C++ where a single syntax has two potential meanings: `x.f()` would either be
a member function call or a non-member function call, it simply depends. 

We already have several examples of this kind of thing in the language, and they
are very frequent sources of complaint by beginners and experts alike:

- `T&&` is either an rvalue reference or a forwarding reference, depending
on where `T` comes from.
- `X{v}` can construct an `initializer_list` from `v`, or not, depending on
`X`. 

Name lookup and overload resolution are two of the most complex aspects of
a very complex language. Instead of pursuing UFCS, we are proposing a second
syntax that actually has the same meaning as an already existing syntax:
`x |> f()` has the exact same meaning as `f(x)`. But both of these syntaxes
always mean invoking the free function `f` and `x.f()` always means invoking
the member function `f`, and having that differentiation seems like a positive
thing rather than a negative thing. 

# What about pipeline composition?

This paper largely focuses on the idea from Ranges that you can have a total
function call and a partial function call:

```cpp
// total call
views::filter(ints, is_even);

// partial call
ints | views::filter(is_even);
```

But there's another important aspect of the Ranges desing that it's important
to discuss: you can build pipelines even without the function call part. That is,
We can just write:

```cpp
auto even = views::filter(is_even);
```

We don't need to "immediately invoke" this filter on a range, we can hold onto it.
We can have a function that returns that filter. And we can build a pipeline
from a large number of range adapters without even having an input range. 

The main function from the calendar example [@range.calendar] is this one:

```cpp
// In:  range<date>
// Out: range<string>, lines of formatted output
auto
format_calendar(std::size_t months_per_line)
{
    return
        // Group the dates by month:
        by_month()
        // Format the month into a range of strings:
      | layout_months()
        // Group the months that belong side-by-side:
      | views::chunk(months_per_line)
        // Transpose the rows and columns of the size-by-side months:
      | transpose_months()
        // Ungroup the side-by-side months:
      | views::join
        // Join the strings of the transposed months:
      | join_months();
}
```

Note the comments: it says that the input is a `range<date>`. Where is it? It's
nowhere. 

What's actually going on here is, effectively, a big foray by C++ into the
world of partial function composition. Now we are become Haskell. This is a very
cool, very useful, bit of functionality that the range adapters provide.

The pipeline rewrite operator we are proposing does not do this. You would
have to write it this way:

```cpp
// In:  range<date>
// Out: range<string>, lines of formatted output
template <typename Range>
auto
format_calendar(Range&& rng, std::size_t months_per_line)
{
    return FWD(rng)
        // Group the dates by month:
      |> by_month()
        // Format the month into a range of strings:
      |> layout_months()
        // Group the months that belong side-by-side:
      |> views::chunk(months_per_line)
        // Transpose the rows and columns of the size-by-side months:
      |> transpose_months()
        // Ungroup the side-by-side months:
      |> views::join
        // Join the strings of the transposed months:
      |> join_months();
}
```

Notably, the formatter becomes a function template since now we actually need
to express the input argument directly. 

This would certainly open up questions about how we want to handle range
adapters in the future if we choose to adopt this proposal. The above code
cannot work without the input range (you cannot write `by_month() |> layout_months()`
to start with since `by_month()` would already be ill-formed - in this new world
it would take a range). The only way to preserve the composition of range
adapters as separate from the range input would be to preserve the current
implementation of `operator|`. 

However, the utility of partial function application and composition is much,
much more expansive than just range adapters. And if we think it's a valuable
things for range adapters, maybe we should find a way to make it work for all
other C++ applications?

# Other Concerns and Direction

Some C++20 code could break. In the same way that the introduction of
`operator<=>` introduced a `<=>` token that would break code that passed the
address of an `operator<=` as a template argument (`f<&operator<=>()` would now
have to be written as `f<&operator<= >()`), a `|>` token would break code that
passes the address of an `operator|` as a template argument (`g<&operator|>()`
would now have to be written as `g<&operator| >()`). This wasn't a huge
concern with the spaceship operator, and it isn't a huge concern with the 
pipeline rewrite operator. 

There may be a concern that this would lead to yet another conflict
in the C++ community as to whether the proper way to invoke functions is
spelled `west(invocable)` or `invocable |> east`. We're not too concerned about
this potential conflict. Just wanted to be thorough.

One question for the future is: 

Do we want to pursue a language feature, similar to what Hack has, for putting
the left-hand side not necessarily as the first argument but also anywhere? This
would allow, from Herb's paper, `file |> fputs("Hello world", $$)`. It could
also be an interesting extension to the _type-constraint_ syntax from concepts
where you could omit a type parameter other than the first in still somewhat
terse notation. 

We do not want to do so for this paper - as the proposal at hand is small, simple,
and provides an enormous amount of utility in its own right without adding
extra complication. But this might be a future direction to consider. 

# Wording

Add `|>` as a token to [lex.operators]{.sref}.

Add `|>` to the _postfix-expression_ grammar in [expr.post]{.sref}:

::: bq
```diff
@_postfix-expression_@:
	@_primary-expression_@
	@_postfix-expression_@ [ @_expr-or-braced-init-list_@ ]
	@_postfix-expression_@ ( @_expression-list_~opt~@ )
	@_simple-type-specifier_@ ( @_expression-list_~opt~@ )
	@_typename-specifier_@ ( @_expression-list_~opt~@ )
	@_simple-type-specifier braced-init-list_@
	@_typename-specifier braced-init-list_@
	@_postfix-expression_@ . template@~opt~ _id-expression_@
	@_postfix-expression_@ -> template@~opt~ _id-expression_@
+   @_postfix-expression_@ |> @_primary-expression_@ ( @_expression-list_~opt~@ ) 
+   @_postfix-expression_@ |> @_primary-expression_@
	@_postfix-expression_@ ++
	@_postfix-expression_@ --
	dynamic_cast < @_type-id_@ > ( @_expression_@ )
	static_cast < @_type-id_@ > ( @_expression_@ )
	reinterpret_cast < @_type-id_@ > ( @_expression_@ )
	const_cast < @_type-id_@ > ( @_expression_@ )
	typeid ( @_expression_@ )
	typeid ( @_type-id_@ )
```
:::

Add a new section immediately after [expr.call]{.sref} named "Pipeline rewrite" [expr.pizza]:

::: bq
::: addu
[1]{.pnum} A postfix expression followed by a `|>` token is a postfix expression.
The `|>` shall be followed by a _primary-expression_.

[2]{.pnum} If the _primary-expression_ is followed by parentheses, then the
expression `E1 |> E2(args)`, were `args` is a possibly empty, comma-separated
list of _initializer-clauses_, is identical (by definition) to `E2(E1, args)`
([expr.call]), except that `E1` is sequenced before `E2`. _\[Note:_ `E2` is
still sequenced before the rest of the function arguments _-end note ]_

[3]{.pnum} If the _primary-expression_ is not followed by parentheses, the
expression `E1 |> E2` is identical (by definition) to `E1 |> E2()`.
:::
:::

Add `|>` to the list of non-overloadable operators in [over.oper]{.sref}/3:

::: bq
[3]{.pnum} The following operators cannot be overloaded:

`.` `.*` `::` `?:` [`|>`]{.addu}

nor can the preprocessing symbols `#`{.x} ([cpp.stringize]) and `##`{.x} ([cpp.concat]).
:::

Add a new Annex C entry, mirroring the one that exists for `<=>` in [diff.cpp17.lex]{.sref}/3:

::: bq
::: addu
**Affected subclause**: [lex.operators]{.sref}
**Change**: New operator `|>`.

**Rationale**: Necessary for new functionality.

**Effect on original feature**: Valid C++ 2017 code that contains a `|` token
immediately followed by a `>` token may be ill-formed or have different
semantics in this International Standard:

```
namespace N {
  struct X {};
  bool operator|(X, X);
  template<bool(X, X)> struct Y {};
  Y<operator|> y;              // ill-formed; previously well-formed
}
```

:::
:::

# Implementation

An implementation in Clang can be found
[here](https://github.com/BRevzin/llvm-project/tree/operator-pizza).
At the moment, the implementation is a direct translations of how we think about
this operator: it actually creates a regular function call expression by
prepending the left-hand side of `|>` to the argument list for function calls
rather than introducing a new AST node for a pipeline rewrite expression.

---
references:
  - id: Hoekstra
    citation-label: Hoekstra
    title: "CppNow 2019: Algorithm Intuition"
    author:
      - family: Conor Hoekstra
    issued:
      - year: 2019
    URL: https://www.youtube.com/watch?v=48gV1SNm3WA
  - id: Elixir.Pipe
    citation-label: Elixir.Pipe
    title: "Pipe Operator - Elixir School"
    author:
        - family: Elixir School
    issued:
        - year: 2019
    URL: https://elixirschool.com/en/lessons/basics/pipe-operator/
  - id: Hack.Pipe
    citation-label: Hack.Pipe
    title: "Expressions and Operators - Pipe"
    author:
        - family: HHVM
    issued:
        - year: 2019
    URL: https://docs.hhvm.com/hack/expressions-and-operators/pipe
  - id: range-v3
    citation-label: range-v3
    title: "Range library for C++14/17/20, basis for C++20's std::ranges"
    author:
        - family: Eric Niebler
    issued:
        - year: 2013
    URL: https://github.com/ericniebler/range-v3/
  - id: range.calendar
    citation-label: range.calendar
    title: "range-v3 calendar example"
    author:
        - family: Eric Niebler
    issued:
        - year: 2015
    URL: https://github.com/ericniebler/range-v3/blob/master/example/calendar.cpp
  - id: Revzin
    citation-label: Revzin
    title: "What is unified function call syntax anyway?"
    author:
        - family: Barry Revzin
    issued:
        - year: 2019
    URL: https://brevzin.github.io/c++/2019/04/13/ufcs-history/
  - id: Stroustrup
    citation-label: Stroustrup
    title: "A bit of background for the unified call proposal"
    author:
        - family: Bjarne Stroustrup
    issued:
        - year: 2016
    URL: https://isocpp.org/blog/2016/02/a-bit-of-background-for-the-unified-call-proposal
---
