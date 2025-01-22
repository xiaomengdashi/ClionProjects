//
// Created by Kolane on 2025/1/18.
//

#include "threadpool.h"
#include <iostream>



Threadpool::Threadpool(int min, int max)
: m_max_thread_num(max), m_min_thread_num(min), m_is_shutdown(false),
m_busy_thread_num(min), m_idle_thread_num(min) {
    // 创建管理者线程
    m_manager = new std::thread(&Threadpool::manager, this);

    // 创建工作线程
    for (int i = 0; i < m_busy_thread_num; i++) {
        std::thread t(&Threadpool::worker, this);
        m_workers.insert(std::make_pair(t.get_id(), move(t)));
    }
}

Threadpool::~Threadpool() {
    m_is_shutdown.store(true);
    m_cond.notify_all();

    for (auto& it : m_workers) {
        if (it.second.joinable()) {
            it.second.join();
            std::cout << "thread out: " << it.first << std::endl;
        }
    }

    if(m_manager->joinable()) {
        std::cout << "manager out: " << m_manager->get_id() << std::endl;
        m_manager->join();
    }

    delete m_manager;
}

void Threadpool::manager(void) {
    while (!m_is_shutdown.load()) {
        std::this_thread::sleep_for(std::chrono::seconds(1));


        int idle = m_idle_thread_num.load();
        int busy = m_busy_thread_num.load();
        std::cout << "idle: " << idle << " busy: " << busy << std::endl;
        if (idle > busy/2 && idle > m_min_thread_num) {
            // 销毁线程
            m_exit_thread_num.store(2);
            m_cond.notify_all();

            std::lock_guard<std::mutex> lock(m_ids_mutex);
            for (auto id : m_thread_ids) {
                auto it = m_workers.find(id);
                if (it != m_workers.end()) {
                    std::cout << "thread out: " << id << std::endl;
                    (*it).second.join();
                    m_workers.erase(it);
                }
            }
            m_thread_ids.clear();
        } else if (idle == 0 && busy < m_max_thread_num) {
            // 创建线程
            m_busy_thread_num++;
            m_idle_thread_num++;
            std::thread t(&Threadpool::worker, this);
            m_workers.insert(std::make_pair(t.get_id(), std::move(t)));
        }
    }
}

void Threadpool::worker() {
    while (!m_is_shutdown.load()) {
        std::function<void(void)> task = nullptr;
        {
            std::unique_lock<std::mutex> ulock(m_queue_mutex);
            while (m_task_queue.empty() && !m_is_shutdown.load()) {
                m_cond.wait(ulock);
                if (m_exit_thread_num.load() > 0) {
                    std::cout << "thread exit: " << std::this_thread::get_id() << std::endl;

                    m_exit_thread_num--;
                    m_busy_thread_num--;
                    m_idle_thread_num--;
                    std::lock_guard<std::mutex> lock(m_ids_mutex);
                    m_thread_ids.emplace_back(std::this_thread::get_id());
                    return;
                }
            }

            if (!m_task_queue.empty()) {
                std::cout << "thread get task: " << std::this_thread::get_id() << std::endl;
                task = std::move(m_task_queue.front());
                m_task_queue.pop();
            }
        }

        if (task != nullptr) {
            m_idle_thread_num--;
            task();
            m_idle_thread_num++;
        }
    }
}

void Threadpool::addTask(std::function<void(void)> task) {
    {
        std::lock_guard<std::mutex> lock(m_queue_mutex);
        m_task_queue.push(task);
    }

    m_cond.notify_one();
}
