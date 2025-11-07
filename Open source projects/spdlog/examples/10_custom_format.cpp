#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include <iostream>

int main() {
    // 创建控制台logger
    auto console = spdlog::stdout_color_mt("custom");

    // 展示不同的格式模式
    std::cout << "=== 格式化示例 ===" << std::endl;

    // 格式1：简单格式
    console->set_pattern("[%l] %v");
    std::cout << "格式1: [%l] %v" << std::endl;
    console->info("Simple format");

    // 格式2：包含时间戳
    console->set_pattern("[%Y-%m-%d %H:%M:%S] [%l] %v");
    std::cout << "\n格式2: [%Y-%m-%d %H:%M:%S] [%l] %v" << std::endl;
    console->info("With timestamp");

    // 格式3：包含线程和进程ID
    console->set_pattern("[%d/%m/%Y %H:%M:%S.%e] [%t] [%p] [%l] %v");
    std::cout << "\n格式3: [%d/%m/%Y %H:%M:%S.%e] [%t] [%p] [%l] %v" << std::endl;
    console->info("With thread and process ID");

    // 格式4：包含logger名称
    console->set_pattern("[%Y-%m-%d %H:%M:%S] [%n] [%l] %v");
    std::cout << "\n格式4: [%Y-%m-%d %H:%M:%S] [%n] [%l] %v" << std::endl;
    console->info("With logger name");

    // 格式5：完整格式
    console->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%l] %v");
    std::cout << "\n格式5: [%Y-%m-%d %H:%M:%S.%e] [%n] [%t] [%l] %v" << std::endl;
    console->info("Full format");
    console->warn("Warning with full format");
    console->error("Error with full format");

    spdlog::info("Custom format example completed");

    return 0;
}
