#define BOOST_TEST_MODULE CrossTuTest

#include "cross_tu_lib.hpp"
#include "woid.hpp"
#include <gtest/gtest.h>

TEST(CrossTu, SafeAnyCastWorksWhenTypeMatches) {
    auto any = mkAny();
    ASSERT_EQ(any_cast<int>(any), 42);
}

TEST(CrossTu, SafeAnyCastWorksWhenTypeMatchesBig) {
    auto any = mkBigAny();
    ASSERT_EQ(any_cast<__int128>(any), __int128{42});
}

#if defined(__cpp_exceptions)

TEST(CrossTu, SafeAnyCastWorksWhenTypeMismatches) {
    auto any = mkAny();
    EXPECT_THROW(any_cast<float>(any), woid::BadAnyCast);
}

TEST(CrossTu, SafeAnyCastWorksWhenTypeMismatchesBig) {
    auto any = mkBigAny();
    EXPECT_THROW(any_cast<float>(any), woid::BadAnyCast);
}

#endif
