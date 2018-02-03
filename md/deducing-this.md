<pre class='metadata'>
Title: Deducing this
Status: D
ED: wg21.tartanllama.xyz/deducing-this
Shortname: D0847R0
Level: 0
Editor: Gašper Ažman, gasper dot azman at gmail dot com
Editor: Ben Deane, ben at elbeno dot com
Editor: Barry Revzin, barry.revzin@gmail.com
Editor: Simon Brand, simon@codeplay.com
Group: wg21
Audience: EWG
Markup Shorthands: markdown yes
Default Highlight: C++
Abstract: We propose a new mechanism for specifying the value category of an instance of a class, which is visible from inside a member function of that class -- in other words, a way to tell from within a member function whether one's this points to an rvalue or an lvalue, and whether it is const or volatile.
</pre>


# Motivation

In C++03, member functions could have *cv*-qualifications, so it was possible to have scenarios where a particular class would want both a `const` and non-`const` overload of a particular member (Of course it was possible to also want `volatile` overloads, but those are less common). In these cases, both overloads do the same thing - the only difference is in the types accessed and used. This was handled by either simply duplicating the function, adjusting types and qualifications as necessary, or having one delegate to the other. An example of the latter can be found in Scott Meyers' "Effective C++", Item 3:

```
class TextBlock {
public:
  const char& operator[](std::size_t position) const {
    // ...
    return text[position];
  }

  char& operator[](std::size_t position) {
    return const_cast<char&>(
      static_cast<const TextBlock&>(this)[position]
    );
  }
  // ...
};
```

Neither the duplication or the delegation via `const_cast` are arguably great solutions, but they work.

In C++11, member functions acquired a new axis to specialize on: ref-qualifiers. Now, instead of potentially needing two overloads of a single member function, we might need four: `&`, `const&`, `&&`, or `const&&`. We have three approaches to deal with this: we implement the same member four times, we can have three of the overloads delegate to the fourth, or we can have all four delegate to a helper, private static member function. One example might be the overload set for `optional<T>::value()`. The way to implement it would be something like:

<table>
<tr><th>Quadruplication</th><th>Delegation to 4th</th><th>Delegation to helper</th></tr>
<tr><td>
```
template <typename T>
class optional {
    // ...
    constexpr T& value() & {
        if (has_value()) {
            return this->m_value;
        }
        throw bad_optional_access();
    }

    constexpr const T& value() const& {
        if (has_value()) {
            return this->m_value;
        }
        throw bad_optional_access();
    }

    constexpr T&& value() && {
        if (has_value()) {
            return std::move(this->m_value);
        }
        throw bad_optional_access();
    }

    constexpr const T&&
    value() const&& {
        if (has_value()) {
            return std::move(this->m_value);
        }
        throw bad_optional_access();
    }
    // ...
};
```
</td>
<td>
```
template <typename T>
class optional {
    // ...
    constexpr T& value() & {
        return const_cast<T&>(
            static_cast<optional const&>(
                *this).value());
    }

    constexpr const T& value() const& {
        if (has_value()) {
            return this->m_value;
        }
        throw bad_optional_access();
    }

    constexpr T&& value() && {
        return const_cast<T&&>(
            static_cast<optional const&>(
                *this).value());
    }

    constexpr const T&&
    value() const&& {
        return static_cast<const T&&>(
            value());
    }
    // ...
};
```
</td>
<td>
```
template <typename T>
class optional {
    // ...
    constexpr T& value() & {
        return value_impl(*this);
    }

    constexpr const T& value() const& {
        return value_impl(*this);
    }

    constexpr T&& value() && {
        return value_impl(std::move(*this));
    }

    constexpr const T&&
    value() const&& {
        return value_impl(std::move(*this));
    }

private:
    template <typename Opt>
    static decltype(auto)
    value_impl(Opt&& opt) {
        if (!opt.has_value()) {
            throw bad_optional_access();
        }
        return std::forward<Opt>(opt).m_value;
    }


    // ...
};
```
</td></tr>
</table>

It's not like this is a complicated function. Far from. But more or less repeating the same code four times, or artificial delegation to avoid doing so, is the kind of thing that begs for a rewrite. Except we can't really. We *have* to implement it this way. It seems like we should be able to abstract away the qualifiers. And we can... sort of. As a non-member function, we simply don't have this problem:

```
template <typename T>
class optional {
    // ...
    template <typename Opt>
    friend decltype(auto) value(Opt&& o) {
        if (o.has_value()) {
            return std::forward<Opt>(o).m_value;
        }
        throw bad_optional_access();
    }
    // ...
};
```

This is great - it's just one function, that handles all four cases for us. Except it's a non-member function, not a member function. Different semantics, different syntax, doesn't help.

There are many, many cases in code-bases where we need two or four overloads of the same member function for different `const`- or ref-qualifiers. More than that, there are likely many cases that a class should have four overloads of a particular member function, but doesn't simply due to laziness by the developer. We think that there are sufficiently many such cases that they merit a better solution than simply: write it, then write it again, then write it two more times.

# Proposal

We propose the ability to add an optional first parameter to any member function of a class `T`, taking the form `T [const] [volatile] [&|&&] this <identifier>`.

To facilitate use in generic lambda expressions, this may also be formulated as `auto [const] [volatile] [&|&&] this <identifier>`.

In all cases, the value category of a so-defined `identifier` inside the member function is exactly what the existing parameter rules would already imply. In other words, the *cv-ref qualifiers* that stand after the function signature now
explicitly apply to the `identifier` so designated with `this`.

This is a strict extension to the language; all existing syntax remains valid.

With this extension, the example from above can be written like so:

```
template <typename T>
class optional {
    // ...
    template <typename Self>
    decltype(auto) value(Self&& this self) {
        if (o.has_value()) {
            return std::forward<Self>(self).m_value;
        }
        throw bad_optional_access();
    }
    // ...
};
```

We believe that the ability to write *cv-ref qualifier*-aware member functions without duplication will improve code maintainability, decrease the likelihood of bugs, and allow users to write fast, correct code more easily.

## What Does `this` in a Parameter List Mean?

Effectively, `this` denotes a parameter, that otherwise behaves completely normally, to be the parameter that `*this` refers to. The name of this parameter follows the general rules for parameter naming, but shall be referred to as `self` for the remainder of this paper.

The entries of this table should be read as if they are inside a class `X`:

```
class X { /* entry */ };
```

In other words, `X` is *not* a template parameter.

<table>
<tr><th>**written as**                         </th><th>**C++17 signature**         </th><th>**comments** </th></tr>
<tr><td>`void f(X this self)`                  </td><td>currently not available     </td><td>   [value]   </td></tr>
<tr><td>`void f(X& this self)`                 </td><td>`void f() &`                </td><td>             </td></tr>
<tr><td>`void f(X&& this self)`                </td><td>`void f() &&`               </td><td>             </td></tr>
<tr><td>`void f(X const this self)`            </td><td>currently not available     </td><td>   [value]   </td></tr>
<tr><td>`void f(X const& this self)`           </td><td>`void f() const&`           </td><td>             </td></tr>
<tr><td>`void f(X const&& this self)`          </td><td>`void f() const&&`          </td><td>             </td></tr>
<tr><td>`void f(X volatile this self)`         </td><td>currently not available     </td><td>   [value]   </td></tr>
<tr><td>`void f(X volatile& this self)`        </td><td>`void f() volatile&`        </td><td>             </td></tr>
<tr><td>`void f(X volatile&& this self)`       </td><td>`void f() volatile&`        </td><td>             </td></tr>
<tr><td>`void f(X const volatile this self)`   </td><td>currently not available     </td><td>   [value]   </td></tr>
<tr><td>`void f(X const volatile& this self)`  </td><td>`void f() const volatile&`  </td><td>             </td></tr>
<tr><td>`void f(X const volatile&& this self)` </td><td>`void f() const volatile&&` </td><td>             </td></tr>
</table>

- *[value]*: whether passing by value should be allowed is debatable, but seems desirable for completeness and parity with inline friend functions.
- The interpretation of `this` in the member function body is always the same -- it points to the same object `self` references.
- `self` is _not_ a reserved identifier -- instead, just a conventional naming. Any valid identifier is valid instead of `self` (as is, for parity with the rest of the language, no name, since parameter names are optional).
- `self`, where it is visible, behaves exactly as a parameter declared without `this`. The only difference is in the call syntax. This means that type deduction, use in `decltype` for trailing return types etc., and use within the function body are completely unsurprising.
- As now, only one definition for a given signature may be present -- e.g. one may define at most one of `void f()`, `void f()&`, or `void f(X& this)`. The first two are already exclusive of each other, we merely add a third way to define the very same method.

## How does a templated `this`-designated parameter work?

The type of the parameter will be deduced as if it were the first parameter of a non-member function template and the implicit object parameter was passed as the first argument.

## What does `this` mean in the body of a member function?

The behavior of \this is unchanged. The behavior of `self` is the same as a paramter declared without the `this` designator. The only difference is in how `self` is bound (to `*this`, and not to an explicitly provided parameter).

## Does this change overload resolution at all?

No. Non-templates still get priority over templates, et cetera.

## How do the explicit `this`-designated parameter and the current, trailing *cv-ref qualifiers* interact?

Other than the pass-by-value member functions, which currently do not have syntax to represent them, the explicit `this` signatures are aliases for those with trailing *cv-ref qualifiers*. They stand for the very same functions.

This means that rewriting the function signature in a different style should not change the ABI of your class, and you should also be able to implement a member function that is forward-declared with one syntax using the other.

## `this` in a variadic parameter pack

Given the fact that there is no obvious meaning to the expression
```
struct X {
  template <typename... Ts>
  void f(Ts... this selves);
};
```
such a program is ill-formed.

## Constructors and Destructors

No change to current rules. Currently, one cannot have different cv-ref versions of either, so you cannot designate any parameter with `this`.

## What about pass-by-value member functions?

We think they are a logical extension of the mechanism, and would go a long way towards making member functions as powerful as inline friend functions, with the only difference being the call syntax.

One implication of this is that the `this` parameter would be move-constructed in cases where the object is an rvalue, allowing you to treat chained builder member functions that return a new object uniformly without having to resort to templates.

*Example*:

```
class string_builder {
  std::string s;

  operator std::string (string_builder this self) {
    return std::move(s);
  }
  string_builder operator*(string_builder this self, int n) {
    assert(n > 0);

    s.reserve(s.size() * n);
    auto const size = s.size();
    for (auto i = 0; i < n; ++i) {
      s.append(s, 0, size);
    }
    return self;
  }
  string_builder bop(string_builder this self) {
    s.append("bop");
    return self;
  }
};

// this is optimally efficient as far as allocations go
std::string const x = (string_builder{{"asdf"}} * 5).bop().bop();
```

Of course, implementing this example with templated `this` member functions would have been slightly more efficient due to also saving on move constructions, but the by-value `this` usage makes for simpler code.

## Writing the function pointer types for such functions

Currently, we write member function pointers like so:

```
struct Y {
  int f(int a, int b) const &;
};
static_assert(std::is_same_v<decltype(&Y::f), int (Y::*)(int, int) const &>);
```

All the member functions that take references already have a function pointer syntax - they are just alternate ways of writing functions we can already write.

The only one that does not have such a syntax is the pass-by-value method, all others have pre-existing signatures that do just fine.

We are asking for suggestions for syntax for these function pointers. We give our first pass here:

```
struct Z {
  int f(Z const& this, int a, int b);
  // same as `int f(int a, int b) const&;`
  int g(Z this, int a, int b);
};
// f is still the same as Y::f
static_assert(std::is_same_v<decltype(&Z::f), int (Z::*)(int, int) const &>);
// but would this alternate syntax make any sense?
static_assert(std::is_same_v<decltype(&Z::f), int (*)(Z::const&, int, int)>);
// It allows us to specify the syntax for Z as a pass-by-value member function
static_assert(std::is_same_v<decltype(&Z::g), int (*)(Z::, int, int)>);
```

Such an approach unifies, to a degree, the member functions and the rest of the function type spaces, since it communicates not only that the first parameter is special, but also its type and calling convention.

## `virtual` and `this` as value

Virtual member functions are always dispatched based on the type of the object the dot -- or arrow, in case of pointer -- operator is being used on. Once the member function is located, the parameter `this` is constructed with the appropriate move or copy constructor and passed as the `this` parameter, which might incur slicing.

Effectively, there is no change from current behavior -- only a slight addition of a new overload that behaves the way a user would expect.

## `virtual` and templated member functions

This paper does not propose a change from the current behavior. `virtual` templates are still disallowed.

## Can `static` member functions have a `this` parameter?

No. Static member functions currently do not have an implicit `this` parameter, and therefore have no reason to have an explicit one.

## Teachability Implications

Using `auto&& this self` follows existing patterns for dealing with forwarding references.

Explicitly naming the object as the `this`-designated first parameter fits with many programmers' mental model of the `this` pointer being the first parameter to member functions "under the hood" and is comparable to usage in other languages, e.g. Python and Rust.

This also makes the definition of "`const` member function" more obvious, meaning it can more easily be taught to students.

It also works as a more obvious way to teach how `std::bind` and `std::function` work with a member function pointer by making the pointer explicit.

## ABI implications for `std::function` and related

If references and pointers do not have the same representation for member functions, this effectively says "for the purposes of the `this`-designated first parameter, they do."

This matters because code written in the "`this` is a pointer" syntax with the `this->` notation needs to be assembly-identical to code written with the `self.` notation; the two are just different ways to implement a function with the same signature.

## Interplays with capturing `[this]` and `[*this]`

`this` just designates the parameter that is bound to the reference to the function object. It does not, in any way, change the meaning of `this`.

If other language features play with what `this` means, they are completely orthogonal and do not have interplays with this proposal. However, it should be obvious that develpers have a great potential for introducing hard-to-read code if they are at all changing the meaning of `this`, especially in conjunction with this proposal.

## Is `auto&& this self` allowed in member functions as well as lambdas?

Yes. `auto&& param_name` has a well-defined meaning that is unified across the language. There is absolutely no reason to make it less so.

## Impact on the Standard

TBD: A bunch of stuff in section 8.1.5 [expr.prim.lambda].

TBD: A bunch of stuff in that \this can appear as the first member function
parameter.

## Minimal Translations

The most common qualifier overload sets for member functions are:

1. `const` and non-`const`
2. `&`, `const&`, `&&`, and `const&&`
3. `const&` and `&&`

Some examples:

<table>
<tr><th>1</th><th>2</th><th>3</th></tr>
<td>
```
struct foo {
  void bar();
  void bar() const;
};
```
</td>
<td>
```
struct foo {
  void bar() &;
  void bar() const&;
  void bar() &&;
  void bar() const&&;
};
```
</td>
<td>
```
struct foo {
  void bar() const&;
  void bar() &&;
};
```
</td>
</tr>
</table>

All three of these can be handled by a single perfect-forwarding overload, like this:

```
struct foo {
    template <class Self>
    void bar (Self&& this self);
};
```

This overload is callable for all `const`- and ref-qualified object parameters, just like the above examples. It is also callable for `volatile`-qualified objects, so the code is not entirely equivalent; however, the `volatile` versions are unlikely to be invalid and more likely to be simply left out for the sake of brevity. The only major difference is in the third case, where non-`const` lvalue arguments would be non-`const` inside the function body, and `const` rvalue arguments would be `const&&` instead of `const&`. Again, this is unlikely to cause correctness issues unless `this` has other member functions called on it which do semantically different things depending on the cv-qualification of `this`.

## Alternative syntax

Instead of qualifiying the parameter with `this`, to mark it as the parameter for deducing `this`, we could let the first parameter be named `this` instead:

```
template <typename T>
struct X {
    T value;

    // as proposed
    template <typename Self>
    decltype(auto) foo(Self&& this self) {
        return std::forward<Self>(self).value;
    }

    // alternative
    template <typename This>
    decltype(auto) foo(This&& this) {
        return std::forward<This>(this).value;
    }
```

In the example above, `this` is no longer a pointer. Since `this` can never be a null pointer in valid code in the first place, this is an advantage, but may add confusion to the language.

Rather than naming the first parameter `this`, we can also consider introducing a dummy template parameter where the qualifications normally reside. This syntax is also ill-formed today, and is purely a language extension:

```
template <typename T>
struct X {
    T value;

    // as proposed
    template <typename Self>
    decltype(auto) foo(Self&& this self) {
        return std::forward<Self>(self).value;
    }

    // alternative
    template <typename This>
    decltype(auto) foo() This&& {
        return std::forward<This>(*this).value;
    }

    // another alternative
    decltype(auto) foo() auto&& {
        return std::forward<decltype(*this)>(*this).value;
    }
};
```

This has the benefit of not muddying the parameter list, and it keeps the qualifier deduction off to the side where qualifiers usually live.

## Unified Function Call Syntax

The proposed use of `this` as the first parameter seems as if these members functions really had better be written as non-members to begin with, and might suggest that the solution to this problem is really unified function call syntax (UFCS).

However, several of the motivating examples presented in this proposal are implementations of operators that cannot be implemented as non-members (`()`, `[]`, `->`, and unary `*`). UFCS alone would be insufficient. Additionally, the member function overload sets in all of these examples exist as *member* functions, not free functions, today. Having to take a member function overload set and rewrite it as a non-member (likely `friend`ed) free function template is very much changing the intended design of a class to overcome a language hurdle. This proposal contends that it would be more in keeping with programmer intent to allow member functions to stay member functions.

# Real-World Examples

## Deduplicating Code

This proposal can de-duplicate and de-quadruplicate a large amount of code. In each case, the single function is only slightly more complex than the initial two or four, which makes for a huge win. What follows are a few examples of how repeated code can be reduced.

The particular implementation of optional is Simon's, and can be viewed on [GitHub](https://github.com/TartanLlama/optional), and this example includes some functions that are proposed in [P0798](https://wg21.link/p0798), with minor changes to better suit this format:

<table>
<tr><th>C++17</th><th>This proposal</th></tr>
<tr>
<td>
```
class TextBlock {
public:
  const char&
  operator[](std::size_t position) const {
    // ...
    return text[position];
  }

  char& operator[](std::size_t position) {
    return const_cast<char&>(
      static_cast<const TextBlock&>
        (this)[position]
    );
  }
  // ...
};
```
<td>
```
class TextBlock {
public:
  template <typename Self>
  auto& operator[](Self&& this self,
                   std::size_t position) {
    // ...
    return this.text[position];
  }
  // ...
};
```
</td>
</tr>
<tr>
<td>
```
template <typename T>
class optional {
  // ...
  constexpr T* operator->() {
    return std::addressof(this->m_value);
  }

  constexpr const T*
  operator->() const {
    return std::addressof(this->m_value);
  }
  // ...
};
```
</td>
<td>
```
template <typename T>
class optional {
  // ...
  template <typename Self>
  constexpr auto operator->(Self&& this self) {
    return std::addressof(this.m_value);
  }
  // ...
};
```
</td>
</tr>

<tr>
<td>
```
template <typename T>
class optional {
  // ...
  constexpr T& operator*() & {
    return this->m_value;
  }

  constexpr const T& operator*() const& {
    return this->m_value;
  }

  constexpr T&& operator*() && {
    return std::move(this->m_value);
  }

  constexpr const T&&
  operator*() const&& {
    return std::move(this->m_value);
  }

  constexpr T& value() & {
    if (has_value()) {
      return this->m_value;
    }
    throw bad_optional_access();
  }

  constexpr const T& value() const& {
    if (has_value()) {
      return this->m_value;
    }
    throw bad_optional_access();
  }

  constexpr T&& value() && {
    if (has_value()) {
      return std::move(this->m_value);
    }
    throw bad_optional_access();
  }

  constexpr const T&& value() const&& {
    if (has_value()) {
      return std::move(this->m_value);
    }
    throw bad_optional_access();
  }
  // ...
};
```
</td>
<td>

```
template <typename T>
class optional {
  // ...
  template <typename Self>
  constexpr auto&& operator*(Self&& this self) {
    return forward<Self>(self).m_value;
  }

  template <typename Self>
  constexpr auto&& value(Self&& this self) {
    if (this.has_value()) {
      return forward<Self>(self).m_value;
    }
    throw bad_optional_access();
  }
  // ...
};
```
</td>

<tr>
<td>
```
template <typename T>
class optional {
  // ...
  template <typename F>
  constexpr auto and_then(F&& f) & {
    using result =
      invoke_result_t<F, T&>;
    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return has_value()
        ? invoke(forward<F>(f), **this)
        : nullopt;
  }

  template <typename F>
  constexpr auto and_then(F&& f) && {
    using result =
      invoke_result_t<F, T&&>;
    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return has_value()
        ? invoke(forward<F>(f),
                 std::move(**this))
        : nullopt;
  }

  template <typename F>
  constexpr auto and_then(F&& f) const& {
    using result =
      invoke_result_t<F, const T&>;
    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return has_value()
        ? invoke(forward<F>(f), **this)
        : nullopt;
  }

  template <typename F>
  constexpr auto and_then(F&& f) const&& {
    using result =
      invoke_result_t<F, const T&&>;
    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return has_value()
        ? invoke(forward<F>(f),
                 std::move(**this))
        : nullopt;
  }
  // ...
};
```
</td>
<td>
```
template <typename T>
class optional {
  // ...
  template <typename Self, typename F>
  constexpr auto
  and_then(Self&& this self, F&& f) & {
    using val = decltype((
        forward<Self>(self).m_value));
    using result = invoke_result_t<F, val>;

    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return this.has_value()
        ? invoke(forward<F>(f),
                 forward<self>
                   (self).m_value)
        : nullopt;
  }
  // ...
};
```
</td>
</table>

Keep in mind that there are a few more functions in P0798 that have this lead to this explosion of overloads, so the code difference and clarity is dramatic.

For those that dislike returning auto in these cases, it is very easy to write a metafunction that matches the appropriate qualifiers from a type. Certainly simpler than copying and pasting code and hoping that the minor changes were made correctly in every case.


## SFINAE-friendly callables

A seemingly unrelated problem to the question of code quadruplication is that of writing these numerous overloads for function wrappers, as demonstrated in [P0826](https://wg21.link/p0826). Consider what happens if we implement `std::not_fn()`, as currently specified:

```
template <typename F>
class call_wrapper {
    F f;
public:
    // ...
    template <typename... Args>
    auto operator()(Args&&... ) &
        -> decltype(!declval<invoke_result_t<F&, Args...>>())'

    template <typename... Args>
    auto operator()(Args&&... ) const&
        -> decltype(!declval<invoke_result_t<const F&, Args...>>());

    // ... same for && and const && ...
};

template <typename F>
auto not_fn(F&& f) {
    return call_wrapper<std::decay_t<F>>{std::forward<F>(f)};
}
```

As described in the paper, this implementation has two pathological cases: one in which the callable is SFINAE-unfriendly (which would cause a call to be ill-formed, when it could otherwise work), and one in which overload is deleted (which would cause a call to fallback to a different overload, when it should fail):

```
struct unfriendly {
    template <typename T>
    auto operator()(T v) {
        static_assert(std::is_same_v<T, int>);
        return v;
    }

    template <typename T>
    auto operator()(T v) const {
        static_assert(std::is_same_v<T, double>);
        return v;
    }
};

struct fun {
    template <typename... Args>
    void operator()(Args&&...) = delete;

    template <typename... Args>
    bool operator()(Args&&...) const { return true; }
};

std::not_fn(unfriendly{})(1); // static assert!
std::not_fn(fun{})();         // ok!? Returns false
```

Gracefully handling SFINAE-unfriendly callables is **not solvable** in C++ today. Preventing fallback can be solved by the addition of yet another four overloads, so that each of the four *cv*/ref-qualifiers leads to a pair of overloads: one enabled and one `deleted`.

This proposal solves both problems by simply allowing `this` to be deduced. The following is a complete implementation of `std::not_fn`:

```
template <typename F>
struct call_wrapper {
    F f;

    template <typename Self, typename... Args>
    auto operator()(Self&& this self, Args&&... )
        -> decltype(!invoke(forward<Self>(self).f, forward<Args>(args)...))
    {
        return !invoke(forward<Self>(self).f, forward<Args>(args)...);
    }
};

template <typename F>
auto not_fn(F&& f) {
    return call_wrapper<std::decay_t<F>>{std::forward<F>(f)};
}

not_fn(unfriendly{})(1); // ok
not_fn(fun{})();         // error
```

Here, there is only one overload with everything deduced together, with either `Self = fun` or `Self = poison` as appropriate. As a result, this singular overload then has precisely the desired behavior: working, for `unfriendly`, and not working, for `fun`.

## Recursive Lambdas

This proposal also allows for an alternative solution to implementing a recursive lambda, since now we open up the possibility of allowing a lambda to reference itself:

```
// as proposed in [P0839]
auto fib = [] self (int n) {
    if (n < 2) return n;
    return self(n-1) + self(n-2);
};

// this proposal
auto fib = [](auto& this self, int n) {
    if (n < 2) return n;
    return self(n-1) + self(n-2);
};
```

If the lambda would otherwise decay to a function pointer, `&self` shall have the value of that function pointer.

### Expressions allowed for \self in lambdas

```
  self(...);      // call with appropriate signature
  decltype(self); // evaluates to the type of the lambda with the appropriate
                  // cv-ref qualifiers
  &self;          // the address of either the closure object or function pointer
  std::move(self) // you're allowed to move yourself into an algorithm...
  /* ... and all other things you're allowed to do with the lambda itself. */
```

Within lambda expressions, the `this` parameter still does not allow one to refer to the members of the closure object, which has no defined storage or layout, nor do its members have names. Instead it allows one to deduce the value category of the lambda and access its members -- including various call operators -- in the way appropriate for the value category.

# Acknowledgements

The authors would like to thank:
- Jon Wakely, for bringing us all together by pointing out we were writing the same paper, twice
- Graham Heynes, Andrew Bennieston, Jeff Snyder for early feedback regarding the meaning of `this` inside function bodies
- Jackie Chen, Vittorio Romeo, Tristan Brindle, for early feedback
- Guilherme Hartmann for his guidance with the implementation
