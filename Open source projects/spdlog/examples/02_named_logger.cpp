#include "spdlog/spdlog.h"
#include "spdlog/sinks/stdout_color_sinks.h"

int main() {
    // 创建一个具名的彩色控制台logger
    auto console = spdlog::stdout_color_mt("console");

    console->info("Hello from named logger: {}", "console");
    console->warn("This is a warning from: {}", "console");
    console->error("Error from: {}", "console");

    return 0;
}
