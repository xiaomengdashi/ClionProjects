# Linux HTTP Server单文件部署指南

## 项目概述
本项目为单文件HTTP服务器部署方案，直接使用您提供的可执行文件，无需额外编译。

## 文件结构
```
boot_demo/
├── A.exe              # 您的HTTP服务器可执行文件
├── install.sh         # 系统安装脚本
├── quick_start.sh     # 手动启动脚本
├── httpserver.service # systemd服务文件
├── DEPLOYMENT.md      # 部署清单
└── README_LINUX.md    # 本文档
```

## 快速部署

### 1. 准备文件
```bash
# 将您的可执行文件重命名为 A.exe
cp your-http-server.exe A.exe

# 确保文件可执行
chmod +x A.exe
```

### 2. 上传部署
```bash
# 上传文件到服务器
scp A.exe install.sh user@your-server:/tmp/

# 登录服务器并安装
ssh user@your-server
cd /tmp
sudo ./install.sh
```

### 3. 管理服务
```bash
# 启动服务
sudo systemctl start httpserver

# 停止服务
sudo systemctl stop httpserver

# 重启服务
sudo systemctl restart httpserver

# 查看状态
sudo systemctl status httpserver

# 开机自启
sudo systemctl enable httpserver

# 查看日志
sudo journalctl -u httpserver -f
```

## 手动启动（无需安装）
```bash
# 使用快速启动脚本
./quick_start.sh start

# 或直接运行
./A.exe
```

## 配置说明

### 配置文件位置
- 系统配置: `/etc/httpserver.conf`
- 日志目录: `/var/log/httpserver/`
- PID文件: `/var/run/httpserver.pid`

### 配置文件内容
```bash
EXEC_PATH=/usr/local/bin/httpserver
PID_FILE=/var/run/httpserver.pid
LOG_FILE=/var/log/httpserver/access.log
ERROR_LOG=/var/log/httpserver/error.log
PORT=8080
```

## 故障排查

### 常见问题
1. **权限问题**: 确保www-data用户有权限访问相关目录
2. **端口冲突**: 检查8080端口是否被占用
3. **依赖问题**: 确保所有依赖库已安装

### 日志查看
```bash
# 系统日志
sudo journalctl -u httpserver --since "1 hour ago"

# 访问日志
sudo tail -f /var/log/httpserver/access.log

# 错误日志
sudo tail -f /var/log/httpserver/error.log
```

## 卸载
```bash
sudo systemctl stop httpserver
sudo systemctl disable httpserver
sudo rm /etc/systemd/system/httpserver.service
sudo rm /usr/local/bin/httpserver
sudo rm -rf /var/log/httpserver
sudo systemctl daemon-reload
```

## 注意事项
- A.exe需要是Linux ELF格式的可执行文件
- 确保已安装所有依赖库
- 服务默认监听8080端口
- 建议使用非root用户运行服务

## 支持
如有问题，请查看日志文件或提交issue。