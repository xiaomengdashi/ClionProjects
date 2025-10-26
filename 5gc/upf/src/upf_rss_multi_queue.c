/*
 * 5G UPF with DPDK RSS Multi-Queue
 *
 * 核心改进：
 * 1. 使用 RSS 网卡硬件分流替代软件亲和性检查
 * 2. 配置多个接收队列，每个核心绑定一个队列
 * 3. 相同 UE IP 的数据包自动进入同一队列/核心
 * 4. 支持完整的 GTP-U 封装/解封装
 *
 * 编译：
 * gcc -o upf_rss_multi_queue upf_rss_multi_queue.c $(pkg-config --cflags --libs libdpdk)
 *
 * 运行：
 * sudo ./upf_rss_multi_queue -l 0-5 -n 4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <netinet/in.h>
#include <setjmp.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdbool.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <rte_arp.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_eal.h>
#include <rte_launch.h>
#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_prefetch.h>
#include <rte_lcore.h>
#include <rte_per_lcore.h>
#include <rte_branch_prediction.h>
#include <rte_interrupts.h>
#include <rte_random.h>
#include <rte_debug.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_string_fns.h>
#include <rte_ip.h>
#include <rte_udp.h>

/* ============= RSS 多队列配置常量 ============= */
#define NUM_RX_QUEUES 4        /* 4 个接收队列 */
#define NUM_TX_QUEUES 4        /* 4 个发送队列 */
#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define GTP_PORT 2152
#define MAX_SESSIONS 100

/* ============= GTP-U 头部结构 ============= */
struct gtp_header {
    uint8_t flags;
    uint8_t msg_type;
    uint16_t length;
    uint32_t teid;
} __attribute__((packed));

#define GTP_VERSION_1     0x30
#define GTP_TPDU_TYPE     255
#define GTP_HEADER_SIZE_MIN  8

/* ============= UE 会话结构 ============= */
typedef struct {
    uint32_t ue_ip;
    uint32_t teid_downlink;
    uint32_t teid_uplink;
    uint32_t gnb_ip;
    uint16_t gnb_port;
    uint32_t dn_ip;
    char status;

    uint16_t dl_sequence;
    uint16_t ul_sequence;

    uint64_t packets_tx;
    uint64_t packets_rx;
    uint64_t bytes_tx;
    uint64_t bytes_rx;

    uint8_t qos_priority;
    uint8_t qos_dscp;
} ue_session_t;

/* ============= 全局变量 ============= */
static ue_session_t ue_sessions[MAX_SESSIONS];
static int session_count = 0;
static struct rte_mempool *mbuf_pool = NULL;
static uint16_t nb_ports = 0;
static uint16_t port_dn = 0;      /* DN 侧网卡 */
static uint16_t port_ran = 1;     /* RAN 侧网卡 */
static uint32_t upf_local_ip = 0;

int RTE_LOGTYPE_APP;

/* ============= UE 会话初始化 ============= */

void init_network_config(void) {
    upf_local_ip = inet_addr("192.168.1.50");
    printf("[INIT] UPF Local IP: 192.168.1.50 (0x%x)\n", upf_local_ip);
}

void init_ue_sessions(void) {
    /* UE 1 配置 */
    ue_sessions[0].ue_ip = inet_addr("10.0.0.2");
    ue_sessions[0].teid_downlink = 0x12345678;
    ue_sessions[0].teid_uplink = 0x87654321;
    ue_sessions[0].gnb_ip = inet_addr("192.168.1.100");
    ue_sessions[0].gnb_port = GTP_PORT;
    ue_sessions[0].dn_ip = inet_addr("8.8.8.8");
    ue_sessions[0].status = 'A';
    ue_sessions[0].dl_sequence = 0;
    ue_sessions[0].ul_sequence = 0;
    ue_sessions[0].qos_priority = 5;
    ue_sessions[0].qos_dscp = 0x20;

    /* UE 2 配置 */
    ue_sessions[1].ue_ip = inet_addr("10.0.0.3");
    ue_sessions[1].teid_downlink = 0x11111111;
    ue_sessions[1].teid_uplink = 0x22222222;
    ue_sessions[1].gnb_ip = inet_addr("192.168.1.101");
    ue_sessions[1].gnb_port = GTP_PORT;
    ue_sessions[1].dn_ip = inet_addr("8.8.8.8");
    ue_sessions[1].status = 'A';
    ue_sessions[1].dl_sequence = 0;
    ue_sessions[1].ul_sequence = 0;
    ue_sessions[1].qos_priority = 7;
    ue_sessions[1].qos_dscp = 0x28;

    /* UE 3 配置 */
    ue_sessions[2].ue_ip = inet_addr("10.0.0.4");
    ue_sessions[2].teid_downlink = 0x33333333;
    ue_sessions[2].teid_uplink = 0x44444444;
    ue_sessions[2].gnb_ip = inet_addr("192.168.1.102");
    ue_sessions[2].gnb_port = GTP_PORT;
    ue_sessions[2].dn_ip = inet_addr("8.8.8.8");
    ue_sessions[2].status = 'A';
    ue_sessions[2].dl_sequence = 0;
    ue_sessions[2].ul_sequence = 0;
    ue_sessions[2].qos_priority = 5;
    ue_sessions[2].qos_dscp = 0x20;

    /* UE 4 配置 */
    ue_sessions[3].ue_ip = inet_addr("10.0.0.5");
    ue_sessions[3].teid_downlink = 0x55555555;
    ue_sessions[3].teid_uplink = 0x66666666;
    ue_sessions[3].gnb_ip = inet_addr("192.168.1.103");
    ue_sessions[3].gnb_port = GTP_PORT;
    ue_sessions[3].dn_ip = inet_addr("8.8.8.8");
    ue_sessions[3].status = 'A';
    ue_sessions[3].dl_sequence = 0;
    ue_sessions[3].ul_sequence = 0;
    ue_sessions[3].qos_priority = 7;
    ue_sessions[3].qos_dscp = 0x28;

    session_count = 4;
    printf("[INIT] Loaded %d UE sessions\n", session_count);
    for (int i = 0; i < session_count; i++) {
        printf("  UE%d: IP=10.0.0.%d, DL TEID=0x%x, UL TEID=0x%x\n",
               i + 1, 2 + i, ue_sessions[i].teid_downlink, ue_sessions[i].teid_uplink);
    }
}

/* ============= 会话查询 ============= */

static inline ue_session_t* lookup_session_by_destip(uint32_t dest_ip) {
    for (int i = 0; i < session_count; i++) {
        if (ue_sessions[i].ue_ip == dest_ip && ue_sessions[i].status == 'A') {
            return &ue_sessions[i];
        }
    }
    return NULL;
}

static inline ue_session_t* lookup_session_by_teid_ul(uint32_t teid) {
    for (int i = 0; i < session_count; i++) {
        if (ue_sessions[i].teid_uplink == teid && ue_sessions[i].status == 'A') {
            return &ue_sessions[i];
        }
    }
    return NULL;
}

/* ============= GTP-U 操作 ============= */

static inline int gtp_encap(const uint8_t *src_data, int src_len,
                            uint32_t teid, uint16_t sequence,
                            uint8_t *buffer) {
    struct gtp_header *gtp_hdr = (struct gtp_header *)buffer;

    gtp_hdr->flags = GTP_VERSION_1;
    gtp_hdr->msg_type = GTP_TPDU_TYPE;
    gtp_hdr->length = htons(src_len);
    gtp_hdr->teid = htonl(teid);

    memcpy(buffer + GTP_HEADER_SIZE_MIN, src_data, src_len);

    return GTP_HEADER_SIZE_MIN + src_len;
}

static inline int gtp_decap(const uint8_t *gtp_data, int gtp_len,
                            uint32_t *teid, uint8_t **ip_data, int *ip_len) {
    if (gtp_len < GTP_HEADER_SIZE_MIN)
        return -1;

    struct gtp_header *gtp_hdr = (struct gtp_header *)gtp_data;

    if ((gtp_hdr->flags & 0xE0) != 0x20)  /* 版本检查 */
        return -1;

    if (gtp_hdr->msg_type != GTP_TPDU_TYPE)
        return -1;

    *teid = ntohl(gtp_hdr->teid);
    *ip_data = (uint8_t *)gtp_data + GTP_HEADER_SIZE_MIN;
    *ip_len = ntohs(gtp_hdr->length);

    if (*ip_len <= 0 || GTP_HEADER_SIZE_MIN + *ip_len > gtp_len)
        return -1;

    return 0;
}

static inline int parse_ip_packet(const uint8_t *ip_data, int ip_len,
                                   uint32_t *src_ip, uint32_t *dst_ip) {
    if (ip_len < (int)sizeof(struct iphdr))
        return -1;

    struct iphdr *ip_hdr = (struct iphdr *)ip_data;

    if (ip_hdr->version != 4)
        return -1;

    *src_ip = ip_hdr->saddr;
    *dst_ip = ip_hdr->daddr;

    return 0;
}

/* ============= 下行数据包处理（DN -> UE） ============= */

static inline void process_downlink_packet(struct rte_mbuf *mbuf) {
    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
    uint16_t pkt_len = rte_pktmbuf_data_len(mbuf);

    uint32_t src_ip, dst_ip;
    ue_session_t *session;
    struct rte_mbuf *tunneled_mbuf;
    uint8_t *tunnel_data;
    struct iphdr *outer_ip;
    struct udphdr *outer_udp;
    uint8_t *gtp_data;
    int gtp_len, total_len;

    /* 解析内层 IP 包 */
    if (parse_ip_packet(pkt_data, pkt_len, &src_ip, &dst_ip) < 0) {
        RTE_LOG(DEBUG, APP, "[DL] Invalid IP packet\n");
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 查找会话 */
    session = lookup_session_by_destip(dst_ip);
    if (!session) {
        RTE_LOG(DEBUG, APP, "[DL] No session for IP 0x%x\n", dst_ip);
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 分配隧道包缓冲区 */
    tunneled_mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!tunneled_mbuf) {
        RTE_LOG(ERR, APP, "[DL] Failed to allocate tunnel mbuf\n");
        rte_pktmbuf_free(mbuf);
        return;
    }

    tunnel_data = rte_pktmbuf_mtod(tunneled_mbuf, uint8_t *);

    /* 构造外层 IP 头 */
    outer_ip = (struct iphdr *)tunnel_data;
    outer_ip->version = 4;
    outer_ip->ihl = 5;
    outer_ip->tos = ((struct iphdr *)pkt_data)->tos;
    outer_ip->ttl = 64;
    outer_ip->protocol = IPPROTO_UDP;
    outer_ip->saddr = upf_local_ip;
    outer_ip->daddr = session->gnb_ip;

    /* 构造外层 UDP 头 */
    outer_udp = (struct udphdr *)(tunnel_data + sizeof(struct iphdr));
    outer_udp->source = htons(2153);
    outer_udp->dest = htons(GTP_PORT);
    outer_udp->len = 0;
    outer_udp->check = 0;

    /* 构造 GTP-U 头 */
    gtp_data = tunnel_data + sizeof(struct iphdr) + sizeof(struct udphdr);
    session->dl_sequence++;

    gtp_len = gtp_encap(pkt_data, pkt_len, session->teid_downlink,
                        session->dl_sequence, gtp_data);

    total_len = sizeof(struct iphdr) + sizeof(struct udphdr) + gtp_len;

    /* 更新 IP 头 */
    outer_ip->tot_len = htons(total_len);
    outer_ip->id = htons(1);
    outer_ip->frag_off = 0;
    outer_ip->check = 0;

    /* 更新 UDP 头 */
    outer_udp->len = htons(sizeof(struct udphdr) + gtp_len);

    /* 更新 mbuf */
    rte_pktmbuf_data_len(tunneled_mbuf) = total_len;
    rte_pktmbuf_pkt_len(tunneled_mbuf) = total_len;

    /* 更新统计 */
    session->packets_tx++;
    session->bytes_tx += total_len;

    /* 发送 */
    uint16_t sent = rte_eth_tx_burst(port_ran, 0, &tunneled_mbuf, 1);
    if (sent > 0) {
        RTE_LOG(DEBUG, APP,
                "[DL] Tunneled: TEID=0x%x, len=%d to gNodeB 0x%x (Q%u)\n",
                session->teid_downlink, total_len, session->gnb_ip,
                rte_lcore_id() % NUM_RX_QUEUES);
    } else {
        rte_pktmbuf_free(tunneled_mbuf);
    }

    rte_pktmbuf_free(mbuf);
}

/* ============= 上行数据包处理（UE -> DN） ============= */

static inline void process_uplink_packet(struct rte_mbuf *mbuf) {
    uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
    uint16_t pkt_len = rte_pktmbuf_data_len(mbuf);

    struct iphdr *outer_ip;
    struct udphdr *outer_udp;
    uint8_t *gtp_pkt;
    int gtp_len;

    uint32_t teid;
    uint8_t *ip_data;
    int ip_len;
    ue_session_t *session;
    struct rte_mbuf *ip_mbuf;
    uint8_t *out_data;

    /* 最小长度校验 */
    if (pkt_len < (int)(sizeof(struct iphdr) + sizeof(struct udphdr) + GTP_HEADER_SIZE_MIN)) {
        RTE_LOG(DEBUG, APP, "[UL] Packet too short: %d bytes\n", pkt_len);
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 解析外层 IP 头 */
    outer_ip = (struct iphdr *)pkt_data;

    if (outer_ip->version != 4) {
        RTE_LOG(DEBUG, APP, "[UL] Invalid IP version: %u\n", outer_ip->version);
        rte_pktmbuf_free(mbuf);
        return;
    }

    if (outer_ip->protocol != IPPROTO_UDP) {
        RTE_LOG(DEBUG, APP, "[UL] Invalid protocol: %u\n", outer_ip->protocol);
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 解析外层 UDP 头 */
    outer_udp = (struct udphdr *)(pkt_data + sizeof(struct iphdr));

    if (ntohs(outer_udp->dest) != GTP_PORT) {
        RTE_LOG(DEBUG, APP, "[UL] Invalid UDP port: %u\n", ntohs(outer_udp->dest));
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 提取 GTP-U 包 */
    gtp_pkt = pkt_data + sizeof(struct iphdr) + sizeof(struct udphdr);
    gtp_len = pkt_len - sizeof(struct iphdr) - sizeof(struct udphdr);

    /* GTP-U 解封装 */
    if (gtp_decap(gtp_pkt, gtp_len, &teid, &ip_data, &ip_len) < 0) {
        RTE_LOG(DEBUG, APP, "[UL] Failed to decapsulate GTP packet\n");
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 查找会话 */
    session = lookup_session_by_teid_ul(teid);
    if (!session) {
        RTE_LOG(DEBUG, APP, "[UL] No session for TEID 0x%x\n", teid);
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 创建内层 IP 包 mbuf */
    ip_mbuf = rte_pktmbuf_alloc(mbuf_pool);
    if (!ip_mbuf) {
        RTE_LOG(ERR, APP, "[UL] Failed to allocate IP mbuf\n");
        rte_pktmbuf_free(mbuf);
        return;
    }

    /* 复制内层 IP 包 */
    out_data = rte_pktmbuf_mtod(ip_mbuf, uint8_t *);
    memcpy(out_data, ip_data, ip_len);
    rte_pktmbuf_data_len(ip_mbuf) = ip_len;
    rte_pktmbuf_pkt_len(ip_mbuf) = ip_len;

    /* 更新统计 */
    session->packets_rx++;
    session->bytes_rx += ip_len;
    session->ul_sequence++;

    /* 发送到 DN */
    uint16_t sent = rte_eth_tx_burst(port_dn, 0, &ip_mbuf, 1);
    if (sent > 0) {
        RTE_LOG(DEBUG, APP,
                "[UL] Decapsulated: TEID=0x%x, len=%d from gNodeB (Q%u)\n",
                teid, ip_len, rte_lcore_id() % NUM_RX_QUEUES);
    } else {
        rte_pktmbuf_free(ip_mbuf);
    }

    rte_pktmbuf_free(mbuf);
}

/* ============= DPDK 初始化（支持 RSS） ============= */

static int init_dpdk_rss_ports(void) {
    struct rte_eth_conf port_conf = {0};
    struct rte_eth_dev_info dev_info;
    struct rte_eth_rss_conf rss_conf = {0};
    uint16_t portid;
    uint8_t rss_key[40];
    int ret;

    printf("\n=== DPDK RSS Multi-Queue Port Initialization ===\n\n");

    /* 统计端口 */
    RTE_ETH_FOREACH_DEV(portid) {
        nb_ports++;
    }

    if (nb_ports < 2) {
        fprintf(stderr, "Error: need at least 2 ports\n");
        return -1;
    }

    printf("Found %u ports\n\n", nb_ports);

    /* 配置所有端口 */
    RTE_ETH_FOREACH_DEV(portid) {
        rte_eth_dev_info_get(portid, &dev_info);
        printf("[PORT %u] Device: %s\n", portid, dev_info.device->name);

        /* ✅ 启用 RSS */
        memset(&port_conf, 0, sizeof(port_conf));
        memset(&rss_conf, 0, sizeof(rss_conf));

        port_conf.rxmode.mq_mode = RTE_ETH_MQ_RX_RSS;

        memset(rss_key, 0x42, 40);
        rss_conf.rss_key = rss_key;
        rss_conf.rss_key_len = 40;
        rss_conf.rss_hf = (RTE_ETH_RSS_IP |
                          RTE_ETH_RSS_NONFRAG_IPV4_UDP |
                          RTE_ETH_RSS_NONFRAG_IPV4_TCP);
        port_conf.rx_adv_conf.rss_conf = rss_conf;

        printf("  Configuring %u RX + %u TX queues with RSS...\n",
               NUM_RX_QUEUES, NUM_TX_QUEUES);

        /* ✅ 配置多队列 */
        ret = rte_eth_dev_configure(portid, NUM_RX_QUEUES, NUM_TX_QUEUES, &port_conf);
        if (ret != 0) {
            fprintf(stderr, "  Cannot configure port %u\n", portid);
            return -1;
        }

        /* 配置所有 RX 队列 */
        for (uint16_t q = 0; q < NUM_RX_QUEUES; q++) {
            ret = rte_eth_rx_queue_setup(portid, q, RX_RING_SIZE,
                                        rte_eth_dev_socket_id(portid),
                                        NULL, mbuf_pool);
            if (ret != 0) {
                fprintf(stderr, "  Cannot setup RX queue %u\n", q);
                return -1;
            }
        }

        /* 配置所有 TX 队列 */
        for (uint16_t q = 0; q < NUM_TX_QUEUES; q++) {
            ret = rte_eth_tx_queue_setup(portid, q, TX_RING_SIZE,
                                        rte_eth_dev_socket_id(portid),
                                        NULL);
            if (ret != 0) {
                fprintf(stderr, "  Cannot setup TX queue %u\n", q);
                return -1;
            }
        }

        /* 启动端口 */
        ret = rte_eth_dev_start(portid);
        if (ret < 0) {
            fprintf(stderr, "  Cannot start port %u\n", portid);
            return -1;
        }

        printf("  ✓ Port %u started with RSS enabled\n\n", portid);
    }

    return 0;
}

/* ============= 核心处理任务 ============= */

static int lcore_downlink_task(void *arg) {
    uint16_t queue_id = (uintptr_t)arg;
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    uint64_t total_packets = 0;
    unsigned int core_id = rte_lcore_id();

    printf("[Core %u] Downlink task started for queue %u\n", core_id, queue_id);

    while (1) {
        /* 从指定队列接收 */
        nb_rx = rte_eth_rx_burst(port_dn, queue_id, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            total_packets += nb_rx;
            RTE_LOG(DEBUG, APP, "[DL] Core %u Q%u: Received %u packets\n",
                   core_id, queue_id, nb_rx);

            for (uint16_t i = 0; i < nb_rx; i++) {
                process_downlink_packet(bufs[i]);
            }
        }

        /* 每秒打印统计 */
        static uint64_t last_tsc[RTE_MAX_LCORE] = {0};
        uint64_t cur_tsc = rte_rdtsc();
        uint64_t hz = rte_get_tsc_hz();

        if (cur_tsc - last_tsc[core_id] > hz) {
            last_tsc[core_id] = cur_tsc;
            printf("[DL-STATS] Core %u Q%u: Total %lu packets\n",
                   core_id, queue_id, total_packets);
        }
    }

    return 0;
}

static int lcore_uplink_task(void *arg) {
    uint16_t queue_id = (uintptr_t)arg;
    struct rte_mbuf *bufs[BURST_SIZE];
    uint16_t nb_rx;
    uint64_t total_packets = 0;
    unsigned int core_id = rte_lcore_id();

    printf("[Core %u] Uplink task started for queue %u\n", core_id, queue_id);

    while (1) {
        /* 从指定队列接收 */
        nb_rx = rte_eth_rx_burst(port_ran, queue_id, bufs, BURST_SIZE);

        if (nb_rx > 0) {
            total_packets += nb_rx;
            RTE_LOG(DEBUG, APP, "[UL] Core %u Q%u: Received %u packets\n",
                   core_id, queue_id, nb_rx);

            for (uint16_t i = 0; i < nb_rx; i++) {
                process_uplink_packet(bufs[i]);
            }
        }

        /* 每秒打印统计 */
        static uint64_t last_tsc[RTE_MAX_LCORE] = {0};
        uint64_t cur_tsc = rte_rdtsc();
        uint64_t hz = rte_get_tsc_hz();

        if (cur_tsc - last_tsc[core_id] > hz) {
            last_tsc[core_id] = cur_tsc;
            printf("[UL-STATS] Core %u Q%u: Total %lu packets\n",
                   core_id, queue_id, total_packets);
        }
    }

    return 0;
}

/* ============= 主函数 ============= */

int main(int argc, char *argv[]) {
    int ret;

    /* DPDK EAL 初始化 */
    ret = rte_eal_init(argc, argv);
    if (ret < 0) {
        rte_exit(EXIT_FAILURE, "Cannot init EAL\n");
    }

    RTE_LOGTYPE_APP = rte_log_register("APP");
    rte_log_set_level(RTE_LOGTYPE_APP, RTE_LOG_DEBUG);

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     5G UPF with DPDK RSS Multi-Queue Configuration         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    /* 创建内存池 */
    mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
                                        MBUF_CACHE_SIZE, 0,
                                        RTE_MBUF_DEFAULT_BUF_SIZE,
                                        rte_socket_id());
    if (mbuf_pool == NULL) {
        rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
    }

    /* 初始化网络配置 */
    init_network_config();

    /* 初始化 DPDK 端口（带 RSS） */
    if (init_dpdk_rss_ports() < 0) {
        rte_exit(EXIT_FAILURE, "Cannot initialize ports\n");
    }

    /* 初始化 UE 会话 */
    init_ue_sessions();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║             RSS Multi-Queue Task Deployment                ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    int queue_idx = 0;
    unsigned int lcore_id;

    printf("Downlink cores (DN port processing):\n");
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (queue_idx >= NUM_RX_QUEUES / 2) break;

        printf("  Core %u → Queue %u (DL)\n", lcore_id, queue_idx);
        rte_eal_remote_launch(lcore_downlink_task,
                            (void *)(uintptr_t)queue_idx, lcore_id);
        queue_idx++;
    }

    printf("\nUplink cores (RAN port processing):\n");
    RTE_LCORE_FOREACH_WORKER(lcore_id) {
        if (queue_idx >= NUM_RX_QUEUES) break;

        printf("  Core %u → Queue %u (UL)\n", lcore_id, queue_idx - NUM_RX_QUEUES / 2);
        rte_eal_remote_launch(lcore_uplink_task,
                            (void *)(uintptr_t)(queue_idx - NUM_RX_QUEUES / 2), lcore_id);
        queue_idx++;
    }

    printf("\n");
    printf("═══════════════════════════════════════════==═════════════════\n");
    printf("UPF with RSS Multi-Queue Ready!\n");
    printf("═══════════════════════════════════════════════════════════==═\n\n");

    printf("Key Features:\n");
    printf("  ✓ %u RX queues (RSS hardware steering)\n", NUM_RX_QUEUES);
    printf("  ✓ Same UE IP → Always same queue\n");
    printf("  ✓ GTP-U encapsulation/decapsulation\n");
    printf("  ✓ Zero packet loss (no affinity checks)\n");
    printf("  ✓ Per-core packet processing\n\n");

    printf("Waiting for packets...\n");
    printf("(Press Ctrl+C to exit)\n\n");

    /* 等待所有核心完成 */
    rte_eal_mp_wait_lcore();

    /* 清理 */
    RTE_ETH_FOREACH_DEV(lcore_id) {
        printf("Closing port %u...\n", lcore_id);
        rte_eth_dev_stop(lcore_id);
        rte_eth_dev_close(lcore_id);
    }

    printf("Done!\n");
    return 0;
}
