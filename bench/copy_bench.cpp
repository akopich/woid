#include "common.hpp"
#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>

using namespace woid;

template <typename T>
struct Copy {
    T t;

    template <typename... Args>
    explicit Copy(Args&&... args) : t{std::forward<Args>(args)...} {}

    Copy(const Copy&) = default;
    Copy& operator=(const Copy&) = default;

    Copy(Copy&& other) : Copy(static_cast<const Copy&>(other)) {}
    Copy& operator=(const Copy&& other) {
        *this = static_cast<const Copy&>(other);
        return *this;
    }

    ~Copy() = default;
};

template <typename Any, typename ValueType>
static void benchVectorConstructionAndSort(benchmark::State& state) {
    using CA = Copy<Any>;
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
    }
}

template <typename Any>
static auto benchVectorConstructionInt = benchVectorConstructionAndSort<Any, int>;

template <typename Any>
static auto benchVectorConstructionInt64 = benchVectorConstructionAndSort<Any, std::uint64_t>;

template <typename Any>
static auto benchVectorConstructionInt128
    = benchVectorConstructionAndSort<Any, bench_common::Int128>;

static constexpr size_t N = 1 << 18;

template <typename Any>
static auto benchVectorConstructionThrowInt
    = benchVectorConstructionAndSort<Any, bench_common::NonNoThrowMoveConstructibleInt>;

constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(benchVectorConstructionInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionInt128<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyOnePtrCpy<16, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyThreePtrs<16, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<std::any>)->Apply(setRange);

BENCHMARK_MAIN();
