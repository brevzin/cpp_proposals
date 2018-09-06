Title: constexpr INVOKE
Document-Number: D1065R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Audience: LEWG, LWG

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

This proposal adds `constexpr` to the following <code><i>INVOKE</i></code>-related machinery: `invoke()`, `reference_wrapper<T>`, `not_fn()`, `bind()`, and `mem_fn()`. The remaining non-`constexpr` elements of the library that are <code><i>INVOKE</i></code>-adjacent are `function<Sig>`, `packaged_task<Sig>`, `async()`, `thread`, and `call_once()`.

The entirety of the wording is the addition of the `constexpr` keyword in 22 places.

## Wording

Add `constexpr` to `std::invoke()` in 19.14.4 [func.invoke]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class F, class... Args>
</code>  <code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;F, Args...> invoke(F&& f, Args&&... args)
    noexcept(is_nothrow_invocable_v&lt;F, Args...>);</code></pre></blockquote>

Add `constexpr` to `std::reference_wrapper<T>` in 19.14.5 [refwrap]

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

And its corresponding subsections, 19.14.5.1 [refwrap.const]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class U>
</code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(U&& u) noexcept(see below );</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper(const reference_wrapper& x) noexcept;</code></pre></blockquote>

19.14.5.2 [refwrap.assign]

<blockquote><pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper& operator=(const reference_wrapper& x) noexcept;</code></pre></blockquote>

19.14.5.3 [refwrap.access]

<blockquote><pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">operator T& () const noexcept;</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp"></code><code><ins>constexpr</ins></code> <code class="language-cpp">T& get() const noexcept;</code></pre></blockquote>


19.14.5.4 [refwrap.invoke]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class... ArgTypes>
  </code><code><ins>constexpr</ins></code> <code class="language-cpp">invoke_result_t&lt;T&, ArgTypes...>
    operator()(ArgTypes&&... args) const;</code></pre></blockquote>

and its helper functions, 19.14.5.5 [refwrap.helpers]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(T& t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>1 Returns</i>: <code class="language-cpp">reference_wrapper&lt;T>(t)</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;T> ref(reference_wrapper&lt;T> t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>2 Returns</i>: <code class="language-cpp">ref(t.get())</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(const T& t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>3 Returns</i>: <code class="language-cpp">reference_wrapper&lt;const T>(t)</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;class T> </code><code><ins>constexpr</ins></code> <code class="language-cpp">reference_wrapper&lt;const T> cref(reference_wrapper&lt;T> t) noexcept;</code></pre>
<span style="margin-left:2em;" /><i>4 Returns</i>: <code class="language-cpp">cref(t.get())</code>.</blockquote>

Add `constexpr` to `std::not_fn` in 19.14.11 [func.not_fn]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class F> </code><code><ins>constexpr</ins></code> <i>unspecified</i> <code class="language-cpp">not_fn(F&& f);</code></pre>
<span style="margin-left:2em;" /><i>1 Effects</i>: Equivalent to: <pre class="inline"><code class="language-cpp">return </code><code><i>call_wrapper</i></code><code class="language-cpp">(std::forward&lt;F>(f));</code></pre> where <code><i>call_wrapper</i></code> is an exposition only class defined as follows:
<pre class="codehilite" style="margin-left:2em"><code class="language-cpp">class </code><code><i>call_wrapper</i></code><code class="language-cpp"> {
  using FD = decay_t&lt;F>;
  FD fd;
  
  explicit </code><code><ins>constexpr</ins> <i>call_wrapper</i></code><code class="language-cpp">(F&& f);  
public:
  </code><code><ins>constexpr</ins> <i>call_wrapper</i>(<i>call_wrapper</i></code><code class="language-cpp"> &&) = default;
  </code><code><ins>constexpr</ins> <i>call_wrapper</i>(</code><code class="language-cpp">const</code><code> <i>call_wrapper</i></code><code class="language-cpp">&) = default;
  
  template&lt;class... Args>
    </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) &
      -> decltype(!declval&lt;invoke_result_t&lt;FD&, Args...>>());
      
  template&lt;class... Args>
    </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) const&
      -> decltype(!declval&lt;invoke_result_t&lt;const FD&, Args...>>());
      
  template&lt;class... Args>
    </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) &&
      -> decltype(!declval&lt;invoke_result_t&lt;FD, Args...>>());
      
  template&lt;class... Args>
    </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) const&&
      -> decltype(!declval&lt;invoke_result_t&lt;const FD, Args...>>());
};</code></pre>

<pre class="codehilite"><code class="language-cpp">explicit </code><code><ins>constexpr</ins></code> <code class="language-cpp"><i>call_wrapper</i>(F&& f);</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp">template&lt;class... Args>
  </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) &
    -> decltype(!declval&lt;invoke_result_t&lt;FD&, Args...>>());
      
template&lt;class... Args>
  </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) const&
    -> decltype(!declval&lt;invoke_result_t&lt;const FD&, Args...>>());</code></pre>
<span style="margin-left:2em;" /><i>5 Effects</i>: Equivalent to: <pre class="codehilite" style="margin-left:3em"><code class="language-cpp">return !</code><code><i>INVOKE</i></code><code class="language-cpp">(fd, std::forward&lt;Args>(args)...); // see 19.14.3</code></pre>

<pre class="codehilite"><code class="language-cpp">template&lt;class... Args>
  </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) &&
    -> decltype(!declval&lt;invoke_result_t&lt;FD&, Args...>>());
      
template&lt;class... Args>
  </code><code><ins>constexpr</ins></code><code class="language-cpp"> auto operator()(Args&&...) const&&
    -> decltype(!declval&lt;invoke_result_t&lt;const FD&, Args...>>());</code></pre>
<span style="margin-left:2em;" /><i>6 Effects</i>: Equivalent to: <pre class="codehilite" style="margin-left:3em"><code class="language-cpp">return !</code><code><i>INVOKE</i></code><code class="language-cpp">(std::move(fd), std::forward&lt;Args>(args)...); // see 19.14.3</code></pre>
</blockquote>

Add `constexpr` to `std::bind()` in 19.14.12.3 [func.bind.bind]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class F, class... BoundArgs>
  </code><code><ins>constexpr</ins> <i>unspecified</i></code><code class="language-cpp"> bind(F&& f, BoundArgs&&... bound_args);</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp">template&lt;class R, class F, class... BoundArgs>
  </code><code><ins>constexpr</ins> <i>unspecified</i></code><code class="language-cpp"> bind(F&& f, BoundArgs&&... bound_args);</code></pre></blockquote>
  
Add `constexpr` to `std::mem_fn()` in 19.14.13 [func.memfn]

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class R, class T> </code><code><ins>constexpr</ins> <i>unspecified</i> </code><code class="language-cpp">mem_fn(R T::* pm) noexcept;</code></pre></blockquote>

# Acknowledgements

Thanks to Casey Carter and Agustín Bergé for going over the history of issues surrounding `constexpr invoke` and suggesting that this proposal be written.