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

## Benchmarking
I promised you performance. To run the benchmarks you would need to pull the libraries we bench against, namely [`boost::te`](https://github.com/boost-ext/te) and [`microsoft/proxy`](https://github.com/microsoft/proxy) with
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
| **FunBench** | `woid::Fun` | `std::function`<br> plain lambda | Passing callables to `std::sort` |
| **InterfaceBench** | `woid::InterfaceBuilder` | `virtual` functions <br>  [`boost::te`](https://github.com/boost-ext/te) <br> [`microsoft/proxy`](https://github.com/microsoft/proxy) | Storing polymorphic objects in a `std::vector`, calling `std::sort` and `std::min_element` |


On my hardware (i9-10850K CPU @ 3.60GHz) `woid` *ranks first* in most cases.
