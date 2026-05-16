#include "../common.hpp"

#include <algorithm>
#include <cstdlib>
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

        void *p = std::malloc(sz);

        if (!p)
            continue;

        std::memset(p, 0xBB, sz);

        ptrs.push_back({p});
    }

    std::shuffle(
        ptrs.begin(),
        ptrs.end(),
        rng);

    for (auto &x : ptrs)
    {
        std::free(x.ptr);
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
        "malloc_random_free",
        cfg,
        start,
        end);
}