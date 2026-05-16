#include "../../include/freelist_allocator.hpp"

#include "../common.hpp"

#include <cstdlib>
#include <cstring>
#include <thread>
#include <vector>

BenchmarkConfig cfg;

FreelistAllocator allocator;

void initialize_pool()
{
    void *memory =
        std::aligned_alloc(
            64,
            cfg.size * cfg.pool);

    for (size_t i = 0; i < cfg.pool; ++i)
    {
        allocator.push(
            static_cast<char *>(memory) + (i * cfg.size));
    }
}

void worker()
{
    std::vector<void *> ptrs;
    ptrs.reserve(cfg.pool);

    for (size_t i = 0; i < cfg.pool; ++i)
    {
        void *p = allocator.pop();

        if (!p)
            continue;

        std::memset(p, 0, cfg.size);

        ptrs.push_back(p);
    }

    constexpr size_t WALKS = 128;

    for (size_t w = 0; w < WALKS; ++w)
    {
        for (void *p : ptrs)
        {
            auto *bytes =
                static_cast<unsigned char *>(p);

            bytes[0]++;
        }
    }

    for (void *p : ptrs)
    {
        allocator.push(p);
    }
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

    initialize_pool();

    auto start = Clock::now();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < cfg.threads; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end = Clock::now();

    print_result(
        "freelist_locality_walk",
        cfg,
        start,
        end);
}