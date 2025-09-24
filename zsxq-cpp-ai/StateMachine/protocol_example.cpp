//
// Created by Kolane on 2025/9/12.
//

#include "state_machine.h"
#include <iostream>
#include <thread>
#include <queue>
#include <sstream>

using namespace StateMachine;

/**
 * @brief TCP连接协议状态机示例
 * 模拟TCP连接的建立、数据传输和关闭过程
 */

/**
 * @brief 关闭状态
 * 连接未建立或已关闭
 */
class ClosedState : public State {
public:
    ClosedState() : State("closed", "关闭状态") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[CLOSED] 连接已关闭，等待新连接请求..." << std::endl;

        // 清理上下文
        auto ctx = engine->GetContext();
        ctx->Remove("remote_address");
        ctx->Remove("remote_port");
        ctx->Set("connection_id", 0);
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "passive_open") {
            std::cout << "[CLOSED] 收到被动打开请求，准备监听..." << std::endl;
            return false; // 让转换处理
        } else if (event.GetID() == "active_open") {
            std::cout << "[CLOSED] 收到主动连接请求..." << std::endl;
            return false; // 让转换处理
        }
        return false;
    }
};

/**
 * @brief 监听状态
 * 等待客户端连接
 */
class ListenState : public State {
private:
    float listen_time_;  // 监听时间计时器

public:
    ListenState() : State("listen", "监听状态"), listen_time_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        listen_time_ = 0;  // 重置计时器

        auto ctx = engine->GetContext();
        ctx->Set("listen_port", 8080);
        std::cout << "[LISTEN] 开始监听端口 8080，等待连接..." << std::endl;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        listen_time_ += deltaTime;

        // 模拟收到连接请求
        if (listen_time_ > 2.0f) {
            listen_time_ = 0;
            std::cout << "[LISTEN] 收到来自客户端的SYN包" << std::endl;
            Event synEvent("recv_syn");
            synEvent.SetData(std::string("192.168.1.100:5000"));
            engine->SendEvent(synEvent);
        }
    }
};

/**
 * @brief SYN已发送状态
 * 主动连接，等待对方响应
 */
class SynSentState : public State {
private:
    int retry_count_;
    float timeout_;  // 超时计时器

public:
    SynSentState() : State("syn_sent", "SYN已发送"), retry_count_(0), timeout_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        retry_count_ = 0;
        timeout_ = 0;
        std::cout << "[SYN_SENT] 发送SYN包，等待响应..." << std::endl;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        timeout_ += deltaTime;

        if (timeout_ > 3.0f) { // 3秒超时
            timeout_ = 0;
            retry_count_++;

            if (retry_count_ > 3) {
                std::cout << "[SYN_SENT] 连接超时，返回关闭状态" << std::endl;
                Event timeoutEvent("timeout");
                engine->SendEvent(timeoutEvent);
            } else {
                std::cout << "[SYN_SENT] 重发SYN包 (第" << retry_count_ << "次重试)" << std::endl;
            }
        }
    }
};

/**
 * @brief SYN已接收状态
 * 被动连接，发送SYN+ACK
 */
class SynReceivedState : public State {
public:
    SynReceivedState() : State("syn_received", "SYN已接收") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[SYN_RECEIVED] 发送SYN+ACK包，等待最终确认..." << std::endl;

        // 设置远程地址信息（从LISTEN状态转换过来）
        if (from && from->GetID() == "listen") {
            engine->GetContext()->Set("remote_address", std::string("192.168.1.100:5000"));
        }

        // 设置计时器，准备接收ACK
        engine->GetContext()->Set("syn_recv_timer", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto timer = ctx->Get<float>("syn_recv_timer");
        if (timer) {
            float newTime = *timer + deltaTime;
            ctx->Set("syn_recv_timer", newTime);

            // 0.5秒后模拟收到ACK
            if (newTime >= 0.5f) {
                ctx->Remove("syn_recv_timer");
                std::cout << "[SYN_RECEIVED] 收到ACK包" << std::endl;
                Event ackEvent("recv_ack");
                engine->SendEvent(ackEvent);
            }
        }
    }
};

/**
 * @brief 已建立连接状态
 * 可以进行数据传输
 */
class EstablishedState : public State {
private:
    int data_sent_;
    int data_received_;
    std::queue<std::string> send_queue_;
    float send_timer_;  // 发送计时器
    float connection_time_;  // 连接时间

public:
    EstablishedState() : State("established", "已建立连接"),
                         data_sent_(0), data_received_(0), send_timer_(0), connection_time_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[ESTABLISHED] 连接已建立，可以传输数据" << std::endl;

        send_timer_ = 0;
        connection_time_ = 0;

        auto ctx = engine->GetContext();
        ctx->Set("data_sent", 0);
        ctx->Set("data_received", 0);

        // 准备一些要发送的数据
        send_queue_.push("Hello, Server!");
        send_queue_.push("This is a test message.");
        send_queue_.push("Connection working properly.");
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        send_timer_ += deltaTime;
        connection_time_ += deltaTime;

        // 每秒发送一条消息
        if (send_timer_ >= 1.0f && !send_queue_.empty()) {
            send_timer_ = 0;
            std::string msg = send_queue_.front();
            send_queue_.pop();

            data_sent_ += msg.length();
            std::cout << "[ESTABLISHED] 发送数据: \"" << msg
                      << "\" (" << msg.length() << " 字节)" << std::endl;

            auto ctx = engine->GetContext();
            ctx->Set("data_sent", data_sent_);

            // 模拟接收响应
            std::stringstream response;
            response << "ACK: " << msg;
            data_received_ += response.str().length();
            ctx->Set("data_received", data_received_);

            std::cout << "[ESTABLISHED] 收到响应: \"" << response.str()
                      << "\" (" << response.str().length() << " 字节)" << std::endl;
        }

        // 模拟5秒后关闭连接
        if (connection_time_ > 5.0f) {
            connection_time_ = 0;
            std::cout << "[ESTABLISHED] 准备关闭连接..." << std::endl;
            std::cout << "[ESTABLISHED] 统计 - 发送: " << data_sent_
                      << " 字节, 接收: " << data_received_ << " 字节" << std::endl;

            Event closeEvent("close");
            engine->SendEvent(closeEvent);
        }
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "send_data") {
            auto data = event.GetData<std::string>();
            if (data) {
                send_queue_.push(*data);
                std::cout << "[ESTABLISHED] 数据已加入发送队列: \"" << *data << "\"" << std::endl;
                return true;
            }
        }
        return false;
    }
};

/**
 * @brief FIN等待1状态
 * 主动关闭，已发送FIN
 */
class FinWait1State : public State {
public:
    FinWait1State() : State("fin_wait_1", "FIN等待1") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[FIN_WAIT_1] 发送FIN包，等待确认..." << std::endl;

        // 设置计时器
        engine->GetContext()->Set("fin_wait1_timer", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto timer = ctx->Get<float>("fin_wait1_timer");
        if (timer) {
            float newTime = *timer + deltaTime;
            ctx->Set("fin_wait1_timer", newTime);

            // 0.5秒后模拟收到ACK
            if (newTime >= 0.5f) {
                ctx->Remove("fin_wait1_timer");
                std::cout << "[FIN_WAIT_1] 收到ACK包" << std::endl;
                Event ackEvent("recv_ack");
                engine->SendEvent(ackEvent);
            }
        }
    }
};

/**
 * @brief FIN等待2状态
 * 等待对方的FIN
 */
class FinWait2State : public State {
public:
    FinWait2State() : State("fin_wait_2", "FIN等待2") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[FIN_WAIT_2] 等待对方发送FIN包..." << std::endl;

        // 设置计时器
        engine->GetContext()->Set("fin_wait2_timer", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto timer = ctx->Get<float>("fin_wait2_timer");
        if (timer) {
            float newTime = *timer + deltaTime;
            ctx->Set("fin_wait2_timer", newTime);

            // 1秒后模拟收到FIN
            if (newTime >= 1.0f) {
                ctx->Remove("fin_wait2_timer");
                std::cout << "[FIN_WAIT_2] 收到对方的FIN包" << std::endl;
                Event finEvent("recv_fin");
                engine->SendEvent(finEvent);
            }
        }
    }
};

/**
 * @brief CLOSE_WAIT状态
 * 被动关闭，等待应用层关闭
 */
class CloseWaitState : public State {
private:
    float wait_time_;

public:
    CloseWaitState() : State("close_wait", "CLOSE_WAIT"), wait_time_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        wait_time_ = 0;
        std::cout << "[CLOSE_WAIT] 收到对方FIN，发送ACK，等待应用层关闭..." << std::endl;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        wait_time_ += deltaTime;

        // 模拟应用层决定关闭
        if (wait_time_ >= 1.0f) {
            std::cout << "[CLOSE_WAIT] 应用层决定关闭连接" << std::endl;
            Event closeEvent("close");
            engine->SendEvent(closeEvent);
        }
    }
};

/**
 * @brief LAST_ACK状态
 * 被动关闭，发送FIN后等待ACK
 */
class LastAckState : public State {
public:
    LastAckState() : State("last_ack", "LAST_ACK") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[LAST_ACK] 发送FIN包，等待最终ACK..." << std::endl;

        // 设置计时器
        engine->GetContext()->Set("last_ack_timer", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto timer = ctx->Get<float>("last_ack_timer");
        if (timer) {
            float newTime = *timer + deltaTime;
            ctx->Set("last_ack_timer", newTime);

            // 0.5秒后模拟收到ACK
            if (newTime >= 0.5f) {
                ctx->Remove("last_ack_timer");
                std::cout << "[LAST_ACK] 收到ACK，关闭连接" << std::endl;
                Event ackEvent("recv_ack");
                engine->SendEvent(ackEvent);
            }
        }
    }
};

/**
 * @brief CLOSING状态
 * 同时关闭，双方同时发送FIN
 */
class ClosingState : public State {
public:
    ClosingState() : State("closing", "CLOSING") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "[CLOSING] 双方同时关闭，等待ACK..." << std::endl;

        // 设置计时器
        engine->GetContext()->Set("closing_timer", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto timer = ctx->Get<float>("closing_timer");
        if (timer) {
            float newTime = *timer + deltaTime;
            ctx->Set("closing_timer", newTime);

            // 0.5秒后模拟收到ACK
            if (newTime >= 0.5f) {
                ctx->Remove("closing_timer");
                std::cout << "[CLOSING] 收到ACK" << std::endl;
                Event ackEvent("recv_ack");
                engine->SendEvent(ackEvent);
            }
        }
    }
};

/**
 * @brief 时间等待状态
 * 等待2MSL时间
 */
class TimeWaitState : public State {
private:
    float wait_time_;

public:
    TimeWaitState() : State("time_wait", "时间等待"), wait_time_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        wait_time_ = 0;
        std::cout << "[TIME_WAIT] 发送最后的ACK，等待2MSL时间..." << std::endl;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        wait_time_ += deltaTime;

        // 模拟2MSL时间（这里用2秒代替）
        if (wait_time_ >= 2.0f) {
            std::cout << "[TIME_WAIT] 2MSL时间到，关闭连接" << std::endl;
            Event timeoutEvent("timeout");
            engine->SendEvent(timeoutEvent);
        }
    }
};

/**
 * @brief 创建TCP协议状态机
 */
std::shared_ptr<StateMachineEngine> CreateTCPStateMachine() {
    auto sm = std::make_shared<StateMachineEngine>("TCP协议状态机");

    // 创建所有状态
    auto closedState = std::make_shared<ClosedState>();
    auto listenState = std::make_shared<ListenState>();
    auto synSentState = std::make_shared<SynSentState>();
    auto synReceivedState = std::make_shared<SynReceivedState>();
    auto establishedState = std::make_shared<EstablishedState>();
    auto finWait1State = std::make_shared<FinWait1State>();
    auto finWait2State = std::make_shared<FinWait2State>();
    auto timeWaitState = std::make_shared<TimeWaitState>();
    auto closeWaitState = std::make_shared<CloseWaitState>();
    auto lastAckState = std::make_shared<LastAckState>();
    auto closingState = std::make_shared<ClosingState>();

    // 设置状态转换规则

    // CLOSED状态转换
    closedState->AddTransition(Transition("passive_open", "listen"));
    closedState->AddTransition(Transition("active_open", "syn_sent"));

    // LISTEN状态转换
    listenState->AddTransition(Transition("recv_syn", "syn_received",
                                          nullptr,
                                          [](const Event& e) {
                                              auto addr = e.GetData<std::string>();
                                              if (addr) {
                                                  // 地址信息将在SynReceivedState::OnEnter中处理
                                                  std::cout << "接受来自 " << *addr << " 的连接" << std::endl;
                                              }
                                          }));
    listenState->AddTransition(Transition("close", "closed"));

    // SYN_SENT状态转换
    synSentState->AddTransition(Transition("recv_syn_ack", "established",
                                           nullptr,
                                           [](const Event& e) {
                                               std::cout << "收到SYN+ACK，发送ACK确认" << std::endl;
                                           }));
    synSentState->AddTransition(Transition("timeout", "closed"));

    // SYN_RECEIVED状态转换
    synReceivedState->AddTransition(Transition("recv_ack", "established"));
    synReceivedState->AddTransition(Transition("timeout", "closed"));

    // ESTABLISHED状态转换
    establishedState->AddTransition(Transition("close", "fin_wait_1"));  // 主动关闭
    establishedState->AddTransition(Transition("recv_fin", "close_wait"));  // 被动关闭

    // FIN_WAIT_1状态转换
    finWait1State->AddTransition(Transition("recv_ack", "fin_wait_2"));  // 收到ACK
    finWait1State->AddTransition(Transition("recv_fin", "closing"));  // 同时关闭
    finWait1State->AddTransition(Transition("recv_fin_ack", "time_wait"));  // 收到FIN+ACK

    // FIN_WAIT_2状态转换
    finWait2State->AddTransition(Transition("recv_fin", "time_wait"));

    // CLOSE_WAIT状态转换
    closeWaitState->AddTransition(Transition("close", "last_ack"));

    // LAST_ACK状态转换
    lastAckState->AddTransition(Transition("recv_ack", "closed"));

    // CLOSING状态转换
    closingState->AddTransition(Transition("recv_ack", "time_wait"));

    // TIME_WAIT状态转换
    timeWaitState->AddTransition(Transition("timeout", "closed"));

    // 添加所有状态到状态机
    sm->AddState(closedState);
    sm->AddState(listenState);
    sm->AddState(synSentState);
    sm->AddState(synReceivedState);
    sm->AddState(establishedState);
    sm->AddState(finWait1State);
    sm->AddState(finWait2State);
    sm->AddState(timeWaitState);
    sm->AddState(closeWaitState);
    sm->AddState(lastAckState);
    sm->AddState(closingState);

    // 设置初始状态
    sm->SetInitialState("closed");

    return sm;
}

/**
 * @brief 演示TCP连接的完整生命周期
 */
void demonstrateTCPConnection() {
    std::cout << "\n--- 演示TCP连接生命周期 ---\n" << std::endl;

    auto tcpSM = CreateTCPStateMachine();

    if (!tcpSM->Start()) {
        std::cerr << "TCP状态机启动失败！" << std::endl;
        return;
    }

    // 被动打开（服务器模式）
    std::cout << "\n1. 服务器端：被动打开，开始监听" << std::endl;
    Event passiveOpen("passive_open");
    tcpSM->SendEvent(passiveOpen);

    // 运行状态机
    for (int i = 0; i < 100 && tcpSM->IsRunning(); i++) {
        tcpSM->Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    if (tcpSM->IsRunning()) {
        tcpSM->Stop();
    }
}

/**
 * @brief 演示主动连接（客户端）
 */
void demonstrateActiveConnection() {
    std::cout << "\n--- 演示主动连接（客户端） ---\n" << std::endl;

    auto tcpSM = CreateTCPStateMachine();

    if (!tcpSM->Start()) {
        std::cerr << "TCP状态机启动失败！" << std::endl;
        return;
    }

    // 主动连接
    std::cout << "\n1. 客户端：主动连接服务器" << std::endl;
    Event activeOpen("active_open");
    tcpSM->SendEvent(activeOpen);

    // 设置一个标志，用于模拟收到SYN+ACK
    tcpSM->GetContext()->Set("wait_syn_ack", true);
    tcpSM->GetContext()->Set("syn_ack_timer", 0.0f);

    // 运行状态机一段时间
    for (int i = 0; i < 30 && tcpSM->IsRunning(); i++) {
        tcpSM->Update();

        // 在第10次循环（1秒后）模拟收到SYN+ACK
        if (i == 10) {
            auto ctx = tcpSM->GetContext();
            auto waitSynAck = ctx->Get<bool>("wait_syn_ack");
            if (waitSynAck && *waitSynAck) {
                std::cout << "\n[客户端] 收到服务器的SYN+ACK包" << std::endl;
                Event synAck("recv_syn_ack");
                tcpSM->SendEvent(synAck);
                ctx->Set("wait_syn_ack", false);
            }
        }

        // 在连接建立后发送一些数据
        if (i == 20 && tcpSM->GetCurrentState()->GetID() == "established") {
            Event dataEvent("send_data");
            dataEvent.SetData(std::string("Custom data from client"));
            tcpSM->SendEvent(dataEvent);
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    if (tcpSM->IsRunning()) {
        tcpSM->Stop();
    }
}

/**
 * @brief 主函数
 */
int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "      TCP协议状态机演示程序" << std::endl;
    std::cout << "=====================================" << std::endl;

    // 演示被动连接（服务器）
    demonstrateTCPConnection();

    std::cout << "\n\n=====================================" << std::endl;

    // 演示主动连接（客户端）
    demonstrateActiveConnection();

    std::cout << "\n=====================================" << std::endl;
    std::cout << "        演示程序结束" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}