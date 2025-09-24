#!/bin/bash

# 创建证书目录
mkdir -p certificates

# 生成私钥
openssl genrsa -out certificates/server.key 2048

# 生成证书签名请求
openssl req -new -key certificates/server.key -out certificates/server.csr \
    -subj "/C=CN/ST=Beijing/L=Beijing/O=VideoChat/OU=Development/CN=localhost"

# 生成自签名证书（有效期365天）
openssl x509 -req -days 365 -in certificates/server.csr -signkey certificates/server.key -out certificates/server.crt

# 生成DH参数文件
openssl dhparam -out certificates/dh.pem 2048

# 清理临时文件
rm certificates/server.csr

echo "证书生成完成！"
echo "证书文件位置："
echo "  - certificates/server.crt"
echo "  - certificates/server.key"
echo "  - certificates/dh.pem" 