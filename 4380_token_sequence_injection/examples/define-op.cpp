#include <meta>

template <class T>
__macro fwd(T&& t) {
    return ^^{ static_cast<\(type_of(t))&&>(\(t)) };
}

__macro define_op(std::meta::token_sequence name,
                  std::meta::token_sequence pattern) {
    // id op id
    if (pattern.size() == 3) {
        return ^^{
            struct \(name) {
                template <class L, class R>
                constexpr decltype(auto) operator()(L&& l, R&& r) const {
                    return fwd!(l) \(pattern[1]) fwd!(r);
                }
            };
        };
    }

    // id op or op id
    auto body = token_kind_of(pattern[0]) == std::meta::token_kind::identifier
        ? ^^{ fwd!(x) \(pattern[1]) }
        : ^^{ \(pattern[0]) fwd!(x) };

    return ^^{
        struct \(name) {
            template <class T>
            constexpr decltype(auto) operator()(T&& x) const {
                return \(body);
            }
        };
    };
}

define_op!(left_shift, x << y);
define_op!(negate, -x);
define_op!(post_inc, x++);

static_assert(left_shift{}(1, 4) == 16);
static_assert(negate{}(1) == -1);
