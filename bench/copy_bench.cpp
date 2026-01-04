#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>

using namespace woid;

template <typename T>
struct CopyOnly {
    T t;

    template <typename... Args>
    explicit CopyOnly(Args&&... args) : t{std::forward<Args>(args)...} {}

    CopyOnly(const CopyOnly&) = default;
    CopyOnly& operator=(const CopyOnly&) = default;

    CopyOnly(CopyOnly&& other) : CopyOnly(static_cast<const CopyOnly&>(other)) {}
    CopyOnly& operator=(const CopyOnly&& other) {
        *this = static_cast<const CopyOnly&>(other);
        return *this;
    }

    ~CopyOnly() = default;
};

static constexpr size_t N = 1 << 18;
using Allocator = detail::OneChunkAllocator<N * sizeof(__int128) * 200>;

template <typename Any, typename ValueType>
static void benchVectorConstructionAndSort(benchmark::State& state) {
    using CA = CopyOnly<Any>;
    auto ints = bench_common::makeRandomVector<ValueType>(state.range(0));

    for (auto _ : state) {
        auto anys = ints
                    | std::views::transform(
                        [](const ValueType& i) { return CA{std::in_place_type<ValueType>, i}; })
                    | std::ranges::to<std::vector>();
        std::sort(anys.begin(), anys.end(), [](CA a, CA b) {
            return any_cast<ValueType>(a.t) < any_cast<ValueType>(b.t);
        });
        benchmark::ClobberMemory();
        state.PauseTiming();
        Allocator::reset();
        state.ResumeTiming();
    }
}

template <typename Any>
static auto benchVectorConstructionAndSortInt = benchVectorConstructionAndSort<Any, int>;

template <typename Any>
static auto benchVectorConstructionInt64 = benchVectorConstructionAndSort<Any, std::uint64_t>;

template <typename Any>
static auto benchVectorConstructionAndSortInt128
    = benchVectorConstructionAndSort<Any, bench_common::Int128>;

template <typename Any>
static auto benchVectorConstructionAndSortThrowInt
    = benchVectorConstructionAndSort<Any, bench_common::NonNoThrowMoveConstructibleInt>;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<TrivialAny<>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<DynamicAny<>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionAndSortInt128<Any<8,
                                                   Copy::ENABLED,
                                                   ExceptionGuarantee::NONE,
                                                   alignof(void*),
                                                   FunPtr::COMBINED,
                                                   SafeAnyCast::DISABLED,
                                                   Allocator>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<Any<8,
                                                   Copy::ENABLED,
                                                   ExceptionGuarantee::NONE,
                                                   alignof(void*),
                                                   FunPtr::DEDICATED,
                                                   SafeAnyCast::DISABLED,
                                                   Allocator>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<
              Any<16, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<
              Any<16, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<TrivialAny<8>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<TrivialAny<16>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<DynamicAny<>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortInt128<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::NONE, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::BASIC, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::COMBINED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<
              Any<8, Copy::ENABLED, ExceptionGuarantee::STRONG, alignof(void*), FunPtr::DEDICATED>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<DynamicAny<>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<TrivialAny<8>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionAndSortThrowInt<std::any>)->Apply(setRange);
BENCHMARK_MAIN();
