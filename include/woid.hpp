#pragma once

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <functional>
#include <limits>
#include <memory>
#include <print>
#include <type_traits>
#include <utility>

#define SUPPRESS_SWITCH_WARNING_START                                                              \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wswitch\"")
#define SUPPRESS_SWITCH_WARNING_END _Pragma("GCC diagnostic pop")

#ifdef WOID_SYMBOL_VISIBILITY
#define WOID_SYMBOL_VISIBILITY_FLAG __attribute__((visibility("default")))
#else
#define WOID_SYMBOL_VISIBILITY_FLAG
#endif

#ifdef __clang__
#define WOID_NO_ICF // clang seems to have no equivalent
#elif __GNUC__
#define WOID_NO_ICF [[gnu::no_icf]]
#else
#define WOID_NO_ICF
#endif

namespace woid WOID_SYMBOL_VISIBILITY_FLAG {

enum class ExceptionGuarantee { NONE, BASIC, STRONG };
enum class Copy { ENABLED, DISABLED };
enum class FunPtr { COMBINED, DEDICATED };

// This one is tricky. SafeAnyCast enables a fast, low-overhead type check. As we rely on no rtti
// for performance sake (even if it's enabled), we have to resort to function address matching. In
// order for this to work correctly across TU boundary be sure to set symbol visibility to default.
// Be it for the whole project (with `-fvisibility=default`) or just for Woid with
// `#define WOID_SYMBOL_VISIBILITY`.
enum class SafeAnyCast { ENABLED, DISABLED };

enum class VTableOwnership { SHARED, DEDICATED };

struct TransferOwnership {};
inline TransferOwnership kTransferOwnership{};

struct DefaultAllocator {
    template <typename T>
    static T* make(auto&&... args) {
        return new T(std::forward<decltype(args)>(args)...);
    }

    template <typename T>
    static void del(T* obj) noexcept {
        delete obj;
    }
};

struct BadAnyCast {};

struct Ref;
struct CRef;

namespace detail {

template <typename... Ts>
struct Typelist;

template <typename TL>
struct HeadTail;

template <typename Head_, typename... Tail_>
struct HeadTail<Typelist<Head_, Tail_...>> {
    using Head = Head_;
    using Tail = Typelist<Tail_...>;
};

template <typename TL>
using Head = HeadTail<TL>::Head;

template <typename TL>
using Tail = HeadTail<TL>::Tail;

template <typename T>
struct TypeTag {
    using Type = T;
};

template <typename T>
inline constexpr TypeTag<T> kTypeTag{};

template <typename T>
static constexpr inline bool IsConstRef = std::is_const_v<std::remove_reference_t<T>>;

template <typename S, typename T>
using RetainConstPtr = std::conditional_t<IsConstRef<S>, const T*, T*>;

struct MemManagerTwoPtrs {
  protected:
    using DeletePtr = void (*)(void*);
    using MovePtr = void (*)(void*, void*);

  public:
    void del(void* p) const { std::invoke(deletePtr, p); }

    void move(void* src, void* dst) const { std::invoke(movePtr, src, dst); }

    DeletePtr deletePtr;
    MovePtr movePtr;
};

struct MemManagerThreePtrs : MemManagerTwoPtrs {
  private:
    using CpyPtr = void (*)(void*, void*);

  public:
    void cpy(void* src, void* dst) const { std::invoke(cpyPtr, src, dst); }

    CpyPtr cpyPtr;
};

template <typename T>
constexpr inline auto delStatic = [](void* ptr) static { static_cast<T*>(ptr)->~T(); };

template <typename T>
constexpr inline auto movStatic = [](void* src, void* dst) static {
    new (dst) T(std::move(*static_cast<T*>(src)));
    static_cast<T*>(src)->~T();
};

template <typename T>
constexpr inline auto cpyStatic
    = [](void* src, void* dst) static { new (dst) T(*static_cast<T*>(src)); };

constexpr inline auto mkMemManagerTwoPtrsStatic = []<typename T>(TypeTag<T>) consteval static {
    return MemManagerTwoPtrs(delStatic<T>, movStatic<T>);
};

constexpr inline auto mkMemManagerThreePtrsStatic = []<typename T>(TypeTag<T>) consteval static {
    return MemManagerThreePtrs{{delStatic<T>, movStatic<T>}, cpyStatic<T>};
};

template <typename T, typename Alloc>
constexpr inline auto delDynamic = [](void* ptr) static { Alloc::del(*static_cast<T**>(ptr)); };

template <typename T>
constexpr inline auto movDynamic = [](void* src, void* dst) static {
    *static_cast<T**>(dst) = *static_cast<T**>(src);
    *static_cast<T**>(src) = nullptr;
};

template <typename T, typename Alloc>
constexpr inline auto cpyDynamic = [](void* src, void* dst) static {
    auto* newPtr = Alloc::template make<T>(**static_cast<T**>(src));
    *static_cast<T**>(dst) = newPtr;
};

constexpr inline auto mkMemManagerTwoPtrsDynamic
    = []<typename T, typename Alloc>(TypeTag<T>, TypeTag<Alloc>) consteval static {
          return MemManagerTwoPtrs{
              delDynamic<T, Alloc>,
              movDynamic<T>,
          };
      };

constexpr inline auto mkMemManagerThreePtrsDynamic
    = []<typename T, typename Alloc>(TypeTag<T>, TypeTag<Alloc>) consteval static {
          return MemManagerThreePtrs{{delDynamic<T, Alloc>, movDynamic<T>}, cpyDynamic<T, Alloc>};
      };

enum Op { DEL, MOV, CPY };

struct MemManagerOnePtr {
  protected:
    using Ptr = void (*)(Op, void*, void*);

  public:
    void del(void* p) const { std::invoke(ptr, DEL, p, nullptr); }

    void move(void* src, void* dst) const { std::invoke(ptr, MOV, src, dst); }

    Ptr ptr;
};

struct MemManagerOnePtrCpy : MemManagerOnePtr {
    void cpy(void* src, void* dst) const { std::invoke(ptr, CPY, src, dst); }
};

template <typename Del, typename Mov>
constexpr auto mkMemManagerOnePtrFromLambdas(Del, Mov) {
    return MemManagerOnePtr{+[](Op op, void* ptr, void* dst) static -> void {
        SUPPRESS_SWITCH_WARNING_START
        switch (op) {
            case DEL:
                Del{}(ptr);
                break;
            case MOV:
                Mov{}(ptr, dst);
        }
        SUPPRESS_SWITCH_WARNING_END
    }};
}

template <typename Del, typename Mov, typename Cpy>
consteval auto mkMemManagerOnePtrCpyFromLambdas(Del, Mov, Cpy) {
    return MemManagerOnePtrCpy{{+[](Op op, void* ptr, void* dst) static -> void {
        switch (op) {
            case DEL:
                Del{}(ptr);
                break;
            case MOV:
                Mov{}(ptr, dst);
                break;
            case CPY:
                Cpy{}(ptr, dst);
        }
    }}};
}

constexpr inline auto mkMemManagerOnePtrStatic = []<typename T>(TypeTag<T>) consteval static {
    return mkMemManagerOnePtrFromLambdas(delStatic<T>, movStatic<T>);
};

constexpr inline auto mkMemManagerOnePtrDynamic
    = []<typename T, typename Alloc>(TypeTag<T>, TypeTag<Alloc>) consteval static {
          return mkMemManagerOnePtrFromLambdas(delDynamic<T, Alloc>, movDynamic<T>);
      };

constexpr inline auto mkMemManagerOnePtrCpyStatic = []<typename T>(TypeTag<T>) consteval static {
    return mkMemManagerOnePtrCpyFromLambdas(delStatic<T>, movStatic<T>, cpyStatic<T>);
};

constexpr inline auto mkMemManagerOnePtrCpyDynamic
    = []<typename T, typename Alloc>(TypeTag<T>, TypeTag<Alloc>) consteval static {
          return mkMemManagerOnePtrCpyFromLambdas(
              delDynamic<T, Alloc>, movDynamic<T>, cpyDynamic<T, Alloc>);
      };

template <typename T, typename Self, typename Void>
T star(Void* p) {
    return static_cast<T>(*static_cast<RetainConstPtr<Self, std::remove_reference_t<T>>>(p));
}

template <auto mmMaker>
using GetMemManager = decltype(mmMaker(kTypeTag<int>));

inline void reportBadAnyCast() {
#if defined(__cpp_exceptions)
    throw BadAnyCast{};
#else
    std::terminate();
#endif
}

template <typename Self, typename S>
decltype(auto) ptr(S&& s) {
    return static_cast<detail::RetainConstPtr<Self, void>>(
        std::launder(&std::forward<S>(s).front()));
}

template <auto mmStaticMaker,
          auto mmDynamicMaker,
          std::size_t kSize,
          std::size_t kAlignment,
          ExceptionGuarantee kEg,
          Copy kCopy,
          SafeAnyCast kSac,
          typename Alloc_>
    requires(kSize >= sizeof(void*) && kAlignment >= alignof(void*)) class Woid {
  private:
    static constexpr bool kIsMoveOnly = kCopy == Copy::DISABLED;

    using MemManager = GetMemManager<mmStaticMaker>;
    static_assert(std::is_same_v<MemManager, GetMemManager<mmStaticMaker>>);

    template <typename T>
    inline static constexpr bool kIsBig
        = sizeof(T) > kSize
          || alignof(T) > kAlignment
          || (kEg == ExceptionGuarantee::STRONG && !std::is_nothrow_move_constructible_v<T>);

    template <typename T>
    static constexpr inline auto dynamicMM WOID_NO_ICF
        = mmDynamicMaker(kTypeTag<T>, kTypeTag<Alloc_>);

    template <typename T>
    static constexpr inline auto staticMM WOID_NO_ICF = mmStaticMaker(kTypeTag<T>);

  public:
    inline static constexpr auto kExceptionGuarantee = kEg;
    inline static constexpr auto kStaticStorageSize = kSize;
    inline static constexpr auto kStaticStorageAlignment = kAlignment;
    inline static constexpr auto kSafeAnyCast = kSac;
    using Alloc = Alloc_;

    template <typename T>
    explicit Woid(T&& t)
        requires(!std::is_same_v<std::remove_cvref_t<T>, Woid>)
          : Woid(std::in_place_type<std::remove_cvref_t<T>>, std::forward<T>(t)) {}

    template <typename T>
    explicit Woid(TransferOwnership, T* tPtr)
        requires(kIsBig<T> && !std::is_const_v<T>) {
        this->mm = &dynamicMM<T>;
        *static_cast<void**>(ptr()) = tPtr;
    }

    template <typename T, typename... Args>
    explicit Woid(std::in_place_type_t<T>, Args&&... args) {
        if constexpr (kIsBig<T>) {
            this->mm = &dynamicMM<T>;
            auto* obj = Alloc::template make<T>(std::forward<Args>(args)...);
            *static_cast<void**>(ptr()) = obj;
        } else {
            this->mm = &staticMM<T>;
            new (ptr()) T(std::forward<Args>(args)...);
        }
    }
    Woid(const Woid& other)
        requires(!kIsMoveOnly)
          : mm(other.mm) {
        mm->cpy(const_cast<void*>(other.ptr()), ptr());
    }
    Woid& operator=(const Woid& other)
        requires(!kIsMoveOnly) {
        if constexpr (kEg == ExceptionGuarantee::STRONG) {
            *this = Woid{other};
        } else {
            if (this == &other)
                return *this;
            if (mm != nullptr)
                mm->del(ptr());
            if constexpr (kEg == ExceptionGuarantee::BASIC)
                mm = nullptr;
            other.mm->cpy(const_cast<void*>(other.ptr()), ptr());
            mm = other.mm;
        }
        return *this;
    }
    Woid(Woid&& other) noexcept(kEg != ExceptionGuarantee::BASIC) : mm(other.mm) {
        mm->move(other.ptr(), ptr());
        other.mm = nullptr;
    }
    Woid& operator=(Woid&& other) noexcept(kEg != ExceptionGuarantee::BASIC) {
        if (this == &other)
            return *this;
        if (mm != nullptr)
            mm->del(ptr());
        if constexpr (kEg == ExceptionGuarantee::BASIC)
            mm = nullptr;
        other.mm->move(other.ptr(), ptr());
        mm = other.mm;
        other.mm = nullptr;
        return *this;
    }
    ~Woid() {
        if (mm != nullptr)
            mm->del(ptr());
    }

    template <typename T, typename Self>
    T get(this Self&& self) {
        constexpr static bool isConst = std::is_const_v<Self>;
        constexpr static bool isRef = std::is_lvalue_reference_v<Self> && !isConst;
        constexpr static bool isConstRef = std::is_lvalue_reference_v<Self> && isConst;
        constexpr static bool isRefRef = std::is_rvalue_reference_v<Self>;
        using TnoRef = std::remove_reference_t<T>;
        static_assert(!isRef || std::is_constructible_v<T, TnoRef&>);
        static_assert(!isConstRef || std::is_constructible_v<T, const TnoRef&>);
        static_assert(!isRefRef || std::is_constructible_v<T, TnoRef>);
        using TnoCvRef = std::remove_cvref_t<T>;

        auto p = const_cast<void*>(std::forward<Self>(self).ptr());
        if constexpr (kIsBig<TnoCvRef>) {
            std::forward<Self>(self).template checkCastIfEnabled<dynamicMM<TnoCvRef>>();
            p = *static_cast<void**>(p);
        } else {
            std::forward<Self>(self).template checkCastIfEnabled<staticMM<TnoCvRef>>();
        }
        return star<T, Self>(p);
    }

  private:
    template <auto& MM, typename Self>
    void checkCastIfEnabled(this Self&& self) {
        if constexpr (kSafeAnyCast == SafeAnyCast::ENABLED) {
            if (std::forward<Self>(self).mm != &MM) {
                reportBadAnyCast();
            }
        }
    }

    alignas(kAlignment) std::array<char, kSize> storage;
    const MemManager* mm;

    template <typename Self>
    decltype(auto) ptr(this Self&& self) {
        return detail::ptr<Self>(std::forward<Self>(self).storage);
    }
};

template <typename Alloc>
struct Deleter {
  protected:
    using Ptr = void (*)(void*);
    Ptr ptr;

    constexpr Deleter() : ptr(nullptr) {}
    explicit Deleter(Ptr p) : ptr(p) {}

  public:
    template <typename T>
    explicit Deleter(TypeTag<T>) : ptr{+[](void* p) -> void { Alloc::del(static_cast<T*>(p)); }} {}

    void del(void* p) const { ptr(p); }
    void operator()(void* p) const { del(p); }
};

template <typename Alloc>
struct DeleterCopier {
  protected:
    using Ptr = void* (*)(Op, void*);
    Ptr ptr;

  public:
    template <typename T>
    explicit DeleterCopier(TypeTag<T>)
          : ptr{+[](Op op, void* p) -> void* {
                switch (op) {
                    case Op::DEL:
                        Alloc::del(static_cast<T*>(p));
                        return nullptr;
                    default:
                        return Alloc::template make<T>(*static_cast<const T*>(p));
                }
            }} {}

    void del(void* p) const { ptr(Op::DEL, p); }
    void operator()(void* p) const { del(p); }
    void* cpy(void* p) const { return this->ptr(Op::CPY, p); }
};

template <Copy C, FunPtr F>
struct MemManagerSelector;

template <>
struct MemManagerSelector<Copy::DISABLED, FunPtr::COMBINED> {
    static constexpr auto Static = detail::mkMemManagerOnePtrStatic;
    static constexpr auto Dynamic = detail::mkMemManagerOnePtrDynamic;
};

template <>
struct MemManagerSelector<Copy::DISABLED, FunPtr::DEDICATED> {
    static constexpr auto Static = detail::mkMemManagerTwoPtrsStatic;
    static constexpr auto Dynamic = detail::mkMemManagerTwoPtrsDynamic;
};

template <>
struct MemManagerSelector<Copy::ENABLED, FunPtr::COMBINED> {
    static constexpr auto Static = detail::mkMemManagerOnePtrCpyStatic;
    static constexpr auto Dynamic = detail::mkMemManagerOnePtrCpyDynamic;
};

template <>
struct MemManagerSelector<Copy::ENABLED, FunPtr::DEDICATED> {
    static constexpr auto Static = detail::mkMemManagerThreePtrsStatic;
    static constexpr auto Dynamic = detail::mkMemManagerThreePtrsDynamic;
};

template <size_t Size>
struct OneChunkAllocator {
    template <typename T>
    static T* make(auto&&... args) {
        if (std::align(alignof(T), sizeof(T), arena.current, arena.sizeLeft)) {
            auto* obj = new (arena.current) T(std::forward<decltype(args)>(args)...);
            arena.sizeLeft -= sizeof(T);
            arena.current = static_cast<char*>(arena.current) + sizeof(T);
            return obj;
        }
        std::terminate();
    }

    static void reset() { arena.reset(); }

    template <typename T>
    static void del(T* obj) noexcept {
        obj->~T();
    }

  private:
    struct Arena {
        Arena() : storage(new char[Size]), current(storage), sizeLeft(Size) {}
        Arena(const Arena&) = delete;
        Arena(Arena&&) = delete;
        Arena& operator=(const Arena&) = delete;
        Arena& operator=(Arena&&) = delete;
        ~Arena() { delete[] storage; }
        void reset() {
            current = storage;
            sizeLeft = Size;
        }
        char* storage;
        void* current;
        size_t sizeLeft;
    };
    inline static Arena arena{};
};

template <typename Storage_, typename R, typename... Args>
class FunBase {
  protected:
    using FunPtr = R (*)(Storage_&, Args...);
    FunPtr funPtr;

  public:
    using Storage = Storage_;
    template <typename F>
    explicit FunBase(F&&)
        requires(!std::is_same_v<std::remove_cvref_t<F>, FunBase>)
          : funPtr{+[](Storage& storage, Args... args) {
                using FnoCv = std::remove_cvref_t<F>;
                static constexpr bool IsConst = std::is_const_v<Storage>;
                using FRef = std::conditional_t<IsConst, const FnoCv&, FnoCv&>;
                return std::invoke(any_cast<FRef>(storage), std::forward<Args>(args)...);
            }} {}
};

template <bool IsNoexcept, typename Storage, typename R, typename... Args>
class ConstFun : public FunBase<const Storage, R, Args...> {
  protected:
    static inline constexpr bool kIsConst = true;

  public:
    using FunBase<const Storage, R, Args...>::FunBase;

    template <typename Self>
    decltype(auto) operator()(this const Self& self, Args... args) noexcept(IsNoexcept) {
        return std::invoke(
            static_cast<const ConstFun*>(&self)->funPtr, self.storage, std::forward<Args>(args)...);
    }
};

template <bool IsNoexcept, typename Storage, typename R, typename... Args>
class NonConstFun : public FunBase<Storage, R, Args...> {
  protected:
    static inline constexpr bool kIsConst = false;

  public:
    using FunBase<Storage, R, Args...>::FunBase;

    template <typename Self>
    decltype(auto) operator()(this Self& self, Args... args) noexcept(IsNoexcept) {
        return std::invoke(
            static_cast<NonConstFun*>(&self)->funPtr, self.storage, std::forward<Args>(args)...);
    }
};

template <typename Storage, typename>
struct MonoFun;

template <typename Storage, typename... Args, typename R>
struct MonoFun<Storage, R(Args...) const noexcept> : detail::ConstFun<true, Storage, R, Args...> {
    using detail::ConstFun<true, Storage, R, Args...>::ConstFun;
};

template <typename Storage, typename... Args, typename R>
struct MonoFun<Storage, R(Args...) const> : detail::ConstFun<false, Storage, R, Args...> {
    using detail::ConstFun<false, Storage, R, Args...>::ConstFun;
};

template <typename Storage, typename... Args, typename R>
struct MonoFun<Storage, R(Args...) noexcept> : detail::NonConstFun<true, Storage, R, Args...> {
    using detail::NonConstFun<true, Storage, R, Args...>::NonConstFun;
};

template <typename Storage, typename... Args, typename R>
struct MonoFun<Storage, R(Args...)> : detail::NonConstFun<false, Storage, R, Args...> {
    using detail::NonConstFun<false, Storage, R, Args...>::NonConstFun;
};

template <typename>
struct MonoFunRef;

template <typename R, typename... Args>
struct MonoFunRef<R(Args...) const> : MonoFun<CRef, R(Args...) const> {
    template <typename F>
    explicit MonoFunRef(const F* f) : MonoFun<CRef, R(Args...) const>{*f} {}
};

template <typename R, typename... Args>
struct MonoFunRef<R(Args...) const noexcept> : MonoFun<CRef, R(Args...) const noexcept> {
    template <typename F>
    explicit MonoFunRef(const F* f) : MonoFun<CRef, R(Args...) const>{*f} {}
};

template <typename R, typename... Args>
struct MonoFunRef<R(Args...)> : MonoFun<Ref, R(Args...)> {
    template <typename F>
    explicit MonoFunRef(F* f) : MonoFun<Ref, R(Args...)>{*f} {}
};

template <typename R, typename... Args>
struct MonoFunRef<R(Args...) noexcept> : MonoFun<Ref, R(Args...) noexcept> {
    template <typename F>
    explicit MonoFunRef(F* f) : MonoFun<Ref, R(Args...)>{*f} {}
};

class FixedString {
    static constexpr inline size_t maxLength = 128;

  public:
    std::array<char, maxLength> data{};

    constexpr FixedString(const char* s) {
        std::copy_n(s, std::char_traits<char>::length(s), &data.front());
    }

    constexpr operator const char*() const { return &data.front(); }
};

constexpr bool operator==(const FixedString& a, const FixedString& b) { return a.data == b.data; }

template <FixedString Name, bool IsConst, typename T>
constexpr inline bool kIsFound = Name == T::Name && IsConst == T::IsConst;

template <FixedString Name, bool IsConst, typename Head, typename... Tail>
struct Contains : std::bool_constant<kIsFound<Name, IsConst, Head>
                                     || Contains<Name, IsConst, Tail...>::value> {};

template <FixedString Name, bool IsConst, typename Head>
struct Contains<Name, IsConst, Head> : std::bool_constant<kIsFound<Name, IsConst, Head>> {};

template <bool Const>
struct RefImpl {
  protected:
    // it's used
    using Obj = std::conditional_t<Const, const void*, void*>;
    [[maybe_unused]] Obj obj;

    struct ConversionTag {};

    RefImpl(ConversionTag, Obj obj) : obj(obj) {}

  public:
    template <typename T>
        requires(Const || !std::is_const_v<T>) explicit RefImpl(T& t) : obj{&t} {}

    template <typename T, typename Self>
    T get(this Self&& self) {
        using TnoRef = std::remove_cvref_t<T>;
        using TP = std::conditional_t<Const, const TnoRef*, TnoRef*>;
        return *static_cast<TP>(self.obj);
    }
};

template <typename M, typename NameT, typename IsConstT, typename ArgsList>
struct OverloadImpl;

template <typename M, typename NameT, typename IsConstT, typename... Args>
struct OverloadImpl<M, NameT, IsConstT, Typelist<Args...>> {
    static M probe(NameT, IsConstT, Args...);
};

template <auto V>
struct ValueTag {};

template <bool IsConst>
struct ConstTag;

template <>
struct ConstTag<false> {};

template <>
struct ConstTag<true> {
    ConstTag(ConstTag<false>) {}
    ConstTag() {}
};

template <typename M>
struct Overload : OverloadImpl<M, ValueTag<M::Name>, ConstTag<M::IsConst>, typename M::Args> {};

template <FixedString Name, bool IsConst, typename ArgList, typename... Ms>
struct FindBest;

template <FixedString Name, bool IsConst, typename... Args, typename... Ms>
struct FindBest<Name, IsConst, Typelist<Args...>, Ms...> : Overload<Ms>... {
    using Overload<Ms>::probe...;

    using type = decltype(probe(ValueTag<Name>{}, ConstTag<IsConst>{}, std::declval<Args>()...));
};

template <FixedString Name, bool IsConst, typename ArgList, typename... Ms>
using FindBestT = FindBest<Name, IsConst, ArgList, Ms...>::type;

template <typename T, bool IsConst>
using ConditionalRef = std::conditional_t<IsConst, const T&, T&>;

template <FixedString Name_,
          auto MethodLam,
          bool IsConst_,
          typename S,
          typename R,
          typename... Args_>
class MethodImpl {
  protected:
    using Ptr = R (*)(detail::ConditionalRef<S, IsConst_>, Args_...);

    Ptr funPtr;

  public:
    template <typename S_>
    using WithStorage = MethodImpl<Name_, MethodLam, IsConst_, S_, R, Args_...>;

    constexpr static inline auto Name = Name_;
    constexpr static inline auto IsConst = IsConst_;
    using Args = Typelist<Args_...>;

    template <typename T>
    MethodImpl(detail::TypeTag<T>)
          : funPtr{+[](detail::ConditionalRef<S, IsConst_> s, Args_... args) -> R {
                static constexpr auto m = MethodLam.template operator()<T>();
                return std::invoke(m,
                                   &any_cast<detail::ConditionalRef<T, IsConst_>>(s),
                                   std::forward<Args_>(args)...);
            }} {}

    decltype(auto) invoke(S& s, Args_&&... args)
        requires(!IsConst_) {
        return std::invoke(funPtr, s, std::forward<Args_&&>(args)...);
    }

    decltype(auto) invoke(const S& s, Args_&&... args) const
        requires(IsConst_) {
        return std::invoke(funPtr, s, std::forward<Args_&&>(args)...);
    }
};

template <FixedString Name_, auto L, typename V, typename F>
struct SealedMethod;

template <FixedString Name_, auto L, typename V, typename R, typename... Args_>
struct SealedMethod<Name_, L, V, R(Args_...)> {
    constexpr static inline auto Name = Name_;
    constexpr static inline auto IsConst = false;
    using Args = Typelist<Args_...>;

    decltype(auto) invoke(V& v, Args_... args) {
        return visit(
            [&args...](auto& obj) { return std::invoke(L, obj, std::forward<Args_>(args)...); }, v);
    }
};

template <FixedString Name_, auto L, typename V, typename R, typename... Args_>
struct SealedMethod<Name_, L, V, R(Args_...) const> {

    constexpr static inline auto Name = Name_;
    constexpr static inline auto IsConst = true;
    using Args = Typelist<Args_...>;

    decltype(auto) invoke(const V& v, Args_... args) const {
        return visit(
            [&args...](const auto& obj) {
                return std::invoke(L, obj, std::forward<Args_>(args)...);
            },
            v);
    }
};

template <typename Storage, typename... Ms>
struct VTable : Ms... {
    template <typename T>
    VTable(TypeTag<T>) : Ms{detail::TypeTag<std::remove_cvref_t<T>>{}}... {}

    VTable() : Ms{}... {}

    template <FixedString Name, typename... Args, typename Self>
    constexpr auto* getMethod(this Self&& self) {
        constexpr bool IsConst = IsConstRef<Self>;
        using Result = RetainConstPtr<Self, FindBestT<Name, IsConst, Typelist<Args...>, Ms...>>;
        return static_cast<Result>(&self);
    }
};

template <typename Storage, typename... Ms>
struct HasVTable {
  private:
    using Table = VTable<Storage, Ms...>;

    template <typename T>
    static inline auto vTableStatic = Table{TypeTag<T>{}};

    Table* vTable;

  public:
    template <typename T>
    HasVTable(TypeTag<T>) {
        vTable = &vTableStatic<T>;
    }

    template <FixedString Name, typename... Args, typename Self>
    constexpr auto* getMethod(this Self&& self) {
        return static_cast<RetainConstPtr<Self, Table>>(self.vTable)
            ->template getMethod<Name, Args...>();
    }
};

template <VTableOwnership O, typename Storage, typename... Ms>
using HasOrIsVTable = std::
    conditional_t<O == VTableOwnership::SHARED, HasVTable<Storage, Ms...>, VTable<Storage, Ms...>>;
} // namespace detail

template <size_t kSize = sizeof(void*),
          Copy kCopy = Copy::ENABLED,
          ExceptionGuarantee kEg = ExceptionGuarantee::NONE,
          size_t kAlignment = alignof(void*),
          FunPtr kFunPtr = FunPtr::COMBINED,
          SafeAnyCast kSafeAnyCast = SafeAnyCast::DISABLED,
          typename Alloc = DefaultAllocator>
struct Any : public detail::Woid<detail::MemManagerSelector<kCopy, kFunPtr>::Static,
                                 detail::MemManagerSelector<kCopy, kFunPtr>::Dynamic,
                                 kSize,
                                 kAlignment,
                                 kEg,
                                 kCopy,
                                 kSafeAnyCast,
                                 Alloc> {
    using detail::Woid<detail::MemManagerSelector<kCopy, kFunPtr>::Static,
                       detail::MemManagerSelector<kCopy, kFunPtr>::Dynamic,
                       kSize,
                       kAlignment,
                       kEg,
                       kCopy,
                       kSafeAnyCast,
                       Alloc>::Woid;
};

template <typename T, typename Storage>
decltype(auto) any_cast(Storage&& s) {
    return std::forward<Storage>(s).template get<T>();
}

template <Copy kCopy = Copy::ENABLED, typename Alloc_ = DefaultAllocator>
class DynamicStorage {
  private:
    static constexpr bool kIsMoveOnly = kCopy == Copy::DISABLED;
    using MM
        = std::conditional_t<kIsMoveOnly, detail::Deleter<Alloc_>, detail::DeleterCopier<Alloc_>>;
    std::unique_ptr<void, MM> storage;

    auto getDeleter() const { return storage.get_deleter(); }

  public:
    inline static constexpr auto kExceptionGuarantee = ExceptionGuarantee::STRONG;
    inline static constexpr auto kStaticStorageSize = 0;
    inline static constexpr auto kStaticStorageAlignment = 0;
    inline static constexpr auto kSafeAnyCast = SafeAnyCast::DISABLED;
    using Alloc = Alloc_;

    constexpr DynamicStorage() : storage(nullptr) {}

    DynamicStorage(const DynamicStorage& other)
        requires(!kIsMoveOnly)
          : storage{other.getDeleter().cpy(other.storage.get()), other.getDeleter()} {}

    DynamicStorage& operator=(const DynamicStorage& other)
        requires(!kIsMoveOnly) {
        *this = DynamicStorage{other};
        return *this;
    }

    template <typename T, typename TnoRef = std::remove_cvref_t<T>>
        requires(!std::is_same_v<std::remove_cvref_t<T>, DynamicStorage>)
    explicit DynamicStorage(T&& t)
          : storage{Alloc::template make<TnoRef>(std::forward<T>(t)),
                    MM{detail::kTypeTag<TnoRef>}} {}

    template <typename T>
    DynamicStorage(TransferOwnership, T* t)
          : storage{t, MM{detail::kTypeTag<std::remove_cvref_t<T>>}} {}

    DynamicStorage& operator=(DynamicStorage&& t) noexcept {
        storage = std::move(t.storage);
        return *this;
    }

    DynamicStorage(DynamicStorage&& t) : storage(std::move(t.storage)) {}

    template <typename T, typename... Args>
    DynamicStorage(std::in_place_type_t<T>, Args&&... args)
          : storage{Alloc::template make<T>(std::forward<Args>(args)...), MM{detail::kTypeTag<T>}} {
    }

    ~DynamicStorage() = default;

    template <typename T, typename Self>
    T get(this Self&& self) {
        return detail::star<T, Self>(std::forward<Self>(self).storage.get());
    }
};

namespace detail {

template <Copy kCopy = Copy::ENABLED, typename Alloc_ = woid::DefaultAllocator>
struct HeapStorage {
  private:
    static constexpr bool kIsMoveOnly = kCopy == Copy::DISABLED;
    void* storage;
    enum Op { DEL, CPY };
    using Ptr = void* (*)(Op, void*);

    struct PtrHolder {
        Ptr ptr;
    };

    template <typename T>
    struct Blk : PtrHolder {
        T t;
        template <typename... Args>
        Blk(Ptr ptr, Args&&... args) : PtrHolder{ptr}, t{std::forward<Args&&>(args)...} {}
    };

  public:
    using Alloc = Alloc_;
    inline static constexpr auto kExceptionGuarantee = ExceptionGuarantee::STRONG;
    inline static constexpr auto kStaticStorageSize = 0;
    inline static constexpr auto kStaticStorageAlignment = 0;
    inline static constexpr auto kSafeAnyCast = SafeAnyCast::DISABLED;

    template <typename T, typename TnoRef = std::remove_cvref_t<T>>
        requires(!std::is_same_v<std::remove_cvref_t<T>, HeapStorage>)
    explicit HeapStorage(T&& t) : HeapStorage{std::in_place_type<TnoRef>, std::forward<T>(t)} {}

    template <typename T, typename... Args>
    explicit HeapStorage(std::in_place_type_t<T>, Args&&... args) {
        Ptr ptr = +[](Op op, void* ptr) -> void* {
            Blk<T>* blk = static_cast<Blk<T>*>(ptr);
            if constexpr (kIsMoveOnly) {
                Alloc::del(blk);
                return nullptr;
            } else {
                if (op == Op::DEL) {
                    Alloc::del(blk);
                    return nullptr;
                }
                return Alloc::template make<Blk<T>>(*blk);
            }
        };

        storage = Alloc::template make<Blk<T>>(ptr, std::forward<Args>(args)...);
    }

    HeapStorage(HeapStorage&& other) noexcept : storage(other.storage) { other.storage = nullptr; }
    HeapStorage& operator=(HeapStorage&& other) noexcept {
        if (this == &other) {
            return *this;
        }

        reset();
        storage = other.storage;
        other.storage = nullptr;
        return *this;
    }

    HeapStorage(const HeapStorage& other)
        requires(!kIsMoveOnly)
          : storage{std::invoke(other.ptr(), Op::CPY, other.storage)} {}
    HeapStorage& operator=(const HeapStorage& other)
        requires(!kIsMoveOnly) {
        *this = HeapStorage{other};

        return *this;
    }

    template <typename T, typename Self>
    T get(this Self&& self) {
        using TnoRef = std::remove_reference_t<T>;
        auto* b
            = reinterpret_cast<RetainConstPtr<Self, Blk<TnoRef>>>(std::forward<Self>(self).storage);
        return std::forward_like<Self>(b->t);
    }

    ~HeapStorage() { reset(); }

  private:
    void reset() {
        if (storage == nullptr)
            return;
        std::invoke(ptr(), Op::DEL, storage);
    }
    auto ptr() const { return reinterpret_cast<const PtrHolder*>(storage)->ptr; }
};

} // namespace detail

template <size_t Size = sizeof(detail::HeapStorage<Copy::ENABLED>),
          Copy kCopy = Copy::ENABLED,
          size_t Alignment = alignof(detail::HeapStorage<Copy::ENABLED>),
          typename HS = detail::HeapStorage<kCopy>>
    requires(Size >= sizeof(HS) && Alignment >= alignof(HS)) class TrivialStorage {
  private:
    bool isOnHeap;
    alignas(Alignment) std::array<char, Size> storage;

    static constexpr bool kIsMoveOnly = kCopy == Copy::DISABLED;

    template <typename T>
    constexpr inline static bool kIsTriviallyRelocatable
        = std::is_trivially_move_constructible_v<T> && std::is_trivially_destructible_v<T>;

    template <typename T>
    constexpr inline static bool kOnHeap
        = sizeof(T) > Size
          || alignof(T) > Alignment
          || !kIsTriviallyRelocatable<T>
          || (!kIsMoveOnly && !std::is_trivially_copy_constructible_v<T>);

  public:
    inline static constexpr auto kExceptionGuarantee = ExceptionGuarantee::STRONG;
    inline static constexpr auto kStaticStorageSize = Size;
    inline static constexpr auto kStaticStorageAlignment = Alignment;
    inline static constexpr auto kSafeAnyCast = SafeAnyCast::DISABLED;
    using Alloc = HS::Alloc;

    template <typename T, typename... Args, typename TnoRef = std::remove_cvref_t<T>>
    TrivialStorage(std::in_place_type_t<T>, Args&&... args) : isOnHeap(kOnHeap<TnoRef>) {
        if constexpr (kOnHeap<TnoRef>) {
            new (&storage) HS{std::in_place_type<TnoRef>, std::forward<Args>(args)...};
        } else {
            new (&storage) T{std::forward<Args>(args)...};
        }
    }

    template <typename T>
    TrivialStorage(TransferOwnership, T* tPtr) : isOnHeap(true) {
        new (&storage) HS{std::move(*tPtr)};
        delete tPtr;
    }

    template <typename T>
        requires(!std::is_same_v<std::remove_cvref_t<T>, TrivialStorage>)
    explicit TrivialStorage(T&& t)
          : TrivialStorage(std::in_place_type<std::remove_cvref_t<T>>, std::forward<T>(t)) {}

    ~TrivialStorage() { reset(); }

    TrivialStorage(const TrivialStorage& other)
        requires(!kIsMoveOnly)
          : isOnHeap(other.isOnHeap) {
        if (isOnHeap) {
            new (&storage) HS{other.getHs()};
        } else {
            storage = other.storage;
        }
    }

    TrivialStorage& operator=(const TrivialStorage& other)
        requires(!kIsMoveOnly) {
        *this = TrivialStorage(other);
        return *this;
    }

    TrivialStorage(TrivialStorage&& other) noexcept : isOnHeap(other.isOnHeap) {
        if (isOnHeap) {
            new (&storage) HS{std::move(other).getHs()};
            other.isOnHeap = false;
        } else {
            this->storage = other.storage;
        }
    }

    TrivialStorage& operator=(TrivialStorage&& other) noexcept {
        if (this != &other) {
            reset();
            isOnHeap = other.isOnHeap;
            if (isOnHeap) {
                new (&storage) HS{std::move(other).getHs()};
            } else {
                this->storage = other.storage;
            }
            other.isOnHeap = false;
        }
        return *this;
    }

    template <typename T, typename Self>
    T get(this Self&& self) {
        using TnoRef = std::remove_cvref_t<T>;
        auto p = const_cast<void*>(std::forward<Self>(self).ptr());
        if constexpr (kOnHeap<TnoRef>) {
            return any_cast<T>(std::forward<Self>(self).getHs());
        }
        return detail::star<T, Self>(p);
    }

  private:
    template <typename Self>
    decltype(auto) getHs(this Self&& self) {
        return std::forward_like<Self>(
            *reinterpret_cast<detail::RetainConstPtr<Self, HS>>(&self.storage.front()));
    }

    void reset() {
        if (isOnHeap) {
            getHs().~HS();
        }
    }

    template <typename Self>
    decltype(auto) ptr(this Self&& self) {
        return detail::ptr<Self>(std::forward<Self>(self).storage);
    }
};

struct Ref : detail::RefImpl<false> {
    using detail::RefImpl<false>::RefImpl;

    friend CRef;
};

struct CRef : detail::RefImpl<true> {
    using detail::RefImpl<true>::RefImpl;

    CRef(Ref ref) : detail::RefImpl<true>{ConversionTag{}, ref.obj} {}
};

namespace detail {

template <size_t Size = 8,
          Copy kCopy = Copy::ENABLED,
          ExceptionGuarantee Eg = ExceptionGuarantee::NONE,
          size_t Alignment = alignof(void*),
          FunPtr kFunPtr = FunPtr::COMBINED,
          SafeAnyCast kSafeAnyCast = SafeAnyCast::DISABLED,
          typename Alloc = DefaultAllocator>
struct AnyBuilderImpl {
  private:
    template <auto V>
    struct UpdateEnum {
        using VType = decltype(V);
        static_assert(std::is_same_v<VType, Copy>
                          || std::is_same_v<VType, ExceptionGuarantee>
                          || std::is_same_v<VType, FunPtr>
                          || std::is_same_v<VType, SafeAnyCast>,
                      "Template parameter V must be an enum constant of type Copy, "
                      "ExceptionGuarantee, FunPtr, or SafeAnyCast.");
        static consteval auto chooseType() {
            if constexpr (std::is_same_v<VType, Copy>) {
                return TypeTag<
                    AnyBuilderImpl<Size, V, Eg, Alignment, kFunPtr, kSafeAnyCast, Alloc>>{};
            }
            if constexpr (std::is_same_v<VType, ExceptionGuarantee>) {
                return TypeTag<
                    AnyBuilderImpl<Size, kCopy, V, Alignment, kFunPtr, kSafeAnyCast, Alloc>>{};
            }
            if constexpr (std::is_same_v<VType, FunPtr>) {
                return TypeTag<
                    AnyBuilderImpl<Size, kCopy, Eg, Alignment, V, kSafeAnyCast, Alloc>>{};
            }
            if constexpr (std::is_same_v<VType, SafeAnyCast>) {
                return TypeTag<AnyBuilderImpl<Size, kCopy, Eg, Alignment, kFunPtr, V, Alloc>>{};
            }
        }

        using Type = decltype(chooseType())::Type;
    };

    template <auto V>
    using With = typename UpdateEnum<V>::Type;

  public:
    template <size_t NewSize>
    using WithSize = AnyBuilderImpl<NewSize, kCopy, Eg, Alignment, kFunPtr, kSafeAnyCast, Alloc>;

    template <size_t NewAlignment>
    using WithAlignment
        = AnyBuilderImpl<Size, kCopy, Eg, NewAlignment, kFunPtr, kSafeAnyCast, Alloc>;

    template <typename NewAlloc>
    using WithAllocator
        = AnyBuilderImpl<Size, kCopy, Eg, Alignment, kFunPtr, kSafeAnyCast, NewAlloc>;

    using EnableCopy = With<Copy::ENABLED>;
    using DisableCopy = With<Copy::DISABLED>;

    using WithNoExceptionGuarantee = With<ExceptionGuarantee::NONE>;
    using WithBasicExceptionGuarantee = With<ExceptionGuarantee::BASIC>;
    using WithStrongExceptionGuarantee = With<ExceptionGuarantee::STRONG>;

    using WithDedicatedFunPtr = With<FunPtr::DEDICATED>;
    using WithCombinedFunPtr = With<FunPtr::COMBINED>;

    using EnableSafeAnyCast = With<SafeAnyCast::ENABLED>;
    using DisableSafeAnyCast = With<SafeAnyCast::DISABLED>;

    using Build = Any<Size, kCopy, Eg, Alignment, kFunPtr, kSafeAnyCast, Alloc>;
};

template <typename T>
struct Free;

template <typename R, typename ArgsTL>
struct MkFunType;

template <typename R, typename... Args>
struct MkFunType<R, Typelist<Args...>> {
    using Type = R(Args...);
    using ConstType = R(Args...) const;
};

template <typename T, typename R_, typename... Args_>
struct Free<R_ (T::*)(Args_...) const> {
    using Type = R_(Args_...);
    using Args = Typelist<Args_...>;
    using StorageArg = Head<Args>;
    using MethodArgs = Tail<Args>;
    using R = R_;
    static inline constexpr bool kIsStorageConst = IsConstRef<StorageArg>;
    using MethodTypeMaker = MkFunType<R, MethodArgs>;
    using MethodType = std::conditional_t<kIsStorageConst,
                                          typename MethodTypeMaker::ConstType,
                                          typename MethodTypeMaker::Type>;
};

template <auto L>
struct MethodType {
    using SomeArbitraryCompleteType = int;
    using Ptr = decltype(&decltype(L)::template operator()<SomeArbitraryCompleteType>);
    using Type = Free<Ptr>::MethodType;
    static inline constexpr bool IsConst = Free<Ptr>::kIsStorageConst;
};

} // namespace detail

using AnyBuilder = detail::AnyBuilderImpl<>;

template <typename Storage, typename... Fs>
    requires(sizeof...(Fs) > 0) struct Fun : detail::MonoFun<Storage, Fs>... {
    std::remove_cv_t<Storage> storage;

    template <typename T>
    explicit Fun(T&& t)
          : detail::MonoFun<Storage, Fs>{std::forward<T>(t)}..., storage{std::forward<T>(t)} {}

    using detail::MonoFun<Storage, Fs>::operator()...;
};

template <typename... Fs>
    requires(sizeof...(Fs) > 0) struct FunRef : detail::MonoFunRef<Fs>... {
  protected:
    static inline constexpr bool kIsConst = (detail::MonoFunRef<Fs>::kIsConst && ...);

  public:
    std::conditional_t<kIsConst, CRef, Ref> storage;

    template <typename T>
    explicit FunRef(T* t) : detail::MonoFunRef<Fs>{t}..., storage{*t} {}

    using detail::MonoFunRef<Fs>::operator()...;
};

template <detail::FixedString Name_, typename S, typename M, auto MethodLam>
class Method;

template <typename S, typename R, typename... Args, detail::FixedString Name_, auto MethodLam>
class Method<Name_, S, R(Args...), MethodLam>
      : public detail::MethodImpl<Name_, MethodLam, false, S, R, Args...> {
  public:
    using detail::MethodImpl<Name_, MethodLam, false, S, R, Args...>::MethodImpl;
};

template <typename S, typename R, typename... Args, detail::FixedString Name_, auto MethodLam>
class Method<Name_, S, R(Args...) const, MethodLam>
      : public detail::MethodImpl<Name_, MethodLam, true, S, R, Args...> {
  public:
    using detail::MethodImpl<Name_, MethodLam, true, S, R, Args...>::MethodImpl;
};

template <VTableOwnership O, typename Storage_, typename... Ms>
struct Interface {
  private:
    detail::HasOrIsVTable<O, Storage_, Ms...> vtable;
    Storage_ storage;

  public:
    using Storage = Storage_;
    constexpr static inline auto kVTableOwnership = O;
    using Self = Interface;

    template <detail::FixedString Name, typename... Args, typename Self>
    constexpr inline decltype(auto) call(this Self&& self, Args&&... args) {
        return self.vtable.template getMethod<Name, Args&&...>()->invoke(
            self.storage, std::forward<Args&&>(args)...);
    }

    template <typename T>
    Interface(T&& t)
        requires(!std::is_same_v<std::remove_cvref_t<T>, Interface>)
          : vtable{detail::TypeTag<T>{}}, storage{std::forward<T>(t)} {}

    template <typename T, typename... Args>
    Interface(std::in_place_type_t<T> tag, Args&&... args)
          : vtable{detail::TypeTag<T>{}}, storage{tag, std::forward<Args>(args)...} {}
};

template <typename Variant, typename... Ms>
struct SealedInterface {
  private:
    [[no_unique_address]] detail::VTable<Variant, Ms...> table;
    Variant v;

  public:
    using Storage = Variant;

    using Self = SealedInterface;

    template <detail::FixedString Name, typename... Args, typename Self>
    constexpr inline decltype(auto) call(this Self&& self, Args&&... args) {
        return self.table.template getMethod<Name, Args&&...>()->invoke(
            self.v, std::forward<Args&&>(args)...);
    }

    template <typename T>
    SealedInterface(T&& t)
        requires(!std::is_same_v<std::remove_cvref_t<T>, SealedInterface>)
          : table{}, v{std::forward<T>(t)} {}

    template <typename T, typename... Args>
    SealedInterface(std::in_place_type_t<T> tag, Args&&... args)
          : table{}, v{tag, std::forward<Args>(args)...} {}
};

namespace detail {
template <VTableOwnership O = VTableOwnership::DEDICATED,
          typename Storage_ = Any<8>,
          typename... Ms>
struct InterfaceBuilderImpl {
    template <VTableOwnership NewO>
    using With = InterfaceBuilderImpl<NewO, Storage_, Ms...>;

    using WithSharedVTable = InterfaceBuilderImpl<VTableOwnership::SHARED, Storage_, Ms...>;
    using WithDedicatedVTable = InterfaceBuilderImpl<VTableOwnership::DEDICATED, Storage_, Ms...>;

    template <typename S>
    using WithStorage = InterfaceBuilderImpl<O, S, typename Ms::template WithStorage<S>...>;

    template <detail::FixedString Name, typename M, auto MethodLam>
    using Method
        = InterfaceBuilderImpl<O, Storage_, Ms..., woid::Method<Name, Storage_, M, MethodLam>>;

    template <detail::FixedString Name, auto L>
    using Fun = Method<Name, typename detail::MethodType<L>::Type, []<typename T> {
        using ObjPtr = std::conditional_t<detail::MethodType<L>::IsConst, const T*, T*>;
        return [](ObjPtr obj, auto&&... args) {
            return std::invoke(L, *obj, std::forward<decltype(args)>(args)...);
        };
    }>;

    using Build = Interface<O, Storage_, Ms...>;
};

template <typename Variant, typename... Ms>
struct SealedInterfaceBuilderImpl {
  private:
    template <detail::FixedString Name, auto L>
    using M = SealedMethod<Name, L, Variant, typename MethodType<L>::Type>;

  public:
    template <detail::FixedString Name, auto L>
    using Fun = SealedInterfaceBuilderImpl<Variant, Ms..., M<Name, L>>;

    using Build = SealedInterface<Variant, Ms...>;
};

} // namespace detail

template <class... Ts>
struct Overloads : Ts... {
    using Ts::operator()...;
};

using InterfaceBuilder = detail::InterfaceBuilderImpl<>;

template <typename Variant>
using SealedInterfaceBuilder = detail::SealedInterfaceBuilderImpl<Variant>;

} // namespace woid WOID_SYMBOL_VISIBILITY_FLAG
