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
auto check_fail(char const* text, std::source_location sloc, T const& expr) -> void {
    std::println("{}:{}: FAILED:", sloc.file_name(), sloc.line());
    std::println("  CHECK( {} )", text);
    std::println("with expansion:");
    std::println("  {}", expr);
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
                if (not (\(ops[0]) \(operator_of(cond)) \(ops[1]))) {
                    ::check_fail(\(text), \(sloc), \(ops[0]), \(op), \(ops[1]));
                } 
            }
        };
    }

    return ^^{
        do {
            auto&& expr = \(cond);
            if (!static_cast<bool>(expr)) {
                ::check_fail(\(text), \(sloc), expr);
            }
        }
    };
}

auto f() -> int {
    std::println("Called f");
    return 1;
}

auto g() -> int {
    std::println("Called g");
    return 2;
}

auto main() -> int {
    check!(f() == g());
}
