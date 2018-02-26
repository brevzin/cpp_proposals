template<typename _Tp>
class optional;

template<typename _Tp, typename _Up>
using __converts_from_optional =
    __or_<is_constructible<_Tp, const optional<_Up>&>,
    is_constructible<_Tp, optional<_Up>&>,
    is_constructible<_Tp, const optional<_Up>&&>,
    is_constructible<_Tp, optional<_Up>&&>,
    is_convertible<const optional<_Up>&, _Tp>,
    is_convertible<optional<_Up>&, _Tp>,
    is_convertible<const optional<_Up>&&, _Tp>,
    is_convertible<optional<_Up>&&, _Tp>>;

template<typename _Tp, typename _Up>
using __assigns_from_optional =
    __or_<is_assignable<_Tp&, const optional<_Up>&>,
    is_assignable<_Tp&, optional<_Up>&>,
    is_assignable<_Tp&, const optional<_Up>&&>,
    is_assignable<_Tp&, optional<_Up>&&>>;

/**
  * @brief Class template for optional values.
  */
template<typename _Tp>
class optional
{
    static_assert(!is_same_v<remove_cv_t<_Tp>, nullopt_t>);
    static_assert(!is_same_v<remove_cv_t<_Tp>, in_place_t>);
    static_assert(!is_reference_v<_Tp>);

private:
    using _Stored_type = remove_const_t<_Tp>;
    struct _Empty_byte { };
    union {
        _Empty_byte _M_empty;
        _Stored_type _M_payload;
    };
    bool _M_engaged;

    template<typename... _Args>
    void _M_construct(_Args&&... __args)
    noexcept(is_nothrow_constructible<_Stored_type, _Args...>())
    {
        ::new ((void *) std::__addressof(this->_M_payload))
        _Stored_type(std::forward<_Args>(__args)...);
        this->_M_engaged = true;
    }

public:
    using value_type = _Tp;

    constexpr optional()
        : _M_empty(), _M_engaged(false) {}

    constexpr optional(nullopt_t) noexcept
        : _M_empty(), _M_engaged(false) {}

    constexpr optional(optional const&)
    requires is_trivially_copy_constructible_v<_Tp> &&
    is_copy_constructible_v<_Tp> = default;
    constexpr optional(optional const& rhs)
    requires is_copy_constructible_v<_Tp> {
        if (rhs._M_engaged)
        {
            _M_construct(rhs._M_payload);
        }
    }

    constexpr optional(optional&&)
    requires is_trivially_move_constructible_v<_Tp> &&
    is_move_constructible_v<_Tp> = default;
    constexpr optional(optional&& rhs)
    noexcept(is_nothrow_move_constructible<_Tp>)
    requires is_move_constructible_v<_Tp> {
        if (rhs._M_engaged)
        {
            _M_construct(std::move(rhs._M_payload));
        }
    }

    optional& operator=(optional const&)
    requires is_copy_constructible_v<_Tp> && is_copy_assignable_v<_Tp>
    && is_trivially_copy_assignable<_Tp> = default;
    optional& operator=(optional const& rhs)
    requires is_copy_constructible_v<_Tp> && is_copy_assignable_v<_Tp> {
        if (_M_engaged && rhs._M_engaged)
        {
            _M_payload = rhs._M_payload;
        } else if (rhs._M_engaged)
        {
            _M_construct(rhs._M_payload);
        } else
        {
            reset();
        }
        return *this;
    }

    optional& operator=(optional&&)
    requires is_move_constructible_v<_Tp> && is_move_assignable_v<_Tp>
    && is_trivially_move_assignable = default;
    optional& operator=(optional&& rhs)
    requires is_move_constructible_v<_Tp> && is_move_assignable_v<_Tp>
    noexcept(__and_<is_nothrow_move_constructible<_Tp>,
             is_nothrow_move_assignable<_Tp>>())
    {
        if (_M_engaged && rhs._M_engaged) {
            _M_payload = std::move(rhs._M_payload);
        } else if (rhs._M_engaged) {
            _M_construct(std::move(rhs._M_payload));
        } else {
            reset();
        }
        return *this;
    }

    // Destructor
    ~optional() requires is_trivially_destructible_v<_Tp> = default;
    ~optional()
    {
        if (_M_engaged) {
            _M_payload.~_Stored_type();
        }
    }

    // Converting constructors for engaged optionals.
    template <typename _Up = _Tp,
              enable_if_t<__and_<
                              __not_<is_same<optional<_Tp>, decay_t<_Up>>>,
                                      __not_<is_same<in_place_t, decay_t<_Up>>>,
                                              is_constructible<_Tp, _Up&&>
                                              >::value, bool> = false>
                                     explicit(!is_convertible_v<_Up&&, _Tp>)
                                     constexpr optional(_Up&& __t)
                                         : optional(std::in_place, std::forward<_Up>(__t)) { }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, const _Up&>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = false>
    explicit(!is_convertible_v<_Up const&, _Tp>)
    constexpr optional(const optional<_Up>& __t)
    {
        if (__t) {
            emplace(*__t);
        }
    }

    template <typename _Up,
              enable_if_t<__and_<
                              __not_<is_same<_Tp, _Up>>,
                              is_constructible<_Tp, _Up&&>,
                              __not_<__converts_from_optional<_Tp, _Up>>
                              >::value, bool> = false>
    explicit(!is_convertible_v<_Up&&, _Tp>)
    constexpr optional(optional<_Up>&& __t)
    {
        if (__t) {
            emplace(std::move(*__t));
        }
    }

    template<typename... _Args,
             enable_if_t<is_constructible_v<_Tp, _Args&&...>, bool> = false>
    explicit constexpr optional(in_place_t, _Args&&... __args)
        : _M_payload(std::forward<_Args>(__args)...), _M_engaged(true) { }

    template<typename _Up, typename... _Args,
             enable_if_t<is_constructible_v<_Tp,
                                            initializer_list<_Up>&,
                                            _Args&&...>, bool> = false>
    explicit constexpr optional(in_place_t,
                                initializer_list<_Up> __il,
                                _Args&&... __args)
        : _M_payload(__il, std::forward<_Args>(__args)...), _M_engaged(true) { }

    // Assignment operators.
    optional&
    operator=(nullopt_t) noexcept
    {
        reset();
        return *this;
    }

    template<typename _Up = _Tp>
    enable_if_t<__and_<
                    __not_<is_same<optional<_Tp>, decay_t<_Up>>>,
                                   is_constructible<_Tp, _Up>,
                                   __not_<__and_<is_scalar<_Tp>,
                                           is_same<_Tp, decay_t<_Up>>>>,
                                           is_assignable<_Tp&, _Up>>::value,
                                          optional&>
                                   operator=(_Up&& __u)
    {
        if (_M_engaged) {
            _M_payload = std::forward<_Up>(__u);
        } else {
            _M_construct(std::forward<_Up>(__u));
        }

        return *this;
    }

    template<typename _Up>
    enable_if_t<__and_<
                    __not_<is_same<_Tp, _Up>>,
                    is_constructible<_Tp, const _Up&>,
                    is_assignable<_Tp&, _Up>,
                    __not_<__converts_from_optional<_Tp, _Up>>,
                    __not_<__assigns_from_optional<_Tp, _Up>>
                    >::value,
                optional&>
    operator=(const optional<_Up>& __u)
    {
        if (__u) {
            if (_M_engaged) {
                _M_payload = *__u;
            } else {
                _M_construct(*__u);
            }
        } else {
            reset();
        }
        return *this;
    }

    template<typename _Up>
    enable_if_t<__and_<
                    __not_<is_same<_Tp, _Up>>,
                    is_constructible<_Tp, _Up>,
                    is_assignable<_Tp&, _Up>,
                    __not_<__converts_from_optional<_Tp, _Up>>,
                    __not_<__assigns_from_optional<_Tp, _Up>>
                    >::value,
                optional&>
    operator=(optional<_Up>&& __u)
    {
        if (__u) {
            if (_M_engaged) {
                _M_payload = std::move(*__u);
            } else {
                _M_construct(std::move(*__u));
            }
        } else {
            reset();
        }

        return *this;
    }

    template<typename... _Args>
    enable_if_t<is_constructible<_Tp, _Args&&...>::value, _Tp&>
    emplace(_Args&&... __args)
    {
        reset();
        _M_construct(std::forward<_Args>(__args)...);
        return _M_payload;
    }

    template<typename _Up, typename... _Args>
    enable_if_t<is_constructible<_Tp, initializer_list<_Up>&,
                                 _Args&&...>::value, _Tp&>
    emplace(initializer_list<_Up> __il, _Args&&... __args)
    {
        reset();
        _M_construct(__il, std::forward<_Args>(__args)...);
        return _M_payload;
    }

    // Destructor is implicit, implemented in _Optional_base.

    // Swap.
    void
    swap(optional& __other)
    noexcept(is_nothrow_move_constructible<_Tp>()
             && is_nothrow_swappable_v<_Tp>)
    {
        using std::swap;

        if (_M_engaged && __other._M_is_engaged()) {
            swap(_M_payload, __other._M_get());
        } else if (_M_engaged) {
            __other._M_construct(std::move(_M_payload));
            this->_M_destruct();
        } else if (__other._M_is_engaged()) {
            this->_M_construct(std::move(__other._M_get()));
            __other._M_destruct();
        }
    }

    // Observers.
    template <typename _Self>
    constexpr auto operator->(_Self&& this)
    {
        return std::__addressof(std::forward_like<_Self>(_M_payload);
    }

    template <typename _Self>
    constexpr auto&& operator*(_Self&& this)
    {
        __glibcxx_assert(_M_engaged);
        return std::forward_like<_Self>(_M_payload);
    }

    constexpr explicit operator bool() const noexcept
    {
        return _M_engaged;
    }

    constexpr bool has_value() const noexcept
    {
        return _M_engaged;
    }

    template <typename _Self>
    constexpr auto&& value(_Self&& this)
    {
        return _M_engaged
        ? std::forward_like<_Self>(_M_payload)
        : (__throw_bad_optional_access(),
           std::forward_like<_Self>(_M_payload));
    }

    template <typename _Self, typename _Up>
    constexpr _Tp
    value_or(_Self&& this, _Up&& __u) const&
    {
        static_assert(is_constructible_v<_Tp, std::like_t<Self, _Tp>>);
        static_assert(is_convertible_v<_Up&&, _Tp>);

        return _M_engaged
               ? std::forward_like<_Self>(_M_payload)
               : static_cast<_Tp>(std::forward<_Up>(__u));
    }

    void reset() noexcept
    {
        if (_M_engaged) {
            _M_payload.~_Stored_type();
            _M_engaged = false;
        }
    }

    template <typename U>
    constexpr auto operator<=>(optional<U> const& rhs) const
    -> decltype(compare_3way(**this, *rhs))
    {
        return has_value() && rhs.has_value()
        ? compare_3way(**this, *rhs)
        : has_value() <=> rhs.has_value();
    }

    template <typename U>
    constexpr auto operator<=>(U const& rhs) const
    -> decltype(compare_3way(**this, rhs))
    {
        return has_value()
        ? compare_3way(**this, rhs)
        : strong_ordering::less;
    }

    constexpr strong_ordering operator<=>(nullopt_t ) const
    {
        return has_value() ? strong_ordering::greater : strong_ordering::equal;
    }
};

// Swap and creation functions.

// _GLIBCXX_RESOLVE_LIB_DEFECTS
// 2748. swappable traits for optionals
template<typename _Tp>
inline enable_if_t<is_move_constructible_v<_Tp> && is_swappable_v<_Tp>>
swap(optional<_Tp>& __lhs, optional<_Tp>& __rhs)
noexcept(noexcept(__lhs.swap(__rhs)))
{
    __lhs.swap(__rhs);
}

template<typename _Tp>
enable_if_t<!(is_move_constructible_v<_Tp> && is_swappable_v<_Tp>)>
swap(optional<_Tp>&, optional<_Tp>&) = delete;

template<typename _Tp>
constexpr optional<decay_t<_Tp>>
make_optional(_Tp&& __t)
{
    return optional<decay_t<_Tp>> { std::forward<_Tp>(__t) };
}

template<typename _Tp, typename ..._Args>
constexpr optional<_Tp>
make_optional(_Args&&... __args)
{
    return optional<_Tp> { in_place, std::forward<_Args>(__args)... };
}

template<typename _Tp, typename _Up, typename ..._Args>
constexpr optional<_Tp>
make_optional(initializer_list<_Up> __il, _Args&&... __args)
{
    return optional<_Tp> { in_place, __il, std::forward<_Args>(__args)... };
}

// Hash.

template<typename _Tp, typename _Up = remove_const_t<_Tp>,
         bool = __poison_hash<_Up>::__enable_hash_call>
struct __optional_hash_call_base {
    size_t
    operator()(const optional<_Tp>& __t) const
    noexcept(noexcept(hash<_Up> {}(*__t)))
    {
        // We pick an arbitrary hash for disengaged optionals which hopefully
        // usual values of _Tp won't typically hash to.
        constexpr size_t __magic_disengaged_hash = static_cast<size_t>(-3333);
        return __t ? hash<_Up> {}(*__t) : __magic_disengaged_hash;
    }
};

template<typename _Tp, typename _Up>
struct __optional_hash_call_base<_Tp, _Up, false> {};

template<typename _Tp>
struct hash<optional<_Tp>>
: private __poison_hash<remove_const_t<_Tp>>,
public __optional_hash_call_base<_Tp> {
    using result_type [[__deprecated__]] = size_t;
    using argument_type [[__deprecated__]] = optional<_Tp>;
};

template<typename _Tp>
struct __is_fast_hash<hash<optional<_Tp>>> : __is_fast_hash<hash<_Tp>> {
};

/// @}

#if __cpp_deduction_guides >= 201606
template <typename _Tp> optional(_Tp) -> optional<_Tp>;
#endif