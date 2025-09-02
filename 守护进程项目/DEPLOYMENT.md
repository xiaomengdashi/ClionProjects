# Linux HTTP Server单文件部署清单

## 必需文件
只需要以下2个文件：
- `A.exe` - 您的HTTP服务器可执行文件
- `install.sh` - 系统安装脚本

## 可选文件
- `quick_start.sh` - 手动启动脚本
- `httpserver.service` - systemd服务文件（由install.sh自动安装）

## 部署步骤

### 步骤1: 准备文件
```bash
# 将您的可执行文件重命名为 A.exe
cp your-http-server.exe A.exe

# 确保文件可执行
chmod +x A.exe
```

### 步骤2: 上传部署
```bash
# 上传文件到服务器
scp A.exe install.sh user@your-server:/tmp/

# 登录服务器
ssh user@your-server

# 进入目录
cd /tmp

# 运行安装
sudo ./install.sh
```

### 步骤3: 管理服务
```bash
# 启动服务
sudo systemctl start httpserver

# 查看状态
sudo systemctl status httpserver

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

## 注意事项
- A.exe需要是Linux ELF格式的可执行文件
- 确保已安装所有依赖库
- 服务默认监听8080端口
- 日志文件在 `/var/log/httpserver/`