BUILD_DIR = build
CONFIG = Release

CORES = 1,3,5,7

PERF_EVENTS = cycles,instructions,branches,branch-misses,cache-references,cache-misses,L1-dcache-loads,L1-dcache-load-misses

ARGS ?= --size 64 --threads 8 --ops 5000000 --batch 256 --pool 1000000

all:
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=$(CONFIG)
	cmake --build $(BUILD_DIR)

debug:
	cmake -B $(BUILD_DIR) -G Ninja -DCMAKE_BUILD_TYPE=Debug
	cmake --build $(BUILD_DIR)

clean:
	rm -rf $(BUILD_DIR)

asan:
	cmake -B $(BUILD_DIR) -G Ninja \
	-DCMAKE_BUILD_TYPE=Debug \
	-DENABLE_ASAN=ON

	cmake --build $(BUILD_DIR)

# RUN

run-%:
	taskset -c $(CORES) ./$(BUILD_DIR)/$* $(ARGS)

# Example:
# make run-tl_bitmap_same_thread
#
# make run-tl_bitmap_same_thread \
# ARGS="--size 64 --threads 8 --ops 5000000 --batch 256 --pool 1000000"

# PERF STAT

perf-%:
	perf stat -e $(PERF_EVENTS) \
	taskset -c $(CORES) ./$(BUILD_DIR)/$* $(ARGS)

# Example:
# sudo make perf-tl_bitmap_same_thread

# PERF RECORD

record-%:
	perf record -g \
	taskset -c $(CORES) ./$(BUILD_DIR)/$* $(ARGS)

# Example:
# sudo make record-mimalloc_same_thread

# PERF C2C

c2c-%:
	perf c2c record \
	taskset -c $(CORES) ./$(BUILD_DIR)/$* $(ARGS)

# PERF REPORT

report:
	perf report