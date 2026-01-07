#include "woid.hpp"
#include <algorithm>
#include <benchmark/benchmark.h>
#include <boost/te.hpp>
#include <print>
#include <proxy/proxy.h>
#include <random>
#include <type_traits>
#include <variant>

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

template <bool IsTrivial>
struct Square {
    double side;

    Square(double s) : side(s) {}

    ~Square()
        requires(!IsTrivial) {}
    ~Square()
        requires(IsTrivial)
    = default;
    Square(Square&&) = default;
    Square(const Square&) = default;
    Square& operator=(Square&&) = default;
    Square& operator=(const Square&) = default;

    double area() const { return side * side; }
    double perimeter() const { return 4 * side; }
    void draw() const { std::println("Square(a={})", side); }
};

template <typename T>
static constexpr inline bool IsTriviallyRelocatable
    = std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>;

static_assert(IsTriviallyRelocatable<Square<true>>);
static_assert(!IsTriviallyRelocatable<Square<false>>);

template <bool IsTrivial>
struct Rectangle {
    double length;
    double width;

    Rectangle(double l, double w) : length(l), width(w) {}

    ~Rectangle()
        requires(!IsTrivial) {}
    ~Rectangle()
        requires(IsTrivial)
    = default;
    Rectangle(Rectangle&&) = default;
    Rectangle(const Rectangle&) = default;
    Rectangle& operator=(Rectangle&&) = default;
    Rectangle& operator=(const Rectangle&) = default;

    double area() const { return length * width; }
    double perimeter() const { return 2 * (length + width); }
    void draw() const { std::println("Rectangle(l={}, w={})", length, width); }
};

static_assert(IsTriviallyRelocatable<Rectangle<true>>);
static_assert(!IsTriviallyRelocatable<Rectangle<false>>);

template <bool IsTrivial>
struct Circle {
    double radius;

    Circle(double r) : radius(r) {}

    ~Circle()
        requires(!IsTrivial) {}
    ~Circle()
        requires(IsTrivial)
    = default;
    Circle(Circle&&) = default;
    Circle(const Circle&) = default;
    Circle& operator=(Circle&&) = default;
    Circle& operator=(const Circle&) = default;

    double area() const { return std::numbers::pi * radius * radius; }
    double perimeter() const { return 2 * std::numbers::pi * radius; }
    void draw() const { std::println("Circle(r={})", radius); }
};

static_assert(IsTriviallyRelocatable<Circle<true>>);
static_assert(!IsTriviallyRelocatable<Circle<false>>);

static_assert(alignof(Rectangle<true>) == alignof(void*));
static_assert(alignof(Rectangle<false>) == alignof(void*));

static constexpr int kRectangleSize = sizeof(Rectangle<true>);

// clang-format off
using Builder = woid::InterfaceBuilder
           ::WithStorage<woid::Any<kRectangleSize, woid::Copy::DISABLED>>
           ::Fun<"area", [](const auto& obj) -> double { return obj.area(); } >
           ::Method<"perimieter", double()const, []<typename T> {return &T::perimeter; } >
           ::Method<"draw", void()const, []<typename T> {return &T::draw; } > ;
// clang-format on

using SharedBase = Builder::WithSharedVTable::Build;

using DedicatedBase = Builder::WithDedicatedVTable::Build;

using DedicatedExceptionSafeBase = Builder::WithStorage<
    woid::Any<kRectangleSize, woid::Copy::DISABLED, woid::ExceptionGuarantee::STRONG>>::
    WithDedicatedVTable::Build;

using SharedTrivialBase = Builder::WithSharedVTable::WithStorage<TrivialAny<kRectangleSize>>::Build;

using DedicatedTrivialBase
    = Builder::WithDedicatedVTable::WithStorage<TrivialAny<kRectangleSize>>::Build;

struct WoidShapeShared : SharedBase {
    using SharedBase::SharedBase;
    double area() const { return call<"area">(); }
};

struct WoidShapeDedicated : DedicatedBase {
    using DedicatedBase::DedicatedBase;
    double area() const { return call<"area">(); }
};

struct WoidShapeDedicatedExceptionSafe : DedicatedExceptionSafeBase {
    using DedicatedExceptionSafeBase::DedicatedExceptionSafeBase;
    double area() const { return call<"area">(); }
};

struct WoidTrivialShapeShared : SharedTrivialBase {
    using SharedTrivialBase::SharedTrivialBase;
    double area() const { return call<"area">(); }
};

struct WoidTrivialShapeDedicated : DedicatedTrivialBase {
    using DedicatedTrivialBase::DedicatedTrivialBase;
    double area() const { return call<"area">(); }
};

using DynamicShardBase
    = Builder::WithSharedVTable::WithStorage<woid::DynamicAny<woid::Copy::DISABLED>>::Build;

struct WoidShapeSharedDynamic : DynamicShardBase {
    using DynamicShardBase::DynamicShardBase;
    double area() const { return call<"area">(); }
};

using NonTrivialShape = std::variant<Square<false>, Circle<false>, Rectangle<false>>;
using TrivialShape = std::variant<Square<true>, Circle<true>, Rectangle<true>>;

// clang-format off
template <typename V>
struct WoidSealedShape : woid::SealedInterfaceBuilder<V>
           ::template Fun<"area", [](const auto& obj) -> double { return obj.area(); } >
           ::Build {
    using WoidSealedShape::Self::Self;
    double area() const { return this->template call<"area">(); }
};
// clang-format on
using WoidTrivialSealedShape = WoidSealedShape<TrivialShape>;
using WoidNonTrivialSealedShape = WoidSealedShape<NonTrivialShape>;

namespace te = boost::te;

struct BoostTeShape final : te::poly<BoostTeShape, te::sbo_storage<kRectangleSize>> {
    using te::poly<BoostTeShape, te::sbo_storage<kRectangleSize>>::poly;

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
    // Why not kRectangleSize? -- we did that for boost::te and woid after all!
    // Proxy does SBO only for the types that are trivially movable and destructible,
    // which our shapes are (intentionally) not.
    // So this size is necessary and sufficient.
    static constexpr std::size_t max_size = sizeof(void*);
};

struct ProxyTrivialShapeFacade : pro::facade_builder
    ::add_convention<MemArea, double() const>
    ::add_convention<MemPerimeter, double() const>
    ::add_convention<MemDraw, void() const>
    ::support_copy<pro::constraint_level::none>
    ::support_relocation<pro::constraint_level::trivial>
    ::build {
    static constexpr std::size_t max_size = kRectangleSize;
};
// clang-format on

using ProxyShape = pro::proxy<ProxyShapeFacade>;
using ProxyTrivialShape = pro::proxy<ProxyTrivialShapeFacade>;

auto makeRandomDoubles(int N) {
    std::vector<double> result(N);

    std::mt19937 gen(1234);
    std::uniform_real_distribution<> distrib(0.0, 1.0);

    std::ranges::generate(result, [&]() { return distrib(gen); });
    return result;
}

template <typename T>
constexpr auto inline IsProxy
    = std::is_same_v<T, ProxyShape> || std::is_same_v<T, ProxyTrivialShape>;

template <typename I>
constexpr static auto kComparator = [](const I& i, const I& j) { return i.area() < j.area(); };

template <typename T>
    requires(IsProxy<T>)
constexpr auto kComparator<T> = [](const T& i, const T& j) { return i->area() < j->area(); };

template <typename T>
constexpr static auto kComparator<std::unique_ptr<T>> =
    [](const std::unique_ptr<T>& i, const std::unique_ptr<T>& j) { return i->area() < j->area(); };

void doN(size_t N, auto foo) {
    for (size_t i = 0; i < N; i++)
        foo();
}

template <typename I, bool IsTrivial>
static auto constexpr kPopulate = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(std::in_place_type<Circle<IsTrivial>>, *it++); });
    doN(N, [&] { v.emplace_back(std::in_place_type<Square<IsTrivial>>, *it++); });
    doN(N, [&] { v.emplace_back(std::in_place_type<Rectangle<IsTrivial>>, *it++, *it++); });
};

template <bool IsTrivial>
auto constexpr kPopulate<BoostTeShape, IsTrivial> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(Circle<IsTrivial>{*it++}); });
    doN(N, [&] { v.emplace_back(Square<IsTrivial>{*it++}); });
    doN(N, [&] { v.emplace_back(Rectangle<IsTrivial>{*it++, *it++}); });
};

template <typename Proxy, bool IsTrivial>
    requires(IsProxy<Proxy>)
auto constexpr kPopulate<Proxy, IsTrivial> = [](auto& v, auto it, size_t N) {
    using Facade = typename Proxy::facade_type;
    doN(N, [&] { v.push_back(pro::make_proxy<Facade, Circle<IsTrivial>>(*it++)); });
    doN(N, [&] { v.push_back(pro::make_proxy<Facade, Square<IsTrivial>>(*it++)); });
    doN(N, [&] { v.push_back(pro::make_proxy<Facade, Rectangle<IsTrivial>>(*it++, *it++)); });
};

template <bool IsTrivial>
auto constexpr kPopulate<std::unique_ptr<VShape>, IsTrivial> = [](auto& v, auto it, size_t N) {
    doN(N, [&] { v.emplace_back(new VCircle{*it++}); });
    doN(N, [&] { v.emplace_back(new VSquare{*it++}); });
    doN(N, [&] { v.emplace_back(new VRectangle{*it++, *it++}); });
};

template <typename VecElem, bool IsTrivial, auto Algo>
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

        kPopulate<VecElem, IsTrivial>(shapes, randomIt, N);

        benchmark::DoNotOptimize(Algo(shapes, kComparator<VecElem>));
    }
}

template <typename I>
static void instantiateAndMinShapes(benchmark::State& state) {
    bench<I, false, std::ranges::min_element>(state);
}

template <>
void instantiateAndMinShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>, false, std::ranges::min_element>(state);
}

template <typename I>
static void instantiateAndSortShapes(benchmark::State& state) {
    bench<I, false, std::ranges::sort>(state);
}

template <>
void instantiateAndSortShapes<VShape>(benchmark::State& state) {
    bench<std::unique_ptr<VShape>, false, std::ranges::sort>(state);
}

static constexpr size_t N = 1 << 17;
constexpr auto setRange
    = [](auto* bench) -> void { bench->MinWarmUpTime(0.1)->RangeMultiplier(2)->Range(1, N); };

BENCHMARK(instantiateAndMinShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeDedicatedExceptionSafe>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidShapeSharedDynamic>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<WoidNonTrivialSealedShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<BoostTeShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinShapes<ProxyShape>)->Apply(setRange);

BENCHMARK(instantiateAndSortShapes<VShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeDedicatedExceptionSafe>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidShapeSharedDynamic>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<WoidNonTrivialSealedShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<BoostTeShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortShapes<ProxyShape>)->Apply(setRange);

template <typename I>
static void instantiateAndMinTrivialShapes(benchmark::State& state) {
    bench<I, true, std::ranges::min_element>(state);
}

template <typename I>
static void instantiateAndSortTrivialShapes(benchmark::State& state) {
    bench<I, true, std::ranges::sort>(state);
}

BENCHMARK(instantiateAndMinTrivialShapes<WoidTrivialShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndMinTrivialShapes<WoidTrivialShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndMinTrivialShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndMinTrivialShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndMinTrivialShapes<WoidTrivialSealedShape>)->Apply(setRange);
BENCHMARK(instantiateAndMinTrivialShapes<ProxyTrivialShape>)->Apply(setRange);

BENCHMARK(instantiateAndSortTrivialShapes<WoidTrivialShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortTrivialShapes<WoidTrivialShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortTrivialShapes<WoidShapeShared>)->Apply(setRange);
BENCHMARK(instantiateAndSortTrivialShapes<WoidShapeDedicated>)->Apply(setRange);
BENCHMARK(instantiateAndSortTrivialShapes<WoidTrivialSealedShape>)->Apply(setRange);
BENCHMARK(instantiateAndSortTrivialShapes<ProxyTrivialShape>)->Apply(setRange);

BENCHMARK_MAIN();
