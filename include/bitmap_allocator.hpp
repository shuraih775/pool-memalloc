#pragma once

#include <atomic>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

class BitmapAllocator
{
public:
    BitmapAllocator(size_t block_size, size_t block_count);
    ~BitmapAllocator();

    void *alloc();
    void dealloc(void *ptr);

private:
    static constexpr uint64_t FULL = ~0ULL;
    static constexpr size_t WORD_BITS = 64;

    size_t m_block_size;
    size_t m_block_count;
    size_t m_word_count;

    uint8_t *m_memory;

    alignas(64) std::atomic<uint64_t> *m_bitmap;

    alignas(64) std::atomic<size_t> m_hint;

private:
    inline size_t find_free_bit(uint64_t word) const;
    inline void *index_to_ptr(size_t index) const;
    inline size_t ptr_to_index(void *ptr) const;
};
