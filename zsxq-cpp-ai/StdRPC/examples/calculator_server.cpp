/**
 * RPC计算器服务器示例
 */

#include <iostream>
#include <signal.h>
#include <cmath>
#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>
#include "../include/stdrpc.hpp"

using namespace stdrpc;

// 全局服务器指针，用于优雅关闭
RpcServerExt* g_server = nullptr;
// 使用原子变量来安全地处理信号
std::atomic<bool> g_shutdown_requested{false};
std::atomic<int> g_last_signal{0};

/**
 * 信号处理函数
 * 注意：在信号处理函数中只能使用信号安全的操作
 */
void signalHandler(int signum) {
    // 不要在信号处理函数中使用 std::cout 或调用复杂函数
    // 只设置原子标志
    g_last_signal = signum;
    g_shutdown_requested = true;
}

/**
 * 计算器服务类
 * 封装各种数学运算
 */
class CalculatorService {
public:
    /**
     * 加法运算
     * @param a 第一个操作数
     * @param b 第二个操作数
     * @return 两数之和
     */
    static int add(int a, int b) {
        std::cout << "[计算器] 执行加法: " << a << " + " << b << " = " << (a + b) << std::endl;
        return a + b;
    }

    /**
     * 减法运算
     * @param a 被减数
     * @param b 减数
     * @return 两数之差
     */
    static int subtract(int a, int b) {
        std::cout << "[计算器] 执行减法: " << a << " - " << b << " = " << (a - b) << std::endl;
        return a - b;
    }

    /**
     * 乘法运算
     * @param a 第一个因数
     * @param b 第二个因数
     * @return 两数之积
     */
    static int multiply(int a, int b) {
        std::cout << "[计算器] 执行乘法: " << a << " * " << b << " = " << (a * b) << std::endl;
        return a * b;
    }

    /**
     * 除法运算
     * @param a 被除数
     * @param b 除数
     * @return 商
     */
    static double divide(double a, double b) {
        if (b == 0) {
            throw std::invalid_argument("除数不能为零");
        }
        double result = a / b;
        std::cout << "[计算器] 执行除法: " << a << " / " << b << " = " << result << std::endl;
        return result;
    }

    /**
     * 幂运算
     * @param base 底数
     * @param exponent 指数
     * @return base的exponent次方
     */
    static double power(double base, double exponent) {
        double result = std::pow(base, exponent);
        std::cout << "[计算器] 执行幂运算: " << base << " ^ " << exponent << " = " << result << std::endl;
        return result;
    }

    /**
     * 平方根运算
     * @param x 要计算平方根的数
     * @return x的平方根
     */
    static double sqrt(double x) {
        if (x < 0) {
            throw std::invalid_argument("不能计算负数的平方根");
        }
        double result = std::sqrt(x);
        std::cout << "[计算器] 执行平方根: √" << x << " = " << result << std::endl;
        return result;
    }

    /**
     * 计算数组的和
     * @param numbers 数字数组
     * @return 数组元素之和
     */
    static int sum(const std::vector<int>& numbers) {
        int result = std::accumulate(numbers.begin(), numbers.end(), 0);
        std::cout << "[计算器] 计算数组和，元素个数: " << numbers.size() << "，结果: " << result << std::endl;
        return result;
    }

    /**
     * 计算数组的平均值
     * @param numbers 数字数组
     * @return 平均值
     */
    static double average(const std::vector<double>& numbers) {
        if (numbers.empty()) {
            throw std::invalid_argument("数组不能为空");
        }
        double sum = std::accumulate(numbers.begin(), numbers.end(), 0.0);
        double avg = sum / numbers.size();
        std::cout << "[计算器] 计算平均值，元素个数: " << numbers.size() << "，结果: " << avg << std::endl;
        return avg;
    }

    /**
     * 查找最大值
     * @param numbers 数字数组
     * @return 最大值
     */
    static int max(const std::vector<int>& numbers) {
        if (numbers.empty()) {
            throw std::invalid_argument("数组不能为空");
        }
        int max_val = *std::max_element(numbers.begin(), numbers.end());
        std::cout << "[计算器] 查找最大值: " << max_val << std::endl;
        return max_val;
    }

    /**
     * 查找最小值
     * @param numbers 数字数组
     * @return 最小值
     */
    static int min(const std::vector<int>& numbers) {
        if (numbers.empty()) {
            throw std::invalid_argument("数组不能为空");
        }
        int min_val = *std::min_element(numbers.begin(), numbers.end());
        std::cout << "[计算器] 查找最小值: " << min_val << std::endl;
        return min_val;
    }

    /**
     * 阶乘运算
     * @param n 要计算阶乘的数
     * @return n的阶乘
     */
    static uint64_t factorial(int n) {
        if (n < 0) {
            throw std::invalid_argument("不能计算负数的阶乘");
        }
        if (n > 20) {
            throw std::invalid_argument("数字太大，可能溢出");
        }
        uint64_t result = 1;
        for (int i = 2; i <= n; ++i) {
            result *= i;
        }
        std::cout << "[计算器] 计算阶乘: " << n << "! = " << result << std::endl;
        return result;
    }

    /**
     * 判断是否为质数
     * @param n 要判断的数
     * @return 如果是质数返回true，否则返回false
     */
    static bool isPrime(int n) {
        if (n <= 1) return false;
        if (n <= 3) return true;
        if (n % 2 == 0 || n % 3 == 0) return false;

        for (int i = 5; i * i <= n; i += 6) {
            if (n % i == 0 || n % (i + 2) == 0) {
                std::cout << "[计算器] " << n << " 不是质数" << std::endl;
                return false;
            }
        }
        std::cout << "[计算器] " << n << " 是质数" << std::endl;
        return true;
    }

    /**
     * 计算最大公约数
     * @param a 第一个数
     * @param b 第二个数
     * @return 最大公约数
     */
    static int gcd(int a, int b) {
        a = std::abs(a);
        b = std::abs(b);
        while (b != 0) {
            int temp = b;
            b = a % b;
            a = temp;
        }
        std::cout << "[计算器] 最大公约数: " << a << std::endl;
        return a;
    }

    /**
     * 计算最小公倍数
     * @param a 第一个数
     * @param b 第二个数
     * @return 最小公倍数
     */
    static int lcm(int a, int b) {
        int g = gcd(a, b);
        int result = std::abs(a * b) / g;
        std::cout << "[计算器] 最小公倍数: " << result << std::endl;
        return result;
    }
};

/**
 * 字符串服务类
 * 提供字符串处理功能
 */
class StringService {
public:
    /**
     * 字符串连接
     * @param str1 第一个字符串
     * @param str2 第二个字符串
     * @return 连接后的字符串
     */
    static std::string concat(const std::string& str1, const std::string& str2) {
        std::string result = str1 + str2;
        std::cout << "[字符串] 连接: \"" << str1 << "\" + \"" << str2 << "\" = \"" << result << "\"" << std::endl;
        return result;
    }

    /**
     * 字符串反转
     * @param str 要反转的字符串
     * @return 反转后的字符串
     */
    static std::string reverse(const std::string& str) {
        std::string result(str.rbegin(), str.rend());
        std::cout << "[字符串] 反转: \"" << str << "\" -> \"" << result << "\"" << std::endl;
        return result;
    }

    /**
     * 转换为大写
     * @param str 要转换的字符串
     * @return 大写字符串
     */
    static std::string toUpper(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = std::toupper(c);
        }
        std::cout << "[字符串] 转大写: \"" << str << "\" -> \"" << result << "\"" << std::endl;
        return result;
    }

    /**
     * 转换为小写
     * @param str 要转换的字符串
     * @return 小写字符串
     */
    static std::string toLower(const std::string& str) {
        std::string result = str;
        for (char& c : result) {
            c = std::tolower(c);
        }
        std::cout << "[字符串] 转小写: \"" << str << "\" -> \"" << result << "\"" << std::endl;
        return result;
    }

    /**
     * 获取字符串长度
     * @param str 字符串
     * @return 字符串长度
     */
    static int length(const std::string& str) {
        int len = static_cast<int>(str.length());
        std::cout << "[字符串] 长度: \"" << str << "\" = " << len << std::endl;
        return len;
    }
};

/**
 * 注册所有服务
 */
void registerServices(RpcServerExt& server) {
    std::cout << "正在注册服务..." << std::endl;

    // 注册计算器服务
    server.registerFunctionWithDesc<int, int, int>("add",
        std::function<int(int, int)>(CalculatorService::add),
        "加法运算",
        "int a, int b - 两个整数",
        "int - 两数之和");

    server.registerFunctionWithDesc<int, int, int>("subtract",
        std::function<int(int, int)>(CalculatorService::subtract),
        "减法运算",
        "int a, int b - 被减数和减数",
        "int - 两数之差");

    server.registerFunctionWithDesc<int, int, int>("multiply",
        std::function<int(int, int)>(CalculatorService::multiply),
        "乘法运算",
        "int a, int b - 两个因数",
        "int - 两数之积");

    server.registerFunctionWithDesc<double, double, double>("divide",
        std::function<double(double, double)>(CalculatorService::divide),
        "除法运算",
        "double a, double b - 被除数和除数",
        "double - 商");

    server.registerFunctionWithDesc<double, double, double>("power",
        std::function<double(double, double)>(CalculatorService::power),
        "幂运算",
        "double base, double exponent - 底数和指数",
        "double - base的exponent次方");

    server.registerFunctionWithDesc<double, double>("sqrt",
        std::function<double(double)>(CalculatorService::sqrt),
        "平方根运算",
        "double x - 要计算平方根的数",
        "double - x的平方根");

    server.registerFunctionWithDesc<int, std::vector<int>>("sum",
        std::function<int(std::vector<int>)>(CalculatorService::sum),
        "计算数组和",
        "vector<int> numbers - 整数数组",
        "int - 数组元素之和");

    server.registerFunctionWithDesc<double, std::vector<double>>("average",
        std::function<double(std::vector<double>)>(CalculatorService::average),
        "计算平均值",
        "vector<double> numbers - 数字数组",
        "double - 平均值");

    server.registerFunctionWithDesc<int, std::vector<int>>("max",
        std::function<int(std::vector<int>)>(CalculatorService::max),
        "查找最大值",
        "vector<int> numbers - 整数数组",
        "int - 最大值");

    server.registerFunctionWithDesc<int, std::vector<int>>("min",
        std::function<int(std::vector<int>)>(CalculatorService::min),
        "查找最小值",
        "vector<int> numbers - 整数数组",
        "int - 最小值");

    server.registerFunctionWithDesc<uint64_t, int>("factorial",
        std::function<uint64_t(int)>(CalculatorService::factorial),
        "计算阶乘",
        "int n - 要计算阶乘的数",
        "uint64_t - n的阶乘");

    server.registerFunctionWithDesc<bool, int>("isPrime",
        std::function<bool(int)>(CalculatorService::isPrime),
        "判断是否为质数",
        "int n - 要判断的数",
        "bool - 如果是质数返回true");

    server.registerFunctionWithDesc<int, int, int>("gcd",
        std::function<int(int, int)>(CalculatorService::gcd),
        "计算最大公约数",
        "int a, int b - 两个整数",
        "int - 最大公约数");

    server.registerFunctionWithDesc<int, int, int>("lcm",
        std::function<int(int, int)>(CalculatorService::lcm),
        "计算最小公倍数",
        "int a, int b - 两个整数",
        "int - 最小公倍数");

    // 注册字符串服务
    server.registerFunctionWithDesc<std::string, std::string, std::string>("concat",
        std::function<std::string(std::string, std::string)>(StringService::concat),
        "字符串连接",
        "string str1, string str2 - 两个字符串",
        "string - 连接后的字符串");

    server.registerFunctionWithDesc<std::string, std::string>("reverse",
        std::function<std::string(std::string)>(StringService::reverse),
        "字符串反转",
        "string str - 要反转的字符串",
        "string - 反转后的字符串");

    server.registerFunctionWithDesc<std::string, std::string>("toUpper",
        std::function<std::string(std::string)>(StringService::toUpper),
        "转换为大写",
        "string str - 要转换的字符串",
        "string - 大写字符串");

    server.registerFunctionWithDesc<std::string, std::string>("toLower",
        std::function<std::string(std::string)>(StringService::toLower),
        "转换为小写",
        "string str - 要转换的字符串",
        "string - 小写字符串");

    server.registerFunctionWithDesc<int, std::string>("length",
        std::function<int(std::string)>(StringService::length),
        "获取字符串长度",
        "string str - 字符串",
        "int - 字符串长度");

    std::cout << "所有服务注册完成！" << std::endl;
}

int main(int argc, char** argv) {
    // 默认端口
    uint16_t port = 8080;

    // 解析命令行参数
    if (argc > 1) {
        port = static_cast<uint16_t>(std::atoi(argv[1]));
    }

    // 清理可能存在的残留信号
    signal(SIGINT, SIG_IGN);
    signal(SIGTERM, SIG_IGN);

    // 小延迟以确保信号被清理
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    // 注册信号处理
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    std::cout << "========================================" << std::endl;
    std::cout << "        计算器服务器" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "版本: " << stdrpc::getVersion() << std::endl;

    try {
        // 创建服务器
        RpcServerExt server(port, 4);
        g_server = &server;

        // 注册服务
        registerServices(server);

        // 启动服务器
        if (!server.start()) {
            std::cerr << "服务器启动失败！" << std::endl;
            return 1;
        }

        std::cout << "\n服务器已启动，监听端口: " << port << std::endl;
        std::cout << "按 Ctrl+C 停止服务器..." << std::endl;
        std::cout << "========================================\n" << std::endl;

        // 运行服务器，同时监控关闭请求
        // 不直接调用 run()，而是自己实现循环以便检查信号
        while (!g_shutdown_requested) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }

        // 收到关闭信号
        std::cout << "\n收到信号 " << g_last_signal.load()
                  << "，正在关闭服务器..." << std::endl;
        server.stop();

    } catch (const std::exception& e) {
        std::cerr << "服务器异常: " << e.what() << std::endl;
        return 1;
    }

    std::cout << "\n服务器已停止。" << std::endl;
    return 0;
}