#include "woid.hpp"

#define BOOST_TEST_MODULE AnyTest

#include <boost/test/included/unit_test.hpp>
#include <boost/test/data/test_case.hpp>
#include <boost/test/data/monomorphic.hpp>
#include <boost/hana.hpp>

namespace hana = boost::hana;
using namespace woid;

template <typename AlignAs>
struct S {
    alignas(AlignAs) std::array<char, 15> payload;
    int i;
    inline static int cnt = 0;
    S(int i): i(i) {  cnt++; }
    S(S&& other): i(other.i) { cnt++; }
    S& operator=(S&& other) { i = other.i; return *this; }
    S(const S&) = delete;
    S& operator=(const S&) = delete;
    ~S() {
        cnt--;
    }
};

template <typename AlignAs>
struct C {
    alignas(AlignAs) std::array<char, 15> payload;
    int i;
    inline static int cnt = 0;
    C(int i): i(i) {  cnt++; }
    C(C&& other): i(other.i) { cnt++; }
    C& operator=(C&& other) { i = other.i;  return *this; }
    C(const C& other) { i = other.i; cnt++; }
    C& operator=(const C& other )  { i = other.i;  return *this; }
    ~C() {
        cnt--;
    }
};

inline constexpr int kInt = 13;

constexpr auto IsExcptSafe = hana::tuple_c<bool, false, true>;
constexpr auto StaticStorageSizes = hana::tuple_c<int, 8, 80>; 

template <template <auto ...> typename T>
constexpr auto mkAny = [](auto T_pair) {
    constexpr int Size = decltype(+hana::at_c<0>(T_pair))::value;
    constexpr bool IsSafe = decltype(+hana::at_c<1>(T_pair))::value;
    return hana::type_c<T<Size, IsSafe>>;
};

auto make_instantiations = [](auto StorageSizes, auto ExcptSafe, auto Transformer) {
    return hana::transform(hana::cartesian_product(hana::make_tuple(StorageSizes, ExcptSafe)), Transformer);
};

constexpr auto AnyOnePtrsInsts = make_instantiations(StaticStorageSizes, IsExcptSafe, mkAny<AnyOnePtr>);
constexpr auto AnyTwoPtrsInsts = make_instantiations(StaticStorageSizes, IsExcptSafe, mkAny<AnyTwoPtrs>);
constexpr auto AnyThreePtrsInsts = make_instantiations(StaticStorageSizes, IsExcptSafe, mkAny<AnyThreePtrs>);
constexpr auto AnyOnePtrCpyInsts = make_instantiations(StaticStorageSizes, IsExcptSafe, mkAny<AnyOnePtrCpy>);

constexpr auto MoveOnlyStorageTypesHana = hana::append(hana::concat(AnyOnePtrsInsts, AnyTwoPtrsInsts), hana::type_c<detail::DynamicStorage>);
constexpr auto CopyStorageTypesHana = hana::concat(AnyThreePtrsInsts, AnyOnePtrCpyInsts);

static_assert(alignof(__int128) > alignof(void*)); //make sure int128 has big alignment
static_assert(alignof(std::int32_t) < alignof(void*)); //make sure int32 has small alignment
constexpr auto IntTypes = hana::tuple_t<std::int32_t, __int128>;

constexpr auto MoveOnlyTypes = hana::transform(IntTypes, hana::template_<S>);
constexpr auto CopyTypes = hana::transform(IntTypes, hana::template_<C>);
constexpr auto ValueTypes = hana::concat(MoveOnlyTypes, CopyTypes);

template <typename Storage_, typename Value_>
struct TestCase {
    using Storage = Storage_;
    using Value = Value_;
};

template <template <typename ...> typename Tmpl >
constexpr auto mk = [](auto T) { return hana::unpack(T, hana::template_<Tmpl>); };

constexpr static auto mkTestCases(auto StorageTypes, auto StoredTypes) {
    return hana::transform(hana::cartesian_product(hana::make_tuple(StorageTypes, StoredTypes)), mk<TestCase>);
}

constexpr auto CopyTypesCopyStorageTestCasesHana = mkTestCases(CopyStorageTypesHana, CopyTypes);
constexpr auto TestCasesHana = hana::concat(mkTestCases(MoveOnlyStorageTypesHana, ValueTypes), CopyTypesCopyStorageTestCasesHana);
constexpr auto CopyTypesTestCasesHana = hana::concat(mkTestCases(MoveOnlyStorageTypesHana, CopyTypes), CopyTypesCopyStorageTestCasesHana);

template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<std::tuple>))::type;

using TestCases = AsTuple<TestCasesHana>;
using CopyTypesTestCases = AsTuple<CopyTypesTestCasesHana>;
using StorageTypes = AsTuple<hana::concat(MoveOnlyStorageTypesHana, CopyStorageTypesHana)>;
using CopyTypesCopyStorageTestCases = AsTuple<CopyTypesCopyStorageTestCasesHana>;

BOOST_AUTO_TEST_CASE_TEMPLATE(canInstantiateFromRef, T, CopyTypesTestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(static_cast<const Value&>(kInt));
        BOOST_CHECK(any_cast<Value&>(storage).i == kInt);
    }
    BOOST_CHECK(T::Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canInstantiateFromRefRef, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Value v{kInt};
        Storage storage(std::move(v));
        BOOST_CHECK(any_cast<Value&>(storage).i == kInt);
    }
    BOOST_CHECK(T::Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canInstantiateInPlaceAndMove, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(std::in_place_type<Value>, kInt);
        BOOST_CHECK(any_cast<Value&>(storage).i == kInt);
        Storage otherStorage = std::move(storage);
        BOOST_CHECK(any_cast<Value&>(otherStorage).i == kInt);
    }
    BOOST_CHECK(T::Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canInstantiateAndMove, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage = std::move(storage);
            BOOST_CHECK(any_cast<Value&>(otherStorage).i == kInt);
        }
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canMoveAssign, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(Value{42});
            otherStorage = std::move(storage);
            BOOST_CHECK(any_cast<Value&>(otherStorage).i == kInt);
        }
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canSwap, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(Value{42});
            std::swap(storage, otherStorage);
            BOOST_CHECK(any_cast<Value&>(otherStorage).i == kInt);
            BOOST_CHECK(any_cast<Value&>(storage).i == 42);
        }
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canSwapHeterogeneously, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        {
            Storage otherStorage(int{42});
            std::swap(storage, otherStorage);
            BOOST_CHECK(any_cast<Value&>(otherStorage).i == kInt);
            BOOST_CHECK(any_cast<int>(storage) == 42);
        }
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canGetByValue, Storage, StorageTypes) {
    using Value = int;
    Storage storage(Value{kInt});
    auto value = any_cast<Value>(storage);
    BOOST_CHECK(value == kInt);
    value++;
    BOOST_CHECK(value == kInt + 1);
    BOOST_CHECK(any_cast<Value>(storage) == kInt);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canGetByRef, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        auto& valueRef = any_cast<Value&>(storage);
        BOOST_CHECK(valueRef.i == kInt);
        valueRef.i++;
        BOOST_CHECK(valueRef.i == kInt + 1);
        BOOST_CHECK(any_cast<Value&>(storage).i == kInt + 1);
    }
    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canGetByRefRef, T, TestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        auto value = any_cast<Value&&>(storage);
        BOOST_CHECK(value.i == kInt);
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canCopy, T, CopyTypesCopyStorageTestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        Storage other = storage;
        BOOST_CHECK(any_cast<Value>(storage).i == kInt);
        BOOST_CHECK(any_cast<Value>(other).i == kInt);
        BOOST_CHECK(Value::cnt == 2);
    }

    BOOST_CHECK(Value::cnt == 0);
}

BOOST_AUTO_TEST_CASE_TEMPLATE(canCopyAssign, T, CopyTypesCopyStorageTestCases) {
    T::Value::cnt = 0;
    using Storage = T::Storage;
    using Value = T::Value;
    {
        Storage storage(Value{kInt});
        Storage other(Value{42});
        BOOST_CHECK(any_cast<Value>(other).i == 42);
        other = storage;
        BOOST_CHECK(any_cast<Value>(storage).i == kInt);
        BOOST_CHECK(any_cast<Value>(other).i == kInt);
        BOOST_CHECK(Value::cnt == 2);
    }

    BOOST_CHECK(Value::cnt == 0);
}
