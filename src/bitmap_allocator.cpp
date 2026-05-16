
#include "bitmap_allocator.hpp"

#ifdef _MSC_VER
#include <intrin.h>
#endif

BitmapAllocator::BitmapAllocator(size_t block_size, size_t block_count)
    : m_block_size(block_size < 16 ? 16 : block_size),
      m_block_count(block_count),
      m_word_count((block_count + WORD_BITS - 1) / WORD_BITS),
      m_memory(nullptr),
      m_bitmap(nullptr),
      m_hint(0)
{
    size_t aligned_size = ((m_block_size + 63) / 64) * 64;
    m_block_size = aligned_size;

    m_memory = static_cast<uint8_t *>(std::aligned_alloc(64, m_block_size * m_block_count));

    if (!m_memory)
        throw std::bad_alloc();

    m_bitmap = static_cast<std::atomic<uint64_t> *>(
        std::aligned_alloc(64, sizeof(std::atomic<uint64_t>) * m_word_count));

    if (!m_bitmap)
    {
        std::free(m_memory);
        throw std::bad_alloc();
    }

    for (size_t i = 0; i < m_word_count; ++i)
    {
        new (&m_bitmap[i]) std::atomic<uint64_t>(0);
    }
}

BitmapAllocator::~BitmapAllocator()
{
    for (size_t i = 0; i < m_word_count; ++i)
    {
        m_bitmap[i].~atomic<uint64_t>();
    }

    std::free(m_bitmap);
    std::free(m_memory);
}

inline size_t BitmapAllocator::find_free_bit(uint64_t word) const
{
#ifdef _MSC_VER
    unsigned long idx;
    _BitScanForward64(&idx, ~word);
    return static_cast<size_t>(idx);
#else
    return static_cast<size_t>(__builtin_ctzll(~word));
#endif
}

inline void *BitmapAllocator::index_to_ptr(size_t index) const
{
    return m_memory + (index * m_block_size);
}

inline size_t BitmapAllocator::ptr_to_index(void *ptr) const
{
    return (static_cast<uint8_t *>(ptr) - m_memory) / m_block_size;
}

void *BitmapAllocator::alloc()
{
    size_t start = m_hint.load(std::memory_order_relaxed);

    for (size_t probe = 0; probe < m_word_count; ++probe)
    {
        size_t word_index = (start + probe) % m_word_count;

        uint64_t word = m_bitmap[word_index].load(std::memory_order_relaxed);

        if (word == FULL)
            continue;

        while (word != FULL)
        {
            size_t free_bit = find_free_bit(word);

            uint64_t mask = (1ULL << free_bit);
            uint64_t new_word = word | mask;

            if (m_bitmap[word_index].compare_exchange_weak(
                    word,
                    new_word,
                    std::memory_order_acquire,
                    std::memory_order_relaxed))
            {
                size_t global_index = (word_index * WORD_BITS) + free_bit;

                if (global_index >= m_block_count)
                {
                    m_bitmap[word_index].fetch_and(~mask, std::memory_order_release);
                    return nullptr;
                }

                m_hint.store(word_index, std::memory_order_relaxed);

                return index_to_ptr(global_index);
            }
        }
    }

    return nullptr;
}

void BitmapAllocator::dealloc(void *ptr)
{
    size_t index = ptr_to_index(ptr);

    size_t word_index = index / WORD_BITS;
    size_t bit_index = index % WORD_BITS;

    uint64_t mask = ~(1ULL << bit_index);

    m_bitmap[word_index].fetch_and(mask, std::memory_order_release);

    m_hint.store(word_index, std::memory_order_relaxed);
}