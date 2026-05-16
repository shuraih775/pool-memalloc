#include "../common.hpp"

#include <cstdlib>
#include <cstring>

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
    ptrs.reserve(OPS);

    for (size_t i = 0; i < OPS; ++i)
    {
        size_t sz = random_size(rng);

        void *p = std::malloc(sz);

        if (!p)
            continue;

        std::memset(p, 0xCC, sz);

        ptrs.push_back({p});
    }

    for (auto &x : ptrs)
    {
        std::free(x.ptr);
    }
}

int main()
{
    auto start = Clock::now();

    std::vector<std::thread> threads;

    for (size_t i = 0; i < THREADS; ++i)
    {
        threads.emplace_back(worker);
    }

    for (auto &t : threads)
    {
        t.join();
    }

    auto end = Clock::now();

    print_result(
        "malloc alloc_storm",
        start,
        end);
}