# spdlog 使用介绍

spdlog 是一个快速的、仅包含头文件的C++日志库。它提供了简单易用的API，支持多种日志输出方式和格式化选项。

## 主要特性

- **快速高效**：使用异步日志提高性能
- **仅头文件**：无需编译库，直接包含使用
- **灵活的格式化**：支持自定义日志格式
- **多种sink类型**：支持文件、控制台、系统日志等输出方式
- **线程安全**：支持多线程日志输出
- **日志级别**：支持trace、debug、info、warn、err、critical等级别

## 基本用法

### 1. 最简单的使用方式

```cpp
#include "spdlog/spdlog.h"

int main() {
    spdlog::info("Hello, World!");
    spdlog::warn("This is a warning");
    spdlog::error("An error occurred");
    return 0;
}
```

### 2. 创建具名logger

```cpp
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main() {
    auto console = spdlog::stdout_color_mt("console");
    console->info("Hello from logger: {}", "console");
    return 0;
}
```

### 3. 日志写入文件

```cpp
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

int main() {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/app.log");
    spdlog::logger logger("file_logger", file_sink);
    logger.info("Log message to file");
    return 0;
}
```

### 4. 多个sink

```cpp
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/rotating_file_sink.h"

int main() {
    std::vector<spdlog::sink_ptr> sinks;

    // 添加控制台输出
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

    // 添加滚动文件输出
    sinks.push_back(std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        "logs/app.log", 1024 * 1024 * 10, 3)); // 10MB, 3个文件

    auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
    spdlog::register_logger(logger);

    logger->info("Message to both console and file");
    return 0;
}
```

### 5. 设置日志级别和格式

```cpp
#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main() {
    auto console = spdlog::stdout_color_mt("console");

    // 设置日志级别（只显示info及以上）
    console->set_level(spdlog::level::info);

    // 设置日志格式
    console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");

    console->trace("Trace message");      // 不会显示
    console->debug("Debug message");      // 不会显示
    console->info("Info message");        // 显示
    console->warn("Warning message");     // 显示
    console->error("Error message");      // 显示

    return 0;
}
```

## 格式化模式说明

| 模式 | 说明 |
|------|------|
| `%v` | 日志消息 |
| `%l` | 日志级别 |
| `%n` | logger名称 |
| `%d` | 日期和时间 |
| `%Y` | 四位数年份 |
| `%m` | 月份 (01-12) |
| `%d` | 日期 (01-31) |
| `%H` | 小时 (00-23) |
| `%M` | 分钟 (00-59) |
| `%S` | 秒 (00-59) |
| `%e` | 毫秒 (000-999) |
| `%p` | 进程ID |
| `%t` | 线程ID |

## 日志级别

- `trace` - 最详细的日志信息
- `debug` - 调试信息
- `info` - 一般信息
- `warn` - 警告信息
- `err` - 错误信息
- `critical` - 严重错误
- `off` - 关闭日志

## 性能优化

### 异步日志

```cpp
#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

int main() {
    spdlog::init_thread_pool(8192, 1); // 8192个线程安全队列，1个后台线程
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("app.log");
    std::vector<spdlog::sink_ptr> sinks{file_sink};
    auto logger = std::make_shared<spdlog::async_logger>(
        "async_logger", sinks.begin(), sinks.end(),
        spdlog::thread_pool(), spdlog::async_overflow_policy::block);
    spdlog::register_logger(logger);

    for (int i = 0; i < 1000; i++) {
        logger->info("Message {}", i);
    }

    spdlog::drop_all();
    return 0;
}
```

## 安装和配置

spdlog 是仅头文件库，只需在项目中包含头文件即可：

```bash
# 方式1：直接下载源码
git clone https://github.com/gabime/spdlog.git
```

在CMakeLists.txt中配置：

```cmake
cmake_minimum_required(VERSION 3.11)
project(MyProject)

set(CMAKE_CXX_STANDARD 11)

# 添加spdlog
add_subdirectory(spdlog)

add_executable(myapp main.cpp)
target_link_libraries(myapp spdlog::spdlog)
```

## 常见问题

**Q: spdlog是线程安全的吗？**
A: 是的，spdlog提供了线程安全的logger（以_mt后缀标识）。

**Q: 如何在生产环境中禁用日志？**
A: 可以设置日志级别为`off`，或者在编译时定义`SPDLOG_NO_EXCEPTIONS`。

**Q: spdlog支持日志轮转吗？**
A: 支持，使用`rotating_file_sink`可以实现按大小轮转，使用`daily_file_sink`可以实现按日期轮转。
