#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <boost/te.hpp>
#include <print>
#include <proxy/proxy.h>
#include <random>

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

    Square(double s) : side(s) {}

    ~Square() {}
    Square(Square&&) = default;
    Square(const Square&) = default;
    Square& operator=(Square&&) = default;
    Square& operator=(const Square&) = default;

    double area() const { return side * side; }
    double perimeter() const { return 4 * side; }
    void draw() const { std::println("Square(a={})", side); }
};

struct Rectangle {
    double length;
    double width;

    Rectangle(double l, double w) : length(l), width(w) {}

    ~Rectangle() {}
    Rectangle(Rectangle&&) = default;
    Rectangle(const Rectangle&) = default;
    Rectangle& operator=(Rectangle&&) = default;
    Rectangle& operator=(const Rectangle&) = default;

    double area() const { return length * width; }
    double perimeter() const { return 2 * (length + width); }
    void draw() const { std::println("Rectangle(l={}, w={})", length, width); }
};

struct Circle {
    double radius;

    Circle(double r) : radius(r) {}

    ~Circle() {}
    Circle(Circle&&) = default;
    Circle(const Circle&) = default;
    Circle& operator=(Circle&&) = default;
    Circle& operator=(const Circle&) = default;

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

using DedicatedShardBase = Builder::WithSharedVTable::WithStorage<woid::DynamicStorage<>>::Build;

struct WoidShapeSharedDynamic : DedicatedShardBase {
    using DedicatedShardBase::DedicatedShardBase;
    double area() const { return call<"area">(); }
};

namespace te = boost::te;

struct BoostTeShape final : te::poly<BoostTeShape, te::sbo_storage<sizeof(Rectangle)>> {
    using te::poly<BoostTeShape, te::sbo_storage<sizeof(Rectangle)>>::poly;

    double area() const {
        return te::call<double>([](auto const& self) -> double { return self.area(); }, *this);
    }
};

PRO_DEF_MEM_DISPATCH(MemArea, area);
PRO_DEF_MEM_DISPATCH(MemPerimeter, perimeter);
PRO_DEF_MEM_DISPATCH(MemDraw, draw);

// clang-format off
struct ProxyShapeFacade : pro::facade_builder
    ::add_convention<MemArea, double() const>
    ::add_convention<MemPerimeter, double() const>
    ::add_convention<MemDraw, void() const>
    ::support_copy<pro::constraint_level::none>
    ::build {
    // Why not sizeof(Rectangle)? -- we did that for boost::te and woid after all!
    // Proxy does SBO only for the types that are trivially movable and destructible,
    // which our shapes are (intentionally) not.
    // So this size is necessary and sufficient.
    static constexpr std::size_t max_size = sizeof(void*);
};
// clang-format on

using ProxyShape = pro::proxy<ProxyShapeFacade>;

auto makeRandomDoubles(int N) {
    std::vector<double> result(N);

    std::mt19937 gen(1234);
    std::uniform_real_distribution<> distrib(0.0, 1.0);

    std::ranges::generate(result, [&]() { return distrib(gen); });
    return result;
}

template <typename I>
constexpr static auto kComparator = [](const I& i, const I& j) { return i.area() < j.area(); };

template <>
constexpr auto kComparator<ProxyShape>
    = [](const ProxyShape& i, const ProxyShape& j) { return i->area() < j->area(); };

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
auto constexpr kPopulate<BoostTeShape> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(Circle{*it++}); });
    doN(N, [&] { v.emplace_back(Square{*it++}); });
    doN(N, [&] { v.emplace_back(Rectangle{*it++, *it++}); });
};

template <>
auto constexpr kPopulate<ProxyShape> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.push_back(pro::make_proxy<ProxyShapeFacade, Circle>(*it++)); });
    doN(N, [&] { v.push_back(pro::make_proxy<ProxyShapeFacade, Square>(*it++)); });
    doN(N, [&] { v.push_back(pro::make_proxy<ProxyShapeFacade, Rectangle>(*it++, *it++)); });
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
BENCHMARK(instantiateAndMinShapes<BoostTeShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<ProxyShape>)->Apply(setRange);

BENCHMARK(instantiateAndSortShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeSharedDynamic>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<BoostTeShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<ProxyShape>)->Apply(setRange);

BENCHMARK_MAIN();
