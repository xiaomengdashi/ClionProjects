#!/bin/bash

# 单独编译项目B的脚本

set -e  # 遇到错误时退出

echo "=== 单独编译项目B ==="

# 检查项目A静态库是否存在
PROJECT_A_LIB="build/lib/libProjectA.a"
if [ ! -f "$PROJECT_A_LIB" ]; then
    echo "错误: 未找到项目A静态库 ($PROJECT_A_LIB)"
    echo "请先编译项目A或运行完整构建:"
    echo "  方法1: ./build.sh (完整构建)"
    echo "  方法2: 先编译项目A"
    echo "    mkdir -p build && cd build"
    echo "    cmake .."
    echo "    make ProjectA"
    echo "    cd .."
    exit 1
fi

echo "✓ 找到项目A静态库: $PROJECT_A_LIB"

# 创建构建目录（如果不存在）
echo "准备构建目录..."
mkdir -p build
cd build

# 配置CMake（如果需要）
if [ ! -f "Makefile" ]; then
    echo "配置CMake..."
    cmake ..
fi

# 只编译项目B
echo "编译项目B..."
make ProjectB

echo "\n=== 项目B编译完成 ==="
echo "可执行文件: build/bin/ProjectB"

echo "\n运行项目B:"
echo "./bin/ProjectB"

echo "\n或者从项目根目录运行:"
echo "./build/bin/ProjectB"