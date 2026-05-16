#include "../../include/tl_freelist_allocator.hpp"

#include "../common.hpp"

#include <cstring>
#include <thread>
#include <vector>

BenchmarkConfig cfg;

ThreadLocalFreelistAllocator *allocator;

void worker()
{
    std::vector<void *> ptrs;
    ptrs.reserve(cfg.pool);

    for (size_t i = 0; i < cfg.pool; ++i)
    {
        void *p = allocator->alloc();

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
        allocator->dealloc(p);
    }
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

    allocator =
        new ThreadLocalFreelistAllocator(
            cfg.size,
            cfg.pool);

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
        "tl_freelist_locality_walk",
        cfg,
        start,
        end);
}