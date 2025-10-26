#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>
#include <print>
#include <random>
#include <ranges>

using namespace woid;

template <typename Any, typename ValueType>
static void benchCopyAssignment(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> intDist(0, std::numeric_limits<int>::max());
    size_t N = state.range(0);
    std::vector<ValueType> ints(N);
    std::ranges::generate(ints, [&] { return intDist(gen); });
    auto anys = ints
                | std::views::transform([](const ValueType& i) { return Any{i}; })
                | std::ranges::to<std::vector>();
    std::vector<Any> result(N + 1, Any{ValueType{0}});
    benchmark::ClobberMemory();

    for (auto _ : state) {
        size_t rotationIndex = intDist(gen) % N;
        auto it = std::next(anys.begin(), rotationIndex);
        std::ranges::rotate_copy(anys, it, result.begin());
        benchmark::ClobberMemory();
    }
}

template <typename Any, typename ValueType>
static void benchCopyCtor(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> intDist(0, std::numeric_limits<int>::max());
    size_t N = state.range(0);
    std::vector<ValueType> ints(N);
    std::ranges::generate(ints, [&] { return intDist(gen); });
    auto anys = ints
                | std::views::transform([](const ValueType& i) { return Any{i}; })
                | std::ranges::to<std::vector>();

    benchmark::ClobberMemory();

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<Any> result;
        result.reserve(N);
        size_t rotationIndex = intDist(gen) % N;
        auto it = std::next(anys.begin(), rotationIndex);

        state.ResumeTiming();
        std::ranges::rotate_copy(anys, it, std::back_inserter(result));
        state.PauseTiming();

        benchmark::ClobberMemory();
    }
}

template <typename Any>
static auto benchCopyCtorInt = benchCopyCtor<Any, int>;

template <typename Any>
static auto benchCopyAssignmentInt = benchCopyAssignment<Any, int>;

template <typename Any>
static auto benchVectorConstructionAndRotateInt64 = benchCopyCtor<Any, std::uint64_t>;

struct Int128 {
    uint64_t a;
    uint64_t b;
    Int128() {}
    Int128(int x) : a(x), b(x) {}
};

template <typename Any>
static auto benchVectorConstructionAndRotateInt128 = benchCopyCtor<Any, Int128>;

static constexpr size_t N = 1 << 18;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<std::any>)->Apply(setRange);

BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<std::any>)->Apply(setRange);

BENCHMARK_MAIN();
