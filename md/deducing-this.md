<pre class='metadata'>
Title: Deducing this
Status: D
ED: wg21.tartanllama.xyz/deducing-this
Shortname: D0847R0
Level: 0
Editor: Barry Revzin, barry.revzin@gmail.com
Editor: Simon Brand, simon@codeplay.com
Group: wg21
Audience: EWG
Markup Shorthands: markdown yes
Default Highlight: C++
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

This paper proposes a new way of declaring a member function that will allow for deducing the type and value category of the class instance parameter, while still being invocable as a member function.

We propose allowing the first parameter of a member function to be named `this`, with the following restrictions:

 - The member function shall not have any *cv*- or ref-qualifiers
 - The member function shall not be `static`
 - The type of the `this` parameter shall be either:
	 - a reference to possibly *cv*-qualified function template parameter
	 - a reference to possibly *cv*-qualified *injected-class-name*

The `this` parameter will bind to the implicit object argument, as if it were passed as the first argument to a non-member function with the same signature. As `this` cannot be used as a parameter name today, this proposal is purely a language extension. All current syntax remains valid.

```
struct X {
    void foo(X& this);

    template <typename This>
    void bar(This&& this, int i);
};

// X::foo is member function taking no arguments, X::bar takes one 
// argument of type int
X x;
x.foo();
x.bar(0);

// These behave as if the call were to these non-member functions
void foo(X& obj);

template <typename This>
void bar(This&& obj, int i);
foo(x);
bar(x, 0);
```

Within these member functions, the keyword `this` will be used as a reference, not as a pointer. While inconsistent with usage in normal member functions, it is more consistent with its declaration as a parameter of reference type and its ability to be deduced as a forwarding reference. This difference will be a signal to users that this is a different kind of member function, additionally obviating any questions about checking against `nullptr`.

Since in many ways member functions act as if they accepted an instance of class type as their first parameter (for instance, in `INVOKE` and all the functional objects that rely on this), we believe this is a logical extension of the language rules to solve a common and growing source of frustration. This sort of code deduplication is, after all, precisely what templates are for.

The usual template deduction rules apply to the `this` parameter. While the naming of the parameter `this` is significant, the naming of the template type parameter as `This` is not. It is used throughout merely a suggested convention.

```
struct Y {
    int i;

    template <typename This, typename T>
    void bar(This&& this, T&& );

    template <typename Self>
    auto& quux(Self& this) { return this.i; }
};

void demo(Y y, const Y* py) {
    y.bar(4);     // invokes Y::bar<Y&, int>
    py->bar(2.0); // invokes Y::bar<const Y&, double>

    Y{}.quux();   // ill-formed
    y.quux();     // invokes Y::quux<Y>
    py->quux();   // invokes Y::quux<const Y>
}
```
It will be possible to take pointers to these member functions. Their types would be qualified based on the deduced qualification of the instance object. That is, `decltype(&Y::bar<Y, int>)` is `void (Y::*)(int) &&` and `decltype(&Y::quux<const Y>)` is `const int& (Y::*)() const&`. These member functions can be invoked via pointers to members as usual.

While the type of the `this` parameter is deduced, it will always be some qualified form of the class type in which the member function is declared, never a derived type:

```
struct B {
    template <typename This>
    void do_stuff(This&& this);
};

struct D : B { };

D d;
d.do_stuff();  // invokes B::do_stuff<B&>, not B::do_stuff<D&>
```

Accessing members would be done via `this.mem` and not `this->mem`. There is no implicit `this` object, since we now have an *explicit* instance parameter, so all member access must be qualified:

```
template <typename T>
class Z {
    T value;
public:
    template <typename Object>
    decltype(auto) get(Object&& this) {
        return value; // error: unknown identifier 'value'
        return std::forward<Object>(this).value; // ok
    }
};
```

The only allowed types for the `this` parameter are reference to function template parameter and reference to *injected-class-name*. We do not expect the latter form to be used very often, but likewise we see no reason to artificially limit the proposal to templates.

```
template <typename T>
struct B {
    template <typename This>
    void a(This&& this);      // ok

    void b(B& this);          // ok
    template <typename This>
    void c(const This& this); // ok

    void d(B this);           // error: not a reference type
    void e(B* this);          // error: not a reference type
    void f(B<T>& this);       // error: not injected-class-name
    template <typename U>
    void g(B<U*>& this);      // error: not reference to template-parameter
    void h(T& this);          // error: not reference to function's template-parameter
};
```

Overload resolution between new-style and old-style member functions would compare the explicit this parameter of the new functions with the implicit this parameter of the old functions:

```
struct C {
    template <typename This>
    void foo(This& this); // #1
    void foo() const;     // #2
};

void demo(C* c, C const* d) {
    c->foo(); // calls #1, better match
    d->foo(); // calls #2, non-template preferred to template
}
```

As these functions are still member functions, they will always take as their first argument an instance of class type. As such, it makes little sense to accept a default argument for the `this` parameter, nor a default type parameter in the case of a function template. Such a defaulted type or argument would never be used anyway and as such would be a sign of user error. This proposal suggests that such use be ill-formed:

```
struct A {
    template <typename This=A> // error
    void foo(This&& this);

    void bar(A&& this = A{});  // error
};
```

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
    template <class This>
    void bar (This&& this);
};
```

This overload is callable for all `const`- and ref-qualified object parameters, just like the above examples. It is also callable for `volatile`-qualified objects, so the code is not entirely equivalent; however, the `volatile` versions are unlikely to be invalid and more likely to be simply left out for the sake of brevity. The only major difference is in the third case, where non-`const` lvalue arguments would be non-`const` inside the function body, and `const` rvalue arguments would be `const&&` instead of `const&`. Again, this is unlikely to cause correctness issues unless `this` has other member functions called on it which do semantically different things depending on the cv-qualification of `this`.

## Alternative syntax

Rather than naming the first parameter `this`, we can also consider introducing a dummy template parameter where the qualifications normally reside. This syntax is also ill-formed today, and is purely a language extension:

```
template <typename T>
struct X {
    T value;

    // as proposed
    template <typename This>
    decltype(auto) foo(This&& this) {
        return std::forward<This>(this).value;
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
  template <typename This>
  auto& operator[](This&& this,
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
  template <typename This>
  constexpr auto operator->(This&& this) {
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
  template <typename This>
  constexpr auto&& operator*(This&& this) {
    return forward<This>(this).m_value;
  }

  template <typename This>
  constexpr auto&& value(This&& this) {
    if (this.has_value()) {
      return forward<This>(this).m_value;
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
  template <typename This, typename F>
  constexpr auto
  and_then(This&& this, F&& f) & {
    using val = decltype((
        forward<This>(this).m_value));
    using result = invoke_result_t<F, val>;

    static_assert(
      is_optional<result>::value,
      "F must return an optional");

    return this.has_value()
        ? invoke(forward<F>(f),
                 forward<This>
                   (this).m_value)
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

    template <typename This, typename... Args>
    auto operator()(This&& this, Args&&... )
        -> decltype(!invoke(forward<This>(this).f, forward<Args>(args)...))
    {
        return !invoke(forward<This>(this).f, forward<Args>(args)...);
    }
};

template <typename F>
auto not_fn(F&& f) {
    return call_wrapper<std::decay_t<F>>{std::forward<F>(f)};
}

not_fn(unfriendly{})(1); // ok
not_fn(fun{})();         // error
```

Here, there is only one overload with everything deduced together, with either `This = fun` or `This = poison` as appropriate. As a result, this singular overload then has precisely the desired behavior: working, for `unfriendly`, and not working, for `fun`.

## Recursive Lambdas

This proposal also allows for an alternative solution to implementing a recursive lambda, since now we open up the possibility of allowing a lambda to reference itself:

```
// as proposed in [P0839]
auto fib = [] self (int n) {
    if (n < 2) return n;
    return self(n-1) + self(n-2);
};

// this proposal
auto fib = [](auto& this, int n) {
    if (n < 2) return n;
    return this(n-1) + this(n-2);
};
```

In the specific case of lambdas, a lambda could both capture `this` and take a generic parameter named `this`. If this happens, use of `this` would refer to the parameter (and hence, the lambda itself) and not the `this` pointer of the outer-most class. This preference follows the normal scoping rules.

```
struct A {
    int bar();

    auto foo() {
        return [this](auto& this, int n) {
            return this->bar() + n; // error: no operator->() for this lambda
        };
    }
};
```


# Acknowledgements

The authors would like to thank:
- Jon Wakely, for bringing us all together by pointing out we were writing the
  same paper, twice
- Graham Heynes, Andrew Bennieston, Jeff Snyder for early feedback
  regarding the meaning of `this` inside function bodies
- Jackie Chen, Vittorio Romeo, Tristan Brindle, for early feedback
- Guilherme Hartmann for his guidance with the implementation

