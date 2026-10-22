#include <meta>
#include <vector>
#include <memory>
#include <print>

template <class... Args>
__macro vec(Args&&... args) {
    using T = std::remove_cvref_t<Args...[0]>;

#ifdef SIMPLE_LOOP
    auto emplace_back_loop = std::meta::token_sequence();
    for (std::meta::info expr : {args...}) {
        emplace_back_loop += ^^{
            res.__emplace_back_assume_capacity(\(expr));
        };
    }
#elifdef PACK_EXPAND
    auto emplace_back_loop = ^^{
        \(
            ^^{
                res.__emplace_back_assume_capacity(\(args));
            }...;
            ^^{ }
        )
    };
#endif

    return ^^{
        do {
            auto res = ::std::vector<\(^^T)>();
            res.reserve(\(sizeof...(Args)));
            \(emplace_back_loop);
            do_return res;
        }
    };
}

auto main() -> int {
    auto v = vec!{
        std::make_unique<int>(1),
        std::make_unique<int>(2),
    };
    std::println("v.size() = {}", v.size());
    std::println("v.capacity() = {}", v.capacity());
    std::println("*v[0] = {}", *v[0]);
    std::println("*v[1] = {}", *v[1]);
}

// ce-run: clang_barry -std=c++29 -freflection -DSIMPLE_LOOP
// ce-run: clang_barry -std=c++29 -freflection -DPACK_EXPAND