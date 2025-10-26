#!/bin/bash
# PFCP 协议交互测试脚本
# 演示 SMF 和 UPF 之间的 PFCP 通信

set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

# 打印带颜色的信息
print_header() {
    echo -e "${BLUE}╔════════════════════════════════════════════════════════════╗${NC}"
    echo -e "${BLUE}║${NC} $1"
    echo -e "${BLUE}╚════════════════════════════════════════════════════════════╝${NC}"
}

print_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

print_success() {
    echo -e "${GREEN}[✓]${NC} $1"
}

print_error() {
    echo -e "${RED}[✗]${NC} $1"
}

print_warn() {
    echo -e "${YELLOW}[⚠]${NC} $1"
}

# ============= 主程序 =============

print_header "PFCP 协议交互测试"

echo ""
print_info "检查必要的可执行文件..."

# 检查文件
if [ ! -f "bin/pfcp_receiver_example" ]; then
    print_error "找不到 bin/pfcp_receiver_example"
    exit 1
fi
print_success "pfcp_receiver_example 存在"

if [ ! -f "bin/smf_pfcp_client" ]; then
    print_error "找不到 bin/smf_pfcp_client"
    exit 1
fi
print_success "smf_pfcp_client 存在"

echo ""
print_header "使用说明"

echo -e "${YELLOW}
方式 1：仅测试 PFCP 协议交互
─────────────────────────────────────────────────────────

步骤 1：在终端 1 启动 UPF PFCP 接收器
  $ ./bin/pfcp_receiver_example

步骤 2：在终端 2 启动 SMF PFCP 客户端
  $ ./bin/smf_pfcp_client

预期结果：
  ✓ UPF 接收 3 个会话建立请求
  ✓ SMF 发送 3 个会话信息
  ✓ 双向 PFCP 消息正常交互
  ✓ 显示十六进制消息内容

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

方式 2：测试与完整 UPF 的交互
─────────────────────────────────────────────────────────

步骤 1：在终端 1 启动完整 RSS UPF
  $ sudo ./bin/upf_rss_multi_queue -l 0-5 -n 4

步骤 2：在终端 2 启动 SMF PFCP 客户端
  $ ./bin/smf_pfcp_client

预期结果：
  ✓ UPF 在 PFCP 8805 端口监听会话建立
  ✓ SMF 发送 3 个会话建立请求
  ✓ UPF 动态建立会话表项
  ✓ 之后可传输 GTP-U 数据包

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

方式 3：自动化测试（需要的情况下）
─────────────────────────────────────────────────────────
  $ ./bin/pfcp_receiver_example &
  $ sleep 1
  $ ./bin/smf_pfcp_client
  $ pkill -f pfcp_receiver_example

${NC}"

echo ""
print_header "PFCP 消息格式说明"

echo -e "
${BLUE}会话建立请求 (Session Establishment Request):${NC}
  消息类型: 50 (0x32)
  方向: SMF → UPF
  内容: Node ID、Create FAR 等 IE

${BLUE}会话建立响应 (Session Establishment Response):${NC}
  消息类型: 51 (0x33)
  方向: UPF → SMF
  内容: Cause IE (表示成功/失败)

${BLUE}序列号匹配:${NC}
  请求序列号: 1, 2, 3, ...
  响应序列号: 必须与请求相同
  用途: 匹配异步请求/响应

${BLUE}TEID (Tunnel Endpoint ID):${NC}
  下行 TEID: UPF 发往 gNodeB 的隧道 ID
  上行 TEID: gNodeB 发往 UPF 的隧道 ID
  格式: 32-bit 无符号整数
"

echo ""
print_header "协议交互流程图"

echo -e "
${BLUE}时间轴:${NC}

  SMF                                    UPF
   │                                      │
   ├─ 会话 1 建立请求 (SEID=1) ─────----───>│
   │                                      │
   │<────── 响应 (Seq=1) ────────--────────┤
   │                                      │
   ├─ 会话 2 建立请求 (SEID=2) ───----─────>│
   │                                      │
   │<────── 响应 (Seq=2) ───────--────────┤
   │                                      │
   ├─ 会话 3 建立请求 (SEID=3) ──----──────>│
   │                                      │
   │<────── 响应 (Seq=3) ──────--─────────┤
   │                                      │
   └─ 完成                                └─ 完成

${BLUE}每个会话包含:${NC}
  • UE IP 地址 (10.0.0.x)
  • gNodeB IP 地址 (192.168.1.10x)
  • 下行 TEID (0x12345678 等)
  • 上行 TEID (0x87654321 等)
  • QoS 配置 (优先级、MBR 等)
"

echo ""
print_header "快速启动"

echo -e "
${GREEN}推荐命令:${NC}

# 在一个终端启动 UPF
./bin/pfcp_receiver_example

# 在另一个终端启动 SMF
./bin/smf_pfcp_client

${YELLOW}预计耗时: 5-10 秒${NC}
${YELLOW}预计输出: 完整的 PFCP 协议交互过程${NC}
"

echo ""
print_success "所有检查完毕，可以开始测试！"
echo ""
