#define BOOST_TEST_MODULE AnyTest

#include "woid.hpp"
#include <boost/hana.hpp>
#include <boost/hana/fwd/filter.hpp>
#include <gtest/gtest-typed-test.h>
#include <gtest/gtest.h>

#define SUPPRESS_SELF_MOVE_START                                                                   \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wself-move\"")
#define SUPPRESS_SELF_MOVE_END _Pragma("GCC diagnostic pop")

#define SUPPRESS_SELF_COPY_START                                                                   \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wself-assign-overloaded\"")
#define SUPPRESS_SELF_COPY_END _Pragma("GCC diagnostic pop")

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

using AlternativeAllocator = detail::OneChunkAllocator<1024>;
constexpr auto Allocators = hana::tuple_t<DefaultAllocator, AlternativeAllocator>;
constexpr auto FunPtrTypes = hana::tuple_c<FunPtr, FunPtr::COMBINED, FunPtr::DEDICATED>;
constexpr auto IsExcptSafe = hana::tuple_c<ExceptionGuarantee,
                                           ExceptionGuarantee::NONE,
                                           ExceptionGuarantee::BASIC,
                                           ExceptionGuarantee::STRONG>;
constexpr auto IsSafeAnyCast
    = hana::tuple_c<SafeAnyCast, SafeAnyCast::ENABLED, SafeAnyCast::DISABLED>;
constexpr auto StaticStorageSizes = hana::tuple_c<size_t, 8, 80>;
constexpr auto Alignments = hana::tuple_c<size_t, sizeof(void*), alignof(__int128)>;

template <size_t... Is>
constexpr auto mkAnyImpl(auto args, std::index_sequence<Is...>) {
    return hana::type_c<Any<hana::at_c<Is>(args).value...,
                            typename decltype(+hana::at_c<sizeof...(Is)>(args))::type>>;
}

constexpr auto mkAny = [](auto args) {
    return mkAnyImpl(args, std::make_index_sequence<hana::size(args).value - 1>{});
};

template <Copy copy>
static constexpr auto make_instantiations() {
    return hana::transform(hana::cartesian_product(hana::make_tuple(StaticStorageSizes,
                                                                    hana::tuple_c<Copy, copy>,
                                                                    IsExcptSafe,
                                                                    Alignments,
                                                                    FunPtrTypes,
                                                                    IsSafeAnyCast,
                                                                    Allocators)),
                           mkAny);
};

constexpr auto MoveOnlyStorageTypes
    = hana::concat(make_instantiations<Copy::DISABLED>(),
                   hana::tuple_t<DynamicStorage<>, DynamicStorage<AlternativeAllocator>>);
constexpr auto CopyStorageTypes = make_instantiations<Copy::ENABLED>();

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

struct SmallInt {
    SmallInt(int i) : i(i) {}
    int i;
    inline static size_t cnt = 0;
};
static_assert(std::is_trivially_move_constructible_v<SmallInt>);

struct NonTrivialInt {
    NonTrivialInt(int i) : i(i) {}
    int i;
    ~NonTrivialInt() {}
    inline static size_t cnt = 0;
};
static_assert(!std::is_trivially_move_constructible_v<NonTrivialInt>);

struct BigInt {
    BigInt(int i) : i(i) {}
    inline static size_t cnt = 0;
    std::array<char, 500> bigginess;
    int i;
};
static_assert(std::is_trivially_move_constructible_v<BigInt>);

constexpr auto TrivialStorageTypes = hana::tuple_t<TrivialStorage<>>;
constexpr auto TrivialValueTypes = hana::tuple_t<SmallInt, BigInt, NonTrivialInt>;

constexpr auto CopyTypesCopyStorageTestCases = hana::concat(
    mkTestCases(CopyStorageTypes, CopyTypes), mkTestCases(TrivialStorageTypes, TrivialValueTypes));
constexpr auto MoveTestCases = hana::flatten(
    hana::make_tuple(mkTestCases(MoveOnlyStorageTypes, ValueTypes), CopyTypesCopyStorageTestCases));
constexpr auto CopyTypesTestCases
    = hana::concat(mkTestCases(MoveOnlyStorageTypes, CopyTypes), CopyTypesCopyStorageTestCases);

constexpr auto MoveTestCasesWithBigObject = hana::filter(MoveTestCases, [](auto testCase) {
    using TC = decltype(testCase)::type;
    using Storage = TC::Storage;
    using Value = TC::Value;
    return hana::bool_c < (Storage::kStaticStorageSize < sizeof(Value)
                           || Storage::kStaticStorageAlignment < alignof(Value))
           && (!std::is_same_v<Value, BigInt> && !std::is_same_v<Value, SmallInt>) > ;
});

template <auto HanaTuple>
using AsTuple = decltype(hana::unpack(HanaTuple, hana::template_<testing::Types>))::type;

constexpr auto AllStorages = hana::concat(MoveOnlyStorageTypes, CopyStorageTypes);

template <typename Value>
struct ValueCntNuller : testing::Test {
    ValueCntNuller() { Value::cnt = 0; }
    ~ValueCntNuller() { Value::cnt = 0; }
};

struct AlternativeAllocatorResetter {
    ~AlternativeAllocatorResetter() { AlternativeAllocator::reset(); }
};

template <typename T>
struct BaseTestCase : ValueCntNuller<typename T::Value>, AlternativeAllocatorResetter {};

template <typename T>
struct MoveTestCase : BaseTestCase<T> {};

TYPED_TEST_SUITE(MoveTestCase, AsTuple<MoveTestCases>);

template <typename T>
struct MoveTestCaseWithBigObject : testing::Test, AlternativeAllocatorResetter {};

TYPED_TEST_SUITE(MoveTestCaseWithBigObject, AsTuple<MoveTestCasesWithBigObject>);

template <typename T>
struct CopyTypesTestCase : BaseTestCase<T> {};

TYPED_TEST_SUITE(CopyTypesTestCase, AsTuple<CopyTypesTestCases>);

template <typename T>
struct StorageType : testing::Test, AlternativeAllocatorResetter {};
TYPED_TEST_SUITE(StorageType, AsTuple<AllStorages>);

template <typename T>
struct MoveStorageTypes : testing::Test, AlternativeAllocatorResetter {};
TYPED_TEST_SUITE(MoveStorageTypes, AsTuple<MoveOnlyStorageTypes>);

template <typename T>
struct CopyTypesCopyStorageTestCase : BaseTestCase<T> {};
TYPED_TEST_SUITE(CopyTypesCopyStorageTestCase, AsTuple<CopyTypesCopyStorageTestCases>);

TEST(AnyBuilder, canBuild) {
    // clang-format off
    using ActualAny = AnyBuilder
                            ::WithSize<128>
                            ::WithAlignment<2 * alignof(void*)>
                            ::DisableCopy
                            ::With<ExceptionGuarantee::BASIC>
                            ::With<SafeAnyCast::ENABLED>
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
    {
        Storage storage(static_cast<const Value&>(kInt));
        ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
    }
    ASSERT_EQ(TypeParam::Value::cnt, 0);
}

TYPED_TEST(MoveTestCaseWithBigObject, canInstantiateFromPtr) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    using Alloc = Storage::Alloc;
    {
        auto* ptr = Alloc::template make<Value>(kInt);
        Storage storage(kTransferOwnership, ptr);
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

TYPED_TEST(MoveTestCase, canSelfMoveAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        {
            SUPPRESS_SELF_MOVE_START
            storage = std::move(storage);
            SUPPRESS_SELF_MOVE_END
            ASSERT_EQ(any_cast<Value&>(storage).i, kInt);
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

TYPED_TEST(MoveTestCase, canGetConstRef) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        const Storage s{kInt};
        ASSERT_EQ(any_cast<const int&>(s), kInt);
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
        ASSERT_NE(&any_cast<Value&>(storage), &any_cast<Value&>(other));
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
        ASSERT_NE(&any_cast<Value&>(storage), &any_cast<Value&>(other));
    }

    ASSERT_EQ(Value::cnt, 0);
}

TYPED_TEST(CopyTypesCopyStorageTestCase, canSelfCopyAssign) {
    using Storage = TypeParam::Storage;
    using Value = TypeParam::Value;
    {
        Storage storage(Value{kInt});
        SUPPRESS_SELF_COPY_START
        storage = storage;
        SUPPRESS_SELF_COPY_END
        ASSERT_EQ(any_cast<Value>(storage).i, kInt);
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

template <template <typename> typename TraitV, typename X, typename Y>
static void checkTraitSame() {
    static_assert(TraitV<X>::value == TraitV<Y>::value);
}

TYPED_TEST(StorageType, storagePropertiesArePropagatedToTheFun) {
    using Storage = TypeParam;
    using Fun = Fun<Storage, void(void) const>;
    checkTraitSame<std::is_copy_constructible, Storage, Fun>();
    checkTraitSame<std::is_copy_assignable, Storage, Fun>();
    checkTraitSame<std::is_move_constructible, Storage, Fun>();
    checkTraitSame<std::is_move_assignable, Storage, Fun>();
    checkTraitSame<std::is_nothrow_move_constructible, Storage, Fun>();
    checkTraitSame<std::is_nothrow_move_assignable, Storage, Fun>();
}

TYPED_TEST(StorageType, canCallOverloaded) {
    using Storage = TypeParam;
    auto add2 = [](int x, int y) { return x + y; };
    auto add4 = [](int v, int&& x, int& y, const int& z) { return v + x + y + z; };
    const Fun<Storage, int(int, int) const, int(int, int&&, int&, const int&) const> f{add2, add4};
    ASSERT_EQ(f(2, 5), add2(2, 5));
    const int z = 7;
    int y = 5;
    int x = 2;
    int w = 1;
    int xCopy = x;
    ASSERT_EQ(f(w, std::move(x), y, z), add4(w, std::move(xCopy), y, z));
}

TEST(FunRef, CanCallOverloaded) {
    auto add2 = [](int x, int y) { return x + y; };
    auto add3 = [](int x, int y, int z) { return x + y + z; };
    const FunRef<int(int, int) const, int(int, int, int) const> f{&add2, &add3};
    ASSERT_EQ(f(2, 5), add2(2, 5));
    ASSERT_EQ(f(2, 5, 7), add3(2, 5, 7));
}

TYPED_TEST(MoveStorageTypes, canCallConstMoveOnlyFunction) {
    using Storage = TypeParam;
    auto add = [uniq = std::make_unique<int>(1)](int x, int y) { return x + y; };
    const Fun<Storage, int(int, int) const> f{std::move(add)};
    ASSERT_EQ(f(2, 5), add(2, 5));
}

TYPED_TEST(StorageType, canCallConstFunction) {
    using Storage = TypeParam;
    auto add = [](int x, int y) { return x + y; };
    const Fun<Storage, int(int, int) const> f{add};
    ASSERT_EQ(f(2, 5), add(2, 5));
}

TYPED_TEST(StorageType, canCallConstVoidFunction) {
    using Storage = TypeParam;
    int i = 0;
    auto setI = [&]() { i = 5; };
    const Fun<Storage, void() const> f{setI};
    f();
    ASSERT_EQ(i, 5);
}

TYPED_TEST(StorageType, canCallNonConstFunction) {
    using Storage = TypeParam;
    auto add = [](int x, int y) mutable { return x + y; };
    Fun<Storage, int(int, int)> f{add};
    ASSERT_EQ(f(2, 5), add(2, 5));
}

TYPED_TEST(StorageType, canCallNonConstVoidFunction) {
    using Storage = TypeParam;
    int i = 0;
    auto setI = [&]() mutable { i = 5; };
    Fun<Storage, void()> f{setI};
    f();
    ASSERT_EQ(i, 5);
}

struct Int {
    int i;
    int add(int d) const { return i + d; }
    int inc(int d) {
        i += d;
        return i;
    }
};

TYPED_TEST(StorageType, canCallMethod) {
    using Storage = TypeParam;
    Int i{3};

    const Fun<Storage, int(Int*, int) const> f{&Int::inc};

    ASSERT_EQ(f(&i, 5), 3 + 5);
    ASSERT_EQ(i.i, 3 + 5);
}

TYPED_TEST(StorageType, canCallConstMethod) {
    using Storage = TypeParam;
    Int i{3};

    const Fun<Storage, int(const Int*, int) const> f{&Int::add};

    ASSERT_EQ(f(&i, 5), 3 + 5);
}

struct MoveOnlyFunctor {
    constexpr MoveOnlyFunctor() = default;
    MoveOnlyFunctor(const MoveOnlyFunctor&) = delete;
    MoveOnlyFunctor& operator=(const MoveOnlyFunctor&) = delete;
    MoveOnlyFunctor(MoveOnlyFunctor&&) = default;
    MoveOnlyFunctor& operator=(MoveOnlyFunctor&&) = default;
    ~MoveOnlyFunctor() = default;
    int operator()(int a, int b) const { return a + b; }
    int operator()(int a, int b) { return 2 * a + b; }
};

template <typename T>
constexpr inline bool IsMovableAndCopyable = std::is_move_constructible_v<T>
                                             && std::is_move_assignable_v<T>
                                             && std::is_copy_constructible_v<T>
                                             && std::is_copy_assignable_v<T>;

TEST(FunRef, canCallConst) {
    constexpr auto add = MoveOnlyFunctor{};
    static_assert(!std::is_move_constructible_v<decltype(add)>);

    using F = FunRef<int(int, int) const>;
    F fRef{&add};
    static_assert(IsMovableAndCopyable<F>);
    ASSERT_EQ(fRef(2, 5), add(2, 5));
}

TEST(FunRef, canCall) {
    auto doubleAdd = MoveOnlyFunctor{};

    using F = FunRef<int(int, int)>;
    F fRef{&doubleAdd};
    static_assert(IsMovableAndCopyable<F>);
    ASSERT_EQ(fRef(2, 5), doubleAdd(2, 5));
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
