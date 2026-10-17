#include <meta>
#include <cstddef>
#include <iterator>
#include <ranges>
#include <vector>

namespace iface {

template <class B> constexpr bool base_can_deref   = requires(B const& b) { *b; };
template <class B> constexpr bool base_can_pre_inc = requires(B& b) { ++b; };
template <class B> constexpr bool base_can_eq      =
    requires(B const& a, B const& b) { { a == b } -> std::convertible_to<bool>; };

struct IterConfig {
    std::meta::info iterator_concept;
    std::meta::info value_type;
    std::meta::info difference_type = ^^std::ptrdiff_t;
    std::meta::info adaptor = std::meta::info();
};

// Which operators has the user declared SO FAR in the class being defined?
// Caveat: We don't see friend declarations, that's still TBD for what the interface should be
consteval auto declares_op(std::meta::info cls, std::meta::operators which) -> bool {
    for (auto m : members_of(cls, std::meta::access_context::unchecked()))
        if (is_operator_function(m) && operator_of(m) == which)
            return true;
    return false;
}

consteval auto declares_fn(std::meta::info cls, std::string_view name) -> bool {
    for (auto m : members_of(cls, std::meta::access_context::unchecked()))
        if (has_identifier(m) && identifier_of(m) == name)
            return true;
    return false;
}

consteval auto iterator_interface(std::meta::info cls, IterConfig cfg) -> void {
    using namespace std::meta;
    auto ts = ^^{
        using iterator_concept = \(cfg.iterator_concept);
        using value_type = \(cfg.value_type);
        using difference_type = \(cfg.difference_type);
    };

    bool plus_eq  = declares_op(cls, op_plus_equals);
    bool minus    = declares_op(cls, op_minus);
    bool pre_inc  = declares_op(cls, op_plus_plus);
    bool pre_dec  = declares_op(cls, op_minus_minus);
    bool eq       = declares_op(cls, op_equals_equals);
    bool deref    = declares_op(cls, op_star);

    if (cfg.adaptor != std::meta::info()) {
        // Adaptor path: defer to cfg.adaptor for all operations
        info base_t = type_of(cfg.adaptor);
        bool base_deref   = extract<bool>(substitute(^^base_can_deref, {base_t}));
        bool base_pre_inc = extract<bool>(substitute(^^base_can_pre_inc, {base_t}));
        bool base_eq      = extract<bool>(substitute(^^base_can_eq, {base_t}));

        if (base_deref && !deref)
            ts += ^^{ constexpr decltype(auto) operator*() const { return *this->\(cfg.adaptor); } };
        if (base_pre_inc && !pre_inc)
            ts += ^^{ constexpr auto operator++() -> \(cls)& { ++base_reference(); return *this; } };
        if (base_pre_inc)
            ts += ^^{ constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; } };
        if (base_eq && !eq)
            ts += ^^{
                friend constexpr bool operator==(\(cls) const& a, \(cls) const& b) {
                    return a.\(cfg.adaptor) == b.\(cfg.adaptor);
                }
            };
        queue_injection(ts);
        return;
    }

    // Direct path: decide based on visible members.
    // Wrong signatures error in the injected bodies at definition time — can't check these yet
    if (plus_eq) {
        ts += ^^{
            constexpr auto operator++() -> \(cls)& { *this += 1; return *this; }
            constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; }
            constexpr auto operator--() -> \(cls)& { *this += -1; return *this; }
            constexpr auto operator--(int) -> \(cls) { auto tmp = *this; --*this; return tmp; }
            constexpr auto operator-=(std::ptrdiff_t n) -> \(cls)& { *this += -n; return *this; }
            constexpr decltype(auto) operator[](std::ptrdiff_t n) const { auto tmp = *this; tmp += n; return *tmp; }
            friend constexpr auto operator+(\(cls) it, std::ptrdiff_t n) -> \(cls) { it += n; return it; }
            friend constexpr auto operator+(std::ptrdiff_t n, \(cls) it) -> \(cls) { it += n; return it; }
            friend constexpr auto operator-(\(cls) it, std::ptrdiff_t n) -> \(cls) { it += -n; return it; }
        };
    } else {
        if (pre_inc)
            ts += ^^{ constexpr auto operator++(int) -> \(cls) { auto tmp = *this; ++*this; return tmp; } };
        if (pre_dec)
            ts += ^^{ constexpr auto operator--(int) -> \(cls) { auto tmp = *this; --*this; return tmp; } };
    }
    if (minus) {
        if (!eq)
            ts += ^^{
                friend constexpr bool operator==(\(cls) const& a, \(cls) const& b) { return (a - b) == 0; }
            };
        ts += ^^{
            friend constexpr auto operator<=>(\(cls) const& a, \(cls) const& b) { return (a - b) <=> 0; }
        };
    }
    queue_injection(ts);
}

} // namespace iface

// ==== Direct random-access user =============================================
class repeated_chars_iterator {
    char const* first_;
    std::ptrdiff_t size_;
    std::ptrdiff_t n_;

public:    
    constexpr repeated_chars_iterator() : first_(nullptr), size_(0), n_(0) {}
    constexpr repeated_chars_iterator(char const* f, std::ptrdiff_t s, std::ptrdiff_t n)
        : first_(f), size_(s), n_(n) {}

    constexpr auto operator*() const -> char { return first_[n_ % size_]; }
    constexpr auto operator+=(std::ptrdiff_t i) -> repeated_chars_iterator& { n_ += i; return *this; }
    constexpr auto operator-(repeated_chars_iterator other) const -> std::ptrdiff_t { return n_ - other.n_; }

    consteval {
        iface::iterator_interface(^^repeated_chars_iterator, {
            .iterator_concept = ^^std::random_access_iterator_tag,
            .value_type = ^^char
        });
    }
};
static_assert(std::random_access_iterator<repeated_chars_iterator>);

constexpr auto test_ra() -> bool {
    repeated_chars_iterator it("ab", 2, 0);
    auto j = it + 3;
    return *j == 'b' && j - it == 3 && it < j && it[2] == 'a' && it != j;
}
static_assert(test_ra());

// ==== Adaptor user: filtered_int_iterator style ==============================
// Provides base it_ and its own ++ (to do the filtering)
// everything else forwards to the (complete!) base iterator type.
class skip_odd_iterator {
    int const* it_;

public:
    constexpr skip_odd_iterator() : it_(nullptr) {}
    constexpr explicit skip_odd_iterator(int const* it) : it_(it) {}

    constexpr auto operator++() -> skip_odd_iterator& {
        do { ++it_; } while (*it_ % 2 != 0);
        return *this;
    }

    consteval {
        iface::iterator_interface(^^skip_odd_iterator, {
            .iterator_concept = ^^std::forward_iterator_tag,
            .value_type = ^^int,
            .adaptor = ^^it_,
        });
    }
};

static_assert(std::forward_iterator<skip_odd_iterator>);

constexpr auto test_adapt() -> bool {
    constexpr int data[] = {2, 3, 5, 4, 7, 6, 0};
    skip_odd_iterator a(data);           // 2
    auto b = a;
    ++b;                                  // user's ++: skips 3, 5 -> 4
    if (*b != 4) return false;            // injected *: forwards to base
    auto c = b++;                         // injected postfix
    if (*c != 4 || *b != 6) return false;
    return a != b && c != b && a == a;    // injected ==: base comparison
}
static_assert(test_adapt());

// ce: clang_barry -std=c++29 -freflection
