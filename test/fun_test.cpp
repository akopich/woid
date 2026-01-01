#define BOOST_TEST_MODULE FunTest

#include "storage_test_setup.hpp"

template <typename T>
struct MoveStorageTypes : testing::Test, AlternativeAllocatorResetter {};
TYPED_TEST_SUITE(MoveStorageTypes, AsTuple<MoveOnlyStorageTypes>);

template <typename T>
struct StorageType : testing::Test, AlternativeAllocatorResetter {};
TYPED_TEST_SUITE(StorageType, AsTuple<AllStorages>);

TYPED_TEST(MoveStorageTypes, canCallConstMoveOnlyFunction) {
    using Storage = TypeParam;
    auto add = [uniq = std::make_unique<int>(1)](int x, int y) { return x + y; };
    const Fun<Storage, int(int, int) const> f{std::move(add)};
    ASSERT_EQ(f(2, 5), add(2, 5));
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
    const Fun<Storage, int(int, int) const, int(int, int&&, int&, const int&) const> f{
        Overloads{add2, add4}};
    ASSERT_EQ(f(2, 5), add2(2, 5));
    const int z = 7;
    int y = 5;
    int x = 2;
    int w = 1;
    int xCopy = x;
    ASSERT_EQ(f(w, std::move(x), y, z), add4(w, std::move(xCopy), y, z));
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
