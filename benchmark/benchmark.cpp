#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <cstring>

#include "../include/pool_allocator.hpp"

constexpr size_t OPS = 1'000'000;
constexpr size_t BLOCK_SIZE = sizeof(FreeBlock);

struct Result
{
    double seconds;
    size_t total_ops;
};

struct IAllocator
{
    virtual void *alloc() = 0;
    virtual void dealloc(void *ptr) = 0;
    virtual ~IAllocator() = default;
};

class FreeListAllocator : public IAllocator
{
private:
    MemAllocator freelist;
    void *memory;
    size_t capacity;

public:
    FreeListAllocator(size_t blocks) : capacity(blocks)
    {
        memory = std::malloc(blocks * BLOCK_SIZE);

        for (size_t i = 0; i < blocks; i++)
        {
            void *block = (char *)memory + i * BLOCK_SIZE;
            freelist.push(block);
        }
    }

    void *alloc() override
    {
        return freelist.pop();
    }

    void dealloc(void *ptr) override
    {
        freelist.push(ptr);
    }

    ~FreeListAllocator()
    {
        std::free(memory);
    }
};

class NewDeleteAllocator : public IAllocator
{
public:
    void *alloc() override
    {
        return ::operator new(BLOCK_SIZE);
    }

    void dealloc(void *ptr) override
    {
        ::operator delete(ptr);
    }
};

void worker(IAllocator *allocator, std::atomic<size_t> &counter)
{
    size_t local = 0;

    for (size_t i = 0; i < OPS; i++)
    {
        void *ptr = allocator->alloc();
        if (ptr)
        {
            allocator->dealloc(ptr);
            local++;
        }
    }

    counter.fetch_add(local, std::memory_order_relaxed);
}

Result run_benchmark(IAllocator *allocator, int threads)
{
    std::atomic<size_t> counter{0};

    // Warmup
    for (int i = 0; i < 10000; i++)
    {
        void *p = allocator->alloc();
        if (p)
            allocator->dealloc(p);
    }

    auto start = std::chrono::high_resolution_clock::now();

    std::vector<std::thread> workers;
    for (int i = 0; i < threads; i++)
    {
        workers.emplace_back(worker, allocator, std::ref(counter));
    }

    for (auto &t : workers)
        t.join();

    auto end = std::chrono::high_resolution_clock::now();

    double seconds = std::chrono::duration<double>(end - start).count();

    return {seconds, counter.load()};
}

void print_result(const std::string &name, const Result &r, int threads)
{
    std::cout << "=== " << name << " | Threads: " << threads << " ===\n";
    std::cout << "Total Ops: " << r.total_ops << "\n";
    std::cout << "Time: " << r.seconds << " sec\n";
    std::cout << "Ops/sec: " << (r.total_ops / r.seconds) << "\n\n";
}

int main()
{
    constexpr size_t BLOCKS = 1'000'000;

    FreeListAllocator freelist_alloc(BLOCKS);
    NewDeleteAllocator newdelete_alloc;

    std::vector<int> thread_counts = {1, 2, 4, 8};

    for (int t : thread_counts)
    {
        auto r1 = run_benchmark(&freelist_alloc, t);
        print_result("MemAllocator", r1, t);

        auto r2 = run_benchmark(&newdelete_alloc, t);
        print_result("New/Delete", r2, t);
    }

    return 0;
}