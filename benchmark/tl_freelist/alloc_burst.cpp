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
    ptrs.reserve(cfg.batch);

    for (size_t i = 0; i < cfg.ops; ++i)
    {
        void *p = allocator->alloc();

        if (!p)
            continue;

        std::memset(p, 0xAB, cfg.size);

        ptrs.push_back(p);

        if (ptrs.size() >= cfg.batch)
        {
            for (void *x : ptrs)
                allocator->dealloc(x);

            ptrs.clear();
        }
    }

    for (void *x : ptrs)
        allocator->dealloc(x);
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
        threads.emplace_back(worker);

    for (auto &t : threads)
        t.join();

    auto end = Clock::now();

    print_result(
        "tl_freelist_alloc_burst",
        cfg,
        start,
        end);
}