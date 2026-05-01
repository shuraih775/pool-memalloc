#include "multi_size_allocator.hpp"

size_t MultiSizeAllocator::class_index(size_t size)
{
    for (size_t i = 0; i < NUM_CLASSES; ++i)
    {
        if (size <= SIZE_CLASSES[i])
            return i;
    }
    return NUM_CLASSES; // oversized
}

MultiSizeAllocator::MultiSizeAllocator(size_t blocks_per_class, size_t batch)
{
    for (size_t i = 0; i < NUM_CLASSES; ++i)
    {
        pools[i] = new CachedAllocator(SIZE_CLASSES[i], blocks_per_class, batch);
    }
}

MultiSizeAllocator::~MultiSizeAllocator()
{
    for (size_t i = 0; i < NUM_CLASSES; ++i)
    {
        delete pools[i];
    }
}

void *MultiSizeAllocator::alloc(size_t size)
{
    size_t idx = class_index(size);
    if (idx >= NUM_CLASSES)
        return nullptr;

    return pools[idx]->alloc();
}

void MultiSizeAllocator::dealloc(void *ptr, size_t size)
{
    size_t idx = class_index(size);
    if (idx >= NUM_CLASSES)
        return;

    pools[idx]->dealloc(ptr);
}
