#include "pool_allocator.hpp"

#ifdef _MSC_VER
#include <intrin.h>
#define PREFETCH(addr) _mm_prefetch(reinterpret_cast<const char *>(addr), _MM_HINT_T0)
#else
#define PREFETCH(addr) __builtin_prefetch(addr, 0, 3)
#endif

MemAllocator::MemAllocator() : head(TaggedPtr{nullptr, 0}) {}

void MemAllocator::push(void *ptr)
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

void *MemAllocator::pop()
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

void MemAllocator::push_bulk(FreeBlock *batch_head, FreeBlock *batch_tail)
{
    TaggedPtr old_head;

    do
    {
        old_head = head.load(std::memory_order_relaxed);
        batch_tail->next = old_head.ptr;
    } while (!head.compare_exchange_weak(
        old_head,
        TaggedPtr{batch_head, old_head.tag + 1},
        std::memory_order_release,
        std::memory_order_relaxed));
}

FreeBlock *MemAllocator::pop_bulk(size_t n)
{
    TaggedPtr old_head;
    FreeBlock *cut;
    size_t count;

    do
    {
        old_head = head.load(std::memory_order_acquire);
        if (!old_head.ptr)
            return nullptr;

        cut = old_head.ptr;
        count = 1;
        while (count < n && cut->next)
        {
            PREFETCH(cut->next->next);
            cut = cut->next;
            ++count;
        }
    } while (!head.compare_exchange_weak(
        old_head,
        TaggedPtr{cut->next, old_head.tag + 1},
        std::memory_order_acquire,
        std::memory_order_relaxed));

    cut->next = nullptr;
    return old_head.ptr;
}