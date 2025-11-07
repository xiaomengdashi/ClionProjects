#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

int main() {
    try {
        // 创建一个文件sink
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/app.log", true);
        spdlog::logger logger("file_logger", file_sink);
        spdlog::register_logger(std::make_shared<spdlog::logger>(logger));

        logger.info("Log message to file");
        logger.warn("Warning message to file");
        logger.error("Error message to file");

        spdlog::info("File logging example completed");
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
