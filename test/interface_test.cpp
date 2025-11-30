
#define BOOST_TEST_MODULE InterfaceTest

#include "woid.hpp"

#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>

using namespace woid;

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

using Storages = testing::Types<woid::Any<8, Copy::ENABLED>, woid::Any<8, Copy::DISABLED>>;

template <typename S>
struct IncAndTwice
      : Interface<S,
                  Method<"set", void(int), []<typename T> { return &T::set; }>,
                  Method<"get", size_t(void) const, []<typename T> { return &T::get; }>,
                  Method<"inc", void(void), []<typename T> { return &T::inc; }>,
                  Method<"twice", void(void), []<typename T> { return &T::twice; }>> {

    void set(size_t i) { this->template call<"set">(i); }
    size_t get() const { return this->template call<"get">(); }
    void inc() { this->template call<"inc">(); }
    void twice() { this->template call<"twice">(); }
};

template <typename T>
struct InterfaceTest : testing::Test {
    ~InterfaceTest() {
        C::cnt = 0;
        CC::cnt = 0;
    }
};

TYPED_TEST_SUITE(InterfaceTest, Storages);

TYPED_TEST(InterfaceTest, canCallMethods) {
    using Storage = TypeParam;

    IncAndTwice<Storage> it{C{}};
    it.set(3);
    it.inc();
    it.twice();
    it.twice();

    ASSERT_EQ(static_cast<const IncAndTwice<Storage>&>(it).get(), 16);
}

TYPED_TEST(InterfaceTest, andPutThemAllInVector) {
    using Storage = TypeParam;

    std::vector<IncAndTwice<Storage>> v;

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

TEST(InterfaceRefTest, worksWithRefToo) {
    C c{};
    CC cc{};

    std::vector<IncAndTwice<Ref>> v;

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
    using Storage = TypeParam;
    using I = IncAndTwice<Storage>;
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

struct GI
      : Interface<
            Any<8>,
            Method<"addAll",
                   int(int, const int, const int&, int&, int&&),
                   []<typename T> { return &T::addAll; }>,
            Method<"isInt",
                   bool(int),
                   []<typename T> { return static_cast<bool (T::*)(int)>(&T::isInt); }>,
            Method<"isInt",
                   bool(float),
                   []<typename T> { return static_cast<bool (T::*)(float)>(&T::isInt); }>,
            Method<"get", int(void), []<typename T> { return static_cast<int (T::*)()>(&T::get); }>,
            Method<"get", int(void) const, []<typename T> {
                return static_cast<int (T::*)() const>(&T::get);
            }>> {};

TEST(Overloads, constOverload) {
    GI g{G{}};
    ASSERT_EQ(g.call<"get">(), 5);

    const GI cg = g;
    ASSERT_EQ(cg.call<"get">(), 45);
}

TEST(Overloads, argTypeOverload) {
    GI g{G{}};
    ASSERT_TRUE(g.call<"isInt">(int{0}));
    ASSERT_FALSE(g.call<"isInt">(float{0}));
}

TEST(Overloads, propagatesConstRef) {
    GI g{G{}};

    int i = 4;
    int j = 5;
    ASSERT_EQ(g.call<"addAll">(1, 2, 3, i, std::move(j)), 15);
}
