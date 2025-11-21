#define BOOST_TEST_MODULE CrossTuTest

#include "cross_tu_lib.hpp"
#include "woid.hpp"
#include <boost/hana.hpp>
#include <gtest/gtest.h>

namespace hana = boost::hana;

constexpr static int kInt = 42;

constexpr static auto SafeStorages = hana::
    tuple_t<AnyMoveOnlyCombined, AnyMoveOnlyDedicated, AnyCopyableCombined, AnyCopyableDedicated>;
constexpr static auto IntTypes = hana::tuple_t<int32_t, __int128>;

template <typename Storage_, typename Value_>
struct TestCase {
    using Storage = Storage_;
    using Value = Value_;
};

constexpr static auto TestCases
    = hana::transform(hana::cartesian_product(hana::make_tuple(SafeStorages, IntTypes)),
                      hana::fuse(hana::template_<TestCase>));

template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<testing::Types>))::type;

template <typename T>
struct SafeAnyCastTest : testing::Test {};

TYPED_TEST_SUITE(SafeAnyCastTest, AsTuple<TestCases>);

TYPED_TEST(SafeAnyCastTest, SafeAnyCastWorksWhenTypeMatches) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;

    auto any = mkAny<Storage, Value>(kInt);
    ASSERT_EQ(any_cast<Value>(any), kInt);
    ASSERT_EQ(any_cast<Value&>(any), kInt);
    ASSERT_EQ(any_cast<const Value&>(any), kInt);
    ASSERT_EQ(any_cast<Value&&>(std::move(any)), kInt);
}

#if defined(__cpp_exceptions)

TYPED_TEST(SafeAnyCastTest, SafeAnyCastThrowsWhenTypeMismatches) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;

    auto any = mkAny<Storage, Value>(kInt);
    EXPECT_THROW(any_cast<float>(any), woid::BadAnyCast);
}

#endif
