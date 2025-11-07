#include "spdlog/spdlog.h"

int main() {
    // 使用全局logger输出日志
    spdlog::info("Hello, spdlog!");
    spdlog::warn("This is a warning message");
    spdlog::error("An error occurred");

    return 0;
}
