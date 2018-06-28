Title: Overload sets as function parameters
Document-Number: DxxxxR0
Date: 2018-06-21
Authors: Barry Revzin, barry dot revzin at gmail dot com
Authors: Andrew Sutton, andrew dot n dot sutton at gmail dot com
Audience: EWG

# Motivation

The desire to pass a callable into a function is pretty fundamental. Anytime today that we can write:

    :::c++
    struct X { ... };
    X getX();
    
    foo(getX()); // for any 'foo'
    
We want to also be able to write:

    :::c++
    template <typename F>
    void algorithm(X x, F f) {
        f(x);
    }
    
    algorithm(getX(), foo); // for that same 'foo'
    
These are, conceptually, very similar. But there are many cases where the former code compiles and runs without issue but the latter fails:

1. `foo` could be a function that takes default arguments
2. `foo` could be a function template
3. `foo` could name an overload set
4. `foo` could be a function that was only found by ADL
5. `foo` in unqualified lookup could have found one function, and so the call to `algorithm()` succeeds, but the ADL `foo` wasn't found and so the call within `algorithm()` fails
6. `foo` could be the name of a non-static member function, or non-static member function template, and we are invoking it from within a non-static member function.
7. `foo` could name a function in the standard library, which we are not allowed to take a pointer to (this restriction made more explicit in light of [P0551](https://wg21.link/p0551r3) and [P0921](https://wg21.link/p0921r0)).

The only solution to this problem today, outside of trafficking exclusively in function objects, is to manually wrap `foo` in a lambda - and probably be quite vigiliant about doing so:

    :::c++
    algorithm(getX(), [&](auto&& ...args) -> decltype(foo(std::forward<decltype(args)>(args)...)) {
        return foo(std::forward<decltype(args)>(args)...);
    });
    
which is usually seen in the wild in macro form:

    :::c++
    #define FWD(x) static_cast<decltype(x)&&>(x)
    #define OVERLOADS_OF(name) [&](auto&& ...args) -> decltype(name(FWD(args)...)) { return name(FWD(args)...); }
    
    algorithm(getX(), OVERLOADS_OF(foo));

However, this is a pretty unsatisfactory solution: we rely on a macro. Or even if not, we have to be vigilant about manually wrapping each and every function at every call site. Why "every"? Because otherwise, we might write code that works today but ends up being very brittle, easily broken. Consider a fairly trivial example:

    :::c+++
    // some library
    void do_something(int );
    
    // some user
    std::invoke(do_something, 42);

This works fine today. But the library that provided `do_something` might someday wish to make some improvements. Maybe add a defaulted second argument to `do_something`. Or a new overload. Or turn `do_something` into a function template. Any number of changes that would not change the meaning of code directly invoking `do_something` with an `int`. The kinds of changes detailed in [P0921](https://wg21.link/p0921r0). All of these changes would break the above user code - even though it compiles today. So _even here_, it would be better had the user written:

    :::c++
    std::invoke(OVERLOADS_OF(do_something), 42);

or, at the very least:

    :::c++
    std::invoke([](int i){ do_something(i); }, 42);
    
And that's simply too much to ask of the user - it's too much to have to think about it, and it's seemingly just extra annotation on the call site. 

These issues prevent C++ from being able to use a point-free style of programming - which can have big readability improvements. After all, the idea we want to express is just to invoke a function by name. Having to explicitly list all of the arguments twice (once in the parameter list, once in the call expression of the lambda body) does aid the reader in any way.

We can do better.
    
# History

This problem has a long history attached to it, with two different tacks explored.

[P0119](https://wg21.link/p0119r2) proposed a syntax-free, "just make it work" approach: synthesize a lambda if, based on a heuristic, it's likely the intended behavior (e.g. if the name nominates an overload set). Unfortunately, this solution fails, as demonstrated in [P0382](https://wg21.link/p0382r0).

[N3617](https://wg21.link/n3617) and later [P0834](https://wg21.link/p0834r0) proposed syntax for the caller to use to explicitly synthesize an overload set:

    :::c++
    algorithm(getX(), []foo);
    std::invoke([]do_something, 42);
    
This was rejected by EWG in the [Albuquerque](http://wiki.edg.com/bin/view/Wg21albuquerque/P0834R0), and would still have the same problem with placing the onus on the user to avoid brittleness at each and every call site. We're still not point-free.

[P0573](https://wg21.link/p0573r2) wouldn't have directly solved this problem, but would have at least made writing the direct lambda less burdensome. It was also rejected in [Albuquerque](http://wiki.edg.com/bin/view/Wg21albuquerque/P0573R2), and also did not even try to solve the point-free problem.

Despite this long history, we believe that this is a problem that needs to be solved. It is unreasonably difficult today to pass a function into another function. The increased emphasis on disallowing users from taking pointers to standard library functions and function templates directly pushes the issue. A significant portion of the discussion of [P0798](https://wg21.link/p0798r0) in LEWG in [Rapperswil](http://wiki.edg.com/bin/view/Wg21rapperswil2018/P0798) was about the problem of passing overload sets into functions - because the paper would simply introduce more places where users may want to do such a thing. 

This is a problem that needs solving, and it has to be solved at the language level.

# Proposal

The community tried to solve this problem without adding any syntax, and it tried to solve this problem by adding syntax on the caller's site. We propose a new approach: adding syntax on the function declaration itself. That is, rather than having overload sets as function *arguments* we instead have overload sets as function *parameters*.

We propose a new magic library type, `std::overload_set<T>` that will deduce an overload set, and we will call this new object an _overload set object_:

    :::c++
    template <typename F>
    void algorithm(X x, std::overload_set<F> f) {
        f(x);
    }
    
    algorithm(getX(), foo);
    std::overload_set<auto> f = foo;

`f` here is a synthesized lambda of the style of `OVERLOADS_OF(foo)`. The difference is that, instead of burdening the caller to annotate each and every function, it's up to the API to do that annotation. There are many more callers than callees, so this seems like it places the burden on the correct party. APIs that use `overload_set` would allow for a point-free style.

## Implementation details and deduction rules

An `overload_set<T>` simply contains a `T` and has a call operator that forwards to `T`'s call operator. Its copy and move constructor and assignment operator are defaulted, its other constructors are unspecified. It is explicitly convertible to `T`.

An implementation might be:

    :::c++
    template <typename T>
    class overload_set {
        T f;
    public:
        overload_set(overload_set const&) = default;
        overload_set(overload_set&&) = default;
        overload_set& operator=(overload_set const&) = default;
        overload_set& operator=(overload_set&&) = default;
        ~overload_set() = default;
        
        template <typename... Us>
        auto operator()(Us&&... us) noexcept(noexcept(f(std::forward<Us>(us)...)))
            -> decltype(f(std::forward<Us>(us)...))
        {
            return f(std::forward<Us>(us)...);
        }
        
        explicit operator T() const { return f; }
    };

The annotation `overload_set` will trigger new deduction rules based not just on the type of the argument it is being deduced from, but also on its _name_. We will go over these proposed deduction rules in detail.

### Deducing from an object

If the argument is an object, whether it is a function object of a pointer to function or pointer to member, then simply deduce that object's type.

    :::c++
    auto square = [](int i){ return i * i; };
    
    template <typename T> void deduce(std::overload_set<T> f);
    deduce(square); // deduce T as decltype(square)
    
The function parameter `f` will have the same underlying type as `square`. There is no synthesis of a new lambda in this scenario, we are just copying the lambda. 

Since `overload_set<T>` is simply a `T`, `overload_set<T&>` is likewise simply a `T&`. This allows for overload set objects that are actually references:

    :::c++
    struct Counter {
        int i;
        int operator() { return ++i; }
    };
    
    Counter c{0};
    c();
    assert(c.i == 1);
    
    std::overload_set<auto> f1 = c;        // this is a copy of c
    std::overload_set<auto const> f2 = c;
    std::overload_set<auto&> f3 = c;       // this is a reference to c
    std::overload_set<auto&&> f4 = c;
    
    f1();
    assert(c.i == 1);
    f2(); // ill-formed, Counter::operator() isn't const
    f3();
    assert(c.i == 2);
    f4();
    assert(c.i == 3);
    
We also propose that deducing an `overload_set` whose template type parameter is an rvalue reference to *cv*-unqualified template parameter behaves as a forwarding reference:

    :::c++
    template <typename T>
    void deduce(std::overload_set<T&&>);
    
    deduce(Counter{0}); // calls deduce<Counter>
    deduce(c);          // calls deduce<Counter&>

This allows the API to avoid copying if it so desires. 
    
This rule gives the appearance of the `&` simply being in the wrong place. But there is a difference between `overload_set<T&>` (an overload set object which is a reference to a callable object) and `overload_set<T>&` (a reference to an overload set object).

### Deducing from a name

TBD

### Deducing from a name that cannot be found by unqualified lookup

TBD

### Deducing from an *operator-function-id*

TBD

### Deducing from class member access

TBD
    
## Overload sets of operators

We also propose that this new style of deduction that leads to synthesizing an overload set apply to names of operators. This obviates the need for learning the names of all the function objects, as everyone is familiar with the operators themselves:

<table style="width:100%">
<tr>
<th style="width:50%">
Today
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::c++
    auto f = std::greater();
    auto f = [](auto const& x, auto const& y) { return x > y; }
</td>
<td>
    :::c++
    auto@ f = operator>
</td>
</tr>
<tr>
<td>
    :::c++
    auto g = std::plus();
    auto g = [](auto const& x, auto const& y) { return x + y; }
</td>
<td>
    :::c++
    auto@ g = operator+;
</td>
</tr>
<tr>
<td>
    :::c++
    auto h = std::times();
    auto h = [](auto const& x, auto const& y) { return x * y; }
</td>
<td>
    :::c++
    auto@ h = operator*;
</td>
</tr>
</table>

## Overload sets of member functions

Likewise, we propose that this does the logical thing for member access and member functions:

    :::c++
    struct X {
        void do_something();
        void do_something(int );
    };

    X x;
    auto@ f = x.do_something;
    
    f();     // equivalent to x.do_something();
    f(1);    // equivalent to x.do_something(1);
    
    auto@ g = &X::do_something;
    g(x);    // equivalent to x.do_something();
    g(x, 1); // equivalent to x.do_something(1);

Note that the declaration of `g` is roughly equivalent to `std::mem_fn(&X::do_something)`, or at least would be if `do_something` was a non-overload, non-template function that had no default arguments. This language feature would make `std::mem_fn` obsolete. 
    
Such partial member syntax is ill-formed today, which seems wasteful, and the desire to write such code is the entire motivation for [P0356](https://wg21.link/p0356r3):

    :::c++
    struct Strategy { double process(std:string, std::string, double, double); };

    std::unique_ptr<Strategy> createStrategy();    
    
    // p0356
    auto f = std::bind_front(&Strategy::process, createStrategy());
    
    // proposed
    auto@ f = createStrategy()->process;

## Library helper    
    
While this approach seems like it may require all functions and function templates to be annotated in the standard library to make full use of the feature, it would be easy to provide a library function that gets the job done:

    :::c++
    template <typename F>
    F overloads_of(F@ f) {
        return f;
    }
    
With that in hand, we can go through the various examples presented in the prior papers:

<table style="width:100%">
<tr>
<th style="width:50%">
Today
</th>
<th style="width:50%">
Proposed
</th>
</tr>
<tr>
<td>
    :::c++
    template <typename T>
    T twice(T x) { return x * x; }
    
    template <typename I>
    void f(I first, I last) {
        std::transform(first, last, first, twice); // error
    
        std::transform(first, last, first, [](auto const& x) { return twice(x); });
    }
    
    auto fn = twice; // error
    auto fn = [](auto const& x) { return twice(x); }
</td>
<td>
    :::c++
    template <typename T>
    T twice(T x) { return x * x; }
    
    template <typename I>
    void f(I first, I last) {
        std::transform(first, last, first, overloads_of(twice));
    }
    
    auto@ fn = twice; // ok
</td>
</tr>
<tr>
<td>
    :::c++
    void sort_decreasing(std::vector<SomeType>& v) {
        std::sort(v.begin(), v.end(), operator>); // error
        std::sort(v.begin(), v.end(), [](SomeType const& x, SomeType const& y){
            return x > y; }); // ok
        std::sort(v.begin(), v.end(), [](auto&& x, auto&& y){
            return x > y; }); // better, shorter            
        std::sort(v.begin(), v.end(), std::greater{}); // shortest
    }
</td>
<td>
    :::c++
    void sort_decreasing(std::vector<SomeType>& v) {
        std::sort(v.begin(), v.end(), overloads_of(operator>));
    }
</td>
</tr>
</table>

## Standard library functions, now and future

This becomes especially important in the context of using names from the standard library directly - which we're only supposed to do in the context of direct function calls.

In the proposed `optional<T>::map`, if we specify it to take an overload set parameter:

    :::c++
    template <typename T>
    struct optional {
        template <Invocable<T const&> F>
        auto map(F@ f) const& -> optional<std::invoke_result_t<F&, T const&>>
        {
            if (!*this) {
                return nullopt;
            } else {
                return f(**this);
            }
        }
    };
    
Then any reasonable usage of it would just work:

    :::c++
    std::optional<std::string> f();
    
    f().map(&std::string::size);     // okay with this proposal, would compile but not allowed today
    f().map(std::size<std::string>); // okay with this proposal, would compile but not allowed today
    f().map(std::size);              // okay with this proposal, ill-formed today
    f().map(size);                   // okay with this proposal (would call std::size), ill-formed today
    
Otherwise, the only approved way to get this behavior would be:

    :::c+++
    f().map([](auto&& s){ return s.size(); });
    
Which is a lot to write for a simple task. Instead, people will consistently reach for the shorter, more brittle solutions. Let's just solve the problem instead.

## Suggested Spellings

Our suggested spelling for this feature is `&[]`. That is:

    :::c++
    template <typename F>
    void algorithm(F&[]);
    
    algorithm(foo);
    auto&[] f = foo;
    
We are open to alternatives.