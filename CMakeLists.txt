cmake_minimum_required(VERSION 3.21)
project(woid)

add_library(te INTERFACE)
target_include_directories(te INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/te/include"
)

add_library(proxy INTERFACE)
target_include_directories(proxy INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/proxy/include"
)

add_library(function2 INTERFACE)
target_include_directories(function2 INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/fu2/include"
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SHUFFLE_SEED "Seed for ASLR." OFF)
option(CC_SELECTION "Select the compiler family (e.g., clang or gcc)." "clang")

if (CC_SELECTION STREQUAL "clang")
    set(CMAKE_C_COMPILER "clang")
    set(CMAKE_CXX_COMPILER "clang++")
    message(STATUS "Using Clang compiler family.")
elseif (CC_SELECTION STREQUAL "gcc")
    set(CMAKE_C_COMPILER "gcc")
    set(CMAKE_CXX_COMPILER "g++")
    message(STATUS "Using GCC compiler family.")
else()
    message(FATAL_ERROR "Unknown compiler selection: ${CC_SELECTION}. Use 'clang' or 'gcc'.")
endif()

add_compile_options("-Wall" "-Wextra" "-pedantic")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug-specific flags
    add_compile_options("-fno-exceptions" "-g3" "-O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "DebugWithExceptions")
    add_compile_options("-fexceptions" "-g3" "-O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-fno-exceptions" "-O3" "-DNDEBUG" "-fno-rtti")
elseif(CMAKE_BUILD_TYPE STREQUAL "ReleaseNative")
    add_compile_options("-fno-exceptions" "-O3" "-DNDEBUG" "-fno-rtti" "-march=native")
elseif(CMAKE_BUILD_TYPE STREQUAL "ReleaseWithExceptions")
    add_compile_options("-O3" "-DNDEBUG" "-fno-rtti")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # Profiling/Release with Debug Info flags
    add_compile_options("-fno-exceptions" "-fno-omit-frame-pointer" "-g" "-O3" "-fno-rtti")
elseif(CMAKE_BUILD_TYPE STREQUAL "FastBuild")
    add_compile_options( "-O0" "-g0" "-fno-exceptions" "-fno-rtti")
elseif(CMAKE_BUILD_TYPE STREQUAL "FastBuildWithExceptions")
    add_compile_options( "-O0" "-g0" "-fno-rtti")
elseif(CMAKE_BUILD_TYPE STREQUAL "AUBSan")
    add_compile_options("-fno-exceptions" "-g" "-O1"
                        "-fsanitize=address,undefined"
                        "-fno-omit-frame-pointer"
                        "-fno-sanitize-recover=all")
    link_libraries("-fsanitize=address,undefined")
elseif(CMAKE_BUILD_TYPE STREQUAL "AUBSanWithExceptions")
    add_compile_options("-g" "-O1"
                        "-fsanitize=address,undefined"
                        "-fno-omit-frame-pointer"
                        "-fno-sanitize-recover=all")
    link_libraries("-fsanitize=address,undefined")
else()
    # Default flags if CMAKE_BUILD_TYPE isn't specified
    add_compile_options("-fno-exceptions" "-g" "-O0" "-fno-rtti")
endif()

find_package(Boost 1.76 REQUIRED CONFIG)
find_package(benchmark REQUIRED)
find_package(GTest REQUIRED)

include_directories(include)

add_executable(CopyTest test/copy_test.cpp)
target_link_libraries(CopyTest GTest::gtest_main)

add_executable(MoveOnlyTest test/move_only_test.cpp)
target_link_libraries(MoveOnlyTest GTest::gtest_main)

add_executable(FunTest test/fun_test.cpp)
target_link_libraries(FunTest GTest::gtest_main)

add_executable(InterfaceTest test/interface_test.cpp)
target_link_libraries(InterfaceTest GTest::gtest_main)

add_executable(MoveOnlyBench bench/move_only_bench.cpp)
target_link_libraries(MoveOnlyBench benchmark::benchmark)

add_executable(CopyBench bench/copy_bench.cpp)
target_link_libraries(CopyBench benchmark::benchmark)

add_executable(FunBench bench/fun_bench.cpp)
target_link_libraries(FunBench benchmark::benchmark function2)

add_executable(InterfaceBench bench/interface_bench.cpp)
target_link_libraries(InterfaceBench benchmark::benchmark te proxy)

add_library(CrossTuLib SHARED test/cross_tu_lib.cpp)

target_compile_options(CrossTuLib PRIVATE -fvisibility=hidden)

add_executable(CrossTuTest test/cross_tu_test.cpp)
target_link_libraries(CrossTuTest
    GTest::gtest_main
    CrossTuLib
)
target_link_options(CrossTuTest PRIVATE
    -Wl,-rpath,'$ORIGIN'
)

if (SHUFFLE_SEED)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--shuffle-sections=.*=${SHUFFLE_SEED}")
else()
    # Provide a default or message when the variable isn't set
    message(STATUS "SHUFFLE_SEED not defined. Using default linker behavior.")
endif()
