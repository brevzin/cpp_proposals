#include <string_view>
#include <meta>
#include <optional>
#include <numeric>
#include <cassert>

using namespace std;

template <auto V> inline constexpr auto const& ref = V;
template <char const* P> inline constexpr char const* lit = P;

static_assert(ref<"hello"sv>.data() == lit<"hello">);
static_assert(&ref<std::optional<int>(7)> == &[: std::meta::reflect_constant(std::optional<int>(7)) :]);

// customizing support
struct Frac {
    int numer;
    int denom;
    consteval operator template() const;
};
template <int N, int D> constexpr Frac interned{N, D};
consteval Frac::operator template() const {
    // as a constant template parameter, always in lowest terms, just because
    int g = std::gcd(numer, denom);
    return substitute(^^interned, {
        std::meta::reflect_constant(numer / g),
        std::meta::reflect_constant(denom / g),
    });
}

static_assert(ref<Frac{2, 4}>.numer == 1);
static_assert(ref<Frac{2, 4}>.denom == 2);
static_assert(&ref<Frac{2, 4}> == &ref<Frac{3, 6}>);

class SmallString {
    char data_[32];
    int length_ = 0; // always <= 32

public:
    SmallString() = default;

    constexpr SmallString(std::initializer_list<char> elems)
        : data_(), length_(elems.size())
    {
        std::copy(elems.begin(), elems.end(), data_);
    }

    constexpr auto data() const -> char const* {
        return data_;
    }

    constexpr auto push_back(char c) -> void {
        assert(length_ < 31);
        data_[length_] = c;
        ++length_;
    }

    constexpr auto pop_back() -> void {
        assert(length_ > 0);
        --length_;
    }

    constexpr auto operator[](size_t idx) const -> char { return data_[idx]; }

    consteval operator template() const;
};

template <char... Cs>
inline constexpr SmallString impl = {Cs...};

consteval SmallString::operator template() const {
    std::vector<std::meta::info> elems;
    for (size_t i = 0; i != length_; ++i) {
        elems.push_back(std::meta::reflect_constant(data_[i]));
    }
    return substitute(^^impl, elems);
}

constexpr auto f() -> SmallString {
    auto s = SmallString();
    s.push_back('x');
    return s;
}

constexpr auto g() -> SmallString {
    auto s = f();
    s.push_back('y');
    s.pop_back();
    return s;
}

static_assert(&ref<f()> == &ref<g()>);
static_assert(ref<g()>[1] == '\0');


template <auto V> using constant = std::integral_constant<decltype(V), V>;

auto f(constant<SmallString{'h', 'i'}> ) -> void {

}

auto main() -> int {

}