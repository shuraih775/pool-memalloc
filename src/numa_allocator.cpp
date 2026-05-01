#include "numa_allocator.hpp"

static constexpr size_t CACHE_LINE = 64;

static size_t align_up(size_t val, size_t alignment)
{
    return (val + alignment - 1) & ~(alignment - 1);
}

// Construction / Destruction

NumaAllocator::NumaAllocator(size_t block_sz, size_t blocks_per_node, size_t batch)
    : num_nodes(numa::node_count()),
      block_size(align_up(block_sz < sizeof(FreeBlock) ? sizeof(FreeBlock) : block_sz, CACHE_LINE))
{
    if (num_nodes > MAX_NODES)
        num_nodes = MAX_NODES;

    for (size_t n = 0; n < num_nodes; ++n)
    {
        NumaPool &pool = pools[n];
        pool.block_size = block_size;
        pool.block_count = blocks_per_node;
        pool.batch_size = batch;
        pool.region_size = block_size * blocks_per_node;

        pool.region = numa::alloc_on_node(pool.region_size, CACHE_LINE, n);
        if (!pool.region)
            continue;

        for (size_t i = 0; i < blocks_per_node; ++i)
        {
            void *block = static_cast<char *>(pool.region) + i * block_size;
            pool.global.push(block);
        }
    }
}

NumaAllocator::~NumaAllocator()
{
    for (size_t n = 0; n < num_nodes; ++n)
    {
        if (pools[n].region)
            numa::free_on_node(pools[n].region, pools[n].region_size);
    }
}

//  Node lookup

size_t NumaAllocator::owner_node(void *ptr) const
{
    auto addr = reinterpret_cast<uintptr_t>(ptr);
    for (size_t n = 0; n < num_nodes; ++n)
    {
        auto base = reinterpret_cast<uintptr_t>(pools[n].region);
        if (addr >= base && addr < base + pools[n].region_size)
            return n;
    }
    return 0; // fallback
}

//  Thread-local cache per NUMA node

ThreadCache &NumaAllocator::get_cache(size_t node)
{
    // One ThreadCache per NUMA node per thread. We use a fixed array
    // sized to MAX_NODES inside a thread_local struct.
    struct PerThread
    {
        ThreadCache *caches[MAX_NODES];
        PerThread()
        {
            for (auto &c : caches)
                c = nullptr;
        }
        ~PerThread()
        {
            for (auto &c : caches)
                delete c;
        }
    };

    thread_local PerThread pt;

    if (!pt.caches[node])
        pt.caches[node] = new ThreadCache(&pools[node].global, pools[node].batch_size);

    return *pt.caches[node];
}

//  Alloc

void *NumaAllocator::alloc()
{
    size_t node = numa::current_node();
    if (node >= num_nodes)
        node = 0;

    // Try NUMA-local pool first
    void *ptr = get_cache(node).alloc();
    if (ptr)
        return ptr;

    // Fallback: try other nodes
    for (size_t n = 0; n < num_nodes; ++n)
    {
        if (n == node)
            continue;
        ptr = get_cache(n).alloc();
        if (ptr)
            return ptr;
    }

    return nullptr;
}

// Dealloc

void NumaAllocator::dealloc(void *ptr)
{
    // Return to the pool that owns this block
    size_t node = owner_node(ptr);
    get_cache(node).dealloc(ptr);
}
