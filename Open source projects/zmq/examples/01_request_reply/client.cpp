#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t socket(ctx, zmq::socket_type::req);
    socket.connect("tcp://localhost:5555");

    std::cout << "[Client] 连接到服务器" << std::endl;

    // 发送5个请求
    for (int i = 1; i <= 5; i++) {
        std::string request = "请求数据" + std::to_string(i);

        std::cout << "[Client] 发送: " << request << std::endl;
        socket.send(zmq::buffer(request), zmq::send_flags::none);

        // 接收应答
        zmq::message_t reply;
        auto result = socket.recv(reply, zmq::recv_flags::none);

        if (result) {
            std::string reply_str(static_cast<char*>(reply.data()), reply.size());
            std::cout << "[Client] 收到: " << reply_str << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
