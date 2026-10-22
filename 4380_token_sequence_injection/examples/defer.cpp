#include <meta>
#include <print>

template <class F>
struct scope_guard {
    F f;
    ~scope_guard() { f(); }
};

__macro defer(std::meta::token_sequence body) {
    return ^^{
        auto _ = scope_guard([&]{ $body; });
    };
}

auto main() -> int {
    std::println("A");
    defer!{
        std::println("deferred");
    };
    std::println("B");
}

// ce-run: clang_barry -std=c++29 -freflection
