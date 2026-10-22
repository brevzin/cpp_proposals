#include <meta>

namespace std {
    struct BitmaskType {
        static consteval auto on_complete(std::meta::info ty) -> void {
            queue_injection(^^{
                constexpr auto operator+($ty e) {
                    return std::to_underlying(e);
                }

                constexpr auto operator~($ty e) -> $ty {
                    return $ty(~+e);
                }
            });

            auto binary = [=](std::meta::operators op, std::meta::operators op_eq){
                queue_injection(^^{
                    constexpr auto operator $op($ty lhs, $ty rhs) -> $ty {
                        return $ty(+lhs $op +rhs);
                    }

                    constexpr auto operator $op_eq(${ty}& lhs, $ty rhs) -> ${ty}& {
                        lhs = lhs $op rhs;
                        return lhs;
                    }
                });
            };

            using enum std::meta::operators;
            binary(op_pipe,      op_pipe_equals);
            binary(op_ampersand, op_ampersand_equals);
            binary(op_caret,     op_caret_equals);
        }
    };

    inline constexpr auto bitmask_type = BitmaskType();
}

// main.cpp
#include <cassert>

namespace N {
    enum class [[=std::bitmask_type]] Permission {
        None    = 0,
        Read    = 1 << 0,
        Write   = 1 << 1,
        Execute = 1 << 2,
    };

}

namespace M {
    // some unrelated type
    struct Mask { };
    auto operator|(Mask, Mask) -> Mask;

    auto something(N::Permission permissions) -> void {
        using enum N::Permission;
        if ((permissions | Write) == Write) {
            // do writing
        }
    }
}

auto main() -> int {
    using enum N::Permission;
    auto x = Read | Write;
    auto y = std::bit_or{}(Read, Write);
    assert(x == N::Permission(3));
    assert(y == N::Permission(3));
}