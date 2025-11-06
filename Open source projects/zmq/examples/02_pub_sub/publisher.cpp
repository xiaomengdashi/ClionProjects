#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <random>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t publisher(ctx, zmq::socket_type::pub);
    publisher.bind("tcp://*:5556");

    std::cout << "[Publisher] 已启动，开始发布消息" << std::endl;

    // 等待订阅者连接
    std::this_thread::sleep_for(std::chrono::seconds(1));

    int counter = 0;
    while (counter < 10) {
        // 发布不同主题的消息
        std::string topic1_msg = "weather 晴天，温度" + std::to_string(rand() % 30 + 10) + "度";
        std::string topic2_msg = "stock AAPL " + std::to_string(rand() % 100 + 100) + "美元";
        std::string topic3_msg = "news 最新" + std::to_string(rand() % 10 + 1) + " 新闻播报";

        publisher.send(zmq::buffer(topic1_msg), zmq::send_flags::none);
        std::cout << "[Publisher] 发送: " << topic1_msg << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        publisher.send(zmq::buffer(topic2_msg), zmq::send_flags::none);
        std::cout << "[Publisher] 发送: " << topic2_msg << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        publisher.send(zmq::buffer(topic3_msg), zmq::send_flags::none);
        std::cout << "[Publisher] 发送: " << topic3_msg << std::endl;

        std::this_thread::sleep_for(std::chrono::seconds(1));

        counter++;
    }

    return 0;
}
