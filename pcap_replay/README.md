# PCAP 文件回放程序

这是一个用于回放 PCAP 文件的网络程序，包含客户端和服务端，能够解析 PCAP 文件并根据数据包的方向进行回放，同时替换 IP 信息。

## 功能特性

- **PCAP 文件解析**: 支持解析标准 PCAP 文件格式
- **双向数据回放**: 根据数据包方向分别由服务端或客户端发送
- **IP 地址替换**: 自动替换原始 IP 地址为新的 IP 地址
- **协议支持**: 支持 TCP 和 UDP 协议
- **实时回放**: 模拟真实网络环境的数据包传输
- **多种运行模式**: 支持独立的服务端/客户端模式和统一回放模式

## 项目结构

```
pcap_replay/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 项目说明文档
├── include/                    # 头文件目录
│   ├── pcap_parser.h          # PCAP 文件解析器
│   ├── packet_modifier.h      # 数据包修改器
│   └── network_utils.h        # 网络工具类
├── src/                       # 源文件目录
│   ├── pcap_parser.cpp        # PCAP 解析实现
│   ├── packet_modifier.cpp    # 数据包修改实现
│   ├── network_utils.cpp      # 网络工具实现
│   ├── server.cpp             # 服务端程序
│   ├── client.cpp             # 客户端程序
│   └── replayer.cpp           # 统一回放程序
└── tools/                     # 工具目录
    └── pcap_generator.cpp     # 示例 PCAP 文件生成器
```

## 编译要求

- C++17 或更高版本
- CMake 3.10 或更高版本
- libpcap 开发库
- POSIX 兼容系统 (Linux/macOS)

### 安装依赖 (macOS)

```bash
# 使用 Homebrew 安装 libpcap
brew install libpcap

# 或者使用系统自带的 libpcap (通常已安装)
```

### 安装依赖 (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install libpcap-dev cmake build-essential
```

## 编译步骤

```bash
# 创建构建目录
mkdir build
cd build

# 配置项目
cmake ..

# 编译项目
make -j$(nproc)
```

编译完成后，将生成以下可执行文件：
- `pcap_server`: 服务端程序
- `pcap_client`: 客户端程序  
- `pcap_replayer`: 统一回放程序
- `pcap_generator`: PCAP 文件生成器

## 使用方法

### 1. 生成示例 PCAP 文件

```bash
./build/pcap_generator sample.pcap
```

### 2. 统一回放模式 (推荐)

```bash
./build/pcap_replayer <PCAP文件> <原始服务端IP> <原始客户端IP> <新服务端IP> <新客户端IP> [端口]
```

示例：
```bash
./build/pcap_replayer sample.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1 8080
```

### 3. 分离模式

#### 启动服务端
```bash
./build/pcap_server <监听IP> <监听端口> <PCAP文件> <原始服务端IP> <原始客户端IP> <新服务端IP> <新客户端IP>
```

示例：
```bash
./build/pcap_server 127.0.0.1 8080 sample.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1
```

#### 启动客户端
```bash
./build/pcap_client <服务端IP> <服务端端口> <PCAP文件> <原始服务端IP> <原始客户端IP> <新客户端IP> [新服务端IP]
```

示例：
```bash
./build/pcap_client 127.0.0.1 8080 sample.pcap 192.168.1.100 192.168.1.200 127.0.0.1 127.0.0.1
```

## 工作原理

### 数据包方向判断

程序通过分析 PCAP 文件中数据包的源 IP 和目标 IP 来判断数据包的方向：

- **服务端到客户端**: 源 IP = 原始服务端 IP，目标 IP = 原始客户端 IP
  - 由服务端程序发送给连接的客户端
- **客户端到服务端**: 源 IP = 原始客户端 IP，目标 IP = 原始服务端 IP  
  - 由客户端程序发送给服务端

### IP 地址替换

程序会自动将数据包中的 IP 地址进行替换：

- 原始服务端 IP → 新服务端 IP
- 原始客户端 IP → 新客户端 IP

这样可以在不同的网络环境中回放相同的 PCAP 文件。

### 数据包处理流程

1. **解析阶段**: 读取 PCAP 文件，解析以太网帧、IP 头、TCP/UDP 头
2. **修改阶段**: 根据配置的 IP 映射关系修改数据包信息
3. **回放阶段**: 根据数据包方向由相应的程序发送数据
4. **接收阶段**: 对端程序接收数据并进行处理

## 核心组件

### PcapParser
- 负责解析 PCAP 文件格式
- 提取数据包的网络层和传输层信息
- 支持过滤器设置

### PacketModifier  
- 实现 IP 地址映射和替换
- 重新计算校验和
- 构建完整的网络数据包

### NetworkUtils
- 提供网络套接字操作封装
- 支持 TCP/UDP 套接字创建和管理
- 实现数据发送和接收功能

## 示例输出

```
启动PCAP回放程序...
PCAP文件: sample.pcap
原始服务端IP: 192.168.1.100
原始客户端IP: 192.168.1.200
新服务端IP: 127.0.0.1
新客户端IP: 127.0.0.1
服务端口: 8080

服务端监听在 127.0.0.1:8080
客户端已连接到服务端
客户端已连接: 127.0.0.1:54321

开始回放PCAP文件: sample.pcap
客户端发送数据包 #1 (载荷: 38 字节)
服务端收到数据: 38 字节
服务端发送数据包 #2 (载荷: 55 字节)
客户端收到数据: 55 字节
...
PCAP回放完成。总数据包: 4, 服务端发送: 2, 客户端发送: 2
```

## 注意事项

1. **权限要求**: 某些系统可能需要 root 权限来创建原始套接字
2. **防火墙设置**: 确保防火墙允许程序使用的端口
3. **PCAP 文件格式**: 目前仅支持标准的 libpcap 格式
4. **网络协议**: 主要支持 TCP 和 UDP 协议的数据包回放
5. **IP 地址格式**: 确保提供的 IP 地址格式正确

## 故障排除

### 编译错误
- 确保安装了 libpcap 开发库
- 检查 CMake 版本是否满足要求
- 验证 C++ 编译器支持 C++17

### 运行时错误
- 检查 PCAP 文件是否存在且可读
- 验证 IP 地址格式是否正确
- 确保端口未被其他程序占用
- 检查网络连接和防火墙设置

### 性能优化
- 调整数据包发送间隔 (修改 `sleep_for` 参数)
- 增加接收缓冲区大小
- 使用更高效的数据结构存储数据包信息

## 扩展功能

可以考虑添加以下功能：
- 支持更多网络协议 (ICMP, SCTP 等)
- 添加数据包过滤和筛选功能
- 实现数据包修改和注入功能
- 支持多客户端并发连接
- 添加 Web 界面进行可视化管理
- 实现数据包统计和分析功能

## 许可证

本项目采用 MIT 许可证，详见 LICENSE 文件。