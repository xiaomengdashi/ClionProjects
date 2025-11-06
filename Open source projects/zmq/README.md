# ZeroMQ C++ 示例项目

这是一个全面展示 ZeroMQ 消息传递库的 C++ 示例项目。包含了 ZeroMQ 的四种主要通信模式和三种传输方式的完整实现。

## 项目概述

**ZeroMQ** 是一个高性能的异步消息传递库，提供了简洁的 API 用于构建分布式应用。本项目使用 **cppzmq**（ZeroMQ 的 C++ 包装库）实现了各种消息传递模式。

## 环境要求

- C++17 或更高版本
- ZeroMQ 库 (libzmq)
- cppzmq 头文件（系统已安装）
- CMake 3.10 或更高版本
- GCC 或 Clang 编译器

## 快速开始

### 1. 编译项目

```bash
cd /home/ubuntu/github/ClionProjects/Open\ source\ projects/zmq
mkdir -p build
cd build
cmake ..
make -j4
```

编译完成后，所有可执行文件都在 `build/` 目录中。

### 2. 项目结构

```
zmq/
├── CMakeLists.txt              # CMake 构建配置
├── README.md                   # 本文档
├── build/                      # 编译输出目录
│   ├── req_rep_server          # 请求-应答服务器
│   ├── req_rep_client          # 请求-应答客户端
│   ├── pub_sub_pub             # 发布-订阅发布者
│   ├── pub_sub_sub             # 发布-订阅订阅者
│   ├── push_pull_push          # 推送-拉取发送端
│   ├── push_pull_pull          # 推送-拉取接收端
│   ├── router_dealer_router    # 路由-经销服务器
│   ├── router_dealer_dealer    # 路由-经销客户端
│   ├── ipc_server              # IPC 服务器
│   ├── ipc_client              # IPC 客户端
│   └── inproc_demo             # INPROC 线程间通信
└── examples/
    ├── 01_request_reply/       # 请求-应答模式
    │   ├── server.cpp
    │   └── client.cpp
    ├── 02_pub_sub/             # 发布-订阅模式
    │   ├── publisher.cpp
    │   └── subscriber.cpp
    ├── 03_push_pull/           # 推送-拉取模式
    │   ├── pusher.cpp
    │   └── puller.cpp
    ├── 04_router_dealer/       # 路由-经销模式
    │   ├── router.cpp
    │   └── dealer.cpp
    ├── 05_ipc/                 # IPC 传输方式
    │   ├── server.cpp
    │   └── client.cpp
    └── 06_inproc/              # INPROC 传输方式
        └── main.cpp
```

## 通信模式详解

### 1. 请求-应答模式 (REQ-REP)

**特点**：
- 同步通信
- 客户端发送请求后阻塞等待应答
- 服务器处理请求后返回应答

**使用场景**：
- RPC（远程过程调用）
- 同步服务请求
- 简单的客户端-服务器通信

**运行示例**：
```bash
# 终端1：启动服务器
./build/req_rep_server

# 终端2：运行客户端
./build/req_rep_client
```

**输出示例**：
```
[Server] 等待客户端请求...
[Server] 收到请求: 请求数据1
[Server] 发送应答: 已处理: 请求数据1

[Client] 连接到服务器
[Client] 发送: 请求数据1
[Client] 收到: 已处理: 请求数据1
```

### 2. 发布-订阅模式 (PUB-SUB)

**特点**：
- 异步通信
- 一个发布者，多个订阅者
- 订阅者只接收订阅后发送的消息
- 发布者不知道有多少订阅者

**使用场景**：
- 消息广播
- 事件通知系统
- 数据分发
- 实时数据推送

**运行示例**：
```bash
# 终端1：启动发布者
./build/pub_sub_pub

# 终端2：订阅 weather 主题
./build/pub_sub_sub weather

# 终端3：订阅 stock 主题
./build/pub_sub_sub stock

# 终端4：订阅 news 主题
./build/pub_sub_sub news
```

**输出示例**：
```
[Publisher] 已启动，开始发布消息
[Publisher] 发送: weather 晴天，温度25度

[Subscriber] 已连接，订阅主题: weather
[Subscriber] 收到: weather 晴天，温度25度
```

### 3. 推送-拉取模式 (PUSH-PULL)

**特点**：
- 异步任务分发
- 一个推送端，多个拉取端
- 任务自动分配给可用的拉取端
- 支持工作队列和负载均衡

**使用场景**：
- 任务队列系统
- 工作池（Worker Pool）
- 负载均衡
- 批量任务处理

**运行示例**：
```bash
# 终端1：启动任务发送者
./build/push_pull_push

# 终端2-4：启动多个工作者
./build/push_pull_pull worker1
./build/push_pull_pull worker2
./build/push_pull_pull worker3
```

**输出示例**：
```
[Pusher] 已启动，开始发送任务
[Pusher] 发送任务: Task-1

[Worker-worker1] 已连接，等待任务...
[Worker-worker1] 收到: Task-1 (耗时234ms)
```

### 4. 路由-经销模式 (ROUTER-DEALER)

**特点**：
- 异步多客户端通信
- 服务器自动管理客户端标识
- 支持复杂的异步请求-应答
- 支持路由和负载均衡

**使用场景**：
- 复杂的多客户端服务
- 异步 RPC 系统
- 消息队列服务
- 分布式协调系统

**运行示例**：
```bash
# 终端1：启动路由器服务器
./build/router_dealer_router

# 终端2-3：启动多个客户端
./build/router_dealer_dealer client1
./build/router_dealer_dealer client2
```

**输出示例**：
```
[Router] 已启动，等待客户端连接...
[Router] 收到来自 Client-client1 的请求: Request-1-from-client1 (第1个)
[Router] 发送响应给 Client-client1: 已处理: Request-1-from-client1

[Client-client1] 已连接到服务器
[Client-client1] 发送: Request-1-from-client1
[Client-client1] 收到: 已处理: Request-1-from-client1
```

## 传输方式详解

### 1. TCP 传输 (tcp://)

**特点**：
- 网络通信
- 支持跨机器通信
- 可靠的面向连接
- 自动重连

**使用**：
```cpp
socket.bind("tcp://*:5555");
socket.connect("tcp://hostname:5555");
```

**适用场景**：
- 分布式系统
- 远程服务通信
- 跨网络的消息传递

### 2. IPC 传输 (ipc://)

**特点**：
- 本地进程间通信
- 比 TCP 快（无网络开销）
- 只能用于同一台机器
- 使用 Unix 域套接字

**使用**：
```cpp
socket.bind("ipc:///tmp/zmq.sock");
socket.connect("ipc:///tmp/zmq.sock");
```

**性能对比**：
- 比 TCP 快 10-100 倍
- 适合本地服务间通信

**运行示例**：
```bash
# 终端1：启动 IPC 服务器
./build/ipc_server

# 终端2：运行 IPC 客户端
./build/ipc_client
```

### 3. INPROC 传输 (inproc://)

**特点**：
- 同进程线程间通信
- **最快的传输方式**（零拷贝）
- 必须使用同一个 context
- 完全在内存中

**使用**：
```cpp
// 注意：两个套接字必须使用同一个 context
auto ctx = std::make_shared<zmq::context_t>(1);
socket1.bind("inproc://channel");
socket2.connect("inproc://channel");
```

**性能对比**：
- 比 IPC 快 10 倍
- 比 TCP 快 100+ 倍
- 完全零开销通信

**运行示例**：
```bash
# 启动线程间通信演示
./build/inproc_demo
```

### 传输方式性能对比

| 传输方式 | 速度 | 跨机器 | 跨进程 | 跨线程 |
|---------|------|--------|--------|--------|
| INPROC  | 极快 | ✗      | ✗      | ✓      |
| IPC     | 快   | ✗      | ✓      | ✗      |
| TCP     | 中   | ✓      | ✓      | ✓      |

## C++ API 示例

### 创建上下文和套接字

```cpp
#include <zmq.hpp>

// 创建上下文（1个 I/O 线程）
zmq::context_t ctx(1);

// 创建 REP 套接字（回复消息）
zmq::socket_t socket(ctx, zmq::socket_type::rep);

// 绑定到地址
socket.bind("tcp://*:5555");

// 或连接到地址
socket.connect("tcp://localhost:5555");
```

### 发送和接收消息

```cpp
// 发送消息
std::string msg = "Hello ZeroMQ";
socket.send(zmq::buffer(msg), zmq::send_flags::none);

// 接收消息
zmq::message_t received;
auto result = socket.recv(received, zmq::recv_flags::none);

if (result) {
    std::string data(static_cast<char*>(received.data()), received.size());
    std::cout << data << std::endl;
}
```

### 多部分消息

```cpp
// 发送多部分消息
socket.send(zmq::buffer("part1"), zmq::send_flags::sndmore);
socket.send(zmq::buffer("part2"), zmq::send_flags::none);

// 接收多部分消息
zmq::message_t part1, part2;
socket.recv(part1, zmq::recv_flags::none);
socket.recv(part2, zmq::recv_flags::none);
```

### 设置套接字选项

```cpp
// 设置订阅主题
subscriber.set(zmq::sockopt::subscribe, "weather");

// 设置身份标识
socket.set(zmq::sockopt::routing_id, "my_id");

// 设置超时
socket.set(zmq::sockopt::rcvtimeo, 5000);
```

## 常见问题

### Q: 为什么发布者的消息我收不到？

**A**: 这是 ZeroMQ PUB-SUB 的一个特性：
- 订阅者只接收订阅**后**发送的消息
- 在建立连接前发送的消息会丢失
- 解决方案：
  1. 先启动订阅者，再启动发布者
  2. 使用其他模式（如 PUSH-PULL）
  3. 添加同步机制

### Q: INPROC 为什么这么快？

**A**: 因为：
- 消息直接在内存中传递
- 无需序列化/反序列化
- 无需网络协议处理
- 零拷贝的共享内存通信

### Q: 如何处理消息丢失？

**A**: ZeroMQ 的对策：
- 内置消息队列（缓冲区）
- 可配置的高水位标记
- 对于关键消息，使用请求-应答模式
- 实现应用层的重试机制

### Q: 能否跨网络使用？

**A**: 可以，但需要注意：
- 只有 TCP 和 PGM 支持网络通信
- IPC 和 INPROC 仅限本地
- 考虑网络延迟和带宽
- 可能需要处理网络断开重连

## 编译选项

### 调试编译
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make
```

### 优化编译
```bash
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### 自定义 C++ 标准
```bash
cmake -DCMAKE_CXX_STANDARD=20 ..
```

## 性能测试

可以使用以下方式进行性能测试：

```bash
# 终端1：任务发送者（发送 10000 个消息）
time ./build/push_pull_push

# 终端2-3：工作者（处理消息）
./build/push_pull_pull worker1 &
./build/push_pull_pull worker2 &
```

观察消息处理速度和系统资源使用情况。

## 进阶主题

### 1. 多线程使用

```cpp
// 创建共享的 context
auto ctx = std::make_shared<zmq::context_t>(1);

// 在不同线程中使用 INPROC
std::thread t1([ctx]() {
    zmq::socket_t s(*ctx, zmq::socket_type::rep);
    s.bind("inproc://channel");
    // ...
});

std::thread t2([ctx]() {
    zmq::socket_t s(*ctx, zmq::socket_type::req);
    s.connect("inproc://channel");
    // ...
});
```

### 2. 设置消息高水位

```cpp
// 设置高水位标记（单位：消息数）
socket.set(zmq::sockopt::sndhwm, 1000);
socket.set(zmq::sockopt::rcvhwm, 1000);
```

### 3. 异常处理

```cpp
try {
    zmq::message_t msg;
    socket.recv(msg, zmq::recv_flags::none);
} catch (const zmq::error_t &e) {
    std::cerr << "ZMQ Error: " << e.what() << std::endl;
}
```

## 参考资源

- [ZeroMQ 官方文档](https://zeromq.org/documentation/)
- [ZeroMQ 指南](https://zguide.zeromq.org/)
- [cppzmq GitHub](https://github.com/zeromq/cppzmq)
- [ZeroMQ Socket Types](https://zeromq.org/socket-api/)

## 许可证

本项目示例代码可自由使用和修改。

## 贡献

如有改进建议，欢迎提交。
