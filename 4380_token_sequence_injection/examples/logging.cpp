#include <meta>
#include <cassert>
#include <print>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

enum class LogLevel { debug, info, error };

template <class E> requires std::is_enum_v<E>
struct std::formatter<E> {
    constexpr auto parse(auto& ctx) { return ctx.begin(); }

    auto format(E e, auto& ctx) const {
        template for (constexpr std::meta::info r : enumerators_of(^^E)) {
            if (e == [:r:]) {
                return std::format_to(ctx.out(), "{}", identifier_of(r));
            }
        }

        return std::format_to(ctx.out(), "<unknown>");
    }
};

struct Logger {
    LogLevel min_level;

    explicit Logger(LogLevel min_level) : min_level(min_level) { }

    auto do_log(LogLevel level, std::string_view fmt, auto&&... args) -> void {
        std::println("[{}] {}", level, std::vformat(fmt, std::make_format_args(args...)));
    }

    consteval {
        for (std::meta::info e : enumerators_of(^^LogLevel)) {
            auto name = std::meta::id(identifier_of(e));
            queue_injection(^^{
                template <class... Args>
                __macro \(name)(this Logger& self, std::string_view raw, Args&&... args) {
                    auto fs = substitute(^^::std::basic_format_string, {^^char, ^^Args...});
                    constexpr auto level = ::LogLevel::\(name);

                    auto call_args = std::meta::list_builder(^^{ , });
                    call_args += ^^{ fmt };
                    ((call_args += ^^{ \(args) }), ...);
                    return ^^{
                        do {
                            constexpr std::string_view fmt = \(fs)(\(raw)).get();
                            auto& self = \(self);
                            if (\(level) >= self.min_level) {
                                self.do_log(\(level), \(call_args));
                            }
                        }
                    };
                }
            });
        }
    }
};

auto get() -> int {
    std::println("** called get **");
    return 6;
}

int main(int, char**) {
    auto log = Logger(LogLevel::info);
    int x = 5;
    log.debug!("x={} y={}", x, get());
    log.info!("{:>3}|{}", "ab", 2.5);
    log.error!("plain");
}
