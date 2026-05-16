#include "../../include/tl_freelist_allocator.hpp"
#include "../common.hpp"

#include <algorithm>
#include <cstring>
#include <random>
#include <thread>
#include <vector>

BenchmarkConfig cfg;

ThreadLocalFreelistAllocator *allocator = nullptr;

struct Item
{
    void *ptr;
};

void worker()
{
    std::mt19937 rng(
        static_cast<unsigned>(
            std::hash<std::thread::id>{}(
                std::this_thread::get_id())));

    std::vector<Item> ptrs;
    ptrs.reserve(cfg.ops);

    for (size_t i = 0; i < cfg.ops; ++i)
    {
        void *p = allocator->alloc();

        if (!p)
            continue;

        size_t sz = random_size(rng);

        std::memset(p, 0xEE, sz);

        ptrs.push_back({p});
    }

    std::shuffle(
        ptrs.begin(),
        ptrs.end(),
        rng);

    for (auto &x : ptrs)
    {
        allocator->dealloc(x.ptr);
    }
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

    ThreadLocalFreelistAllocator alloc(
        cfg.size,
        cfg.pool,
        cfg.batch);

    allocator = &alloc;

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
        "tl_freelist_random_free",
        cfg,
        start,
        end);
}