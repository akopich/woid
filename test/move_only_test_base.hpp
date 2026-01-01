#include "storage_test_setup.hpp"

inline static constexpr auto kMoveTestShard = hana::size_c<4>;

template <typename T>
struct MoveTestCase : BaseTestCase<T> {};

template <typename T>
struct MoveTestCaseWithBigObject : testing::Test, AlternativeAllocatorResetter {};

TYPED_TEST_SUITE_P(MoveTestCase);

TYPED_TEST_P(MoveTestCase, canInstantiateFromRefRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Value v{kInt};
    Storage storage(std::move(v));
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
}

TYPED_TEST_P(MoveTestCase, canInstantiateInPlaceAndMove) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(std::in_place_type<Value>, kInt);
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
    Storage otherStorage = std::move(storage);
    ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
}

TYPED_TEST_P(MoveTestCase, canInstantiateAndMove) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage otherStorage = std::move(storage);
    ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
}

TYPED_TEST_P(MoveTestCase, canSelfMoveAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    SUPPRESS_SELF_MOVE_START
    storage = std::move(storage);
    SUPPRESS_SELF_MOVE_END
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
}

TYPED_TEST_P(MoveTestCase, canMoveAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage otherStorage(Value{42});
    otherStorage = std::move(storage);
    ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
}

TYPED_TEST_P(MoveTestCase, canSwap) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage otherStorage(Value{42});
    std::swap(storage, otherStorage);
    ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
    ASSERT_EQ(any_cast<Value&>(storage).i, 42);
}

TYPED_TEST_P(MoveTestCase, canSwapHeterogeneously) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage otherStorage(int{42});

    std::swap(storage, otherStorage);
    ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
    ASSERT_EQ(any_cast<int>(storage), 42);

    std::swap(storage, otherStorage);
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
    ASSERT_EQ(any_cast<int>(otherStorage), 42);
}

TYPED_TEST_P(MoveTestCase, canGetByRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    auto& valueRef = any_cast<Value&>(storage);
    ASSERT_EQ(valueRef.i, kInt);
    valueRef.i++;
    ASSERT_EQ(valueRef.i, kInt + 1);
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt + 1);
}

TYPED_TEST_P(MoveTestCase, canGetByRefRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    auto value = any_cast<Value&&>(std::move(storage));
    ASSERT_EQ(value.i, kInt);
}

TYPED_TEST_P(MoveTestCase, canMoveAssignToMovedFrom) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage{Value{42}};
    Storage unused{std::move(storage)};
    storage = Storage{Value{kInt}};
    auto value = any_cast<Value&&>(std::move(storage));
    ASSERT_EQ(value.i, kInt);
}

TYPED_TEST_P(MoveTestCase, canGetConstRef) {
    using Storage = TypeParam::Storage;
    const Storage s{kInt};
    ASSERT_EQ(any_cast<const int&>(s), kInt);
}

REGISTER_TYPED_TEST_SUITE_P(MoveTestCase,
                            canInstantiateFromRefRef,
                            canInstantiateInPlaceAndMove,
                            canInstantiateAndMove,
                            canSelfMoveAssign,
                            canMoveAssign,
                            canSwap,
                            canSwapHeterogeneously,
                            canGetByRef,
                            canGetByRefRef,
                            canMoveAssignToMovedFrom,
                            canGetConstRef);

TYPED_TEST_SUITE_P(MoveTestCaseWithBigObject);

TYPED_TEST_P(MoveTestCaseWithBigObject, canInstantiateFromPtr) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    using Alloc = Storage::Alloc;
    auto* ptr = Alloc::template make<Value>(kInt);
    Storage storage(kTransferOwnership, ptr);
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
}

REGISTER_TYPED_TEST_SUITE_P(MoveTestCaseWithBigObject, canInstantiateFromPtr);
