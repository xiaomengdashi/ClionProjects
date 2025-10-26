# DPDK RSS 配置指南 - 相同 UE IP 数据分配到相同核心

## 一、RSS 工作原理

### 1.1 什么是 RSS (Receive Side Scaling)?
- **网卡硬件特性**，根据数据包的 5 元组（协议、源IP、目的IP、源端口、目的端口）计算哈希值
- 将哈希值映射到不同的接收队列（RX Queue）
- 每个队列绑定到特定的 CPU 核心

### 1.2 工作流程
```
数据包到达网卡
    ↓
网卡计算哈希值 (基于源IP/目的IP等)
    ↓
哈希值 % 队列数 = 目标队列
    ↓
直接将数据包DMA到目标队列
    ↓
对应的核心处理数据包 (无跨核转移!)
    ↓
✅ 相同 UE IP 的数据始终到达同一个核心
```

### 1.3 关键优势
| 优势 | 说明 |
|------|------|
| **硬件分流** | 网卡层面就分配，不需要软件过滤 |
| **零丢包** | 不会出现核心亲和性不匹配的情况 |
| **负载均衡** | 数据包自动均衡分配到各个核心 |
| **缓存效率高** | 相同 UE 的数据总在同一个核心，L3 缓存命中率高 |

---

## 二、RSS 配置方式

### 2.1 DPDK 中的 RSS 配置结构

```c
struct rte_eth_rss_conf {
    uint8_t *rss_key;           /* RSS key pointer */
    uint8_t rss_key_len;        /* RSS key length */
    uint64_t rss_hf;            /* Hash functions to apply */
};

/* 哈希函数类型 */
#define RTE_ETH_RSS_IPV4        0x00000001  /* IPv4 */
#define RTE_ETH_RSS_NONFRAG_IPV4_TCP    0x00000004  /* TCP */
#define RTE_ETH_RSS_NONFRAG_IPV4_UDP    0x00000008  /* UDP */
#define RTE_ETH_RSS_IPV6        0x00000100  /* IPv6 */
/* ... 更多哈希函数 ... */
```

### 2.2 关键参数说明

#### rss_hf (Hash Functions)
决定用哪些字段计算哈希：
- **RTE_ETH_RSS_NONFRAG_IPV4_UDP** : 使用 源IP + 目的IP + 源端口 + 目的端口
- **RTE_ETH_RSS_IP** : 仅使用 源IP + 目的IP

#### rss_key
RSS 哈希密钥，不同密钥会产生不同的哈希分布。

---

## 三、改进方案：多队列 + RSS 配置

### 3.1 当前代码问题 (L633, L649)
```c
struct rte_eth_conf port_conf = {0};
if (rte_eth_dev_configure(portid, 1, 1, &port_conf) < 0) {
    // ❌ 只配置了 1 个 RX 队列和 1 个 TX 队列
    // ❌ 无法利用多个核心
    // ❌ 所有数据包都到同一个队列，必须由一个核心处理
}
```

### 3.2 改进的配置方案

**步骤 1：定义队列数 = 核心数**

```c
#define NUM_RX_QUEUES 4   /* 4 个 RX 队列，对应 4 个核心 */
#define NUM_TX_QUEUES 4   /* 4 个 TX 队列 */
```

**步骤 2：配置 RSS**

```c
static int init_dpdk_ports_with_rss(void) {
    uint16_t portid;
    struct rte_eth_dev_info dev_info;
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_rss_conf rss_conf;

    /* RSS 哈希密钥 (可选，使用默认值或自定义) */
    static uint8_t rss_key[40];  /* Intel 网卡使用 40 字节 */

    /* 初始化 RSS 哈希密钥 (这里使用全 0，也可以自定义) */
    memset(rss_key, 0x42, 40);  /* 42 是任意值 */

    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_dev_info_get(portid, &dev_info);
        printf("[INIT] Port %u: %s\n", portid, dev_info.device->name);

        /* ✅ 配置 RSS */
        memset(&port_conf, 0, sizeof(port_conf));

        /* 启用 RSS */
        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;

        /* 配置 RSS 哈希函数 */
        port_conf.rx_adv_conf.rss_conf.rss_hf =
            RTE_ETH_RSS_NONFRAG_IPV4_TCP |   /* TCP: 根据 源IP+目的IP+源端口+目的端口 */
            RTE_ETH_RSS_NONFRAG_IPV4_UDP |   /* UDP: 根据 源IP+目的IP+源端口+目的端口 */
            RTE_ETH_RSS_IPV4;                 /* 也支持 IPv4 */

        port_conf.rx_adv_conf.rss_conf.rss_key = rss_key;
        port_conf.rx_adv_conf.rss_conf.rss_key_len = 40;

        /* 配置端口：多个队列 */
        if (rte_eth_dev_configure(portid, NUM_RX_QUEUES, NUM_TX_QUEUES, &port_conf) < 0) {
            fprintf(stderr, "Cannot configure port %u\n", portid);
            return -1;
        }

        /* 配置所有 RX 队列 */
        for (uint16_t q = 0; q < NUM_RX_QUEUES; q++) {
            if (rte_eth_rx_queue_setup(portid, q, RX_RING_SIZE,
                                       rte_eth_dev_socket_id(portid),
                                       NULL, mbuf_pool) < 0) {
                fprintf(stderr, "Cannot setup RX queue %u for port %u\n", q, portid);
                return -1;
            }
        }

        /* 配置所有 TX 队列 */
        for (uint16_t q = 0; q < NUM_TX_QUEUES; q++) {
            if (rte_eth_tx_queue_setup(portid, q, TX_RING_SIZE,
                                       rte_eth_dev_socket_id(portid),
                                       NULL) < 0) {
                fprintf(stderr, "Cannot setup TX queue %u for port %u\n", q, portid);
                return -1;
            }
        }

        /* 启动端口 */
        if (rte_eth_dev_start(portid) < 0) {
            fprintf(stderr, "Cannot start port %u\n", portid);
            return -1;
        }

        printf("[INIT] Port %u started with %u RX/TX queues and RSS enabled\n",
               portid, NUM_RX_QUEUES);
    }

    return 0;
}
```

---

## 四、核心绑定到队列

### 4.1 使用 Linux affinity 绑定

每个核心只监听指定的队列：

```c
/* 启动下行任务 */
RTE_LCORE_FOREACH_WORKER(lcore_id) {
    if (lcore_id == 2) {
        /* 核心 2 处理队列 0 */
        rte_eal_remote_launch(lcore_downlink_task_multi_queue,
                            (void *)(uintptr_t)0,  /* 队列 0 */
                            lcore_id);
    }
    else if (lcore_id == 3) {
        /* 核心 3 处理队列 1 */
        rte_eal_remote_launch(lcore_downlink_task_multi_queue,
                            (void *)(uintptr_t)1,  /* 队列 1 */
                            lcore_id);
    }
    else if (lcore_id == 4) {
        /* 核心 4 处理队列 2 */
        rte_eal_remote_launch(lcore_downlink_task_multi_queue,
                            (void *)(uintptr_t)2,  /* 队列 2 */
                            lcore_id);
    }
    else if (lcore_id == 5) {
        /* 核心 5 处理队列 3 */
        rte_eal_remote_launch(lcore_downlink_task_multi_queue,
                            (void *)(uintptr_t)3,  /* 队列 3 */
                            lcore_id);
    }
}
```

### 4.2 修改任务函数

```c
static int lcore_downlink_task_multi_queue(void *arg) {
    uint16_t queue_id = (uintptr_t)arg;  /* 获取队列 ID */
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;

    printf("[LCORE-DL] Downlink task started on core %u for queue %u\n",
           rte_lcore_id(), queue_id);

    while (1) {
        /* 从指定队列接收数据包 */
        nb_rx = rte_eth_rx_burst(port_dn, queue_id, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            RTE_LOG(DEBUG, GENERAL,
                   "[DL-RX] Queue %u received %u packets on core %u\n",
                   queue_id, nb_rx, rte_lcore_id());

            for (uint16_t i = 0; i < nb_rx; i++) {
                process_downlink_packet(bufs[i]);
            }
        }
    }

    return 0;
}
```

---

## 五、数据流对比

### 5.1 改进前 (所有核心都收到所有数据)
```
┌─────────────────┐
│ 网卡 (队列 0)   │
└────────┬────────┘
         │ (所有数据)
    ┌────┴──────┬───────────┬────────┐
    │            │           │        │
核心2(✅)   核心3(❌)   核心4(❌)  核心5(❌)
处理         过滤         过滤      过滤
           丢弃UE1      丢弃UE1   丢弃UE1
           丢弃UE3      丢弃UE3   丢弃UE3
```

**问题**：50% 的数据被丢弃！


### 5.2 改进后 (网卡直接分流)
```
┌────────────────────────────────────┐
│  网卡 RSS 哈希                      │
│  根据源IP/目的IP自动分流           │
└─┬──────┬────────┬────────┬─────┘
  │      │        │        │
队列0   队列1   队列2   队列3
  │      │        │        │
核心2  核心3   核心4   核心5
  │      │        │        │
 UE1   UE2    UE3    UE4
 UE5   UE6    UE7    UE8
(同UE  (同UE  (同UE  (同UE
的IP   的IP   的IP   的IP
来自   来自   来自   来自
同一   同一   同一   同一
队列)  队列)  队列)  队列)
```

**优点**：
- ✅ 没有数据丢失
- ✅ 相同 UE 的数据自动到达同一个核心
- ✅ 完全不需要软件过滤和丢弃


---

## 六、RSS 哈希计算示例

### 6.1 IPv4 UDP 哈希（GTP-U）

GTP-U 数据包结构：
```
┌──────────────────┬────────────┬────────────┐
│   外层 IP 头     │  UDP 头    │ GTP-U 头   │
├──────────────────┼────────────┼────────────┤
│ 源IP: 192.168.x.y
│ 目的IP: 192.168.1.50 (UPF)
│ 源端口: 高端口
│ 目的端口: 2152 (GTP-U)
└──────────────────┴────────────┴────────────┘
```

RSS 哈希计算：
```
哈希输入 = {源IP, 目的IP, 源端口, 目的端口, 协议}

对于相同 UE 的多个数据包：
- 源IP 可能不同（不同时间的RAN设备）
- 目的IP 总是 192.168.1.50 (UPF)
- 源端口 可能不同
- 目的端口 总是 2152
- 协议 总是 UDP

⚠️ 问题：源IP/源端口不同，可能导致不同 UE 的数据到不同队列！

✅ 解决方案：使用 RTE_ETH_RSS_IP (仅基于 IP 地址)，或者
            配置网卡提取内层 IP (GTP-U 内层的 UE IP)
```

---

## 七、高级：提取 GTP-U 内层 IP 进行哈希

### 7.1 使用 Flow Rules (DPDK >= 19.05)

某些网卡支持隧道感知的 RSS，可以对 GTP-U 内层 IP 进行哈希：

```c
struct rte_flow_action_rss action_rss = {
    .types = RTE_ETH_RSS_NONFRAG_IPV4_UDP,  /* 对 GTP 内层 UDP 哈希 */
    .level = 1,  /* 内层 (隧道内) */
    .queues = queues,
    .queue_num = NUM_RX_QUEUES,
};
```

**优点**：相同 UE 的所有数据（无论源 gNodeB）都会进入同一队列！

---

## 八、验证 RSS 是否工作

### 8.1 检查网卡 RSS 能力

```bash
# 查看网卡支持的哈希函数
ethtool -n <interface> rx-flow-hash udp4

# 或使用 DPDK 工具
dpdk-dump-info | grep RSS
```

### 8.2 检查队列分配

```c
/* 添加调试代码 */
for (int i = 0; i < nb_rx; i++) {
    struct rte_mbuf *mbuf = bufs[i];
    struct rte_ipv4_hdr *ipv4_hdr = rte_pktmbuf_mtod_offset(mbuf,
                                                              struct rte_ipv4_hdr *,
                                                              sizeof(struct rte_ether_hdr));
    uint32_t dst_ip = rte_be_to_cpu_32(ipv4_hdr->dst_addr);

    /* RSS 哈希值存储在 mbuf 中 */
    printf("Packet dst_ip=0x%x, RSS_hash=0x%x, queue=%u, core=%u\n",
           dst_ip, mbuf->hash.rss, queue_id, rte_lcore_id());
}
```

---

## 九、完整改进代码框架

```c
/* 1. 增加宏定义 */
#define NUM_RX_QUEUES 4
#define NUM_TX_QUEUES 4

/* 2. 修改初始化函数 */
static int init_dpdk_ports_with_rss(void) {
    // ... (参考上面的详细代码)
}

/* 3. 修改任务函数使用队列 ID */
static int lcore_downlink_task_multi_queue(void *arg) {
    uint16_t queue_id = (uintptr_t)arg;
    // ... (参考上面的详细代码)
}

/* 4. 修改 main 函数中的启动逻辑 */
int main(int argc, char *argv[]) {
    // ... 初始化代码

    // 调用新的初始化函数
    if (init_dpdk_ports_with_rss() < 0) {
        return -1;
    }

    // 为每个核心绑定一个队列
    int queue_idx = 0;
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (lcore_id < (2 + NUM_RX_QUEUES)) {  /* 假设队列分配从核心 2 开始 */
            rte_eal_remote_launch(lcore_downlink_task_multi_queue,
                                (void *)(uintptr_t)queue_idx,
                                lcore_id);
            queue_idx++;
        }
    }

    // ... 其他代码
}
```

---

## 十、总结与对比

| 特性 | 改进前 | 改进后 (RSS) |
|------|------|----------|
| **RX 队列数** | 1 个 | 多个 (= 核心数) |
| **数据分配** | 软件过滤，可能丢弃 | 网卡硬件直接分流 |
| **相同 UE** | 多个核心重复处理 | 自动到同一队列/核心 |
| **数据丢失** | 是 (50%+) | 否 (0%) |
| **性能** | 低 (跨核竞争) | 高 (核心本地化) |
| **可扩展性** | 不好 | 好 |

**建议**：立即采用 RSS 多队列方案，替代当前的软件亲和性过滤！

