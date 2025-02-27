
#pragma once

#include "count_down_latch.hpp"
#include <boost/asio.hpp>
#include <vector>
#include <thread>

#include <mutex>
#include <chrono>
#include <iostream>
#include <sys/prctl.h>


namespace util {
    class io_t {
    public:
        explicit io_t(std::size_t concurrency = 1) : context_(concurrency), strand_(context_) {};

        virtual ~io_t() = default;

        inline boost::asio::io_context &context() { return context_; }

        inline boost::asio::io_context::strand &strand() { return strand_; }

    protected:
        boost::asio::io_context context_;
        boost::asio::iocontext::strand strand_;
    };

    using io_ptr_t = std::shared_ptr<io_t>;

    class iopool {
    public:
        explicit iopool(std::size_t concurrency) : ios_(concurrency) {
            assert(concurrency > 0);

            works_.reserve(ios_.size());
            threads_.reserve(ios_.size());
        }

        iopool(const std::vector<int> &cpu_scheds, std::size_t concurrency = 0)
        : iopool(concurrency == 0 ? cpu_scheds.size() : concurrency) {
        cpu_scheds_ = cpu_scheds;
        }

        virtual ~iopool(){
            stop();
        }

        bool start(const std::string & prefix_name = "") {
            std::lock_guard<std::mutex> lock(mutex_);

            if (!stopped_) {
                return false;
            }

            if (!works_.empty() || !threads_.empty()) {
                return false;
            }

            // Creat a pool of threads to run all of the io_context objects.
            for (size_t i = 0; i < ios_.size(); ++i) {
                ios_[i].context().restart();

                works_.emplace_back(ios_[i].context().get_executor());

                // start work thread
                threads_.emplace_back([i, prefix_name, this](){
                    if (!prefix_name.empty()) {
                        std::string name = prefix_name + std::to_string(i);
                        ::prtcl(PR_SET_NAME, name.c_str());
                    }

                    set_sched(i);

                    ios_[i].context().run();
                });
            }

            stopped_ = false;

            return true;
        }

        virtual void set_sched(int thread_id) {
            if (!cpu_scheds_.empty()) {
                cpu_set_t cpuset;
                CPU_ZERO(&cpuset);

                for (auto cpu : cpu_scheds_) {
                    CPU_SET(cpu, &cpuset);
                }

                if (CPU_COUNT(&cpuset) > 0) {
                    sched_setaffinity(0, sizeof(cpuset), &cpuset);
                }
            }
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(mutex_);

                if (stopped_) {
                    return;
                }

                if (works_.empty() && threads_.empty()) {
                    return;
                }
                if (running_in_iopool_threads()) {
                    return;
                }

                stopped_ = true;
            }

            wait_iothreads();

            {
                std::lock_guard<std::mutex> guard(mutex_);

                for (auto &work : works_) {
                    work.reset();
                }

                for (auto &thread : threads_) {
                    thread.join();
                }

                works_.clear();
                threads_.clear();
            }
        }

        inline bool is_started() const {
            return !stopped_;
        }

        inline bool is_stopped() const {
            return stopped_;
        }

        inline io_t& get(std::size_t index = static_cast<std::size_t>(-1)) {
            return ios_[index < ios_.size()? index : next_++ % ios_.size()];
        }

        inline bool running_in_iopool_threads() const {
            std::thread::id curr_tid = std::this_thread::get_id();
            for (auto &thread : threads_) {
                if (thread.get_id() == curr_tid) {
                    return true;
                }
            }
            return false;
        }

        void wait_iothreads() {
            {
                std::lock_guard<std::mutex> lock(mutex_);

                if (running_in_iopool_threads())
                    return;

                if (works_.empty())
                    return;
            }

            constexpr auto max = std::chrono::milliseconds(10);
            constexpr auto min = std::chrono::milliseconds(1);

            if (!ios_.empty()) {
                auto t1 = std::chrono::steady_clock::now();
                boost::asio::io_context &ioc = ios_.front().context();
                while (!ioc.stopped()) {
                    auto t2 = std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(
                            (std::max<std::chrono::steady_clock::duration>)(
                                    (std::min<std::chrono::steady_clock::duration>)(
                                            t2 - t1, max), min));
                }
            }

            {
                std::lock_guard<std::mutex> guard(mutex_);

                for (auto &work : works_) {
                    work.reset();
                }
            }

            for (auto &io : ios_) {
                auto t1 = std::chrono::steady_clock::now();
                boost::asio::io_context & ioc = io.context();
                while (!ioc.stopped()) {
                    auto t2 = std::chrono::steady_clock::now();
                    std::this_thread::sleep_for(
                            (std::max<std::chrono::steady_clock::duration>)(
                                    (std::min<std::chrono::steady_clock::duration>)(
                                            t2 - t1, max
                                    ), min)
                    );
                }
            }
        }

        std::size_t concurrency() const {
            return ios_.size();
        }

    protected:
        std::vector<std::thread> threads_;
        std::vector<io_t> ios_;
        std::vector<int> cpu_scheds_;
        std::mutex mutex_;
        bool stopped_ = true;
        std::size_t  next_ = 0;
        std::vector<boost::asio::executor_work_guard<boost::asio::io_context::executor_type> works_;
    };

    using iopool_ptr_t = std::shared_ptr<iopool>;

    class fifo_iopool : public iopool {
    public:
        explicit fifo_iopool(std::size_t concurrency) : iopool(concurrency) {};

        fifo_iopool(const std::vector<int> &cpu_scheds)
            : iopool(cpu_scheds) {};

        virtual ~fifo_iopool() = default;

        void set_sched(int thread_id) override {
            cpu_set_t cpu_mask_;

            CPU_ZERO(&cpu_mask_);
            CPU_SET(cpu_scheds_[thread_id], &cpu_mask_);

            sched_setaffinity(0, sizeof(cpu_mask_), &cpu_mask_)
            struct sched_param param;
            param.sched_priority = sched_get_priority_max(SCHED_FIFO);
            if (0 != pthread_setschedparam(pthread_self(), SCHED_FIFO, &param))
                printf("pthread_setschedparam error!\n");
        }
    };

    using fifo_iopool_ptr_t = std::shared_ptr<fifo_iopool>;
}

