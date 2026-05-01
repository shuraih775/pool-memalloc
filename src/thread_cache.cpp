#include "thread_cache.hpp"

#ifdef _MSC_VER
#include <intrin.h>
#define PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char *>(addr), _MM_HINT_T0)
#else
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif

//  ThreadCache

ThreadCache::ThreadCache(MemAllocator *global_alloc, size_t batch)
    : local_head(nullptr),
      local_size(0),
      batch_size(batch),
      low_watermark(batch / 2),
      high_watermark(batch * 2),
      global(global_alloc)
{
}

void ThreadCache::refill()
{
    FreeBlock *batch = global->pop_bulk(batch_size);
    if (!batch)
        return;

    // Find tail and count the returned list (may be < batch_size)
    FreeBlock *node = batch;
    size_t count = 1;
    while (node->next)
    {
        node = node->next;
        ++count;
    }

    // Prepend batch list to local cache
    node->next = local_head;
    local_head = batch;
    local_size += count;

    ALLOC_STAT_INC(refills);
    ALLOC_STAT_ADD(refill_blocks, count);
}

void ThreadCache::flush(size_t excess)
{

    FreeBlock *batch_head = local_head;
    FreeBlock *batch_tail = local_head;
    size_t count = 1;

    while (count < excess && batch_tail->next)
    {
        batch_tail = batch_tail->next;
        ++count;
    }

    local_head = batch_tail->next;
    local_size -= count;

    global->push_bulk(batch_head, batch_tail);

    ALLOC_STAT_INC(flushes);
    ALLOC_STAT_ADD(flush_blocks, count);
}

void *ThreadCache::alloc()
{
    if (local_size <= low_watermark)
        refill();

    if (!local_head)
        return nullptr;

    FreeBlock *block = local_head;
    local_head = block->next;
    if (local_head)
        PREFETCH(local_head->next);
    --local_size;

    ALLOC_STAT_INC(allocs);
    ALLOC_DEBUG_ON_ALLOC(block);
    return block;
}

void ThreadCache::dealloc(void *ptr)
{
    ALLOC_STAT_INC(deallocs);
    ALLOC_DEBUG_ON_DEALLOC(ptr);

    FreeBlock *block = static_cast<FreeBlock *>(ptr);
    block->next = local_head;
    local_head = block;
    ++local_size;

    if (local_size > high_watermark)
        flush(local_size - batch_size);
}

// CachedAllocator

static constexpr size_t CACHE_LINE = 64;

static size_t align_up(size_t val, size_t alignment)
{
    return (val + alignment - 1) & ~(alignment - 1);
}

CachedAllocator::CachedAllocator(size_t block_sz, size_t num_blocks, size_t batch)
    : batch_size(batch),
      block_size(align_up(block_sz < sizeof(FreeBlock) ? sizeof(FreeBlock) : block_sz, CACHE_LINE)),
      block_count(num_blocks),
      region(nullptr)
{
    size_t total = block_size * block_count;

#ifdef _WIN32
    region = _aligned_malloc(total, CACHE_LINE);
#else
    region = std::aligned_alloc(CACHE_LINE, total);
#endif

    if (!region)
        return;

    for (size_t i = 0; i < block_count; ++i)
    {
        void *block = static_cast<char *>(region) + i * block_size;
        global.push(block);
    }
}

CachedAllocator::~CachedAllocator()
{
    if (region)
    {
#ifdef _WIN32
        _aligned_free(region);
#else
        std::free(region);
#endif
    }
}

ThreadCache &CachedAllocator::get_cache()
{
    thread_local ThreadCache cache(&global, batch_size);
    return cache;
}

void *CachedAllocator::alloc()
{
    return get_cache().alloc();
}

void CachedAllocator::dealloc(void *ptr)
{
    get_cache().dealloc(ptr);
}
