#include <meta>
#include <algorithm>
#include <debugging>

namespace using_detail {

using namespace std::meta;

consteval auto is(token t, punctuator p) -> bool {
    return token_kind_of(t) == token_kind::punctuator
        and punctuator_of(t) == p;
}

// Split at top-level commas.
consteval auto split_commas(auto&& r) -> std::vector<token_sequence> {
    auto out = std::vector<token_sequence>(1);
    int depth = 0;
    for (token t : r) {
        if (token_kind_of(t) == token_kind::punctuator) {
            switch (punctuator_of(t)) {
            case punctuator::left_paren:
            case punctuator::left_square:
            case punctuator::left_brace:
                ++depth;
                break;
            case punctuator::right_paren:
            case punctuator::right_square:
            case punctuator::right_brace:
                --depth;
                break;
            case punctuator::comma:
                if (depth == 0) {
                    out.emplace_back();
                    continue;
                }
                break;
            default:
                break;
            }
        }

        out.back() += t;
    }
    return out;
}

consteval auto expand(token_sequence item,
                      token_sequence prefix,
                      token_sequence& out) -> void {
    auto lb = std::ranges::find_if(item, [](token t) {
        return is(t, punctuator::left_brace);
    });
    if (lb == item.end()) {
        out += ^^{ using \(prefix) \(item); };
        return;
    }

    if (lb == item.begin() or not is(lb[-1], punctuator::colon_colon)) {
        std::constexpr_error_str("using_", "a group must follow '::'");
    }
    if (not is(item.back(), punctuator::right_brace)) {
        std::constexpr_error_str("using_", "nothing may follow a group");
    }

    token_sequence head = prefix + token_sequence(std::ranges::subrange(item.begin(), lb));
    for (token_sequence inner : split_commas(std::ranges::subrange(lb + 1, item.end() - 1))) {
        expand(inner, head, out);
    }
}

}  // namespace using_detail

__macro using_(std::meta::token_sequence names) {
    auto out = ^^{ };
    for (auto item : using_detail::split_commas(names)) {
        using_detail::expand(item, ^^{}, out);
    }
    return out;
}

#include <chrono>
using_!(std::chrono::{nanoseconds, time_point, system_clock});
static_assert(time_point<system_clock, nanoseconds>(nanoseconds(100)).time_since_epoch().count() == 100);

namespace lib {
    constexpr int a = 1, b = 2;
    namespace inner {
        constexpr int c = 3, d = 4;
    }
    constexpr int e = 5;
} // namespace lib

using_!(lib::{a, b});
using_!(::lib::{inner::{c, d}, e});
static_assert(a + b + c + d + e == 15);

// ce: clang_barry -std=c++29 -freflection -Xclang -ast-print