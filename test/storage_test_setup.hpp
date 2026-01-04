
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
                   hana::tuple_t<DynamicAny<Copy::DISABLED>,
                                 DynamicAny<Copy::DISABLED, AlternativeAllocator>,
                                 TrivialAny<8, Copy::DISABLED>,
                                 TrivialAny<8, Copy::DISABLED, 8, AlternativeAllocator>>);
constexpr auto CopyStorageTypes
    = hana::concat(make_instantiations<Copy::ENABLED>(),
                   hana::tuple_t<DynamicAny<Copy::ENABLED>,
                                 DynamicAny<Copy::ENABLED, AlternativeAllocator>,
                                 TrivialAny<8, Copy::ENABLED>,
                                 TrivialAny<8, Copy::ENABLED, 8, AlternativeAllocator>>);

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

constexpr auto TrivialStorageTypes = hana::tuple_t<TrivialAny<>>;
constexpr auto TrivialValueTypes = hana::tuple_t<SmallInt, BigInt, NonTrivialInt>;

constexpr auto CopyTypesCopyStorageTestCases = hana::concat(
    mkTestCases(CopyStorageTypes, CopyTypes), mkTestCases(TrivialStorageTypes, TrivialValueTypes));
constexpr auto MoveTestCases = hana::flatten(hana::make_tuple(
    mkTestCases(MoveOnlyStorageTypes, ValueTypes),
    CopyTypesCopyStorageTestCases,
    mkTestCases(hana::tuple_t<TrivialAny<16, Copy::DISABLED>>, TrivialValueTypes)));
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

constexpr auto AllStorages
    = hana::flatten(hana::make_tuple(MoveOnlyStorageTypes, CopyStorageTypes, TrivialStorageTypes));

template <typename Value>
struct ValueCntNuller : testing::Test {
    ValueCntNuller() { Value::cnt = 0; }
    ~ValueCntNuller() {
        EXPECT_EQ(Value::cnt, 0);
        Value::cnt = 0;
    }
};

struct AlternativeAllocatorResetter {
    ~AlternativeAllocatorResetter() { AlternativeAllocator::reset(); }
};

template <typename T>
struct BaseTestCase : ValueCntNuller<typename T::Value>, AlternativeAllocatorResetter {};

template <auto T>
struct TestV;

template <typename T>
constexpr inline bool IsMovableAndCopyable = std::is_move_constructible_v<T>
                                             && std::is_move_assignable_v<T>
                                             && std::is_copy_constructible_v<T>
                                             && std::is_copy_assignable_v<T>;

template <typename T>
struct Test;

inline static constexpr auto shard = [](auto sequence, auto i, auto shards) {
    auto N = hana::size(sequence);
    auto K = shards;

    auto minSize = N / K;
    auto remainder = N % K;

    auto start = hana::size_c<(i * minSize) + (i < remainder ? i : remainder)>;

    auto shardSize = minSize + hana::size_c<(i < remainder ? 1 : 0)>;

    return hana::slice(sequence, hana::make_range(start, start + shardSize));
};
