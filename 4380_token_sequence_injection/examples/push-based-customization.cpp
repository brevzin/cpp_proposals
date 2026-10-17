#include <meta>
#include <print>

// ------------------------- the library -------------------------
namespace lib {
    struct inject_bindings_t {
        consteval auto on_template_defined(std::meta::info tmpl) const -> void {
            auto d = declaration_of(tmpl);
            auto head = template_parameter_list_for(d, {.defaults = false});
            auto args = template_argument_list_for(d);
            auto self = ^^{ \(tmpl)<\(args)> };

            queue_injection(^^std, ^^{
                template <\(head)>
                struct tuple_size<\(self)>
                    : integral_constant<size_t, size(\(self)::tuple_elements)>
                { };

                template <size_t I, \(head)>
                struct tuple_element<I, \(self)> {
                    using type = [: type_of(\(self)::tuple_elements[I]) :];
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
