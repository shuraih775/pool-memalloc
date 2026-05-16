#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <thread>
#include <vector>

class ThreadLocalBitmapAllocator
{
public:
    ThreadLocalBitmapAllocator(size_t block_size,
                               size_t blocks_per_thread,
                               size_t max_threads);

    ~ThreadLocalBitmapAllocator();

    void *alloc();
    void dealloc(void *ptr);

private:
    static constexpr uint64_t FULL = ~0ULL;
    static constexpr size_t WORD_BITS = 64;

    struct alignas(64) Region
    {
        uint8_t *memory;
        std::atomic<uint64_t> *bitmap;

        size_t block_count;
        size_t word_count;

        alignas(64) size_t hint;
    };

private:
    size_t m_block_size;
    size_t m_blocks_per_thread;
    size_t m_max_threads;

    std::vector<Region> m_regions;

    std::atomic<size_t> m_thread_counter;

private:
    static thread_local size_t t_region_index;

private:
    size_t assign_region();

    inline void *index_to_ptr(Region &region, size_t index);
    inline size_t ptr_to_index(Region &region, void *ptr);

    inline size_t find_free_bit(uint64_t word);
};