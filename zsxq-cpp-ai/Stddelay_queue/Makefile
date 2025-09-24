# Makefile for Delay Queue
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread
TARGET = test_delay_queue
SOURCE = test_delay_queue.cpp
HEADER = delay_queue.hpp

# 默认目标
all: $(TARGET)

# 编译测试程序
$(TARGET): $(SOURCE) $(HEADER)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCE)

# 运行测试
run: $(TARGET)
	./$(TARGET)

# 清理生成的文件
clean:
	rm -f $(TARGET)

# 调试版本
debug: CXXFLAGS = -std=c++11 -Wall -Wextra -g -pthread -DDEBUG
debug: $(TARGET)

.PHONY: all run clean debug