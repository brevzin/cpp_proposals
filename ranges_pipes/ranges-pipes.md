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
        return std::forward<C>(rhs)(std::forward<R>(lhs));
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
            return rhs(lhs(std::forward<R>(r)));
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
        return raco_pipe<decay_t<LHS>, decay_t<RHS>>(std::forward<LHS>(lhs), std::forward<RHS>(rhs));
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
  struct transform_view_fn {
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
        -> decltype(join_view{std::forward<E>(e)})
    {
        return join_view{std::forward<E>(e)};
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
            return std::move(vw)(std::forward<R>(rng));
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
  struct transform_view_fn {
    // the overload that has all the information
    template <viewable_range E, typename F>
        requires /* ... */
    constexpr auto operator()(E&& e, F&& f) const
        -> transform_view<all_t<E>, decay_t<F>>;
    
    // the partial overload
    template <typename F>
    constexpr auto operator()(F f) const {
      return view_closure(
        bind_back(transform_view_fn{}, std::move(f)));
    }
  };
  
  // for user consumption
  inline constexpr transform_view_fn transform{};
}
```
:::

Compared to NanoRange, this looks very similar. We have to manually write both overloads for `transform`, where the partial overload returns some kind of special library closure object (`view_closure` vs `rao_proxy`). The primary difference here is that with NanoRange, `join_view_fn` needed to be defined in the `nano::detail` namespace and then the variable template `is_raco` needed to be specialized to `true`, while in range-v3, `join_view_fn` can actually be in any namespace as long as the `join` object itself has type `view_closure<join_view_fn>`.

## gcc 10

TODO

## gcc 11

TODO

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
---