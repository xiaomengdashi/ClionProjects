#include <zmq.hpp>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>

int main() {
    zmq::context_t ctx(1);
    zmq::socket_t socket(ctx, zmq::socket_type::req);
    socket.connect("ipc:///tmp/zmq_ipc.sock");

    std::cout << "[IPC Client] 连接到 ipc:///tmp/zmq_ipc.sock" << std::endl;

    for (int i = 1; i <= 3; i++) {
        std::string request = "IPC请求" + std::to_string(i);

        std::cout << "[IPC Client] 发送: " << request << std::endl;
        socket.send(zmq::buffer(request), zmq::send_flags::none);

        zmq::message_t reply;
        auto result = socket.recv(reply, zmq::recv_flags::none);

        if (result) {
            std::string reply_str(static_cast<char*>(reply.data()), reply.size());
            std::cout << "[IPC Client] 收到: " << reply_str << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
}
