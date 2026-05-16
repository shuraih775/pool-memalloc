# Pool Memory Allocator

Fixed-size pool allocators experimenting with:

* lock-free freelists
* thread-local caches
* bitmap allocators
* batch transfers
* locality-focused allocation paths

Built for low-latency allocation workloads and allocator experimentation.

## Build

```bash
make
```

Debug:

```bash
make debug
```

ASAN:

```bash
make asan
```

Clean:

```bash
make clean
```

---

## Benchmarks

Run:

```bash
make run-tl_bitmap_same_thread
```

Override parameters:

```bash
make run-tl_bitmap_same_thread \
SIZE=64 \
THREADS=8 \
OPS=5000000 \
BATCH=256 \
POOL=1000000
```

Available benchmark families:

```txt
same_thread
alloc_burst
producer_consumer
locality_walk
alloc_storm
random_free
```

Available allocators:

```txt
malloc (external)
mimalloc (external)
freelist
tl_freelist
bitmap
tl_bitmap
```

Example:

```bash
make run-mimalloc_alloc_burst
make run-tl_freelist_locality_walk
make run-bitmap_random_free
```

---

## perf

perf stat:

```bash
sudo make perf-tl_bitmap_same_thread
```

perf record:

```bash
sudo make record-tl_bitmap_same_thread
```

perf c2c:

```bash
sudo make c2c-tl_bitmap_same_thread
```

perf report:

```bash
make report
```

---

## Usage

### Thread-local freelist

```cpp
#include "tl_freelist_allocator.hpp"

ThreadLocalFreelistAllocator alloc(
    64,
    1'000'000,
    64);

void* p = alloc.alloc();
alloc.dealloc(p);
```

### Bitmap allocator

```cpp
#include "bitmap_allocator.hpp"

BitmapAllocator alloc(
    64,
    1'000'000);

void* p = alloc.alloc();
alloc.dealloc(p);
```

### Multi-size allocator

```cpp
#include "multi_size_allocator.hpp"

MultiSizeAllocator alloc(100'000);

void* p = alloc.alloc(100);
alloc.dealloc(p, 100);
```

---

## Benchmark Config

| Benchmark Environment | Value                                                      |
| --------------------- | ---------------------------------------------------------- |
| CPU                   | Ryzen 5                                                    |
| RAM                   | 16GB                                                       |
| Pool Size             | 1,000,000 blocks                                           |
| Operations            | 5,000,000                                                  |
| Compiler              | GCC C++20                                                  |
| Build Type            | Release                                                    |
| Compiler Flags        | `-O3 -march=native -flto -pthread -fno-omit-frame-pointer` |
| Thread Pinning        | enabled (`taskset`)                                        |
| Core Affinity         | `1,3,5,7`                                                  |
| Sanitizers            | ASAN optional (`make asan`)                                |
| Benchmark Framework   | custom microbenchmark harness                              |
| Allocator Types       | freelist, tl_freelist, bitmap, tl_bitmap, malloc, mimalloc |


---

## Alloc Burst

### 64B — 8 Threads — Batch 256

| Allocator   | Time (ms) |
| ----------- | --------- |
| mimalloc    | 155       |
| malloc      | 410       |
| tl_bitmap   | 496       |
| bitmap      | 2025      |
| tl_freelist | 2307*     |
| freelist    | 9476*     |


---

## Locality Walk

### 64B — Batch 256

| Threads | mimalloc | malloc | freelist | tl_freelist | bitmap | tl_bitmap |
| ------- | -------- | ------ | -------- | ----------- | ------ | --------- |
| 1       | 884      | 1014   | 819      | 817         | 856    | 857       |
| 2       | 1358     | 1561   | 838      | 758         | 27681  | 1363      |
| 4       | 2750     | 3117   | 926      | 892         | 51399  | 2747      |
| 8       | 7609     | 8264   | 1789     | 1951        | 120280 | 7623      |

---

## Producer Consumer

### 64B — 8 Threads — Batch 256

| Allocator   | Time (ms) |
| ----------- | --------- |
| tl_freelist | 665       |
| mimalloc    | 755       |
| freelist    | 935       |
| tl_bitmap   | 966       |
| bitmap      | 1055      |
| malloc      | 1395      |

---

## Batch Sensitivity

### Alloc Burst — 64B — 8 Threads

| Batch | mimalloc | malloc | tl_freelist | bitmap | tl_bitmap |
| ----- | -------- | ------ | ----------- | ------ | --------- |
| 16    | 155      | 324    | 102         | 2563   | 203       |
| 64    | 128      | 305    | 88          | 2036   | 208       |
| 256   | 155      | 410    | 2307        | 2025   | 496       |

---

## Notes

* Thread-local caching drastically reduces bitmap allocator contention.
* Freelist allocators show strong locality characteristics under traversal-heavy workloads.
* Batch sizing heavily impacts thread-local freelist performance.
* Bitmap allocators degrade significantly under shared concurrent access patterns.
* Current freelist bulk-transfer implementation still needs correctness work under  tiny allocation sizes ( <=4).
