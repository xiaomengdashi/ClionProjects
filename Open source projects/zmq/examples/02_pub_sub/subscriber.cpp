#include <zmq.hpp>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "用法: " << argv[0] << " <topic>" << std::endl;
        std::cerr << "示例: " << argv[0] << " weather" << std::endl;
        return 1;
    }

    std::string topic = argv[1];

    zmq::context_t ctx(1);
    zmq::socket_t subscriber(ctx, zmq::socket_type::sub);
    subscriber.connect("tcp://localhost:5556");

    // 订阅指定的主题
    subscriber.set(zmq::sockopt::subscribe, topic);

    std::cout << "[Subscriber] 已连接，订阅主题: " << topic << std::endl;

    int msg_count = 0;
    while (msg_count < 10) {
        zmq::message_t message;
        auto result = subscriber.recv(message, zmq::recv_flags::none);

        if (!result) break;

        std::string msg_str(static_cast<char*>(message.data()), message.size());
        std::cout << "[Subscriber] 收到: " << msg_str << std::endl;

        msg_count++;
    }

    std::cout << "[Subscriber] 已接收10条消息，退出" << std::endl;

    return 0;
}
