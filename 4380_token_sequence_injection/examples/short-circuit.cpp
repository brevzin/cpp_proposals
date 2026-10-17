#include <meta>
#include <string>
#include <print>

template <class T>
__macro fwd(T&& t) {
    return ^^{ static_cast<\(type_of(t))&&>(\(t)) };
}

struct Bool {
    bool b;

    __macro operator or(this Bool self, bool rhs) {
        return ^^{ \(self).b or \(rhs) };
    }

    __macro operator and(this Bool self, bool rhs) {
        return ^^{ \(self).b and \(rhs) };
    }    
};

auto call() -> bool {
    std::println("call()");
    return true;
}

auto main() -> int {
    bool const a = Bool{true} or call();
    std::println("a = {}", a);
    bool const b = Bool{false} or call();
    std::println("b = {}", b);
    bool const c = Bool{true} and call();
    std::println("c = {}", c);
    bool const d = Bool{false} and call();
    std::println("d = {}", d);        
}
