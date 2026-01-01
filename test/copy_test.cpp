#define BOOST_TEST_MODULE AnyTest

#include "storage_test_setup.hpp"

template <typename T>
struct CopyTypesTestCase : BaseTestCase<T> {};
TYPED_TEST_SUITE(CopyTypesTestCase, AsTuple<CopyTypesTestCases>);

template <typename T>
struct StorageType : testing::Test, AlternativeAllocatorResetter {};
TYPED_TEST_SUITE(StorageType, AsTuple<AllStorages>);

template <typename T>
struct CopyTypesCopyStorageTestCase : BaseTestCase<T> {};
TYPED_TEST_SUITE(CopyTypesCopyStorageTestCase, AsTuple<CopyTypesCopyStorageTestCases>);

TEST(AnyBuilder, canBuild) {
    // clang-format off
    using ActualAny = AnyBuilder
                            ::WithSize<128>
                            ::WithAlignment<2 * alignof(void*)>
                            ::DisableCopy
                            ::WithBasicExceptionGuarantee
                            ::EnableSafeAnyCast
                            ::WithDedicatedFunPtr
                            ::WithAllocator<detail::OneChunkAllocator<1234>>
                            ::Build;
    // clang-format on
    using ExpectedAny = Any<128,
                            Copy::DISABLED,
                            ExceptionGuarantee::BASIC,
                            2 * alignof(void*),
                            FunPtr::DEDICATED,
                            SafeAnyCast::ENABLED,
                            detail::OneChunkAllocator<1234>>;
    static_assert(std::is_same_v<ActualAny, ExpectedAny>);
}

TYPED_TEST(CopyTypesTestCase, canInstantiateFromRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(static_cast<const Value&>(kInt));
    ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
}

TYPED_TEST(StorageType, canGetByValue) {
    using Value = int;
    using Storage = TypeParam;
    Storage storage(Value{kInt});
    auto value = any_cast<Value>(storage);
    ASSERT_EQ(value, kInt);
    value++;
    ASSERT_EQ(value, kInt + 1);
    ASSERT_EQ(any_cast<Value>(storage), kInt);
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canCopy) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage other = storage;
    ASSERT_EQ(any_cast<Value>(storage).i, kInt);
    ASSERT_EQ(any_cast<Value>(other).i, kInt);
    ASSERT_NE(&any_cast<Value&>(storage), &any_cast<Value&>(other));
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canCopyAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    Storage other(Value{42});
    ASSERT_EQ(any_cast<Value>(other).i, 42);
    other = storage;
    ASSERT_EQ(any_cast<Value>(storage).i, kInt);
    ASSERT_EQ(any_cast<Value>(other).i, kInt);
    ASSERT_NE(&any_cast<Value&>(storage), &any_cast<Value&>(other));
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canSelfCopyAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage(Value{kInt});
    SUPPRESS_SELF_COPY_START
    storage = storage;
    SUPPRESS_SELF_COPY_END
    ASSERT_EQ(any_cast<Value>(storage).i, kInt);
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canCopyAssignToMovedFrom) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage storage{Value{42}};
    Storage unused{std::move(storage)};
    Storage other{Value{kInt}};
    storage = other;
    auto value = any_cast<Value&&>(std::move(storage));
    ASSERT_EQ(value.i, kInt);
}

TEST(FunRef, CanCallOverloaded) {
    constexpr auto add2 = [](int x, int y) { return x + y; };
    constexpr auto add3 = [](int x, int y, int z) { return x + y + z; };
    constexpr auto add23const = Overloads{add2, add3};

    const FunRef<int(int, int) const, int(int, int, int) const> fConst{&add23const};
    ASSERT_EQ(fConst(2, 5), add2(2, 5));
    ASSERT_EQ(fConst(2, 5, 7), add3(2, 5, 7));

    auto add23 = Overloads{add2, add3};
    FunRef<int(int, int), int(int, int, int)> f{&add23};
    ASSERT_EQ(f(2, 5), add2(2, 5));
    ASSERT_EQ(f(2, 5, 7), add3(2, 5, 7));

    FunRef<int(int, int), int(int, int, int) const> fHalfConst{&add23};
    ASSERT_EQ(fHalfConst(2, 5), add2(2, 5));
    ASSERT_EQ(std::as_const(fHalfConst)(2, 5, 7), add3(2, 5, 7));
}

template <typename T>
struct RefTest : testing::Test {};

using RefTypes = testing::Types<Ref, CRef>;
TYPED_TEST_SUITE(RefTest, RefTypes);

TYPED_TEST(RefTest, canCreateAndGet) {
    using R = TypeParam;

    int a = 1;
    int b = 2;
    R ra{a};
    R rb{b};

    ASSERT_EQ(any_cast<const int&>(rb), b);
    ASSERT_EQ(any_cast<int>(ra), a);
    ASSERT_EQ(any_cast<int>(rb), b);

    std::swap(ra, rb);
    ASSERT_EQ(any_cast<int>(rb), a);
    ASSERT_EQ(any_cast<int>(ra), b);
}

TYPED_TEST(RefTest, canCopyAndMove) {
    using R = TypeParam;
    static_assert(IsMovableAndCopyable<R>);
}

TEST(CRef, canCreateAndGet) {
    constexpr int a = 1;
    constexpr int b = 2;
    CRef ra{a};
    CRef rb{b};

    ASSERT_EQ(any_cast<int>(ra), a);
    ASSERT_EQ(any_cast<int>(rb), b);
    ASSERT_EQ(any_cast<const int&>(ra), a);
    ASSERT_EQ(any_cast<const int&>(rb), b);

    std::swap(ra, rb);
    ASSERT_EQ(any_cast<const int&>(rb), a);
    ASSERT_EQ(any_cast<const int&>(ra), b);
}

TEST(Ref, canConvert) {
    int a = 1;
    Ref ra{a};

    CRef cra{ra};
    ASSERT_EQ(any_cast<const int&>(cra), a);
}

#if defined(__cpp_exceptions)

class Bomb {
  public:
    int i;
    std::array<char, 15> payload;
    inline static int cnt = 0;
    Bomb(int i) : i(i) { cnt++; }
    ~Bomb() {
        i = -1;
        cnt--;
    }

    Bomb(const Bomb&) { throw std::runtime_error("Copy construction failed."); }

    Bomb& operator=(const Bomb& other) {
        if (this != &other) {
            throw std::runtime_error("Copy assignment failed.");
        }
        return *this;
    }

    Bomb(Bomb&&) noexcept(false) { throw std::runtime_error("Move construction failed."); }

    Bomb& operator=(Bomb&& other) noexcept(false) {
        if (this != &other) {
            throw std::runtime_error("Move assignment failed.");
        }
        return *this;
    }
};

template <ExceptionGuarantee eg>
constexpr auto filterByEg(auto storages) {
    return hana::filter(storages, [&](auto testCase) {
        using Storage = decltype(testCase)::type;
        return hana::bool_c < Storage::kExceptionGuarantee == eg > ;
    });
}

constexpr auto BasicEgMovableStorages = filterByEg<ExceptionGuarantee::BASIC>(AllStorages);
constexpr auto StrongEgMovableStorages = filterByEg<ExceptionGuarantee::STRONG>(AllStorages);

constexpr auto BasicEgCopyableStorages = filterByEg<ExceptionGuarantee::BASIC>(CopyStorageTypes);
constexpr auto StrongEgCopyableStorages = filterByEg<ExceptionGuarantee::STRONG>(CopyStorageTypes);

template <typename T>
struct EgTest : ValueCntNuller<Bomb>, AlternativeAllocatorResetter {};

template <typename T>
struct BasicEgMovableStorage : EgTest<T> {};
TYPED_TEST_SUITE(BasicEgMovableStorage, AsTuple<BasicEgMovableStorages>);

template <typename T>
struct StrongEgMovableStorage : EgTest<T> {};
TYPED_TEST_SUITE(StrongEgMovableStorage, AsTuple<StrongEgMovableStorages>);

template <typename T>
struct BasicEgCopyableStorage : EgTest<T> {};
TYPED_TEST_SUITE(BasicEgCopyableStorage, AsTuple<BasicEgCopyableStorages>);

template <typename T>
struct StrongEgCopyableStorage : EgTest<T> {};
TYPED_TEST_SUITE(StrongEgCopyableStorage, AsTuple<StrongEgCopyableStorages>);

TYPED_TEST(BasicEgMovableStorage, throwOnMoveWeak) {
    using Storage = TypeParam;
    {
        Storage first{std::in_place_type<Bomb>, kInt};
        Storage other{std::in_place_type<Bomb>, 123};
        try {
            other = std::move(first);
        } catch (...) {
        }
    }
    ASSERT_EQ(Bomb::cnt, 0);
}

TYPED_TEST(StrongEgMovableStorage, throwOnMoveStrong) {
    using Storage = TypeParam;
    {
        // under strong exception guarantees the container's move-assignment is noexcept
        Storage first{std::in_place_type<Bomb>, kInt};
        Storage other{std::in_place_type<Bomb>, 123};
        other = std::move(first);
        ASSERT_EQ(any_cast<Bomb&>(other).i, kInt);
    }
    ASSERT_EQ(Bomb::cnt, 0);
}

TYPED_TEST(BasicEgCopyableStorage, throwOnCopyWeak) {
    using Storage = TypeParam;
    {
        Storage first{std::in_place_type<Bomb>, kInt};
        Storage other{std::in_place_type<Bomb>, 123};
        try {
            other = first;
        } catch (...) {
        }
        ASSERT_EQ(any_cast<Bomb&>(first).i, kInt);
    }
    ASSERT_EQ(Bomb::cnt, 0);
}

TYPED_TEST(StrongEgCopyableStorage, throwOnCopyStrong) {
    using Storage = TypeParam;
    {
        Storage first{std::in_place_type<Bomb>, kInt};
        Storage other{std::in_place_type<Bomb>, 123};
        try {
            other = first;
        } catch (...) {
        }
        ASSERT_EQ(any_cast<Bomb&>(first).i, kInt);
        ASSERT_EQ(any_cast<Bomb&>(other).i, 123);
        ASSERT_EQ(Bomb::cnt, 2);
    }
    ASSERT_EQ(Bomb::cnt, 0);
}

constexpr auto SafeAnyCastTestCases = hana::filter(MoveTestCases, [](auto testCase) {
    using S = decltype(testCase)::type::Storage;
    return hana::bool_c < S::kSafeAnyCast == SafeAnyCast::ENABLED > ;
});

template <typename T>
struct SafeAnyCastTest : EgTest<T> {};
TYPED_TEST_SUITE(SafeAnyCastTest, AsTuple<SafeAnyCastTestCases>);

TYPED_TEST(SafeAnyCastTest, throwOnInvalidAnyCast) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    Storage first{std::in_place_type<Value>, kInt};
    EXPECT_THROW(any_cast<double>(first), BadAnyCast);
}

#endif
