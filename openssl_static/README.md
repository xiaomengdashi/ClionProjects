# OpenSSL静态库项目演示

本项目演示了如何创建一个链接OpenSSL静态库的项目A，并创建项目B来使用项目A的静态库。

## 项目结构

```
openssl_static/
├── CMakeLists.txt          # 顶层CMake配置
├── build.sh                # 构建脚本
├── README.md               # 项目说明
├── projectA/               # 项目A - 加密工具静态库
│   ├── CMakeLists.txt
│   ├── include/
│   │   └── crypto_utils.h  # 加密工具头文件
│   └── src/
│       └── crypto_utils.cpp # 加密工具实现
└── projectB/               # 项目B - 使用项目A的可执行程序
    ├── CMakeLists.txt
    └── src/
        └── main.cpp        # 主程序
```

## 项目说明

### 项目A (ProjectA)
- **功能**: 提供加密工具函数库
- **依赖**: OpenSSL静态库
- **输出**: 静态库 `libProjectA.a`
- **功能模块**:
  - SHA256哈希计算
  - AES-256-CBC加密/解密
  - 随机数生成

### 项目B (ProjectB)
- **功能**: 演示如何使用项目A的加密功能
- **依赖**: 项目A静态库 + OpenSSL静态库
- **输出**: 可执行文件 `ProjectB`
- **演示内容**:
  - SHA256哈希计算演示
  - AES-256加密解密演示
  - 随机数生成演示

## 构建要求

- CMake 3.16+
- C++17编译器
- OpenSSL开发库

### macOS安装OpenSSL
```bash
brew install openssl
```

### Ubuntu/Debian安装OpenSSL
```bash
sudo apt-get install libssl-dev
```

## 构建方法

### 方法1: 使用构建脚本（推荐）
```bash
chmod +x build.sh
./build.sh
```

### 方法2: 手动构建
```bash
mkdir build
cd build
cmake ..
make
```

## 运行演示

构建完成后，运行项目B：
```bash
./build/bin/ProjectB
```

## 输出文件

- 项目A静态库: `build/lib/libProjectA.a`
- 项目B可执行文件: `build/bin/ProjectB`

## 技术特点

1. **静态链接**: 项目A链接OpenSSL静态库，生成独立的静态库
2. **模块化设计**: 项目B通过链接项目A的静态库来使用加密功能
3. **现代C++**: 使用C++17标准和RAII模式
4. **错误处理**: 完善的异常处理机制
5. **跨平台**: 支持macOS、Linux等平台

## API说明

### CryptoUtils命名空间

- `std::string sha256(const std::string& input)` - 计算SHA256哈希
- `std::vector<unsigned char> aes256Encrypt(...)` - AES-256加密
- `std::string aes256Decrypt(...)` - AES-256解密
- `std::vector<unsigned char> generateRandomBytes(int length)` - 生成随机字节

## 注意事项

1. 确保OpenSSL库已正确安装
2. 在某些系统上可能需要指定OpenSSL路径
3. 静态链接会增加可执行文件大小，但提供更好的部署便利性