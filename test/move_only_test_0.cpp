#include "move_only_test_base.hpp"

inline static constexpr auto kShard = hana::size_c<0>;

INSTANTIATE_TYPED_TEST_SUITE_P(Shard0,
                               MoveTestCase,
                               AsTuple<shard(MoveTestCases, kShard, kMoveTestShard)>);

INSTANTIATE_TYPED_TEST_SUITE_P(Shard0,
                               MoveTestCaseWithBigObject,
                               AsTuple<shard(MoveTestCasesWithBigObject, kShard, kMoveTestShard)>);
