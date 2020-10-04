---
title: "Missing `constexpr` in `std::optional` and `std::variant`"
document: P2231R0
date: today
audience: Library Evolution
author:
    - name: Barry Revzin
      email: <barry.revzin@gmail.com>
toc: false
---

# Introduction

Each new language standard has increased the kinds of operations that we can do during constant evaluation time. C++20 was no different. With the adoption of [@P1330R0], C++20 added the ability to change the active member of a union inside constexpr (the paper specifically mentions `std::optional`). And with the adoption of [@P0784R7], C++20 added the ability to do placement new inside constexpr (by way of `std::construct_at`).

But even though the language provided the tools to make `std::optional` and `std::variant` completely constexpr-able, there was no such update to the library. This paper seeks to remedy that omission.

But the library was not updated in response to the new addition. This paper fixes that omission by simply adding `constexpr` to all the relevant places. I updated libstdc++'s implementation of `std::optional` to add these `constexpr`s (and replace the placement `new` calls with calls to `std::construct_at`) and it [compiles](https://godbolt.org/z/nbhqer) with both gcc and clang.

# Wording

[The wording here just shows the added `constexpr`s in the synopses. They all need to be repeated in the specific wording for each function.]{.ednote}

Add `constexpr` to the `swap` in [optional.syn]{.sref} (and in [optional.specalg]{.sref}):

```diff
namespace std {
  // [optional.optional], class template optional
  template<class T>
    class optional;
    
  [...]
  
  // [optional.specalg], specialized algorithms
  template<class T>
-   void swap(optional<T>&, optional<T>&) noexcept(@_see below_@);  
+   @[constexpr]{.diffins}@ void swap(optional<T>&, optional<T>&) noexcept(@_see below_@);  

  [...]

}
```

Add `constexpr` to the rest of the functions in [optional.optional.general]{.sref} (and likewise in [optional.ctor]{.sref}, [optional.assign]{.sref}, [optional.swap]{.sref}, and [optional.mod]{.sref}):

```diff
namespace std {
  template<class T>
  class optional {
  public:
    using value_type = T;

    // [optional.ctor], constructors
    constexpr optional() noexcept;
    constexpr optional(nullopt_t) noexcept;
    constexpr optional(const optional&);
    constexpr optional(optional&&) noexcept(@_see below_@);
    template<class... Args>
      constexpr explicit optional(in_place_t, Args&&...);
    template<class U, class... Args>
      constexpr explicit optional(in_place_t, initializer_list<U>, Args&&...);
    template<class U = T>
      constexpr explicit(@_see below_@) optional(U&&);
    template<class U>
-     explicit(@_see below_@) optional(const optional<U>&);
+     @[constexpr]{.diffins}@ explicit(@_see below_@) optional(const optional<U>&);
    template<class U>
-     explicit(@_see below_@) optional(optional<U>&&);
+     @[constexpr]{.diffins}@ explicit(@_see below_@) optional(optional<U>&&);

    // [optional.dtor], destructor
    ~optional();

    // [optional.assign], assignment
-   optional& operator=(nullopt_t) noexcept;
+   @[constexpr]{.diffins}@ optional& operator=(nullopt_t) noexcept;
    constexpr optional& operator=(const optional&);
    constexpr optional& operator=(optional&&) noexcept(@_see below_@);
-   template<class U = T> optional& operator=(U&&);
-   template<class U> optional& operator=(const optional<U>&);
-   template<class U> optional& operator=(optional<U>&&);
-   template<class... Args> T& emplace(Args&&...);
-   template<class U, class... Args> T& emplace(initializer_list<U>, Args&&...);
+   template<class U = T> @[constexpr]{.diffins}@ optional& operator=(U&&);
+   template<class U> @[constexpr]{.diffins}@ optional& operator=(const optional<U>&);
+   template<class U> @[constexpr]{.diffins}@ optional& operator=(optional<U>&&);
+   template<class... Args> @[constexpr]{.diffins}@ T& emplace(Args&&...);
+   template<class U, class... Args> @[constexpr]{.diffins}@ T& emplace(initializer_list<U>, Args&&...);

    // [optional.swap], swap
-   void swap(optional&) noexcept(@_see below_@);
+   @[constexpr]{.diffins}@ void swap(optional&) noexcept(@_see below_@);

    // [optional.observe], observers
    constexpr const T* operator->() const;
    constexpr T* operator->();
    constexpr const T& operator*() const&;
    constexpr T& operator*() &;
    constexpr T&& operator*() &&;
    constexpr const T&& operator*() const&&;
    constexpr explicit operator bool() const noexcept;
    constexpr bool has_value() const noexcept;
    constexpr const T& value() const&;
    constexpr T& value() &;
    constexpr T&& value() &&;
    constexpr const T&& value() const&&;
    template<class U> constexpr T value_or(U&&) const&;
    template<class U> constexpr T value_or(U&&) &&;

    // [optional.mod], modifiers
-   void reset() noexcept;
+   @[constexpr]{.diffins}@ void reset() noexcept;

  private:
    T *val;         // exposition only
  };

  template<class T>
    optional(T) -> optional<T>;
}
```

Add `constexpr` to `swap` in [variant.syn]{.sref} (and likewise in [variant.specalg]{.sref}):

```diff
namespace std {
  // [variant.variant], class template variant
  template<class... Types>
    class variant;

  [...]
  
  // [variant.specalg], specialized algorithms
  template<class... Types>
-   void swap(variant<Types...>&, variant<Types...>&) noexcept(@_see below_@);
+   @[constexpr]{.diffins}@ void swap(variant<Types...>&, variant<Types...>&) noexcept(@_see below_@);

  [...]
}    
```

Add `constexpr` to the rest of the functions in [variant.variant.general]{.sref} (and likewise in [variant.assign]{.sref}, [variant.mod]{.sref}, and [variant.swap]{.sref}):

```diff
namespace std {
  template<class... Types>
  class variant {
  public:
    // [variant.ctor], constructors
    constexpr variant() noexcept(@_see below_@);
    constexpr variant(const variant&);
    constexpr variant(variant&&) noexcept(@_see below_@);

    template<class T>
      constexpr variant(T&&) noexcept(@_see below_@);

    template<class T, class... Args>
      constexpr explicit variant(in_place_type_t<T>, Args&&...);
    template<class T, class U, class... Args>
      constexpr explicit variant(in_place_type_t<T>, initializer_list<U>, Args&&...);

    template<size_t I, class... Args>
      constexpr explicit variant(in_place_index_t<I>, Args&&...);
    template<size_t I, class U, class... Args>
      constexpr explicit variant(in_place_index_t<I>, initializer_list<U>, Args&&...);

    // [variant.dtor], destructor
    ~variant();

    // [variant.assign], assignment
    constexpr variant& operator=(const variant&);
    constexpr variant& operator=(variant&&) noexcept(@_see below_@);

-   template<class T> variant& operator=(T&&) noexcept(@_see below_@);
+   template<class T> @[constexpr]{.diffins}@ variant& operator=(T&&) noexcept(@_see below_@);

    // [variant.mod], modifiers
-   template<class T, class... Args>
-     T& emplace(Args&&...);
-   template<class T, class U, class... Args>
-     T& emplace(initializer_list<U>, Args&&...);
-   template<size_t I, class... Args>
-     variant_alternative_t<I, variant<Types...>>& emplace(Args&&...);
-   template<size_t I, class U, class... Args>
-     variant_alternative_t<I, variant<Types...>>& emplace(initializer_list<U>, Args&&...);
+   template<class T, class... Args>
+     @[constexpr]{.diffins}@ T& emplace(Args&&...);
+   template<class T, class U, class... Args>
+     @[constexpr]{.diffins}@ T& emplace(initializer_list<U>, Args&&...);
+   template<size_t I, class... Args>
+     @[constexpr]{.diffins}@ variant_alternative_t<I, variant<Types...>>& emplace(Args&&...);
+   template<size_t I, class U, class... Args>
+     @[constexpr]{.diffins}@ variant_alternative_t<I, variant<Types...>>& emplace(initializer_list<U>, Args&&...);

    // [variant.status], value status
    constexpr bool valueless_by_exception() const noexcept;
    constexpr size_t index() const noexcept;

    // [variant.swap], swap
-   void swap(variant&) noexcept(@_see below_@);
+   @[constexpr]{.diffins}@ void swap(variant&) noexcept(@_see below_@);
  };
}
```