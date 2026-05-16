#ifndef FREE_LIST_ALLOCATOR_HPP
#define FREE_LIST_ALLOCATOR_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>

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

class FreelistAllocator
{
private:
    alignas(64) std::atomic<TaggedPtr> head;
    char pad_[64 - sizeof(std::atomic<TaggedPtr>)];
    std::mutex bulk_lock;

public:
    FreelistAllocator();

    void push(void *ptr);
    void *pop();

    void push_bulk(FreeBlock *batch_head, FreeBlock *batch_tail);
    FreeBlock *pop_bulk(size_t n);
};

#endif // FREE_LIST_ALLOCATOR_HPP