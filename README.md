# Pool Memory Allocator

A high-performance, lock-free memory pool allocator designed for low-latency systems. Features a hierarchical architecture with per-thread caching, batch operations, NUMA awareness, and multiple size class support.

## Project Structure

```
include/
  pool_allocator.hpp       # Lock-free freelist with tagged pointers (ABA-safe)
  thread_cache.hpp          # Per-thread cache + CachedAllocator
  multi_size_allocator.hpp  # Multiple size class pools (16–256B)
  numa_allocator.hpp        # NUMA-aware allocator
  numa_utils.hpp            # OS-level NUMA primitives
  alloc_stats.hpp           # Instrumentation & debug checks
src/
  pool_allocator.cpp
  thread_cache.cpp
  multi_size_allocator.cpp
  numa_allocator.cpp
  numa_utils.cpp
benchmark/
  benchmark.cpp             # Realistic pressure benchmarks
architecture.md             # Design rationale and target architecture
```

## Requirements

- C++20 compiler (GCC 11+, Clang 13+, MSVC 2022+)
- CMake 3.16+
- `libatomic` (linked automatically via CMake on GCC)

## Build

### Release (default)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Debug (with instrumentation)

Enables per-thread allocation counters and double-free / invalid-pointer detection:

```bash
cmake -B build_debug -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS_DEBUG="-O0 -g -pthread -DALLOC_INSTRUMENTATION -DALLOC_DEBUG"
cmake --build build_debug
```

| Flag                     | Effect                                         |
|--------------------------|-------------------------------------------------|
| `ALLOC_INSTRUMENTATION`  | Thread-local counters for allocs, frees, batches |
| `ALLOC_DEBUG`            | Detects double-free and invalid pointer on free  |

Both compile to zero cost when not defined.

## Run Benchmarks

```bash
./build/benchmark
```

The benchmark runs two scenarios across `MemAllocator`, `CachedAllocator`, and `malloc/free`:

**Burst alloc + delayed free** — allocates in bursts of 128, holds a random number of blocks (16–256) before freeing. Tests separated alloc/free phases under thread contention (1, 2, 4, 8 threads).

**Cross-thread free** — producer threads allocate and enqueue pointers; consumer threads dequeue and free. Tests cross-thread ownership transfer (1, 2, 4 producer/consumer pairs).

Output includes throughput (ops/sec) and latency percentiles (p50, p99) for both alloc and free operations.

### Example output

```
==============================================================================================
  BURST ALLOC + DELAYED FREE
==============================================================================================
MemAllocator           | burst-alloc          |  1 thr |     500000 ops |      6035775 ops/s | p50     100 ns | p99     100 ns
CachedAllocator        | burst-alloc          |  1 thr |     500000 ops |      6673117 ops/s | p50       0 ns | p99     200 ns
malloc/free            | burst-alloc          |  1 thr |     500000 ops |      6449141 ops/s | p50       0 ns | p99     900 ns
```

## Usage

### CachedAllocator (recommended)

Self-contained pool with per-thread caching. Zero syscalls after construction.

```cpp
#include "thread_cache.hpp"

// 64-byte blocks, 1M blocks, batch size 64
CachedAllocator alloc(64, 1'000'000);

void* p = alloc.alloc();
alloc.dealloc(p);
```

### MultiSizeAllocator

Separate pools for size classes 16, 32, 64, 128, 256 bytes.

```cpp
#include "multi_size_allocator.hpp"

MultiSizeAllocator alloc(100'000);  // blocks per size class

void* p = alloc.alloc(100);        // rounds up to 128B class
alloc.dealloc(p, 100);
```

### NumaAllocator

Per-NUMA-node pools. Threads automatically pull from their local node.

```cpp
#include "numa_allocator.hpp"

NumaAllocator alloc(64, 500'000);  // block size, blocks per node

void* p = alloc.alloc();
alloc.dealloc(p);
```

### MemAllocator (low-level)

Bare lock-free freelist. Use directly only if you need manual control.

```cpp
#include "pool_allocator.hpp"

MemAllocator freelist;

// Pre-populate
freelist.push(block);

// Single ops
void* p = freelist.pop();
freelist.push(p);

// Batch ops (one CAS per batch)
freelist.push_bulk(head, tail);
FreeBlock* batch = freelist.pop_bulk(64);
```

### Instrumentation (debug builds)

```cpp
#include "alloc_stats.hpp"

// After some alloc/free work:
AllocStats s = ALLOC_STAT_GET();
alloc_stats_dump("my-thread");
// prints: [AllocStats my-thread] allocs=... deallocs=... refills=... flushes=...
```

## Design

See [architecture.md](architecture.md) for the full design rationale, tradeoff analysis, and target architecture.

Key decisions:
- **Tagged pointers** for ABA safety (64-bit tag, no epoch GC)
- **Per-thread cache** eliminates atomics on the hot path
- **Batch transfers** (configurable, default 64) — one CAS per batch
- **Watermark-based** refill/flush for predictable memory usage
- **Cache-line alignment** (`alignas(64)`) on all critical structures
- **Hardware prefetch** during pointer walks
