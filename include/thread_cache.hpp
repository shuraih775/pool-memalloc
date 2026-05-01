#ifndef THREAD_CACHE_HPP
#define THREAD_CACHE_HPP

#include "pool_allocator.hpp"
#include "alloc_stats.hpp"
#include <cstdlib>

class alignas(64) ThreadCache
{
private:
    FreeBlock *local_head;
    size_t local_size;
    size_t batch_size;
    size_t low_watermark;
    size_t high_watermark;
    MemAllocator *global;

    void refill();
    void flush(size_t excess);

public:
    ThreadCache(MemAllocator *global_alloc, size_t batch = 64);

    void *alloc();
    void dealloc(void *ptr);
};

class CachedAllocator
{
private:
    MemAllocator global;
    size_t batch_size;
    size_t block_size;
    size_t block_count;
    void *region;

    ThreadCache &get_cache();

public:
    CachedAllocator(size_t block_sz, size_t num_blocks, size_t batch = 64);
    ~CachedAllocator();

    CachedAllocator(const CachedAllocator &) = delete;
    CachedAllocator &operator=(const CachedAllocator &) = delete;

    void *alloc();
    void dealloc(void *ptr);
};

#endif // THREAD_CACHE_HPP
