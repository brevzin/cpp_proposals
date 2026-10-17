#include <meta>
#include <print>

namespace N {
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
}

struct [[=N::derive_debug]] Config {
    std::string name;
    int amount;
};

template <class T>
struct [[=N::derive_debug]] Widget {
    T thing;
};

int main() {
    std::println("Config: {}", Config{.name="abc", .amount=10});
    std::println("Widget<int>: {}", Widget<int>{.thing=5});
}
