---
title: "Pipe support for user-defined range adaptors"
document: DxxxxR0
date: today
audience: LEWG
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: true
---

## NanoRange

In Tristan Brindle's [@NanoRange], we have the following approach. NanoRange uses the acronyms `raco` for **R**ange **A**daptor **C**losure **O**bject and `rao` for **R**ange **A**daptor **O**bject (both defined in [range.adaptor.object]{.sref}).

```cpp
namespace nano::detail {
    // variable template to identify a Range Adaptor Closure Object
    template <typename>
    inline constexpr bool is_raco = false;

    // support for R | C to evaluate as C(R)
    template <viewable_range R, typename C>
        requires is_raco<remove_cvref_t<C>>
              && invocable<C, R>
    constexpr auto operator|(R&& lhs, C&& rhs) -> decltype(auto) {
        return FWD(rhs)(FWD(lhs));
    }
    
    // a type to handle merging two Range Adaptor Closure Objects together
    template <typename LHS, typename RHS>
    struct raco_pipe {
        LHS lhs;
        RHS rhs;

        // ...
        
        template <viewable_range R>
            requires invocable<LHS&, R>
                  && invocable<RHS&, invoke_result_t<LHS&, R>>
        constexpr auto operator()(R&& r) const {
            return rhs(lhs(FWD(r)));
        }
        
        // ...
    };
    
    // ... which is itself a Range Adaptor Closure Object
    template <typename LHS, typename RHS>
    inline constexpr bool is_raco<raco_pipe<LHS, RHS>> = true;
    
    // support for C | D to produce a new Range Adaptor Closure Object
    // so that (R | C) | D and R | (C | D) can be equivalent
    template <typename LHS, typename RHS>
        requires is_raco<remove_cvref_t<LHS>>
              && is_raco<remove_cvref_t<RHS>>
    constexpr auto operator|(LHS&&, RHS&&) {
        return raco_pipe<decay_t<LHS>, decay_t<RHS>>(FWD(lhs), FWD(rhs));
    }
    
    // ... and a convenience type for creating range adaptor objects
    template <typename F>
    struct rao_proxy : F {
        constexpr explicit rao_proxy(F&& f) : F(std::move(f)) { }
    };
    
    template <typename F>
    inline constexpr bool is_raco<rao_proxy<F>> = true;
}
```

And with that out of the way, NanoRange can create range adaptors fairly easily whether or not the range adaptor does not take any extra arguments (as in `join`) or does (as in `transform`). Note that `join_view_fn` _must_ be in the `nano::detail` namespace in order for `rng | nano::views::join` to find the appropriate `operator|` (but `transform_view_fn` does not, since with `rng | nano::views::transform(f)` the invocation of `transform(f)` returns a `rao_proxy` which is itself a type in the `nano::detail` namespace):

::: cmptable
### `join`
```cpp
namespace nano::detail {
  struct join_view_fn {
    template <viewable_range R>
        requires /* ... */
    constexpr auto operator(R&& r)
        -> join_view<all_t<R>>;
  };
  
  template <>
  inline constexpr is_raco<join_view_fn> = true;
}

namespace nano::views {
  // for user consumption
  inline constexpr detail::join_view_fn join{};
}
```

### `transform`
```cpp
namespace nano::detail {
  struct transform_view_fn_base {
    // the overload that has all the information
    template <viewable_range E, typename F>
        requires /* ... */
    constexpr auto operator()(E&& e, F&& f) const
        -> transform_view<all_t<E>, decay_t<F>>;
    
    // the partial overload
    template <typename F>
    constexpr auto operator()(F f) const {
        return rao_proxy{
            [f=move(f)](viewable_range auto&& r)
                requires /* ... */
            {
                return /* ... */;
            }};
    }
  };
}

namespace nano::views {
  // for user consumption
  inline constexpr detail::transform_view_fn transform{};
}
```
:::

Although practically speaking, users will not just copy again the constraints for `struct meow_view_fn` that they had written for `struct meow_view`. Indeed, NanoRange does not do this. Instead, it uses the trailing-return-type based SFINAE to use the underlying range adaptor's constraints. So `join_view_fn` actually looks like this (and it's up to `join_view` to express its constraints properly:

```cpp
struct join_view_fn {
    template <typename E>
    constexpr auto operator()(E&& e) const
        -> decltype(join_view{FWD(e)})
    {
        return join_view{FWD(e)};
    }
};
```

## range-v3

In [@range-v3], the approach is a bit more involved but still has the same kind of structure. Here, we have three types: `view_closure<F>` inherits from `view_closure_base` inherits from `view_closure_base_` (the latter of which is an empty class).

`view_closure<F>` is a lot like NanoRange's `rao_proxy<F>`, just with an extra base class:

```cpp
template <typename ViewFn>
struct view_closure : view_closure_base, ViewFn
{
    view_closure() = default;
    
    constexpr explicit view_closure(ViewFn fn) : ViewFn(std::move(fn)) { }
};
```

The interesting class is the intermediate `view_closure_base`, which has all the functionality:


```cpp
namespace ranges::views {
    // this type is its own namespace for ADL inhibition
    namespace view_closure_base_ns { struct view_closure_base; }
    using view_closure_base_ns::view_closure_base;    
    namespace detail { struct view_closure_base_; }
    
    // Piping a value into a range adaptor closure object should not yield another closure
    template <typename ViewFn, typename Rng>
    concept invocable_view_closure =
        invocable<ViewFn, Rng>
        && (not derived_from<invoke_result_t<ViewFn, Rng>, detail::view_closure_base_>);
    
    struct view_closure_base_ns::view_closure_base : detail::view_closure_base_ {
        // support for R | C to evaluate as C(R)
        template <viewable_range R, invocable_view_closure<R> ViewFn>
        friend constexpr auto operator|(R&& rng, view_closure<ViewFn> vw) {
            return std::move(vw)(FWD(rng));
        }
        
        // for diagnostic purposes, we delete the overload for R | C
        // if R is a range but not a viewable_range
        template <range R, typename ViewFn>
            requires (not viewable_range<R>)
        friend constexpr auto operator|(R&&, view_closure<ViewFn>) = delete;
        
        // support for C | D to produce a new Range Adaptor Closure Object
        // so that (R | C) | D and R | (C | D) can be equivalent
        template <typename ViewFn, derived_from<detail::view_closure_base_> Pipeable>
        friend constexpr auto operator|(view_closure<ViewFn> vw, Pipeable pipe) {
            // produced a new closure, E, such that E(R) == D(C(R))
            return view_closure(compose(std::move(pipe), std::move(vw)));
        }
    };
}
```

And with that, we can implement `join` and `transform` as follows:

::: cmptable
### `join`
```cpp
namespace ranges::views {
  struct join_view_fn {
    template <viewable_range R>
        requires /* ... */
    constexpr auto operator(R&& r)
        -> join_view<all_t<R>>;
  };
  
  // for user consumption
  inline constexpr view_closure<join_view_fn> join{};
}
```

### `transform`
```cpp
namespace ranges::views {
  struct transform_view_fn_base {
    // the overload that has all the information
    template <viewable_range E, typename F>
        requires /* ... */
    constexpr auto operator()(E&& e, F&& f) const
        -> transform_view<all_t<E>, decay_t<F>>;
  };
    
  struct transform_view_fn
    : transform_view_fn_base
  {
    using transform_view_fn_base::operator();
  
    // the partial overload
    template <typename F>
    constexpr auto operator()(F f) const {
      return view_closure(bind_back(
        transform_view_fn_base{}, std::move(f)));
    }
  };
  
  // for user consumption
  inline constexpr transform_view_fn transform{};
}
```
:::

Compared to NanoRange, this looks very similar. We have to manually write both overloads for `transform`, where the partial overload returns some kind of special library closure object (`view_closure` vs `rao_proxy`). The primary difference here is that with NanoRange, `join_view_fn` needed to be defined in the `nano::detail` namespace and then the variable template `is_raco` needed to be specialized to `true`, while in range-v3, `join_view_fn` can actually be in any namespace as long as the `join` object itself has type `view_closure<join_view_fn>`.

## gcc 10

The implementation of pipe support in [@gcc-10] is quite different from either NanoRange or range-v3. There, we had two class templates: `__adaptor::_RangeAdaptorClosure<F>` and `__adaptor::_RangeAdaptor<F>`, which represent range adaptor closure objects and range adaptors, respectively. 

The latter either invokes `F` if possible (to handle the `adaptor(range, args...)` case) or, if not, returns a `_RangeAdaptorClosure` specialization (to handle the `adaptor(args...)` case). The following implementation is reduced a bit, to simply convey how it works (and to use non-uglified names):

```cpp
template <typename Callable>
struct _RangeAdaptor {
    [[no_unique_address]] Callable callable;
    
    template <typename... Args>
        requires (sizeof...(Args) >= 1)
    constexpr auto operator()(Args&&... args) const {
        if constexpr (invocable<Callable, Args...>) {
            // The adaptor(range, args...) case
            return callable(FWD(args)...);
        } else {
            // The adaptor(args...)(range) case
            return _RangeAdaptorClosure(
                [...args=FWD(args), callable]<typename R>(R&& r){
                    return callable(FWD(r), args...);
                });
        }
    }
};
```

The former provides piping support:

```cpp
template <typename Callable>
struct _RangeAdaptorClosure : _RangeAdaptor<Callable>
{
    // support for C(R)
    template <viewable_range R> requires invocable<Callable, R>
    constexpr auto operator()(R&& r) const {
        return callable(FWD(r));
    }
    
    // support for R | C to evaluate as C(R)
    template <viewable_range R> requires invocable<Callable, R>
    friend constexpr auto operator|(R&& r, _RangeAdaptorClosure const& o) {
        return o.callable(FWD(r));
    }
    
    // support for C | D to produce a new Range Adaptor Closure Object
    // so that (R | C) | D and R | (C | D) can be equivalent    
    template <typename T>
    friend constexpr auto operator|(_RangeAdaptorClosure<T> const& lhs, _RangeAdaptorClosure const& rhs) {
        return _RangeAdaptorClosure([lhs, rhs]<typename R>(R&& r){
            return FWD(r) | lhs | rhs;
        });
    }
};
```

And with that, we can implement `join` and `transform` as follows:

::: cmptable
### `join`
```cpp
namespace std::ranges::views {
  // for user consumption
  inline constexpr __adaptor::_RangeAdaptorClosure join
    = []<viewable_range R> requires /* ... */
      (R&& r) {
        return join_view(FWD(r));
      };
}
```

### `transform`
```cpp
namespace std::ranges::views {
  // for user consumption
  inline constexpr __adaptor::_RangeAdaptor transform
    = []<viewable_range R, typename F>
        requires /* ... */
      (R&& r, F&& f){
        return transform_view(FWD(r), FWD(f));
      };
}
```
:::

Compared to either NanoRange or range-v3, this implementation strategy has the significant advantage that we don't have to write both overloads of `transform` manually: we just write a single lambda and use class template argument deduction to wrap its type in the right facility (`_RangeAdaptorClosure` for `join` and `_RangeAdaptor` for `transform`) to provide `|` support. 

This becomes clearer if we look at gcc 10's implementation of `views::transform` vs range-v3's directly:

::: cmptable
### range-v3
```cpp
namespace ranges::views {

}
```

### gcc 10
```cpp
namespace std::ranges::views {
  // for user consumption
  inline constexpr __adaptor::_RangeAdaptor transform
    = []<viewable_range R, typename F>
        requires /* ... */
      (R&& r, F&& f){
        return transform_view(FWD(r), FWD(f));
      };
}
```
:::


## gcc 11

The implementation of pipe support in [@gcc-11] is closer to the range-v3/NanoRange implementations than the gcc 10 one. 

In this implementation, `_RangeAdaptorClosure` is an empty type that is the base class of every range adaptor closure, equivalent to range-v3's `view_closure_base`:

```cpp
struct _RangeAdaptorClosure {
    // support for R | C to evaluate as C(R)
    template <typename Range, typename Self>
        requires derived_from<remove_cvref_t<Self>, _RangeAdaptorClosure>
              && invocable<Self, Range>
    friend constexpr auto operator|(Range&& r, Self&& self) {
        return FWD(self)(FWD(r));
    }
    
    // support for C | D to produce a new Range Adaptor Closure Object
    // so that (R | C) | D and R | (C | D) can be equivalent    
    template <typename Lhs, typename Rhs>
        requires derived_from<Lhs, _RangeAdaptorClosure>
              && derived_from<Rhs, _RangeAdaptorClosure>
    friend constexpr auto operator|(Lhs lhs, Rhs rhs) {
        return _Pipe<Lhs, Rhs>(std::move(lhs), std::move(rhs));
    }
};
```

`_RangeAdaptor` is a CRTP template that is a base class of every range adaptor object (not range adaptor closure):

```cpp
template <typename Derived>
struct _RangeAdaptor {
    // provides the partial overload
    // such that adaptor(args...)(range) is equivalent to adaptor(range, args...)
    // _Partial<Adaptor, Args...> is a _RangeAdaptorClosure
    template <typename... Args>
        requires (Derived::arity > 1)
              && (sizeof...(Args) == Derived::arity - 1)
              && (constructible_from<decay_t<Args>, Args> && ...)
    constexpr auto operator()(Args&&... args) const {
        return _Partial<Derived, decay_t<Args>...>(FWD(args)...);
    }
};
```

The interesting point here is that every adaptor has to specify an `arity`, and the partial call must take all but one of those arguments. As we'll see shortly, `transform` has arity `2` and so this call operator is only viable for a single argument. As such, the library still implements every partial call, but it requires more input from the adaptor declaration itself.

The types `_Pipe<T, U>` and `_Partial<D, Args...>` are both `_RangeAdaptorClosure`s that provide call operators that accept a `viewable_range` and eagerly invoke the appropriate functions (both, in the case of `_Pipe`, and a `bind_back`, in the case of `_Partial`). Both types have appeared in other implementations already. 

And with that, we can implement `join` and `transform` as follows:

::: cmptable
### `join`
```cpp
namespace std::ranges::views {
  struct Join : _RangeAdaptorClosure {
    template <viewable_range R>
        requires /* ... */
    constexpr auto operator()(R&& r) const
        -> join_view<all_t<R>>;
  };
  
  // for user consumption
  inline constexpr Join join;
}
```

### `transform`
```cpp
namespace std::ranges::views {
  struct Transform : _RangeAdaptor<Transform> {
    template <viewable_range R, typeanme F>
      requires /* ... */
    constexpr auto operator()(R&& r, F&& f) const
      -> transform_view<all_t<R>, F>;
      
    using _RangeAdaptor<Transform>::operator();
    static constexpr int arity = 2;
  };
  
  // for user consumption
  inline constexpr Transform transform;
}
```
:::

This is longer than the gcc 10 implementation in that we need both a type and a variable, whereas before we only needed the lambda. But it's still shorter than either the NanoRange or range-v3 implementations in that we do not need to manually implement the partial overload. The library does that for us, we simply have to provide the _using-declaration_ to bring in the partial `operator()` as well as declare our `arity`.

---
references:
    - id: NanoRange
      citation-label: NanoRange
      title: NanoRange
      author:
        - family: Tristan Brindle
      issued:
        - year: 2017
      URL: https://github.com/tcbrindle/nanorange
    - id: range-v3
      citation-label: range-v3
      title: "Range library for C++14/17/20, basis for C++20's std::ranges"
      author:
          - family: Eric Niebler
      issued:
          - year: 2013
      URL: https://github.com/ericniebler/range-v3/
    - id: gcc-10
      citation-label: gcc-10
      title: "`<ranges>` in gcc 10"
      author:
          - family: Jonathan Wakely
      issued:
          - year: 2020
      URL: https://github.com/gcc-mirror/gcc/blob/860c5caf8cbb87055c02b1e77d04f658d2c75880/libstdc%2B%2B-v3/include/std/ranges
    - id: gcc-11
      citation-label: gcc-11
      title: "`<ranges>` in gcc 11"
      author:
          - family: Patrick Palka
      issued:
          - year: 2021
      URL: https://github.com/gcc-mirror/gcc/blob/5e0236d3b0e0d7ad98bcee36128433fa755b5558/libstdc%2B%2B-v3/include/std/ranges
---