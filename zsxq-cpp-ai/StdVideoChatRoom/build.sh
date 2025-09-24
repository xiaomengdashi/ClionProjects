#!/bin/bash

# 创建构建目录
mkdir -p build

# 进入构建目录
cd build

# 运行CMake
echo "配置项目..."
cmake .. -DCMAKE_BUILD_TYPE=Release

# 编译项目
echo "编译项目..."
make -j$(nproc)

if [ $? -eq 0 ]; then
    echo "构建成功！"
    echo "可执行文件位于: build/chat_server"
else
    echo "构建失败！"
    exit 1
fi 