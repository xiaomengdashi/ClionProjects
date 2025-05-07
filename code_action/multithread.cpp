#include <iostream>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <memory>


// 耗时内容处理类
class StateSubmitor
{
private:
    
public:
    explicit StateSubmitor(const std::string& str);
    ~StateSubmitor();
    void submit(const std::string& content); // content 可任意
    void flash();

private:
    StateSubmitor(const StateSubmitor&) = delete;
    StateSubmitor& operator=(const StateSubmitor&) = delete;
};

// 节点监控类 （时刻监控一个线程的状态)
class NodeMonitor
{
public:
    ~NodeMonitor();
    static NodeMonitor* instance();

    void start();
    void shutdown();
    bool init();

private:
    NodeMonitor();

    NodeMonitor(const NodeMonitor&) = delete;
    NodeMonitor& operator=(const NodeMonitor&) = delete;
    void stateInfo(const std::string& strs);

    void ThreadFunc();

    bool shutdown_;
    std::mutex mutex_;
    std::thread thread_;
    std::condition_variable cond_;
    //queue
    std::queue<std::string> task_queue_;
    std::unique_ptr<StateSubmitor> submitor_;
};


StateSubmitor::StateSubmitor(const std::string& str) {

}

StateSubmitor::~StateSubmitor() {
    this->flash();
}

void StateSubmitor::submit(const  std::string& content) {
    /*
        对content的耗时处理逻辑
    */

    std::cout << "content: " << content << std::endl;
}

void StateSubmitor::flash() {

}

NodeMonitor::NodeMonitor() : shutdown_(false) {

}

NodeMonitor::~NodeMonitor() {
    this->shutdown();
}

NodeMonitor* NodeMonitor::instance() {
    static NodeMonitor* instance = nullptr;
    static std::once_flag flag;
    std::call_once(flag, [&]() {
        instance = new (std::nothrow) NodeMonitor();
    });

    return instance;
}

void NodeMonitor::start() {
    this->thread_ = std::thread(&NodeMonitor::ThreadFunc, this);
    if (!init()) {
        return;
    }
}

bool NodeMonitor::init() {
    submitor_.reset(new StateSubmitor("abc"));

    /*
    不断填充stateinfo
    */
    while (true) {
        stateInfo("12345");
    }

}

// 填入需要的信息
void NodeMonitor::stateInfo(const std::string& strs) {
    std::unique_lock<std::mutex> lock(mutex_);
    task_queue_.push(strs);
    cond_.notify_one();
}

// 线程销毁
void NodeMonitor::shutdown() {
    std::unique_lock<std::mutex> lock(mutex_);
    shutdown_ = true;
    cond_.notify_all();
    if (thread_.joinable()) {
        thread_.join();
    }
}

// 主线程函数
void NodeMonitor::ThreadFunc() {
    while (!shutdown_) {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_.wait(lock, [this]() {
            return shutdown_ || !task_queue_.empty();
        });
    
        if (shutdown_) {
            break;;
        }

        std::string str = task_queue_.front();
        task_queue_.pop();
        lock.unlock();

        submitor_->submit(str);
    }
}


int main() {
    NodeMonitor::instance()->start();
    return 0;
}