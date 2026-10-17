#include <meta>
#include <algorithm>
#include <cassert>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

template <class I>
struct mock : I {
    mutable std::vector<std::string_view> calls_;

    int call_count(std::string_view name) const {
        return std::ranges::count(calls_, name);
    }

    consteval {
        for (std::meta::info m : members_of(^^I, std::meta::access_context::current())) {
            if (not is_function(m) 
                or not is_virtual(m)
                or is_destructor(m)
                or is_operator_function(m) 
                or is_conversion_function(m)) {
                continue;
            }

            auto name = identifier_of(m);
            auto d = make_override(declaration_of(m));
            if (is_noexcept(type_of(m))) {
                d = make_noexcept(d);
            }
            std::meta::info ret = return_type_of(m);

            // The handler member's type: std::function<R(Params...)>.
            auto ptypes = std::meta::list_builder(^^{ , });
            for (std::meta::info p : parameters_of(m)) {
                ptypes += ^^{ \(type_of(p)) };
            }

            // Forward the clone's parameters (p0, p1, ...) into the handler.
            auto args = argument_list_for(d);

            // The handler named {name}_
            auto handler = std::meta::id(name, "_");
            queue_injection(^^{
                public:
                std::function<\(ret)(\(ptypes))> \(handler);
            });

            // The full override
            queue_injection(^^{
                public:
                \(d) {
                    calls_.push_back(\(std::meta::str_lit(name)));
                    if (\(handler)) {
                        return \(handler)(\(args));
                    } else {
                        return \(ret)();
                    }
                }
            });
        }
    }
};

// ----------------------------------------------------------------------------
// The interface under test.
// ----------------------------------------------------------------------------
struct Calculator {
    virtual ~Calculator() = default;
    virtual int add(int a, int b) = 0;
    virtual int negate(int x) const = 0;           // const virtual
    virtual std::string describe() const = 0;      // class-type result
    virtual void reset() = 0;                      // void
    virtual int increment(int x, int by = 1) = 0;  // default argument
    virtual int fast(int x) const noexcept = 0;    // noexcept virtual
};

// Every pure virtual is (implicitly) overridden by its clone.
static_assert(std::is_abstract_v<Calculator>);
static_assert(!std::is_abstract_v<mock<Calculator>>);

// The handler members have the real signatures.
static_assert(std::is_same_v<decltype(mock<Calculator>::add_),
                             std::function<int(int, int)>>);
static_assert(std::is_same_v<decltype(mock<Calculator>::describe_),
                             std::function<std::string()>>);

// make_noexcept mirrored the noexcept virtual (a laxer override would not
// even have compiled).
static_assert(noexcept(std::declval<const mock<Calculator>&>().fast(1)));
static_assert(!noexcept(std::declval<mock<Calculator>&>().add(1, 2)));

// Transformations produce new, distinct descriptions.
static_assert(std::meta::declaration_of(^^Calculator::add) !=
              std::meta::make_override(std::meta::declaration_of(^^Calculator::add)));

// Code under test: only sees the interface.
auto run_twice(Calculator& c, int start) -> int {
    c.reset();
    return c.increment(c.increment(start));  // uses the default 'by'
}

auto main() -> int {
    mock<Calculator> m;
    m.add_ = [](int a, int b) { return a + b; };
    m.increment_ = [](int x, int by) { return x + by; };

    Calculator& c = m;

    // Virtual dispatch lands in the mock.
    assert(c.add(2, 3) == 5);

    // Uninteresting calls (no handler) return a default-constructed result.
    assert(c.negate(5) == 0);
    assert(c.describe() == "");

    // Handlers can be (re)set mid-test.
    m.negate_ = [](int x) { return -x; };
    assert(c.negate(7) == -7);

    // The cloned default argument works through both static types. (GMock's
    // MOCK_METHOD cannot mock a defaulted parameter at all.)
    assert(c.increment(41) == 42);
    assert(m.increment(10, 5) == 15);

    // Drive it through interface-only code.
    assert(run_twice(c, 0) == 2);

    // The noexcept member mocks like any other.
    m.fast_ = [](int x) { return x * 3; };
    assert(c.fast(5) == 15);

    // Expectations.
    assert(m.call_count("add") == 1);
    assert(m.call_count("negate") == 2);
    assert(m.call_count("describe") == 1);
    assert(m.call_count("reset") == 1);
    assert(m.call_count("increment") == 4);
    assert(m.calls_.front() == "add");
}

// ce: clang_barry -std=c++29 -freflection
