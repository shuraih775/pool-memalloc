#include "../common.hpp"

#include <mimalloc.h>

#include <cstring>
#include <random>
#include <thread>
#include <vector>

BenchmarkConfig cfg;

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
        size_t sz = random_size(rng);

        void *p = mi_malloc(sz);

        if (!p)
            continue;

        std::memset(p, 0xCC, sz);

        ptrs.push_back({p});
    }

    for (auto &x : ptrs)
    {
        mi_free(x.ptr);
    }
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

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
        "mimalloc_alloc_storm",
        cfg,
        start,
        end);
}