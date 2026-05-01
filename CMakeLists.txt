cmake_minimum_required(VERSION 3.16)
project(mem_alloc LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -flto -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g -pthread")

include_directories(include)

add_library(allocator
    src/pool_allocator.cpp
    src/thread_cache.cpp
    src/multi_size_allocator.cpp
    src/numa_utils.cpp
    src/numa_allocator.cpp
)

add_executable(benchmark
    benchmark/benchmark.cpp
)

target_link_libraries(benchmark PRIVATE allocator pthread atomic)