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

} // namespace bench_common
