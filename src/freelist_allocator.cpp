#include "freelist_allocator.hpp"
#include <mutex>

#ifdef _MSC_VER
#include <intrin.h>
#define PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char *>(addr), _MM_HINT_T0)
#else
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif

FreelistAllocator::FreelistAllocator() : head(TaggedPtr{nullptr, 0}) {}

void FreelistAllocator::push(void *ptr)
{
    FreeBlock *node = static_cast<FreeBlock *>(ptr);
    TaggedPtr old_head;

    do
    {
        old_head = head.load(std::memory_order_relaxed);
        node->next = old_head.ptr;
    } while (!head.compare_exchange_weak(
        old_head,
        TaggedPtr{node, old_head.tag + 1},
        std::memory_order_release,
        std::memory_order_relaxed));
}

void *FreelistAllocator::pop()
{
    TaggedPtr old_head;

    do
    {
        old_head = head.load(std::memory_order_acquire);
        if (!old_head.ptr)
            return nullptr;

        PREFETCH(old_head.ptr->next);
    } while (!head.compare_exchange_weak(
        old_head,
        TaggedPtr{old_head.ptr->next, old_head.tag + 1},
        std::memory_order_acquire,
        std::memory_order_relaxed));

    return old_head.ptr;
}

void FreelistAllocator::push_bulk(
    FreeBlock *batch_head,
    FreeBlock *batch_tail)
{
    std::lock_guard<std::mutex> guard(bulk_lock);

    batch_tail->next = nullptr;

    FreeBlock *node = batch_head;

    while (node)
    {
        FreeBlock *next = node->next;

        push(node);

        node = next;
    }
}

FreeBlock *FreelistAllocator::pop_bulk(size_t n)
{
    std::lock_guard<std::mutex> guard(bulk_lock);

    FreeBlock *start = static_cast<FreeBlock *>(pop());

    if (!start)
        return nullptr;

    FreeBlock *tail = start;

    size_t count = 1;

    while (count < n)
    {
        FreeBlock *next =
            static_cast<FreeBlock *>(pop());

        if (!next)
            break;

        tail->next = next;
        tail = next;

        ++count;
    }

    tail->next = nullptr;

    return start;
}