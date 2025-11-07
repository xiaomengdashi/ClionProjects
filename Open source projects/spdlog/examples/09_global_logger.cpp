#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main() {
    try {
        // 获取或创建全局logger
        auto console = spdlog::stdout_color_mt("global_console");
        spdlog::set_default_logger(console);

        // 现在可以使用全局函数输出日志
        spdlog::info("This uses the global logger");
        spdlog::warn("Global logger warning");
        spdlog::error("Global logger error");

        // 也可以通过名称获取logger
        auto logger = spdlog::get("global_console");
        logger->info("Retrieved logger from registry");

        spdlog::info("Global logger example completed");
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
