cmake_minimum_required(VERSION 3.16)

project(mem_alloc LANGUAGES CXX)

option(ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENABLE_ASAN)

    add_compile_options(
        -fsanitize=address
        -fno-omit-frame-pointer
        -g
    )

    add_link_options(
        -fsanitize=address
    )

endif()

find_library(MIMALLOC_LIB mimalloc REQUIRED)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/build)

include_directories(
    include
    benchmark
)

# COMMON FLAGS

set(COMMON_FLAGS
    -march=native
    -pthread
    -fno-omit-frame-pointer
)

set(RELEASE_FLAGS
    -O3
    -flto
    -DNDEBUG
    -g
)

set(DEBUG_FLAGS
    -O0
    -g3
)

# ALLOCATOR LIBRARY

add_library(allocator STATIC
    src/freelist_allocator.cpp
    src/tl_freelist_allocator.cpp
    src/bitmap_allocator.cpp
    src/tl_bitmap_allocator.cpp
    src/multi_size_allocator.cpp
    src/numa_utils.cpp
    src/numa_allocator.cpp
)

target_compile_options(allocator PRIVATE
    ${COMMON_FLAGS}
    $<$<CONFIG:Release>:${RELEASE_FLAGS}>
    $<$<CONFIG:Debug>:${DEBUG_FLAGS}>
)

target_link_libraries(allocator PRIVATE
    pthread
    atomic
)

# BENCHMARK HELPERS

function(configure_target target)

    target_compile_options(${target} PRIVATE
        ${COMMON_FLAGS}
        $<$<CONFIG:Release>:${RELEASE_FLAGS}>
        $<$<CONFIG:Debug>:${DEBUG_FLAGS}>
    )

endfunction()

function(add_allocator_benchmark target source)

    add_executable(${target} ${source})

    configure_target(${target})

    target_link_libraries(${target} PRIVATE
        allocator
    )

endfunction()

function(add_malloc_benchmark target source)

    add_executable(${target} ${source})

    configure_target(${target})

    target_link_libraries(${target} PRIVATE
        pthread
    )

endfunction()

function(add_mimalloc_benchmark target source)

    add_executable(${target} ${source})

    configure_target(${target})

    target_link_libraries(${target} PRIVATE
        pthread
        ${MIMALLOC_LIB}
    )

endfunction()

# GENERAL BENCHMARKS

# malloc

add_malloc_benchmark(
    malloc_random_free
    benchmark/malloc/random_free.cpp
)

add_malloc_benchmark(
    malloc_alloc_storm
    benchmark/malloc/alloc_storm.cpp
)

# mimalloc

add_mimalloc_benchmark(
    mimalloc_random_free
    benchmark/mimalloc/random_free.cpp
)

add_mimalloc_benchmark(
    mimalloc_alloc_storm
    benchmark/mimalloc/alloc_storm.cpp
)

# freelist

add_allocator_benchmark(
    freelist_random_free
    benchmark/freelist/random_free.cpp
)

add_allocator_benchmark(
    freelist_alloc_storm
    benchmark/freelist/alloc_storm.cpp
)

# tl_freelist

add_allocator_benchmark(
    tl_freelist_random_free
    benchmark/tl_freelist/random_free.cpp
)

add_allocator_benchmark(
    tl_freelist_alloc_storm
    benchmark/tl_freelist/alloc_storm.cpp
)

# bitmap

add_allocator_benchmark(
    bitmap_random_free
    benchmark/bitmap/random_free.cpp
)

add_allocator_benchmark(
    bitmap_alloc_storm
    benchmark/bitmap/alloc_storm.cpp
)

# tl_bitmap

add_allocator_benchmark(
    tl_bitmap_random_free
    benchmark/tl_bitmap/random_free.cpp
)

add_allocator_benchmark(
    tl_bitmap_alloc_storm
    benchmark/tl_bitmap/alloc_storm.cpp
)

# LOW LATENCY BENCHMARKS

set(LOWLAT_BENCHES
    same_thread
    alloc_burst
    producer_consumer
    locality_walk
)

set(ALLOCATORS
    malloc
    mimalloc
    freelist
    tl_freelist
    bitmap
    tl_bitmap
)

foreach(bench ${LOWLAT_BENCHES})

    # malloc

    add_malloc_benchmark(
        malloc_${bench}
        benchmark/malloc/${bench}.cpp
    )

    # mimalloc

    add_mimalloc_benchmark(
        mimalloc_${bench}
        benchmark/mimalloc/${bench}.cpp
    )

    # freelist

    add_allocator_benchmark(
        freelist_${bench}
        benchmark/freelist/${bench}.cpp
    )

    # tl_freelist

    add_allocator_benchmark(
        tl_freelist_${bench}
        benchmark/tl_freelist/${bench}.cpp
    )

    # bitmap

    add_allocator_benchmark(
        bitmap_${bench}
        benchmark/bitmap/${bench}.cpp
    )

    # tl_bitmap

    add_allocator_benchmark(
        tl_bitmap_${bench}
        benchmark/tl_bitmap/${bench}.cpp
    )

endforeach()