#ifndef MULTI_SIZE_ALLOCATOR_HPP
#define MULTI_SIZE_ALLOCATOR_HPP

#include "thread_cache.hpp"
#include <cstddef>

class MultiSizeAllocator
{
public:
    static constexpr size_t NUM_CLASSES = 5;
    static constexpr size_t SIZE_CLASSES[NUM_CLASSES] = {16, 32, 64, 128, 256};

    MultiSizeAllocator(size_t blocks_per_class, size_t batch = 64);
    ~MultiSizeAllocator();

    MultiSizeAllocator(const MultiSizeAllocator &) = delete;
    MultiSizeAllocator &operator=(const MultiSizeAllocator &) = delete;

    void *alloc(size_t size);
    void dealloc(void *ptr, size_t size);

private:
    CachedAllocator *pools[NUM_CLASSES];

    static size_t class_index(size_t size);
};

#endif // MULTI_SIZE_ALLOCATOR_HPP
