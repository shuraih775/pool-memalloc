#include "../common.hpp"

#include <mimalloc.h>

#include <thread>
#include <vector>
#include <cstring>

BenchmarkConfig cfg;

void worker()
{
    std::vector<void *> ptrs;
    ptrs.reserve(cfg.batch);

    for (size_t i = 0; i < cfg.ops; ++i)
    {
        void *p = mi_malloc(cfg.size);

        if (!p)
            continue;

        std::memset(p, 0xAB, cfg.size);

        ptrs.push_back(p);

        if (ptrs.size() >= cfg.batch)
        {
            for (void *x : ptrs)
                mi_free(x);

            ptrs.clear();
        }
    }

    for (void *x : ptrs)
        mi_free(x);
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

    auto start = Clock::now();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < cfg.threads; ++i)
        threads.emplace_back(worker);

    for (auto &t : threads)
        t.join();

    auto end = Clock::now();

    print_result(
        "mimalloc_same_thread",
        cfg,
        start,
        end);
}