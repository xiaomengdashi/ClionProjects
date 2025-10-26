# 5G AMF系统

这是一个5G核心网AMF（Access and Mobility Management Function）系统的实现，包含AMF状态机、SBI（Service Based Interface）消息处理、N1/N2接口管理、NF管理和HTTP服务器通信。

## 项目结构

```
5gc/
├── CMakeLists.txt              # 构建配置文件
├── README.md                   # 项目说明文档
├── amf_http_server.cpp         # HTTP AMF服务器（核心程序）
├── include/                    # 头文件目录
│   ├── AmfConfiguration.h     # AMF配置管理头文件
│   ├── AmfSm.h                # AMF状态机头文件
│   ├── HttpServer.h           # HTTP服务器头文件
│   ├── N1N2Interface.h        # N1/N2接口管理头文件
│   ├── NfManagement.h         # 网络功能管理头文件
│   ├── SbiMessage.h           # SBI消息头文件
│   ├── SbiMessageFactory.h    # SBI消息工厂头文件
│   └── UeContext.h            # UE上下文管理头文件
└── src/                       # 源文件目录
    ├── AmfConfiguration.cpp   # AMF配置管理实现
    ├── AmfSm.cpp              # AMF状态机实现
    ├── HttpServer.cpp         # HTTP服务器实现
    ├── N1N2Interface.cpp      # N1/N2接口管理实现
    ├── NfManagement.cpp       # 网络功能管理实现
    ├── SbiMessage.cpp         # SBI消息实现
    ├── SbiMessageFactory.cpp  # SBI消息工厂实现
    └── UeContext.cpp          # UE上下文管理实现
```

## 核心组件

### 1. AMF状态机 (AmfSm)
- 实现5G网络中UE（用户设备）的状态管理
- 支持三种主要状态：DEREGISTERED、REGISTERED_IDLE、REGISTERED_CONNECTED
- 处理各种5G网络事件和状态转换
- 集成NF发现和选择功能

### 2. N1/N2接口管理 (N1N2Interface)
- **N1接口**：AMF与UE之间的NAS（Non-Access Stratum）消息处理
- **N2接口**：AMF与gNB之间的NGAP（NG Application Protocol）消息处理
- 支持注册管理、会话管理、移动性管理等功能
- N2接口监听端口：38412（标准NGAP端口）

### 3. 网络功能管理 (NfManagement)
- **NF注册与发现**：支持向NRF注册和发现其他网络功能
- **负载均衡**：基于负载和容量的NF实例选择
- **健康检查**：定期检查NF实例的健康状态
- **心跳服务**：维护与NRF的心跳连接

### 4. SBI消息系统 (SbiMessage)
- 实现5G SBI接口的消息格式和处理
- 支持多种SBI服务类型（Namf_Communication、Nsmf_PDUSession等）
- 提供消息工厂模式创建各种SBI消息

### 5. HTTP AMF服务器 (amf_http_server)
- 真实的HTTP服务器，监听8080端口
- 接收和处理5G SBI HTTP请求
- 集成AMF状态机，根据SBI消息更新UE状态
- 集成N1/N2接口管理器
- 返回JSON格式的HTTP响应

### 6. UE上下文管理 (UeContext)
- 管理UE的注册信息、安全上下文、会话信息
- 支持多种接入类型和网络切片
- 维护UE的位置信息和移动性状态

### 7. AMF配置管理 (AmfConfiguration)
- 管理AMF的配置参数
- 支持PLMN配置、网络切片配置
- 支持加密算法和安全策略配置

## 编译和运行

### 编译项目
```bash
mkdir build && cd build
cmake ..
make
```

### 运行程序

1. **启动AMF HTTP服务器**：
```bash
./amf_http_server
```
服务器将启动以下服务：
- HTTP SBI服务器：监听8080端口
- N1/N2接口管理器：监听38412端口（NGAP）
- 自动向NRF注册AMF实例

启动日志示例：
```
Starting 5G AMF HTTP Server...
AMF Instance ID: amf-001
AMF Name: AMF-Beijing-001
PLMN ID: 46001
AMF state machine created. Initial state: DEREGISTERED
N1N2 Interface Manager started successfully on 0.0.0.0:38412
Registering AMF instance amf-001 to NRF: http://nrf.5gc.mnc001.mcc460.3gppnetwork.org:8080
```

## 支持的5G SBI消息类型

1. **UE Context Create Request** - UE上下文创建请求
2. **UE Authentication Request** - UE认证请求
3. **PDU Session Create Request** - PDU会话创建请求
4. **AM Policy Control Create Request** - AM策略控制创建请求
5. **NF Register Request** - 网络功能注册请求
6. **NF Discover Request** - 网络功能发现请求
7. **PDU Session Release Request** - PDU会话释放请求
8. **UE Context Release Request** - UE上下文释放请求

## 支持的API端点

- `POST /namf-comm/v1/ue-contexts` - UE上下文创建
- `POST /namf-comm/v1/ue-authentications` - UE认证
- `POST /nsmf-pdusession/v1/pdu-sessions` - PDU会话创建
- `POST /npcf-am-policy-control/v1/policies` - AM策略控制
- `POST /nnrf-nfm/v1/nf-instances` - NF注册

## 使用示例

### 使用curl测试
```bash
# 发送UE上下文创建请求
curl -X POST http://localhost:8080/namf-comm/v1/ue-contexts \
  -H "Content-Type: application/json" \
  -d '{
    "supi": "imsi-460001234567890",
    "pei": "imeisv-1234567890123456",
    "gpsi": "msisdn-8613800138000",
    "accessType": "3GPP_ACCESS",
    "ratType": "NR"
  }'
```

### 通信流程

1. **外部系统发送HTTP请求**：包含5G SBI头部和JSON消息体
2. **AMF服务器接收并解析**：将HTTP请求转换为SBI消息对象
3. **AMF状态机处理**：根据消息类型更新UE状态
4. **生成HTTP响应**：返回包含处理结果的JSON响应

## 技术特性

- **真实HTTP通信**：基于标准HTTP协议的服务器架构
- **5G标准兼容**：符合3GPP 5G SBI接口规范和NGAP协议
- **完整的AMF功能**：
  - AMF状态机实现
  - N1/N2接口管理
  - NF注册与发现
  - UE上下文管理
  - 配置管理
- **网络接口支持**：
  - SBI接口（HTTP/JSON，端口8080）
  - N2接口（NGAP，端口38412）
  - N1接口（NAS消息处理）
- **服务发现与管理**：
  - 向NRF自动注册
  - NF实例健康检查
  - 负载均衡和故障转移
- **消息处理**：
  - SBI消息工厂模式
  - N1/N2消息类型支持
  - JSON格式的请求和响应
- **多线程安全**：支持并发请求处理和异步消息处理

这个系统实现了完整的5G AMF功能，包括核心网络功能管理、接口处理和状态管理，可以用于5G网络的开发、测试和学习。