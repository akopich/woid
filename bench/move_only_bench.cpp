#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>
#include <utility>

using namespace woid;

template <typename AnyType, typename AnyValueType, typename... Args>
static auto runAny(Args&&... args) {
    AnyType a{std::in_place_type<AnyValueType>, std::forward<Args>(args)...};
    benchmark::ClobberMemory();
    AnyType b(std::move(a));
    benchmark::ClobberMemory();
    return any_cast<AnyValueType>(b);
}

template <typename Any, typename ValueType, auto Value>
static void benchWithValue(benchmark::State& state) {
    for (auto _ : state) {
        runAny<Any, ValueType>(Value);
        benchmark::ClobberMemory();
    }
}

template <typename Any>
static void benchCtorInt(benchmark::State& state) {
    for (auto _ : state)
        benchmark::DoNotOptimize(Any{42});
}

template <typename Any>
static void benchGetInt(benchmark::State& state) {
    Any any{42};
    benchmark::ClobberMemory();
    for (auto _ : state)
        benchmark::DoNotOptimize(any_cast<int>(any));
}

template <typename Any>
static void benchMoveInt(benchmark::State& state) {
    for (auto _ : state) {
        Any source{42};
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(Any{std::move(source)});
    }
}

template <typename Any>
static void benchSwapInt(benchmark::State& state) {
    Any a{42};
    Any b{37};
    for (auto _ : state) {
        using std::swap;
        swap(a, b);
        benchmark::ClobberMemory();
    }
}

template <typename Any, typename ValueType>
static void benchVectorConstructionAndSort(benchmark::State& state) {
    auto ints = bench_common::makeRandomVector<ValueType>(state.range(0));

    for (auto _ : state) {
        auto anys = bench_common::wrapInts<Any, ValueType>(ints);
        std::ranges::sort(anys, [](const Any& a, const Any& b) {
            return any_cast<ValueType>(a) < any_cast<ValueType>(b);
        });
        benchmark::ClobberMemory();
    }
}

template <typename Any>
static auto benchVectorConstructionInt = benchVectorConstructionAndSort<Any, int>;

template <typename Any>
static auto benchVectorConstructionInt64 = benchVectorConstructionAndSort<Any, std::uint64_t>;

template <typename Any>
static auto benchVectorConstructionInt128
    = benchVectorConstructionAndSort<Any, bench_common::Int128>;

template <typename Any>
static auto benchVectorConstructionThrowInt
    = benchVectorConstructionAndSort<Any, bench_common::NonNoThrowMoveConstructibleInt>;

static constexpr size_t N = 1 << 18;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionInt128<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<16, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<16, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<16, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<
              Any<16, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::DISABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(
    benchVectorConstructionThrowInt<
        Any<8, Copy::DISABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<std::any>)->Apply(setRange);

BENCHMARK_MAIN();
