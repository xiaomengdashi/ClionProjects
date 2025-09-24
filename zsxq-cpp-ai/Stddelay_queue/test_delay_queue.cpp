#include "delay_queue.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>

// 获取当前时间的字符串表示
std::string getCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%H:%M:%S");
    ss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

// 测试任务结构体
struct Task {
    int id;
    std::string name;

    Task(int i, const std::string& n) : id(i), name(n) {}
};

/**
 * @brief 测试基本的延迟队列功能
 */
void testBasicOperations() {
    std::cout << "\n=== 测试基本操作 ===" << std::endl;

    // 创建一个存储字符串的延迟队列
    DelayQueue<std::string> queue;

    // 添加几个带不同延迟的元素
    std::cout << "[" << getCurrentTime() << "] 添加元素..." << std::endl;
    queue.put("立即过期", 0);                    // 立即过期
    queue.put("1秒后过期", 1000);               // 1秒后过期
    queue.put("2秒后过期", 2000);               // 2秒后过期
    queue.put("500毫秒后过期", 500);            // 500毫秒后过期

    std::cout << "队列大小: " << queue.size() << std::endl;
    std::cout << "队首元素延迟: " << queue.peekDelay() << " ms" << std::endl;

    // 尝试非阻塞获取
    std::string result;
    if (queue.tryTake(result)) {
        std::cout << "[" << getCurrentTime() << "] 非阻塞获取: " << result << std::endl;
    }

    // 等待并获取其他元素
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    if (queue.tryTake(result)) {
        std::cout << "[" << getCurrentTime() << "] 获取: " << result << std::endl;
    }

    // 阻塞获取剩余元素
    while (queue.take(result)) {
        std::cout << "[" << getCurrentTime() << "] 阻塞获取: " << result << std::endl;
        if (queue.empty()) break;
    }
}

/**
 * @brief 测试多线程生产者-消费者场景
 */
void testProducerConsumer() {
    std::cout << "\n=== 测试生产者-消费者模式 ===" << std::endl;

    DelayQueue<Task> taskQueue;
    std::atomic<bool> stopFlag(false);

    // 生产者线程 - 定期添加任务
    std::thread producer([&taskQueue, &stopFlag]() {
        int taskId = 1;
        for (int i = 0; i < 5; ++i) {
            // 添加不同延迟的任务
            int delay = (i % 3 + 1) * 500;  // 500ms, 1000ms, 1500ms

            std::stringstream taskName;
            taskName << "任务-" << taskId;

            taskQueue.put(Task(taskId, taskName.str()), delay);

            std::cout << "[" << getCurrentTime() << "] [生产者] 添加 "
                     << taskName.str() << " (延迟 " << delay << " ms)" << std::endl;

            taskId++;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        }

        // 等待所有任务处理完成
        std::this_thread::sleep_for(std::chrono::seconds(3));
        stopFlag = true;
        taskQueue.shutdown();  // 关闭队列，唤醒消费者
    });

    // 消费者线程 - 处理到期的任务
    std::thread consumer([&taskQueue, &stopFlag]() {
        while (!stopFlag || !taskQueue.empty()) {
            Task task(0, "");

            // 使用带超时的获取，避免无限等待
            if (taskQueue.take(task, 100)) {
                std::cout << "[" << getCurrentTime() << "] [消费者] 处理 "
                         << task.name << " (ID: " << task.id << ")" << std::endl;

                // 模拟任务处理
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            }
        }
    });

    // 等待线程结束
    producer.join();
    consumer.join();

    std::cout << "生产者-消费者测试完成" << std::endl;
}

/**
 * @brief 测试批量处理功能
 */
void testBatchProcessing() {
    std::cout << "\n=== 测试批量处理 ===" << std::endl;

    DelayQueue<int> queue;

    // 添加多个元素，部分立即过期，部分延迟过期
    std::cout << "添加10个元素..." << std::endl;
    for (int i = 1; i <= 10; ++i) {
        int delay = (i <= 5) ? 0 : 1000;  // 前5个立即过期，后5个延迟1秒
        queue.put(i, delay);
    }

    std::cout << "队列大小: " << queue.size() << std::endl;

    // 批量获取已过期的元素
    auto expired = queue.drainExpired(3);  // 最多获取3个
    std::cout << "第一次批量获取 (最多3个): ";
    for (int val : expired) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    // 再次批量获取
    expired = queue.drainExpired();  // 获取所有已过期的
    std::cout << "第二次批量获取 (所有已过期): ";
    for (int val : expired) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "剩余队列大小: " << queue.size() << std::endl;

    // 等待1秒后再次批量获取
    std::this_thread::sleep_for(std::chrono::seconds(1));
    expired = queue.drainExpired();
    std::cout << "1秒后批量获取: ";
    for (int val : expired) {
        std::cout << val << " ";
    }
    std::cout << std::endl;

    std::cout << "最终队列大小: " << queue.size() << std::endl;
}

/**
 * @brief 测试定时任务调度场景
 */
void testScheduledTasks() {
    std::cout << "\n=== 测试定时任务调度 ===" << std::endl;

    // 定义一个定时任务类型
    struct ScheduledTask {
        std::string name;
        std::function<void()> action;

        ScheduledTask() = default;
        ScheduledTask(const std::string& n, std::function<void()> a)
            : name(n), action(a) {}
    };

    DelayQueue<ScheduledTask> scheduler;

    // 添加几个定时任务
    std::cout << "[" << getCurrentTime() << "] 添加定时任务..." << std::endl;

    scheduler.put(ScheduledTask("任务A", []() {
        std::cout << "[" << getCurrentTime() << "] 执行任务A: 打印日志" << std::endl;
    }), 500);

    scheduler.put(ScheduledTask("任务B", []() {
        std::cout << "[" << getCurrentTime() << "] 执行任务B: 清理缓存" << std::endl;
    }), 1000);

    scheduler.put(ScheduledTask("任务C", []() {
        std::cout << "[" << getCurrentTime() << "] 执行任务C: 发送心跳" << std::endl;
    }), 1500);

    scheduler.put(ScheduledTask("任务D", []() {
        std::cout << "[" << getCurrentTime() << "] 执行任务D: 数据同步" << std::endl;
    }), 800);

    // 创建调度线程
    std::thread schedulerThread([&scheduler]() {
        while (scheduler.isRunning()) {
            ScheduledTask task;
            if (scheduler.take(task, 2000)) {  // 使用2秒超时
                std::cout << "[" << getCurrentTime() << "] 调度器执行: "
                         << task.name << std::endl;
                task.action();  // 执行任务
            } else if (scheduler.empty()) {
                break;  // 队列为空且超时，退出
            }
        }
    });

    // 等待所有任务执行完成
    std::this_thread::sleep_for(std::chrono::seconds(2));
    scheduler.shutdown();
    schedulerThread.join();

    std::cout << "定时任务调度测试完成" << std::endl;
}

/**
 * @brief 测试使用chrono duration的API
 */
void testChronoDuration() {
    std::cout << "\n=== 测试chrono duration API ===" << std::endl;

    DelayQueue<std::string> queue;

    // 使用不同的时间单位添加元素
    queue.put("500毫秒", std::chrono::milliseconds(500));
    queue.put("1秒", std::chrono::seconds(1));
    queue.put("100毫秒", std::chrono::milliseconds(100));
    queue.put("1.5秒", std::chrono::milliseconds(1500));

    std::cout << "添加了4个元素，使用chrono duration指定延迟" << std::endl;

    // 获取元素
    std::string result;
    while (!queue.empty()) {
        if (queue.take(result, 2000)) {
            std::cout << "[" << getCurrentTime() << "] 获取: " << result << std::endl;
        }
    }
}

int main() {
    std::cout << "===== 延迟队列（Delay Queue）测试程序 =====" << std::endl;
    std::cout << "C++11 实现，支持线程安全的延迟元素管理" << std::endl;

    // 运行各项测试
    testBasicOperations();
    testProducerConsumer();
    testBatchProcessing();
    testScheduledTasks();
    testChronoDuration();

    std::cout << "\n===== 所有测试完成 =====" << std::endl;

    return 0;
}