#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/basic_file_sink.h"

int main() {
    try {
        // 初始化异步日志线程池
        spdlog::init_thread_pool(8192, 1); // 8192个线程安全队列，1个后台线程

        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("logs/async.log", true);
        std::vector<spdlog::sink_ptr> sinks{file_sink};

        // 创建异步logger
        auto logger = std::make_shared<spdlog::async_logger>(
            "async_logger", sinks.begin(), sinks.end(),
            spdlog::thread_pool(), spdlog::async_overflow_policy::block);

        spdlog::register_logger(logger);

        // 输出日志
        for (int i = 0; i < 100; i++) {
            logger->info("Async message {}", i);
        }

        spdlog::info("Async logging example completed");

        // 关闭所有logger
        spdlog::drop_all();
    } catch (const spdlog::spdlog_ex &ex) {
        spdlog::error("Log initialization failed: {}", ex.what());
    }

    return 0;
}
