#ifndef ALLOC_STATS_HPP
#define ALLOC_STATS_HPP

#include <cstddef>
#include <cstdint>
#include <cstdio>

// ── Compile-time toggle ──────────────────────────────────────
// Define ALLOC_INSTRUMENTATION to enable counters.
// Define ALLOC_DEBUG to enable double-free / invalid-pointer detection.
// Both are off by default; neither has any cost when disabled.

// ── Per-thread statistics (no atomics, no contention) ────────

struct AllocStats
{
    size_t allocs;
    size_t deallocs;
    size_t refills;       // batch fetches from global
    size_t flushes;       // batch returns to global
    size_t refill_blocks; // total blocks received via refill
    size_t flush_blocks;  // total blocks returned via flush
};

#ifdef ALLOC_INSTRUMENTATION

// Thread-local stats - zero-initialized per thread.
inline thread_local AllocStats tl_alloc_stats{};

#define ALLOC_STAT_INC(field) (tl_alloc_stats.field++)
#define ALLOC_STAT_ADD(field, n) (tl_alloc_stats.field += (n))
#define ALLOC_STAT_GET() (tl_alloc_stats)

inline void alloc_stats_dump(const char *label = "")
{
    const auto &s = tl_alloc_stats;
    std::fprintf(stderr,
                 "[AllocStats %s] allocs=%zu deallocs=%zu "
                 "refills=%zu(%zu blks) flushes=%zu(%zu blks)\n",
                 label, s.allocs, s.deallocs,
                 s.refills, s.refill_blocks,
                 s.flushes, s.flush_blocks);
}

#else // ALLOC_INSTRUMENTATION disabled

#define ALLOC_STAT_INC(field) ((void)0)
#define ALLOC_STAT_ADD(field, n) ((void)0)
#define ALLOC_STAT_GET() (AllocStats{})

inline void alloc_stats_dump(const char * = "") {}

#endif // ALLOC_INSTRUMENTATION

// Debug-mode checks

#ifdef ALLOC_DEBUG

#include <unordered_set>

// Per-thread set of currently-allocated pointers.
// No contention - each thread tracks only its own allocations.
struct AllocDebugState
{
    std::unordered_set<void *> live;
};

inline thread_local AllocDebugState tl_debug_state{};

inline void alloc_debug_on_alloc(void *ptr)
{
    if (ptr)
        tl_debug_state.live.insert(ptr);
}

inline bool alloc_debug_on_dealloc(void *ptr)
{
    if (!ptr)
    {
        std::fprintf(stderr, "[AllocDebug] WARNING: dealloc(nullptr)\n");
        return false;
    }
    auto erased = tl_debug_state.live.erase(ptr);
    if (erased == 0)
    {
        std::fprintf(stderr,
                     "[AllocDebug] ERROR: double-free or invalid pointer %p\n",
                     ptr);
        return false;
    }
    return true;
}

#define ALLOC_DEBUG_ON_ALLOC(ptr) alloc_debug_on_alloc(ptr)
#define ALLOC_DEBUG_ON_DEALLOC(ptr) alloc_debug_on_dealloc(ptr)

#else // ALLOC_DEBUG disabled

#define ALLOC_DEBUG_ON_ALLOC(ptr) ((void)0)
#define ALLOC_DEBUG_ON_DEALLOC(ptr) ((void)0)

#endif // ALLOC_DEBUG

#endif // ALLOC_STATS_HPP
