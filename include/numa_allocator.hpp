#ifndef NUMA_ALLOCATOR_HPP
#define NUMA_ALLOCATOR_HPP

#include "pool_allocator.hpp"
#include "thread_cache.hpp"
#include "numa_utils.hpp"

// Per-NUMA-node pool: owns a contiguous aligned region allocated on the node,
// a global lock-free freelist, and per-thread caches that pull from it.
struct NumaPool
{
    MemAllocator global;
    void *region;
    size_t region_size;
    size_t block_size;
    size_t block_count;
    size_t batch_size;
};

class NumaAllocator
{
public:
    static constexpr size_t MAX_NODES = 16;

    NumaAllocator(size_t block_sz, size_t blocks_per_node, size_t batch = 64);
    ~NumaAllocator();

    NumaAllocator(const NumaAllocator &) = delete;
    NumaAllocator &operator=(const NumaAllocator &) = delete;

    void *alloc();
    void dealloc(void *ptr);

private:
    NumaPool pools[MAX_NODES];
    size_t num_nodes;
    size_t block_size;

    ThreadCache &get_cache(size_t node);

    // Given a pointer, determine which NUMA pool it belongs to.
    size_t owner_node(void *ptr) const;
};

#endif // NUMA_ALLOCATOR_HPP
