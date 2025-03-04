#include "msg_que.h"
#include <gtest/gtest.h>
#include <thread>

using namespace std;

// 测试消息队列的基本功能
TEST(MessageQueueTest, BasicFunctionality) {
    msg_que queue("test_queue");
    elements msg{};
    memcpy(msg.msg, "Hello, World!", sizeof("Hello, World!"));

    // 发送消息
    queue.send_msg(&msg);

    // 接收消息
    elements received_msg;
    ASSERT_TRUE(queue.recv_msg(&received_msg));
    EXPECT_STREQ(reinterpret_cast<const char*>(received_msg.msg), "Hello, World!"); // 确保 received_msg.msg 是 const char*
}

// 测试消息队列的阻塞和非阻塞模式
TEST(MessageQueueTest, BlockingAndNonBlocking) {
    msg_que blocking_queue("blocking_queue", 1); // 阻塞模式
    msg_que non_blocking_queue("non_blocking_queue", 0); // 非阻塞模式

    elements msg{};
    memcpy(msg.msg, "Test Message", sizeof("Test Message"));

    // 发送消息到阻塞队列
    blocking_queue.send_msg(&msg);

    // 接收消息，应该阻塞直到消息可用
    elements received_msg;
    ASSERT_TRUE(blocking_queue.recv_msg(&received_msg));
    EXPECT_STREQ(reinterpret_cast<const char*>(received_msg.msg), "Test Message");

    // 发送消息到非阻塞队列
    non_blocking_queue.send_msg(&msg);

    // 接收消息，应该立即返回，因为是非阻塞模式
    ASSERT_TRUE(non_blocking_queue.recv_msg(&received_msg));
    EXPECT_STREQ(reinterpret_cast<const char*>(received_msg.msg), "Test Message");

    // 再次尝试接收消息，应该返回 false，因为队列中没有消息
    ASSERT_FALSE(non_blocking_queue.recv_msg(&received_msg));
}

// 测试消息队列的多线程功能
TEST(MessageQueueTest, MultiThreading) {
    msg_que queue("multi_thread_queue");
    atomic<bool> done{false};

    // 生产者线程
    thread producer([&] {
        elements msg{};
        memcpy(msg.msg, "Multi-threaded Message", sizeof("Multi-threaded Message"));
        queue.send_msg(&msg);
        done = true;
    });

    // 消费者线程
    thread consumer([&] {
        elements received_msg;
        while (!done) {
            if (queue.recv_msg(&received_msg)) {
                EXPECT_STREQ(reinterpret_cast<const char*>(received_msg.msg), "Multi-threaded Message");
                break;
            }
            this_thread::sleep_for(chrono::milliseconds(100));
        }
    });

    producer.join();
    consumer.join();
}

// 测试消息队列的一对多功能
TEST(MessageQueueTest, OneToMany) {
    const int num_consumers = 5; // 定义消费者数量
    msg_que queue("one_to_many_queue");
    atomic<int> received_count{0}; // 用于统计接收到的消息数量

    // 生产者线程
    thread producer([&] {
        elements msg{};
        memcpy(msg.msg, "One-to-Many Message", sizeof("One-to-Many Message"));
        queue.send_msg("consumer", &msg); // 发送消息到指定的消费者
    });

    // 消费者线程
    vector<thread> consumers;
    for (int i = 0; i < num_consumers; ++i) {
        consumers.emplace_back([&] {
            elements received_msg;
            if (queue.recv_msg("consumer", &received_msg)) { // 接收指定消费者的消息
                EXPECT_STREQ(reinterpret_cast<const char*>(received_msg.msg), "One-to-Many Message");
                received_count.fetch_add(1, memory_order_relaxed);
            }
        });
    }

    producer.join();
    for (auto& consumer : consumers) {
        consumer.join();
    }

    // 确保所有消费者都接收到了消息
    EXPECT_EQ(received_count.load(memory_order_relaxed), num_consumers);
}

   

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}