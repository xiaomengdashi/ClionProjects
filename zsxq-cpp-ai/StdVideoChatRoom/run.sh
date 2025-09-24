#!/bin/bash

# 检查是否存在证书
if [ ! -f "certificates/server.crt" ] || [ ! -f "certificates/server.key" ]; then
    echo "未找到SSL证书，正在生成..."
    ./generate_certificates.sh
fi

# 检查是否存在可执行文件
if [ ! -f "build/chat_server" ]; then
    echo "未找到可执行文件，正在构建..."
    ./build.sh
fi

# 运行服务器
echo "启动视频聊天服务器..."
echo "HTTPS端口: 8443"
echo "WebSocket端口: 9443"
echo "访问地址: https://localhost:8443"
echo ""
echo "注意：由于使用自签名证书，浏览器会显示安全警告，请选择继续访问"
echo "按 Ctrl+C 停止服务器"
echo ""

./build/chat_server 8443 9443 