# 客户端-服务端架构设计

## 概述
将现有的单一PCAP回放程序重构为客户端-服务端模式，通过共享内存进行状态同步和包发送协调。

## 核心需求
1. 客户端发送PCAP中的发出包
2. 服务端发送PCAP中的接收包
3. 通过共享内存检测两端状态
4. 实现包发送同步逻辑：
   - 如果服务端收到包，按PCAP时间间隔发出包
   - 如果服务端没收到包，检测客户端是否发出包
   - 如果客户端已发出，在PCAP间隔超时后发出服务端包
   - 后续收到客户端包则忽略

## 包方向性分析

### 问题
- PCAP文件本身不包含明确的方向信息
- 需要通过其他方式区分发送和接收包

### 解决方案
1. **基于MAC地址**：指定本地MAC地址，以此区分发送/接收
2. **基于IP地址**：指定本地IP地址范围
3. **基于端口**：指定客户端/服务端端口范围
4. **基于包序号**：奇数包为客户端，偶数包为服务端（简单但不准确）

### 推荐方案
使用IP地址方案：
- 用户指定客户端IP地址
- 源IP匹配客户端IP的包 → 客户端发送
- 目标IP匹配客户端IP的包 → 服务端发送

## 共享内存设计

### 数据结构
```cpp
struct SharedMemoryData {
    // 控制信息
    std::atomic<bool> client_ready;     // 客户端就绪
    std::atomic<bool> server_ready;     // 服务端就绪
    std::atomic<bool> replay_started;   // 回放开始
    std::atomic<bool> replay_finished;  // 回放结束
    
    // 包发送状态
    std::atomic<int> current_packet_index;  // 当前包索引
    std::atomic<bool> client_packet_sent;   // 客户端包已发送
    std::atomic<bool> server_packet_received; // 服务端收到包
    std::atomic<uint64_t> last_send_time;   // 最后发送时间(微秒)
    
    // 统计信息
    std::atomic<int> client_sent_count;     // 客户端发送计数
    std::atomic<int> server_sent_count;     // 服务端发送计数
    std::atomic<int> client_failed_count;   // 客户端失败计数
    std::atomic<int> server_failed_count;   // 服务端失败计数
    
    // 同步锁
    std::atomic<bool> sync_lock;            // 同步锁
};
```

### 共享内存管理
- 使用POSIX共享内存 (shm_open/mmap)
- 命名：`/pcap_replay_shm`
- 大小：4KB足够
- 权限：0666

## 程序架构

### 文件结构
```
src/
├── common/
│   ├── shared_memory.h     # 共享内存定义
│   ├── shared_memory.cpp   # 共享内存操作
│   ├── packet_analyzer.h   # 包分析器
│   └── packet_analyzer.cpp # 包分析实现
├── client/
│   ├── pcap_client.h       # 客户端头文件
│   └── pcap_client.cpp     # 客户端实现
├── server/
│   ├── pcap_server.h       # 服务端头文件
│   └── pcap_server.cpp     # 服务端实现
└── send_pcap.cpp           # 原程序(保留)
```

### 客户端程序 (pcap_client)
- 解析PCAP文件，筛选出客户端包
- 初始化共享内存
- 等待服务端就绪
- 按照同步逻辑发送包
- 更新共享内存状态

### 服务端程序 (pcap_server)
- 解析PCAP文件，筛选出服务端包
- 连接共享内存
- 监听网络接口，检测收到的包
- 根据同步逻辑决定发送时机
- 更新共享内存状态

## 同步逻辑详细设计

### 状态机
```
初始状态 → 等待对方就绪 → 开始回放 → 包发送循环 → 结束
```

### 包发送循环逻辑
1. **客户端逻辑**：
   - 发送当前客户端包
   - 设置 `client_packet_sent = true`
   - 等待服务端响应或超时
   - 继续下一个包

2. **服务端逻辑**：
   - 监听网络接口
   - 如果收到包：设置 `server_packet_received = true`，按PCAP间隔发送
   - 如果未收到包：检查 `client_packet_sent`
     - 如果客户端已发送：等待PCAP间隔超时后发送
     - 设置忽略标志，后续收到包则忽略

### 超时处理
- 使用PCAP文件中的原始时间间隔作为超时时间
- 最小超时：10ms
- 最大超时：10秒

## 命令行接口

### 客户端
```bash
./pcap_client <pcap文件> <接口名> <客户端IP> [模式] [参数]
```

### 服务端
```bash
./pcap_server <pcap文件> <接口名> <客户端IP> [模式] [参数]
```

### 参数说明
- `客户端IP`：用于区分包方向的IP地址
- `模式`：回放模式(1-4)，与原程序兼容
- `参数`：模式相关参数

## 实现计划
1. 实现共享内存管理模块
2. 实现包分析器（区分包方向）
3. 实现客户端程序
4. 实现服务端程序
5. 更新构建系统
6. 测试和调试