# 视频语音聊天室应用

一个基于C++11的实时视频语音聊天室应用，支持一对一或多人群组通话。采用前后端分离架构，使用WebRTC技术实现实时音视频通信。

## 功能特性

- 🎥 实时视频通话
- 🎤 实时语音通话
- 👥 支持多人群组聊天
- 🔒 HTTPS安全连接
- 🎯 自动房间管理
- 📱 响应式界面设计
- 🎮 实时控制（静音/关闭摄像头）

## 技术架构

### 后端
- **语言**: C++11
- **HTTP服务器**: cpp-httplib
- **WebSocket**: websocketpp
- **JSON处理**: nlohmann/json
- **SSL/TLS**: OpenSSL

### 前端
- **实时通信**: WebRTC
- **UI框架**: 原生HTML/CSS/JavaScript
- **通信协议**: WebSocket

## 系统要求

- C++11兼容的编译器（GCC 4.8.1+或Clang 3.3+）
- CMake 3.10+
- OpenSSL 1.0.2+开发库
- Boost库 1.66.0+（主要用于websocketpp）
- 支持WebRTC的现代浏览器（Chrome 60+、Firefox 55+、Safari 11+）
- 至少2GB内存和500MB硬盘空间

### 支持的操作系统
- Ubuntu 16.04+/Debian 9+
- CentOS/RHEL 7+
- macOS 10.12+（需要额外配置）
- Windows 10+（通过WSL2或手动编译）

## 快速开始

### 1. 安装依赖

在Ubuntu/Debian系统上：
```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libssl-dev libboost-all-dev openssl
```

### 2. 克隆项目
```bash
git clone <repository-url>
cd StdHTTP
```

### 3. 构建项目
```bash
./build.sh
```

### 4. 生成SSL证书
```bash
./generate_certificates.sh
```

### 5. 运行服务器
```bash
./run.sh
```

服务器将在以下端口启动：
- HTTPS: 8443
- WebSocket: 9443

### 6. 访问应用

打开浏览器访问: https://localhost:8443

**注意**: 由于使用自签名证书，浏览器会显示安全警告，请选择继续访问。

## 使用说明

1. **加入房间**: 在首页输入房间号，点击"加入房间"
2. **权限授予**: 首次使用需要授予摄像头和麦克风权限
3. **视频通话**: 成功加入后会自动与房间内其他用户建立连接
4. **控制选项**:
   - 🎤 点击麦克风图标切换静音
   - 📷 点击摄像头图标开关视频
   - 点击"离开房间"退出当前房间

## 项目结构

```
StdHTTP/
├── src/
│   ├── server/          # 服务器端代码
│   │   ├── main.cpp     # 主程序入口
│   │   ├── ChatServer.* # HTTPS服务器
│   │   ├── WebSocketHandler.* # WebSocket处理
│   │   └── RoomManager.*      # 房间管理
│   └── common/          # 公共定义
│       └── Message.h    # 消息类型定义
├── public/              # 前端静态文件
│   ├── index.html       # 主页面
│   ├── css/            # 样式文件
│   └── js/             # JavaScript文件
├── external/           # 第三方库
├── certificates/       # SSL证书目录
├── build/             # 构建输出目录
└── CMakeLists.txt     # CMake配置文件
```

## 测试方法

### 单机测试
1. 启动服务器
2. 打开多个浏览器标签页
3. 使用相同的房间号加入
4. 验证视频/音频是否正常传输

### 局域网测试
1. 确保防火墙开放8443和9443端口
2. 其他设备访问 https://<服务器IP>:8443
3. 使用相同房间号进行测试

## 常见问题

### Q: 浏览器提示"不安全的连接"
A: 这是因为使用了自签名证书。在测试环境中，可以选择"高级"并继续访问。生产环境建议使用正规CA签发的证书。

### Q: 无法访问摄像头/麦克风
A: 
- 确保浏览器已授予权限
- 检查设备是否被其他应用占用
- 确保使用HTTPS协议访问（WebRTC要求）

### Q: 视频无法显示
A: 
- 检查浏览器控制台是否有错误信息
- 确认WebSocket连接是否正常
- 验证防火墙是否阻止了相关端口

### Q: 编译时出现找不到库的错误
A:
- 确认已安装所有依赖：`build-essential`, `cmake`, `libssl-dev`, `libboost-all-dev`
- 检查external目录中是否包含所有第三方库
- 如果缺少第三方库，按照"安装依赖"部分的说明下载

### Q: 运行时出现"error while loading shared libraries"错误
A:
- 这通常是因为找不到OpenSSL库。确保已安装`libssl-dev`
- 也可能是Boost库路径问题，检查`libboost-all-dev`是否正确安装
- 运行`ldconfig`更新共享库缓存

## 性能优化建议

1. **视频质量**: 可在`webrtc.js`中调整视频分辨率参数
2. **服务器部署**: 建议使用专业的STUN/TURN服务器
3. **网络优化**: 确保服务器有足够的带宽支持多人视频

## 安全注意事项

- 生产环境请使用正规SSL证书
- 建议添加用户认证机制
- 考虑实现房间密码保护
- 定期更新依赖库版本

## 许可证

本项目采用MIT许可证。详见LICENSE文件。

## 贡献指南

欢迎提交Issue和Pull Request。在提交PR前，请确保：
- 代码符合C++11标准
- 通过所有测试
- 更新相关文档

## 联系方式

如有问题或建议，请通过Issue反馈。 