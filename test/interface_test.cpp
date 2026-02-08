
#define BOOST_TEST_MODULE InterfaceTest

#include "woid.hpp"

#include <boost/hana.hpp>
#include <boost/hana/fwd/filter.hpp>
#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>
#include <variant>

using namespace woid;
namespace hana = boost::hana;

struct C {
    static inline size_t cnt = 0;
    void set(size_t i) { cnt = i; }
    size_t get() const { return cnt; }
    void inc() { cnt++; }
    void twice() const { cnt *= 2; }
};

struct CC {
    static inline size_t cnt = 0;
    void set(size_t i) { cnt = i; }
    size_t get() const { return cnt; }
    void inc() { cnt += 2; }
    void twice() { cnt *= 4; }
};

using Variant = std::variant<C, CC>;

// clang-format off
using InterfaceViaMethods = InterfaceBuilder
            ::Method<"set", void(int), []<typename T> { return &T::set; }>
            ::Method<"get", size_t(void) const, []<typename T> { return &T::get; }>
            ::Method<"inc", void(void), []<typename T> { return &T::inc; }>
            ::Method<"twice", void(void), []<typename T> { return &T::twice; }>;

using InterfaceViaFuns = InterfaceBuilder
            ::Fun<"set",   [](auto& obj, int i) -> void { obj.set(i); }>
            ::Fun<"get",   [](const auto& obj) -> size_t { return obj.get(); }>
            ::Fun<"inc",   [](auto& obj) -> void { obj.inc(); }>
            ::Fun<"twice", [](auto& obj) -> void { obj.twice();  }>;

struct SealedIncAndTwice : SealedInterfaceBuilder<Variant>
            ::Fun<"set",   [](auto& obj, int i) -> void { obj.set(i); }>
            ::Fun<"get",   [](const auto& obj) -> size_t { return obj.get(); }>
            ::Fun<"inc",   [](auto& obj) -> void { obj.inc(); }>
            ::Fun<"twice", [](auto& obj) -> void { obj.twice();  }>
            ::Build {
    void set(size_t i) { call<"set">(i); }
    size_t get() const { return call<"get">(); }
    void inc() { call<"inc">(); }
    void twice() { call<"twice">(); }
};

template <typename Interface, VTableOwnership O, typename S>
struct IncAndTwice
      : Interface
            ::template With<O>
            ::template WithStorage<S>
            ::Build {
    void set(size_t i) { this->template call<"set">(i); }
    size_t get() const { return this->template call<"get">(); }
    void inc() { this->template call<"inc">(); }
    void twice() { this->template call<"twice">(); }
};
// clang-format on

constexpr auto Storages = hana::tuple_t<woid::Any<8, Copy::ENABLED>,
                                        woid::Any<8, Copy::DISABLED>,
                                        DynamicAny<Copy::ENABLED>,
                                        DynamicAny<Copy::DISABLED>,
                                        std::any>;
constexpr auto VTableOwnerships
    = hana::tuple_c<VTableOwnership, VTableOwnership::DEDICATED, VTableOwnership::SHARED>;
constexpr auto Interfaces = hana::tuple_t<InterfaceViaFuns, InterfaceViaMethods>;

constexpr auto TestCases
    = hana::concat(hana::transform(hana::cartesian_product(
                                       hana::make_tuple(Interfaces, VTableOwnerships, Storages)),
                                   hana::fuse([](auto i, auto v, auto s) {
                                       return hana::type_c<IncAndTwice<typename decltype(i)::type,
                                                                       decltype(v)::value,
                                                                       typename decltype(s)::type>>;
                                   })),
                   hana::tuple_t<SealedIncAndTwice>);

template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<testing::Types>))::type;

template <typename T>
struct InterfaceTest : testing::Test {
    ~InterfaceTest() {
        C::cnt = 0;
        CC::cnt = 0;
    }
};

TYPED_TEST_SUITE(InterfaceTest, AsTuple<TestCases>);

TYPED_TEST(InterfaceTest, canCallMethods) {
    using I = TypeParam;

    I it{C{}};
    it.set(3);
    it.inc();
    it.twice();
    it.twice();

    ASSERT_EQ(static_cast<const I&>(it).get(), 16);
}

TYPED_TEST(InterfaceTest, andPutThemAllInVector) {
    using I = TypeParam;

    std::vector<I> v;

    v.emplace_back(C{});
    v.emplace_back(std::in_place_type<CC>);

    for (auto& x : v) {
        x.inc();
        x.inc();
        x.inc();
        x.twice();
    }

    ASSERT_EQ(C::cnt, 6);
    ASSERT_EQ(CC::cnt, 24);
}

constexpr auto VTableOwnderships
    = hana::tuple_c<VTableOwnership, VTableOwnership::SHARED, VTableOwnership::DEDICATED>;

template <typename T>
struct VTableParameterizedTest : testing::Test {
    ~VTableParameterizedTest() {
        C::cnt = 0;
        CC::cnt = 0;
    }
};

TYPED_TEST_SUITE(VTableParameterizedTest, AsTuple<VTableOwnderships>);

TYPED_TEST(VTableParameterizedTest, worksWithRefToo) {
    static constexpr auto O = TypeParam::value;
    C c{};
    CC cc{};

    std::vector<IncAndTwice<InterfaceViaFuns, O, Ref>> v;

    v.emplace_back(c);
    v.emplace_back(cc);

    for (auto x : v) {
        x.inc();
        x.inc();
        x.inc();
        x.twice();
    }

    ASSERT_EQ(C::cnt, 6);
    ASSERT_EQ(CC::cnt, 24);
}

template <template <typename> typename TraitV, typename X, typename Y>
static void checkTraitSame() {
    static_assert(TraitV<X>::value == TraitV<Y>::value);
}

TYPED_TEST(InterfaceTest, storagePropertiesArePropagatedToTheInterface) {
    using Storage = TypeParam::Storage;
    using I = TypeParam;
    checkTraitSame<std::is_copy_constructible, Storage, I>();
    checkTraitSame<std::is_copy_assignable, Storage, I>();
    checkTraitSame<std::is_move_constructible, Storage, I>();
    checkTraitSame<std::is_move_assignable, Storage, I>();
    checkTraitSame<std::is_nothrow_move_constructible, Storage, I>();
    checkTraitSame<std::is_nothrow_move_assignable, Storage, I>();
}

struct G {
    int get() { return 5; }
    int get() const { return 45; }

    bool isInt(int) { return true; }
    bool isInt(float) { return false; }

    int addAll(int i, const int j, const int& k, int& l, int&& m) { return i + j + k + l + m; }
};

// clang-format off
template <VTableOwnership O>
struct GI
      : InterfaceBuilder
            ::With<O>
            ::template Method<"addAll", int(int, const int, const int&, int&, int&&), []<typename T> { return &T::addAll; }>
            ::template Fun<"addAllFun", [](auto& obj, int i, const int j, const int& k, int& l, int&& m) -> int {
                return obj.addAll(i, j, k, l, std::move(m));
            }>
            ::template Method<"isInt", bool(int), []<typename T> { return static_cast<bool (T::*)(int)>(&T::isInt); }>
            ::template Method<"isInt", bool(float), []<typename T> { return static_cast<bool (T::*)(float)>(&T::isInt); }>
            ::template Method<"get", int(void), []<typename T> { return static_cast<int (T::*)()>(&T::get); }>
            ::template Method<"get", int(void) const, []<typename T> { return static_cast<int (T::*)() const>(&T::get); }>
            ::template Fun<"getFun", [](auto& obj) -> int { return obj.get(); } >
            ::template Fun<"getFun", [](const auto& obj) -> int { return obj.get(); } >
            ::template Fun<"isIntFun", [](auto& obj, int i) -> bool {
               return obj.isInt(i);
            }>
            ::template Fun<"isIntFun", [](auto& obj, float f) -> bool {
               return obj.isInt(f);
            }>
            ::Build {};
// clang-format on

TYPED_TEST(VTableParameterizedTest, constOverload) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};
    ASSERT_EQ(g.template call<"get">(), 5);
    ASSERT_EQ(g.template call<"getFun">(), 5);

    const GI cg = g;
    ASSERT_EQ(cg.template call<"get">(), 45);
    ASSERT_EQ(cg.template call<"getFun">(), 45);
}

TYPED_TEST(VTableParameterizedTest, argTypeOverload) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};
    ASSERT_TRUE(g.template call<"isInt">(int{0}));
    ASSERT_FALSE(g.template call<"isInt">(float{0}));
    ASSERT_TRUE(g.template call<"isIntFun">(int{0}));
    ASSERT_FALSE(g.template call<"isIntFun">(float{0}));
}

TYPED_TEST(VTableParameterizedTest, propagatesConstRef) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};

    int i = 4;
    int j = 5;
    ASSERT_EQ(g.template call<"addAll">(1, 2, 3, i, std::move(j)), 15);
    ASSERT_EQ(g.template call<"addAllFun">(1, 2, 3, i, std::move(j)), 15);
    int one = 1;
    int two = 2;
    int three = 3;
    ASSERT_EQ(g.template call<"addAll">(one, two, three, i, std::move(j)), 15);
    ASSERT_EQ(g.template call<"addAllFun">(one, two, three, i, std::move(j)), 15);
}
