#include "../../include/freelist_allocator.hpp"
#include "../common.hpp"

#include <cstring>
#include <random>
#include <thread>
#include <vector>

BenchmarkConfig cfg;

FreelistAllocator allocator;

void *memory = nullptr;

struct Item
{
    void *ptr;
};

void initialize_pool()
{
    size_t total = cfg.size * cfg.pool;

    total = (total + 63) & ~63ULL;

    memory = std::aligned_alloc(64, total);

    for (size_t i = 0; i < cfg.pool; ++i)
    {
        allocator.push(
            static_cast<char *>(memory) + (i * cfg.size));
    }
}

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
        void *p = allocator.pop();

        if (!p)
            continue;

        size_t sz = random_size(rng);

        std::memset(p, 0xCC, sz);

        ptrs.push_back({p});
    }

    for (auto &x : ptrs)
    {
        allocator.push(x.ptr);
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
        "freelist_alloc_storm",
        cfg,
        start,
        end);

    std::free(memory);
}