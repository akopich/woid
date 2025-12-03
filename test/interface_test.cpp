
#define BOOST_TEST_MODULE InterfaceTest

#include "woid.hpp"

#include <boost/hana.hpp>
#include <boost/hana/fwd/filter.hpp>
#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>

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

// clang-format off
template <VTableOwnership O, typename S>
struct IncAndTwice
      : InterfaceBuilder
            ::With<O>
            ::template WithStorage<S>
            ::template Method<"set", void(int), []<typename T> { return &T::set; }>
            ::template Method<"get", size_t(void) const, []<typename T> { return &T::get; }>
            ::template Method<"inc", void(void), []<typename T> { return &T::inc; }>
            ::template Method<"twice", void(void), []<typename T> { return &T::twice; }>
            ::Build {
    void set(size_t i) { this->template call<"set">(i); }
    size_t get() const { return this->template call<"get">(); }
    void inc() { this->template call<"inc">(); }
    void twice() { this->template call<"twice">(); }
};
// clang-format on

using TestCases
    = testing::Types<IncAndTwice<VTableOwnership::DEDICATED, woid::Any<8, Copy::ENABLED>>,
                     IncAndTwice<VTableOwnership::DEDICATED, woid::Any<8, Copy::DISABLED>>,
                     IncAndTwice<VTableOwnership::SHARED, woid::Any<8, Copy::ENABLED>>,
                     IncAndTwice<VTableOwnership::SHARED, woid::Any<8, Copy::DISABLED>>,
                     IncAndTwice<VTableOwnership::SHARED, detail::DynamicStorage<>>,
                     IncAndTwice<VTableOwnership::DEDICATED, detail::DynamicStorage<>>

                     >;

template <typename T>
struct InterfaceTest : testing::Test {
    ~InterfaceTest() {
        C::cnt = 0;
        CC::cnt = 0;
    }
};

TYPED_TEST_SUITE(InterfaceTest, TestCases);

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
    v.emplace_back(CC{});

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
template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<testing::Types>))::type;

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

    std::vector<IncAndTwice<O, Ref>> v;

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
            ::template Method<"isInt", bool(int), []<typename T> { return static_cast<bool (T::*)(int)>(&T::isInt); }>
            ::template Method<"isInt", bool(float), []<typename T> { return static_cast<bool (T::*)(float)>(&T::isInt); }>
            ::template Method<"get", int(void), []<typename T> { return static_cast<int (T::*)()>(&T::get); }>
            ::template Method<"get", int(void) const, []<typename T> { return static_cast<int (T::*)() const>(&T::get); }>
            ::Build {};
// clang-format on

TYPED_TEST(VTableParameterizedTest, constOverload) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};
    ASSERT_EQ(g.template call<"get">(), 5);

    const GI cg = g;
    ASSERT_EQ(cg.template call<"get">(), 45);
}

TYPED_TEST(VTableParameterizedTest, argTypeOverload) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};
    ASSERT_TRUE(g.template call<"isInt">(int{0}));
    ASSERT_FALSE(g.template call<"isInt">(float{0}));
}

TYPED_TEST(VTableParameterizedTest, propagatesConstRef) {
    static constexpr auto O = TypeParam::value;
    GI<O> g{G{}};

    int i = 4;
    int j = 5;
    ASSERT_EQ(g.template call<"addAll">(1, 2, 3, i, std::move(j)), 15);
}
