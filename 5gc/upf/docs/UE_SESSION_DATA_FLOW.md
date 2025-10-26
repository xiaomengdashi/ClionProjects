# 5G 核心网中 UE 会话信息来源分析

## 一、UE 会话建立流程（3GPP 协议）

### 1.1 整体流程图

```
┌──────┐         ┌─────-─┐         ┌─────┐        ┌-────┐
│ UE   │         │ gNodeB│         │ AMF │        │ UPF │
└──┬───┘         └─-─┬───┘         └──┬──┘        └─-┬──┘
   │                 │                │              │
   │  NR RRC Setup   │                │              │
   ├────────────────>│                │              │
   │                 │  NAS           │              │
   │                 │  Registration  │              │
   │                 │<───────────────┤              │
   │                 │                │              │
   │                 │ Registration   │              │
   │                 │ Accept         │              │
   │                 ├──────────────> │              │
   │                 │                │              │
   │                 │                │ PDU Session  │
   │                 │                │ Establishment│
   │                 │                ├─────────────>│
   │                 │                │              │
   │  GTP-U 隧道建立  │                │  SESSION OK  │
   │ ◄───────────────────────────────────────────-──>│
   │                 │                │              │
   │   数据传输       │                │              │
   │ ◄────────────────────────────────────────────────>
   │                 │                │              │

时间顺序：
1. UE ↔ gNodeB: 无线接入（NR 协议）
2. gNodeB ↔ AMF: 会话管理（NAS 协议）
3. AMF ↔ UPF: PDU 会话建立（Nsmf 协议）
4. gNodeB ↔ UPF: 数据传输（GTP-U）
```

---

## 二、UE 会话信息来源（按优先级）

### 2.1 🥇 第一来源：PFCP 协议（Session Establishment）

**PFCP = Packet Forwarding Control Protocol**

#### 来源通道
- **接收方：** UPF
- **发送方：** SMF (Session Management Function)
- **协议：** UDP 端口 8805
- **消息：** `PFCP Session Establishment Request (PSER)`

#### 包含的关键信息

```c
/* PFCP Session Establishment Request 结构 */
struct pfcp_session_establishment_request {
    /* 会话 ID */
    uint64_t seid;                    /* Session Endpoint ID */

    /* UE 身份信息 */
    char supi[20];                    /* Subscription Permanent Identifier */
    uint8_t pdu_session_id;           /* PDU Session ID (0-255) */

    /* UE IP 地址 */
    uint32_t ue_ipv4_addr;            /* UE IPv4 address */
    uint8_t ue_ipv6_addr[16];         /* UE IPv6 address (可选) */

    /* gNodeB 信息 */
    uint32_t gnb_ipv4_addr;           /* gNodeB IPv4 address */
    uint8_t gnb_ipv6_addr[16];        /* gNodeB IPv6 address */
    uint16_t gnb_udp_port;            /* gNodeB UDP port (通常 2152) */

    /* GTP-U 隧道信息 */
    uint32_t upf_teid_dl;             /* UPF 下行 TEID */
    uint32_t gnb_teid_ul;             /* gNodeB 上行 TEID */

    /* QoS 信息 */
    uint8_t qos_profile_id;           /* QoS Profile */
    uint8_t qos_priority;             /* Priority Level */
    uint32_t qos_mbr_ul;              /* MBR Uplink (bps) */
    uint32_t qos_mbr_dl;              /* MBR Downlink (bps) */
    uint32_t qos_gbr_ul;              /* GBR Uplink (bps) */
    uint32_t qos_gbr_dl;              /* GBR Downlink (bps) */

    /* FAR (Forwarding Action Rules) */
    uint32_t far_id;                  /* FAR ID */
    uint8_t far_action;               /* DROP, FORWARD, BUFFER */

    /* PDR (Packet Detection Rules) */
    uint32_t pdr_id;                  /* PDR ID */
    uint16_t pdr_precedence;          /* Precedence */
};
```

#### 示例 PFCP 消息（十六进制）

```
PFCP Header:
  Version: 1
  Message Type: 50 (Session Establishment Request)
  Message Length: 256 bytes
  SEID: 0x1234567890ABCDEF

PFCP Grouped Information Elements:
  - Node ID: SMF address (192.168.1.30)
  - Create PDR: Rules for UE traffic detection
  - Create FAR: Actions to forward traffic
  - Create QER: QoS enforcement
  - Create URR: Usage reporting

示例包含：
  SUPI: 234010012340000
  UE IPv4: 10.0.0.2 (UE 分配的内网 IP)
  gNodeB IPv4: 192.168.1.100
  gNodeB UDP Port: 2152
  UPF DL TEID: 0x12345678 (由 SMF 分配)
  gNodeB UL TEID: 0x87654321 (由 gNodeB 分配)
```

---

### 2.2 🥈 第二来源：User Plane Function (UPF) 本地配置

#### 直接配置
UPF 可以有预配置的用户信息（类似 HSS 备份）

```c
/* 本地 UPF 会话缓存（来自 PFCP）*/
struct upf_session_cache {
    uint64_t seid;                    /* Session Endpoint ID */
    char supi[20];                    /* 订阅者 ID */
    uint32_t ue_ip;                   /* UE IP */
    uint32_t teid_dl;                 /* 下行 TEID */
    uint32_t teid_ul;                 /* 上行 TEID */
    uint32_t gnb_ip;                  /* gNodeB IP */
    time_t session_created;           /* 会话创建时间 */
    time_t session_last_updated;      /* 最后更新时间 */
};
```

---

### 2.3 🥉 第三来源：GTP-U 隧道建立时学习

#### 被动学习
当第一个上行数据包到达时，UPF 可以学习会话信息

```c
/* 从上行 GTP-U 包解析会话信息 */
struct gtp_u_header {
    uint8_t flags;
    uint8_t msg_type;
    uint16_t length;
    uint32_t teid;              /* ← 关键信息！从包中提取 */
};

/* UPF 使用 TEID 查表找到对应的会话 */
/* 然后记录源 IP（gNodeB IP）和数据包入口时间戳 */
```

---

## 三、PFCP 协议详解

### 3.1 PFCP 消息流（实时示例）

```
┌─────────────┐              PFCP/UDP 8805            ┌─────────────┐
│    SMF      │──────────────────────────────────────>│     UPF     │
│             │   Session Establishment Request       │             │
│             │   (包含所有 UE 会话参数)                 │             │
└─────────────┘              PFCP/UDP 8805            └─────────────┘
      │                                                     │
      │  包含的参数：                                         │
      ├─ SUPI: 234010012340000                              │
      ├─ PDU Session ID: 1                                  │
      ├─ UE IP: 10.0.0.2                                    │
      ├─ gNodeB IP: 192.168.1.100                           │
      ├─ gNodeB Port: 2152                                  │
      ├─ TEID (DL): 0x12345678                              │
      ├─ TEID (UL): 0x87654321                              │
      └─ QoS 配置                                           │
                                                             │
                                                  处理完成后 │
                                                             │
┌─────────────┐              PFCP/UDP 8805            ┌─────────────┐
│    SMF      │<──────────────────────────────────────│     UPF     │
│             │   Session Establishment Response      │             │
│             │   (确认收到并激活会话)                   │             │
└─────────────┘              PFCP/UDP 8805            └─────────────┘
      │
      └─> 通知 AMF 会话建立成功
          AMF 通知 gNodeB
          gNodeB 通知 UE
          UE 开始发送数据
```

---

## 四、在改进的 UPF 代码中实现 PFCP

### 4.1 当前代码问题

```c
/* 当前：硬编码 UE 会话 */
void init_ue_sessions(void) {
    ue_sessions[0].ue_ip = inet_addr("10.0.0.2");
    ue_sessions[0].teid_downlink = 0x12345678;
    ue_sessions[0].teid_uplink = 0x87654321;
    ue_sessions[0].gnb_ip = inet_addr("192.168.1.100");
    // ... 硬编码
}
```

**问题：**
- ❌ 不能动态添加 UE 会话
- ❌ 无法与实际 SMF 通信
- ❌ 会话信息固定，无法更新
- ❌ 无法删除过期会话

### 4.2 改进方案：实现 PFCP 接收

```c
#include <stdint.h>
#include <netinet/in.h>

/* PFCP 报头 */
struct pfcp_header {
    uint8_t flags;
    uint8_t msg_type;
    uint16_t length;
    uint32_t seid_high;  /* SEID 高 32 位 */
    uint32_t seid_low;   /* SEID 低 32 位 */
    uint32_t seq_num;
} __attribute__((packed));

/* PFCP 会话建立请求 */
struct pfcp_session_establishment_request {
    struct pfcp_header hdr;

    /* 必需的 IEs (Information Elements) */
    uint64_t seid;              /* Session Endpoint ID */

    /* Node ID (SMF) */
    uint8_t node_id_type;       /* IPv4, IPv6, etc */
    uint32_t smf_ipv4;          /* SMF IPv4 地址 */

    /* PDU Session ID */
    uint8_t pdu_session_id;     /* 0-255 */

    /* UE Address Pool Information */
    uint32_t ue_ipv4_addr;      /* UE IPv4 地址 */

    /* FAR (Forwarding Action Rules) */
    uint32_t far_id;
    uint8_t far_action;         /* 1=DROP, 2=FORWARD, 4=BUFFER */
    uint32_t far_destination;   /* 1=RAN, 2=CP, 3=DN */

    /* PDR (Packet Detection Rule) */
    uint32_t pdr_id;
    uint16_t pdr_precedence;

    /* GTP-U Tunnel Info */
    struct {
        uint32_t teid;          /* TEID */
        uint8_t ipv4[4];        /* IP 地址 */
        uint16_t port;          /* UDP 端口 */
    } gtp_u_peer_info;

    /* QER (QoS Enforcement Rule) */
    uint8_t qos_priority;
    uint32_t qos_mbr_ul;
    uint32_t qos_mbr_dl;
};

/* 从 PFCP 消息中提取会话信息 */
int parse_pfcp_session_establishment(const uint8_t *pfcp_msg,
                                     int msg_len,
                                     ue_session_t *session) {
    struct pfcp_header *hdr = (struct pfcp_header *)pfcp_msg;

    /* 验证消息类型 */
    if (hdr->msg_type != 50) {  /* 50 = Session Establishment Request */
        printf("[PFCP] Invalid message type: %u\n", hdr->msg_type);
        return -1;
    }

    /* 解析 IEs (这是简化版本) */
    const uint8_t *ptr = pfcp_msg + sizeof(struct pfcp_header);

    while ((ptr - pfcp_msg) < msg_len) {
        uint8_t ie_type = *ptr++;
        uint16_t ie_len = (ptr[0] << 8) | ptr[1];
        ptr += 2;

        switch (ie_type) {
            case 1:  /* Node ID */
                /* 提取 SMF IP 地址 */
                break;
            case 8:  /* Create PDR */
                /* 提取 PDR 信息 */
                break;
            case 20: /* Create FAR */
                /* 提取 FAR 信息 */
                break;
            case 86: /* Node ID (UPF) */
                /* 提取 UE IP */
                memcpy(&session->ue_ip, ptr, 4);
                break;
            case 108: /* GTP-U Peer Information */
                /* 提取 gNodeB IP 和 TEID */
                session->gnb_ip = *(uint32_t *)ptr;
                session->teid_uplink = *(uint32_t *)(ptr + 4);
                break;
        }

        ptr += ie_len;
    }

    return 0;
}

/* PFCP 接收线程 */
int pfcp_receiver_thread(void *arg) {
    int pfcp_sock = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in addr;

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(8805);  /* PFCP UDP 端口 */

    bind(pfcp_sock, (struct sockaddr *)&addr, sizeof(addr));

    uint8_t buffer[4096];

    while (1) {
        int recv_len = recvfrom(pfcp_sock, buffer, sizeof(buffer), 0, NULL, NULL);

        if (recv_len > 0) {
            ue_session_t new_session;

            if (parse_pfcp_session_establishment(buffer, recv_len, &new_session) == 0) {
                /* 添加到会话表 */
                if (session_count < MAX_SESSIONS) {
                    ue_sessions[session_count++] = new_session;
                    printf("[PFCP] New session created: UE IP=0x%x, TEID=0x%x\n",
                           new_session.ue_ip, new_session.teid_uplink);
                }

                /* 发送 PFCP 响应 */
                send_pfcp_session_establishment_response(pfcp_sock, buffer);
            }
        }
    }

    close(pfcp_sock);
    return 0;
}
```

---

## 五、完整的 UE 会话生命周期

### 5.1 会话建立阶段

```
时间   事件                          UE IP    TEID_DL    TEID_UL    状态
─────────────────────────────────────────────────────────────────────────
 0    UE 开始注册                    未分配    -          -          IDLE

 1    AMF 请求 UPF 会话              -        -          -          PENDING
      ↓ PFCP Session Establishment

 2    UPF 收到 PFCP 消息             10.0.0.2 0x12345678 0x87654321  ACTIVE
      ↓ UPF 发送 PFCP 响应

 3    SMF 收到确认                   -        -          -          -
      ↓ 通知 AMF、gNodeB、UE

 4    gNodeB 建立隧道                -        -          -          -
      ↓ UE 开始发送数据

 5    UE 第一个上行包到达            10.0.0.2 0x12345678 0x87654321  DATA
      ↓ 学习源 gNodeB IP

 6    持续数据传输                   10.0.0.2 0x12345678 0x87654321  ACTIVE
```

### 5.2 会话修改阶段

```
PFCP Session Modification Request (可能的修改)：
- 修改 QoS 参数
- 添加额外的 PDR/FAR
- 更新 gNodeB 地址（切换）
- 修改数据包检测规则
```

### 5.3 会话删除阶段

```
PFCP Session Deletion Request：
- 释放 TEID
- 清理会话表项
- 停止数据转发
- 发送使用情况报告
```

---

## 六、真实场景中的 UE 会话表

### 6.1 使用哈希表优化查询（生产级代码）

```c
#include <rte_hash.h>

/* 使用 DPDK 的哈希表存储 UE 会话 */
struct rte_hash *session_hash_by_ip = NULL;
struct rte_hash *session_hash_by_teid = NULL;

void init_session_lookup_tables(void) {
    struct rte_hash_parameters hash_params = {0};

    /* 按 UE IP 查询 */
    hash_params.name = "session_by_ip";
    hash_params.entries = 100000;          /* 10万 UE */
    hash_params.key_len = sizeof(uint32_t); /* IP 地址 */
    hash_params.hash_func = rte_hash_crc;
    hash_params.hash_func_init_val = 0;

    session_hash_by_ip = rte_hash_create(&hash_params);

    /* 按 TEID 查询 */
    hash_params.name = "session_by_teid";
    hash_params.key_len = sizeof(uint32_t); /* TEID */

    session_hash_by_teid = rte_hash_create(&hash_params);
}

/* 快速查询 */
static inline ue_session_t* lookup_session_by_ip_fast(uint32_t ue_ip) {
    int pos = rte_hash_lookup(session_hash_by_ip, &ue_ip);
    if (pos >= 0) {
        return &ue_sessions[pos];
    }
    return NULL;
}

static inline ue_session_t* lookup_session_by_teid_fast(uint32_t teid) {
    int pos = rte_hash_lookup(session_hash_by_teid, &teid);
    if (pos >= 0) {
        return &ue_sessions[pos];
    }
    return NULL;
}
```

---

## 七、与实际 5G 核心网的集成

### 7.1 完整架构图

```
5G 网络拓扑：
┌────────────────────────────────────────────────────┐
│                    5G 核心网                        │
│  ┌──────┐  ┌──────┐  ┌──────┐  ┌──────┐            │
│  │ AMF  │  │ SMF  │  │ UPF  │  │ DN   │            │
│  │(认证)│  │(会话)│  │(转发)│  │(数据)│               │
│  └──┬───┘  └──┬───┘  └──┬───┘  └──┬───┘            │
│     │         │         │         │                │
│     └────────>├────────>├────────>│                │
│      Nsmf    PFCP   GTP-U        IP                │
│    (会话建立)  8805   2152       Protocol           │
└────────────────────────────────────────────────────┘
         │         │         │         │
         │         └─────────┴─────────┤
         │                             │
┌────────┴─────┐            ┌─────────┴──────┐
│   gNodeB     │            │   UE (手机)    │
│   (基站)     │            │                │
└──────────────┘            └────────────────┘
   NR 协议                    NR 协议
   无线                       无线

数据流向：
1. UE 向 gNodeB 发送数据（NR 无线）
2. gNodeB 将数据通过 GTP-U 隧道发送到 UPF（端口 2152）
3. UPF 查表找到对应的 DN 地址，转发数据包
4. DN（如 Internet）向 UE 返回数据

会话信息流：
1. AMF 收到 UE 的 PDU 会话请求
2. AMF 选择 SMF
3. SMF 选择 UPF
4. SMF 通过 PFCP 协议（UDP 8805）通知 UPF
5. UPF 建立会话表项
6. UE 开始通过 GTP-U 隧道传输数据
```

---

## 八、总结

| 信息来源 | 协议 | 端口 | 更新频率 | 可靠性 |
|--------|------|------|--------|-------|
| **PFCP** (推荐) | UDP | 8805 | 实时 | 高 ✅ |
| 本地缓存 | - | - | 静态 | 低 |
| GTP-U 学习 | - | 2152 | 动态 | 中 |

### 完整实现建议

1. **立即实施**：实现 PFCP 接收器
2. **功能完善**：添加 PFCP 会话修改/删除处理
3. **性能优化**：使用哈希表加速查询
4. **生产部署**：集成真实 SMF 系统
5. **监控告警**：添加会话生命周期监控

