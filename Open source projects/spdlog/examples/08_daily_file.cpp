#include "spdlog/spdlog.h"
#include "spdlog/sinks/daily_file_sink.h"

int main() {
    try {
        // 创建每日日志文件sink
        // 参数：文件名、每天的切换时刻(2:00 AM)、日志文件保留天数
        auto daily_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            "logs/daily.log", 2, 0);

        spdlog::logger daily_logger("daily_logger", daily_sink);
        spdlog::register_logger(std::make_shared<spdlog::logger>(daily_logger));

        // 输出日志
        for (int i = 0; i < 20; i++) {
            daily_logger.info("Daily log message {}", i);
        }

        spdlog::info("Daily file logging example completed");
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
