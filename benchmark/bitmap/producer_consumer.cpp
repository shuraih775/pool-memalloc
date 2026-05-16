#include "../../include/bitmap_allocator.hpp"

#include "../common.hpp"

#include <atomic>
#include <cstring>
#include <mutex>
#include <queue>
#include <thread>

BenchmarkConfig cfg;

BitmapAllocator *allocator;

struct Queue
{
    std::mutex mtx;
    std::queue<void *> q;

    void push(void *p)
    {
        std::lock_guard<std::mutex> lock(mtx);
        q.push(p);
    }

    bool pop(void *&p)
    {
        std::lock_guard<std::mutex> lock(mtx);

        if (q.empty())
            return false;

        p = q.front();
        q.pop();

        return true;
    }
};

Queue queue_;
std::atomic<bool> done = false;

void producer()
{
    for (size_t i = 0; i < cfg.ops; ++i)
    {
        void *p = allocator->alloc();

        if (!p)
            continue;

        std::memset(p, 0xAB, cfg.size);

        queue_.push(p);
    }

    done.store(true, std::memory_order_release);
}

void consumer()
{
    void *p;

    while (true)
    {
        if (queue_.pop(p))
        {
            allocator->dealloc(p);
        }
        else if (done.load(std::memory_order_acquire))
        {
            break;
        }
    }
}

int main(int argc, char **argv)
{
    cfg = parse_args(argc, argv);

    allocator =
        new BitmapAllocator(
            cfg.size,
            cfg.pool);

    auto start = Clock::now();

    std::thread prod(producer);
    std::thread cons(consumer);

    prod.join();
    cons.join();

    auto end = Clock::now();

    print_result(
        "bitmap_producer_consumer",
        cfg,
        start,
        end);
}