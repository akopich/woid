#include "woid.hpp"
#include <algorithm>
#include <any>
#include <benchmark/benchmark.h>
#include <limits>
#include <random>
#include <ranges>
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

static void benchGetWithGet(benchmark::State& state) {
    using Any = AnyOnePtr<8>;
    Any any{42};
    for (auto _ : state)
        benchmark::DoNotOptimize(any.template get<int>());
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
static void benchVectorConstruction(benchmark::State& state) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> intDist(1, std::numeric_limits<int>::max());
    size_t N = state.range(0);
    std::vector<ValueType> ints(N);
    std::ranges::generate(ints, [&] { return intDist(gen); });

    for (auto _ : state) {
        auto anys = ints | std::views::transform([](const ValueType& i) { return Any{i}; }) |
                    std::ranges::to<std::vector>();
        std::ranges::sort(anys, [](const Any& a, const Any& b) {
            return any_cast<ValueType>(a) < any_cast<ValueType>(b);
        });
        benchmark::ClobberMemory();
    }
}

template <typename Any>
static auto benchWithInt = benchWithValue<Any, int, 42>;

BENCHMARK(benchWithInt<AnyOnePtr<8>>);
BENCHMARK(benchWithInt<std::any>);

template <typename Any>
static auto benchWithInt64 = benchWithValue<Any, std::uint64_t, 0xDEADBEEF>;

BENCHMARK(benchWithInt64<AnyOnePtr<8>>);
BENCHMARK(benchWithInt64<std::any>);

template <typename Any>
static auto benchWithInt128 = benchWithValue<Any, __int128, 0xDEADDEADBEEF>;

BENCHMARK(benchWithInt128<AnyOnePtr<8>>);
BENCHMARK(benchWithInt128<std::any>);

template <typename Any>
static auto benchVectorConstructionInt = benchVectorConstruction<Any, int>;

template <typename Any>
static auto benchVectorConstructionInt64 = benchVectorConstruction<Any, std::uint64_t>;

struct Int128 {
    uint64_t a;
    uint64_t b;
    Int128() {}
    Int128(int x) : a(x), b(x) {}
};

bool operator<(const Int128& a, const Int128& b) { return a.a < b.a; }

static_assert(alignof(Int128) == alignof(void*));

template <typename Any>
static auto benchVectorConstructionInt128 = benchVectorConstruction<Any, Int128>;

struct NonNoThrowMoveConstructibleInt {
    int x;
    NonNoThrowMoveConstructibleInt(int x) : x(x) {}
    NonNoThrowMoveConstructibleInt() = default;
    NonNoThrowMoveConstructibleInt(const NonNoThrowMoveConstructibleInt&) = default;
    NonNoThrowMoveConstructibleInt& operator=(const NonNoThrowMoveConstructibleInt&) = default;

    NonNoThrowMoveConstructibleInt(NonNoThrowMoveConstructibleInt&& other) : x(other.x) {};
    NonNoThrowMoveConstructibleInt& operator=(NonNoThrowMoveConstructibleInt&&) = default;

    ~NonNoThrowMoveConstructibleInt() = default;
};

bool operator<(const NonNoThrowMoveConstructibleInt& a, const NonNoThrowMoveConstructibleInt& b) {
    return a.x < b.x;
}

static_assert(!std::is_nothrow_move_constructible_v<NonNoThrowMoveConstructibleInt>);

template <typename Any>
static auto benchVectorConstructionThrowInt =
    benchVectorConstruction<Any, NonNoThrowMoveConstructibleInt>;

static constexpr size_t N = 1 << 18;

constexpr auto setRange = [](auto* bench) -> void { bench->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(benchVectorConstructionInt<AnyOnePtr<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyTwoPtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyOnePtr<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyTwoPtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionInt128<AnyOnePtr<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyOnePtr<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyTwoPtrs<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyTwoPtrs<16, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyOnePtrCpy<16, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<AnyThreePtrs<16, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionInt128<std::any>)->Apply(setRange);

BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtr<8, ExceptionGuarantee::NONE>>)->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtr<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtr<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyTwoPtrs<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyTwoPtrs<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyTwoPtrs<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyOnePtrCpy<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::NONE>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::BASIC>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<AnyThreePtrs<8, ExceptionGuarantee::STRONG>>)
    ->Apply(setRange);
BENCHMARK(benchVectorConstructionThrowInt<std::any>)->Apply(setRange);

// BENCHMARK(benchCtorInt<AnyOnePtr<8>>)->MinTime(kMinTime);
// BENCHMARK(benchCtorInt<std::any>)->MinTime(kMinTime);
// BENCHMARK(benchGetInt<AnyOnePtr<8>>)->MinTime(kMinTime);
// BENCHMARK(benchGetWithGet)->MinTime(kMinTime);
// BENCHMARK(benchGetInt<std::any>)->MinTime(kMinTime);
// BENCHMARK(benchMoveInt<AnyOnePtr<8>>)->MinTime(kMinTime);
// BENCHMARK(benchMoveInt<std::any>)->MinTime(kMinTime);
// BENCHMARK(benchSwapInt<AnyOnePtr<8>>)->MinTime(kMinTime);
// BENCHMARK(benchSwapInt<std::any>)->MinTime(kMinTime);

BENCHMARK_MAIN();
