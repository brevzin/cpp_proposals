Title: constexpr <code><i>INVOKE</i></code>
Document-Number: D1065R1
Authors: Tomasz Kamiński, tomaszkam at gmail dot com
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: LWG

# Revision History

Since [R0](https://wg21.link/P1065R0), just wording changes to correctly describe what it means for things `bind` to be `constexpr` and also including `bind_front()`.

# Motivation

Currently, one of the most important utility functions in the standard libary, `std::invoke()`, is not `constexpr`. Even though `std::apply()` and `std::visit()`, both of which rely on <code><i>INVOKE</i></code>, are both `constexpr`. The standard library thus finds itself in an odd state where `std::invoke()` is and is not `constexpr`. 

The reason that `std::invoke()` is not `constexpr` has some interesting history associated with it. But at this point, it is simply history, and there is no further blocker to making this change. This proposal resolves [LWG 2894](https://wg21.link/lwg2894) but also goes one step further and addresses various other <code><i>INVOKE</i></code>-related machinery.

# History

Our tale beings in April, 2015 with [llvm bug 23141](https://bugs.llvm.org/show_bug.cgi?id=23141 "std::bind const-qualifying bound arguments captured by value when compiled as C++14"), which presented this code which broke in clang in C++14 (but had compiled in C++11 mode) due to the introduction of a `constexpr __invoke` (which ended up breaking range-v3):

    :::cpp
    #include <functional>
    #include <type_traits>

    struct Fun
    {
      template<typename T, typename U>
      void operator()(T && t, U && u) const
      {
        static_assert(std::is_same<U, int &>::value, "");
      }
    };

    int main()
    {
        std::bind(Fun{}, std::placeholders::_1, 42)("hello");
    }

as well as the similar [llvm bug 23135](https://bugs.llvm.org/show_bug.cgi?id=23135 "[C++11/14] Body of constexpr function templates instantiated too eagerly in unevaluated operands"), which was about this program:

    :::cpp
    template<typename T>
    int f(T x)
    {
        return x.get();
    }

    template<typename T>
    constexpr int g(T x)
    {
        return x.get();
    }

    int main() {

      // O.K. The body of `f' is not required.
      decltype(f(0)) a;

      // Seems to instantiate the body of `g'
      // and results in an error.
      decltype(g(0)) b;

      return 0;
    }
    
In both cases the fundamental issue was eager instantiation of the body, which doesn't actually seem necessary to determine the results here. In neither example is the return type deduced.

These are incarnations of [CWG 1581](https://wg21.link/cwg1581), which dealt with the question of when, exactly, are `constexpr` functions defined. In the broken programs above, the `constexpr` functions (the non-`const` call operator of the binder object being returned in the first case and `g()` in the second) were eagerly instantiated, triggering hard compile errors, in cases where the program ultimately would not have required their instantiation. 

Thankfully, this difficult problem has been resolved by the adoption of [P0859](https://wg21.link/p0859) in Albuquerque, 2017. As a result, both of the above programs are valid. 

This issue was the blocker for having a `constexpr std::invoke()` due to this eager instantiation issue - which no longer exists. 

# Proposal

This proposal adds `constexpr` to the following <code><i>INVOKE</i></code>-related machinery: `invoke()`, `reference_wrapper<T>`, `not_fn()`, `bind()`, `bind_front()`, and `mem_fn()`. The remaining non-`constexpr` elements of the library that are <code><i>INVOKE</i></code>-adjacent are `function<Sig>`, `packaged_task<Sig>`, `async()`, `thread`, and `call_once()`.

This proposal resolves [LWG 2894](https://wg21.link/lwg2894), [LWG 2957](https://wg21.link/lwg2957), and [LWG 3023](https://wg21.link/lwg3023). The last is addressed by guaranteeing that call wrappers that are produced by `not_fn()` and `bind()` have the same type if their state entities have the same type (note that this guarantee does not imply any restriction on implementors). Thus the types of `f1`, `f2`, `f3`, and `f4` in the following example are now guaranteed to be the same:

    :::cpp
    auto func = [](std::string) {};
    std::string s("foo");
    auto f1 = std::bind(func, s);
    auto f2 = std::bind(std::as_const(func), std::as_const(s));
    auto f3 = std::bind(func, std::string("bar"));
    auto f4 = std::bind(std::move(func), std::move(s));

The wording uses the phrase "shall be constexpr functions" in a couple places. We don't seem to have a way to say that in Library, see also [LWG 2833](https://wg21.link/lwg2833) and [LWG 2289](https://wg21.link/lwg2289).

## Wording

Add `constexpr` to several places in the synopsis in 20.14.1 [functional.syn]

<blockquote><pre class="codehilite"><code class="language-cpp">namespace std {
  // [func.invoke], invoke
  template&lt;class F, class... Args>
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;F, Args...> invoke(F&& f, Args&&... args)
      noexcept(is_nothrow_invocable_v&lt;F, Args...>);

  // [refwrap], reference_wrapper
  template&lt;class T> class reference_wrapper;

  template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(T&) noexcept;
  template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(const T&) noexcept;
  template&lt;class T> void ref(const T&&) = delete;
  template&lt;class T> void cref(const T&&) = delete;

  template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(reference_wrapper&lt;T>) noexcept;
  template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(reference_wrapper&lt;T>) noexcept;
  
  // [arithmetic.operations], arithmetic operations
  // ...
  
  // [comparisons], comparisons
  // ...

  // [logical.operations], logical operations
  // ...

  // [bitwise.operations], bitwise operations
  // ...

  // [func.identity], identity
  // ...

  // [func.not_fn], function template not_fn
  template&lt;class F> </code><code><ins>constexpr</ins></code> <i>unspecified</i><code class="language-cpp"> not_fn(F&& f);
  
  // [func.bind.front], function template bind_front
  template&lt;class F, class... Args&gt; </code><code><ins>constexpr</ins></code> <i>unspecified</i><code class="language-cpp"> bind_front(F&&, Args&&...);  

  // [func.bind], bind
  template&lt;class T> struct is_bind_expression;
  template&lt;class T> struct is_placeholder;

  template&lt;class F, class... BoundArgs>
    </code><code><ins>constexpr</ins></code> <i>unspecified</i><code class="language-cpp"> bind(F&&, BoundArgs&&...);
  template&lt;class R, class F, class... BoundArgs>
    </code><code><ins>constexpr</ins></code> <i>unspecified</i><code class="language-cpp"> bind(F&&, BoundArgs&&...);

  namespace placeholders {
    // M is the implementation-defined number of placeholders
    see below _1;
    see below _2;
               .
               .
               .
    see below _M;
  }

  // [func.memfn], member function adaptors
  template&lt;class R, class T>
    </code><code><ins>constexpr</ins></code> <i>unspecified</i><code class="language-cpp"> mem_fn(R T::*) noexcept;

  // ...    
}</code></pre></blockquote>  

The definition of the *simple call wrapper* (used only for `mem_fn`) is changed to be a refinement of *perfect forwarding call wrapper*, instead of *argument forwarding call wrapper*. These make the invocation operator conditionally `constexpr` and `noexcept`. In addition we state explicitly the copy/move constructor/assignment of simple call wrapper is core constant expression. [ *Note*: The definition of simple call wrapper is still required to guarantee assignability. ]

The requirement of copy/move operation to be defined in terms of state entities is now extended to any argument forwarding call wrapper (as we define them for `not_fn` and `bind`).

Apply following changes to 20.14.3 [func.require]:

> Every call wrapper ([func.def]) <ins>meets the</ins><del>is</del> *Cpp17MoveConstructible* <ins>and *Cpp17Destructible* requirements</ins>. <del>A</del> <ins>An</ins> *argument forwarding call wrapper* is a call wrapper that can be called with an arbitrary argument list and delivers the arguments to the wrapped callable object as references. This forwarding step delivers rvalue arguments as rvalue references and lvalue arguments as lvalue references. <del>A *simple call wrapper* is an argument forwarding call wrapper that is *Cpp17CopyConstructible* and *Cpp17CopyAssignable* and whose copy constructor, move constructor, copy assignment operator, and move assignment operator do not throw exceptions.</del> [ *Note*: In a typical implementation, argument forwarding call wrappers have an overloaded function call operator of the form
> <pre><code class=language-cpp">template&lt;class... UnBoundArgs&gt;
  </code><code><ins>constexpr</ins></code><code class="language-cpp"> R operator()(UnBoundArgs&&... unbound_args) </code><code><i>cv-qual</i>;</code></pre>
> —*end note*]  
> 
> A *perfect forwarding call wrapper* is an argument forwarding call wrapper that forwards its state entities to the underlying call expression. This forwarding step delivers a state entity of type `T` as *cv* `T&` when the call is performed on an lvalue of the call wrapper type and as *cv* `T&&` otherwise, where *cv* represents the cv-qualifiers of the call wrapper and where *cv* shall be neither `volatile` nor `const volatile`.  
> 
> A *call pattern* defines the semantics of invoking a perfect forwarding call wrapper. A postfix call performed on a perfect forwarding call wrapper is expression-equivalent ([defns.expression-equivalent]) to an expression e determined from its call pattern cp by replacing all occurrences of the arguments of the call wrapper and its state entities with references as described in the corresponding forwarding steps.  
> 
> <ins>A *simple call wrapper* is a perfect forwarding call wrapper that meets the *Cpp17CopyConstructible* and *Cpp17CopyAssignable* and whose copy constructor, move constructor, and assignment operator are constexpr functions which do not throw exceptions.</ins>
> 
> The copy/move constructor of <del>a perfect</del> <ins>an argument</ins> forwarding call wrapper has the same apparent semantics as if memberwise copy/move of its state entities were performed ([class.copy.ctor]). [ *Note*: This implies that each of the copy/move constructors has the same *exception-specification* as the corresponding implicit definition and is declared as `constexpr` if the corresponding implicit definition would be considered to be constexpr. —*end note* ]
> 
> <del>Perfect</del> <ins>Argument</ins> forwarding call wrappers returned by a given standard library function template have the same type if the types of their corresponding state entities are the same.

Add `constexpr` to `std::invoke()` in 20.14.4 [func.invoke]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class F, class... Args>
</code>  <code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;F, Args...> invoke(F&& f, Args&&... args)
    noexcept(is_nothrow_invocable_v&lt;F, Args...>);</code></pre></blockquote>

Add `constexpr` to `std::reference_wrapper<T>` in 20.14.5 [refwrap]

<blockquote><pre class="codehilite"><code class="language-cpp">namespace std {
  template&lt;class T> class reference_wrapper {
  public:
    // types
    using type = T;
    
    // construct/copy/destroy
    template&lt;class U>
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(U&&) noexcept(see below );
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(const reference_wrapper& x) noexcept;
    
    // assignment
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper& operator=(const reference_wrapper& x) noexcept;
    
    // access
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">operator T& () const noexcept;
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">T& get() const noexcept;
    
    // invocation
    template&lt;class... ArgTypes>
    </code><code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;T&, ArgTypes...> operator()(ArgTypes&&...) const;
  };
  
  template&lt;class T>
  reference_wrapper(T&) -> reference_wrapper&lt;T>;
}</code></pre></blockquote>

And its corresponding subsections, 20.14.5.1 [refwrap.const]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class U>
</code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(U&& u) noexcept(see below );</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(const reference_wrapper& x) noexcept;</code></pre></blockquote>

20.14.5.2 [refwrap.assign]

<blockquote><pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper& operator=(const reference_wrapper& x) noexcept;</code></pre></blockquote>

20.14.5.3 [refwrap.access]

<blockquote><pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">operator T& () const noexcept;</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">T& get() const noexcept;</code></pre></blockquote>


20.14.5.4 [refwrap.invoke]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class... ArgTypes>
  </code><code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;T&, ArgTypes...>
    operator()(ArgTypes&&... args) const;</code></pre></blockquote>

and its helper functions, 20.14.5.5 [refwrap.helpers]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(T& t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>1 Returns</i>: <code class="language-cpp">reference_wrapper&lt;T>(t)</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(reference_wrapper&lt;T> t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>2 Returns</i>: <code class="language-cpp">ref(t.get())</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(const T& t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>3 Returns</i>: <code class="language-cpp">reference_wrapper&lt;const T>(t)</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(reference_wrapper&lt;T> t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>4 Returns</i>: <code class="language-cpp">cref(t.get())</code>.</blockquote>

Add `constexpr` to `std::not_fn()` in 20.14.12 [func.not.fn]:

> <pre><code class="language-cpp">template&lt;class F&gt; </code><code><ins>constexpr</ins> <i>unspecified</i></code><code class="language-cpp"> not_fn(F&& f);</code></pre>

Add `constexpr` to `std::bind_front()` in 20.14.13 [func.bind.front]:

> <pre><code class="language-cpp">template&lt;class F, class... Args&gt;
  </code><code><ins>constexpr</ins> <i>unspecified</i></code><code class="language-cpp"> bind_front(F&& f, Args&&... args);</code></pre>

Apply the following changes to `std::bind()` in 20.14.14.3 [func.bind.bind], merging `bind` and `bind<R>`:

> In the text that follows:
> 
> - <ins>`g` is a value of the result of a `bind` invocation,</ins>
> - `FD` is the type `decay_t<F>`,
> - `fd` is <del>an lvalue of type `FD` constructed from `std::forward<F>(f)`,</del> <ins>a target object of `g` ([func.def]) of type `FD` direct-non-list-initialized with `std::forward<F>(f)`,</ins>
> - <code>T<sub>i</sub></code> is the `i`th type in the template parameter pack `BoundArgs`,
> - <code>TD<sub>i</sub></code> is the type <code>decay_t&lt;T<sub>i</sub>&gt;</code>,
> - <code>t<sub>i</sub></code> is the `i`th argument in the function parameter pack `bound_args`,
> - <code>td<sub>i</sub></code> is <del>an lvalue of type <code>TD<sub>i</sub></code> constructed from <code>std::forward&lt;T<sub>i</sub>&gt;(t<sub>i</sub>)</code>,</del> <ins>a bound argument entity of `g` ([func.def]) of type <code>TD<sub>i</sub></code> direct-non-list-initialized with <code>std::forward&lt;T<sub>i</sub>&gt;(t<sub>i</sub>)</code>,</ins>
> - <code>U<sub>j</sub></code> is the `j`th deduced type of the `UnBoundArgs&&...` parameter of the argument forwarding call wrapper, and
> - <code>u<sub>j</sub></code> is the `j`th argument associated with <code>U<sub>j</sub></code>.
> 
> <pre><code class="language-cpp">template&lt;class F, class... BoundArgs&gt;
  </code><code><ins>constexpr</ins> <i>unspecified</i></code><code class="language-cpp"> bind(F&& f, BoundArgs&&... bound_args);
</code><code><ins>template&lt;class R, class F, class... BoundArgs&gt;
  constexpr <i>unspecified</i> bind(F&& f, BoundArgs&&... bound_args);</code></pre>
> 
> <del>*Requires*</del> <ins>*Mandates*</ins>: `is_constructible_v<FD, F>` <del>shall be</del> <ins>is</ins> `true`. For each <code>T<sub>i</sub></code> in `BoundArgs`, <code>is_constructible_v&lt;TD<sub>i</sub>, T<sub>i</sub>&gt;</code> <del>shall be</del> <ins>is</ins> `true`.

> <ins>*Expects*: `FD` and each <code>TD<sub>i</sub></code> meets the of *Cpp17MoveConstructible* and *Cpp17Destructible* requirements.</ins> <code>INVOKE(fd, w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub>)</code> ([func.require]) <del>shall be</del> <ins>is</ins> a valid expression for some values <code>w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub></code>, where `N` has the value `sizeof...(bound_args)`. <del>The cv-qualifiers *cv* of the call wrapper `g`, as specified below, shall be neither `volatile` nor `const volatile`.</del>
> 
> *Returns*: An argument forwarding call wrapper `g` ([func.require]). <ins>A program that attempts to invoke a volatile-qualified `g` is ill-formed. When `g` is not volatile-qualified,</ins><del>The effect of</del> <ins>invocation</ins> <code>g(u<sub>1</sub>, u<sub>2</sub>, …, u<sub>M</sub>)</code> <del>shall be</del> <ins>is expression-equivalent ([defns.expression-equivalent]) to</ins> <del><code>INVOKE(fd, std::forward&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), std::forward&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …, std::forward&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code></del> <ins><code>INVOKE(static_cast&lt;V<sub>fd</sub>&gt;(v<sub>fd</sub>), static_cast&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), static_cast&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …, static_cast&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code> for the first overload, and <code>INVOKE&lt;R&gt;(static_cast&lt;V<sub>fd</sub>&gt;(v<sub>fd</sub>), static_cast&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), static_cast&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …, static_cast&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code> for the second overload,</ins> where the values and types of <ins>the target argument <code>v<sub>fd</sub></code> and of</ins> the bound arguments <code>v<sub>1</sub>, v<sub>2</sub>, …, v<sub>N</sub></code> are determined as specified below. <del>The copy constructor and move constructor of the argument forwarding call wrapper shall throw an exception if and only if the corresponding constructor of `FD` or of any of the types <code>TD<sub>i</sub></code> throws an exception.</del>
> 
> *Throws*: <del>Nothing unless the construction of `fd` or of one of the values <code>td<sub>i</sub></code> throws an exception.</del> <ins>Any exception thrown by the initialization of the state entities of `g`.</ins>
> 
> <del>*Remarks*: The return type shall satisfy the *Cpp17MoveConstructible* requirements. If all of `FD` and <code>TD<sub>i</sub></code> satisfy the *Cpp17CopyConstructible* requirements, then the return type shall satisfy the *Cpp17CopyConstructible* requirements. [*Note*: This implies that all of <code>FD</code> and <code>TD<sub>i</sub></code> are *Cpp17MoveConstructible*. —*end note*]</del>
> 
> <ins>[*Note*: If all of `FD` and <code>TD<sub>i</sub></code> meet the requirements of *Cpp17CopyConstructible*, then the return type meets the requirements of *Cpp17CopyConstructible*. -*end note*]</ins>

> <pre><code><del>template&lt;class R, class F, class... BoundArgs&gt;
  unspecified bind(F&& f, BoundArgs&&... bound_args);</del></code></pre>
> <del>*Requires*: `is_constructible_v<FD, F>` shall be `true`. For each <code>T<sub>i</sub></code> in `BoundArgs`, <code>is_constructible_v&lt;TD<sub>i</sub>, T<sub>i</sub>&gt;</code> shall be true. <code>INVOKE(fd, w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub>)</code> ([func.require]) shall be a valid expression for some values <code>w<sub>1</sub>, w<sub>2</sub>, …, w<sub>N</sub></code>, where `N` has the value `sizeof...(bound_args)`. The cv-qualifiers *cv* of the call wrapper `g`, as specified below, shall be neither `volatile` nor `const volatile`.</del>
> 
> <del>*Returns*: An argument forwarding call wrapper g ([func.require]). The effect of <code>g(u<sub>1</sub>, u<sub>2</sub>, …, u<sub>M</sub>)</code> shall be <code>INVOKE&lt;R&gt;(fd, std::forward&lt;V<sub>1</sub>&gt;(v<sub>1</sub>), std::forward&lt;V<sub>2</sub>&gt;(v<sub>2</sub>), …, std::forward&lt;V<sub>N</sub>&gt;(v<sub>N</sub>))</code> where the values and types of the bound arguments <code>v<sub>1</sub>, v<sub>2</sub>, …, v<sub>N</sub></code> are determined as specified below. The copy constructor and move constructor of the argument forwarding call wrapper shall throw an exception if and only if the corresponding constructor of `FD` or of any of the types <code>TD<sub>i</sub></code> throws an exception.</del>
> 
> <del>*Throws*: Nothing unless the construction of `fd` or of one of the values <code>td<sub>i</sub></code> throws an exception.</del>
> 
> <del>*Remarks*: The return type shall satisfy the *Cpp17MoveConstructible* requirements. If all of `FD` and <code>TD<sub>i</sub></code> satisfy the *Cpp17CopyConstructible* requirements, then the return type shall satisfy the *Cpp17CopyConstructible* requirements. [*Note*: This implies that all of <code>FD</code> and <code>TD<sub>i</sub></code> are *Cpp17MoveConstructible*. —*end note*]</del>

Define <code>v<sub>fd</sub></code> and add reference to the *cv*-qualifies in 20.14.14.3 [func.bind.bind]/10: 

> The values of the *bound arguments* <code>v<sub>1</sub></code>, <code>v<sub>2</sub></code>, ..., <code>v<sub>N</sub></code> and their corresponding types <code>V<sub>1</sub></code>, <code>V<sub>2</sub></code>, ..., <code>V<sub>N</sub></code> depend on the types <code>TD<sub><i>i</i></sub></code> derived from the call to bind and the cv-qualifiers *cv* of the call wrapper `g` as follows:
> 
> - if <code>TD<sub><i>i</i></sub></code> is `reference_wrapper<T>`, [...]
> - if the value of <code>is_bind_expression_v&lt;TD<sub><i>i</i></sub>&gt;</code> is `true`, the argument is <code><del>td<sub><i>i</i></sub></del> <ins>static_cast&lt;TD<sub><i>i</i></sub> <i>cv</i> &&gt;</ins>(std::forward&lt;U<sub>j</sub>&gt;(u<sub>j</sub>)...)</code> and its type <code>V<sub><i>i</i></sub></code> is <code>invoke_result_t&lt;TD<sub><i>i</i></sub> <i>cv</i> &, U<sub><i>j</i></sub>...&gt;&&</code>;
> - if the value `j` of [...] 
> - otherwise, [...]
>
> <ins>The value of the <i>target argument</i> <code>v<sub>fd</sub></code> is <code>fd</code> and its corresponding type <code>V<sub>fd</sub></code> is <code>FD <i>cv</i> &</code>.</ins>

Add constant requirement to the placeholders in 20.14.14.4 [func.bind.place]/1:

> All placeholder types <ins>meet the</ins><del>shall be</del> *Cpp17DefaultConstructible* and *Cpp17CopyConstructible*<ins> requirements</ins>, and their default constructors and copy/move constructors <ins>are constexpr functions which do</ins><del>shall</del> not throw exceptions. It is implementation-defined whether placeholder types <ins>meet the</ins><del>are</del> *Cpp17CopyAssignable*<ins> requirements, but if so, their</ins><del>. *Cpp17CopyAssignable* placeholders'</del> copy assignment operators <ins>are constexpr functions which do</ins><del>shall</del> not throw exceptions.
  
Add `constexpr` to `std::mem_fn()` in 20.14.15 [func.memfn]

> <pre class="codehilite"><code class="language-cpp">template&lt;class R, class T> </code><code><ins>constexpr</ins> <i>unspecified</i> </code><code class="language-cpp">mem_fn(R T::* pm) noexcept;</code></pre>

> *Returns*: A simple call wrapper `fn` <del>such that the expression <code>fn(t, a<sub>2</sub>, …, a<sub>N</sub>)</code> is equivalent to <code>INVOKE(pm, t, a<sub>2</sub>, …, a<sub>N</sub>)</code> ([func.require]).</del> <ins>with call pattern `invoke(pmd, call_args...)`, where `pmd` is the target object of `fn` of type `R T::*` direct-non-list-initialized with `pm`, and `call_args` is an argument pack used in a function call expression ([expr.call]) of `pm`.</ins>

# Acknowledgements

Thanks to Casey Carter and Agustín Bergé for going over the history of issues surrounding `constexpr invoke` and suggesting that this proposal be written. Thanks to Daniel Krügler, Tim Song and  Casey Carter for help on the wording.
