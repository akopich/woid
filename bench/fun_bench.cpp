
#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <function2/function2.hpp>
#include <functional>

using namespace woid;

template <typename Op, typename F>
static void benchVectorSort(benchmark::State& state) {
    auto intsOriginal = bench_common::makeRandomVector<int>(state.range(0));
    auto op = Op{};
    auto f = F{op};
    benchmark::ClobberMemory();

    for (auto _ : state) {
        state.PauseTiming();
        auto ints = intsOriginal;
        state.ResumeTiming();
        std::sort(ints.begin(), ints.end(), f);
        benchmark::ClobberMemory();
        state.PauseTiming();
    }
}

static constexpr size_t N = 1 << 18;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

struct BigLess {
    std::array<char, 32> payload;
    BigLess() {}
    bool operator()(int a, int b) const noexcept { return a < b; }
};
static_assert(sizeof(BigLess) == 32);
template <typename F>
static auto benchVectorStdLess = benchVectorSort<std::less<int>, F>;
template <typename F>
static auto benchVectorBigLess = benchVectorSort<BigLess, F>;

template <typename F>
struct Id {
    F f;
    bool operator()(int a, int b) { return f(a, b); }
};

struct Fu2SmallCapacity {
    static constexpr std::size_t capacity = 8;
    static constexpr std::size_t alignment = alignof(void*);
};
struct Fu2BigCapacity {
    static constexpr std::size_t capacity = 32;
    static constexpr std::size_t alignment = alignof(void*);
};

BENCHMARK(benchVectorStdLess<Id<std::less<int>>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int)>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int) const>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int) const noexcept>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<TrivialStorage<8>, bool(int, int) const noexcept>>)
    ->Apply(setRange);
BENCHMARK(benchVectorStdLess<
              fu2::function_base<true, true, Fu2SmallCapacity, false, false, bool(int, int) const>>)
    ->Apply(setRange);
BENCHMARK(benchVectorStdLess<std::function<bool(int, int)>>)->Apply(setRange);

BENCHMARK(benchVectorBigLess<Id<BigLess>>)->Apply(setRange);
BENCHMARK(benchVectorBigLess<Fun<Any<8>, bool(int, int) noexcept>>)->Apply(setRange);
BENCHMARK(benchVectorBigLess<
              Fun<Any<32, Copy::ENABLED, ExceptionGuarantee::NONE>, bool(int, int) noexcept>>)
    ->Apply(setRange);
BENCHMARK(benchVectorBigLess<Fun<TrivialStorage<32>, bool(int, int) const noexcept>>)
    ->Apply(setRange);
BENCHMARK(
    benchVectorBigLess<
        fu2::
            function_base<true, true, Fu2BigCapacity, false, false, bool(int, int) const noexcept>>)
    ->Apply(setRange);
BENCHMARK(benchVectorBigLess<std::function<bool(int, int)>>)->Apply(setRange);

BENCHMARK_MAIN();
