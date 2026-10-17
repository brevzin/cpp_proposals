#include <meta>
#include <string>

template <class... Ts>
struct my_tuple : std::tuple<Ts...> {
    using std::tuple<Ts...>::tuple;

    template <class Self>
    __macro operator[](this Self&& self, size_t idx) {
        return ^^{ 
            std::get<\(idx)>(\(self))
        };
    }
};

constexpr auto elems = my_tuple<int, std::string>(1, std::string("hello"));
static_assert(elems[0] == 1);
static_assert(elems[1] == "hello");

static_assert(std::same_as<decltype(elems[0]), int const&>);
static_assert(std::same_as<decltype(std::move(elems)[0]), int const&&>);

// ce: clang_barry -std=c++29 -freflection
