
#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
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

template <size_t Alignment>
struct alignas(Alignment) BigLess {
    int i = 0;
    BigLess() {}
    bool operator()(int a, int b) noexcept {
        i++;
        return a < b;
    }
};
static_assert(sizeof(BigLess<32>) == 32);
template <typename F>
static auto benchVectorStdLess = benchVectorSort<std::less<int>, F>;
template <typename F>
static auto benchVectorBigLess = benchVectorSort<BigLess<32>, F>;

template <typename F>
struct Id {
    F f;
    bool operator()(int a, int b) { return f(a, b); }
};

BENCHMARK(benchVectorStdLess<Id<std::less<int>>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int)>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int) const>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<Fun<Any<8>, bool(int, int) const noexcept>>)->Apply(setRange);
BENCHMARK(benchVectorStdLess<std::function<bool(int, int)>>)->Apply(setRange);

BENCHMARK(benchVectorBigLess<Id<BigLess<32>>>)->Apply(setRange);
BENCHMARK(benchVectorBigLess<Fun<Any<8>, bool(int, int) noexcept>>)->Apply(setRange);
BENCHMARK(benchVectorBigLess<
              Fun<Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, 32>, bool(int, int) noexcept>>)
    ->Apply(setRange);
BENCHMARK(benchVectorBigLess<std::function<bool(int, int)>>)->Apply(setRange);

BENCHMARK_MAIN();
