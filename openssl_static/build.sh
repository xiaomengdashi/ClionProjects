#!/bin/bash

# OpenSSL静态库项目构建脚本

set -e  # 遇到错误时退出

echo "=== OpenSSL静态库项目构建 ==="

# 检查OpenSSL是否安装
if ! command -v openssl &> /dev/null; then
    echo "警告: 未找到OpenSSL命令，请确保已安装OpenSSL"
    echo "在macOS上可以使用: brew install openssl"
fi

# 创建构建目录
echo "创建构建目录..."
mkdir -p build
cd build

# 配置CMake
echo "配置CMake..."
cmake ..

# 编译项目
echo "编译项目..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "\n=== 构建完成 ==="
echo "项目A静态库: build/lib/libProjectA.a"
echo "项目B可执行文件: build/bin/ProjectB"

echo "\n运行项目B演示:"
echo "./build/bin/ProjectB"

echo "\n或者直接运行:"
./build/bin/ProjectB