#ifndef STATE_MACHINE_H
#define STATE_MACHINE_H

#include <memory>
#include <unordered_map>
#include <functional>
#include <string>
#include <vector>
#include <chrono>
#include <typeindex>
#include <iostream>
#include <exception>
#include <mutex>

namespace StateMachine {

/**
 * @brief 简化版Any类（C++11兼容）
 * 用于存储任意类型的值
 */
    class Any {
    public:
        Any() : content_(nullptr) {}

        template<typename T>
        Any(const T& value) : content_(new Holder<T>(value)) {}

        Any(const Any& other)
                : content_(other.content_ ? other.content_->clone() : nullptr) {}

        ~Any() { delete content_; }

        Any& operator=(const Any& other) {
            if (this != &other) {
                delete content_;
                content_ = other.content_ ? other.content_->clone() : nullptr;
            }
            return *this;
        }

        template<typename T>
        Any& operator=(const T& value) {
            delete content_;
            content_ = new Holder<T>(value);
            return *this;
        }

        bool empty() const { return !content_; }

        template<typename T>
        T* cast() {
            if (!content_) return nullptr;
            return dynamic_cast<Holder<T>*>(content_) ?
                   &static_cast<Holder<T>*>(content_)->held : nullptr;
        }

        template<typename T>
        const T* cast() const {
            if (!content_) return nullptr;
            return dynamic_cast<const Holder<T>*>(content_) ?
                   &static_cast<const Holder<T>*>(content_)->held : nullptr;
        }

    private:
        class PlaceHolder {
        public:
            virtual ~PlaceHolder() {}
            virtual PlaceHolder* clone() const = 0;
        };

        template<typename T>
        class Holder : public PlaceHolder {
        public:
            T held;
            Holder(const T& value) : held(value) {}
            virtual PlaceHolder* clone() const override {
                return new Holder(held);
            }
        };

        PlaceHolder* content_;
    };

// 前向声明
    class State;
    class StateMachineEngine;
    class Event;

// 状态ID类型定义
    using StateID = std::string;
// 事件ID类型定义
    using EventID = std::string;
// 状态智能指针
    using StatePtr = std::shared_ptr<State>;
// 转换条件函数类型
    using TransitionCondition = std::function<bool(const Event&)>;
// 转换动作函数类型
    using TransitionAction = std::function<void(const Event&)>;

/**
 * @brief 事件基类
 * 用于触发状态转换的事件对象
 */
    class Event {
    public:
        /**
         * @brief 构造函数
         * @param id 事件ID
         */
        explicit Event(const EventID& id) : id_(id), timestamp_(std::chrono::steady_clock::now()) {}

        /**
         * @brief 获取事件ID
         */
        const EventID& GetID() const { return id_; }

        /**
         * @brief 获取事件时间戳
         */
        std::chrono::steady_clock::time_point GetTimestamp() const { return timestamp_; }

        /**
         * @brief 设置事件数据
         * @param data 任意类型的数据
         */
        template<typename T>
        void SetData(const T& data) {
            data_ = data;
        }

        /**
         * @brief 获取事件数据
         * @return 指定类型的数据指针，如果类型不匹配返回nullptr
         */
        template<typename T>
        const T* GetData() const {
            return data_.cast<T>();
        }

    private:
        EventID id_;                                           // 事件ID
        std::chrono::steady_clock::time_point timestamp_;      // 事件时间戳
        Any data_;                                             // 事件携带的数据
    };

/**
 * @brief 状态转换定义
 * 描述从一个状态到另一个状态的转换规则
 */
    struct Transition {
        EventID event;                 // 触发转换的事件ID
        StateID target;                // 目标状态ID
        TransitionCondition condition;  // 转换条件（可选）
        TransitionAction action;        // 转换时执行的动作（可选）

        /**
         * @brief 构造函数
         */
        Transition(const EventID& evt, const StateID& tgt,
                   TransitionCondition cond = nullptr,
                   TransitionAction act = nullptr)
                : event(evt), target(tgt), condition(cond), action(act) {}
    };

/**
 * @brief 状态基类
 * 所有具体状态都应该继承此类
 */
    class State : public std::enable_shared_from_this<State> {
    public:
        /**
         * @brief 构造函数
         * @param id 状态ID
         * @param name 状态名称（用于调试）
         */
        State(const StateID& id, const std::string& name = "")
                : id_(id), name_(name.empty() ? id : name) {}

        virtual ~State() = default;

        /**
         * @brief 进入状态时调用
         * @param engine 状态机引擎
         * @param from 来源状态（可能为空）
         */
        virtual void OnEnter(StateMachineEngine* engine, StatePtr from) {
            std::cout << "进入状态: " << name_ << std::endl;
        }

        /**
         * @brief 退出状态时调用
         * @param engine 状态机引擎
         * @param to 目标状态（可能为空）
         */
        virtual void OnExit(StateMachineEngine* engine, StatePtr to) {
            std::cout << "退出状态: " << name_ << std::endl;
        }

        /**
         * @brief 状态更新函数
         * @param engine 状态机引擎
         * @param deltaTime 距离上次更新的时间间隔（秒）
         */
        virtual void OnUpdate(StateMachineEngine* engine, float deltaTime) {}

        /**
         * @brief 处理事件
         * @param engine 状态机引擎
         * @param event 事件对象
         * @return 是否处理了该事件
         */
        virtual bool OnEvent(StateMachineEngine* engine, const Event& event) {
            return false;
        }

        /**
         * @brief 添加状态转换
         * @param transition 转换定义
         */
        void AddTransition(const Transition& transition) {
            transitions_[transition.event].push_back(transition);
        }

        /**
         * @brief 获取状态ID
         */
        const StateID& GetID() const { return id_; }

        /**
         * @brief 获取状态名称
         */
        const std::string& GetName() const { return name_; }

        /**
         * @brief 获取所有转换
         */
        const std::unordered_map<EventID, std::vector<Transition>>& GetTransitions() const { return transitions_; }

    protected:
        StateID id_;                                                        // 状态ID
        std::string name_;                                                  // 状态名称
        std::unordered_map<EventID, std::vector<Transition>> transitions_; // 状态转换映射表
    };

/**
 * @brief 状态机上下文
 * 用于存储状态机运行时的共享数据
 */
    class Context {
    public:
        /**
         * @brief 设置变量值
         * @param key 变量名
         * @param value 变量值
         */
        template<typename T>
        void Set(const std::string& key, const T& value) {
            data_[key] = value;
        }

        /**
         * @brief 获取变量值
         * @param key 变量名
         * @return 变量值指针，如果不存在或类型不匹配返回nullptr
         */
        template<typename T>
        const T* Get(const std::string& key) const {
            auto it = data_.find(key);
            if (it != data_.end()) {
                return it->second.cast<T>();
            }
            return nullptr;
        }

        /**
         * @brief 检查变量是否存在
         * @param key 变量名
         */
        bool Has(const std::string& key) const {
            return data_.find(key) != data_.end();
        }

        /**
         * @brief 删除变量
         * @param key 变量名
         */
        void Remove(const std::string& key) {
            data_.erase(key);
        }

        /**
         * @brief 清空所有变量
         */
        void Clear() {
            data_.clear();
        }

    private:
        std::unordered_map<std::string, Any> data_;  // 数据存储
    };

/**
 * @brief 状态机引擎
 * 管理状态和状态转换的核心类
 */
    class StateMachineEngine {
    public:
        /**
         * @brief 构造函数
         * @param name 状态机名称
         */
        explicit StateMachineEngine(const std::string& name = "StateMachine")
                : name_(name), running_(false) {
            context_ = std::make_shared<Context>();
        }

        /**
         * @brief 添加状态
         * @param state 状态对象
         */
        void AddState(StatePtr state) {
            states_[state->GetID()] = state;
        }

        /**
         * @brief 设置初始状态
         * @param stateID 状态ID
         */
        void SetInitialState(const StateID& stateID) {
            initial_state_id_ = stateID;
        }

        /**
         * @brief 启动状态机
         * @return 是否成功启动
         */
        bool Start() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (initial_state_id_.empty()) {
                std::cerr << "错误: 未设置初始状态" << std::endl;
                return false;
            }

            auto it = states_.find(initial_state_id_);
            if (it == states_.end()) {
                std::cerr << "错误: 初始状态不存在: " << initial_state_id_ << std::endl;
                return false;
            }

            current_state_ = it->second;
            current_state_->OnEnter(this, nullptr);
            running_ = true;
            last_update_time_ = std::chrono::steady_clock::now();

            std::cout << "状态机 [" << name_ << "] 已启动，初始状态: " << current_state_->GetName() << std::endl;
            return true;
        }

        /**
         * @brief 停止状态机
         */
        void Stop() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (running_ && current_state_) {
                current_state_->OnExit(this, nullptr);
                current_state_ = nullptr;
            }
            running_ = false;
            std::cout << "状态机 [" << name_ << "] 已停止" << std::endl;
        }

        /**
         * @brief 更新状态机
         * 应该在主循环中定期调用
         */
        void Update() {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (!running_ || !current_state_) return;

            auto now = std::chrono::steady_clock::now();
            float deltaTime = std::chrono::duration<float>(now - last_update_time_).count();
            last_update_time_ = now;

            current_state_->OnUpdate(this, deltaTime);

            // 处理延迟事件
            ProcessPendingEvents();
        }

        /**
         * @brief 发送事件
         * @param event 事件对象
         * @param immediate 是否立即处理（false则加入队列）
         */
        void SendEvent(const Event& event, bool immediate = true) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            if (immediate) {
                ProcessEvent(event);
            } else {
                pending_events_.push_back(event);
            }
        }

        /**
         * @brief 强制转换到指定状态
         * @param stateID 目标状态ID
         * @return 是否成功转换
         */
        bool ForceTransition(const StateID& stateID) {
            std::lock_guard<std::recursive_mutex> lock(mutex_);

            auto it = states_.find(stateID);
            if (it == states_.end()) {
                std::cerr << "错误: 目标状态不存在: " << stateID << std::endl;
                return false;
            }

            PerformTransition(it->second);
            return true;
        }

        /**
         * @brief 获取当前状态
         */
        StatePtr GetCurrentState() const { return current_state_; }

        /**
         * @brief 获取状态机上下文
         */
        std::shared_ptr<Context> GetContext() { return context_; }

        /**
         * @brief 检查状态机是否正在运行
         */
        bool IsRunning() const {
            std::lock_guard<std::recursive_mutex> lock(mutex_);
            return running_;
        }

        /**
         * @brief 获取状态机名称
         */
        const std::string& GetName() const { return name_; }

    private:
        /**
         * @brief 处理单个事件
         */
        void ProcessEvent(const Event& event) {
            if (!current_state_) return;

            // 首先让当前状态处理事件
            if (current_state_->OnEvent(this, event)) {
                return;
            }

            // 检查状态转换
            const auto& transitions = current_state_->GetTransitions();
            auto it = transitions.find(event.GetID());
            if (it != transitions.end()) {
                for (const auto& transition : it->second) {
                    // 检查转换条件
                    if (!transition.condition || transition.condition(event)) {
                        // 执行转换动作
                        if (transition.action) {
                            transition.action(event);
                        }

                        // 执行状态转换
                        auto targetIt = states_.find(transition.target);
                        if (targetIt != states_.end()) {
                            PerformTransition(targetIt->second);
                        }
                        break;  // 只执行第一个满足条件的转换
                    }
                }
            }
        }

        /**
         * @brief 执行状态转换
         */
        void PerformTransition(StatePtr newState) {
            if (current_state_) {
                std::cout << "状态转换: " << current_state_->GetName()
                          << " -> " << newState->GetName() << std::endl;
                current_state_->OnExit(this, newState);
            }

            StatePtr oldState = current_state_;
            current_state_ = newState;
            current_state_->OnEnter(this, oldState);
        }

        /**
         * @brief 处理待处理的事件队列
         */
        void ProcessPendingEvents() {
            if (pending_events_.empty()) return;

            auto events = std::move(pending_events_);
            pending_events_.clear();

            for (const auto& event : events) {
                ProcessEvent(event);
            }
        }

    private:
        std::string name_;                                     // 状态机名称
        std::unordered_map<StateID, StatePtr> states_;        // 所有状态
        StatePtr current_state_;                               // 当前状态
        StateID initial_state_id_;                            // 初始状态ID
        bool running_;                                        // 运行状态
        std::shared_ptr<Context> context_;                    // 状态机上下文
        std::vector<Event> pending_events_;                   // 待处理事件队列
        std::chrono::steady_clock::time_point last_update_time_; // 上次更新时间
        mutable std::recursive_mutex mutex_;                  // 递归互斥锁，用于线程同步
    };

} // namespace StateMachine

#endif // STATE_MACHINE_H