## 项目简介

StdFTP是一个使用C++11从零开始实现的完整FTP（文件传输协议）系统，包含功能齐全的FTP服务器和客户端。该项目展示了网络编程、多线程处理、文件系统操作等高级编程技术。

## 功能特性

### FTP服务器
- ✅ 完整的FTP协议实现（RFC 959）
- ✅ 多客户端并发连接支持
- ✅ 用户认证系统
- ✅ 主动模式（PORT）和被动模式（PASV）数据传输
- ✅ 文件上传/下载
- ✅ 目录浏览和管理
- ✅ ASCII和二进制传输模式
- ✅ 会话管理和超时处理

### FTP客户端
- ✅ 交互式命令行界面
- ✅ 完整的FTP命令支持
- ✅ 文件上传/下载进度显示
- ✅ 自动被动模式连接
- ✅ 详细/静默输出模式

## 项目结构

```
├── include/            # 头文件目录
│   ├── ftp_protocol.h  # FTP协议定义
│   └── ftp_session.h   # 会话管理
├── common/             # 公共代码
│   ├── ftp_protocol.cpp
│   └── ftp_session.cpp
├── server/             # 服务器代码
│   └── ftp_server.cpp
├── client/             # 客户端代码
│   └── ftp_client.cpp
├── CMakeLists.txt      # CMake构建文件
├── Makefile            # Make构建文件
├── build.sh            # 构建脚本
└── README.md           # 本文档
```

### 快速构建

使用构建脚本（推荐）：
```bash
chmod +x build.sh
./build.sh        # 默认使用Make构建
./build.sh cmake  # 使用CMake构建
```

使用Make：
```bash
make              # 构建发布版本
make debug        # 构建调试版本
make clean        # 清理构建文件
```

使用CMake：
```bash
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## 使用方法

### 启动FTP服务器

```bash
# 使用默认设置（端口21，需要root权限）
sudo ./bin/ftp_server

# 指定端口和目录（推荐用于测试）
./bin/ftp_server -p 2121 -d /tmp/ftp

# 查看帮助
./bin/ftp_server --help
```

服务器选项：
- `-p, --port <端口>`: 指定监听端口（默认21）
- `-d, --dir <目录>`: 指定FTP根目录（默认/tmp/ftp）
- `-h, --help`: 显示帮助信息

### 使用FTP客户端

```bash
# 启动客户端
./bin/ftp_client

# 在客户端中连接服务器
ftp> open localhost 2121
ftp> login testuser
密码: [输入任意密码]
ftp> ls
ftp> quit
```

## 客户端命令列表

| 命令 | 说明 | 示例 |
|------|------|------|
| `open <主机> [端口]` | 连接到FTP服务器 | `open localhost 2121` |
| `login <用户名>` | 登录服务器 | `login admin` |
| `close` | 关闭当前连接 | `close` |
| `pwd` | 显示当前目录 | `pwd` |
| `cd <目录>` | 改变目录 | `cd /home` |
| `ls [路径]` | 列出目录内容 | `ls` |
| `get <远程文件> [本地文件]` | 下载文件 | `get file.txt` |
| `put <本地文件> [远程文件]` | 上传文件 | `put local.txt` |
| `mkdir <目录名>` | 创建目录 | `mkdir newfolder` |
| `rmdir <目录名>` | 删除目录 | `rmdir oldfolder` |
| `delete <文件名>` | 删除文件 | `delete old.txt` |
| `binary` | 设置二进制传输模式 | `binary` |
| `ascii` | 设置ASCII传输模式 | `ascii` |
| `verbose` | 切换详细输出模式 | `verbose` |
| `help` | 显示帮助信息 | `help` |
| `quit` | 退出程序 | `quit` |

## 测试示例

### 基本功能测试

1. 启动服务器：
```bash
./bin/ftp_server -p 2121 -d /tmp/ftp
```

2. 在另一个终端启动客户端并测试：
```bash
./bin/ftp_client
ftp> open localhost 2121
ftp> login test
密码: test
ftp> pwd
ftp> mkdir testdir
ftp> cd testdir
ftp> put README.md
ftp> ls
ftp> get README.md README_copy.md
ftp> delete README.md
ftp> cd ..
ftp> rmdir testdir
ftp> quit
```

### 性能测试

```bash
# 创建测试文件
dd if=/dev/zero of=test_100M.bin bs=1M count=100

# 上传大文件
ftp> binary
ftp> put test_100M.bin

# 下载大文件
ftp> get test_100M.bin test_download.bin
```

## 支持的FTP命令

### 访问控制命令
- USER - 用户名
- PASS - 密码
- QUIT - 退出
- CWD - 改变工作目录
- CDUP - 改变到父目录

### 传输参数命令
- PORT - 主动模式数据端口
- PASV - 被动模式
- TYPE - 传输类型（A=ASCII, I=二进制）

### 服务命令
- RETR - 下载文件
- STOR - 上传文件
- LIST - 详细目录列表
- NLST - 简单文件名列表
- MKD - 创建目录
- RMD - 删除目录
- DELE - 删除文件
- PWD - 打印工作目录
- SIZE - 获取文件大小
- SYST - 系统类型
- NOOP - 空操作