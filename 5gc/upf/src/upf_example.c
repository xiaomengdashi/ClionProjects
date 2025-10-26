/*
 * 5G UPF 简化示例代码
 * 功能: DN侧数据包接收 -> GTP-U处理 -> IP匹配TEID -> 基站转发
 * 说明: 不包含QoS处理，展示核心数据平面逻辑
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <netinet/ip.h>

/* ============= GTP-U 报文头定义 ============= */

#define GTP_PORT 2152

/* GTP-U 头部 (8字节基本头) */
struct gtp_header {
	uint8_t flags;           /* 版本、PT、是否有可选字段等 */
	uint8_t msg_type;        /* 消息类型 (T-PDU=255) */
	uint16_t length;         /* 负载长度 */
	uint32_t teid;           /* Tunnel Endpoint ID */
	/* 可选: SN、PN 等 */
} __attribute__((packed));

#define GTP_V1_ENABLED    0x40
#define GTP_PT_GTP        0x10
#define GTP_TPDU_TYPE     255

/* ============= 用户会话信息 ============= */

/* UE 会话表项：映射 UE IP -> TEID 和基站信息 */
typedef struct {
	uint32_t ue_ip;              /* UE 分配的 IP 地址 */
	uint32_t teid_downlink;      /* 下行 TEID（UE方向）*/
	uint32_t teid_uplink;        /* 上行 TEID（RAN方向）*/
	uint32_t gnb_ip;             /* gNodeB IP 地址 */
	uint16_t gnb_port;           /* gNodeB 端口 */
	uint32_t dn_ip;              /* DN IP 地址 */
	char status;                 /* 会话状态 */
} ue_session_t;

/* UE 会话表 (简化版，实际应使用哈希表) */
#define MAX_SESSIONS 1000
static ue_session_t ue_sessions[MAX_SESSIONS];
static int session_count = 0;

/* ============= 初始化 UE 会话表 ============= */

void init_ue_sessions(void) {
	/* 示例会话配置 */

	/* UE 1: IP=10.0.0.2, 绑定基站 192.168.1.100:2152 */
	ue_sessions[0].ue_ip = inet_addr("10.0.0.2");
	ue_sessions[0].teid_downlink = 0x12345678;
	ue_sessions[0].teid_uplink = 0x87654321;
	ue_sessions[0].gnb_ip = inet_addr("192.168.1.100");
	ue_sessions[0].gnb_port = 2152;
	ue_sessions[0].dn_ip = inet_addr("8.8.8.8");
	ue_sessions[0].status = 'A';

	/* UE 2: IP=10.0.0.3 */
	ue_sessions[1].ue_ip = inet_addr("10.0.0.3");
	ue_sessions[1].teid_downlink = 0x11111111;
	ue_sessions[1].teid_uplink = 0x22222222;
	ue_sessions[1].gnb_ip = inet_addr("192.168.1.101");
	ue_sessions[1].gnb_port = 2152;
	ue_sessions[1].dn_ip = inet_addr("8.8.8.8");
	ue_sessions[1].status = 'A';

	session_count = 2;

	printf("[INIT] UE Session table initialized with %d sessions\n", session_count);
}

/* ============= IP 查表，根据目的 IP 匹配 TEID ============= */

ue_session_t* lookup_session_by_destip(uint32_t dest_ip) {
	/* 简化实现: 线性查表
	 * 实际应使用 LPM (最长前缀匹配) 或哈希表
	 */
	for (int i = 0; i < session_count; i++) {
		if (ue_sessions[i].ue_ip == dest_ip && ue_sessions[i].status == 'A') {
			return &ue_sessions[i];
		}
	}
	return NULL;
}

/* ============= GTP-U 封装函数 ============= */

/*
 * 对 IP 包进行 GTP-U 封装
 * 参数:
 *   src_data: 原始 IP 包数据
 *   src_len:  原始包长度
 *   teid:     要封装的 TEID
 *   buffer:   输出缓冲区
 * 返回:
 *   GTP-U 封装后的总长度
 */
int gtp_encap(const uint8_t *src_data, int src_len, uint32_t teid, uint8_t *buffer) {
	struct gtp_header *gtp_hdr = (struct gtp_header *)buffer;

	/* 设置 GTP 头部 */
	gtp_hdr->flags = GTP_V1_ENABLED | GTP_PT_GTP;
	gtp_hdr->msg_type = GTP_TPDU_TYPE;
	gtp_hdr->length = htons(src_len);  /* GTP 负载长度 */
	gtp_hdr->teid = htonl(teid);

	/* 复制 IP 包到 GTP 负载 */
	memcpy(buffer + sizeof(struct gtp_header), src_data, src_len);

	return sizeof(struct gtp_header) + src_len;
}

/* ============= GTP-U 解封装函数 ============= */

/*
 * 对 GTP-U 包进行解封装
 * 参数:
 *   gtp_data:  GTP-U 包数据
 *   gtp_len:   GTP-U 包长度
 *   teid:      输出参数，提取的 TEID
 *   ip_data:   输出参数，指向 IP 包数据
 *   ip_len:    输出参数，IP 包长度
 * 返回:
 *   0 成功, -1 失败
 */
int gtp_decap(const uint8_t *gtp_data, int gtp_len, uint32_t *teid,
              const uint8_t **ip_data, int *ip_len) {

	if (gtp_len < sizeof(struct gtp_header)) {
		printf("[GTP] Packet too short for GTP header\n");
		return -1;
	}

	struct gtp_header *gtp_hdr = (struct gtp_header *)gtp_data;

	/* 验证 GTP 版本和类型 */
	if ((gtp_hdr->flags & 0xE0) != 0x20) {  /* 版本应为1 (0010 0000) */
		printf("[GTP] Invalid GTP version\n");
		return -1;
	}

	/* 提取 TEID */
	*teid = ntohl(gtp_hdr->teid);

	/* 提取负载长度 */
	int payload_len = ntohs(gtp_hdr->length);

	/* 指向 IP 包数据 */
	*ip_data = gtp_data + sizeof(struct gtp_header);
	*ip_len = payload_len;

	if (*ip_len <= 0 || *ip_len > gtp_len - sizeof(struct gtp_header)) {
		printf("[GTP] Invalid GTP payload length: %d\n", *ip_len);
		return -1;
	}

	return 0;
}

/* ============= IP 包分析 ============= */

/*
 * 解析 IP 包头，提取源和目的地址
 */
int parse_ip_packet(const uint8_t *ip_data, int ip_len,
                    uint32_t *src_ip, uint32_t *dst_ip) {

	if (ip_len < sizeof(struct iphdr)) {
		printf("[IP] Packet too short for IP header\n");
		return -1;
	}

	struct iphdr *ip_hdr = (struct iphdr *)ip_data;

	/* 验证 IP 版本 */
	if (ip_hdr->version != 4) {
		printf("[IP] Non-IPv4 packet (version=%d)\n", ip_hdr->version);
		return -1;
	}

	*src_ip = ip_hdr->saddr;
	*dst_ip = ip_hdr->daddr;

	return 0;
}

/* ============= 数据包处理核心函数 ============= */

/*
 * 处理从 DN 侧接收的数据包
 * 流程:
 *   1. 接收原始 IP 包
 *   2. 解析 IP 头，获取目的 IP
 *   3. 根据目的 IP 查表获取 UE 会话和 TEID
 *   4. 用下行 TEID 进行 GTP-U 封装
 *   5. 将 GTP-U 包发送给基站
 */
void process_downlink_packet(const uint8_t *ip_packet, int packet_len) {
	uint32_t src_ip, dst_ip;
	ue_session_t *session;
	uint8_t gtp_buffer[2048];
	int gtp_len;

	/* 步骤 1: 解析 IP 包头 */
	if (parse_ip_packet(ip_packet, packet_len, &src_ip, &dst_ip) < 0) {
		printf("[DL] Failed to parse IP packet\n");
		return;
	}

	/* 打印分析结果 */
	char src_str[32], dst_str[32];
	inet_ntop(AF_INET, &src_ip, src_str, sizeof(src_str));
	inet_ntop(AF_INET, &dst_ip, dst_str, sizeof(dst_str));
	printf("[DL] IP Packet: %s -> %s (len=%d)\n", src_str, dst_str, packet_len);

	/* 步骤 2: 根据目的 IP 查表获取 UE 会话 */
	session = lookup_session_by_destip(dst_ip);
	if (!session) {
		printf("[DL] No session found for destination IP %s\n", dst_str);
		return;
	}

	printf("[DL] Session found: TEID=0x%08x, gNodeB=%s:%u\n",
	       session->teid_downlink,
	       inet_ntoa(*(struct in_addr *)&session->gnb_ip),
	       session->gnb_port);

	/* 步骤 3: GTP-U 封装 */
	gtp_len = gtp_encap(ip_packet, packet_len, session->teid_downlink, gtp_buffer);
	printf("[GTP] GTP-U encapsulated: TEID=0x%08x, total_len=%d\n",
	       session->teid_downlink, gtp_len);

	/* 步骤 4: 模拟发送到基站 */
	printf("[TX] Sending to gNodeB %s:%u (GTP packet size=%d bytes)\n",
	       inet_ntoa(*(struct in_addr *)&session->gnb_ip),
	       session->gnb_port, gtp_len);
	printf("[TX] Packet hex dump (first 64 bytes):\n");
	for (int i = 0; i < gtp_len && i < 64; i++) {
		printf("%02x ", gtp_buffer[i]);
		if ((i + 1) % 16 == 0) printf("\n");
	}
	printf("\n");
}

/* ============= 处理从基站接收的数据包 ============= */

/*
 * 处理从基站(RAN)侧接收的数据包
 * 流程:
 *   1. 接收 GTP-U 包
 *   2. 解封装 GTP-U，提取 IP 包和 TEID
 *   3. 验证 TEID 是否有效
 *   4. 将原始 IP 包发送给 DN
 */
void process_uplink_packet(const uint8_t *gtp_packet, int packet_len) {
	uint32_t teid;
	const uint8_t *ip_data;
	int ip_len;
	ue_session_t *session;
	uint32_t src_ip, dst_ip;

	/* 步骤 1: 解封装 GTP-U */
	if (gtp_decap(gtp_packet, packet_len, &teid, &ip_data, &ip_len) < 0) {
		printf("[UL] Failed to decapsulate GTP-U packet\n");
		return;
	}

	printf("[UL] GTP-U decapsulated: TEID=0x%08x, payload_len=%d\n", teid, ip_len);

	/* 步骤 2: 解析 IP 包头 */
	if (parse_ip_packet(ip_data, ip_len, &src_ip, &dst_ip) < 0) {
		printf("[UL] Failed to parse IP packet\n");
		return;
	}

	char src_str[32], dst_str[32];
	inet_ntop(AF_INET, &src_ip, src_str, sizeof(src_str));
	inet_ntop(AF_INET, &dst_ip, dst_str, sizeof(dst_str));
	printf("[UL] IP Packet: %s -> %s (len=%d)\n", src_str, dst_str, ip_len);

	/* 步骤 3: 验证 TEID 并查找会话 */
	for (int i = 0; i < session_count; i++) {
		if (ue_sessions[i].teid_uplink == teid && ue_sessions[i].status == 'A') {
			session = &ue_sessions[i];
			printf("[UL] Session found: UE_IP=%s\n",
			       inet_ntoa(*(struct in_addr *)&session->ue_ip));

			/* 步骤 4: 模拟发送到 DN */
			printf("[TX] Sending to DN %s (packet size=%d bytes)\n",
			       inet_ntoa(*(struct in_addr *)&session->dn_ip), ip_len);
			return;
		}
	}

	printf("[UL] No session found for TEID 0x%08x\n", teid);
}

/* ============= 主程序和测试 ============= */

int main(int argc, char *argv[]) {
	printf("\n=== 5G UPF Example (DN -> UE -> gNodeB) ===\n\n");

	/* 初始化 UE 会话表 */
	init_ue_sessions();
	printf("\n");

	/* ========== 测试 1: 下行数据包处理 (DN -> UE) ========== */
	printf("========== Test 1: Downlink Packet (DN -> UE) ==========\n\n");

	/* 构造一个简单的 IP 包
	 * 源: 8.8.8.8 (DN)
	 * 目的: 10.0.0.2 (UE1)
	 */
	uint8_t test_ip_packet[84];
	struct iphdr *ip_hdr = (struct iphdr *)test_ip_packet;

	/* 填充 IP 头 */
	ip_hdr->version = 4;
	ip_hdr->ihl = 5;                           /* 头长度 20 字节 */
	ip_hdr->tot_len = htons(84);                /* 总长度 */
	ip_hdr->id = htons(1234);
	ip_hdr->frag_off = 0;
	ip_hdr->ttl = 64;
	ip_hdr->protocol = IPPROTO_ICMP;            /* ICMP */
	ip_hdr->saddr = inet_addr("8.8.8.8");
	ip_hdr->daddr = inet_addr("10.0.0.2");
	ip_hdr->check = 0;                          /* 简化: 不计算校验和 */

	/* 填充一些数据 */
	memset(test_ip_packet + sizeof(struct iphdr), 'X', 84 - sizeof(struct iphdr));

	process_downlink_packet(test_ip_packet, 84);
	printf("\n");

	/* ========== 测试 2: 上行数据包处理 (UE -> DN) ========== */
	printf("========== Test 2: Uplink Packet (UE -> DN) ==========\n\n");

	/* 构造一个 GTP-U 包
	 * TEID: 0x87654321 (UE1 上行)
	 * 包含的 IP 数据: UE 回复给 DN
	 */
	uint8_t test_gtp_packet[128];

	/* 先构造 IP 包 (UE -> DN) */
	struct iphdr *ip_hdr2 = (struct iphdr *)(test_gtp_packet + sizeof(struct gtp_header));
	ip_hdr2->version = 4;
	ip_hdr2->ihl = 5;
	ip_hdr2->tot_len = htons(64);
	ip_hdr2->id = htons(5678);
	ip_hdr2->frag_off = 0;
	ip_hdr2->ttl = 64;
	ip_hdr2->protocol = IPPROTO_ICMP;
	ip_hdr2->saddr = inet_addr("10.0.0.2");
	ip_hdr2->daddr = inet_addr("8.8.8.8");
	ip_hdr2->check = 0;

	memset((uint8_t *)ip_hdr2 + sizeof(struct iphdr), 'Y', 64 - sizeof(struct iphdr));

	/* 进行 GTP-U 封装 */
	int gtp_len = gtp_encap((uint8_t *)ip_hdr2, 64, 0x87654321, test_gtp_packet);

	/* 处理这个 GTP-U 包 */
	process_uplink_packet(test_gtp_packet, gtp_len);
	printf("\n");

	/* ========== 测试 3: 另一个 UE 的下行数据包 ========== */
	printf("========== Test 3: Downlink Packet for UE2 ==========\n\n");

	struct iphdr *ip_hdr3 = (struct iphdr *)test_ip_packet;
	ip_hdr3->saddr = inet_addr("8.8.8.8");
	ip_hdr3->daddr = inet_addr("10.0.0.3");  /* UE2 */

	process_downlink_packet(test_ip_packet, 84);
	printf("\n");

	printf("=== Test Completed ===\n\n");

	return 0;
}
