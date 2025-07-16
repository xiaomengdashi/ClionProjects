# 开发者指南

## 项目概述

本项目是一个基于OpenSSL的加密工具库，提供SHA256哈希、AES-256加密解密和随机数生成功能。项目采用模块化设计，包含静态库(ProjectA)和演示应用(ProjectB)。

## 开发环境设置

### 必需依赖
- CMake 3.16+
- C++17兼容编译器 (GCC 7+, Clang 5+, MSVC 2017+)
- OpenSSL 1.1.0+

### 可选依赖
- Google Test (单元测试)
- Google Benchmark (性能测试)
- lcov/gcov (代码覆盖率)

### 环境配置

#### macOS
```bash
# 使用Homebrew安装依赖
brew install openssl cmake
brew install googletest google-benchmark  # 可选
```

#### Ubuntu/Debian
```bash
sudo apt-get update
sudo apt-get install libssl-dev cmake build-essential
sudo apt-get install libgtest-dev libbenchmark-dev  # 可选
```

## 项目结构

```
openssl_static/
├── CMakeLists.txt              # 顶层构建配置
├── README.md                   # 项目说明
├── BUILD_GUIDE.md             # 构建指南
├── DEVELOPER_GUIDE.md         # 开发者指南
├── build.sh                   # 基础构建脚本
├── advanced_build.sh          # 高级构建脚本
├── build_projectB_only.sh     # 单独构建ProjectB
├── projectA/                  # 静态库项目
│   ├── CMakeLists.txt
│   ├── projecta.pc.in         # pkg-config模板
│   ├── include/
│   │   ├── crypto_utils.h     # 主要API
│   │   ├── crypto_config.h    # 配置管理
│   │   └── crypto_logger.h    # 日志系统
│   ├── src/
│   │   ├── crypto_utils.cpp
│   │   ├── crypto_config.cpp
│   │   └── crypto_logger.cpp
│   ├── tests/
│   │   └── test_crypto_utils.cpp
│   └── benchmarks/
│       └── crypto_benchmark.cpp
└── projectB/                  # 演示应用
    ├── CMakeLists.txt
    └── src/
        └── main.cpp
```

## 构建系统

### 基础构建
```bash
# 快速构建
./build.sh

# 或手动构建
mkdir build && cd build
cmake ..
make -j$(nproc)
```

### 高级构建选项
```bash
# 查看所有选项
./advanced_build.sh --help

# Debug构建 + 测试
./advanced_build.sh --debug --tests

# Release构建 + 基准测试
./advanced_build.sh --release --benchmarks

# 代码覆盖率分析
./advanced_build.sh --coverage --tests

# 清理重新构建
./advanced_build.sh --clean --tests --benchmarks
```

### CMake选项
- `BUILD_TESTS`: 启用单元测试 (默认: OFF)
- `BUILD_BENCHMARKS`: 启用性能基准测试 (默认: OFF)
- `ENABLE_COVERAGE`: 启用代码覆盖率 (默认: OFF)
- `CMAKE_BUILD_TYPE`: 构建类型 (Debug/Release)

## 代码规范

### 命名约定
- 类名: PascalCase (`CryptoUtils`)
- 函数名: camelCase (`generateRandomBytes`)
- 变量名: snake_case (`random_bytes`)
- 常量: UPPER_SNAKE_CASE (`MAX_KEY_SIZE`)
- 文件名: snake_case (`crypto_utils.h`)

### 代码风格
- 缩进: 4个空格
- 行长度: 最大100字符
- 大括号: Allman风格
- 包含顺序: 标准库 → 第三方库 → 项目头文件

### 注释规范
```cpp
/**
 * 简要描述函数功能
 * @param param1 参数1描述
 * @param param2 参数2描述
 * @return 返回值描述
 * @throws std::runtime_error 异常情况描述
 */
std::string functionName(const std::string& param1, int param2);
```

## 测试指南

### 单元测试
```bash
# 构建并运行测试
./advanced_build.sh --tests

# 手动运行测试
cd build
./tests/crypto_tests

# 运行特定测试
./tests/crypto_tests --gtest_filter="SHA256Test.*"
```

### 性能基准测试
```bash
# 构建并运行基准测试
./advanced_build.sh --benchmarks

# 手动运行基准测试
./build/benchmarks/crypto_benchmark

# 运行特定基准测试
./build/benchmarks/crypto_benchmark --benchmark_filter="BM_SHA256.*"
```

### 代码覆盖率
```bash
# 生成覆盖率报告
./advanced_build.sh --coverage --tests

# 查看覆盖率报告
cd build
lcov --list coverage.info
```

## API文档

### 核心功能

#### SHA256哈希
```cpp
#include "crypto_utils.h"

std::string data = "Hello, World!";
std::string hash = CryptoUtils::sha256(data);
```

#### AES-256加密/解密
```cpp
std::string plaintext = "Secret message";
auto key = CryptoUtils::generateRandomBytes(32);  // 256位密钥
auto iv = CryptoUtils::generateRandomBytes(16);   // 128位IV

// 加密
std::string encrypted = CryptoUtils::aes256Encrypt(plaintext, key, iv);

// 解密
std::string decrypted = CryptoUtils::aes256Decrypt(encrypted, key, iv);
```

#### 随机数生成
```cpp
// 生成32字节随机数据
auto randomData = CryptoUtils::generateRandomBytes(32);
```

### 配置管理
```cpp
#include "crypto_config.h"

auto& config = CryptoUtils::Config::getInstance();
config.setLogLevel(CryptoUtils::LogLevel::DEBUG);
config.setDefaultKeySize(32);
```

### 日志系统
```cpp
#include "crypto_logger.h"

// 使用便利宏
CRYPTO_LOG_INFO("操作成功完成");
CRYPTO_LOG_ERROR("发生错误: " + error_message);

// 或直接使用Logger
auto& logger = CryptoUtils::Logger::getInstance();
logger.setLevel(CryptoUtils::LogLevel::DEBUG);
logger.setOutputFile("crypto.log");
```

## 调试指南

### 常见问题

#### 链接错误
```
undefined reference to OpenSSL functions
```
**解决方案**: 确保OpenSSL正确安装并在CMake中找到
```bash
# macOS
export OPENSSL_ROOT_DIR=/usr/local/opt/openssl

# Linux
sudo apt-get install libssl-dev
```

#### 测试失败
```
crypto_tests: command not found
```
**解决方案**: 确保使用`--tests`选项构建
```bash
./advanced_build.sh --tests
```

### 调试技巧

#### 启用详细输出
```bash
./advanced_build.sh --verbose --debug
```

#### 使用调试器
```bash
# GDB
gdb ./build/bin/ProjectB
(gdb) run
(gdb) bt  # 查看调用栈

# LLDB (macOS)
lldb ./build/bin/ProjectB
(lldb) run
(lldb) bt  # 查看调用栈
```

#### 内存检查
```bash
# Valgrind (Linux)
valgrind --leak-check=full ./build/bin/ProjectB

# AddressSanitizer
cmake -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
```

## 贡献指南

### 提交代码前检查清单
- [ ] 代码符合项目风格规范
- [ ] 添加了适当的单元测试
- [ ] 所有测试通过
- [ ] 更新了相关文档
- [ ] 提交信息清晰明确

### 提交信息格式
```
类型(范围): 简短描述

详细描述（可选）

相关问题: #123
```

类型:
- `feat`: 新功能
- `fix`: 错误修复
- `docs`: 文档更新
- `style`: 代码格式
- `refactor`: 重构
- `test`: 测试相关
- `chore`: 构建/工具相关

### 分支策略
- `main`: 稳定版本
- `develop`: 开发分支
- `feature/*`: 功能分支
- `hotfix/*`: 紧急修复

## 性能优化

### 编译优化
```bash
# Release构建启用优化
cmake -DCMAKE_BUILD_TYPE=Release ..

# 启用链接时优化
cmake -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON ..
```

### 运行时优化
- 重用密钥和IV以减少随机数生成开销
- 批量处理数据以提高吞吐量
- 使用内存池减少内存分配

### 基准测试分析
```bash
# 运行基准测试并保存结果
./build/benchmarks/crypto_benchmark --benchmark_format=json > benchmark_results.json

# 比较不同版本性能
./build/benchmarks/crypto_benchmark --benchmark_filter="BM_SHA256.*"
```

## 部署指南

### 静态链接部署
```bash
# 构建静态链接版本
cmake -DCMAKE_EXE_LINKER_FLAGS="-static" ..
```

### 安装系统库
```bash
# 安装到系统目录
cd build
sudo make install

# 使用pkg-config
pkg-config --cflags --libs projecta
```

### Docker部署
```dockerfile
FROM ubuntu:20.04
RUN apt-get update && apt-get install -y libssl-dev
COPY build/bin/ProjectB /usr/local/bin/
CMD ["ProjectB"]
```

## 许可证

本项目采用MIT许可证，详见LICENSE文件。

## 联系方式

如有问题或建议，请通过以下方式联系：
- 提交Issue
- 发送Pull Request
- 邮件联系: [your-email@example.com]