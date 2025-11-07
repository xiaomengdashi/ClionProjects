cmake_minimum_required(VERSION 3.11)
project(spdlog_examples)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 查找已安装的spdlog库
find_package(spdlog REQUIRED)

# 示例1：基本使用
add_executable(example_basic examples/01_basic.cpp)
target_link_libraries(example_basic spdlog::spdlog)

# 示例2：创建具名logger
add_executable(example_named_logger examples/02_named_logger.cpp)
target_link_libraries(example_named_logger spdlog::spdlog)

# 示例3：日志写入文件
add_executable(example_file_logging examples/03_file_logging.cpp)
target_link_libraries(example_file_logging spdlog::spdlog)

# 示例4：多sink
add_executable(example_multi_sink examples/04_multi_sink.cpp)
target_link_libraries(example_multi_sink spdlog::spdlog)

# 示例5：日志级别和格式
add_executable(example_level_pattern examples/05_level_pattern.cpp)
target_link_libraries(example_level_pattern spdlog::spdlog)

# 示例6：异步日志
add_executable(example_async_logging examples/06_async_logging.cpp)
target_link_libraries(example_async_logging spdlog::spdlog)

# 示例7：旋转日志文件
add_executable(example_rotating_file examples/07_rotating_file.cpp)
target_link_libraries(example_rotating_file spdlog::spdlog)

# 示例8：每日日志文件
add_executable(example_daily_file examples/08_daily_file.cpp)
target_link_libraries(example_daily_file spdlog::spdlog)

# 示例9：全局logger
add_executable(example_global_logger examples/09_global_logger.cpp)
target_link_libraries(example_global_logger spdlog::spdlog)

# 示例10：自定义格式
add_executable(example_custom_format examples/10_custom_format.cpp)
target_link_libraries(example_custom_format spdlog::spdlog)
