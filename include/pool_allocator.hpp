#ifndef LOCK_FREE_FREE_LIST_HPP
#define LOCK_FREE_FREE_LIST_HPP

#include <atomic>

struct FreeBlock
{
    FreeBlock *next;
};

class MemAllocator
{
private:
    std::atomic<FreeBlock *> head;

public:
    MemAllocator();

    void push(void *ptr);
    void *pop();
};

#endif // LOCK_FREE_FREE_LIST_HPP