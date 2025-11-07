#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main() {
    // 创建控制台logger
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

    spdlog::info("Level and pattern example completed");

    return 0;
}
