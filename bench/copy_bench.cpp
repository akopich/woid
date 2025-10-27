#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>

using namespace woid;

template <typename Any, typename ValueType>
static void benchCopyAssignment(benchmark::State& state) {
    size_t N = state.range(0);

    for (auto _ : state) {
        state.PauseTiming();
        auto anys = bench_common::wrapInts<Any>(bench_common::makeRandomVector<ValueType>(N));
        std::vector<Any> result(N + 1, Any{ValueType{0}});

        benchmark::ClobberMemory();

        state.ResumeTiming();
        std::ranges::copy(anys, result.begin());
        state.PauseTiming();

        benchmark::ClobberMemory();
    }
}

template <typename Any, typename ValueType>
static void benchCopyCtor(benchmark::State& state) {
    size_t N = state.range(0);
    auto anys = bench_common::wrapInts<Any>(bench_common::makeRandomVector<ValueType>(N));

    benchmark::ClobberMemory();

    for (auto _ : state) {
        state.PauseTiming();
        std::vector<Any> result;
        result.reserve(N);

        benchmark::ClobberMemory();

        state.ResumeTiming();
        std::ranges::copy(anys, std::back_inserter(result));
        state.PauseTiming();

        benchmark::ClobberMemory();
    }
}

template <typename Any>
static auto benchCopyCtorInt = benchCopyCtor<Any, int>;

template <typename Any>
static auto benchCopyCtorInt128 = benchCopyCtor<Any, __int128>;

template <typename Any>
static auto benchCopyCtorThrowInt
    = benchCopyCtor<Any, bench_common::NonNoThrowMoveConstructibleInt>;

template <typename Any>
static auto benchCopyAssignmentInt = benchCopyAssignment<Any, int>;

template <typename Any>
static auto benchCopyAssignmentInt128 = benchCopyAssignment<Any, __int128>;

template <typename Any>
static auto benchCopyAssignmentThrowInt
    = benchCopyAssignment<Any, bench_common::NonNoThrowMoveConstructibleInt>;

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

BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt<std::any>)->Apply(setRange);

BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt<std::any>)->Apply(setRange);

BENCHMARK(benchCopyCtorInt128<AnyOnePtrCpy<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<AnyThreePtrs<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<AnyOnePtrCpy<16, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<AnyThreePtrs<16, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<AnyOnePtrCpy<16, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<AnyThreePtrs<16, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorInt128<std::any>)->Apply(setRange);

BENCHMARK(benchCopyAssignmentInt128<AnyOnePtrCpy<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<AnyThreePtrs<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<AnyOnePtrCpy<16, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<AnyThreePtrs<16, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<AnyOnePtrCpy<16, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<AnyThreePtrs<16, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentInt128<std::any>)->Apply(setRange);

BENCHMARK(benchCopyCtorThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchCopyCtorThrowInt<std::any>)->Apply(setRange);

BENCHMARK(benchCopyAssignmentThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchCopyAssignmentThrowInt<std::any>)->Apply(setRange);

BENCHMARK_MAIN();
