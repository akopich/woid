#pragma once

#include <array>
#include <cstdlib>
#include <exception>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#define SUPPRESS_SWITCH_WARNING_START                                                              \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wswitch\"")
#define SUPPRESS_SWITCH_WARNING_END _Pragma("GCC diagnostic pop")

namespace woid {

enum class ExceptionGuarantee { NONE, BASIC, STRONG };
enum class Copy { ENABLED, DISABLED };
enum class FunPtr { COMBINED, DEDICATED };

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

namespace detail {

template <typename T>
class TypeTag {
    using Type = T;
};

template <typename T>
inline constexpr TypeTag<T> kTypeTag{};

template <typename S, typename T>
using RetainConstPtr
    = std::conditional_t<std::is_const_v<std::remove_reference_t<S>>, const T*, T*>;

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
consteval auto mkMemManagerOnePtrFromLambdas(Del, Mov) {
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

template <auto mmStaticMaker,
          auto mmDynamicMaker,
          std::size_t Size,
          std::size_t Alignment,
          ExceptionGuarantee Eg,
          Copy copy,
          typename Alloc_>
    requires(Size >= sizeof(void*)) class Woid {
  private:
    static constexpr bool kIsMoveOnly = copy == Copy::DISABLED;

    using MemManager = GetMemManager<mmStaticMaker>;
    static_assert(std::is_same_v<MemManager, GetMemManager<mmStaticMaker>>);

    template <typename T>
    inline static constexpr bool kIsBig
        = sizeof(T) > Size
          || alignof(T) > Alignment
          || (Eg == ExceptionGuarantee::STRONG && !std::is_nothrow_move_constructible_v<T>);

  public:
    inline static constexpr auto kExceptionGuarantee = Eg;
    inline static constexpr auto kStaticStorageSize = Size;
    inline static constexpr auto kStaticStorageAlignment = Alignment;
    using Alloc = Alloc_;

    template <typename T>
    explicit Woid(T&& t) : Woid(std::in_place_type<std::remove_cvref_t<T>>, std::forward<T>(t)) {}

    template <typename T>
    explicit Woid(TransferOwnership, T* tPtr)
        requires(kIsBig<T> && !std::is_const_v<T>) {
        static constinit auto mm = mmDynamicMaker(kTypeTag<T>, kTypeTag<Alloc>);
        this->mm = &mm;
        *static_cast<void**>(ptr()) = tPtr;
    }

    template <typename T, typename... Args>
    explicit Woid(std::in_place_type_t<T>, Args... args) {
        if constexpr (kIsBig<T>) {
            static constinit auto mm = mmDynamicMaker(kTypeTag<T>, kTypeTag<Alloc>);
            this->mm = &mm;
            auto* obj = Alloc::template make<T>(std::forward<Args>(args)...);
            *static_cast<void**>(ptr()) = obj;
        } else {
            static constinit auto mm = mmStaticMaker(kTypeTag<T>);
            this->mm = &mm;
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
        if constexpr (Eg == ExceptionGuarantee::STRONG) {
            *this = Woid{other};
        } else {
            if (this == &other)
                return *this;
            if (mm != nullptr)
                mm->del(ptr());
            if constexpr (Eg == ExceptionGuarantee::BASIC)
                mm = nullptr;
            other.mm->cpy(const_cast<void*>(other.ptr()), ptr());
            mm = other.mm;
        }
        return *this;
    }
    Woid(Woid&& other) noexcept(Eg != ExceptionGuarantee::BASIC) : mm(other.mm) {
        mm->move(other.ptr(), ptr());
        other.mm = nullptr;
    }
    Woid& operator=(Woid&& other) noexcept(Eg != ExceptionGuarantee::BASIC) {
        if (this == &other)
            return *this;
        if (mm != nullptr)
            mm->del(ptr());
        if constexpr (Eg == ExceptionGuarantee::BASIC)
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

        auto p = const_cast<void*>(std::forward<Self>(self).ptr());
        if constexpr (kIsBig<TnoRef>) {
            p = *static_cast<void**>(p);
        }
        return star<T, Self>(p);
    }

  private:
    alignas(Alignment) std::array<char, Size> storage;
    MemManager* mm;

    template <typename Self>
    decltype(auto) ptr(this Self&& self) {
        return static_cast<RetainConstPtr<Self, void>>(&std::forward<Self>(self).storage.front());
    }
};

template <typename Alloc>
struct Deleter {
  private:
    using DeletePtr = void (*)(void*);

  public:
    constexpr Deleter() : deletePtr(nullptr) {}

    template <typename T>
    explicit Deleter(TypeTag<T>)
          : deletePtr(+[](void* ptr) static { Alloc::del(static_cast<T*>(ptr)); }) {}

    void del(void* p) const { std::invoke(deletePtr, p); }

    void operator()(void* p) const { del(p); }

    DeletePtr deletePtr;
};

template <typename Alloc_ = DefaultAllocator>
class DynamicStorage {
  private:
    std::unique_ptr<void, Deleter<Alloc_>> storage;

  public:
    inline static constexpr auto kExceptionGuarantee = ExceptionGuarantee::STRONG;
    inline static constexpr auto kStaticStorageSize = 0;
    inline static constexpr auto kStaticStorageAlignment = 0;
    using Alloc = Alloc_;

    constexpr DynamicStorage() : storage(nullptr) {}

    DynamicStorage(const DynamicStorage&) = delete;
    DynamicStorage& operator=(const DynamicStorage&) = delete;

    template <typename T, typename TnoRef = std::remove_cvref_t<T>>
    DynamicStorage(T&& t)
          : storage{Alloc::template make<TnoRef>(std::forward<T>(t)),
                    Deleter<Alloc_>{kTypeTag<TnoRef>}} {}

    template <typename T>
    DynamicStorage(TransferOwnership, T* t)
          : storage{t, Deleter<Alloc_>{kTypeTag<std::remove_cvref_t<T>>}} {}

    DynamicStorage& operator=(DynamicStorage&& t) noexcept {
        storage = std::move(t.storage);
        return *this;
    }

    DynamicStorage(DynamicStorage&& t) : storage(std::move(t.storage)) {}

    template <typename T, typename... Args>
    DynamicStorage(std::in_place_type_t<T>, Args&&... args)
          : storage{Alloc::template make<T>(std::forward<Args>(args)...),
                    Deleter<Alloc_>{kTypeTag<T>}} {}

    ~DynamicStorage() = default;

    template <typename T, typename Self>
    T get(this Self&& self) {
        return star<T, Self>(std::forward<Self>(self).storage.get());
    }
};

template <typename T, typename Storage>
decltype(auto) any_cast(Storage&& s) {
    return std::forward<Storage>(s).template get<T>();
}

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
        if (std::align(alignof(T), sizeof(T), current, sizeLeft)) {
            if (sizeLeft < sizeof(T))
                std::terminate();
            auto* obj = new (current) T(std::forward<decltype(args)>(args)...);
            sizeLeft -= sizeof(T);
            current = static_cast<char*>(current) + sizeof(T);
            return obj;
        }
        std::terminate();
    }

    template <typename T>
    static void del(T* obj) noexcept {
        obj->~T();
    }

    static void reset() {
        current = storage;
        sizeLeft = Size;
    }

  private:
    static void cleanup() { delete[] storage; }
    inline static char* storage = [] {
        char* result = new char[Size];
        std::atexit(&OneChunkAllocator::cleanup);
        return result;
    }();
    inline static void* current = storage;
    inline static size_t sizeLeft = Size;
};

} // namespace detail

template <size_t Size,
          Copy kCopy = Copy::ENABLED,
          ExceptionGuarantee Eg = ExceptionGuarantee::NONE,
          size_t Alignment = alignof(void*),
          FunPtr kFunPtr = FunPtr::COMBINED,
          typename Alloc = DefaultAllocator>
struct Any : public detail::Woid<detail::MemManagerSelector<kCopy, kFunPtr>::Static,
                                 detail::MemManagerSelector<kCopy, kFunPtr>::Dynamic,
                                 Size,
                                 Alignment,
                                 Eg,
                                 kCopy,
                                 Alloc> {
    using detail::Woid<detail::MemManagerSelector<kCopy, kFunPtr>::Static,
                       detail::MemManagerSelector<kCopy, kFunPtr>::Dynamic,
                       Size,
                       Alignment,
                       Eg,
                       kCopy,
                       Alloc>::Woid;
};
} // namespace woid
