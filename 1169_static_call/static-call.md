Title: static operator()
Document-Number: D1169R0
Authors: Barry Revzin, barry dot revzin at gmail dot com
Authors: Casey Carter, casey at carter dot net
Audience: EWG, LEWG

# Motivation

The standard libary has always accepted arbitrary function objects - whether to be unary or binary predicates, or perform arbitrary operations. Function objects with call operator templates in particular have a significant advantage today over using overload sets since you can just pass them into algorithms. This makes, for instance, `std::less<>{}` very useful.

As part of the Ranges work, more and more function objects are being added to the standard library - the set of Customization Point Objects (CPOs). These objects are Callable, but they don't, as a rule, have any members. They simply exist to do what Eric Niebler termed the ["Std Swap Two-Step"](http://ericniebler.com/2014/10/21/customization-point-design-in-c11-and-beyond/). Nevertheless, the call operators of all of these types are non-static member functions. Because _all_ call operators have to be non-static member functions. 

What this means is that if the call operator happens to not be inlined, an extra register must be used to pass in the `this` pointer to the object - even if there is no need for it whatsoever. Here is a [simple example](https://godbolt.org/z/ajTZo2):

    :::cpp
    struct X {
        bool operator()(int) const;
        static bool f(int);
    };

    inline constexpr X x;

    int count_x(std::vector<int> const& xs) {
        return std::count_if(xs.begin(), xs.end(),
    #ifdef STATIC
        X::f
    #else
        x
    #endif
        );
    }    

`x` is a global function object that has no members that is intended to be passed into various algorithms. But in order to work in algorithms, it needs to have a call operator - which must be non-static. You can see the difference in the generated asm btween using the function object as intended and passing in an equivalent static member function:

<table style="width:100%">
<tr>
<th style="width:50%">
Non-static call operator
</th>
<th style="width:50%">
Static member function
</th>
</tr>
<tr>
<td>
    :::nasm
    count_x(std::vector<int, std::allocator<int> > const&):
            push    r12
            push    rbp
            push    rbx
            sub     rsp, 16
            mov     r12, QWORD PTR [rdi+8]
            mov     rbx, QWORD PTR [rdi]
            mov     BYTE PTR [rsp+15], 0
            cmp     r12, rbx
            je      .L5
            xor     ebp, ebp
    .L4:
            mov     esi, DWORD PTR [rbx]
            lea     rdi, [rsp+15]
            call    X::operator()(int) const
            cmp     al, 1
            sbb     rbp, -1
            add     rbx, 4
            cmp     r12, rbx
            jne     .L4
            add     rsp, 16
            mov     eax, ebp
            pop     rbx
            pop     rbp
            pop     r12
            ret
    .L5:
            add     rsp, 16
            xor     eax, eax
            pop     rbx
            pop     rbp
            pop     r12
            ret    
</td>
<td>
    :::nasm
    count_x(std::vector<int, std::allocator<int> > const&):
            push    r12
            push    rbp
            push    rbx
            mov     r12, QWORD PTR [rdi+8]
            mov     rbx, QWORD PTR [rdi]
            cmp     r12, rbx
            je      .L5
            xor     ebp, ebp
    .L4:
            mov     edi, DWORD PTR [rbx]
            call    X::f(int)
            cmp     al, 1
            sbb     rbp, -1
            add     rbx, 4
            cmp     r12, rbx
            jne     .L4
            mov     eax, ebp
            pop     rbx
            pop     rbp
            pop     r12
            ret
    .L5:
            pop     rbx
            xor     eax, eax
            pop     rbp
            pop     r12
            ret    
</td>
</tr>
</table>

Even in this simple example, you can see the extra zeroing out of `[rsp+15]`, the extra `lea` to move that zero-ed out area as the object parameter - which we know doesn't need to be used. This is wasteful, and seems to violate the fundamental philosophy that we don't pay for what we don't need.

The typical way to express the idea that we don't need an object parameter is to declare functions `static`. We just don't have that ability in this case.

# Proposal

The proposal is to just allow the ability to make the call operator a static member function, instead of requiring it to be a non-static member function. We have many years of experience with member-less function objects being useful. Let's remove the unnecessary object parameter overhead.

There are other operators that are currently required to be implemented as non-static member functions - all the unary operators, assignment, subscripting, and class member access. We do not believe that being able to declare any of these as static will have as much value, so we are not pursuing those at this time. 

## Language Wording

Change 7.5.5.1 [expr.prim.lambda.closure] paragraph 4:

> The function call operator or operator template is declared <ins>`static` if the *lambda-expression* has no *capture*, otherwise it is non-static. If it is a non-static member function, it is declared</ins>`const` ([class.mfct.non-static]) if and only if the *lambda-expression*'s *parameter-declaration-clause* is not followed by `mutable`. It is neither virtual nor declared `volatile`. Any *noexcept-specifier* specified on a *lambda-expression* applies to the corresponding function call operator or operator template. An *attribute-specifier-seq* in a *lambda-declarator* appertains to the type of the corresponding function call operator or operator template. The function call operator or any given operator template specialization is a `constexpr` function if either the corresponding *lambda-expression*'s *parameter-declaration-clause* is followed by `constexpr`, or it satisfies the requirements for a `constexpr` function. 

Change 11.5 [over.oper] paragraph 6:

> An operator function shall either be a <del>non-static</del> member function or be a non-member function that has at least one parameter whose type is a class, a reference to a class, an enumeration, or a reference to an enumeration. It is not possible to change the precedence, grouping, or number of operands of operators. The meaning of the operators `=`, (unary) `&`, and `,` (comma), predefined for each type, can be changed for specific class and enumeration types by defining operator functions that implement these operators. Operator functions are inherited in the same manner as other base class functions.

Change 11.5.4 [over.call] paragraph 1:

> `operator()` shall be a <del>non-static</del> member function with an arbitrary number of parameters. It can have default arguments. [...]

## Library Wording

There are many function objects in the standard library which could have their call operators be declared static instead of non-static. 

In 19.11.1.1.2 [unique.ptr.dltr.dflt], `default_delete<T>`:

<blockquote><pre class="codehilite"><code class="language-cpp">namespace std {
  template&lt;class T> struct default_delete {
    constexpr default_delete() noexcept = default;
    template&lt;class U> default_delete(const default_delete&lt;U>&) noexcept;
    </code><code><ins>static </ins></code><code class="language-cpp">void operator()(T*)</code><code><del> const</del>;
  };
}</code></pre>
[...]
<pre class="codehilite"><code><ins>static </ins></code><code class="language-cpp">void operator()(T* ptr)</code><code><del> const</del>;</code></pre>
</blockquote>

In 19.11.1.1.3 [unique.ptr.dltr.dflt1], `default_delete<T[]>`:

<blockquote><pre class="codehilite"><code class="language-cpp">namespace std {
  template&lt;class T> struct default_delete&lt;T[]> {
    constexpr default_delete() noexcept = default;
    template&lt;class U> default_delete(const default_delete&lt;U[]>&) noexcept;
    template&lt;class U> </code><code><ins>static </ins></code><code class="language-cpp">void operator()(U*)</code><code><del> const</del>;
  };
}</code></pre>
[...]
<pre class="codehilite"><code class="language-cpp">template&lt;class U> </code><code><ins>static </ins></code><code class="language-cpp">void operator()(U*)</code><code><del> const</del>;</code></pre>
</blockquote>

In 19.11.5 [util.smartptr.ownerless], `owner_less<T>`:

<blockquote><pre class="codehilite"><code class="language-cpp">namespace std {
  template&lt;class T = void> struct owner_less;
  
  template&lt;class T> struct owner_less&lt;shared_ptr&lt;T>> {
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const shared_ptr&lt;T>&, const shared_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const shared_ptr&lt;T>&, const weak_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const weak_ptr&lt;T>&, const shared_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
  };
  
  template&lt;class T> struct owner_less<weak_ptr&lt;T>> {
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const weak_ptr&lt;T>&, const weak_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const shared_ptr&lt;T>&, const weak_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const weak_ptr&lt;T>&, const shared_ptr&lt;T>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
  };
  
  template&lt;> struct owner_less&lt;void> {
    template&lt;class T, class U>
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const shared_ptr&lt;T>&, const shared_ptr&lt;U>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    template&lt;class T, class U>
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const shared_ptr&lt;T>&, const weak_ptr&lt;U>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    template&lt;class T, class U>
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const weak_ptr&lt;T>&, const shared_ptr&lt;U>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    template&lt;class T, class U>
    </code><code><ins>static </ins></code><code class="language-cpp">bool operator()(const weak_ptr&lt;T>&, const weak_ptr&lt;U>&)</code><code><del> const</del></code><code class="language-cpp"> noexcept;
    
    using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
  };
}</code></pre></blockquote>

In 19.14.6.1 [arithmetic.operations.plus], `plus`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct plus {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x + y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct plus&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) + std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) + std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) + std::forward&lt;U>(u)</code>.</blockquote>

In 19.14.6.2 [arithmetic.operations.minus], `minus`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct minus {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x - y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct minus&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) - std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) - std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) - std::forward&lt;U>(u)</code>.</blockquote>

In 19.14.6.3 [arithmetic.operations.multiplies], `multiplies`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct multiplies {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x * y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct multiplies&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) * std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) * std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) * std::forward&lt;U>(u)</code>.</blockquote>

In 19.14.6.4 [arithmetic.operations.divides], `divides`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct divides {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x / y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct divides&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) / std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) / std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) / std::forward&lt;U>(u)</code>.</blockquote>

In 19.14.6.5 [arithmetic.operations.modulus], `modulus`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct modulus {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x % y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct modulus&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) % std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) % std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) % std::forward&lt;U>(u)</code>.</blockquote>

In 19.14.6.6 [arithmetic.operations.negate], `negate`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct negate {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">-x</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct negate&lt;void> {
  template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(-std::forward&lt;T>(t));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(-std::forward&lt;T>(t));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">-std::forward&lt;T>(t)</code>.</blockquote>

In 19.14.7.1 [comparisons.equal_to], `equal_to`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct equal_to {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x == y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct equal_to&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) == std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) == std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) == std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.7.2 [comparisons.not_equal_to], `not_equal_to`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct not_equal_to {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x != y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct not_equal_to&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) != std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) != std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) != std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.7.3 [comparisons.greater], `greater`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct greater {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x > y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct greater&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) > std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) > std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) > std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.7.4 [comparisons.less], `less`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct less {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x < y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct less&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) < std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) < std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) < std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.7.5 [comparisons.greater_equal], `greater_equal`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct greater_equal {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x >= y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct greater_equal&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) >= std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) >= std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) >= std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.7.6 [comparisons.less_equal], `less_equal`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct less_equal {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x <= y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct less_equal&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) <= std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) <= std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) <= std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.8.1 [logical.operations.and], `logical_and`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct logical_and {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x && y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct logical_and&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) && std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) && std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) && std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.8.2 [logical.operations.or], `logical_or`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct logical_or {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x || y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct logical_or&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) || std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) || std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) || std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.8.3 [logical.operations.not], `logical_not`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct logical_not {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">!x</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct logical_not&lt;void> {
  template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(!std::forward&lt;T>(t));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(!std::forward&lt;T>(t));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">!std::forward&lt;T>(t)</code>.</blockquote>

In 19.14.9.1 [bitwise.operations.and], `bit_and`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct bit_and {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x & y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct bit_and&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) & std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) & std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) & std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.9.2 [bitwise.operations.or], `bit_or`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct bit_or {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x | y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct bit_or&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) | std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) | std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) | std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.9.3 [bitwise.operations.xor], `bit_xor`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct bit_xor {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x, const T& y)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">x ^ y</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct bit_xor&lt;void> {
  template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) ^ std::forward&lt;U>(u));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T, class U> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t, U&& u)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(std::forward&lt;T>(t) ^ std::forward&lt;U>(u));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">std::forward&lt;T>(t) ^ std::forward&lt;U>(u)</code>.</blockquote>


In 19.14.9.4 [bitwise.operations.not], `bit_not`

<blockquote><pre class="codehilite"><code class="language-cpp">template&lt;class T = void> struct bit_not {
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;
};
</code><code><ins>static </ins></code><code class="language-cpp">constexpr T operator()(const T& x)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Returns</i>: <code class="language-cpp">~x</code>.
<pre class="codehilite"><code class="language-cpp">template&lt;> struct bit_not&lt;void> {
  template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(~std::forward&lt;T>(t));
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T> </code><code><ins>static </ins></code><code class="language-cpp">constexpr auto operator()(T&& t)</code><code><del> const</del></code><code class="language-cpp">
    -> decltype(~std::forward&lt;T>(t));</code></pre>
<span style="margin-left:2em"><i>2 Returns</i>: <code class="language-cpp">~std::forward&lt;T>(t)</code>.</blockquote>


In 19.14.10 [func.identity], `identity`

<blockquote><pre class="codehilite"><code class="language-cpp">struct identity {
  template&lt;class T>
    </code><code><ins>static </ins></code><code class="language-cpp">constexpr T&& operator()(T&& x)</code><code><del> const</del></code><code class="language-cpp">;
    
  using is_transparent = </code><code><i>unspecified</i></code><code class="language-cpp">;
};
template&lt;class T>
  </code><code><ins>static </ins></code><code class="language-cpp">constexpr T&& operator()(T&& x)</code><code><del> const</del></code><code class="language-cpp">;</code></pre>
<span style="margin-left:2em" /><i>1 Effects</i>: Equivalent to <code class="language-cpp">return std::forward&lt;T>(t)</code>.</blockquote>


