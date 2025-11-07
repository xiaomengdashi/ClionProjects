#include "spdlog/spdlog.h"
#include "spdlog/sinks/rotating_file_sink.h"

int main() {
    try {
        // 创建旋转日志文件sink
        // 参数：文件名、每个文件最大大小(10MB)、最多保留3个文件
        auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            "logs/rotating.log", 1024 * 1024 * 10, 3);

        spdlog::logger rotating_logger("rotating_logger", rotating_sink);
        spdlog::register_logger(std::make_shared<spdlog::logger>(rotating_logger));

        // 输出日志（模拟大量日志）
        for (int i = 0; i < 50; i++) {
            rotating_logger.info("Rotating log message {}", i);
        }

        spdlog::info("Rotating file logging example completed");
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
