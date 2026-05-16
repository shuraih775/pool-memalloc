#pragma once

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <random>
#include <thread>
#include <vector>

using Clock = std::chrono::high_resolution_clock;

constexpr size_t OPS = 500'000;
constexpr size_t THREADS = 8;

constexpr size_t SIZE_CLASSES[] = {
    16,
    32,
    64,
    128,
    256,
    512,
    1024};

inline size_t random_size(std::mt19937 &rng)
{
    static thread_local std::uniform_int_distribution<size_t>
        dist(0, std::size(SIZE_CLASSES) - 1);

    return SIZE_CLASSES[dist(rng)];
}

inline void print_result(const char *name,
                         std::chrono::high_resolution_clock::time_point start,
                         std::chrono::high_resolution_clock::time_point end)
{
    auto ms =
        std::chrono::duration_cast<
            std::chrono::milliseconds>(
            end - start)
            .count();

    std::cout
        << name
        << " benchmark completed in "
        << ms
        << " ms\n";
}