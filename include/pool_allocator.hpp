#ifndef LOCK_FREE_FREE_LIST_HPP
#define LOCK_FREE_FREE_LIST_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>

struct alignas(64) FreeBlock
{
    FreeBlock *next;
};

struct TaggedPtr
{
    FreeBlock *ptr;
    uint64_t tag;

    bool operator==(const TaggedPtr &other) const
    {
        return ptr == other.ptr && tag == other.tag;
    }
};

class MemAllocator
{
private:
    alignas(64) std::atomic<TaggedPtr> head;
    char pad_[64 - sizeof(std::atomic<TaggedPtr>)];

public:
    MemAllocator();

    void push(void *ptr);
    void *pop();

    void push_bulk(FreeBlock *batch_head, FreeBlock *batch_tail);
    FreeBlock *pop_bulk(size_t n);
};

#endif // LOCK_FREE_FREE_LIST_HPP