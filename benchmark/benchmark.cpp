#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <algorithm>
#include <random>
#include <queue>
#include <mutex>
#include <cstring>
#include <cstdio>

#include "../include/pool_allocator.hpp"
#include "../include/thread_cache.hpp"
#include "../include/multi_size_allocator.hpp"
#include "../include/numa_allocator.hpp"

// ── Configuration ────────────────────────────────────────────

constexpr size_t BLOCKS       = 1'000'000;
constexpr size_t OPS_PER_THR  = 500'000;
constexpr size_t BURST_SIZE   = 128;
constexpr size_t HOLD_MIN     = 16;
constexpr size_t HOLD_MAX     = 256;
constexpr size_t BLOCK_SIZE   = sizeof(FreeBlock);

using Clock  = std::chrono::high_resolution_clock;
using Nanos  = std::chrono::nanoseconds;

// ── Allocator interface ──────────────────────────────────────

struct IAllocator
{
    virtual void *alloc()           = 0;
    virtual void  dealloc(void *p)  = 0;
    virtual ~IAllocator()           = default;
};

// ── Latency helpers ──────────────────────────────────────────

struct LatencyResult
{
    double throughput;    // ops/sec
    double p50_ns;
    double p99_ns;
    size_t total_ops;
};

static LatencyResult summarize(std::vector<int64_t> &samples, double wall_sec)
{
    if (samples.empty())
        return {0, 0, 0, 0};

    std::sort(samples.begin(), samples.end());
    size_t n = samples.size();
    double p50 = static_cast<double>(samples[n * 50 / 100]);
    double p99 = static_cast<double>(samples[n * 99 / 100]);
    return {static_cast<double>(n) / wall_sec, p50, p99, n};
}

// ── Cross-thread free queue ──────────────────────────────────

struct CrossFreeQueue
{
    std::mutex mtx;
    std::queue<void *> q;

    void push(void *ptr)
    {
        std::lock_guard<std::mutex> lk(mtx);
        q.push(ptr);
    }

    void *try_pop()
    {
        std::lock_guard<std::mutex> lk(mtx);
        if (q.empty()) return nullptr;
        void *p = q.front();
        q.pop();
        return p;
    }

    size_t size()
    {
        std::lock_guard<std::mutex> lk(mtx);
        return q.size();
    }
};

// ── Scenario 1: Burst alloc → delayed free ───────────────────

static void bench_burst_delayed(
    IAllocator *alloc,
    std::vector<int64_t> &alloc_lat,
    std::vector<int64_t> &free_lat)
{
    std::mt19937 rng(static_cast<unsigned>(
        std::hash<std::thread::id>{}(std::this_thread::get_id())));
    std::uniform_int_distribution<size_t> hold_dist(HOLD_MIN, HOLD_MAX);

    std::vector<void *> held;
    held.reserve(BURST_SIZE);
    size_t ops_done = 0;

    while (ops_done < OPS_PER_THR)
    {
        // ── Burst alloc phase ──
        size_t burst = std::min(BURST_SIZE, OPS_PER_THR - ops_done);
        for (size_t i = 0; i < burst; ++i)
        {
            auto t0 = Clock::now();
            void *p = alloc->alloc();
            auto t1 = Clock::now();
            if (p)
            {
                alloc_lat.push_back(
                    std::chrono::duration_cast<Nanos>(t1 - t0).count());
                held.push_back(p);
            }
        }
        ops_done += burst;

        // ── Hold some, free with random delay ──
        size_t keep = hold_dist(rng);
        if (keep > held.size()) keep = held.size();

        // Free the tail beyond 'keep'
        while (held.size() > keep)
        {
            auto t0 = Clock::now();
            alloc->dealloc(held.back());
            auto t1 = Clock::now();
            free_lat.push_back(
                std::chrono::duration_cast<Nanos>(t1 - t0).count());
            held.pop_back();
        }
    }

    // Drain remaining
    for (void *p : held)
    {
        auto t0 = Clock::now();
        alloc->dealloc(p);
        auto t1 = Clock::now();
        free_lat.push_back(
            std::chrono::duration_cast<Nanos>(t1 - t0).count());
    }
}

// ── Scenario 2: Producer/consumer (cross-thread free) ────────

static void bench_producer(
    IAllocator *alloc,
    CrossFreeQueue &xq,
    std::vector<int64_t> &alloc_lat,
    std::atomic<bool> &done)
{
    for (size_t i = 0; i < OPS_PER_THR; ++i)
    {
        auto t0 = Clock::now();
        void *p = alloc->alloc();
        auto t1 = Clock::now();
        if (p)
        {
            alloc_lat.push_back(
                std::chrono::duration_cast<Nanos>(t1 - t0).count());
            xq.push(p);
        }
    }
    done.store(true, std::memory_order_release);
}

static void bench_consumer(
    IAllocator *alloc,
    CrossFreeQueue &xq,
    std::vector<int64_t> &free_lat,
    std::atomic<bool> &producer_done)
{
    while (true)
    {
        void *p = xq.try_pop();
        if (!p)
        {
            if (producer_done.load(std::memory_order_acquire) && xq.size() == 0)
                break;
            std::this_thread::yield();
            continue;
        }
        auto t0 = Clock::now();
        alloc->dealloc(p);
        auto t1 = Clock::now();
        free_lat.push_back(
            std::chrono::duration_cast<Nanos>(t1 - t0).count());
    }
}

// ── Print helpers ────────────────────────────────────────────

static void print_latency(const char *name, const char *scenario,
                          int threads, const LatencyResult &r)
{
    std::printf("%-22s | %-20s | %2d thr | %10zu ops | %12.0f ops/s | "
                "p50 %7.0f ns | p99 %7.0f ns\n",
                name, scenario, threads,
                r.total_ops, r.throughput, r.p50_ns, r.p99_ns);
}

// ── Run helpers ──────────────────────────────────────────────

static void run_burst_delayed(const char *name, IAllocator *alloc, int nthreads)
{
    std::vector<std::vector<int64_t>> alloc_lats(nthreads);
    std::vector<std::vector<int64_t>> free_lats(nthreads);

    for (auto &v : alloc_lats) v.reserve(OPS_PER_THR);
    for (auto &v : free_lats)  v.reserve(OPS_PER_THR);

    auto t0 = Clock::now();
    std::vector<std::thread> workers;
    for (int i = 0; i < nthreads; ++i)
        workers.emplace_back(bench_burst_delayed, alloc,
                             std::ref(alloc_lats[i]),
                             std::ref(free_lats[i]));
    for (auto &t : workers) t.join();
    double wall = std::chrono::duration<double>(Clock::now() - t0).count();

    // Merge samples
    std::vector<int64_t> merged_alloc, merged_free;
    for (auto &v : alloc_lats)
        merged_alloc.insert(merged_alloc.end(), v.begin(), v.end());
    for (auto &v : free_lats)
        merged_free.insert(merged_free.end(), v.begin(), v.end());

    auto ra = summarize(merged_alloc, wall);
    auto rf = summarize(merged_free, wall);
    print_latency(name, "burst-alloc", nthreads, ra);
    print_latency(name, "delayed-free", nthreads, rf);
}

static void run_cross_thread(const char *name, IAllocator *alloc, int pairs)
{
    std::vector<CrossFreeQueue> queues(pairs);
    std::vector<std::vector<int64_t>> alloc_lats(pairs);
    std::vector<std::vector<int64_t>> free_lats(pairs);
    std::vector<std::atomic<bool>> done_flags(pairs);

    for (auto &v : alloc_lats) v.reserve(OPS_PER_THR);
    for (auto &v : free_lats)  v.reserve(OPS_PER_THR);
    for (auto &f : done_flags) f.store(false);

    auto t0 = Clock::now();
    std::vector<std::thread> workers;
    for (int i = 0; i < pairs; ++i)
    {
        workers.emplace_back(bench_producer, alloc,
                             std::ref(queues[i]),
                             std::ref(alloc_lats[i]),
                             std::ref(done_flags[i]));
        workers.emplace_back(bench_consumer, alloc,
                             std::ref(queues[i]),
                             std::ref(free_lats[i]),
                             std::ref(done_flags[i]));
    }
    for (auto &t : workers) t.join();
    double wall = std::chrono::duration<double>(Clock::now() - t0).count();

    std::vector<int64_t> merged_alloc, merged_free;
    for (auto &v : alloc_lats)
        merged_alloc.insert(merged_alloc.end(), v.begin(), v.end());
    for (auto &v : free_lats)
        merged_free.insert(merged_free.end(), v.begin(), v.end());

    auto ra = summarize(merged_alloc, wall);
    auto rf = summarize(merged_free, wall);
    print_latency(name, "cross-thr alloc", pairs * 2, ra);
    print_latency(name, "cross-thr free",  pairs * 2, rf);
}

// ── Allocator wrappers ───────────────────────────────────────

class FreeListAdapter : public IAllocator
{
    MemAllocator freelist;
    void *memory;
public:
    FreeListAdapter(size_t blocks)
    {
        memory = std::malloc(blocks * BLOCK_SIZE);
        for (size_t i = 0; i < blocks; ++i)
            freelist.push(static_cast<char *>(memory) + i * BLOCK_SIZE);
    }
    void *alloc() override { return freelist.pop(); }
    void  dealloc(void *p) override { freelist.push(p); }
    ~FreeListAdapter() { std::free(memory); }
};

class CachedAdapter : public IAllocator
{
    CachedAllocator ca;
public:
    CachedAdapter(size_t blocks) : ca(BLOCK_SIZE, blocks) {}
    void *alloc() override { return ca.alloc(); }
    void  dealloc(void *p) override { ca.dealloc(p); }
};

class MallocAdapter : public IAllocator
{
public:
    void *alloc() override { return std::malloc(BLOCK_SIZE); }
    void  dealloc(void *p) override { std::free(p); }
};

// ── Main ─────────────────────────────────────────────────────

int main()
{
    FreeListAdapter  bare_alloc(BLOCKS);
    CachedAdapter    cached_alloc(BLOCKS);
    MallocAdapter    malloc_alloc;

    struct Entry { const char *name; IAllocator *alloc; };
    Entry entries[] = {
        {"MemAllocator",    &bare_alloc},
        {"CachedAllocator", &cached_alloc},
        {"malloc/free",     &malloc_alloc},
    };

    std::vector<int> thread_counts = {1, 2, 4, 8};

    std::puts("==============================================================================================");
    std::puts("  BURST ALLOC + DELAYED FREE");
    std::puts("==============================================================================================");

    for (int t : thread_counts)
    {
        for (auto &e : entries)
            run_burst_delayed(e.name, e.alloc, t);
        std::puts("---");
    }

    std::puts("");
    std::puts("==============================================================================================");
    std::puts("  CROSS-THREAD FREE (producer/consumer pairs)");
    std::puts("==============================================================================================");

    std::vector<int> pair_counts = {1, 2, 4};
    for (int p : pair_counts)
    {
        for (auto &e : entries)
            run_cross_thread(e.name, e.alloc, p);
        std::puts("---");
    }

    return 0;
}