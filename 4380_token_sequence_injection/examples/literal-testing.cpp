#include <meta>
#include <print>
#include <ranges>
#include <cstdint>

constexpr std::uint64_t splitmix64( std::uint64_t z )
{
    z ^= z >> 30;
    z *= 0xbf58476d1ce4e5b9ull;
    z ^= z >> 27;
    z *= 0x94d049bb133111ebull;
    z ^= z >> 31;

    return z;
}

static int s_errors;

template<class T1, class T2>
void test( char const* q1, char const* q2, T1 const& t1, T2 const& t2 )
{
    if( !(t1 == t2) )
    {
        std::print( stderr, "Test '{} == {}' failed: '{}' != '{}'\n", q1, q2, t1, t2 );
        ++s_errors;
    }
}

template<int N> int popcount( unsigned _BitInt(N) const& v )
{
    int r = 0;

    for( int i = 0; i < N; i += 64 )
    {
        r += __builtin_popcount( static_cast<std::uint64_t>( v >> i ) );
    }

    return r;
}

int popcount( std::uint64_t v )
{
    return __builtin_popcountll( v );
}

int main()
{
    constexpr auto fn = ^^{ popcount };

    consteval
    {
        for( auto v: std::views::iota( 0, 5 ) | std::views::transform( splitmix64 ) )
        {
            auto t1 = std::meta::tokenize( v, "__uwb" ); // "uwb"
            auto q1 = ^^{ \(fn)(\(t1)) };

            auto t2 = std::meta::tokenize( v, "ull" );
            auto q2 = ^^{ \(fn)(\(t2)) };

            queue_injection( ^^{ test( \(stringize(q1)), \(stringize(q2)), \(q1), \(q2) ); } );
        }
    }

    return s_errors;
}

// ce-run: clang_barry -std=c++29 -freflection -O2
