#ifndef THREAD_LOCAL_FREELIST_ALLOCATOR_HPP
#define THREAD_LOCAL_FREELIST_ALLOCATOR_HPP

#include "freelist_allocator.hpp"
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
    FreelistAllocator *global;

    void refill();
    void flush(size_t excess);

public:
    ThreadCache(FreelistAllocator *global_alloc, size_t batch = 64);
    ~ThreadCache() = default;

    void *alloc();
    void dealloc(void *ptr);
};

class ThreadLocalFreelistAllocator
{
private:
    FreelistAllocator global;
    size_t batch_size;
    size_t block_size;
    size_t block_count;
    void *region;

    ThreadCache &get_cache();

public:
    ThreadLocalFreelistAllocator(size_t block_sz, size_t num_blocks, size_t batch = 64);
    ~ThreadLocalFreelistAllocator();

    ThreadLocalFreelistAllocator(const ThreadLocalFreelistAllocator &) = delete;
    ThreadLocalFreelistAllocator &operator=(const ThreadLocalFreelistAllocator &) = delete;

    void *alloc();
    void dealloc(void *ptr);
};

#endif // THREAD_LOCAL_FREELIST_ALLOCATOR_HPP
