/**
 * RPC计算器客户端示例
 */

#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <thread>
#include <chrono>
#include "../include/stdrpc.hpp"

using namespace stdrpc;

/**
 * 打印菜单
 */
void printMenu() {
    std::cout << "\n========== RPC 计算器客户端 ==========\n";
    std::cout << "1.  加法 (add)\n";
    std::cout << "2.  减法 (subtract)\n";
    std::cout << "3.  乘法 (multiply)\n";
    std::cout << "4.  除法 (divide)\n";
    std::cout << "5.  幂运算 (power)\n";
    std::cout << "6.  平方根 (sqrt)\n";
    std::cout << "7.  数组求和 (sum)\n";
    std::cout << "8.  平均值 (average)\n";
    std::cout << "9.  最大值 (max)\n";
    std::cout << "10. 最小值 (min)\n";
    std::cout << "11. 阶乘 (factorial)\n";
    std::cout << "12. 质数判断 (isPrime)\n";
    std::cout << "13. 最大公约数 (gcd)\n";
    std::cout << "14. 最小公倍数 (lcm)\n";
    std::cout << "15. 字符串连接 (concat)\n";
    std::cout << "16. 字符串反转 (reverse)\n";
    std::cout << "17. 转大写 (toUpper)\n";
    std::cout << "18. 转小写 (toLower)\n";
    std::cout << "19. 字符串长度 (length)\n";
    std::cout << "20. 性能测试\n";
    std::cout << "21. 异步调用示例\n";
    std::cout << "22. 批量调用示例\n";
    std::cout << "23. 服务发现\n";
    std::cout << "24. 健康检查\n";
    std::cout << "0.  退出\n";
    std::cout << "=====================================\n";
    std::cout << "请选择操作: ";
}

/**
 * 读取整数数组
 */
std::vector<int> readIntArray() {
    std::cout << "请输入整数数组（空格分隔，回车结束）: ";
    std::string line;
    std::getline(std::cin >> std::ws, line);
    std::istringstream iss(line);
    std::vector<int> result;
    int num;
    while (iss >> num) {
        result.push_back(num);
    }
    return result;
}

/**
 * 读取浮点数数组
 */
std::vector<double> readDoubleArray() {
    std::cout << "请输入浮点数数组（空格分隔，回车结束）: ";
    std::string line;
    std::getline(std::cin >> std::ws, line);
    std::istringstream iss(line);
    std::vector<double> result;
    double num;
    while (iss >> num) {
        result.push_back(num);
    }
    return result;
}

/**
 * 性能测试
 */
void performanceTest(RpcClient& client) {
    std::cout << "\n=== 性能测试 ===" << std::endl;

    const int num_calls = 1000;
    std::cout << "执行 " << num_calls << " 次加法调用..." << std::endl;

    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_calls; ++i) {
        try {
            int result = client.call<int>("add", i, i + 1);
            // 不输出每次结果，避免影响性能
        } catch (const std::exception& e) {
            std::cerr << "调用失败: " << e.what() << std::endl;
            return;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "完成！" << std::endl;
    std::cout << "总时间: " << duration.count() << " ms" << std::endl;
    std::cout << "平均每次调用: " << (double)duration.count() / num_calls << " ms" << std::endl;
    std::cout << "每秒调用次数: " << (double)num_calls * 1000 / duration.count() << std::endl;
}

/**
 * 异步调用示例
 */
void asyncCallExample(RpcClient& client) {
    std::cout << "\n=== 异步调用示例 ===" << std::endl;

    // 发起多个异步调用
    std::cout << "发起异步调用..." << std::endl;

    auto future1 = client.asyncCall<int>("add", 100, 200);
    auto future2 = client.asyncCall<int>("multiply", 10, 20);
    auto future3 = client.asyncCall<double>("power", 2.0, 10.0);
    auto future4 = client.asyncCall<uint64_t>("factorial", 10);
    auto future5 = client.asyncCall<std::string>("reverse", "Hello World");

    std::cout << "等待结果..." << std::endl;

    // 获取结果
    try {
        int result1 = future1.get();
        std::cout << "100 + 200 = " << result1 << std::endl;

        int result2 = future2.get();
        std::cout << "10 * 20 = " << result2 << std::endl;

        double result3 = future3.get();
        std::cout << "2 ^ 10 = " << result3 << std::endl;

        uint64_t result4 = future4.get();
        std::cout << "10! = " << result4 << std::endl;

        std::string result5 = future5.get();
        std::cout << "reverse(\"Hello World\") = \"" << result5 << "\"" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "异步调用失败: " << e.what() << std::endl;
    }
}

/**
 * 批量调用示例
 */
void batchCallExample(RpcClient& client) {
    std::cout << "\n=== 批量调用示例 ===" << std::endl;
    std::cout << "同时计算多个数学运算..." << std::endl;

    // 并发调用多个方法
    std::vector<std::future<int>> futures;

    // 计算 1到10 的平方
    for (int i = 1; i <= 10; ++i) {
        futures.push_back(client.asyncCall<int>("multiply", i, i));
    }

    // 等待并收集结果
    std::cout << "平方数: ";
    for (int i = 0; i < 10; ++i) {
        try {
            int result = futures[i].get();
            std::cout << result << " ";
        } catch (const std::exception& e) {
            std::cout << "ERROR ";
        }
    }
    std::cout << std::endl;
}

/**
 * 服务发现
 */
void serviceDiscovery(RpcClient& client) {
    std::cout << "\n=== 服务发现 ===" << std::endl;

    try {
        // 获取服务列表
        auto services = client.call<std::vector<std::string>>("__list_services");
        std::cout << "可用服务数量: " << services.size() << std::endl;
        std::cout << "服务列表:" << std::endl;
        for (const auto& service : services) {
            std::cout << "  - " << service << std::endl;
        }

        // 获取服务文档
        std::cout << "\n获取服务文档..." << std::endl;
        std::string doc = client.call<std::string>("__get_documentation");
        std::cout << doc << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "服务发现失败: " << e.what() << std::endl;
    }
}

/**
 * 健康检查
 */
void healthCheck(RpcClient& client) {
    std::cout << "\n=== 健康检查 ===" << std::endl;

    try {
        std::string status = client.call<std::string>("__health_check");
        std::cout << "服务器状态: " << status << std::endl;

        // 获取统计信息
        std::string stats = client.call<std::string>("__get_stats");
        std::cout << "服务器统计:" << std::endl;
        std::cout << stats << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "健康检查失败: " << e.what() << std::endl;
    }
}

int main(int argc, char** argv) {
    // 默认服务器地址和端口
    std::string server_addr = "127.0.0.1";
    uint16_t server_port = 8080;

    // 解析命令行参数
    if (argc > 1) {
        server_addr = argv[1];
    }
    if (argc > 2) {
        server_port = static_cast<uint16_t>(std::atoi(argv[2]));
    }

    std::cout << "========================================" << std::endl;
    std::cout << "        计算器客户端" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "版本: " << stdrpc::getVersion() << std::endl;
    std::cout << "连接到服务器: " << server_addr << ":" << server_port << std::endl;

    try {
        // 创建客户端
        RpcClient client(server_addr, server_port);

        // 连接服务器
        if (!client.connect()) {
            std::cerr << "无法连接到服务器！" << std::endl;
            return 1;
        }

        std::cout << "成功连接到服务器！" << std::endl;

        // 主循环
        bool running = true;
        while (running) {
            printMenu();

            int choice;
            std::cin >> choice;

            if (std::cin.fail()) {
                std::cin.clear();
                std::cin.ignore(10000, '\n');
                std::cout << "无效输入！" << std::endl;
                continue;
            }

            try {
                switch (choice) {
                    case 0:
                        running = false;
                        break;

                    case 1: {  // 加法
                        int a, b;
                        std::cout << "请输入两个整数: ";
                        std::cin >> a >> b;
                        int result = client.call<int>("add", a, b);
                        std::cout << "结果: " << a << " + " << b << " = " << result << std::endl;
                        break;
                    }

                    case 2: {  // 减法
                        int a, b;
                        std::cout << "请输入两个整数: ";
                        std::cin >> a >> b;
                        int result = client.call<int>("subtract", a, b);
                        std::cout << "结果: " << a << " - " << b << " = " << result << std::endl;
                        break;
                    }

                    case 3: {  // 乘法
                        int a, b;
                        std::cout << "请输入两个整数: ";
                        std::cin >> a >> b;
                        int result = client.call<int>("multiply", a, b);
                        std::cout << "结果: " << a << " * " << b << " = " << result << std::endl;
                        break;
                    }

                    case 4: {  // 除法
                        double a, b;
                        std::cout << "请输入两个浮点数: ";
                        std::cin >> a >> b;
                        double result = client.call<double>("divide", a, b);
                        std::cout << "结果: " << a << " / " << b << " = " << result << std::endl;
                        break;
                    }

                    case 5: {  // 幂运算
                        double base, exp;
                        std::cout << "请输入底数和指数: ";
                        std::cin >> base >> exp;
                        double result = client.call<double>("power", base, exp);
                        std::cout << "结果: " << base << " ^ " << exp << " = " << result << std::endl;
                        break;
                    }

                    case 6: {  // 平方根
                        double x;
                        std::cout << "请输入一个数: ";
                        std::cin >> x;
                        double result = client.call<double>("sqrt", x);
                        std::cout << "结果: √" << x << " = " << result << std::endl;
                        break;
                    }

                    case 7: {  // 数组求和
                        auto numbers = readIntArray();
                        if (!numbers.empty()) {
                            int result = client.call<int>("sum", numbers);
                            std::cout << "结果: 数组和 = " << result << std::endl;
                        }
                        break;
                    }

                    case 8: {  // 平均值
                        auto numbers = readDoubleArray();
                        if (!numbers.empty()) {
                            double result = client.call<double>("average", numbers);
                            std::cout << "结果: 平均值 = " << result << std::endl;
                        }
                        break;
                    }

                    case 9: {  // 最大值
                        auto numbers = readIntArray();
                        if (!numbers.empty()) {
                            int result = client.call<int>("max", numbers);
                            std::cout << "结果: 最大值 = " << result << std::endl;
                        }
                        break;
                    }

                    case 10: {  // 最小值
                        auto numbers = readIntArray();
                        if (!numbers.empty()) {
                            int result = client.call<int>("min", numbers);
                            std::cout << "结果: 最小值 = " << result << std::endl;
                        }
                        break;
                    }

                    case 11: {  // 阶乘
                        int n;
                        std::cout << "请输入一个非负整数: ";
                        std::cin >> n;
                        uint64_t result = client.call<uint64_t>("factorial", n);
                        std::cout << "结果: " << n << "! = " << result << std::endl;
                        break;
                    }

                    case 12: {  // 质数判断
                        int n;
                        std::cout << "请输入一个整数: ";
                        std::cin >> n;
                        bool result = client.call<bool>("isPrime", n);
                        std::cout << "结果: " << n << (result ? " 是质数" : " 不是质数") << std::endl;
                        break;
                    }

                    case 13: {  // 最大公约数
                        int a, b;
                        std::cout << "请输入两个整数: ";
                        std::cin >> a >> b;
                        int result = client.call<int>("gcd", a, b);
                        std::cout << "结果: gcd(" << a << ", " << b << ") = " << result << std::endl;
                        break;
                    }

                    case 14: {  // 最小公倍数
                        int a, b;
                        std::cout << "请输入两个整数: ";
                        std::cin >> a >> b;
                        int result = client.call<int>("lcm", a, b);
                        std::cout << "结果: lcm(" << a << ", " << b << ") = " << result << std::endl;
                        break;
                    }

                    case 15: {  // 字符串连接
                        std::string str1, str2;
                        std::cout << "请输入第一个字符串: ";
                        std::cin >> str1;
                        std::cout << "请输入第二个字符串: ";
                        std::cin >> str2;
                        std::string result = client.call<std::string>("concat", str1, str2);
                        std::cout << "结果: \"" << result << "\"" << std::endl;
                        break;
                    }

                    case 16: {  // 字符串反转
                        std::string str;
                        std::cout << "请输入字符串: ";
                        std::cin >> str;
                        std::string result = client.call<std::string>("reverse", str);
                        std::cout << "结果: \"" << result << "\"" << std::endl;
                        break;
                    }

                    case 17: {  // 转大写
                        std::string str;
                        std::cout << "请输入字符串: ";
                        std::cin >> str;
                        std::string result = client.call<std::string>("toUpper", str);
                        std::cout << "结果: \"" << result << "\"" << std::endl;
                        break;
                    }

                    case 18: {  // 转小写
                        std::string str;
                        std::cout << "请输入字符串: ";
                        std::cin >> str;
                        std::string result = client.call<std::string>("toLower", str);
                        std::cout << "结果: \"" << result << "\"" << std::endl;
                        break;
                    }

                    case 19: {  // 字符串长度
                        std::string str;
                        std::cout << "请输入字符串: ";
                        std::cin >> str;
                        int result = client.call<int>("length", str);
                        std::cout << "结果: 长度 = " << result << std::endl;
                        break;
                    }

                    case 20:  // 性能测试
                        performanceTest(client);
                        break;

                    case 21:  // 异步调用示例
                        asyncCallExample(client);
                        break;

                    case 22:  // 批量调用示例
                        batchCallExample(client);
                        break;

                    case 23:  // 服务发现
                        serviceDiscovery(client);
                        break;

                    case 24:  // 健康检查
                        healthCheck(client);
                        break;

                    default:
                        std::cout << "无效选择！" << std::endl;
                        break;
                }
            } catch (const RpcException& e) {
                std::cerr << "RPC错误: " << e.what() << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "错误: " << e.what() << std::endl;
            }
        }

        // 断开连接
        client.disconnect();

    } catch (const std::exception& e) {
        std::cerr << "客户端异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n客户端已退出。" << std::endl;
    return 0;
}