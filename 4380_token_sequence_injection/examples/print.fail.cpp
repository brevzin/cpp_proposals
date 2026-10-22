#include <meta>
#include <debugging>
using std::meta::id;

consteval auto interface_functions_of(std::meta::info ty) -> std::vector<std::meta::info> {
    auto v = members_of(ty, std::meta::access_context::current());
    std::erase_if(v, [](std::meta::info m){
        return not is_function(m) or is_special_member_function(m) or is_static_member(m);
    });
    return v;
}

consteval auto param_tokens(std::vector<std::meta::info> params,
                            std::string_view  name_prefix = "")
    -> std::meta::token_sequence
{
    auto result = std::meta::list_builder(^^{ , });
    for (int k = 0; std::meta::info p : params) {
        if (is_function_parameter(p)) p = type_of(p);
        if (not name_prefix.empty()) {
            result += ^^{ \(p) \(id(name_prefix, k++)) };
        } else {
            result += ^^{ \(p) };
        }
    }

    return result;
}

consteval auto inject_Vtable(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// struct VTable {
    ///   auto (*f0)(void const*, std::ostream&);
    /// } const *vtable;
    /// ```
    /// 
    /// The function is named f0, not draw, in order to handle overloads
    auto vtable_members = std::meta::list_builder();
    for (int k = 0; std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info  r = return_type_of(mem);
        auto params = std::meta::list_builder(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* } : ^^{ void* };
        params += param_tokens(parameters_of(mem));
        vtable_members += ^^{
            \(r) (*\(id("f", k++)))(\(params));
        };
    }

    queue_injection(^^{
        struct VTable {
            \(vtable_members)
        } const *vtable;
    });
}

consteval auto inject_vtable_for(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// template <class T>
    /// static inline constexpr VTable vtable_for = {
    ///   +[](void const* obj, std::ostream& p0) {
    ///     return static_cast<T const*>(obj)->draw(static_cast<std::ostream&>(p0));
    ///   }
    /// };
    /// ```
    /// The seemingly-unnecessary static_cast is to forward the parameters. 
    auto inits = std::meta::list_builder(^^{ , });
    for (std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = identifier_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += is_const(type_of(mem)) ? ^^{ void const* obj } : ^^{ void* obj };
        params += param_tokens(parameters_of(mem), "p");
        std::meta::token_sequence cast_type = is_const(type_of(mem)) ? ^^{ T const* } : ^^{ T* };
        for (int k = 0; std::meta::info _ : parameters_of(mem)) {
            auto pN = id("q", k++);
            args += ^^{ static_cast<decltype(\(pN))&&>(\(pN)) };
        }

        auto next_fn = ^^{
            +[](\(params))-> \(r) {
                return static_cast<\(cast_type)>(obj)->\(id(name))( \(args) );
            }
        };

        std::constexpr_print_str(stringize(next_fn));

        inits += next_fn;
    }

    queue_injection(^^{
        template <class T>
        static inline constexpr VTable vtable_for = {
            \(inits)
        };
    });
}


consteval auto inject_interface(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// auto draw(std::ostream& p0) const -> void {
    ///     return vtable->f0(data, static_cast<std::ostream&>(p0));
    /// }
    /// ```    
    auto forwarders = std::meta::list_builder();
    for (int idx = 0; std::meta::info mem : interface_functions_of(interface)) {
        std::meta::info r = return_type_of(mem);
        auto name = id(identifier_of(mem));
        auto vtable_func = id("f", idx++);
        auto param_list = parameters_of(mem);
        std::meta::list_builder params(^^{ , }), args(^^{ , });
        params += param_tokens(param_list, "p");
        args += ^^{ data };
        for (int k = 0; k < param_list.size(); ++k) {
            auto pN = id("p", k);
            args += ^^{ static_cast<decltype(\(pN))&&>(\(pN)) };
        }
        auto suffix = is_const(type_of(mem)) ? ^^{ const } : ^^{ };

        forwarders += ^^{
            auto \(name)(\(params)) \(suffix) -> \(r) {
                return vtable->\(vtable_func)(\(args));
            }
        };
    }

    queue_injection(forwarders);
}

consteval auto inject_satisfies_for(std::meta::info interface) -> void {
    /// Injects e.g.
    /// ```
    /// template <class T>
    /// static consteval auto satisfies_interface() -> bool {
    ///     return requires { 
    ///         { std::declval<T const&>().draw(std::declval<std::ostream&>()) } -> std::convertible_to<void>;
    ///     };
    /// }
    /// ```
    auto constraints = std::meta::list_builder();
    for (std::meta::info mem : interface_functions_of(interface)) {
        auto obj_type = is_const(type_of(mem)) ? ^^{ T const } : ^^{ T };
        auto args = std::meta::list_builder(^^{ , });
        for (std::meta::info param : parameters_of(mem)) {
            args += ^^{ std::declval<\(type_of(param))>() };
        }
        constraints += ^^{
            { std::declval<\(obj_type)&>().\(id(identifier_of(mem)))(\(args)) } -> std::convertible_to<\(return_type_of(mem))>;
        };
    }

    queue_injection(^^{
        template <class T>
        static consteval auto satisfies_interface() -> bool {
            return requires {
                \(constraints)
            };
        }
    });
}

consteval auto inject_erasing_ctor() -> void {
    queue_injection(^^{
        template <class T>
            requires (!std::same_as<std::remove_cvref_t<T>, DynRef>)
                 and (satisfies_interface<std::remove_reference_t<T>>())
        DynRef(T&& t [[clang::lifetimebound]])
            : data((void*)(&t))
            , vtable(&vtable_for<std::remove_cvref_t<T>>)
        {}
    });
}

template<class Iface> class DynRef {
    void *data;
    consteval {
        inject_Vtable(^^Iface);
        inject_vtable_for(^^Iface);
        inject_satisfies_for(^^Iface);
    }

public:
    consteval {
        inject_interface(^^Iface);
        inject_erasing_ctor();
    }

    DynRef(DynRef&) = default;
    DynRef(DynRef const&) = default;
};

#include <iostream>

struct Interface {
    auto draw(std::ostream&) const -> void;
};

// We actually do validate that the types conform to the interface
// int isn't even a class type, Wrong::draw is non-const
struct Wrong { auto draw(std::ostream&) -> void; };
static_assert(!std::constructible_from<DynRef<Interface>, int>);
static_assert(!std::constructible_from<DynRef<Interface>, Wrong>);

struct Constant {
    auto draw(std::ostream& out) const -> void {
        out << "Constant(" << 1 << ')';
    }
};

struct Variable {
    int i;

    auto draw(std::ostream& out) const -> void {
        out << "Variable(" << i << ')';
    }
};

int main() {
    auto c = Constant();
    auto v1 = Variable{10};
    auto const v2 = Variable{20};

    auto stuff = std::vector<DynRef<Interface>>{c, v1, v2};
    for (auto& d : stuff) {
        std::cout << "* ";
        d.draw(std::cout);
        std::cout << '\n';
    }
}
