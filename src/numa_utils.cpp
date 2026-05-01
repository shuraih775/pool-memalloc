#include "numa_utils.hpp"

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <unistd.h>
#include <cstdlib>
#include <cstdio>
#ifdef __linux__
#include <sched.h>
#include <sys/mman.h>
#include <linux/mempolicy.h>
#include <sys/syscall.h>
#endif
#endif

namespace numa
{

    // Node count

    size_t node_count()
    {
#ifdef _WIN32
        ULONG highest = 0;
        if (GetNumaHighestNodeNumber(&highest))
            return static_cast<size_t>(highest) + 1;
        return 1;
#elif defined(__linux__)
        // Count directories /sys/devices/system/node/node*
        size_t count = 0;
        for (size_t n = 0; n < 256; ++n)
        {
            char path[128];
            std::snprintf(path, sizeof(path),
                          "/sys/devices/system/node/node%zu", n);
            if (::access(path, F_OK) == 0)
                ++count;
            else
                break;
        }
        return count > 0 ? count : 1;
#else
        return 1;
#endif
    }

    // Current node

    size_t current_node()
    {
#ifdef _WIN32
        PROCESSOR_NUMBER pn;
        GetCurrentProcessorNumberEx(&pn);
        USHORT node = 0;
        if (GetNumaProcessorNodeEx(&pn, &node))
            return static_cast<size_t>(node);
        return 0;
#elif defined(__linux__)
        int cpu = sched_getcpu();
        if (cpu < 0)
            return 0;

        char path[128];
        std::snprintf(path, sizeof(path),
                      "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", cpu);
        FILE *f = std::fopen(path, "r");
        if (!f)
            return 0;

        size_t node = 0;
        if (std::fscanf(f, "%zu", &node) != 1)
            node = 0;
        std::fclose(f);
        return node;
#else
        return 0;
#endif
    }

    //  NUMA-local allocation

    void *alloc_on_node(size_t size, size_t alignment, size_t node)
    {
        (void)alignment; // alignment handled by the VirtualAlloc page granularity or mmap

#ifdef _WIN32
        // VirtualAllocExNuma returns page-aligned memory (4KB+), satisfies any power-of-2 alignment ≤ 4096.
        void *ptr = VirtualAllocExNuma(
            GetCurrentProcess(),
            nullptr,
            size,
            MEM_COMMIT | MEM_RESERVE,
            PAGE_READWRITE,
            static_cast<DWORD>(node));
        if (ptr)
            return ptr;

        // Fallback: regular aligned alloc
        return _aligned_malloc(size, alignment);

#elif defined(__linux__)
        // mmap anonymous, then bind to node via mbind syscall
        void *ptr = ::mmap(nullptr, size,
                           PROT_READ | PROT_WRITE,
                           MAP_PRIVATE | MAP_ANONYMOUS,
                           -1, 0);
        if (ptr == MAP_FAILED)
            return nullptr;

        // Build nodemask for mbind (bit 'node' set)
        unsigned long nodemask = 1UL << node;
        long ret = ::syscall(SYS_mbind, ptr, size,
                             MPOL_BIND, &nodemask,
                             sizeof(nodemask) * 8,
                             0);
        if (ret != 0)
        {
            // mbind failed - memory is still usable, just not NUMA-bound
        }
        return ptr;

#else
        return std::aligned_alloc(alignment, size);
#endif
    }

    void free_on_node(void *ptr, size_t size)
    {
        if (!ptr)
            return;

#ifdef _WIN32
        // Try VirtualFree first (works if allocated via VirtualAllocExNuma)
        if (!VirtualFree(ptr, 0, MEM_RELEASE))
        {
            // Fallback: _aligned_free for _aligned_malloc allocations
            _aligned_free(ptr);
        }
#elif defined(__linux__)
        ::munmap(ptr, size);
#else
        (void)size;
        std::free(ptr);
#endif
    }

} // namespace numa
