# PcapPlusPlus Replay Project

这是一个基于PcapPlusPlus库的网络数据包处理项目，支持跨平台编译（Windows、macOS、Linux）。

## 项目结构

```
pcapplusplus_repaly/
├── CMakeLists.txt          # 主CMake配置文件
├── README.md               # 项目说明文档
├── resources/              # 资源文件目录
│   └── 1.pcap             # 示例pcap文件
├── src/                    # 源代码目录
│   ├── CMakeLists.txt     # 源代码CMake配置
│   └── send_pcap.cpp      # 主程序源文件
└── test/                   # 测试代码目录
    ├── CMakeLists.txt     # 测试CMake配置
    └── pcap.cpp           # 测试程序源文件
```

## 依赖要求

- CMake 3.10 或更高版本
- C++11 兼容的编译器
- PcapPlusPlus 库

### 平台特定要求

**macOS:**
- Xcode Command Line Tools
- Homebrew (推荐安装PcapPlusPlus)

**Linux:**
- GCC 或 Clang
- libpcap-dev

**Windows:**
- Visual Studio 2017 或更高版本
- WinPcap 或 Npcap

## 安装PcapPlusPlus

### macOS (使用Homebrew)
```bash
brew install pcapplusplus
```

### Linux (从源码编译)
```bash
git clone https://github.com/seladb/PcapPlusPlus.git
cd PcapPlusPlus
./configure-linux.sh
make all
sudo make install
```

### Windows
请参考 [PcapPlusPlus官方文档](https://pcapplusplus.github.io/docs/install/build-source/windows) 进行安装。

## 编译项目

### 1. 创建构建目录
```bash
mkdir build
cd build
```

### 2. 配置项目

**使用默认路径:**
```bash
cmake ..
```

**指定PcapPlusPlus路径:**
```bash
# 使用CMake变量
cmake -DPCAPPLUSPLUS_ROOT=/path/to/pcapplusplus ..

# 或使用环境变量
export PCAPPLUSPLUS_ROOT=/path/to/pcapplusplus
cmake ..
```

### 3. 编译
```bash
make
```

## 运行程序

编译成功后，会生成两个可执行文件：

- `src/send_pcap` - 主程序
- `test/test_pcap` - 测试程序

### 运行主程序
```bash
./src/send_pcap
```

### 运行测试程序
```bash
./test/test_pcap
```

## 默认PcapPlusPlus路径

项目会自动检测以下默认路径：

- **macOS:** `/opt/homebrew/opt/pcapplusplus`
- **Linux:** `/usr/local`
- **Windows:** `C:/PcapPlusPlus`

如果PcapPlusPlus安装在其他位置，请使用上述方法指定正确路径。

## 支持的编译器

- **MSVC** (Windows)
- **MinGW** (Windows)
- **GCC** (Linux)
- **Clang** (macOS/Linux)

## 故障排除

### 找不到PcapPlusPlus库
确保PcapPlusPlus已正确安装，并设置正确的路径：
```bash
export PCAPPLUSPLUS_ROOT=/your/pcapplusplus/path
```

### 编译错误
1. 检查CMake版本是否满足要求
2. 确认编译器支持C++11
3. 验证PcapPlusPlus库路径是否正确

## 许可证

本项目遵循相应的开源许可证。请确保遵守PcapPlusPlus库的许可证要求。

## 贡献

欢迎提交Issue和Pull Request来改进这个项目。