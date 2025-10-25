cmake_minimum_required(VERSION 3.21)
project(woid)

set(CMAKE_CXX_STANDARD 26)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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

add_compile_options("-Wall" "-Wextra" "-pedantic" "-fno-rtti")

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    # Debug-specific flags
    add_compile_options("-fno-exceptions" "-g3" "-O0")
elseif(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_compile_options("-fno-exceptions" "-O3" "-DNDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "ReleaseWithExceptions")
    add_compile_options("-O3" "-DNDEBUG")
elseif(CMAKE_BUILD_TYPE STREQUAL "RelWithDebInfo")
    # Profiling/Release with Debug Info flags
    add_compile_options("-fno-exceptions" "-fno-omit-frame-pointer" "-g" "-O2")
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
    add_compile_options("-fno-exceptions" "-g" "-O0")
endif()

find_package(Boost 1.76 REQUIRED)
find_package(benchmark REQUIRED)

include_directories(include)

add_executable(WoidTest test/woid_test.cpp)
target_link_libraries(WoidTest ${Boost_LIBRARIES})

add_executable(MoveOnlyBench bench/move_only_bench.cpp)
target_link_libraries(MoveOnlyBench benchmark::benchmark)
