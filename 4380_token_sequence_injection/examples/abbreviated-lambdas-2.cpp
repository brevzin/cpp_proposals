#include <vector>
#include <meta>
#include <algorithm>
#include <ranges>
#include <iostream>

struct Fun {
    std::meta::token_sequence args;
    std::meta::token_sequence body;
};

__macro fun(std::meta::token_sequence tokens) {
    auto [args, body] = do -> Fun {
        auto args = std::meta::list_builder(^^{ , });

        for (size_t i = 0; i < tokens.size(); ++i) {
            auto const tok = tokens[i];
            if (token_kind_of(tok) == std::meta::token_kind::identifier) {
                args += ^^{ auto&& $tok };
            } else if (tok == ^^{ = }) {
                if (tokens[i + 1] != ^^{ > }) {
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

    return ^^{
        [&]($args) -> decltype(auto) {
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