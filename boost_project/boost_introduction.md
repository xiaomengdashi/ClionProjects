# Boost C++ 库简介

## 简介

**Boost** 是一套经过同行评审的 C++ 库，提供了对标准库的补充功能。它是 C++ 开发中最常用和最重要的第三方库之一。

## 版本信息

- **最新版本**：Boost 1.90.0（发布于 2025年10月10日）
- **推荐版本**：1.88.0 或更高版本
- **最低 C++ 标准**：C++11（Boost 1.84 开始不再支持 C++03）
- **发布周期**：平均每半年发布一个新版本

## 主要特点

- **高质量**：由 C++ 标准委员会成员维护
- **广泛使用**：被许多大型项目采用
- **文档完善**：提供详细的使用文档
- **跨平台**：支持多个操作系统和编译器
- **持续更新**：定期更新和改进

## 常用模块

| 模块 | 用途 |
|------|------|
| **Asio** | 网络编程和异步 I/O，支持异步操作和高性能网络 |
| **Thread** | 多线程编程，包括线程管理和同步机制 |
| **Smart Ptr** | 智能指针（shared_ptr、unique_ptr 等）|
| **Regex** | 正则表达式处理 |
| **Filesystem** | 文件系统操作（已成为 C++17 标准库的一部分） |
| **String** | 字符串处理工具 |
| **Math** | 数学函数库和数值计算 |
| **Date Time** | 日期和时间处理 |
| **Serialization** | 对象序列化和反序列化 |
| **Program Options** | 命令行参数解析 |
| **Json** | JSON 数据处理 |
| **Hash2** | 哈希库（Boost 1.88 新增） |
| **MQTT5** | MQTT 5.0 协议支持（Boost 1.88 新增） |

## 最新版本新特性（1.88-1.90）

- **Hash2 库**：新增高性能哈希算法库
- **MQTT5 库**：添加对 MQTT 5.0 协议的完整支持
- **字段名反射**（Boost 1.84）：支持编译期获取聚合类型的字段名
- **日志库改进**（Boost 1.85）：增强日志文件轮转和通道管理功能
- **性能优化**：多个库的性能改进和优化
- **C++20 支持**：增强对 C++20 特性的支持

## 安装

### Ubuntu/Debian
```bash
# 安装最新版本（通过包管理器）
sudo apt-get install libboost-all-dev

# 查看可用版本
apt-cache search libboost-dev
```

### macOS
```bash
# 使用 Homebrew 安装
brew install boost

# 安装特定版本
brew install boost@1.88
```

### 源码编译（推荐用于最新版本）
```bash
# 下载最新版本（1.90.0）
wget https://github.com/boostorg/boost/releases/download/boost-1.90.0/boost-1.90.0.tar.gz

# 解压并编译
tar -xzf boost-1.90.0.tar.gz
cd boost-1.90.0
./bootstrap.sh
./b2 install

# 或使用 CMake (Boost 1.74+)
mkdir build && cd build
cmake -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build . --target install
```

### CMake 集成（推荐）
```cmake
# 在 CMakeLists.txt 中使用
find_package(Boost 1.88 REQUIRED COMPONENTS system thread)
target_link_libraries(your_target Boost::system Boost::thread)
```

## 简单使用示例

### 1. 智能指针（现代写法）
```cpp
#include <boost/smart_ptr/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <iostream>

int main() {
    // C++17+ 推荐使用 std::shared_ptr，但 Boost 提供了兼容性和扩展
    auto ptr = boost::make_shared<int>(42);
    std::cout << *ptr << std::endl;
    return 0;
}
```

### 2. 异步网络编程（Asio）
```cpp
#include <boost/asio.hpp>
#include <iostream>

using boost::asio::ip::tcp;

int main() {
    boost::asio::io_context io;
    tcp::resolver resolver(io);
    auto results = resolver.resolve("example.com", "http");

    std::cout << "Resolved addresses:" << std::endl;
    for (auto result : results) {
        std::cout << result.endpoint() << std::endl;
    }
    return 0;
}
```

### 3. 字符串处理和正则表达式
```cpp
#include <boost/regex.hpp>
#include <boost/algorithm/string.hpp>
#include <iostream>
#include <string>

int main() {
    std::string text = "Hello Boost Library";

    // 字符串操作
    boost::to_upper(text);
    std::cout << text << std::endl;

    // 正则表达式匹配
    boost::regex pattern("(\\w+)\\s(\\w+)");
    if (boost::regex_match(text, pattern)) {
        std::cout << "Pattern matched!" << std::endl;
    }
    return 0;
}
```

### 4. 文件系统操作
```cpp
#include <boost/filesystem.hpp>
#include <iostream>

namespace fs = boost::filesystem;

int main() {
    fs::path dir("./data");

    if (fs::exists(dir)) {
        for (const auto& entry : fs::directory_iterator(dir)) {
            std::cout << entry.path() << std::endl;
        }
    }
    return 0;
}
```

## 资源和参考

- **官方网站**：https://www.boost.org/
- **GitHub 仓库**：https://github.com/boostorg/boost
- **官方文档**：https://www.boost.org/doc/
- **教程网站**：https://theboostcpplibraries.com/
