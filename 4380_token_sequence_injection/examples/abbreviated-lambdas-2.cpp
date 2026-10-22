#include <vector>
#include <meta>
#include <algorithm>
#include <iostream>
#include <ranges>

struct Fun {
    std::meta::token_sequence args;
    std::meta::token_sequence body;
};

__macro fun(std::meta::token_sequence tokens) {
    auto [args, body] = do -> Fun {
        auto args = std::meta::token_sequence();

        for (size_t i = 0; i < tokens.size(); ++i) {
            auto const tok = tokens[i];
            if (token_kind_of(tok) == std::meta::token_kind::identifier) {
                args += tok;
            } else if (tok == ^^{ = }) {
                if (tokens[i+1] != ^^{ > }) {
                    throw "expected '=>' token";
                }

                do_return {
                    .args = args,
                    .body = tokens.subseq(i + 2),
                };
            }
        }

        throw "failed to parse fun";
    };

    auto args_list = std::meta::list_builder(^^{ , });
    for (std::meta::token arg : args) {
        args_list += ^^{ auto&& \(arg) };
    }

    return ^^{
        [&]($args_list) -> decltype(auto) {
            return $body;
        }
    };
}

struct vec2 {
    int x;
    int y;
};

auto main() -> int {
    std::vector<vec2> points = {{2, 67}, {2, 42}, {3, 666}};

    std::vector<int> ys = points
        | std::views::filter(fun!(p => p.x % 2))
        | std::views::transform(fun!(p => p.y))
        | std::ranges::to<std::vector>();

    std::ranges::sort(points, fun!((l, r) => l.y < r.y));
    std::cout << points[0].y << "\n";

    std::ranges::sort(points, std::less{}, fun!(p => p.x));
    std::cout << points[0].y << "\n";
}