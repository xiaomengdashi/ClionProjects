# C++聊天室项目 Makefile
# 支持多人聊天、私聊、用户注册/登录、数据持久存储

# 编译器设置
CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -O2 -pthread

# 目标文件
SERVER_TARGET = server
CLIENT_TARGET = client

# 源文件
SERVER_SOURCES = server_main.cpp ChatServer.cpp UserManager.cpp User.cpp Message.cpp FileInfo.cpp FileManager.cpp
CLIENT_SOURCES = client_main.cpp ChatClient.cpp Message.cpp FileInfo.cpp

# 对象文件目录
OBJDIR = obj

# 对象文件
SERVER_OBJECTS = $(addprefix $(OBJDIR)/, $(SERVER_SOURCES:.cpp=.o))
CLIENT_OBJECTS = $(addprefix $(OBJDIR)/, $(CLIENT_SOURCES:.cpp=.o))

# 依赖文件
DEPENDENCIES = Message.h User.h UserManager.h ChatServer.h ChatClient.h FileInfo.h FileManager.h

# 默认目标
all: $(SERVER_TARGET) $(CLIENT_TARGET)

# 服务器目标
$(SERVER_TARGET): $(SERVER_OBJECTS)
	@echo "正在链接服务器程序..."
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "服务器编译完成: $@"

# 客户端目标
$(CLIENT_TARGET): $(CLIENT_OBJECTS)
	@echo "正在链接客户端程序..."
	$(CXX) $(CXXFLAGS) -o $@ $^
	@echo "客户端编译完成: $@"

# 编译规则
$(OBJDIR)/%.o: %.cpp $(DEPENDENCIES) | $(OBJDIR)
	@echo "正在编译: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 创建对象文件目录
$(OBJDIR):
	@echo "创建对象文件目录: $(OBJDIR)"
	mkdir -p $(OBJDIR)

# 清理目标
clean:
	@echo "清理编译文件..."
	rm -rf $(OBJDIR) $(SERVER_TARGET) $(CLIENT_TARGET)
	rm -f users.dat  # 清理用户数据文件
	@echo "清理完成"

# 清理对象文件但保留可执行文件
clean-obj:
	@echo "清理对象文件..."
	rm -rf $(OBJDIR)
	@echo "对象文件清理完成"

# 安装目标（可选）
install: all
	@echo "安装程序到 /usr/local/bin/"
	sudo cp $(SERVER_TARGET) /usr/local/bin/chatroom-server
	sudo cp $(CLIENT_TARGET) /usr/local/bin/chatroom-client
	@echo "安装完成"

# 卸载目标（可选）
uninstall:
	@echo "卸载程序..."
	sudo rm -f /usr/local/bin/chatroom-server
	sudo rm -f /usr/local/bin/chatroom-client
	@echo "卸载完成"

# 运行服务器（开发用）
run-server: $(SERVER_TARGET)
	@echo "启动聊天室服务器..."
	./$(SERVER_TARGET)

# 运行客户端（开发用）
run-client: $(CLIENT_TARGET)
	@echo "启动聊天室客户端..."
	./$(CLIENT_TARGET)

# 测试编译（检查语法错误）
test-compile: 
	@echo "测试编译所有源文件..."
	$(CXX) $(CXXFLAGS) -fsyntax-only $(SERVER_SOURCES) $(CLIENT_SOURCES)
	@echo "语法检查通过"

# 显示帮助信息
help:
	@echo "C++聊天室项目 Makefile 使用说明:"
	@echo ""
	@echo "编译目标:"
	@echo "  all          - 编译服务器和客户端（默认）"
	@echo "  server       - 只编译服务器"
	@echo "  client       - 只编译客户端"
	@echo ""
	@echo "清理目标:"
	@echo "  clean        - 清理所有编译文件和数据文件"
	@echo "  clean-obj    - 只清理对象文件"
	@echo ""
	@echo "运行目标:"
	@echo "  run-server   - 编译并运行服务器"
	@echo "  run-client   - 编译并运行客户端"
	@echo ""
	@echo "其他目标:"
	@echo "  install      - 安装到系统目录"
	@echo "  uninstall    - 从系统目录卸载"
	@echo "  test-compile - 测试编译（语法检查）"
	@echo "  help         - 显示此帮助信息"
	@echo ""
	@echo "使用示例:"
	@echo "  make                    # 编译所有程序"
	@echo "  make clean && make      # 清理后重新编译"
	@echo "  make run-server         # 运行服务器"
	@echo "  make run-client         # 运行客户端"

# 声明伪目标
.PHONY: all clean clean-obj install uninstall run-server run-client test-compile help

# 项目信息
info:
	@echo "=== C++多人聊天室项目信息 ==="
	@echo "版本: 1.0"
	@echo "编译器: $(CXX)"
	@echo "编译选项: $(CXXFLAGS)"
	@echo "功能特性:"
	@echo "  - 多用户并发聊天"
	@echo "  - 用户注册和登录系统"
	@echo "  - 账号数据持久存储"
	@echo "  - 群聊和私聊支持"
	@echo "  - 在线用户列表"
	@echo "  - 中文界面和注释"
	@echo "=========================="