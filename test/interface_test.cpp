
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

using Storage = woid::Any<8>;

struct IncAndTwice : Interface<Storage,
                               Method<"set", void(int), []<typename T> { return &T::set; }>,
                               Method<"get", size_t(), []<typename T> { return &T::get; }>,
                               Method<"inc", void(void), []<typename T> { return &T::inc; }>,
                               Method<"twice", void(void), []<typename T> { return &T::twice; }>> {

    void set(size_t i) { call<"set", size_t>(i); }
    size_t get() { return call<"get">(); }
    void inc() { call<"inc">(); }
    void twice() { call<"twice">(); }
};

TEST(InterfaceTest, canCallNoArgNoConstMethods) {
    C::cnt = 0;

    IncAndTwice it{C{}};
    it.set(3);
    it.inc();
    it.twice();
    it.twice();

    ASSERT_EQ(it.get(), 16);
}

TEST(InterfaceTest, anPutThemAllInVector) {
    C::cnt = 0;

    std::vector<IncAndTwice> v;

    v.emplace_back(C{});
    v.emplace_back(CC{});

    for (auto x : v) {
        x.inc();
        x.inc();
        x.inc();
        x.twice();
    }

    ASSERT_EQ(C::cnt, 6);
    ASSERT_EQ(CC::cnt, 24);
}
