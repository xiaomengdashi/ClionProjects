#pragma once


#include <queue>
#include <thread>
#include <vector>
#include <atomic>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <map>
#include <future>
#include <memory>



class Threadpool {
public:
    Threadpool(int min_thread_num = 2, int max_thread_num = std::thread::hardware_concurrency());
    ~Threadpool();
    void addTask(std::function<void(void)>);

    template<typename F, typename ... Args>
    auto addTask(F&& f, Args&& ... args)->std::future<typename std::result_of<F(Args...)>::type> {
        // 1. packaged_task
        using return_type = typename std::result_of<F(Args...)>::type;
        auto my_task = std::make_shared<std::packaged_task<return_type()>>(
                    std::bind(std::forward<F>(f), std::forward<Args>(args)...)
                );
        // 2. get future
        std::future<return_type> res = my_task->get_future();
        // 3. push task
        {
            std::lock_guard<std::mutex> lock(m_queue_mutex);
            m_task_queue.emplace([my_task]() {
                (*my_task)();
            });
        }

        m_cond.notify_one();

        return res;
    }

private:
    void manager(void);
    void worker(void);


private:
    // 管理则线程
    std::thread* m_manager;

    // 工作线程
    std::map<std::thread::id, std::thread> m_workers;
    std::vector<std::thread::id> m_thread_ids; // 已经退出的线程id

    std::atomic<int> m_max_thread_num;  // 最大线程数
    std::atomic<int> m_min_thread_num;  // 最小线程数
    std::atomic<int> m_busy_thread_num; // 忙碌线程数
    std::atomic<int> m_idle_thread_num; // 空闲线程数
    std::atomic<int> m_exit_thread_num; // 主动销毁的线程数
    std::atomic<bool> m_is_shutdown;



    // 任务队列
    std::queue<std::function<void(void)>> m_task_queue;

    // 线程同步
    std::mutex m_queue_mutex;
    std::mutex m_ids_mutex;
    std::condition_variable m_cond;
};

