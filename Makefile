CXX = g++
CXXFLAGS = -std=c++20 -O3 -march=native -flto -pthread -Iinclude

SRC = src/pool_allocator.cpp
BENCH = benchmark/benchmark.cpp
BUILD_DIR = build

OUT = $(BUILD_DIR)/benchmark_app

all: $(OUT)

$(OUT): $(SRC) $(BENCH)
	mkdir -p $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

run: $(OUT)
	./$(OUT)

clean:
	rm -rf $(BUILD_DIR)