#include <debugging>
#include <meta>
#include <ranges>

namespace my {

namespace impl {
    void begin() = delete;

    template <class R>
    constexpr auto adl_begin(R&& r) noexcept(noexcept(auto(begin(r)))) -> decltype(auto(begin(r))) {
        return auto(begin(r));
    }
}

inline constexpr struct begin_fn {
    template <class R>
    __macro operator()(this begin_fn, R&& r) {
        if (not std::is_lvalue_reference_v<R> and not std::ranges::enable_borrowed_range<std::remove_cv_t<R>>) {
            std::constexpr_error_str("no-begin", "rvalue range is not borrowed");
        }
        else if (std::is_array_v<std::remove_reference_t<R>>) {
            if (not is_complete_type(remove_all_extents(^^std::remove_reference_t<R>))) {
                std::constexpr_error_str("no-begin", "T is an array with incomplete element type");
            } else {
                return ^^{ (\(as_lvalue(r)) + 0) };
            }
        }
        else if (requires(R&& t) { { auto(t.begin()) } -> std::input_or_output_iterator; }) {
            return ^^{ auto(\(as_lvalue(r)).begin()) };
        }
        else if (requires(R&& t) { { impl::adl_begin(t) } -> std::input_or_output_iterator; }) {
            return ^^{ ::my::impl::adl_begin(\(as_lvalue(r))) };
        }
        else {
            std::constexpr_error_str("no-begin", "no viable begin for this type");
        }

        return ^^{};  // unreachable: the error produces no expansion
    }
} begin{};

}

struct HasMember {
  int store[3] = {1, 2, 3};
  constexpr int* begin() { return store; }
};
struct RefBegin {
  int* p = nullptr;
  int*& begin() { return p; }
};
struct RvalueOnlyBegin {
  int* begin() && { return nullptr; }
};
struct AdlOnly {
  int store[2] = {4, 5};
  friend constexpr int* begin(AdlOnly& a) { return a.store; }
};
struct BadBegin {
  int begin() const { return 0; }
};
struct NoBegin {};


template <class R>
concept can_begin = requires (R&& r) {
    my::begin((R&&)r);
};

static_assert(can_begin<std::vector<int>&>);
static_assert(!can_begin<std::vector<int>&&>);
static_assert(can_begin<std::string_view&>);
static_assert(can_begin<std::string_view&&>);
static_assert(can_begin<int(&)[4]>);
static_assert(can_begin<HasMember&>);
static_assert(can_begin<RefBegin&>);
static_assert(!can_begin<RvalueOnlyBegin&>);
static_assert(can_begin<AdlOnly&>);
static_assert(!can_begin<BadBegin&>);
static_assert(!can_begin<NoBegin&>);

// ce: clang_barry -std=c++29 -freflection
