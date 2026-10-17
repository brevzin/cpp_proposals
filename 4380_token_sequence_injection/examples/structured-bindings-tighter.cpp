#include <meta>
#include <print>

// ------------------------- the library -------------------------
namespace lib {
    template <class T, template <class...> class Z>
    concept specializes = has_template_arguments(remove_cvref(^^T))
                      and template_of(remove_cvref(^^T)) == ^^Z;

    struct inject_bindings_t {
        consteval auto on_template_defined(std::meta::info tmpl) const -> void {
            queue_injection(^^std, ^^{
                template <class... Ts>
                struct tuple_size<\(tmpl)<Ts...>>
                    : integral_constant<size_t, size(\(tmpl)<Ts...>::tuple_elements)>
                { };

                template <size_t I, class... Ts>
                struct tuple_element<I, \(tmpl)<Ts...>> {
                    using type = [: type_of(\(tmpl)<Ts...>::tuple_elements[I]) :];
                };
            });
        }

        consteval auto inject_members(std::meta::info) const -> std::meta::token_sequence {
            return ^^{
                public:
                template <size_t I, class Self>
                constexpr auto get(this Self&& self) -> decltype(auto) {
                    return (((Self&&)self).[: tuple_elements[I] :]);
                }
            };
        }
    };
    inline constexpr inject_bindings_t inject_bindings{};
}

// ------------------------- the user code -------------------------
template <class T>
class [[=lib::inject_bindings]] wide_result {
    T hi;
    T lo;

public:
    constexpr wide_result(T hi, T lo) : hi(hi), lo(lo) { }

    static constexpr std::meta::info tuple_elements[] = {^^hi, ^^lo};
};

static_assert(std::tuple_size_v<wide_result<uint64_t>> == 2); // works!

auto main() -> int {
    auto [hi, lo] = wide_result<uint64_t>(123, 456);
    std::println("hi={}, lo={}", hi, lo);
}
