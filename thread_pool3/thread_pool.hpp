
// thread_pool.hpp
#include <iostream>
#include <thread>
#include <utility>
#include <vector>
#include <functional>
#include <queue>
#include <cassert>
#include <mutex>
#include <condition_variable>


struct Task
{
    int _priority{0};
    std::function<void()> _execute_task;
    Task() = default;
    Task(std::function<void()> &&task) : _execute_task{task} {}
    Task(std::function<void()> &&task, int priority) : _execute_task{task}, _priority{priority} {}
};


struct Job
{
    Task _task{};
    time_t _creat_time{};
    bool operator<(const Job &other) const;
    Job(Task task) : _task{std::move(task)}
    {
        this->_creat_time = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    }
};


bool Job::operator<(const Job &other) const
{
    return this->_task._priority == other._task._priority ? this->_creat_time < other._creat_time : this->_task._priority < other._task._priority;
}


class ThreadPool
{
private:
    std::priority_queue<Job> _task_queue{};
    std::vector<std::thread> _thread_list{};
    std::mutex _thread_lock{};
    std::condition_variable _cv;
    std::atomic<bool> _is_stop{false};
    bool _is_started{false};
    size_t _thread_num{0};
    std::function<void()> _get_execute_func();
    void _add_thread_unsafe(size_t thread_num);

public:
    ThreadPool(ThreadPool &&other) = delete;
    ThreadPool(const ThreadPool &other) = delete;
    void operator=(ThreadPool &&) = delete;
    void start();
    void force_stop_gracefully();
    void force_stop();
    void add_task(std::function<void()> &&task, int priority);
    void add_task_unsafe(std::function<void()> &&task, int priority);
    void sync();
    void add_thread_safe(size_t thread_num);
    [[nodiscard]] size_t get_cur_thread_num() const;
    [[nodiscard]] size_t get_task_queue_size();
    ~ThreadPool();
    ThreadPool(size_t thread_num);
};

ThreadPool::ThreadPool(size_t thread_num) : _thread_num{thread_num} {}
void ThreadPool::start()
{
    if (_is_started)
        throw std::runtime_error("the thread pool already started...");
    _is_started = true;
    _add_thread_unsafe(_thread_num);
}

void ThreadPool::add_task(std::function<void()> &&task, int priority = 0)
{
    std::unique_lock<std::mutex> lock(_thread_lock);
    _task_queue.push({{std::move(task), priority}});
}

void ThreadPool::add_task_unsafe(std::function<void()> &&task, int priority = 0)
{
    _task_queue.push({{std::move(task), priority}});
}

void ThreadPool::sync()
{
    std::for_each(_thread_list.begin(), _thread_list.end(), [](std::thread &t)
                  { t.join(); });
}

void ThreadPool::force_stop_gracefully()
{
    _is_started = false;
    _is_stop = true;
    this->sync();
}

ThreadPool::~ThreadPool()
{
    if (!_is_stop)
        force_stop_gracefully();
}

size_t ThreadPool::get_task_queue_size()
{
    return this->_task_queue.size();
}

void ThreadPool::_add_thread_unsafe(size_t thread_num)
{
    for (int i = 0; i < thread_num; i++)
    {
        auto execute_func = _get_execute_func();
        _thread_list.emplace_back(execute_func);
    }
}

void ThreadPool::add_thread_safe(size_t thread_num)
{
    std::unique_lock<std::mutex> lock(_thread_lock);
    _thread_num += thread_num;
    for (int i = 0; i < thread_num; i++)
    {
        auto execute_func = _get_execute_func();
        _thread_list.emplace_back(std::move(execute_func));
    }
}

std::function<void()> ThreadPool::_get_execute_func()
{
    return [&]()
    {
        while (true)
        {
            if (_is_stop)
                return;
            std::unique_lock<std::mutex> lock(_thread_lock);
            while (_task_queue.empty())
            {
                _cv.wait(lock, [&]
                         { return !_task_queue.empty() || _is_stop; });
                if (_is_stop)
                    return;
            }
            auto job = _task_queue.top();
            _task_queue.pop();
            lock.unlock();
            try
            {
                job._task._execute_task();
            }
            catch (const std::exception &e)
            {
                std::cerr << "Caught an exception in ThreadPool: " << e.what() << std::endl;
            }
            catch (...)
            {
                std::cerr << "Caught an unknown exception in ThreadPool." << std::endl;
            }
        }
    };
}

size_t ThreadPool::get_cur_thread_num() const
{
    return _thread_num;
}

void ThreadPool::force_stop()
{
    _is_started = false;
    if (_is_stop)
        throw std::runtime_error("thread pool already shutdown!");
    for (auto &thread : _thread_list)
    {
        thread.detach();
    }
}

ThreadPool &get_thread_pool(size_t thread_num)
{
    static ThreadPool pool{thread_num};
    return pool;
}