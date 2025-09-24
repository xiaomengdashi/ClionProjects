CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -pthread -O2

.PHONY: all clean

all: test_mem_pool benchmark_mem_pool

test_mem_pool: test_mem_pool.cpp StdIndexedMemPool.h
	$(CXX) $(CXXFLAGS) -o $@ $<

benchmark_mem_pool: benchmark_mem_pool.cpp StdIndexedMemPool.h
	$(CXX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f test_mem_pool benchmark_mem_pool 