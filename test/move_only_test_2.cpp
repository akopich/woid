#include "move_only_test_base.hpp"

inline static constexpr auto kShard = hana::size_c<2>;

INSTANTIATE_TYPED_TEST_SUITE_P(Shard2,
                               MoveTestCase,
                               AsTuple<shard(MoveTestCases, kShard, kMoveTestShard)>);

INSTANTIATE_TYPED_TEST_SUITE_P(Shard2,
                               MoveTestCaseWithBigObject,
                               AsTuple<shard(MoveTestCasesWithBigObject, kShard, kMoveTestShard)>);
