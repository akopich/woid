#define BOOST_TEST_MODULE AnyTest

#include "woid.hpp"
#include <boost/hana.hpp>
#include <boost/hana/fwd/filter.hpp>
#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>

namespace hana = boost::hana;
using namespace woid;

template <typename AlignAs>
struct S {
    alignas(AlignAs) std::array<char, 15> payload;
    int i;
    inline static int cnt = 0;
    S(int i) : i(i) { cnt++; }
    S(S&& other) : i(other.i) { cnt++; }
    S& operator=(S&& other) {
        i = other.i;
        return *this;
    }
    S(const S&) = delete;
    S& operator=(const S&) = delete;
    ~S() {
        i = -1;
        cnt--;
    }
};

template <typename AlignAs>
struct C {
    alignas(AlignAs) std::array<char, 15> payload;
    int i;
    inline static int cnt = 0;
    C(int i) : i(i) { cnt++; }
    C(C&& other) : i(other.i) { cnt++; }
    C& operator=(C&& other) {
        i = other.i;
        return *this;
    }
    C(const C& other) {
        i = other.i;
        cnt++;
    }
    C& operator=(const C& other) {
        i = other.i;
        return *this;
    }
    ~C() {
        i = -1;
        cnt--;
    }
};

inline constexpr int kInt = 13;

constexpr auto FunPtrTypes = hana::tuple_c<FunPtr, FunPtr::COMBINED, FunPtr::DEDICATED>;
constexpr auto IsExcptSafe = hana::tuple_c<ExceptionGuarantee,
                                           ExceptionGuarantee::NONE,
                                           ExceptionGuarantee::BASIC,
                                           ExceptionGuarantee::STRONG>;
constexpr auto StaticStorageSizes = hana::tuple_c<size_t, 8, 80>;
constexpr auto Alignments = hana::tuple_c<size_t, sizeof(void*), alignof(__int128)>;

template <template <auto...> typename T>
constexpr auto mkAny = [](auto args) {
    constexpr auto Size = hana::at_c<0>(args).value;
    constexpr auto IsCopyEnabled = hana::at_c<1>(args).value;
    constexpr auto IsSafe = hana::at_c<2>(args).value;
    constexpr auto Aligment = hana::at_c<3>(args).value;
    constexpr auto FunPtrType = hana::at_c<4>(args).value;
    return hana::type_c<T<Size, IsCopyEnabled, IsSafe, Aligment, FunPtrType>>;
};

template <Copy copy, template <auto...> typename Any>
static constexpr auto make_instantiations() {
    return hana::transform(
        hana::cartesian_product(hana::make_tuple(
            StaticStorageSizes, hana::tuple_c<Copy, copy>, IsExcptSafe, Alignments, FunPtrTypes)),
        mkAny<Any>);
};

constexpr auto MoveOnlyStorageTypes = hana::append(make_instantiations<Copy::DISABLED, Any>(),
                                                   hana::type_c<detail::DynamicStorage>);
constexpr auto CopyStorageTypes = make_instantiations<Copy::ENABLED, Any>();

static_assert(alignof(__int128) > alignof(void*));     // make sure int128 has big alignment
static_assert(alignof(std::int32_t) < alignof(void*)); // make sure int32 has small alignment
constexpr auto IntTypes = hana::tuple_t<std::int32_t, __int128>;

constexpr auto MoveOnlyTypes = hana::transform(IntTypes, hana::template_<S>);
constexpr auto CopyTypes = hana::transform(IntTypes, hana::template_<C>);
constexpr auto ValueTypes = hana::concat(MoveOnlyTypes, CopyTypes);

template <typename Storage_, typename Value_>
struct TestCase {
    using Storage = Storage_;
    using Value = Value_;
};

template <template <typename...> typename Tmpl>
constexpr auto mk = [](auto T) { return hana::unpack(T, hana::template_<Tmpl>); };

constexpr static auto mkTestCases(auto StorageTypes, auto StoredTypes) {
    return hana::transform(hana::cartesian_product(hana::make_tuple(StorageTypes, StoredTypes)),
                           mk<TestCase>);
}

constexpr auto CopyTypesCopyStorageTestCases = mkTestCases(CopyStorageTypes, CopyTypes);
constexpr auto MoveTestCases
    = hana::concat(mkTestCases(MoveOnlyStorageTypes, ValueTypes), CopyTypesCopyStorageTestCases);
constexpr auto CopyTypesTestCases
    = hana::concat(mkTestCases(MoveOnlyStorageTypes, CopyTypes), CopyTypesCopyStorageTestCases);

template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<testing::Types>))::type;

constexpr auto AllStorages = hana::concat(MoveOnlyStorageTypes, CopyStorageTypes);

template <typename Value>
struct ValueCntNuller : testing::Test {
    ValueCntNuller() { Value::cnt = 0; }
    ~ValueCntNuller() { Value::cnt = 0; }
};

template <typename T>
struct BaseTestCase : ValueCntNuller<typename T::Value> {};

template <typename T>
struct MoveTestCase : testing::Test {};

TYPED_TEST_SUITE(MoveTestCase, AsTuple<MoveTestCases>);

template <typename T>
struct CopyTypesTestCase : BaseTestCase<T> {};

TYPED_TEST_SUITE(CopyTypesTestCase, AsTuple<CopyTypesTestCases>);

template <typename T>
struct StorageType : testing::Test {};
TYPED_TEST_SUITE(StorageType, AsTuple<AllStorages>);

template <typename T>
struct CopyTypesCopyStorageTestCase : BaseTestCase<T> {};
TYPED_TEST_SUITE(CopyTypesCopyStorageTestCase, AsTuple<CopyTypesCopyStorageTestCases>);

TYPED_TEST(CopyTypesTestCase, canInstantiateFromRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(static_cast<const Value&>(kInt));
        ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
    }
    ASSERT_EQ(TypeParam::Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canInstantiateFromRefRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Value v{kInt};
        Storage storage(std::move(v));
        ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
    }
    ASSERT_EQ(TypeParam::Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canInstantiateInPlaceAndMove) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(std::in_place_type<Value>, kInt);
        ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
        Storage otherStorage = std::move(storage);
        ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
    }
    ASSERT_EQ(TypeParam::Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canInstantiateAndMove) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage = std::move(storage);
            ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
        }
    }

    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canMoveAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(Value{42});
            otherStorage = std::move(storage);
            ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
        }
    }
    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canSwap) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(Value{42});
            std::swap(storage, otherStorage);
            ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
            ASSERT_EQ(any_cast<Value&>(storage).i, 42);
        }
    }
    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canSwapHeterogeneously) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(int{42});

            std::swap(storage, otherStorage);
            ASSERT_EQ(any_cast<Value&>(otherStorage).i, kInt);
            ASSERT_EQ(any_cast<int>(storage), 42);

            std::swap(storage, otherStorage);
            ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
            ASSERT_EQ(any_cast<int>(otherStorage), 42);
        }
    }
    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canGetByRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        auto& valueRef = any_cast<Value&>(storage);
        ASSERT_EQ(valueRef.i, kInt);
        valueRef.i++;
        ASSERT_EQ(valueRef.i, kInt + 1);
        ASSERT_EQ(any_cast<Value&>(storage).i, kInt + 1);
    }
    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canGetByRefRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        auto value = any_cast<Value&&>(std::move(storage));
        ASSERT_EQ(value.i, kInt);
    }

    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(MoveTestCase, canMoveAssignToMovedFrom) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage{Value{42}};
        Storage unused{std::move(storage)};
        storage = Storage{Value{kInt}};
        auto value = any_cast<Value&&>(std::move(storage));
        ASSERT_EQ(value.i, kInt);
    }

    ASSERT_EQ(Value::cnt, 0);
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
    {
        Storage storage(Value{kInt});
        Storage other = storage;
        ASSERT_EQ(any_cast<Value>(storage).i, kInt);
        ASSERT_EQ(any_cast<Value>(other).i, kInt);
        ASSERT_EQ(Value::cnt, 2);
    }

    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canCopyAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        Storage other(Value{42});
        ASSERT_EQ(any_cast<Value>(other).i, 42);
        other = storage;
        ASSERT_EQ(any_cast<Value>(storage).i, kInt);
        ASSERT_EQ(any_cast<Value>(other).i, kInt);
        ASSERT_EQ(Value::cnt, 2);
    }

    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canCopyAssignToMovedFrom) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage{Value{42}};
        Storage unused{std::move(storage)};
        Storage other{Value{kInt}};
        storage = other;
        auto value = any_cast<Value&&>(std::move(storage));
        ASSERT_EQ(value.i, kInt);
    }

    ASSERT_EQ(Value::cnt, 0);
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
        return hana::bool_c < Storage::exceptionGuarantee == eg > ;
    });
}

constexpr auto BasicEgMovableStorages = filterByEg<ExceptionGuarantee::BASIC>(AllStorages);
constexpr auto StrongEgMovableStorages = filterByEg<ExceptionGuarantee::STRONG>(AllStorages);

constexpr auto BasicEgCopyableStorages = filterByEg<ExceptionGuarantee::BASIC>(CopyStorageTypes);
constexpr auto StrongEgCopyableStorages = filterByEg<ExceptionGuarantee::STRONG>(CopyStorageTypes);

template <typename T>
struct EgTest : ValueCntNuller<Bomb> {};

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

#endif
