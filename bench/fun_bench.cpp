
#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <functional>
#include <ranges>

using namespace woid;

template <typename Op, typename F>
static void benchVectorFold(benchmark::State& state) {
    auto ints = bench_common::makeRandomVector<int>(state.range(0));
    auto op = Op{};
    F{op};
    benchmark::ClobberMemory();

    for (auto _ : state) {
        benchmark::DoNotOptimize(std::ranges::fold_left(ints, 0, F{op}));
    }
}

static constexpr size_t N = 1 << 18;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

template <typename F>
static auto benchVectorStdPlus = benchVectorFold<std::plus<int>, F>;

using Plus = int(int, int);
BENCHMARK(benchVectorStdPlus<std::function<Plus>>)->Apply(setRange);
BENCHMARK(benchVectorStdPlus<Fun<Any<8>, int(int, int)>>)->Apply(setRange);
BENCHMARK(benchVectorStdPlus<Fun<Any<8>, int(int, int)> const>)->Apply(setRange);
BENCHMARK(benchVectorStdPlus<Fun<Any<8>, int(int, int) const noexcept>>)->Apply(setRange);

BENCHMARK_MAIN();
