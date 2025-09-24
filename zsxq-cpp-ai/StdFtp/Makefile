# FTP服务器和客户端的Makefile
# 使用C++11标准编译

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Wpedantic -I./include
LDFLAGS = -pthread -lstdc++fs

# 调试和发布版本标志
DEBUG_FLAGS = -g -O0 -DDEBUG
RELEASE_FLAGS = -O3 -DNDEBUG

# 目录
SRCDIR = .
BINDIR = bin
OBJDIR = obj

# 源文件
COMMON_SRCS = common/ftp_protocol.cpp common/ftp_session.cpp
SERVER_SRCS = server/ftp_server.cpp $(COMMON_SRCS)
CLIENT_SRCS = client/ftp_client.cpp common/ftp_protocol.cpp
TEST_SRCS = test/main.cpp common/ftp_protocol.cpp common/ftp_session.cpp

# 目标文件
SERVER_OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(SERVER_SRCS))
CLIENT_OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(CLIENT_SRCS))
TEST_OBJS = $(patsubst %.cpp,$(OBJDIR)/%.o,$(TEST_SRCS))

# 可执行文件
SERVER_TARGET = $(BINDIR)/ftp_server
CLIENT_TARGET = $(BINDIR)/ftp_client
TEST_TARGET = $(BINDIR)/ftp_test

# 默认目标
.PHONY: all
all: release

# 发布版本
.PHONY: release
release: CXXFLAGS += $(RELEASE_FLAGS)
release: build

# 调试版本
.PHONY: debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: build

# 构建所有目标
.PHONY: build
build: directories $(SERVER_TARGET) $(CLIENT_TARGET) $(TEST_TARGET)
	@echo "=========================================="
	@echo "构建完成!"
	@echo "服务器: $(SERVER_TARGET)"
	@echo "客户端: $(CLIENT_TARGET)"
	@echo "=========================================="

# 创建必要的目录
.PHONY: directories
directories:
	@mkdir -p $(BINDIR)
	@mkdir -p $(OBJDIR)/common
	@mkdir -p $(OBJDIR)/server
	@mkdir -p $(OBJDIR)/client
	@mkdir -p /tmp/ftp

# 构建服务器
$(SERVER_TARGET): $(SERVER_OBJS)
	@echo "链接服务器: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 构建客户端
$(CLIENT_TARGET): $(CLIENT_OBJS)
	@echo "链接客户端: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# 构建测试
$(TEST_TARGET): $(TEST_OBJS)
	@echo "链接测试: $@"
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS) -lgtest -lgtest_main

# 编译规则
$(OBJDIR)/%.o: %.cpp
	@echo "编译: $<"
	$(CXX) $(CXXFLAGS) -c -o $@ $<

# 清理
.PHONY: clean
clean:
	@echo "清理构建文件..."
	@rm -rf $(OBJDIR) $(BINDIR)

# 运行服务器
.PHONY: run-server
run-server: $(SERVER_TARGET)
	@echo "启动FTP服务器..."
	@$(SERVER_TARGET) -p 2121 -d /tmp/ftp

# 运行客户端
.PHONY: run-client
run-client: $(CLIENT_TARGET)
	@echo "启动FTP客户端..."
	@$(CLIENT_TARGET)

# 运行测试
.PHONY: run-test
run-test: $(TEST_TARGET)
	@echo "运行测试..."
	@$(TEST_TARGET)

# 安装
.PHONY: install
install: release
	@echo "安装到系统..."
	@sudo cp $(SERVER_TARGET) /usr/local/bin/
	@sudo cp $(CLIENT_TARGET) /usr/local/bin/
	@sudo mkdir -p /var/ftp
	@echo "安装完成!"

# 卸载
.PHONY: uninstall
uninstall:
	@echo "从系统卸载..."
	@sudo rm -f /usr/local/bin/ftp_server
	@sudo rm -f /usr/local/bin/ftp_client
	@echo "卸载完成!"

# 帮助信息
.PHONY: help
help:
	@echo "FTP服务器和客户端 Makefile"
	@echo "=========================================="
	@echo "可用目标:"
	@echo "  all/release  - 构建发布版本（默认）"
	@echo "  debug        - 构建调试版本"
	@echo "  clean        - 清理所有构建文件"
	@echo "  run-server   - 运行FTP服务器"
	@echo "  run-client   - 运行FTP客户端"
	@echo "  run-test     - 运行测试"
	@echo "  install      - 安装到系统"
	@echo "  uninstall    - 从系统卸载"
	@echo "  help         - 显示此帮助信息"
	@echo "=========================================="
	@echo "示例:"
	@echo "  make             # 构建发布版本"
	@echo "  make debug       # 构建调试版本"
	@echo "  make run-server  # 运行服务器"
	@echo "  make run-client  # 运行客户端"
	@echo "  make run-test    # 运行测试"

# 依赖关系（自动生成）
-include $(SERVER_OBJS:.o=.d)
-include $(CLIENT_OBJS:.o=.d)
-include $(TEST_OBJS:.o=.d)

# 生成依赖文件
$(OBJDIR)/%.d: %.cpp
	@mkdir -p $(dir $@)
	@$(CXX) $(CXXFLAGS) -MM -MT '$(@:.d=.o)' $< > $@