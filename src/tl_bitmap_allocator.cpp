
#include "tl_bitmap_allocator.hpp"

#ifdef _MSC_VER
#include <intrin.h>
#endif

thread_local size_t ThreadLocalBitmapAllocator::t_region_index = SIZE_MAX;

ThreadLocalBitmapAllocator::ThreadLocalBitmapAllocator(
    size_t block_size,
    size_t blocks_per_thread,
    size_t max_threads)
    : m_block_size(((block_size + 63) / 64) * 64),
      m_blocks_per_thread(blocks_per_thread),
      m_max_threads(max_threads),
      m_regions(max_threads),
      m_thread_counter(0)
{
    for (size_t i = 0; i < max_threads; ++i)
    {
        Region &r = m_regions[i];

        r.block_count = blocks_per_thread;
        r.word_count =
            (blocks_per_thread + WORD_BITS - 1) / WORD_BITS;

        r.hint = 0;

        r.memory = static_cast<uint8_t *>(
            std::aligned_alloc(
                64,
                m_block_size * blocks_per_thread));

        r.bitmap = static_cast<std::atomic<uint64_t> *>(
            std::aligned_alloc(
                64,
                sizeof(std::atomic<uint64_t>) * r.word_count));

        for (size_t w = 0; w < r.word_count; ++w)
        {
            new (&r.bitmap[w]) std::atomic<uint64_t>(0);
        }
    }
}

ThreadLocalBitmapAllocator::~ThreadLocalBitmapAllocator()
{
    for (Region &r : m_regions)
    {
        for (size_t w = 0; w < r.word_count; ++w)
        {
            r.bitmap[w].~atomic<uint64_t>();
        }

        std::free(r.bitmap);
        std::free(r.memory);
    }
}

size_t ThreadLocalBitmapAllocator::assign_region()
{
    size_t idx =
        m_thread_counter.fetch_add(
            1,
            std::memory_order_relaxed);

    return idx % m_max_threads;
}

inline size_t
ThreadLocalBitmapAllocator::find_free_bit(uint64_t word)
{
#ifdef _MSC_VER
    unsigned long idx;
    _BitScanForward64(&idx, ~word);
    return static_cast<size_t>(idx);
#else
    return static_cast<size_t>(__builtin_ctzll(~word));
#endif
}

inline void *
ThreadLocalBitmapAllocator::index_to_ptr(
    Region &region,
    size_t index)
{
    return region.memory + (index * m_block_size);
}

inline size_t
ThreadLocalBitmapAllocator::ptr_to_index(
    Region &region,
    void *ptr)
{
    return (static_cast<uint8_t *>(ptr) - region.memory) / m_block_size;
}

void *ThreadLocalBitmapAllocator::alloc()
{
    if (t_region_index == SIZE_MAX)
    {
        t_region_index = assign_region();
    }

    Region &region = m_regions[t_region_index];

    size_t start = region.hint;

    for (size_t probe = 0;
         probe < region.word_count;
         ++probe)
    {
        size_t word_index =
            (start + probe) % region.word_count;

        uint64_t word =
            region.bitmap[word_index].load(
                std::memory_order_relaxed);

        if (word == FULL)
            continue;

        while (word != FULL)
        {
            size_t bit = find_free_bit(word);

            uint64_t mask = (1ULL << bit);

            uint64_t desired = word | mask;

            if (region.bitmap[word_index]
                    .compare_exchange_weak(
                        word,
                        desired,
                        std::memory_order_acquire,
                        std::memory_order_relaxed))
            {
                size_t global_index =
                    (word_index * WORD_BITS) + bit;

                if (global_index >= region.block_count)
                {
                    region.bitmap[word_index]
                        .fetch_and(
                            ~mask,
                            std::memory_order_release);

                    return nullptr;
                }

                region.hint = word_index;

                return index_to_ptr(
                    region,
                    global_index);
            }
        }
    }

    return nullptr;
}

void ThreadLocalBitmapAllocator::dealloc(void *ptr)
{
    for (Region &region : m_regions)
    {
        uint8_t *start = region.memory;

        uint8_t *end =
            region.memory +
            (region.block_count * m_block_size);

        if (ptr >= start && ptr < end)
        {
            size_t index =
                ptr_to_index(region, ptr);

            size_t word_index =
                index / WORD_BITS;

            size_t bit_index =
                index % WORD_BITS;

            uint64_t mask =
                ~(1ULL << bit_index);

            region.bitmap[word_index]
                .fetch_and(
                    mask,
                    std::memory_order_release);

            region.hint = word_index;

            return;
        }
    }
}