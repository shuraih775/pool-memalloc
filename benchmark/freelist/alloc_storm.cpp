#include "../../include/freelist_allocator.hpp"
#include "../common.hpp"

#include <cstring>

constexpr size_t BLOCK_SIZE = 1024;
constexpr size_t BLOCK_COUNT = 1'000'000;
FreelistAllocator allocator;

struct Item
{
    void *ptr;
};

void initialize_pool()
{
    void *memory =
        std::aligned_alloc(
            64,
            BLOCK_SIZE * BLOCK_COUNT);

    for (size_t i = 0; i < BLOCK_COUNT; ++i)
    {
        allocator.push(
            static_cast<char *>(memory) + (i * BLOCK_SIZE));
    }
}

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

int main()
{
    initialize_pool();

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
        "FreelistAllocator alloc_storm",
        start,
        end);
}