#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <client_id>" << std::endl;
        return 1;
    }

    std::string client_id = argv[1];
    srand(time(NULL));

    zmq::context_t context(1);
    zmq::socket_t dealer(context, zmq::socket_type::dealer);

    // 设置客户端标识
    dealer.set(zmq::sockopt::routing_id, client_id);
    dealer.connect("tcp://localhost:5558");

    std::cout << "[Client-" << client_id << "] 已连接到服务器" << std::endl;

    // 发送5个请求
    for (int i = 1; i <= 5; i++) {
        std::string request = "Request-" + std::to_string(i) + "-from-" + client_id;

        std::cout << "[Client-" << client_id << "] 发送: " << request << std::endl;
        dealer.send(zmq::buffer(request), zmq::send_flags::none);

        // 接收响应
        zmq::message_t response_msg;
        dealer.recv(response_msg, zmq::recv_flags::none);
        std::string response = response_msg.to_string();

        std::cout << "[Client-" << client_id << "] 收到: " << response << std::endl;

        std::this_thread::sleep_for(std::chrono::milliseconds(500 + rand() % 500));
    }

    std::cout << "[Client-" << client_id << "] 已发送5个请求，退出" << std::endl;

    return 0;
}
