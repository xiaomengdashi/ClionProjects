# Makefile for Load Balancing Algorithms Study
# 负载均衡算法学习案例 Makefile

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread

# 源文件和目标文件
SOURCES = examples/main.cpp
PERF_SOURCES = examples/performance_test.cpp
HEADERS = include/server/Server.h include/balancer/LoadBalancer.h include/balancer/RoundRobinBalancer.h include/balancer/ConsistentHashBalancer.h include/balancer/LeastConnectionsBalancer.h
TARGET = load_balancer_demo
PERF_TARGET = performance_test

# 默认目标
all: $(TARGET) $(PERF_TARGET)

# 编译规则
$(TARGET): $(SOURCES) $(HEADERS)
	@echo "正在编译负载均衡算法学习案例..."
	$(CXX) $(CXXFLAGS) -Iinclude $(SOURCES) -o $(TARGET)
	@echo "编译完成！运行 ./$(TARGET) 查看演示"

# 编译性能测试程序
$(PERF_TARGET): $(PERF_SOURCES) $(HEADERS)
	@echo "正在编译性能测试程序..."
	$(CXX) $(CXXFLAGS) -Iinclude $(PERF_SOURCES) -o $(PERF_TARGET)
	@echo "性能测试程序编译完成！运行 ./$(PERF_TARGET) 进行性能测试"

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 运行性能测试
perf: $(PERF_TARGET)
	./$(PERF_TARGET)

# 调试版本
debug: CXXFLAGS = -std=c++11 -Wall -Wextra -g -pthread -DDEBUG
debug: $(TARGET)
	@echo "调试版本编译完成"

# 清理编译文件
clean:
	rm -f $(TARGET) $(PERF_TARGET)
	@echo "清理完成"

# 帮助信息
help:
	@echo "使用方法:"
	@echo "  make        - 编译所有程序"
	@echo "  make run    - 编译并运行演示程序"
	@echo "  make perf   - 编译并运行性能测试"
	@echo "  make debug  - 编译调试版本"
	@echo "  make clean  - 清理编译文件"
	@echo "  make help   - 显示帮助信息"

.PHONY: all run perf debug clean help