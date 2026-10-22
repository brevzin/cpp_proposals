#include <meta>
#include <debugging>

__macro maybe(bool cond, std::meta::token_sequence decl) {
    return extract<bool>(constant_of(cond))
        ? decl
        : ^^{ };
}

__macro init_if_member(std::meta::token_sequence init) {
    std::meta::info ctor = std::meta::macro_expansion_context();
    if (not std::meta::is_constructor(ctor)) {
        std::constexpr_error_str("init-if-member",
                                 "init_if_member! must initialize a constructor");
    }

    std::string_view name = identifier_of(init[0]);
    for (std::meta::info m : nonstatic_data_members_of(parent_of(ctor), std::meta::access_context::unchecked())) {
        if (has_identifier(m) && identifier_of(m) == name) {
            return init;
        }
    }

    return ^^{};
}

template <bool B>
struct C {
    int always;
    maybe!(B, int sometimes);

    constexpr C(int x, int y)
        : always(x)
        , init_if_member!(sometimes(y))
    { }
};

constexpr auto ctx = std::meta::access_context::current();
static_assert(nonstatic_data_members_of(^^C<true>, ctx).size() == 2);
static_assert(nonstatic_data_members_of(^^C<false>, ctx).size() == 1);

static_assert(C<true>(1, 2).sometimes == 2);

// ce: clang_barry -std=c++29 -freflection