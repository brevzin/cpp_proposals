#include <meta>
#include <print>
#include <algorithm>

template <class T>
__macro fwd(T&& t) {
    return ^^{ static_cast<\(type_of(t))&&>(\(t)) };
}

consteval auto is_comparison(std::meta::operators op) -> bool {
    switch (op) {
        using enum std::meta::operators;
        case op_equals_equals:
        case op_exclamation_equals:
        case op_less:
        case op_greater:
        case op_less_equals:
        case op_greater_equals:
            return true;
        default:
            return false;
    }
}

template <class T, class U>
auto check_fail(char const* text, std::source_location sloc, T const& lhs, char const* op, U const& rhs) -> void {
    std::println("{}:{}: FAILED:", sloc.file_name(), sloc.line());
    std::println("  CHECK( {} )", text);
    std::println("with expansion:");
    std::println("  {} {} {}", lhs, op, rhs);
}

template <class T>
__macro check(T&& cond) {
    auto const text = std::meta::str_lit(source_text_of(cond));
    auto const sloc = source_location_of(cond);

    if (is_binary_operation(cond) and is_comparison(operator_of(cond))) {
        auto ops = operands_of(cond);
        char const* op = stringize(^^{ \(operator_of(cond)) });

        return ^^{
            do {
                auto&& lhs = \(ops[0]);
                auto&& rhs = \(ops[1]);
                if (not (fwd!(lhs) \(operator_of(cond)) fwd!(rhs))) {
                    ::check_fail(\(text), \(sloc), lhs, \(op), rhs);
                } 
            }
        };
    }

    return ^^{
        do {
            if (!static_cast<bool>(\(cond))) {
                ::check_fail(\(text), \(sloc));
            }
        }
    };
}

consteval auto placeholder_index(std::string_view id) -> std::optional<int> {
    if (id.size() == 2 and id[0] == '_' and '1' <= id[1] and id[1] <= '9') {
        return id[1] - '0';
    }
    return std::nullopt;
}

__macro λ(std::meta::token_sequence body) {
    auto arity = std::optional<int>();
    for (std::meta::token_sequence tok : body) {
        if (token_kind_of(tok) == std::meta::token_kind::identifier) {
            arity = std::max(arity, placeholder_index(identifier_of(tok)));
        }
    }

    auto params = std::meta::list_builder(^^{ , });
    for (int i = 1; i <= arity.value_or(0); ++i) {
        params += ^^{ auto&& \(std::meta::id("_", i)) };
    }

    return ^^{ [&](\(params)) -> decltype(auto) { return \(body); } };
}

struct DeriveDebug {
    consteval auto on_complete(std::meta::info ty) const -> void {
        auto fmt_body = std::meta::list_builder();
        fmt_body += ^^{
            auto out = std::format_to(ctx.out(), "{}{{", \(display_string_of(ty)));
        };

        auto delim = [first=true, &fmt_body]() mutable {
            if (not first) {
                fmt_body += ^^{
                    *out++ = ',';
                    *out++ = ' ';
                };
            }
            first = false;
        };

        auto unchecked = std::meta::access_context::unchecked();
        for (auto nsdm : nonstatic_data_members_of(ty, unchecked)) {
            delim();
            fmt_body += ^^{
                out = std::format_to(out,
                    \(std::meta::str_lit(".", identifier_of(nsdm), "={}")),
                    object.\(nsdm)
                );
            };
        }

        fmt_body += ^^{
            *out++ = '}';
            return out;
        };

        queue_injection(^^std, ^^{
            template <>
            struct formatter<\(ty)> {
                constexpr auto parse(auto& ctx) { return ctx.begin(); }

                auto format(\(ty) const& object, auto& ctx) const {
                    \(fmt_body);
                }
            };
        });
    }
};

inline constexpr DeriveDebug derive_debug{};

auto f() -> int {
    std::println("Called f");
    return 1;
}

auto g() -> int {
    std::println("Called g");
    return 2;
}

struct [[=derive_debug]] Point {
    int x;
    int y;
    auto operator==(Point const&) const -> bool = default;
};

auto main() -> int {
    check!(f() == g());

    std::vector<Point> points = {{1, 2}, {3, 1}};
    std::ranges::sort(points, λ!(_1.y < _2.y));
    check!(points == std::vector<Point>{{3, 2}, {1, 2}});
}
