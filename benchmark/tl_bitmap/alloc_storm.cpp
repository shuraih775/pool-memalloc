#include "../../include/tl_bitmap_allocator.hpp"
#include "../common.hpp"

#include <cstring>

ThreadLocalBitmapAllocator allocator(
    1024,
    10'000'000,
    THREADS);

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
        void *p = allocator.alloc();

        if (!p)
            continue;

        size_t sz = random_size(rng);

        std::memset(p, 0xCC, sz);

        ptrs.push_back({p});
    }

    for (auto &x : ptrs)
    {
        allocator.dealloc(x.ptr);
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
        "ThreadLocalBitmapAllocator alloc_storm",
        start,
        end);
}