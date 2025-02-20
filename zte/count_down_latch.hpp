#pragma

#include <mutex>
#include <condition_variable>

namespace util {
    class CountDownLatch {
    public:
        CountDownLatch(const CountDownLatch&) = delete;
        explicit CountDownLatch(int count = 0) : count_(count) {}
        void Await() {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait(lock, [this] { return count_ == 0; });
        }
        void AwaitFor(const std::chrono::milliseconds& timeout) {
            std::unique_lock<std::mutex> lock(mutex_);
            condition_.wait_for(lock, timeout, [this] { return count_ == 0; });
        }
        void CountDown() {
            std::lock_guard<std::mutex> lock(mutex_);
            if (count_ == 0) {
                return;
            }
            count_--;
            if (count_ == 0) {
                condition_.notify_all();
            }
        }
        void CountUp() {
            std::lock_guard<std::mutex> guard(mutex_);
            count_++;
        }
        int Count() const {
            std::lock_guard<std::mutex> guard(mutex_);
            return count_;
        }
        void SetCountOne() {
            std::lock_guard<std::mutex> guard(mutex_);
            count_ = 1;
        }
    private:
        mutable std::mutex mutex_;
        std::condition_variable condition_;
        int count_ {0};
    };

    using CountDonwLatchPtr = std::shared_ptr<CountDownLatch>;
}