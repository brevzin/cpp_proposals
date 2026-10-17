#include <meta>
#include <algorithm>

template <class T>
concept EqComparable = requires (T const& t) {
  static_cast<bool>(t == t);
};

struct Eq_t {
    consteval auto inject_members(std::meta::info r) const -> std::meta::token_sequence {
        auto subobjects = subobjects_of(r, std::meta::access_context::unchecked());
        if (not std::ranges::all_of(subobjects, [](std::meta::info o){
            return extract<bool>(substitute(^^EqComparable, {type_of(o)}));
            }))
        {
            // not all comparable? defined as deleted
            return ^^{
                friend constexpr bool operator==(\(r) const&, \(r) const&) = delete;
                friend constexpr bool operator!=(\(r) const&, \(r) const&) = delete;
            };        
        }

        std::meta::list_builder cmp(^^{ && });
        for (std::meta::info s : subobjects) {
            cmp += ^^{ (__lhs.\(s) == __rhs.\(s)) };
        }

        if (subobjects.empty()) {
            cmp += ^^{ true };
        }

        return ^^{
            friend constexpr bool operator==(\(r) const& __lhs [[maybe_unused]],
                                             \(r) const& __rhs [[maybe_unused]]) {
                return \(cmp);
            }

            friend constexpr bool operator!=(\(r) const& __lhs, \(r) const& __rhs) {
                return !(__lhs == __rhs);
            }
        };
    }
};
inline constexpr Eq_t Eq{};

struct [[=Eq]] P {
  int x;
  int y;
};
static_assert(P{1, 2} == P{1, 2});
static_assert(P{1, 2} != P{1, 3});

struct [[=Eq]] D : P {
    int z;
};
static_assert(D{{1, 2}, 3} == D{{1, 2}, 3});
static_assert(D{{1, 2}, 3} != D{{1, 2}, 4});
static_assert(D{{1, 2}, 3} != D{{1, 3}, 3});


template <class T> struct [[=Eq]] Wrap { T t; };
struct NonComparable { };

static_assert(std::equality_comparable<Wrap<int>>);
static_assert(!std::equality_comparable<Wrap<NonComparable>>);

// ce: clang_barry -std=c++29 -freflection
