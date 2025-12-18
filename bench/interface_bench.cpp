#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <print>
#include <random>
#include <ranges>
#include <type_traits>

using namespace woid;

struct VShape {
    virtual ~VShape() = default;
    virtual double area() const = 0;
    virtual double perimeter() const = 0;
    virtual void draw() const = 0;
};

struct VSquare : VShape {
    double side;
    VSquare(double s) : side(s) {}

    double area() const override { return side * side; }

    double perimeter() const override { return 4 * side; }

    void draw() const override { std::println("VSquare(a={})", side); }
};

struct VRectangle : VShape {
    double length;
    double width;
    VRectangle(double l, double w) : length(l), width(w) {}

    double area() const override { return length * width; }

    double perimeter() const override { return 2 * (length + width); }

    void draw() const override { std::println("VRectangle(l={}, w={})", length, width); }
};

struct VCircle : VShape {
    double radius;
    VCircle(double r) : radius(r) {}

    double area() const override { return std::numbers::pi * radius * radius; }

    double perimeter() const override { return 2 * std::numbers::pi * radius; }

    void draw() const override { std::println("VCircle(r={})", radius); }
};

struct Square {
    double side;

    double area() const { return side * side; }

    double perimeter() const { return 4 * side; }

    void draw() const { std::println("Square(a={})", side); }
};

struct Rectangle {
    double length;
    double width;

    double area() const { return length * width; }

    double perimeter() const { return 2 * (length + width); }

    void draw() const { std::println("Rectangle(l={}, w={})", length, width); }
};

struct Circle {
    double radius;

    double area() const { return std::numbers::pi * radius * radius; }

    double perimeter() const { return 2 * std::numbers::pi * radius; }

    void draw() const { std::println("Circle(r={})", radius); }
};

static_assert(alignof(Rectangle) == alignof(void*));

// clang-format off
using Builder = woid::InterfaceBuilder
           ::WithStorage<woid::Any<sizeof(Rectangle), woid::Copy::DISABLED>>
           ::Fun<"area", [](const auto& obj) -> double { return obj.area(); } >
           ::Method<"perimieter", double()const, []<typename T> {return &T::perimeter; } >
           ::Method<"draw", void()const, []<typename T> {return &T::draw; } > ;
// clang-format on

using SharedBase = Builder::WithSharedVTable::Build;

using DedicatedBase = Builder::WithDedicatedVTable::Build;

struct WoidShapeShared : SharedBase {
    using SharedBase::SharedBase;
    double area() const { return call<"area">(); }
};

struct WoidShapeDedicated : DedicatedBase {
    using DedicatedBase::DedicatedBase;
    double area() const { return call<"area">(); }
};

using DedicatedShardBase
    = Builder::WithSharedVTable::WithStorage<woid::detail::DynamicStorage<>>::Build;

struct WoidShapeSharedDynamic : DedicatedShardBase {
    using DedicatedShardBase::DedicatedShardBase;
    double area() const { return call<"area">(); }
};

auto makeRandomDoubles(int N) {
    std::vector<double> result(N);

    std::mt19937 gen(1234);
    std::uniform_real_distribution<> distrib(0.0, 1.0);

    std::ranges::generate(result, [&]() { return distrib(gen); });
    return result;
}

template <typename I>
constexpr static auto kComparator = [](const I& i, const I& j) { return i.area() < j.area(); };

template <typename T>
constexpr static auto kComparator<std::unique_ptr<T>> =
    [](const std::unique_ptr<T>& i, const std::unique_ptr<T>& j) { return i->area() < j->area(); };

void doN(size_t N, auto foo) {
    for (size_t i = 0; i < N; i++)
        foo();
}

template <typename I>
static auto constexpr kPopulate = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(std::in_place_type<Circle>, *it++); });
    doN(N, [&] { v.emplace_back(std::in_place_type<Square>, *it++); });
    doN(N, [&] { v.emplace_back(std::in_place_type<Rectangle>, *it++, *it++); });
};

template <>
auto constexpr kPopulate<std::unique_ptr<VShape>> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(new VCircle{*it++}); });
    doN(N, [&] { v.emplace_back(new VSquare{*it++}); });
    doN(N, [&] { v.emplace_back(new VRectangle{*it++, *it++}); });
};

template <typename I>
auto constexpr kPopulate<std::unique_ptr<I>> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(new I{std::in_place_type<VCircle>, *it++}); });

    doN(N, [&] { v.emplace_back(new I{std::in_place_type<VSquare>, *it++}); });
    doN(N, [&] { v.emplace_back(new I{std::in_place_type<VRectangle>, *it++, *it++}); });
};

template <typename VecElem, auto Algo>
static void bench(benchmark::State& state) {
    size_t N = state.range(0);
    size_t totalShapes = 3 * N;

    auto randomDims = makeRandomDoubles(N * 5);
    std::vector<VecElem> shapes;
    shapes.reserve(totalShapes);
    benchmark::ClobberMemory();

    for (auto _ : state) {
        auto randomIt = randomDims.begin();
        shapes.clear();

        kPopulate<VecElem>(shapes, randomIt, N);

        benchmark::DoNotOptimize(Algo(shapes, kComparator<VecElem>));
    }
}

template <typename I>
static void instantiateAndMinShapes(benchmark::State& state) {
    bench<I, std::ranges::min_element>(state);
}

template <>
void instantiateAndMinShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>, std::ranges::min_element>(state);
}

template <typename I>
static void instantiateAndSortShapes(benchmark::State& state) {
    bench<I, std::ranges::sort>(state);
}

template <>
void instantiateAndSortShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>, std::ranges::sort>(state);
}

static constexpr size_t N = 1 << 17;
constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(instantiateAndMinShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeSharedDynamic>)->Apply(setRange);

BENCHMARK(instantiateAndSortShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<std::unique_ptr<WoidShapeShared>>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<std::unique_ptr<WoidShapeDedicated>>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeSharedDynamic>)->Apply(setRange);

BENCHMARK_MAIN();
