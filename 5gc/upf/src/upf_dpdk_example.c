/*
 * 5G UPF with DPDK 示例代码
 * 功能: 使用 DPDK 网卡驱动接收/发送数据包
 * 编译: gcc -o upf_dpdk_example upf_dpdk_example.c $(pkg-config --cflags --libs libdpdk)
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_mbuf.h>
#include <rte_mempool.h>
#include <rte_cycles.h>
#include <rte_lcore.h>
#include <rte_log.h>

/* ============= 常量定义 ============= */

#define NUM_MBUFS 8191
#define MBUF_CACHE_SIZE 250
#define BURST_SIZE 32
#define GTP_PORT 2152

#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024

#define US_PER_S 1000000
#define BURST_TX_DRAIN_US 100

/* 定义日志类型 */
int RTE_LOGTYPE_GENERAL;

/* ============= 网络参数 ============= */
/* UPF 本地 IP 地址（用于构造外层 IP 头） */
static uint32_t upf_local_ip = 0;      /* 将在初始化时设置 */
#define UPF_LOCAL_IP_STR "192.168.1.50" /* UPF 在 IP 网络中的地址 */

/* ============= GTP-U 头部结构 ============= */
/* RFC 3152: GTP-U v1 头部定义 */

/* 基础 GTP-U 头部 (8 字节，无扩展字段) */
struct gtp_header {
	uint8_t flags;      /* Version(3bit)=001, PT(1bit)=1, Spare(3bit)=0, E/S/PN(3bit)=0 = 0x30 */
	uint8_t msg_type;   /* Message Type = 255 (T-PDU) */
	uint16_t length;    /* GTP Payload Length (不包括 8 字节头部) */
	uint32_t teid;      /* Tunnel Endpoint Identifier */
} __attribute__((packed));

/* GTP-U 扩展头部 (可选，当 E=1 时存在) */
struct gtp_header_ext {
	struct gtp_header base;
	uint16_t sequence;       /* 序列号（当 S=1 时有效）*/
	uint8_t n_pdu_number;    /* N-PDU 号（当 PN=1 时有效）*/
	uint8_t next_ext_hdr;    /* 下一个扩展头类型（当 E=1 时有效）*/
} __attribute__((packed));

/* Flags 字段位定义 */
#define GTP_VERSION_1     0x30  /* 版本 1，PT=1，预留位=0，E/S/PN=0 */
#define GTP_TPDU_TYPE     255   /* T-PDU 消息类型 */

/* 扩展头标志 */
#define GTP_FLAG_EXTENSION  0x04  /* E 标志: Bit 0，表示有扩展头 */
#define GTP_FLAG_SEQUENCE   0x02  /* S 标志: Bit 1，表示有序列号 */
#define GTP_FLAG_NPDU       0x01  /* PN标志: Bit 2，表示有 N-PDU 号 */

/* 版本校验掩码和有效值 */
#define GTP_VERSION_MASK  0xE0  /* 掩盖版本和 PT 位 (1110 0000)，共 5 bit */
#define GTP_V1_VALID      0x20  /* 版本 1 (0010 0000) */

/* 计算 GTP 头部大小的宏 */
#define GTP_HEADER_SIZE_MIN  8  /* 最小头部大小 */
#define GTP_HEADER_SIZE_EXT  12 /* 包含序列号和 N-PDU 号的大小 */

/* ============= UE 会话结构 ============= */

typedef struct {
	uint32_t ue_ip;
	uint32_t teid_downlink;
	uint32_t teid_uplink;
	uint32_t gnb_ip;
	uint16_t gnb_port;
	uint32_t dn_ip;
	char status;

	/* 序列号计数器（用于 GTP 封装） */
	uint16_t dl_sequence;    /* 下行序列号 */
	uint16_t ul_sequence;    /* 上行序列号 */

	/* 统计信息 */
	uint64_t packets_tx;
	uint64_t packets_rx;
	uint64_t bytes_tx;
	uint64_t bytes_rx;

	/* QoS 相关 */
	uint8_t qos_priority;    /* 优先级 (0-15) */
	uint8_t qos_dscp;        /* DSCP 值 */

	/* 核心亲和性（会话绑定到指定核心） */
	uint16_t affinity_core;  /* 绑定的核心 ID */
	char affinity_enabled;   /* 是否启用亲和性 */
} ue_session_t;

#define MAX_SESSIONS 1000
static ue_session_t ue_sessions[MAX_SESSIONS];
static int session_count = 0;

/* ============= DPDK 全局变量 ============= */

static struct rte_mempool *mbuf_pool;
static uint16_t nb_ports = 0;
static uint16_t port_dn = 0;    /* DN 侧网卡 (接收数据包) */
static uint16_t port_ran = 1;   /* RAN 侧网卡 (发送到基站) */

/* ============= 核心亲和性配置 ============= */
/* 下行处理核心列表 */
static uint16_t dl_cores[8] = {0};
static int dl_core_count = 0;

/* 上行处理核心列表 */
static uint16_t ul_cores[8] = {0};
static int ul_core_count = 0;

/* 用于均衡分配会话到核心的计数器 */
static int session_affinity_idx = 0;

/* ============= 初始化 UE 会话 ============= */

void init_network_config(void) {
	/* 初始化 UPF 本地 IP 地址 */
	upf_local_ip = inet_addr(UPF_LOCAL_IP_STR);
	printf("[INIT] UPF Local IP: %s (0x%x)\n", UPF_LOCAL_IP_STR, upf_local_ip);
}

/**
 * 注册下行处理核心
 * 用于会话亲和性分配
 */
void register_downlink_core(uint16_t lcore_id) {
	if (dl_core_count < 8) {
		dl_cores[dl_core_count++] = lcore_id;
		printf("[INIT] Registered downlink core: %u\n", lcore_id);
	}
}

/**
 * 注册上行处理核心
 * 用于会话亲和性分配
 */
void register_uplink_core(uint16_t lcore_id) {
	if (ul_core_count < 8) {
		ul_cores[ul_core_count++] = lcore_id;
		printf("[INIT] Registered uplink core: %u\n", lcore_id);
	}
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
	ue_sessions[0].qos_priority = 5;     /* 默认中等优先级 */
	ue_sessions[0].qos_dscp = 0x20;      /* BE (Best Effort) */

	/* 核心亲和性: 绑定到下行核心 0 */
	if (dl_core_count > 0) {
		ue_sessions[0].affinity_core = dl_cores[session_affinity_idx % dl_core_count];
		ue_sessions[0].affinity_enabled = 1;
		session_affinity_idx++;
	}

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
	ue_sessions[1].qos_priority = 7;     /* 中高优先级 */
	ue_sessions[1].qos_dscp = 0x28;      /* AF31 (保障转发) */

	/* 核心亲和性: 绑定到下行核心 1 */
	if (dl_core_count > 0) {
		ue_sessions[1].affinity_core = dl_cores[session_affinity_idx % dl_core_count];
		ue_sessions[1].affinity_enabled = 1;
		session_affinity_idx++;
	}

	session_count = 2;
	printf("[INIT] UE Sessions loaded: %d active sessions\n", session_count);
	printf("[INIT] Session affinity configuration:\n");
	for (int i = 0; i < session_count; i++) {
		if (ue_sessions[i].affinity_enabled) {
			printf("  UE%d (IP: 10.0.0.%d) -> Core %u\n",
			       i + 1, 2 + i, ue_sessions[i].affinity_core);
		}
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

/**
 * GTP-U 封装（不含外层 IP/UDP，仅 GTP 部分）
 * @src_data: 源数据（IP 包）
 * @src_len: 源数据长度
 * @teid: 隧道端点标识符
 * @sequence: 序列号（用于报文重排序）
 * @with_seq: 是否包含序列号字段
 * @buffer: 输出缓冲区
 * @return: 封装后的 GTP 包长度
 */
static inline int gtp_encap(const uint8_t *src_data, int src_len,
                            uint32_t teid, uint16_t sequence,
                            int with_seq, uint8_t *buffer) {
	struct gtp_header *gtp_hdr = (struct gtp_header *)buffer;
	int gtp_hdr_size = GTP_HEADER_SIZE_MIN;
	int gtp_payload_len = src_len;

	/* 设置 GTP-U v1 基础头部 (RFC 3152) */
	if (with_seq) {
		/* 启用序列号标志 */
		gtp_hdr->flags = GTP_VERSION_1 | GTP_FLAG_SEQUENCE; /* v1, PT=1, S=1 */
		gtp_hdr_size = GTP_HEADER_SIZE_EXT;
		gtp_payload_len = src_len + 4; /* 包括序列号和 N-PDU */

		/* 设置序列号 */
		struct gtp_header_ext *gtp_ext = (struct gtp_header_ext *)buffer;
		gtp_ext->sequence = htons(sequence);
		gtp_ext->n_pdu_number = 0;      /* N-PDU 号为 0 */
		gtp_ext->next_ext_hdr = 0;      /* 无更多扩展头 */
	} else {
		/* 基础头部，无扩展 */
		gtp_hdr->flags = GTP_VERSION_1;  /* v1, PT=1, E/S/PN=0 */
		gtp_payload_len = src_len;
	}

	/* 设置消息类型和长度 */
	gtp_hdr->msg_type = GTP_TPDU_TYPE;           /* 消息类型: T-PDU (255) */
	gtp_hdr->length = htons(gtp_payload_len);    /* 网络字节序: 负载长度 */
	gtp_hdr->teid = htonl(teid);                 /* 网络字节序: TEID */

	/* 复制负载数据到 GTP 包后面 */
	memcpy(buffer + gtp_hdr_size, src_data, src_len);

	return gtp_hdr_size + src_len;
}

/**
 * GTP-U 解封装
 * @gtp_data: GTP 包数据
 * @gtp_len: GTP 包总长度
 * @teid: 输出 TEID
 * @sequence: 输出序列号
 * @ip_data: 输出内层 IP 包指针
 * @ip_len: 输出内层 IP 包长度
 * @return: 0 成功，-1 失败
 */
static inline int gtp_decap(const uint8_t *gtp_data, int gtp_len,
                            uint32_t *teid, uint16_t *sequence,
                            uint8_t **ip_data, int *ip_len) {
	if (gtp_len < GTP_HEADER_SIZE_MIN)
		return -1;

	struct gtp_header *gtp_hdr = (struct gtp_header *)gtp_data;
	int gtp_hdr_size = GTP_HEADER_SIZE_MIN;
	int payload_len = ntohs(gtp_hdr->length);

	/* 校验 GTP 版本 (应为 v1) 和 PT (应为 1，表示 GTP) */
	/* Flags: [版本(3bit)=001][PT(1bit)=1][预留(1bit)] ... */
	if ((gtp_hdr->flags & GTP_VERSION_MASK) != GTP_V1_VALID) {
		RTE_LOG(DEBUG, GENERAL, "[GTP] Invalid version or PT: flags=0x%02x\n",
		        gtp_hdr->flags);
		return -1;
	}

	/* 校验消息类型应为 T-PDU (255) */
	if (gtp_hdr->msg_type != GTP_TPDU_TYPE) {
		RTE_LOG(DEBUG, GENERAL, "[GTP] Invalid message type: %u\n",
		        gtp_hdr->msg_type);
		return -1;
	}

	/* 检查是否有扩展头或序列号 */
	if ((gtp_hdr->flags & (GTP_FLAG_EXTENSION | GTP_FLAG_SEQUENCE | GTP_FLAG_NPDU)) != 0) {
		/* 有扩展字段 */
		if (gtp_len < GTP_HEADER_SIZE_EXT) {
			RTE_LOG(DEBUG, GENERAL, "[GTP] Buffer too small for extended header\n");
			return -1;
		}

		struct gtp_header_ext *gtp_ext = (struct gtp_header_ext *)gtp_data;

		/* 提取序列号 */
		if (gtp_hdr->flags & GTP_FLAG_SEQUENCE) {
			*sequence = ntohs(gtp_ext->sequence);
		} else {
			*sequence = 0;
		}

		gtp_hdr_size = GTP_HEADER_SIZE_EXT;
		payload_len -= 4; /* 减去序列号和 N-PDU 的 4 字节 */

		/* 如果有扩展头标志，需要跳过扩展头 (当前不支持) */
		if (gtp_hdr->flags & GTP_FLAG_EXTENSION) {
			RTE_LOG(DEBUG, GENERAL, "[GTP] Extension headers not fully supported yet\n");
			/* 实际应用中应该解析扩展头链表 */
		}
	} else {
		*sequence = 0;
	}

	/* 提取 TEID */
	*teid = ntohl(gtp_hdr->teid);

	/* 指向负载数据 */
	*ip_data = (uint8_t *)gtp_data + gtp_hdr_size;

	/* 校验长度有效性 */
	if (payload_len <= 0 || gtp_hdr_size + payload_len > gtp_len) {
		RTE_LOG(DEBUG, GENERAL, "[GTP] Invalid payload length: %d (hdr_size: %d, total: %d)\n",
		        payload_len, gtp_hdr_size, gtp_len);
		return -1;
	}

	*ip_len = payload_len;

	return 0;
}

/* ============= IP 包解析 ============= */

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

/* ============= 数据包处理函数 ============= */

/*
 * 处理下行数据包 (DN -> UE)
 * 完整的隧道格式: [外层IP] -> [外层UDP] -> [GTP-U] -> [内层IP]
 * mbuf: DPDK 内存缓冲区（接收的内层 IP 包）
 * 核心亲和性: 仅处理绑定到当前核心的会话
 */
static inline void process_downlink_packet(struct rte_mbuf *mbuf) {
	uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
	uint16_t pkt_len = rte_pktmbuf_data_len(mbuf);

	uint32_t src_ip, dst_ip;
	ue_session_t *session;
	unsigned int current_core = rte_lcore_id();
	struct rte_mbuf *tunneled_mbuf;
	uint8_t *tunnel_data;
	struct iphdr *outer_ip;
	struct udphdr *outer_udp;
	uint8_t *gtp_data;
	int gtp_len;
	int total_len;

	/* 解析内层 IP 包头 */
	if (parse_ip_packet(pkt_data, pkt_len, &src_ip, &dst_ip) < 0) {
		RTE_LOG(DEBUG, GENERAL, "[DL] Invalid IP packet\n");
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 根据目的 IP 查找会话 */
	session = lookup_session_by_destip(dst_ip);
	if (!session) {
		RTE_LOG(DEBUG, GENERAL, "[DL] No session for IP 0x%x\n", dst_ip);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* ✅ 核心亲和性检查: 验证会话是否绑定到当前核心 */
	if (session->affinity_enabled && session->affinity_core != current_core) {
		RTE_LOG(DEBUG, GENERAL,
		        "[DL-AFFINITY] Packet for UE (IP:0x%x) belongs to core %u, current core is %u - SKIPPED\n",
		        dst_ip, session->affinity_core, current_core);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 创建新的 mbuf 用于隧道包 */
	tunneled_mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!tunneled_mbuf) {
		RTE_LOG(ERR, GENERAL, "[DL] Failed to allocate tunnel mbuf\n");
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 获取隧道包缓冲区 */
	tunnel_data = rte_pktmbuf_mtod(tunneled_mbuf, uint8_t *);

	/* 1. 构造外层 IP 头 (20 字节) */
	outer_ip = (struct iphdr *)tunnel_data;
	outer_ip->version = 4;
	outer_ip->ihl = 5;                      /* IP 头长度 = 5 * 4 = 20 字节 */
	outer_ip->tos = ((struct iphdr *)pkt_data)->tos;  /* 复制内层 DSCP 值 */
	outer_ip->ttl = 64;                     /* TTL 值 */
	outer_ip->protocol = IPPROTO_UDP;       /* 协议: UDP */
	outer_ip->saddr = upf_local_ip;         /* 源 IP: UPF IP */
	outer_ip->daddr = session->gnb_ip;      /* 目标 IP: gNodeB IP */

	/* 2. 构造外层 UDP 头 (8 字节) */
	outer_udp = (struct udphdr *)(tunnel_data + sizeof(struct iphdr));
	outer_udp->source = htons(2153);        /* 源端口: UPF 使用 2153 */
	outer_udp->dest = htons(GTP_PORT);      /* 目标端口: 2152 (GTP-U) */
	outer_udp->len = 0;                     /* 后续计算 */
	outer_udp->check = 0;                   /* 不计算校验和（可选） */

	/* 3. 构造 GTP-U 头 */
	gtp_data = tunnel_data + sizeof(struct iphdr) + sizeof(struct udphdr);

	/* 更新序列号 */
	session->dl_sequence++;

	/* GTP 封装（包括序列号） */
	gtp_len = gtp_encap(pkt_data, pkt_len, session->teid_downlink,
	                     session->dl_sequence, 1, gtp_data);

	/* 计算总长度 */
	total_len = sizeof(struct iphdr) + sizeof(struct udphdr) + gtp_len;

	/* 更新 IP 头字段 */
	outer_ip->tot_len = htons(total_len);
	outer_ip->id = htons(1);                /* 分片 ID */
	outer_ip->frag_off = 0;                 /* 不分片 */
	outer_ip->check = 0;                    /* 校验和由硬件计算 */

	/* 更新 UDP 头字段 */
	outer_udp->len = htons(sizeof(struct udphdr) + gtp_len);

	/* 更新 mbuf 长度 */
	rte_pktmbuf_data_len(tunneled_mbuf) = total_len;
	rte_pktmbuf_pkt_len(tunneled_mbuf) = total_len;

	/* 更新会话统计 */
	session->packets_tx++;
	session->bytes_tx += total_len;

	/* 从 RAN 侧网卡发送 */
	uint16_t sent = rte_eth_tx_burst(port_ran, 0, &tunneled_mbuf, 1);
	if (sent > 0) {
		RTE_LOG(DEBUG, GENERAL,
		        "[DL] Sent GTP tunnel: TEID=0x%x, seq=%u, len=%d to gNodeB %x (core %u)\n",
		        session->teid_downlink, session->dl_sequence, total_len, session->gnb_ip, current_core);
	} else {
		rte_pktmbuf_free(tunneled_mbuf);
	}

	rte_pktmbuf_free(mbuf);
}

/*
 * 处理上行数据包 (UE -> DN)
 * 完整的隧道格式: [外层IP] -> [外层UDP] -> [GTP-U] -> [内层IP]
 * mbuf: DPDK 内存缓冲区（接收的隧道包）
 * 核心亲和性: 仅处理绑定到当前核心的会话
 */
static inline void process_uplink_packet(struct rte_mbuf *mbuf) {
	uint8_t *pkt_data = rte_pktmbuf_mtod(mbuf, uint8_t *);
	uint16_t pkt_len = rte_pktmbuf_data_len(mbuf);
	unsigned int current_core = rte_lcore_id();

	struct iphdr *outer_ip;
	struct udphdr *outer_udp;
	uint8_t *gtp_pkt;
	int gtp_len;

	uint32_t teid;
	uint16_t sequence;
	uint8_t *ip_data;
	int ip_len;
	ue_session_t *session;
	struct rte_mbuf *ip_mbuf;
	uint8_t *out_data;

	/* 最小长度校验: IP(20) + UDP(8) + GTP(8) = 36 字节 */
	if (pkt_len < (int)(sizeof(struct iphdr) + sizeof(struct udphdr) + GTP_HEADER_SIZE_MIN)) {
		RTE_LOG(DEBUG, GENERAL, "[UL] Packet too short: %d bytes\n", pkt_len);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 1. 解析外层 IP 头 */
	outer_ip = (struct iphdr *)pkt_data;

	/* IP 版本检查 */
	if (outer_ip->version != 4) {
		RTE_LOG(DEBUG, GENERAL, "[UL] Invalid IP version: %u\n", outer_ip->version);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 协议校验（应为 UDP） */
	if (outer_ip->protocol != IPPROTO_UDP) {
		RTE_LOG(DEBUG, GENERAL, "[UL] Invalid protocol: %u (expected UDP)\n", outer_ip->protocol);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 2. 解析外层 UDP 头 */
	outer_udp = (struct udphdr *)(pkt_data + sizeof(struct iphdr));

	/* UDP 目标端口应为 2152 (GTP-U) */
	if (ntohs(outer_udp->dest) != GTP_PORT) {
		RTE_LOG(DEBUG, GENERAL, "[UL] Invalid UDP port: %u (expected %u)\n",
		        ntohs(outer_udp->dest), GTP_PORT);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 3. 提取 GTP-U 包 */
	gtp_pkt = pkt_data + sizeof(struct iphdr) + sizeof(struct udphdr);
	gtp_len = pkt_len - sizeof(struct iphdr) - sizeof(struct udphdr);

	/* GTP-U 解封装 */
	if (gtp_decap(gtp_pkt, gtp_len, &teid, &sequence, &ip_data, &ip_len) < 0) {
		RTE_LOG(DEBUG, GENERAL, "[UL] Failed to decapsulate GTP packet\n");
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 4. 查找会话 */
	session = lookup_session_by_teid_ul(teid);
	if (!session) {
		RTE_LOG(DEBUG, GENERAL, "[UL] No session for TEID 0x%x\n", teid);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* ✅ 核心亲和性检查: 验证会话是否绑定到当前核心 */
	if (session->affinity_enabled && session->affinity_core != current_core) {
		RTE_LOG(DEBUG, GENERAL,
		        "[UL-AFFINITY] Packet for TEID(0x%x) belongs to core %u, current core is %u - SKIPPED\n",
		        teid, session->affinity_core, current_core);
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 5. 源地址校验（来自已授权的 gNodeB） */
	if (outer_ip->saddr != session->gnb_ip) {
		RTE_LOG(NOTICE, GENERAL, "[UL] Source address mismatch: got 0x%x, expected 0x%x\n",
		        outer_ip->saddr, session->gnb_ip);
		/* 根据策略可以丢弃或记录，这里允许通过但记录日志 */
	}

	/* 创建新的 mbuf 用于内层 IP 包 */
	ip_mbuf = rte_pktmbuf_alloc(mbuf_pool);
	if (!ip_mbuf) {
		RTE_LOG(ERR, GENERAL, "[UL] Failed to allocate IP mbuf\n");
		rte_pktmbuf_free(mbuf);
		return;
	}

	/* 复制内层 IP 包数据 */
	out_data = rte_pktmbuf_mtod(ip_mbuf, uint8_t *);
	memcpy(out_data, ip_data, ip_len);
	rte_pktmbuf_data_len(ip_mbuf) = ip_len;
	rte_pktmbuf_pkt_len(ip_mbuf) = ip_len;

	/* 更新统计 */
	session->packets_rx++;
	session->bytes_rx += ip_len;
	session->ul_sequence = sequence;  /* 记录序列号 */

	/* 从 DN 侧网卡发送 */
	uint16_t sent = rte_eth_tx_burst(port_dn, 0, &ip_mbuf, 1);
	if (sent > 0) {
		RTE_LOG(DEBUG, GENERAL,
		        "[UL] Decapsulated GTP: TEID=0x%x, seq=%u, len=%d from gNodeB (core %u)\n",
		        teid, sequence, ip_len, current_core);
	} else {
		rte_pktmbuf_free(ip_mbuf);
	}

	rte_pktmbuf_free(mbuf);
}

/* ============= DPDK 初始化 ============= */

static int init_dpdk_ports(void) {
	uint16_t portid;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf port_conf = {0};

	RTE_ETH_FOREACH_DEV(portid) {
		nb_ports++;
	}

	if (nb_ports < 2) {
		fprintf(stderr, "Error: need at least 2 ports for DN and RAN\n");
		return -1;
	}

	RTE_ETH_FOREACH_DEV(portid) {
		rte_eth_dev_info_get(portid, &dev_info);
		printf("[INIT] Port %u: %s\n", portid, dev_info.device->name);

		/* 配置端口 */
		if (rte_eth_dev_configure(portid, 1, 1, &port_conf) < 0) {
			fprintf(stderr, "Cannot configure port %u\n", portid);
			return -1;
		}

		/* 配置 RX 队列 */
		if (rte_eth_rx_queue_setup(portid, 0, RX_RING_SIZE,
		                             rte_eth_dev_socket_id(portid),
		                             NULL, mbuf_pool) < 0) {
			fprintf(stderr, "Cannot setup RX queue for port %u\n", portid);
			return -1;
		}

		/* 配置 TX 队列 */
		if (rte_eth_tx_queue_setup(portid, 0, TX_RING_SIZE,
		                             rte_eth_dev_socket_id(portid),
		                             NULL) < 0) {
			fprintf(stderr, "Cannot setup TX queue for port %u\n", portid);
			return -1;
		}

		/* 启动端口 */
		if (rte_eth_dev_start(portid) < 0) {
			fprintf(stderr, "Cannot start port %u\n", portid);
			return -1;
		}

		printf("[INIT] Port %u started\n", portid);
	}

	return 0;
}

/* ============= 数据包处理任务 ============= */

/*
 * 下行数据包处理任务 (DN -> UE)
 * 部署在核心 2, 3 上
 */
static int lcore_downlink_task(void *arg) {
	struct rte_mbuf *bufs[BURST_SIZE];
	uint16_t nb_rx;
	uint64_t prev_tsc = 0, cur_tsc;
	uint64_t hz = rte_get_tsc_hz();
	(void)arg;  /* 避免未使用变量警告 */

	printf("[LCORE-DL] Downlink task started on core %u (DN port processing)\n",
	       rte_lcore_id());

	while (1) {
		cur_tsc = rte_rdtsc();

		/* 从 DN 侧接收数据包 */
		nb_rx = rte_eth_rx_burst(port_dn, 0, bufs, BURST_SIZE);
		if (nb_rx > 0) {
			RTE_LOG(DEBUG, GENERAL, "[DL-RX] Received %u packets from DN on core %u\n",
			        nb_rx, rte_lcore_id());
			for (uint16_t i = 0; i < nb_rx; i++) {
				process_downlink_packet(bufs[i]);
			}
		}

		/* 每秒打印下行统计信息 */
		if (cur_tsc - prev_tsc > hz) {
			prev_tsc = cur_tsc;
			printf("[DL-STATS] Core %u: ", rte_lcore_id());
			for (int i = 0; i < session_count; i++) {
				printf("UE%d: TX=%lu pkt ", i + 1, ue_sessions[i].packets_tx);
			}
			printf("\n");
		}
	}

	return 0;
}

/*
 * 上行数据包处理任务 (UE -> DN)
 * 部署在核心 4, 5 上
 */
static int lcore_uplink_task(void *arg) {
	struct rte_mbuf *bufs[BURST_SIZE];
	uint16_t nb_rx;
	uint64_t prev_tsc = 0, cur_tsc;
	uint64_t hz = rte_get_tsc_hz();
	(void)arg;  /* 避免未使用变量警告 */

	printf("[LCORE-UL] Uplink task started on core %u (RAN port processing)\n",
	       rte_lcore_id());

	while (1) {
		cur_tsc = rte_rdtsc();

		/* 从 RAN 侧接收数据包 */
		nb_rx = rte_eth_rx_burst(port_ran, 0, bufs, BURST_SIZE);
		if (nb_rx > 0) {
			RTE_LOG(DEBUG, GENERAL, "[UL-RX] Received %u packets from RAN on core %u\n",
			        nb_rx, rte_lcore_id());
			for (uint16_t i = 0; i < nb_rx; i++) {
				process_uplink_packet(bufs[i]);
			}
		}

		/* 每秒打印上行统计信息 */
		if (cur_tsc - prev_tsc > hz) {
			prev_tsc = cur_tsc;
			printf("[UL-STATS] Core %u: ", rte_lcore_id());
			for (int i = 0; i < session_count; i++) {
				printf("UE%d: RX=%lu pkt ", i + 1, ue_sessions[i].packets_rx);
			}
			printf("\n");
		}
	}

	return 0;
}

/* ============= 主函数 ============= */

int main(int argc, char **argv) {
	int ret;

	/* DPDK EAL 初始化 */
	ret = rte_eal_init(argc, argv);
	if (ret < 0) {
		rte_exit(EXIT_FAILURE, "Cannot initialize EAL\n");
	}

	/* 注册日志类型 */
	RTE_LOGTYPE_GENERAL = rte_log_register("general");
	if (RTE_LOGTYPE_GENERAL < 0) {
		RTE_LOGTYPE_GENERAL = 0;  /* 使用默认值 */
	}

	/* 创建内存池 */
	mbuf_pool = rte_pktmbuf_pool_create("MBUF_POOL", NUM_MBUFS,
	                                     MBUF_CACHE_SIZE, 0,
	                                     RTE_MBUF_DEFAULT_BUF_SIZE,
	                                     rte_socket_id());
	if (mbuf_pool == NULL) {
		rte_exit(EXIT_FAILURE, "Cannot create mbuf pool\n");
	}

	/* 初始化网络配置（UPF IP 地址） */
	init_network_config();

	/* 初始化网卡 */
	if (init_dpdk_ports() < 0) {
		rte_exit(EXIT_FAILURE, "Cannot initialize ports\n");
	}

	/* 初始化 UE 会话表 */
	init_ue_sessions();

	printf("\n=== 5G UPF with DPDK ===\n");
	printf("DN port: %u\n", port_dn);
	printf("RAN port: %u\n", port_ran);
	printf("\nTask deployment configuration:\n");
	printf("  Downlink task  (DN processing): cores 2, 3\n");
	printf("  Uplink task    (RAN processing): cores 4, 5\n");
	printf("\nStarting packet forwarding...\n");

	/* 任务部署策略：使用rte_eal_remote_launch部署到指定核心 */
	unsigned int lcore_id;
	unsigned int dl_count = 0, ul_count = 0;

	/* 遍历所有活跃的从核 */
	RTE_LCORE_FOREACH_WORKER(lcore_id) {
		/* 核心 2, 3 运行下行任务 */
		if (lcore_id == 2 || lcore_id == 3) {
			printf("[DEPLOY] Launching downlink task on core %u\n", lcore_id);
			register_downlink_core(lcore_id);  /* ✅ 注册核心用于会话亲和性 */
			rte_eal_remote_launch(lcore_downlink_task, NULL, lcore_id);
			dl_count++;
		}
		/* 核心 4, 5 运行上行任务 */
		else if (lcore_id == 4 || lcore_id == 5) {
			printf("[DEPLOY] Launching uplink task on core %u\n", lcore_id);
			register_uplink_core(lcore_id);    /* ✅ 注册核心用于会话亲和性 */
			rte_eal_remote_launch(lcore_uplink_task, NULL, lcore_id);
			ul_count++;
		}
	}

	printf("\n[DEPLOY] Summary:\n");
	printf("  Downlink tasks deployed: %u\n", dl_count);
	printf("  Uplink tasks deployed: %u\n", ul_count);
	printf("  Total downlink cores: %d\n", dl_core_count);
	printf("  Total uplink cores: %d\n", ul_core_count);

	/* 等待所有核心任务完成 */
	rte_eal_mp_wait_lcore();

	return 0;
}
