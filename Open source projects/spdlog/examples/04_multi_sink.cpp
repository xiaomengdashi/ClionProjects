#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"
#include "spdlog/sinks/basic_file_sink.h"

int main() {
    try {
        // 创建多个sink
        std::vector<spdlog::sink_ptr> sinks;

        // 添加控制台输出
        auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
        sinks.push_back(console_sink);

        // 添加文件输出
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/multi_sink.log", true);
        sinks.push_back(file_sink);

        // 创建logger并注册
        auto logger = std::make_shared<spdlog::logger>("multi_sink", sinks.begin(), sinks.end());
        logger->set_level(spdlog::level::info);
        spdlog::register_logger(logger);

        logger->info("Message to both console and file");
        logger->warn("Warning message to both outputs");
        logger->error("Error message to both outputs");

        spdlog::info("Multi-sink logging example completed");
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
