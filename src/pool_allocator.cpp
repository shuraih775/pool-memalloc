#include "pool_allocator.hpp"

MemAllocator::MemAllocator() : head(nullptr) {}

void MemAllocator::push(void *ptr)
{
    FreeBlock *node = static_cast<FreeBlock *>(ptr);
    FreeBlock *old_head;

    do
    {
        old_head = head.load(std::memory_order_relaxed);
        node->next = old_head;
    } while (!head.compare_exchange_weak(
        old_head,
        node,
        std::memory_order_release,
        std::memory_order_relaxed));
}

void *MemAllocator::pop()
{
    FreeBlock *old_head;
    FreeBlock *next_node;

    do
    {
        old_head = head.load(std::memory_order_acquire);
        if (!old_head)
            return nullptr;

        next_node = old_head->next;
    } while (!head.compare_exchange_weak(
        old_head,
        next_node,
        std::memory_order_acquire,
        std::memory_order_relaxed));

    return old_head;
}