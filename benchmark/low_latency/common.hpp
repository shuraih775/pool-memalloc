#pragma once

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string_view>

using Clock = std::chrono::high_resolution_clock;

struct BenchmarkConfig
{
    size_t size = 64;
    size_t ops = 5'000'000;
    size_t threads = 8;
    size_t batch = 256;
    size_t pool = 1'000'000;
};

inline BenchmarkConfig parse_args(int argc, char **argv)
{
    BenchmarkConfig cfg;

    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];

        auto next = [&](size_t &x)
        {
            if (i + 1 < argc)
                x = std::strtoull(argv[++i], nullptr, 10);
        };

        if (arg == "--size")
            next(cfg.size);

        else if (arg == "--ops")
            next(cfg.ops);

        else if (arg == "--threads")
            next(cfg.threads);

        else if (arg == "--batch")
            next(cfg.batch);

        else if (arg == "--pool")
            next(cfg.pool);
    }

    return cfg;
}

inline void print_result(
    const char *name,
    const BenchmarkConfig &cfg,
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
        << " | size="
        << cfg.size
        << " | threads="
        << cfg.threads
        << " | ops="
        << cfg.ops
        << " | batch="
        << cfg.batch
        << " | "
        << ms
        << " ms\n";
}