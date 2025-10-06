#include <iostream>
#include <memory>
#include <thread>
#include <chrono>
#include <string>
#include <vector>
#include <random>
#include "message_queue.h"
#include "publisher.h"
#include "sample_subscribers.h"

// 测试场景1：广播模式测试
void testBroadcastMode() {
    std::cout << "\n############## 测试场景1：广播模式 ##############\n" << std::endl;

    // 创建消息队列（4个工作线程）
    auto queue = std::make_shared<MessageQueue>(4);

    // 创建三个控制台订阅者
    auto subscriber1 = std::make_shared<ConsoleSubscriber>("订阅者1", "sub_001");
    auto subscriber2 = std::make_shared<ConsoleSubscriber>("订阅者2", "sub_002");
    auto subscriber3 = std::make_shared<ConsoleSubscriber>("订阅者3", "sub_003");

    // 订阅同一主题（广播模式）
    queue->subscribe("news", subscriber1, DistributionStrategy::BROADCAST);
    queue->subscribe("news", subscriber2, DistributionStrategy::BROADCAST);
    queue->subscribe("news", subscriber3, DistributionStrategy::BROADCAST);

    // 创建发布者
    Publisher publisher(queue, "publisher_001");

    // 发布几条消息
    std::cout << "发布者开始发送广播消息...\n" << std::endl;
    publisher.publish("news", std::string("重要新闻：系统更新完成"), Message::Priority::HIGH);
    publisher.publish("news", std::string("普通消息：今天天气晴朗"), Message::Priority::NORMAL);
    publisher.publish("news", std::string("紧急通知：维护即将开始"), Message::Priority::URGENT);

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n广播模式测试完成！每个订阅者都收到了所有消息。\n" << std::endl;
}

// 测试场景2：轮询模式测试
void testRoundRobinMode() {
    std::cout << "\n############## 测试场景2：轮询模式 ##############\n" << std::endl;

    // 创建消息队列
    auto queue = std::make_shared<MessageQueue>(2);

    // 创建三个订阅者
    auto subscriber1 = std::make_shared<ConsoleSubscriber>("工作者1", "worker_001");
    auto subscriber2 = std::make_shared<ConsoleSubscriber>("工作者2", "worker_002");
    auto subscriber3 = std::make_shared<ConsoleSubscriber>("工作者3", "worker_003");

    // 订阅同一主题（轮询模式）
    queue->subscribe("tasks", subscriber1, DistributionStrategy::ROUND_ROBIN);
    queue->subscribe("tasks", subscriber2, DistributionStrategy::ROUND_ROBIN);
    queue->subscribe("tasks", subscriber3, DistributionStrategy::ROUND_ROBIN);

    // 创建发布者
    Publisher publisher(queue, "task_dispatcher");

    // 发布多条任务消息
    std::cout << "发布者开始发送任务（轮询分发）...\n" << std::endl;
    for (int i = 1; i <= 6; ++i) {
        std::string task = "任务 #" + std::to_string(i);
        publisher.publish("tasks", task, Message::Priority::NORMAL);
    }

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n轮询模式测试完成！每个工作者处理了不同的任务。\n" << std::endl;
}

// 测试场景3：随机模式测试
void testRandomMode() {
    std::cout << "\n############## 测试场景3：随机模式 ##############\n" << std::endl;

    // 创建消息队列
    auto queue = std::make_shared<MessageQueue>(2);

    // 创建统计订阅者
    auto stats1 = std::make_shared<StatisticsSubscriber>("统计订阅者1", "stats_001");
    auto stats2 = std::make_shared<StatisticsSubscriber>("统计订阅者2", "stats_002");
    auto stats3 = std::make_shared<StatisticsSubscriber>("统计订阅者3", "stats_003");

    // 订阅同一主题（随机模式）
    queue->subscribe("events", stats1, DistributionStrategy::RANDOM);
    queue->subscribe("events", stats2, DistributionStrategy::RANDOM);
    queue->subscribe("events", stats3, DistributionStrategy::RANDOM);

    // 创建发布者
    Publisher publisher(queue, "event_source");

    // 发布大量消息
    std::cout << "发布者开始发送事件（随机分发）...\n" << std::endl;
    for (int i = 1; i <= 30; ++i) {
        std::string event = "事件 #" + std::to_string(i);
        Message::Priority priority = static_cast<Message::Priority>(rand() % 4);
        publisher.publish("events", event, priority);
    }

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // 打印统计信息
    std::cout << "\n随机模式测试完成！统计结果：\n" << std::endl;
    stats1->printStatistics();
    stats2->printStatistics();
    stats3->printStatistics();
}

// 测试场景4：多主题订阅测试
void testMultipleTopics() {
    std::cout << "\n############## 测试场景4：多主题订阅 ##############\n" << std::endl;

    // 创建消息队列
    auto queue = std::make_shared<MessageQueue>(4);

    // 创建文件订阅者
    auto fileLogger = std::make_shared<FileSubscriber>("日志记录器", "logger_001", "message_log.txt");

    // 创建控制台订阅者
    auto errorHandler = std::make_shared<ConsoleSubscriber>("错误处理器", "error_001");
    auto infoHandler = std::make_shared<ConsoleSubscriber>("信息处理器", "info_001");

    // 订阅不同主题
    queue->subscribe("logs", fileLogger, DistributionStrategy::BROADCAST);
    queue->subscribe("errors", errorHandler, DistributionStrategy::BROADCAST);
    queue->subscribe("errors", fileLogger, DistributionStrategy::BROADCAST);  // 文件也记录错误
    queue->subscribe("info", infoHandler, DistributionStrategy::BROADCAST);

    // 创建发布者
    Publisher publisher(queue, "multi_topic_pub");

    std::cout << "发布者开始向不同主题发送消息...\n" << std::endl;

    // 发布不同主题的消息
    publisher.publish("logs", std::string("系统启动"), Message::Priority::LOW);
    publisher.publish("errors", std::string("错误：连接超时"), Message::Priority::HIGH);
    publisher.publish("info", std::string("信息：用户登录成功"), Message::Priority::NORMAL);
    publisher.publish("logs", std::string("操作完成"), Message::Priority::LOW);
    publisher.publish("errors", std::string("错误：文件未找到"), Message::Priority::URGENT);

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n多主题订阅测试完成！不同订阅者处理了相应主题的消息。\n" << std::endl;
}

// 测试场景5：过滤器订阅者测试
void testFilterSubscriber() {
    std::cout << "\n############## 测试场景5：消息过滤 ##############\n" << std::endl;

    // 创建消息队列
    auto queue = std::make_shared<MessageQueue>(2);

    // 创建基础订阅者
    auto baseSubscriber = std::make_shared<ConsoleSubscriber>("高优先级处理器", "high_priority_001");

    // 创建过滤器订阅者（只处理高优先级和紧急优先级消息）
    auto filterSubscriber = std::make_shared<FilterSubscriber>(
        "优先级过滤器", "filter_001",
        [](const std::shared_ptr<Message>& msg) {
            return msg->getPriority() >= Message::Priority::HIGH;
        },
        baseSubscriber
    );

    // 订阅主题
    queue->subscribe("filtered_events", filterSubscriber, DistributionStrategy::BROADCAST);

    // 创建发布者
    Publisher publisher(queue, "filter_test_pub");

    std::cout << "发布者开始发送不同优先级的消息...\n" << std::endl;

    // 发布不同优先级的消息
    publisher.publish("filtered_events", std::string("低优先级任务"), Message::Priority::LOW);
    publisher.publish("filtered_events", std::string("普通优先级任务"), Message::Priority::NORMAL);
    publisher.publish("filtered_events", std::string("高优先级任务"), Message::Priority::HIGH);
    publisher.publish("filtered_events", std::string("紧急任务"), Message::Priority::URGENT);
    publisher.publish("filtered_events", std::string("另一个低优先级任务"), Message::Priority::LOW);

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(2));

    std::cout << "\n过滤器测试完成！"
              << "过滤掉的消息数：" << filterSubscriber->getFilteredCount()
              << "，处理的消息数：" << filterSubscriber->getProcessedCount() << "\n" << std::endl;
}

// 测试场景6：性能压力测试
void testPerformance() {
    std::cout << "\n############## 测试场景6：性能压力测试 ##############\n" << std::endl;

    // 创建消息队列（8个工作线程）
    auto queue = std::make_shared<MessageQueue>(8);

    // 创建统计订阅者
    std::vector<std::shared_ptr<StatisticsSubscriber>> subscribers;
    for (int i = 0; i < 5; ++i) {
        auto subscriber = std::make_shared<StatisticsSubscriber>(
            "统计订阅者" + std::to_string(i),
            "perf_sub_" + std::to_string(i)
        );
        subscribers.push_back(subscriber);
        queue->subscribe("performance", subscriber, DistributionStrategy::RANDOM);
    }

    // 创建多个发布者线程
    std::vector<std::thread> publisherThreads;
    std::atomic<size_t> totalPublished(0);

    auto start = std::chrono::steady_clock::now();

    std::cout << "启动5个发布者线程，每个发送1000条消息...\n" << std::endl;

    for (int t = 0; t < 5; ++t) {
        publisherThreads.emplace_back([queue, t, &totalPublished]() {
            Publisher publisher(queue, "perf_pub_" + std::to_string(t));

            for (int i = 0; i < 1000; ++i) {
                std::string data = "性能测试消息 " + std::to_string(i);
                Message::Priority priority = static_cast<Message::Priority>(i % 4);
                publisher.publish("performance", data, priority);
                totalPublished++;
            }
        });
    }

    // 等待所有发布者完成
    for (auto& t : publisherThreads) {
        t.join();
    }

    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(3));

    auto end = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    std::cout << "\n性能测试完成！\n"
              << "总发布消息数：" << totalPublished.load() << "\n"
              << "总处理消息数：" << queue->getTotalMessagesProcessed() << "\n"
              << "总耗时：" << duration.count() << " ms\n"
              << "吞吐量：" << (totalPublished.load() * 1000 / duration.count()) << " 消息/秒\n"
              << std::endl;

    // 打印每个订阅者的统计信息
    for (const auto& subscriber : subscribers) {
        subscriber->printStatistics();
    }
}

// 测试场景7：动态订阅/取消订阅
void testDynamicSubscription() {
    std::cout << "\n############## 测试场景7：动态订阅/取消订阅 ##############\n" << std::endl;

    // 创建消息队列
    auto queue = std::make_shared<MessageQueue>(2);

    // 创建订阅者
    auto subscriber1 = std::make_shared<ConsoleSubscriber>("动态订阅者1", "dynamic_001");
    auto subscriber2 = std::make_shared<ConsoleSubscriber>("动态订阅者2", "dynamic_002");

    // 创建发布者
    Publisher publisher(queue, "dynamic_pub");

    // 第一阶段：只有订阅者1
    std::cout << "阶段1：只有订阅者1订阅\n" << std::endl;
    queue->subscribe("dynamic_topic", subscriber1, DistributionStrategy::BROADCAST);
    publisher.publish("dynamic_topic", std::string("消息1：只有订阅者1能收到"));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 第二阶段：添加订阅者2
    std::cout << "\n阶段2：订阅者2加入\n" << std::endl;
    queue->subscribe("dynamic_topic", subscriber2, DistributionStrategy::BROADCAST);
    publisher.publish("dynamic_topic", std::string("消息2：两个订阅者都能收到"));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 第三阶段：取消订阅者1
    std::cout << "\n阶段3：订阅者1取消订阅\n" << std::endl;
    queue->unsubscribe("dynamic_topic", subscriber1->getId());
    publisher.publish("dynamic_topic", std::string("消息3：只有订阅者2能收到"));
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "\n动态订阅测试完成！\n" << std::endl;
}

// 主函数
int main() {
    std::cout << "===================================================\n"
              << "        C++ 消息队列框架 - 功能演示程序\n"
              << "===================================================\n" << std::endl;

    try {
        // 运行所有测试场景
        testBroadcastMode();
        testRoundRobinMode();
        testRandomMode();
        testMultipleTopics();
        testFilterSubscriber();
        testDynamicSubscription();
        testPerformance();

        std::cout << "\n===================================================\n"
                  << "              所有测试完成！\n"
                  << "===================================================\n" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常：" << e.what() << std::endl;
        return 1;
    }

    return 0;
}