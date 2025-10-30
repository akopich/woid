#pragma once

#include <array>
#include <functional>
#include <memory>
#include <type_traits>
#include <utility>

#define SUPPRESS_SWITCH_WARNING_START                                                              \
    _Pragma("GCC diagnostic push") _Pragma("GCC diagnostic ignored \"-Wswitch\"")
#define SUPPRESS_SWITCH_WARNING_END _Pragma("GCC diagnostic pop")

namespace woid {

enum class ExceptionGuarantee { NONE, BASIC, STRONG };

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
constexpr inline auto delStatic = [](void* ptr) { static_cast<T*>(ptr)->~T(); };

template <typename T>
constexpr inline auto movStatic = [](void* src, void* dst) {
    new (dst) T(std::move(*static_cast<T*>(src)));
    static_cast<T*>(src)->~T();
};

template <typename T>
constexpr inline auto cpyStatic = [](void* src, void* dst) { new (dst) T(*static_cast<T*>(src)); };

constexpr inline auto mkMemManagerTwoPtrsStatic = []<typename T>(TypeTag<T>) consteval {
    return MemManagerTwoPtrs(delStatic<T>, movStatic<T>);
};

constexpr inline auto mkMemManagerThreePtrsStatic = []<typename T>(TypeTag<T>) consteval {
    return MemManagerThreePtrs{{delStatic<T>, movStatic<T>}, cpyStatic<T>};
};

template <typename T>
constexpr inline auto delDynamic = [](void* ptr) { delete *static_cast<T**>(ptr); };

template <typename T>
constexpr inline auto movDynamic = [](void* src, void* dst) {
    *static_cast<T**>(dst) = *static_cast<T**>(src);
    *static_cast<T**>(src) = nullptr;
};

template <typename T>
constexpr inline auto cpyDynamic = [](void* src, void* dst) {
    auto* newPtr = new T(**static_cast<T**>(src));
    *static_cast<T**>(dst) = newPtr;
};

constexpr inline auto mkMemManagerTwoPtrsDynamic = []<typename T>(TypeTag<T>) consteval {
    return MemManagerTwoPtrs{
        delDynamic<T>,
        movDynamic<T>,
    };
};

constexpr inline auto mkMemManagerThreePtrsDynamic = []<typename T>(TypeTag<T>) consteval {
    return MemManagerThreePtrs{{delDynamic<T>, movDynamic<T>}, cpyDynamic<T>};
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
    return MemManagerOnePtr{+[](Op op, void* ptr, void* dst) -> void {
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
    return MemManagerOnePtrCpy{{+[](Op op, void* ptr, void* dst) -> void {
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

constexpr inline auto mkMemManagerOnePtrStatic = []<typename T>(TypeTag<T>) consteval {
    return mkMemManagerOnePtrFromLambdas(delStatic<T>, movStatic<T>);
};

constexpr inline auto mkMemManagerOnePtrDynamic = []<typename T>(TypeTag<T>) consteval {
    return mkMemManagerOnePtrFromLambdas(delDynamic<T>, movDynamic<T>);
};

constexpr inline auto mkMemManagerOnePtrCpyStatic = []<typename T>(TypeTag<T>) consteval {
    return mkMemManagerOnePtrCpyFromLambdas(delStatic<T>, movStatic<T>, cpyStatic<T>);
};

constexpr inline auto mkMemManagerOnePtrCpyDynamic = []<typename T>(TypeTag<T>) consteval {
    return mkMemManagerOnePtrCpyFromLambdas(delDynamic<T>, movDynamic<T>, cpyDynamic<T>);
};

template <typename T, typename Self, typename Void>
T star(Void* p) {
    return static_cast<T>(*static_cast<RetainConstPtr<Self, std::remove_reference_t<T>>>(p));
}

template <typename MemManager,
          auto mmStaticMaker,
          auto mmDynamicMaker,
          std::size_t Size,
          ExceptionGuarantee Eg,
          bool IsMoveOnly>
    requires(Size >= sizeof(void*)) class StaticStorage {
  private:
    template <typename T>
    inline static constexpr bool kIsBig
        = sizeof(T) > Size
          || alignof(T) > alignof(void*)
          || (Eg == ExceptionGuarantee::STRONG && !std::is_nothrow_move_constructible_v<T>);

  public:
    inline static constexpr auto exceptionGuarantee = Eg;

    template <typename T>
    explicit StaticStorage(T&& t)
          : StaticStorage(std::in_place_type<std::remove_cvref_t<T>>, std::forward<T>(t)) {}

    template <typename T, typename... Args>
    StaticStorage(std::in_place_type_t<T>, Args... args) {
        if constexpr (kIsBig<T>) {
            static constinit auto mm = mmDynamicMaker(kTypeTag<T>);
            this->mm = &mm;
            auto* obj = new T(std::forward<Args>(args)...);
            *static_cast<void**>(ptr()) = obj;
        } else {
            static constinit auto mm = mmStaticMaker(kTypeTag<T>);
            this->mm = &mm;
            new (ptr()) T(std::forward<Args>(args)...);
        }
    }
    StaticStorage(const StaticStorage& other)
        requires(!IsMoveOnly)
          : mm(other.mm) {
        mm->cpy(const_cast<void*>(other.ptr()), ptr());
    }
    StaticStorage& operator=(const StaticStorage& other)
        requires(!IsMoveOnly) {
        if constexpr (Eg == ExceptionGuarantee::STRONG) {
            *this = StaticStorage{other};
        } else {
            if (mm != nullptr)
                mm->del(ptr());
            if constexpr (Eg == ExceptionGuarantee::BASIC)
                mm = nullptr;
            other.mm->cpy(const_cast<void*>(other.ptr()), ptr());
            mm = other.mm;
        }
        return *this;
    }
    StaticStorage(StaticStorage&& other) noexcept(Eg != ExceptionGuarantee::BASIC) : mm(other.mm) {
        mm->move(other.ptr(), ptr());
        other.mm = nullptr;
    }
    StaticStorage& operator=(StaticStorage&& other) noexcept(Eg != ExceptionGuarantee::BASIC) {
        if (mm != nullptr)
            mm->del(ptr());
        if constexpr (Eg == ExceptionGuarantee::BASIC)
            mm = nullptr;
        other.mm->move(other.ptr(), ptr());
        mm = other.mm;
        other.mm = nullptr;
        return *this;
    }
    ~StaticStorage() {
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
    alignas(void*) std::array<char, Size> storage;
    MemManager* mm;

    template <typename Self>
    decltype(auto) ptr(this Self&& self) {
        return static_cast<RetainConstPtr<Self, void>>(&std::forward<Self>(self).storage.front());
    }
};

struct Deleter {
  private:
    using DeletePtr = void (*)(void*);

  public:
    constexpr Deleter() : deletePtr(nullptr) {}

    template <typename T>
    Deleter(TypeTag<T>) : deletePtr(+[](void* ptr) { delete static_cast<T*>(ptr); }) {}

    void del(void* p) const { std::invoke(deletePtr, p); }

    void operator()(void* p) const { del(p); }

    DeletePtr deletePtr;
};

class DynamicStorage {
  private:
    std::unique_ptr<void, Deleter> storage;

  public:
    inline static constexpr auto exceptionGuarantee = ExceptionGuarantee::STRONG;

    constexpr DynamicStorage() : storage(nullptr) {}

    DynamicStorage(const DynamicStorage&) = delete;
    DynamicStorage& operator=(const DynamicStorage&) = delete;

    template <typename T, typename TnoRef = std::remove_cvref_t<T>>
    DynamicStorage(T&& t) : storage{new TnoRef(std::forward<T>(t)), Deleter{kTypeTag<TnoRef>}} {}

    DynamicStorage& operator=(DynamicStorage&& t) noexcept {
        storage = std::move(t.storage);
        return *this;
    }

    DynamicStorage(DynamicStorage&& t) : storage(std::move(t.storage)) {}

    template <typename T, typename... Args>
    DynamicStorage(std::in_place_type_t<T>, Args&&... args)
          : storage{new T(std::forward<Args>(args)...), Deleter{kTypeTag<T>}} {}

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
} // namespace detail

template <size_t Size, ExceptionGuarantee Eg = ExceptionGuarantee::NONE>
using AnyOnePtr = detail::StaticStorage<detail::MemManagerOnePtr,
                                        detail::mkMemManagerOnePtrStatic,
                                        detail::mkMemManagerOnePtrDynamic,
                                        Size,
                                        Eg,
                                        /*IsMoveOnly=*/true>;

template <size_t Size, ExceptionGuarantee Eg = ExceptionGuarantee::NONE>
using AnyTwoPtrs = detail::StaticStorage<detail::MemManagerTwoPtrs,
                                         detail::mkMemManagerTwoPtrsStatic,
                                         detail::mkMemManagerTwoPtrsDynamic,
                                         Size,
                                         Eg,
                                         /*IsMoveOnly=*/true>;

template <size_t Size, ExceptionGuarantee Eg = ExceptionGuarantee::NONE>
using AnyThreePtrs = detail::StaticStorage<detail::MemManagerThreePtrs,
                                           detail::mkMemManagerThreePtrsStatic,
                                           detail::mkMemManagerThreePtrsDynamic,
                                           Size,
                                           Eg,
                                           /*IsMoveOnly=*/false>;

template <size_t Size, ExceptionGuarantee Eg = ExceptionGuarantee::NONE>
using AnyOnePtrCpy = detail::StaticStorage<detail::MemManagerOnePtrCpy,
                                           detail::mkMemManagerOnePtrCpyStatic,
                                           detail::mkMemManagerOnePtrCpyDynamic,
                                           Size,
                                           Eg,
                                           /*IsMoveOnly=*/false>;

} // namespace woid
