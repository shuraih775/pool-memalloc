# System Architecture

## 1. Goals

* **Deterministic latency (p99 ≈ p50)**
* **Zero syscalls in hot path**
* **No global contention**
* **Cache-friendly + NUMA-aware**
* **Bounded memory**

---

## 2. Architecture

### 2.1 Hierarchical Design

```
                ----------------------
                │   Global Pool      │  <- MemAllocator (lock-free, ABA-safe)
                │  (cold path only)  │
                ----------------------
                         ↓  batch transfer (push_bulk / pop_bulk)
                ----------------------
                │   Per-NUMA Pool    │  <- NumaAllocator (one MemAllocator per node)
                ----------------------
                         ↓  batch transfer
                ----------------------
                │  Per-Thread Cache  │  <- ThreadCache (no atomics, pure pointers)
                ----------------------
```

---

## 3. Core Components

### 3.1 MemAllocator — Global Lock-Free Freelist

```cpp
alignas(64) std::atomic<TaggedPtr> head;
```

* **Tagged pointer** (`FreeBlock* + uint64_t tag`) eliminates ABA
* **128-bit CAS** (`cmpxchg16b`) — single atomic for pointer + version
* **Batch ops**: `push_bulk(head, tail)` and `pop_bulk(n)` — one CAS per batch
* **Memory ordering**: `release` on push, `acquire` on pop, no `seq_cst`
* **Cache-line padded** to prevent false sharing

### 3.2 ThreadCache — Per-Thread Local Freelist

```cpp
class alignas(64) ThreadCache {
    FreeBlock *local_head;
    size_t local_size;
    size_t low_watermark;   // batch/2 — triggers proactive refill
    size_t high_watermark;  // batch*2 — triggers flush
};
```

* **Zero atomics** on the hot path — pure pointer manipulation
* **Watermark-based flow control**:
  * `local_size ≤ low_watermark` → `refill()` fetches `batch_size` blocks from global
  * `local_size > high_watermark` → `flush(excess)` returns blocks to global
* **Bounded memory** — each thread holds at most `high_watermark` blocks momentarily
* **Prefetch** — `alloc()` prefetches `local_head->next` after pop

### 3.3 CachedAllocator — Self-Contained Pool

* Allocates one contiguous 64-byte-aligned region at startup (`_aligned_malloc` / `aligned_alloc`)
* Partitions into fixed-size blocks (each block ≥ 64B, cache-line aligned)
* Pushes all blocks into global freelist at init — **zero malloc after construction**
* Vends `thread_local ThreadCache` per calling thread
* RAII — destructor frees the region

### 3.4 MultiSizeAllocator — Size Classes

* Size classes: **16, 32, 64, 128, 256** bytes
* One `CachedAllocator` per class — fully independent pools
* `alloc(size)` rounds up to nearest class, dispatches
* No cross-size reuse — prevents fragmentation

### 3.5 NumaAllocator — NUMA-Aware Pools

* Detects NUMA topology at startup (Windows `GetNumaHighestNodeNumber` / Linux sysfs)
* Allocates per-node regions via `VirtualAllocExNuma` / `mmap` + `mbind`
* `alloc()` pulls from `numa::current_node()` — local pool first
* `dealloc()` returns to the **owning** node (address range check) — no cross-node pollution
* Fallback to other nodes only if local pool is empty
* Per-thread `ThreadCache` per NUMA node (lazily initialized)

### 3.6 Memory Layout Optimization

* `FreeBlock` is `alignas(64)` — one block per cache line
* `MemAllocator::head` is `alignas(64)` with padding
* `ThreadCache` is `alignas(64)` — prevents false sharing between thread-local instances
* Hardware prefetch (`__builtin_prefetch` / `_mm_prefetch`):
  * `pop()` → prefetches `old_head.ptr->next` before CAS
  * `pop_bulk()` → prefetches `cut->next->next` during walk
  * `ThreadCache::alloc()` → prefetches next-next after pop

---

## 4. Allocation Flow

### Fast Path (>99% of operations)

```
ThreadCache::alloc():
    pop from local_head   <- no atomics, no CAS, ~0ns
```

### Refill Path (triggered at low_watermark)

```
ThreadCache::refill():
    batch = global.pop_bulk(batch_size)   <- single CAS
    prepend batch to local_head
```

### Free Path

```
ThreadCache::dealloc():
    push to local_head   <- no atomics

    if local_size > high_watermark:
        flush(local_size - batch_size)   <- single CAS to return excess
```

---

## 5. Memory Ordering

| Operation              | Ordering                                    |
|------------------------|---------------------------------------------|
| Thread-local alloc/free| No atomics                                  |
| Global push / push_bulk| `memory_order_release` on CAS success       |
| Global pop / pop_bulk  | `memory_order_acquire` on load and CAS      |
| CAS failure            | `memory_order_relaxed`                      |
| `seq_cst`              | **Never used**                              |

---

## 6. Instrumentation

Compile-time toggles with zero release-mode cost:

| Define                  | Effect                                                |
|-------------------------|-------------------------------------------------------|
| `ALLOC_INSTRUMENTATION` | Per-thread counters: allocs, deallocs, refills, flushes, block counts |
| `ALLOC_DEBUG`           | Per-thread `unordered_set<void*>` tracking live allocations; detects double-free and invalid pointer |

Both expand to `((void)0)` when not defined.

---

## 7. Benchmarking

Two realistic scenarios (no trivial alloc→free loops):

1. **Burst alloc + delayed free** — allocates in bursts of 128, holds 16–256 blocks before freeing. Measures contention under asymmetric pressure.

2. **Cross-thread free (producer/consumer)** — one thread allocates and enqueues; another dequeues and frees. Tests ownership transfer across threads.

Metrics: **throughput** (ops/sec) and **latency percentiles** (p50, p99) for both alloc and free, across 1/2/4/8 threads.


### Burst Alloc + Delayed Free

| Allocator       | Threads | p50    | p99      | Throughput    |
| --------------- | ------- | ------ | -------- | ------------- |
| MemAllocator    | 1       | 50 ns  | 130 ns   | ~5.0M ops/s   |
| MemAllocator    | 4       | 270 ns | 3,000 ns | ~3.7M ops/s   |
| MemAllocator    | 8       | 330 ns | 8,500 ns | ~3.1M ops/s   |
| CachedAllocator | 1       | 40 ns  | 200 ns   | ~5.0M ops/s   |
| CachedAllocator | 4       | 40 ns  | 2,100 ns | ~13.5M ops/s  |
| CachedAllocator | 8       | 60 ns  | 2,300 ns | ~20.0M ops/s  |
| malloc/free     | 1       | 40 ns  | 200 ns   | ~4.5M ops/s   |
| malloc/free     | 4       | 40 ns  | 200 ns   | ~14.0M ops/s  |
| malloc/free     | 8       | 60 ns  | 200 ns   | ~22–31M ops/s |

---

### Cross-Thread Free

| Allocator       | Threads | p50    | p99      | Throughput  |
| --------------- | ------- | ------ | -------- | ----------- |
| MemAllocator    | 2       | 120 ns | 400 ns   | ~3.0M ops/s |
| MemAllocator    | 8       | 500 ns | 5,000 ns | ~3.0M ops/s |
| CachedAllocator | 2       | 40 ns  | 2,300 ns | ~2.5M ops/s |
| CachedAllocator | 8       | 60 ns  | 1,100 ns | ~8.0M ops/s |
| malloc/free     | 2       | 40 ns  | 700 ns   | ~2.2M ops/s |
| malloc/free     | 8       | 60 ns  | 750 ns   | ~6.5M ops/s |

---

```
Observations:
- Global lock-free freelist does not scale due to CAS contention
- Thread-local caching significantly improves throughput
- Tail latency (p99) remains a challenge under high contention
- glibc malloc still outperforms in tail latency due to mature optimizations
```



Thread-local caching removes most of the contention bottleneck of the global freelist
(p99 improves from ~10 μs → ~2 μs at 8 threads), but tail latency is still ~10× worse
than malloc/free, indicating remaining contention and cross-thread inefficiencies.

---

## 8. Tradeoffs Summary

| Decision             | Benefit                   | Cost                              |
|----------------------|---------------------------|-----------------------------------|
| Thread-local cache   | Eliminates contention     | Memory overhead per thread        |
| Batch operations     | Fewer CAS ops             | Slight spike if batch too large   |
| Tagged pointers      | ABA safety                | 128-bit CAS (requires libatomic)  |
| Watermark flow ctrl  | Bounded memory            | Slight refill latency             |
| NUMA pools           | Avoids cross-node access  | Complexity, detection overhead    |
| Size classes         | No fragmentation          | More pools, memory per class      |
| Cache-line alignment | No false sharing          | 64B minimum block size            |
| Hardware prefetch    | Hides pointer-chase cost  | Wasted prefetch if CAS retries    |
