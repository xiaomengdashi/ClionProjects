#!/bin/bash

# HTTP Server安装脚本 (单文件版)
# 适用于Linux系统

set -e

# 颜色输出
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}开始安装HTTP Server...${NC}"

# 检查是否以root权限运行
if [[ $EUID -ne 0 ]]; then
   echo -e "${RED}错误: 请使用sudo或root权限运行此脚本${NC}"
   exit 1
fi

# 检查可执行文件是否存在
if [ ! -f "A.exe" ]; then
    echo -e "${RED}错误: 未找到可执行文件 A.exe${NC}"
    echo -e "${YELLOW}请将可执行文件重命名为 A.exe 并放在当前目录下${NC}"
    exit 1
fi

# 安装可执行文件
echo -e "${YELLOW}安装可执行文件...${NC}"
cp A.exe /usr/local/bin/httpserver
chmod +x /usr/local/bin/httpserver

# 创建用户和组
if ! id "www-data" &>/dev/null; then
    useradd -r -s /bin/false www-data
fi

# 创建日志目录
mkdir -p /var/log/httpserver
chown www-data:www-data /var/log/httpserver

# 创建PID文件目录
mkdir -p /var/run
chown www-data:www-data /var/run

# 安装systemd服务
echo -e "${YELLOW}配置systemd服务...${NC}"
cp httpserver.service /etc/systemd/system/
systemctl daemon-reload

# 创建配置文件
CONFIG_FILE="/etc/httpserver.conf"
cat > $CONFIG_FILE << EOF
# HTTP Server配置文件
EXEC_PATH=/usr/local/bin/httpserver
PID_FILE=/var/run/httpserver.pid
LOG_FILE=/var/log/httpserver/access.log
ERROR_LOG=/var/log/httpserver/error.log
PORT=8080
EOF

# 创建日志轮转配置
LOGROTATE_FILE="/etc/logrotate.d/httpserver"
cat > $LOGROTATE_FILE << EOF
/var/log/httpserver/*.log {
    daily
    rotate 30
    compress
    delaycompress
    missingok
    notifempty
    create 644 www-data www-data
    postrotate
        systemctl reload httpserver
    endscript
}
EOF

echo -e "${GREEN}安装完成!${NC}"
echo -e "${YELLOW}使用方法:${NC}"
echo "  启动服务: sudo systemctl start httpserver"
echo "  停止服务: sudo systemctl stop httpserver"
echo "  查看状态: sudo systemctl status httpserver"
echo "  开机自启: sudo systemctl enable httpserver"
echo "  查看日志: sudo journalctl -u httpserver -f"