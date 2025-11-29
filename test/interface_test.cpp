
#define BOOST_TEST_MODULE InterfaceTest

#include "woid.hpp"

#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>

using namespace woid;

struct C {
    static inline size_t cnt = 0;
    void set(size_t i) { cnt = i; }
    size_t get() { return cnt; }
    void inc() { cnt++; }
    void twice() { cnt *= 2; }
};

struct CC {
    static inline size_t cnt = 0;
    void set(size_t i) { cnt = i; }
    size_t get() { return cnt; }
    void inc() { cnt += 2; }
    void twice() { cnt *= 4; }
};

using Storages = testing::Types<woid::Any<8, Copy::ENABLED>, woid::Any<8, Copy::DISABLED>>;

template <typename S>
struct IncAndTwice : Interface<S,
                               Method<"set", void(int), []<typename T> { return &T::set; }>,
                               Method<"get", size_t(), []<typename T> { return &T::get; }>,
                               Method<"inc", void(void), []<typename T> { return &T::inc; }>,
                               Method<"twice", void(void), []<typename T> { return &T::twice; }>> {

    void set(size_t i) { this->template call<"set", size_t>(i); }
    size_t get() { return this->template call<"get">(); }
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

TYPED_TEST(InterfaceTest, canCallNoArgNoConstMethods) {
    using Storage = TypeParam;

    IncAndTwice<Storage> it{C{}};
    it.set(3);
    it.inc();
    it.twice();
    it.twice();

    ASSERT_EQ(it.get(), 16);
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
