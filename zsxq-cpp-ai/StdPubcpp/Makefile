# C++消息队列框架 - Makefile
# 使用C++17标准，支持多线程

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -O2 -pthread
DEBUG_FLAGS = -g -DDEBUG

# 目标程序名
TARGET = message_queue_demo

# 源文件和头文件
SOURCES = main.cpp
HEADERS = message.h subscriber.h message_queue.h publisher.h sample_subscribers.h

# 目标规则
all: $(TARGET)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SOURCES)

# 调试版本
debug: $(SOURCES) $(HEADERS)
	$(CXX) $(CXXFLAGS) $(DEBUG_FLAGS) -o $(TARGET)_debug $(SOURCES)

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 运行调试版本
run-debug: debug
	./$(TARGET)_debug

# 清理编译产物
clean:
	rm -f $(TARGET) $(TARGET)_debug *.o message_log.txt

# 清理所有（包括日志文件）
clean-all: clean
	rm -f *.log *.txt

# 帮助信息
help:
	@echo "C++消息队列框架 - Makefile使用说明"
	@echo "=================================="
	@echo "make          - 编译项目（优化版本）"
	@echo "make debug    - 编译调试版本"
	@echo "make run      - 编译并运行程序"
	@echo "make run-debug- 编译并运行调试版本"
	@echo "make clean    - 清理编译产物"
	@echo "make clean-all- 清理所有文件（包括日志）"
	@echo "make help     - 显示此帮助信息"

.PHONY: all debug run run-debug clean clean-all help