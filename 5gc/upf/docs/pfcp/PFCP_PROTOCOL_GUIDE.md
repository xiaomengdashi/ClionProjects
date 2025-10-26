# SMF PFCP 客户端与 UPF PFCP 接收器的交互通信

## 一、系统架构

### 1.1 完整的 PFCP 交互链路

```
┌──────────────┐  PFCP/UDP 8805  ┌──────────────┐
│     SMF      │◄───────────────>│     UPF      │
│ (Client)     │   协议交互      │  (Server)    │
└──────────────┘                 └──────────────┘
     ↑                                  ↓
  发送请求                          处理请求
  接收响应                          建立会话
     ↑                                  ↓
  smf_pfcp_client.c          pfcp_receiver_example.c
```

### 1.2 消息流图

```
时间线：

SMF                              UPF
 │                               │
 │─ Session Establishment Req ──>│
 │   (msg type 50)               │
 │   SEID=0x1000000000000001     │
 │   UE IP=10.0.0.2              │
 │   gNodeB IP=192.168.1.100     │
 │   TEID_DL=0x12345678          │
 │   TEID_UL=0x87654321          │
 │                               │
 │                    [处理请求] │
 │                    [建立会话] │
 │                               │
 │<─ Session Establishment Rsp ──│
 │   (msg type 51)               │
 │   确认消息                     │
 │                               │
 │─ Session Modification Req ──> │
 │   (msg type 52, 可选)         │
 │   修改 QoS 参数               │
 │                               │
 │<─ Session Modification Rsp ──│
 │   (msg type 53)               │
 │   确认修改                     │
 │                               │
 │─ Session Deletion Req ────────>│
 │   (msg type 54, 最后)         │
 │   删除会话                     │
 │                               │
 │<─ Session Deletion Rsp ──────│
 │   (msg type 55)               │
 │   确认删除                     │
 │                               │
```

---

## 二、PFCP 协议详解

### 2.1 PFCP 报头结构

```
PFCP 报头（8 字节固定）:

 0                   1                   2                   3
 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   Flags   |    Message Type       |       Message Length       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                   SEID (可选，8 字节)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Sequence Number (3 字节) |         Reserved (1 字节)           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

字段说明：
  Flags:
    - Bit 7 (S): SEID 是否存在 (0=无, 1=有)
    - Bits 6-5 (Version): PFCP 版本 (01=版本1)
    - Bits 4-0: 保留

  Message Type: 消息类型 (1-255)

  Message Length: 消息长度 (不含前 4 字节头)

  SEID: Session Endpoint ID (64-bit)
    - 仅在 S=1 时存在
    - 用于识别特定的会话

  Sequence Number: 3 字节序列号
    - 用于匹配请求/响应
    - 每个新消息递增

  Reserved: 1 字节保留位
```

### 2.2 消息类型定义

```
消息类型 (Message Type):

值      名称                          方向      功能
─────────────────────────────────────────────────────────────
50     Session Establishment Request   SMF→UPF   创建会话
51     Session Establishment Response  UPF→SMF   确认创建
52     Session Modification Request    SMF→UPF   修改会话
53     Session Modification Response   UPF→SMF   确认修改
54     Session Deletion Request        SMF→UPF   删除会话
55     Session Deletion Response       UPF→SMF   确认删除
56     (保留)
...
60     Heartbeat Request               任意→任意 保活
61     Heartbeat Response              任意→任意 保活响应
```

---

## 三、PFCP Information Elements (IEs)

### 3.1 常用 IE 类型

```
IE 类型 (Information Elements):

类型  长度  名称                          用途
─────────────────────────────────────────────────────────────
56    可变  Create PDR                    创建数据检测规则
70    可变  Create FAR                    创建转发规则
72    可变  Create QER                    创建 QoS 规则
60    可变  Node ID                       节点标识 (IP/FQDN)
86    可变  F-SEID (F-TEID)              远端会话端点
101   可变  PDI                           数据包检测信息
102   可变  FD (Flow Description)         流描述
103   可变  Priority                      优先级
```

### 3.2 Create FAR IE 详解

```
Create FAR (Forwarding Action Rule) 结构:

+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Type = 70 (2 bytes)                      |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Length (2 bytes)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| FAR ID (4 bytes)                         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| FAR Action (1 byte)                      |
| - 0x01 = DROP (丢弃)                     |
| - 0x02 = FORWARD (转发)                  |
| - 0x04 = BUFFER (缓冲)                   |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Destination (4 bytes)                    |
| - 0x01 = RAN (向基站转发)               |
| - 0x02 = CP (向控制面转发)              |
| - 0x03 = DN (向数据网转发)              |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Outer IP Address (4 bytes)               |
| 外层 IP 地址 (gNodeB 地址等)            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
| Outer UDP Port (2 bytes)                 |
| 外层 UDP 端口 (通常 2152)               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
```

---

## 四、完整的协议交互示例

### 4.1 会话建立请求（SMF → UPF）

```
原始十六进制数据：

20 32 00 XX XX XX XX XX    - 报头
                             - 20: Flags (No SEID, Version=1)
                             - 32: Message Type = 50 (Session Est Req)
                             - 00 XX: Message Length

[Node ID IE]
00 3C 00 05 00 C0 A8 01 1E  - Node ID IE (SMF IP = 192.168.1.30)

[Create FAR IE]
00 46 00 18                 - FAR IE Header (Type=70, Length=24)
00 00 00 01                 - FAR ID = 1
02                          - FAR Action = FORWARD
00 00 00 01                 - Destination = RAN
C0 A8 01 64                 - Outer IP = 192.168.1.100 (gNodeB)
08 68                       - Outer UDP Port = 2152

对应的结构体：
  flags = 0x20 (Version=1, No SEID)
  msg_type = 50
  msg_length = (动态计算)
  seid = 0 (无 SEID)
  seq_number = (自动递增)
```

### 4.2 会话建立响应（UPF → SMF）

```
原始十六进制数据：

20 33 00 10 XX XX XX XX    - 报头
                             - 20: Flags
                             - 33: Message Type = 51 (Session Est Rsp)
                             - 00 10: Message Length = 16

[Cause IE]
00 12 00 01 01             - Cause IE (Type=18)
                             - Cause Value = 1 (Success)

[其他 IE...]

对应的结构体：
  flags = 0x20
  msg_type = 51
  msg_length = 16
  seq_number = (必须与请求相同)
```

---

## 五、序列号匹配机制

### 5.1 序列号的作用

```
SMF                              UPF
 │                               │
 │─ Request, Seq=1 ─────────────>│
 │                               │
 │                    [处理]     │
 │                    [响应]     │
 │                               │
 │<─ Response, Seq=1 ────────────│
 │   (序列号必须相同！)          │
 │                               │
 │─ Request, Seq=2 ─────────────>│
 │                               │
 │                    [处理]     │
 │                    [响应]     │
 │                               │
 │<─ Response, Seq=2 ────────────│
 │   (不同的请求，序列号不同)    │
```

### 5.2 序列号字段详解

```
序列号占用 3 字节，范围：0 - 16777215 (0xFFFFFF)

构造方法（在代码中）：
  seq_number = htonl(get_next_seq_number() << 8)
                ↑                              ↑
              网络字节序                   左移 8 位
                                    (因为低 8 位是保留位)

解析方法：
  seq = (ntohl(received_seq) >> 8) & 0xFFFFFF
```

---

## 六、运行方法

### 6.1 同时运行两个程序进行交互通信

#### 方式 1：基础测试（仅 PFCP 协议）

**终端 1：启动 UPF PFCP 接收器**
```bash
./bin/pfcp_receiver_example
```

输出：
```
╔════════════════════════════════════════╗
║       PFCP Receiver Demonstrator       ║
║    (Simulates UPF receiving SMF cmd)   ║
╚════════════════════════════════════════╝

[PFCP] Server listening on UDP port 8805
[INFO] Waiting for PFCP messages from SMF...
```

**终端 2：启动 SMF PFCP 客户端**
```bash
./bin/smf_pfcp_client
```

输出：
```
╔════════════════════════════════════════════════════════════╗
║              SMF PFCP Client (Simulator)                   ║
║         Sends PFCP messages to UPF for session mgmt        ║
╚════════════════════════════════════════════════════════════╝

[SMF] Connecting to UPF at 127.0.0.1:8805
[SMF] Creating example sessions...
  Session 1: SUPI=234010012340000, UE IP=10.0.0.2
  Session 2: SUPI=234010012340001, UE IP=10.0.0.3
  Session 3: SUPI=234010012340002, UE IP=10.0.0.4

═══════════════════════════════════════════════════════════
Session 1/3: SUPI=234010012340000
═══════════════════════════════════════════════════════════

[PFCP] Sending Session Establishment Request to UPF
  Target: 127.0.0.1:8805
[Request Hex] 20 32 00 32 ...
[SUCCESS] Request sent

[PFCP] Waiting for UPF response...
[PFCP] Received XX bytes from 127.0.0.1:8805
[Response Hex] 20 33 00 0c ...
[PFCP Response Received]
  Message Type: 51
  ✅ Session established successfully
```

#### 方式 2：集成测试（完整 UPF）

**终端 1：启动完整 UPF**
```bash
sudo ./bin/upf_rss_multi_queue -l 0-5 -n 4
```

**终端 2：启动 SMF 客户端**
```bash
./bin/smf_pfcp_client
```

两个程序会通过 PFCP 协议交互，建立 3 个 UE 会话。

---

## 七、协议交互完整示例

### 7.1 会话创建流程

```
执行步骤：

1️⃣ 初始化
   ├─ SMF 创建 3 个示例 UE 会话配置
   ├─ 每个会话包含：SUPI、UE IP、gNodeB IP、TEID 等
   └─ 生成唯一的 SEID 和序列号

2️⃣ 会话 1 建立
   ├─ SMF 构造 PFCP Session Establishment Request
   │  ├─ Message Type = 50
   │  ├─ SEID = 0x1000000000000001
   │  ├─ Sequence = 1
   │  └─ IEs: Node ID, Create FAR
   │
   ├─ SMF 通过 UDP 8805 发送到 UPF (127.0.0.1)
   │
   ├─ UPF 接收消息并解析
   │  ├─ 验证消息类型 (50 ✓)
   │  ├─ 提取会话信息
   │  ├─ 建立会话表项
   │  └─ 保存 UE IP、gNodeB IP、TEID
   │
   ├─ UPF 构造 PFCP Session Establishment Response
   │  ├─ Message Type = 51
   │  ├─ Sequence = 1 (与请求相同!)
   │  └─ Cause = 1 (Success)
   │
   ├─ UPF 发送响应回 SMF
   │
   ├─ SMF 接收响应并验证
   │  ├─ 检查序列号 (1 ✓)
   │  ├─ 检查消息类型 (51 ✓)
   │  └─ 确认会话创建成功
   │
   └─ 等待 1 秒

3️⃣ 会话 2 建立 (同上，Sequence = 2)

4️⃣ 会话 3 建立 (同上，Sequence = 3)

5️⃣ 完成
   └─ 所有会话已建立，程序结束
```

### 7.2 数据包流向

建立会话后，实际数据流向：

```
下行方向（DN → UE）：

数据包到达 UPF DN 侧网卡
    ↓
UPF 根据目的 IP (UE IP) 查找会话
    ↓ 使用 gNodeB IP 和 TEID 转发
GTP-U 隧道
    ↓
传输到 gNodeB
    ↓
gNodeB 转发给 UE


上行方向（UE → DN）：

GTP-U 数据包到达 UPF RAN 侧网卡
    ↓
UPF 解析 TEID，查找会话
    ↓
提取内层 IP 包
    ↓
根据目的 IP (DN IP) 转发
    ↓
传输到数据网络 (DN)
```

---

## 八、关键代码片段

### 8.1 SMF 发送请求

```c
// 构造 PFCP 报头
pfcp_build_header(buffer, PFCP_SESSION_EST_REQ, msg_length, seid, 0);

// 构造 FAR IE
struct pfcp_create_far_ie *far_ie = (struct pfcp_create_far_ie *)ptr;
far_ie->type = htons(70);
far_ie->far_id = htonl(1);
far_ie->far_action = 2;  // FORWARD
far_ie->outer_ip_addr = gnb_ip;
far_ie->outer_udp_port = htons(2152);

// 发送
sendto(sock, buffer, msg_len, 0, (struct sockaddr *)&upf_addr, sizeof(upf_addr));
```

### 8.2 UPF 接收并响应

```c
// 接收消息
recvfrom(sock, buffer, sizeof(buffer), 0, NULL, NULL);

// 验证消息类型
if (hdr->msg_type != PFCP_SESSION_EST_REQ) {
    // 错误处理
}

// 解析会话信息
ue_session_t new_session;
parse_pfcp_message(buffer, recv_len, &new_session);

// 建立会话表项
ue_sessions[session_count++] = new_session;

// 发送响应
create_pfcp_response(response_buffer, sizeof(response_buffer), &new_session);
sendto(sock, response_buffer, response_len, 0, ...);
```

---

## 九、故障排除

### 9.1 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| UPF 无法收到消息 | SMF 未启动或地址错误 | 检查 127.0.0.1:8805 |
| 序列号不匹配 | 响应时序列号改变 | 响应必须保持原序列号 |
| 消息解析失败 | IE 格式不正确 | 检查十六进制输出 |
| 超时 | UPF 未监听 | 确认 UPF 正在运行 |

---

## 十、总结

```
┌─────────────────────────────────────────────────────┐
│         PFCP 协议交互的关键要点                     │
├─────────────────────────────────────────────────────┤
│                                                     │
│  1. 端口：UDP 8805（PFCP 标准端口）               │
│                                                     │
│  2. 报头：固定 8 字节                              │
│     ├─ Flags (1 byte)                              │
│     ├─ Message Type (1 byte)                       │
│     ├─ Message Length (2 bytes)                    │
│     ├─ SEID (8 bytes, 可选)                        │
│     └─ Sequence (3 bytes)                          │
│                                                     │
│  3. 消息匹配：通过序列号                           │
│     └─ 响应的序列号必须与请求相同                 │
│                                                     │
│  4. 会话建立：
│     ├─ 请求：PFCP_SESSION_EST_REQ (50)            │
│     └─ 响应：PFCP_SESSION_EST_RSP (51)            │
│                                                     │
│  5. 数据流：
│     ├─ 下行：DN → gNodeB → UE                      │
│     └─ 上行：UE → gNodeB → DN                      │
│                                                     │
└─────────────────────────────────────────────────────┘
```

