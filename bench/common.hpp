#include <algorithm>
#include <limits>
#include <random>
#include <ranges>

namespace bench_common {

inline auto randInt() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> intDist(1, std::numeric_limits<int>::max());
    return intDist(gen);
}

template <typename ValueType>
auto makeRandomVector(size_t N) {
    std::vector<ValueType> ints(N);
    std::ranges::generate(ints, &randInt);
    return ints;
}

template <typename Any, typename ValueType>
auto wrapInts(const std::vector<ValueType>& ints) {
    return ints
           | std::views::transform([](const ValueType& i) { return Any{i}; })
           | std::ranges::to<std::vector>();
}

struct NonNoThrowMoveConstructibleInt {
    int x;
    NonNoThrowMoveConstructibleInt(int x) : x(x) {}
    NonNoThrowMoveConstructibleInt() = default;
    NonNoThrowMoveConstructibleInt(const NonNoThrowMoveConstructibleInt&) = default;
    NonNoThrowMoveConstructibleInt& operator=(const NonNoThrowMoveConstructibleInt&) = default;

    NonNoThrowMoveConstructibleInt(NonNoThrowMoveConstructibleInt&& other) noexcept(false)
        = default;
    NonNoThrowMoveConstructibleInt& operator=(NonNoThrowMoveConstructibleInt&&) noexcept(false)
        = default;

    ~NonNoThrowMoveConstructibleInt() = default;
};

static_assert(!std::is_nothrow_move_constructible_v<bench_common::NonNoThrowMoveConstructibleInt>);

inline bool operator<(const NonNoThrowMoveConstructibleInt& a,
                      const NonNoThrowMoveConstructibleInt& b) {
    return a.x < b.x;
}

struct Int128 {
    uint64_t a;
    uint64_t b;
    Int128() {}
    Int128(int x) : a(x), b(x) {}
};

inline bool operator<(const Int128& a, const Int128& b) {
    return std::pair{a.a, a.b} < std::pair{b.a, b.b};
}

static_assert(alignof(Int128) == alignof(void*));

} // namespace bench_common
