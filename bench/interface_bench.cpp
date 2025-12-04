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
           ::WithStorage<woid::Any<sizeof(Rectangle)>>
           ::Method<"area", double()const, []<typename T> {return &T::area; } >
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

struct WoidSharedDynamic : DedicatedShardBase {
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

template <typename T>
struct Test;

template <typename VecElem, auto Populate, auto Algo, auto Proj>
static void bench(benchmark::State& state) {
    size_t N = state.range(0);
    size_t totalShapes = 3 * N;

    auto randomDims = makeRandomDoubles(N * 5);

    benchmark::ClobberMemory();

    for (auto _ : state) {
        std::vector<VecElem> shapes;
        shapes.reserve(totalShapes);
        auto randomIt = randomDims.begin();

        for (size_t i = 0; i < N; ++i) {
            Populate(shapes, randomIt);
        }

        benchmark::DoNotOptimize(Algo(shapes, std::less{}, Proj));
    }
}

static auto constexpr populateWithValues = [](auto& v, auto it) {
    v.emplace_back(std::in_place_type<Circle>, *it++);
    v.emplace_back(std::in_place_type<Square>, *it++);
    v.emplace_back(std::in_place_type<Rectangle>, *it++, *it++);
};

static auto constexpr populateWithPtrs = [](auto& v, auto it) {
    v.emplace_back(new VCircle{*it++});
    v.emplace_back(new VSquare{*it++});
    v.emplace_back(new VRectangle{*it++, *it++});
};

template <typename I>
static void instantiateAndMinShapes(benchmark::State& state) {
    bench<I, populateWithValues, std::ranges::min_element, &I::area>(state);
}

template <>
void instantiateAndMinShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>,
          populateWithPtrs,
          std::ranges::min_element,
          [](const std::unique_ptr<VShape>& a) { return a->area(); }>(state);
}

template <typename I>
static auto constexpr populateWithInterfacePtrs = [](auto& v, auto it) {
    v.push_back(new I{std::in_place_type<VCircle>, *it++});
    v.push_back(new I{std::in_place_type<VSquare>, *it++});
    v.push_back(new I{std::in_place_type<VRectangle>, *it++, *it++});
};

template <typename I>
    requires(!std::is_pointer_v<I>) static void instantiateAndSortShapes(benchmark::State& state) {
    bench<I, populateWithValues, std::ranges::sort, &I::area>(state);
}

template <typename IPtr>
    requires(std::is_pointer_v<IPtr>)
static void instantiateAndSortShapes(benchmark::State& state) {
    using I = std::remove_pointer_t<IPtr>;
    bench<IPtr, populateWithInterfacePtrs<I>, std::ranges::sort, [](const I* i) {
        return i->area();
    }>(state);
}

template <>
void instantiateAndSortShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>,
          populateWithPtrs,
          std::ranges::sort,
          [](const std::unique_ptr<VShape>& a) { return a->area(); }>(state);
}

static constexpr size_t N = 1 << 17;
constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(instantiateAndMinShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidSharedDynamic>)->Apply(setRange);

BENCHMARK(instantiateAndSortShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeShared*>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicated*>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidSharedDynamic>)->Apply(setRange);

BENCHMARK_MAIN();
