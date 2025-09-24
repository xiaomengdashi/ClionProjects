/**
 * 简单的RPC测试程序
 */

#include <iostream>
#include <thread>
#include <chrono>
#include <mutex>
#include "../include/stdrpc.hpp"

using namespace stdrpc;

/**
 * 简单的服务器线程
 */
void serverThread() {
    try {
        // 创建服务器
        RpcServer server(9999);

        // 注册简单的服务
        server.registerFunction<int, int, int>("add",
            std::function<int(int, int)>([](int a, int b) {
                std::cout << "[服务器] 执行 add(" << a << ", " << b << ")" << std::endl;
                return a + b;
            }));

        server.registerFunction<std::string, std::string>("echo",
            std::function<std::string(std::string)>([](const std::string& msg) {
                std::cout << "[服务器] 执行 echo(\"" << msg << "\")" << std::endl;
                return "Echo: " + msg;
            }));

        server.registerFunction("print",
            std::function<void(std::string)>([](const std::string& msg) {
                std::cout << "[服务器] 打印消息: " << msg << std::endl;
            }));

        // 启动服务器
        std::cout << "[服务器] 启动在端口 9999" << std::endl;
        server.start();

        // 运行30秒，确保所有测试完成
        std::this_thread::sleep_for(std::chrono::seconds(30));

        // 停止服务器
        server.stop();

    } catch (const std::exception& e) {
        std::cerr << "[服务器] 错误: " << e.what() << std::endl;
    }
}

/**
 * 简单的客户端线程
 */
void clientThread() {
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(1));

    try {
        // 创建客户端
        RpcClient client("127.0.0.1", 9999);

        // 连接服务器
        std::cout << "[客户端] 连接服务器..." << std::endl;
        if (!client.connect()) {
            std::cerr << "[客户端] 连接失败!" << std::endl;
            return;
        }

        // 测试加法
        std::cout << "\n=== 测试加法 ===" << std::endl;
        int result1 = client.call<int>("add", 10, 20);
        std::cout << "[客户端] 10 + 20 = " << result1 << std::endl;

        int result2 = client.call<int>("add", 100, 200);
        std::cout << "[客户端] 100 + 200 = " << result2 << std::endl;

        // 测试字符串回显
        std::cout << "\n=== 测试回显 ===" << std::endl;
        std::string echo1 = client.call<std::string>("echo", "Hello, World!");
        std::cout << "[客户端] 收到: \"" << echo1 << "\"" << std::endl;

        std::string echo2 = client.call<std::string>("echo", "StdRPC Framework");
        std::cout << "[客户端] 收到: \"" << echo2 << "\"" << std::endl;

        // 测试无返回值的函数
        std::cout << "\n=== 测试打印 ===" << std::endl;
        client.callVoid("print", "这是一条测试消息");
        client.callVoid("print", "RPC调用成功！");

        // 测试异步调用
        std::cout << "\n=== 测试异步调用 ===" << std::endl;
        auto future1 = client.asyncCall<int>("add", 1, 2);
        auto future2 = client.asyncCall<int>("add", 3, 4);
        auto future3 = client.asyncCall<int>("add", 5, 6);

        std::cout << "[客户端] 等待异步结果..." << std::endl;
        std::cout << "[客户端] 1 + 2 = " << future1.get() << std::endl;
        std::cout << "[客户端] 3 + 4 = " << future2.get() << std::endl;
        std::cout << "[客户端] 5 + 6 = " << future3.get() << std::endl;

        // 测试错误处理
        std::cout << "\n=== 测试错误处理 ===" << std::endl;
        try {
            // 调用不存在的方法
            client.call<int>("nonexistent", 1, 2);
        } catch (const RpcException& e) {
            std::cout << "[客户端] 预期的错误: " << e.what() << std::endl;
        }

        // 断开连接
        client.disconnect();
        std::cout << "[客户端] 已断开连接" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "[客户端] 错误: " << e.what() << std::endl;
    }
}

/**
 * 主函数
 */
int main() {
    std::cout << "========================================" << std::endl;
    std::cout << "      简单测试程序" << std::endl;
    std::cout << "========================================" << std::endl;
    std::cout << "版本: " << stdrpc::getVersion() << std::endl << std::endl;

    // 启动服务器线程
    std::thread server(serverThread);

    // 启动客户端线程
    std::thread client(clientThread);

    // 等待线程结束
    client.join();
    server.join();

    std::cout << "\n测试完成！" << std::endl;
    return 0;
}