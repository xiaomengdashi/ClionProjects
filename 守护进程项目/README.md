# 守护进程项目

## 项目简介
本项目是一个Linux HTTP服务器单文件守护进程部署方案，使用systemd实现进程监控和自动重启功能。

## 文件清单
```
守护进程项目/
├── install.sh         # 系统安装脚本
├── quick_start.sh     # 手动启动脚本
├── httpserver.service # systemd服务配置文件
├── README_LINUX.md    # Linux部署详细指南
├── DEPLOYMENT.md      # 单文件部署清单
└── README.md          # 项目说明（本文件）
```

## 使用说明

### 部署准备
1. 将您的HTTP服务器可执行文件重命名为 `A.exe`
2. 确保A.exe是Linux ELF格式的可执行文件
3. 确保已安装所有依赖库

### 部署步骤
1. **上传文件**：将A.exe和所有脚本文件上传到Linux服务器
2. **一键安装**：运行 `sudo ./install.sh` 自动完成安装
3. **服务管理**：使用systemctl命令管理服务

### 核心功能
- ✅ **进程监控**：systemd持续监控A.exe进程状态
- ✅ **自动重启**：进程崩溃后5秒内自动重启
- ✅ **优雅关闭**：支持systemctl stop优雅停止
- ✅ **日志管理**：自动日志轮转和集中管理
- ✅ **开机自启**：支持开机自动启动

### 常用命令
```bash
# 启动服务
sudo systemctl start httpserver

# 停止服务
sudo systemctl stop httpserver

# 重启服务
sudo systemctl restart httpserver

# 查看状态
sudo systemctl status httpserver

# 查看日志
sudo journalctl -u httpserver -f
```

## 技术架构
- **进程管理**：systemd（Linux系统级进程管理器）
- **监控机制**：cgroups + 进程状态监控
- **重启策略**：
  - 异常退出：立即重启（5秒延迟）
  - 正常退出：不重启
  - 频繁重启：60秒内最多3次，防止无限重启

## 文件说明
- **install.sh**：自动化安装脚本，包含文件复制、权限设置、服务安装等
- **quick_start.sh**：手动启动脚本，支持start/stop/restart/status/logs等操作
- **httpserver.service**：systemd服务配置文件，定义重启策略和资源限制
- **DEPLOYMENT.md**：详细部署清单和注意事项
- **README_LINUX.md**：完整的Linux部署指南

## 注意事项
- 服务默认监听8080端口
- 日志文件位于 `/var/log/httpserver/`
- 配置文件位于 `/etc/httpserver.conf`
- 建议使用非root用户运行服务

## 故障排查
如果服务无法启动或频繁重启，请检查：
1. 端口是否被占用
2. 依赖库是否缺失
3. 文件权限是否正确
4. 查看日志获取详细错误信息