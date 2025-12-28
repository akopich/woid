# Woid

**Step into the woid.**

`woid` is a high-performance single-header-only C++ library for **type erasure** and **non-intrusive polymorphism**.

## Key features
- Value semantics
- Duck typing
- Performance
- Extreme customizability

## Components

| Component | Better version of... |
| :--- | :--- |
| `woid::Any` and friends | `std::any` |
| `woid::Fun` / `woid::FunRef` | `std::function` |
| `woid::InterfaceBuilder` | `virtual` functions and inheritance |


##  Usage Example: Non-Intrusive Interfaces
```cpp
#include <woid.hpp> // Requires C++23, tested with Clang 21 and GCC 15

struct Circle {
    double radius;
    double area() const { return std::numbers::pi * radius * radius; }
};
struct Square {
    double side;
    double area() const { return side * side; }
};
struct Shape : woid::InterfaceBuilder
           ::Fun<"area", [](const auto& obj) -> double { return obj.area(); } >
           ::Build {
    auto area() const { return call<"area">(); }
};

void printArea(const Shape& s) {
    std::println("Area {}", s.area());
}

printArea(Shape{Circle{1.5}});
printArea(Shape{Square{1.5}});
```

## Getting it

```bash
git clone https://github.com/akopich/woid.git
```
`woid` is header-only. So just move the only header file the library consists of (`include/woid.hpp`) into your project's include path.

## Components
### Storages
`Woid` provides a number of storages.

The *owning* storages can be instantiated with a universal reference or the object can be created in-place e.g.
```cpp
Circle c{3.15};
woid::Any<> any{c};
woid::Any<> sameAny{std::move(c)};
woid::Any<> identicalAny{std::in_place_type<Circle>, c};
```

The *non-owning* storages can be constructed with lvalue-ref
```cpp
woid::Ref cr{c};
```

What the storages all have in common is the `woid::any_cast` function that can be used to extract the stored value
```cpp
any_cast<T>(storage);              // returns T
any_cast<T&>(storage);             // returns T&
any_cast<const T&>(storage);       // returns const T&
any_cast<T&&>(std::move(storage)); // returns T&&
```
#### `woid::Any`

`woid::Any` is a general-purpose *owning* type-erasing container. The type accepts 7 template parameters so for the sake of sanity preservation we also provide `woid::AnyBuilder`. The defaults are
```cpp
using ActualAny = AnyBuilder
                        ::WithSize<sizeof(void*)>
                        ::WithAlignment<alignof(void*)>
                        ::EnableCopy
                        ::WithNoExceptionGuarantee
                        ::DisableSafeAnyCast
                        ::WithCombinedFunPtr
                        ::WithAllocator<woid::DefaultAllocator>
                        ::Build;
static_assert(std::is_same_v<ActualAny, Any<>>);
```

- `kSize`/`kAlignment` The size and alignment (in bytes) of the internal storage used for the Small Buffer Optimization (SBO).
- `kCopy` Whether the storage supports the copy-construction and copy-assignment operations. When `Copy::DISABLED` is passed, a move-only object can be stored.
- `kEg` The level of exception guarantee provided. This drives the way we implement the copy and move assignment. Naturally, the higher guarantee comes with a performance cost.
    -  `ExceptionGuarantee::NONE` UB is triggered if the stored object's copy/move constructor throws.
    -  `ExceptionGuarantee::BASIC` If the stored object's copy/move constructor throws, the state of the operands is valid.
    - `ExceptionGuarantee::STRONG` If the stored object's copy/move constructor throws, the state of the operands before the assignment operation is restored. Also fails the SBO when the stored object is not `nothrow_move_constructible`.
- `kFunPtr` Defines the way we store pointers to the special member functions of the stored object. With `FunPtr::DEDICATED` we store one function pointer for each (which may be faster) while with `Fun::Ptr::COMBINED` we only store one and do some branching therein (which surely saves space).
- `kSafeAnyCast` When `DISABLED`, `any_cast` triggers UB if the requested type does not match the type of the stored object. Otherwise `woid::BadAnyCast` is thrown. See the comment above `woid::SafeAnyCast` definition for details.
- `Alloc` An allocator we request the memory from if SBO fails. *Note*, it is not `std::allocator`.

#### `woid::TrivialStorage`
 `woid::TrivialStorage` is another *owning* storage similar to `woid::Any` in that it utilizes SBO (again, configured via `kSize`/`kAlignment` template parameters). Its performance is tuned for the trivial objects. A non-trivial object **can** be stored, but the SBO fails if the object is not trivially movable or trivially destructible. Additionally, if copying is enabled via the `kCopy` parameter, the object must also be trivially copyable to qualify for SBO.

#### `woid::DynamicStorage`
`woid::DynamicStorage` is a *move-only owning* type-erased container that does not bother with the SBO. Provides strong exception guarantee.

#### `woid::Ref`/`woid::CRef`
These two are *non-owning* containers essentially being wrappers over `void*` and `const void*` respectively.

## Benchmarking
I promised you performance. To run the benchmarks you would need to pull the libraries we bench against, namely [`function2`](https://github.com/Naios/function2), [`boost::te`](https://github.com/boost-ext/te) and [`microsoft/proxy`](https://github.com/microsoft/proxy) with
```bash
git submodule update --init --recursive
```
We also have a dependency on [google/benchmark](https://github.com/google/benchmark).

Further you would need `Python` and some packages installed. The desired setup is (relatively) easy to achieve with `venv`
```bash
python -m venv benchvenv
source benchvenv/bin/activate
pip install -r bench/requirements.txt
```

Now you're ready to verify the setup with a dry run

```bash
python bench/plot.py --target MoveOnlyBench --reps 1 --seed 10 --mode show -- --benchmark_dry_run
```
This should build the `MoveOnlyBench` target, run it and plot the sanity-check-only measurements. Alternatively you can pass `--mode save` for the plots to be saved on the disk.

If this worked, drop the dry-run flag
```bash
python bench/plot.py --target MoveOnlyBench --reps 1 --seed 10 --mode show
```

The benchmark targets:
| Target | `woid` Component | Comparison Targets | Bench Scenario |
| :--- | :--- | :--- | :--- |
| **MoveOnlyBench** | `woid::Any`, `TrivialStorage` | `std::any` | Compares `woid::Any` and `woid::TrivialStorage` vs `std::any` in a move-intensive workflow, namely array sorting |
| **CopyBench** | `woid::Any`, `TrivialStorage` | `std::any` | Same as above but we force the copy instead of moves. |
| **FunBench** | `woid::Fun` | `std::function`<br>`function2`<br> plain lambda | Passing callables to `std::sort` |
| **InterfaceBench** | `woid::InterfaceBuilder` | `virtual` functions <br>  [`boost::te`](https://github.com/boost-ext/te) <br> [`microsoft/proxy`](https://github.com/microsoft/proxy) | Storing polymorphic objects in a `std::vector`, calling `std::sort` and `std::min_element` |


On my hardware (i9-10850K CPU @ 3.60GHz) `woid` *ranks first* in most cases.
