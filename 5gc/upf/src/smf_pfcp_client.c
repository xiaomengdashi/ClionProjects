/*
 * SMF PFCP 客户端程序
 *
 * 功能：模拟 SMF (Session Management Function)
 * 向 UPF 发送 PFCP 会话管理消息
 *
 * PFCP 交互流程：
 * 1. SMF → UPF: PFCP Session Establishment Request (msg type 50)
 * 2. UPF → SMF: PFCP Session Establishment Response (msg type 51)
 * 3. SMF → UPF: PFCP Session Modification Request (msg type 52)
 * 4. UPF → SMF: PFCP Session Modification Response (msg type 53)
 * 5. SMF → UPF: PFCP Session Deletion Request (msg type 54)
 * 6. UPF → SMF: PFCP Session Deletion Response (msg type 55)
 *
 * 编译：
 * gcc -o smf_pfcp_client smf_pfcp_client.c -lm
 *
 * 运行：
 * ./smf_pfcp_client
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <time.h>
#include <math.h>

/* ============= PFCP 协议常量定义 ============= */
#define PFCP_SERVER_IP "127.0.0.1"
#define PFCP_SERVER_PORT 8805
#define PFCP_VERSION 1

/* PFCP 消息类型 */
#define PFCP_SESSION_EST_REQ   50   /* 会话建立请求 */
#define PFCP_SESSION_EST_RSP   51   /* 会话建立响应 */
#define PFCP_SESSION_MOD_REQ   52   /* 会话修改请求 */
#define PFCP_SESSION_MOD_RSP   53   /* 会话修改响应 */
#define PFCP_SESSION_DEL_REQ   54   /* 会话删除请求 */
#define PFCP_SESSION_DEL_RSP   55   /* 会话删除响应 */

/* PFCP 原因码 */
#define PFCP_CAUSE_OK          1    /* 请求成功 */
#define PFCP_CAUSE_INVALID     2    /* 无效消息 */

/* ============= PFCP 报头定义 ============= */

struct pfcp_header {
    uint8_t flags;          /* Bit 7: SEID flag, Bits 6-5: Version */
    uint8_t msg_type;       /* 消息类型 */
    uint16_t msg_length;    /* 消息长度（不含前 4 字节头） */
    uint64_t seid;          /* Session Endpoint ID (可选) */
    uint32_t seq_number;    /* 序列号（3 字节） + 预留 1 字节 */
} __attribute__((packed));

/* ============= PFCP Information Elements (IEs) ============= */

/* Node ID IE (type 60) */
struct pfcp_node_id_ie {
    uint16_t type;              /* IE type = 60 */
    uint16_t length;            /* IE length */
    uint8_t node_id_type;       /* 0: IPv4, 1: IPv6, 2: FQDN */
    uint32_t ipv4_addr;         /* IPv4 地址 */
} __attribute__((packed));

/* Create PDR IE (type 56) - Packet Detection Rule */
struct pfcp_create_pdr_ie {
    uint16_t type;              /* IE type = 56 */
    uint16_t length;
    uint32_t pdr_id;            /* PDR ID */
    uint16_t pdr_precedence;    /* 优先级 */
} __attribute__((packed));

/* Create FAR IE (type 70) - Forwarding Action Rule */
struct pfcp_create_far_ie {
    uint16_t type;              /* IE type = 70 */
    uint16_t length;
    uint32_t far_id;            /* FAR ID */
    uint8_t far_action;         /* DROP=1, FORWARD=2, BUFFER=4 */
    uint32_t destination;       /* 1=RAN, 2=CP, 3=DN */
    uint32_t outer_ip_addr;     /* 外层 IP 地址 */
    uint16_t outer_udp_port;    /* 外层 UDP 端口 */
} __attribute__((packed));

/* ============= 数据结构定义 ============= */

typedef struct {
    uint64_t seid;              /* Session ID */
    char supi[20];              /* Subscription Permanent Identifier */
    uint32_t ue_ip;             /* UE IPv4 地址 */
    uint32_t gnb_ip;            /* gNodeB IPv4 地址 */
    uint16_t gnb_port;          /* gNodeB UDP 端口 */
    uint32_t teid_downlink;     /* 下行 TEID */
    uint32_t teid_uplink;       /* 上行 TEID */
    uint8_t pdu_session_id;     /* PDU 会话 ID */
    uint8_t qos_priority;       /* QoS 优先级 */
    uint32_t qos_mbr_ul;        /* QoS MBR 上行 */
    uint32_t qos_mbr_dl;        /* QoS MBR 下行 */
    uint32_t state;             /* 会话状态: 0=创建中, 1=活跃, 2=修改中 */
} session_info_t;

/* ============= 全局变量 ============= */
static uint32_t g_seq_number = 1;
static session_info_t g_sessions[10];
static int g_session_count = 0;

/* ============= 辅助函数 ============= */

/**
 * 打印十六进制数据
 */
void print_hex(const char *title, const uint8_t *data, int len) {
    printf("[%s] ", title);
    for (int i = 0; i < len && i < 48; i++) {
        printf("%02x ", data[i]);
        if ((i + 1) % 16 == 0 && i + 1 < len) printf("\n           ");
    }
    if (len > 48) printf("...");
    printf("\n");
}

/**
 * 打印 IP 地址
 */
void print_ip(uint32_t ip) {
    printf("%d.%d.%d.%d",
           (ip >>  0) & 0xFF,
           (ip >>  8) & 0xFF,
           (ip >> 16) & 0xFF,
           (ip >> 24) & 0xFF);
}

/**
 * 获取下一个序列号
 */
uint32_t get_next_seq_number(void) {
    return g_seq_number++;
}

/**
 * 获取下一个 SEID
 */
uint64_t get_next_seid(void) {
    static uint64_t seid_counter = 0x1000000000000001UL;
    return seid_counter++;
}

/* ============= PFCP 消息构造函数 ============= */

/**
 * 构造 PFCP 报头
 */
void pfcp_build_header(uint8_t *buffer, uint8_t msg_type,
                       uint16_t msg_length, uint64_t seid, int with_seid) {
    struct pfcp_header *hdr = (struct pfcp_header *)buffer;

    /* flags: bit 7=S (SEID present), bits 6-5=Version */
    hdr->flags = 0x00;  /* Version 1, no SEID initially */
    if (with_seid) {
        hdr->flags |= 0x80;  /* Set S bit */
    }
    hdr->flags |= 0x20;  /* Set Version = 1 */

    hdr->msg_type = msg_type;
    hdr->msg_length = htons(msg_length);

    if (with_seid) {
        hdr->seid = htobe64(seid);
        /* 序列号在 SEID 之后 */
        uint32_t *seq_ptr = (uint32_t *)(&hdr->seq_number);
        *seq_ptr = htonl(get_next_seq_number() << 8);  /* 3 字节序列号 */
    } else {
        /* 序列号紧跟在报头后 */
        uint32_t *seq_ptr = (uint32_t *)(&hdr->seq_number);
        *seq_ptr = htonl(get_next_seq_number() << 8);  /* 3 字节序列号 */
    }
}

/**
 * 构造会话建立请求消息
 *
 * 返回消息长度
 */
int pfcp_build_session_establishment_request(uint8_t *buffer,
                                            const session_info_t *session) {
    uint8_t *ptr = buffer;
    uint16_t msg_length = 0;
    uint64_t seid = get_next_seid();

    /* 构造报头（无 SEID） */
    pfcp_build_header(ptr, PFCP_SESSION_EST_REQ, 100, 0, 0);
    ptr += 8;
    msg_length += 8;

    /* Node ID IE (type 60) - SMF 节点标识 */
    struct pfcp_node_id_ie *node_ie = (struct pfcp_node_id_ie *)ptr;
    node_ie->type = htons(60);
    node_ie->length = htons(5);
    node_ie->node_id_type = 0;  /* IPv4 */
    node_ie->ipv4_addr = inet_addr("192.168.1.30");  /* SMF IP */
    ptr += sizeof(*node_ie);
    msg_length += sizeof(*node_ie);

    /* Create FAR IE (type 70) - 转发规则 */
    uint8_t *far_start = ptr;
    struct pfcp_create_far_ie *far_ie = (struct pfcp_create_far_ie *)ptr;
    far_ie->type = htons(70);
    far_ie->far_id = htonl(1);
    far_ie->far_action = 2;  /* FORWARD */
    far_ie->destination = htonl(1);  /* RAN */
    far_ie->outer_ip_addr = session->gnb_ip;
    far_ie->outer_udp_port = htons(2152);
    far_ie->length = htons(sizeof(*far_ie) - 4);
    ptr += sizeof(*far_ie);
    msg_length += sizeof(*far_ie);

    /* 更新消息长度 */
    struct pfcp_header *hdr = (struct pfcp_header *)buffer;
    hdr->msg_length = htons(msg_length);

    /* 保存 SEID 用于后续操作 */
    session_info_t *new_session = &g_sessions[g_session_count];
    memcpy(new_session, session, sizeof(*session));
    new_session->seid = seid;
    new_session->state = 0;  /* 创建中 */
    g_session_count++;

    printf("[PFCP] Session Establishment Request created\n");
    printf("  SEID: 0x%016lx\n", seid);
    printf("  Message Length: %u bytes\n", msg_length);

    return msg_length + 8;  /* 包括报头 */
}

/**
 * 解析 PFCP 会话建立响应
 */
int pfcp_parse_session_establishment_response(const uint8_t *buffer,
                                             int len) {
    if (len < 8) {
        printf("ERROR: Message too short\n");
        return -1;
    }

    struct pfcp_header *hdr = (struct pfcp_header *)buffer;

    printf("\n[PFCP Response Received]\n");
    printf("  Message Type: %u\n", hdr->msg_type);
    printf("  Message Length: %u\n", ntohs(hdr->msg_length));

    if (hdr->msg_type != PFCP_SESSION_EST_RSP) {
        printf("ERROR: Expected Session Establishment Response (51)\n");
        return -1;
    }

    printf("  ✅ Session established successfully\n");

    return 0;
}

/* ============= 会话管理 ============= */

/**
 * 创建示例会话
 */
void create_example_sessions(void) {
    /* 会话 1 */
    session_info_t session1;
    strcpy(session1.supi, "234010012340000");
    session1.ue_ip = inet_addr("10.0.0.2");
    session1.gnb_ip = inet_addr("192.168.1.100");
    session1.gnb_port = 2152;
    session1.teid_downlink = 0x12345678;
    session1.teid_uplink = 0x87654321;
    session1.pdu_session_id = 1;
    session1.qos_priority = 5;
    session1.qos_mbr_ul = 1000000;
    session1.qos_mbr_dl = 10000000;

    /* 会话 2 */
    session_info_t session2;
    strcpy(session2.supi, "234010012340001");
    session2.ue_ip = inet_addr("10.0.0.3");
    session2.gnb_ip = inet_addr("192.168.1.101");
    session2.gnb_port = 2152;
    session2.teid_downlink = 0x11111111;
    session2.teid_uplink = 0x22222222;
    session2.pdu_session_id = 1;
    session2.qos_priority = 7;
    session2.qos_mbr_ul = 2000000;
    session2.qos_mbr_dl = 20000000;

    /* 会话 3 */
    session_info_t session3;
    strcpy(session3.supi, "234010012340002");
    session3.ue_ip = inet_addr("10.0.0.4");
    session3.gnb_ip = inet_addr("192.168.1.102");
    session3.gnb_port = 2152;
    session3.teid_downlink = 0x33333333;
    session3.teid_uplink = 0x44444444;
    session3.pdu_session_id = 1;
    session3.qos_priority = 5;
    session3.qos_mbr_ul = 1500000;
    session3.qos_mbr_dl = 15000000;

    printf("\n[SMF] Creating example sessions...\n");
    printf("  Session 1: SUPI=%s, UE IP=10.0.0.2\n", session1.supi);
    printf("  Session 2: SUPI=%s, UE IP=10.0.0.3\n", session2.supi);
    printf("  Session 3: SUPI=%s, UE IP=10.0.0.4\n", session3.supi);
}

/* ============= PFCP 客户端交互 ============= */

/**
 * 发送 PFCP 会话建立请求到 UPF
 */
int send_session_establishment_request(int sock,
                                      const struct sockaddr_in *upf_addr,
                                      const session_info_t *session) {
    uint8_t buffer[4096];
    int msg_len = pfcp_build_session_establishment_request(buffer, session);

    printf("\n[PFCP] Sending Session Establishment Request to UPF\n");
    printf("  Target: ");
    print_ip(ntohl(upf_addr->sin_addr.s_addr));
    printf(":%u\n", ntohs(upf_addr->sin_port));

    print_hex("Request Hex", buffer, msg_len);

    if (sendto(sock, buffer, msg_len, 0,
               (struct sockaddr *)upf_addr, sizeof(*upf_addr)) < 0) {
        perror("sendto");
        return -1;
    }

    printf("[SUCCESS] Request sent\n");
    return 0;
}

/**
 * 接收 PFCP 响应
 */
int receive_pfcp_response(int sock, uint8_t *buffer, int buffer_len) {
    struct sockaddr_in src_addr;
    socklen_t src_addr_len = sizeof(src_addr);

    printf("\n[PFCP] Waiting for UPF response...\n");

    int recv_len = recvfrom(sock, buffer, buffer_len, 0,
                           (struct sockaddr *)&src_addr, &src_addr_len);

    if (recv_len < 0) {
        perror("recvfrom");
        return -1;
    }

    printf("[PFCP] Received %d bytes from ", recv_len);
    print_ip(ntohl(src_addr.sin_addr.s_addr));
    printf(":%u\n", ntohs(src_addr.sin_port));

    print_hex("Response Hex", buffer, recv_len);

    return recv_len;
}

/* ============= 主函数 ============= */

int main(int argc, char *argv[]) {
    int sock;
    struct sockaddr_in upf_addr;
    uint8_t buffer[4096];
    int recv_len;

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║              SMF PFCP Client (Simulator)                   ║\n");
    printf("║         Sends PFCP messages to UPF for session mgmt        ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n\n");

    /* 创建 UDP 套接字 */
    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }

    /* 设置套接字选项：允许端口重用 */
    int opt = 1;
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    /* 设置接收超时 */
    struct timeval tv;
    tv.tv_sec = 5;  /* 5 秒超时 */
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    /* 连接到 UPF */
    memset(&upf_addr, 0, sizeof(upf_addr));
    upf_addr.sin_family = AF_INET;
    upf_addr.sin_addr.s_addr = inet_addr(PFCP_SERVER_IP);
    upf_addr.sin_port = htons(PFCP_SERVER_PORT);

    printf("[SMF] Connecting to UPF at ");
    print_ip(inet_addr(PFCP_SERVER_IP));
    printf(":%u\n", PFCP_SERVER_PORT);

    /* 创建示例会话 */
    create_example_sessions();

    /* 发送多个会话建立请求 */
    printf("\n╔════════════════════════════════════════════════════════════╗\n");
    printf("║              PFCP Session Establishment Flow               ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    for (int i = 0; i < 3; i++) {
        session_info_t session;

        /* 准备会话信息 */
        switch (i) {
            case 0:
                strcpy(session.supi, "234010012340000");
                session.ue_ip = inet_addr("10.0.0.2");
                session.gnb_ip = inet_addr("192.168.1.100");
                session.teid_downlink = 0x12345678;
                session.teid_uplink = 0x87654321;
                break;
            case 1:
                strcpy(session.supi, "234010012340001");
                session.ue_ip = inet_addr("10.0.0.3");
                session.gnb_ip = inet_addr("192.168.1.101");
                session.teid_downlink = 0x11111111;
                session.teid_uplink = 0x22222222;
                break;
            case 2:
                strcpy(session.supi, "234010012340002");
                session.ue_ip = inet_addr("10.0.0.4");
                session.gnb_ip = inet_addr("192.168.1.102");
                session.teid_downlink = 0x33333333;
                session.teid_uplink = 0x44444444;
                break;
        }

        session.gnb_port = 2152;
        session.pdu_session_id = 1;
        session.qos_priority = 5;
        session.qos_mbr_ul = 1000000 * (i + 1);
        session.qos_mbr_dl = 10000000 * (i + 1);

        printf("\n");
        printf("═══════════════════════════════════════════════════════════\n");
        printf("Session %d/%d: SUPI=%s\n", i + 1, 3, session.supi);
        printf("═══════════════════════════════════════════════════════════\n");

        /* 发送建立请求 */
        if (send_session_establishment_request(sock, &upf_addr, &session) < 0) {
            fprintf(stderr, "Failed to send request\n");
            continue;
        }

        /* 接收响应 */
        recv_len = receive_pfcp_response(sock, buffer, sizeof(buffer));
        if (recv_len > 0) {
            pfcp_parse_session_establishment_response(buffer, recv_len);
        } else {
            printf("[WARNING] No response from UPF (timeout or error)\n");
        }

        sleep(1);  /* 等待 1 秒后发送下一个请求 */
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════\n");
    printf("[SMF] All sessions sent\n");
    printf("═══════════════════════════════════════════════════════════\n\n");

    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                  Summary                                   ║\n");
    printf("╠════════════════════════════════════════════════════════════╣\n");
    printf("║  Sent:      3 PFCP Session Establishment Requests          ║\n");
    printf("║  Protocol:  PFCP (UDP 8805)                                ║\n");
    printf("║  Target:    UPF at 127.0.0.1:8805                          ║\n");
    printf("║  Status:    Complete                                       ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    close(sock);
    return 0;
}

/*
 * ============= 使用说明 =============
 *
 * 编译：
 *   gcc -o smf_pfcp_client smf_pfcp_client.c -lm
 *
 * 运行方式 1：与本地 UPF 通信
 *   终端 1: ./bin/pfcp_receiver_example
 *   终端 2: ./bin/smf_pfcp_client
 *
 * 运行方式 2：与完整 UPF 通信
 *   终端 1: sudo ./bin/upf_rss_multi_queue -l 0-5 -n 4
 *   终端 2: ./bin/smf_pfcp_client
 *
 * 协议流程：
 *   1. SMF 发送 PFCP Session Establishment Request
 *   2. UPF 接收请求，解析会话信息
 *   3. UPF 建立会话表项
 *   4. UPF 发送 PFCP Session Establishment Response
 *   5. SMF 接收响应，确认会话创建
 *   6. 循环创建多个会话
 *
 * PFCP 协议规范：
 *   - UDP 端口：8805
 *   - 报头格式：固定 8 字节
 *   - 序列号：3 字节（用于匹配请求/响应）
 *   - SEID：可选，用于会话标识
 *
 */
