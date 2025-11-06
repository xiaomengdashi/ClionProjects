#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t socket(ctx, zmq::socket_type::rep);
    socket.bind("tcp://*:5555");

    std::cout << "[Server] 等待客户端请求..." << std::endl;

    while (true) {
        // 接收请求
        zmq::message_t request;
        auto result = socket.recv(request, zmq::recv_flags::none);

        if (!result) break;

        std::string msg(static_cast<char*>(request.data()), request.size());
        std::cout << "[Server] 收到请求: " << msg << std::endl;

        // 处理请求（延迟1秒模拟处理）
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 发送应答
        std::string reply = "已处理: " + msg;
        socket.send(zmq::buffer(reply), zmq::send_flags::none);
        std::cout << "[Server] 发送应答: " << reply << std::endl;
    }

    return 0;
}
