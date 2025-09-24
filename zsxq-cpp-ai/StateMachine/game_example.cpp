//
// Created by Kolane on 2025/9/12.
//

#include "state_machine.h"
#include <iostream>
#include <thread>
#include <random>

using namespace StateMachine;

/**
 * @brief 待机状态
 * 角色在此状态下等待命令，并可以恢复生命值
 */
class IdleState : public State {
private:
    float heal_timer_;

public:
    IdleState() : State("idle", "待机状态"), heal_timer_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "角色进入待机状态，等待命令..." << std::endl;

        // 设置待机时间
        auto ctx = engine->GetContext();
        ctx->Set("idle_time", 0.0f);
        heal_timer_ = 0;  // 重置恢复计时器

        // 从战斗胜利返回时清理敌人信息
        if (from && from->GetID() == "combat") {
            ctx->Remove("current_enemy");
        }
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto idleTime = ctx->Get<float>("idle_time");
        if (idleTime) {
            float newTime = *idleTime + deltaTime;
            ctx->Set("idle_time", newTime);

            heal_timer_ += deltaTime;
            if (heal_timer_ >= 2.0f) {
                heal_timer_ = 0;  // 重置计时器

                auto health = ctx->Get<int>("health");
                auto maxHealth = ctx->Get<int>("max_health");
                if (health && maxHealth && *health < *maxHealth) {
                    int healAmount = 5;
                    int newHealth = std::min(*health + healAmount, *maxHealth);
                    ctx->Set("health", newHealth);
                    std::cout << "休息恢复生命值: +" << healAmount
                              << " (当前: " << newHealth << "/" << *maxHealth << ")" << std::endl;
                }
            }

            // 待机超过3秒自动巡逻
            if (newTime > 3.0f) {
                std::cout << "待机时间过长，开始巡逻..." << std::endl;
                Event patrolEvent("start_patrol");
                engine->SendEvent(patrolEvent);
            }
        }
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "health_check") {
            auto ctx = engine->GetContext();
            auto health = ctx->Get<int>("health");
            auto maxHealth = ctx->Get<int>("max_health");
            if (health && maxHealth) {
                std::cout << "[待机]健康检查: 当前生命值 = " << *health << "/" << *maxHealth << std::endl;
            }
            return true;
        }
        return false;
    }
};

/**
 * @brief 巡逻状态
 * 角色在地图上巡逻
 */
class PatrolState : public State {
private:
    static std::mt19937 gen_;  // 静态随机数生成器
    static std::uniform_real_distribution<> dis_;  // 静态分布

public:
    PatrolState() : State("patrol", "巡逻状态") {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "角色开始巡逻，搜索敌人..." << std::endl;

        auto ctx = engine->GetContext();
        ctx->Set("patrol_distance", 0.0f);
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        auto ctx = engine->GetContext();
        auto distance = ctx->Get<float>("patrol_distance");
        if (distance) {
            float newDistance = *distance + deltaTime * 10.0f; // 移动速度: 10单位/秒
            ctx->Set("patrol_distance", newDistance);

            // 模拟发现敌人（提高概率到8%）
            if (dis_(gen_) < 0.08) { // 8%概率发现敌人
                std::cout << "发现敌人！巡逻距离: " << newDistance << " 单位" << std::endl;

                // 随机生成不同类型的敌人
                std::string enemyType = (dis_(gen_) < 0.7) ? "Goblin" : "Orc";  // 70%哥布林，30%兽人
                Event enemyEvent("enemy_detected");
                enemyEvent.SetData(enemyType);
                engine->SendEvent(enemyEvent);
            }
        }
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "health_check") {
            auto ctx = engine->GetContext();
            auto health = ctx->Get<int>("health");
            auto maxHealth = ctx->Get<int>("max_health");
            if (health && maxHealth) {
                std::cout << "[巡逻]健康检查: 当前生命值 = " << *health << "/" << *maxHealth << std::endl;
            }
            return true;
        }
        return false;
    }
};

// 静态成员初始化
std::mt19937 PatrolState::gen_(std::random_device{}());
std::uniform_real_distribution<> PatrolState::dis_(0, 1);

/**
 * @brief 战斗状态
 * 角色与敌人战斗
 */
class CombatState : public State {
private:
    int attack_count_;  // 攻击次数
    float attack_timer_;  // 攻击计时器
    static std::mt19937 gen_;  // 静态随机数生成器
    static std::uniform_real_distribution<> dis_;  // 静态分布

public:
    CombatState() : State("combat", "战斗状态"), attack_count_(0), attack_timer_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        attack_count_ = 0;
        attack_timer_ = 0;

        auto ctx = engine->GetContext();

        // 获取或初始化敌人信息
        auto enemy = ctx->Get<std::string>("current_enemy");
        if (!enemy) {
            // 如果没有敌人信息，默认设置为Goblin
            ctx->Set("current_enemy", std::string("敌人"));
            enemy = ctx->Get<std::string>("current_enemy");
        }

        std::cout << "进入战斗！目标: " << *enemy << std::endl;

        // 增加战斗计数
        auto battleCount = ctx->Get<int>("battle_count");
        int newCount = battleCount ? *battleCount + 1 : 1;
        ctx->Set("battle_count", newCount);
        std::cout << "这是第 " << newCount << " 场战斗" << std::endl;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        attack_timer_ += deltaTime;

        // 每秒攻击一次
        if (attack_timer_ >= 1.0f) {
            attack_timer_ = 0;
            attack_count_++;

            auto ctx = engine->GetContext();
            auto enemy = ctx->Get<std::string>("current_enemy");

            std::cout << "攻击 " << (enemy ? *enemy : "敌人")
                      << "! (第" << attack_count_ << "次攻击)" << std::endl;

            float hurtChance = 0.20f;  // 提高基础概率到20%
            if (enemy && *enemy == "Orc") {
                hurtChance = 0.35f;  // 兽人更强，35%概率受伤
            }

            // 模拟战斗结果
            if (dis_(gen_) < hurtChance) {
                std::cout << "被敌人反击，受到伤害！" << std::endl;

                // 根据敌人类型决定伤害值
                int damage = (enemy && *enemy == "Orc") ? 30 : 20;
                Event hurtEvent("take_damage");
                hurtEvent.SetData(damage);
                engine->SendEvent(hurtEvent);
            } else if (attack_count_ >= 3) { // 攻击3次后击败敌人
                std::cout << "敌人被击败！" << std::endl;
                Event victoryEvent("enemy_defeated");
                engine->SendEvent(victoryEvent);
            }
        }
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "health_check") {
            auto ctx = engine->GetContext();
            auto health = ctx->Get<int>("health");
            auto maxHealth = ctx->Get<int>("max_health");
            if (health && maxHealth) {
                std::cout << "[战斗]健康检查: 当前生命值 = " << *health << "/" << *maxHealth << std::endl;
            }
            return true;
        }
        return false;
    }
};

// 静态成员初始化
std::mt19937 CombatState::gen_(std::random_device{}());
std::uniform_real_distribution<> CombatState::dis_(0, 1);

/**
 * @brief 受伤状态
 * 角色受到伤害后的恢复状态
 */
class HurtState : public State {
private:
    float recovery_time_;  // 恢复时间

public:
    HurtState() : State("hurt", "受伤状态"), recovery_time_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        recovery_time_ = 0;

        // 处理伤害（从战斗状态转换过来）
        auto ctx = engine->GetContext();
        auto health = ctx->Get<int>("health");
        if (health && from && from->GetID() == "combat") {
            // 从事件中获取伤害值
            auto enemy = ctx->Get<std::string>("current_enemy");
            int damage = (enemy && *enemy == "Orc") ? 30 : 20;

            int newHealth = std::max(0, *health - damage);
            ctx->Set("health", newHealth);
            health = ctx->Get<int>("health");  // 重新获取更新后的值
        }

        if (health) {
            auto maxHealth = ctx->Get<int>("max_health");
            std::cout << "角色受伤！剩余生命值: " << *health << "/" << (maxHealth ? *maxHealth : 100) << std::endl;

            if (*health <= 0) {
                std::cout << "生命值归零，角色死亡！" << std::endl;
                Event deathEvent("death");
                engine->SendEvent(deathEvent);
                return;  // 避免继续执行恢复逻辑
            }

            std::cout << "进入恢复状态..." << std::endl;
        }
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        recovery_time_ += deltaTime;

        // 恢复2秒后返回待机状态
        if (recovery_time_ >= 2.0f) {
            std::cout << "恢复完成，返回待机状态" << std::endl;
            Event recoveryEvent("recovery_complete");
            engine->SendEvent(recoveryEvent);
        }
    }

    bool OnEvent(StateMachineEngine* engine, const Event& event) override {
        if (event.GetID() == "health_check") {
            auto ctx = engine->GetContext();
            auto health = ctx->Get<int>("health");
            auto maxHealth = ctx->Get<int>("max_health");
            if (health && maxHealth) {
                std::cout << "[受伤]健康检查: 当前生命值 = " << *health << "/" << *maxHealth << std::endl;
            }
            return true;
        }
        return false;
    }
};

/**
 * @brief 死亡状态
 * 角色死亡，游戏结束
 */
class DeathState : public State {
private:
    float death_timer_;

public:
    DeathState() : State("death", "死亡状态"), death_timer_(0) {}

    void OnEnter(StateMachineEngine* engine, StatePtr from) override {
        State::OnEnter(engine, from);
        std::cout << "\n========== 游戏结束 ==========" << std::endl;
        std::cout << "角色已死亡！" << std::endl;

        auto ctx = engine->GetContext();
        auto battleCount = ctx->Get<int>("battle_count");
        if (battleCount) {
            std::cout << "总共进行了 " << *battleCount << " 场战斗" << std::endl;
        }
        std::cout << "==============================" << std::endl;

        death_timer_ = 0;
    }

    void OnUpdate(StateMachineEngine* engine, float deltaTime) override {
        death_timer_ += deltaTime;

        // 1秒后停止状态机
        if (death_timer_ >= 1.0f) {
            engine->Stop();
        }
    }
};

/**
 * @brief 创建游戏角色状态机
 */
std::shared_ptr<StateMachineEngine> CreateCharacterStateMachine() {
    auto sm = std::make_shared<StateMachineEngine>("游戏角色状态机");

    // 创建所有状态
    auto idleState = std::make_shared<IdleState>();
    auto patrolState = std::make_shared<PatrolState>();
    auto combatState = std::make_shared<CombatState>();
    auto hurtState = std::make_shared<HurtState>();
    auto deathState = std::make_shared<DeathState>();

    // 设置状态转换规则

    // 待机状态转换
    idleState->AddTransition(Transition("start_patrol", "patrol"));
    idleState->AddTransition(Transition("enemy_detected", "combat",
                                        nullptr,  // 无条件
                                        [sm](const Event& e) {  // 转换动作 - 捕获状态机指针
                                            std::cout << "警报！发现敌人，准备战斗！" << std::endl;
                                            // 从事件中获取敌人类型并保存到上下文
                                            auto enemy = e.GetData<std::string>();
                                            if (enemy) {
                                                sm->GetContext()->Set("current_enemy", *enemy);
                                            }
                                        }));

    // 巡逻状态转换
    patrolState->AddTransition(Transition("enemy_detected", "combat",
                                          nullptr,  // 无条件
                                          [sm](const Event& e) {  // 转换动作 - 捕获状态机指针
                                              auto enemy = e.GetData<std::string>();
                                              if (enemy) {
                                                  std::cout << "发现 " << *enemy << "，准备战斗！" << std::endl;
                                                  // 将敌人信息保存到上下文
                                                  sm->GetContext()->Set("current_enemy", *enemy);
                                              }
                                          }));
    patrolState->AddTransition(Transition("return_idle", "idle"));

    // 战斗状态转换
    combatState->AddTransition(Transition("take_damage", "hurt",
                                          nullptr,  // 无条件转换
                                          nullptr   // 伤害处理在HurtState::OnEnter中进行
    ));
    combatState->AddTransition(Transition("enemy_defeated", "idle",
                                          nullptr,
                                          [](const Event& e) {
                                              std::cout << "战斗胜利，返回待机状态" << std::endl;
                                          }));

    // 受伤状态转换
    hurtState->AddTransition(Transition("recovery_complete", "idle"));
    hurtState->AddTransition(Transition("death", "death"));

    // 添加所有状态到状态机
    sm->AddState(idleState);
    sm->AddState(patrolState);
    sm->AddState(combatState);
    sm->AddState(hurtState);
    sm->AddState(deathState);

    // 设置初始状态
    sm->SetInitialState("idle");

    // 初始化上下文数据（降低初始生命值，使死亡状态更容易触发）
    sm->GetContext()->Set("health", 80);  // 降低初始生命值到80
    sm->GetContext()->Set("max_health", 100);
    sm->GetContext()->Set("attack_power", 10);
    sm->GetContext()->Set("battle_count", 0);  // 战斗计数器

    return sm;
}

/**
 * @brief 主函数 - 演示状态机使用
 */
int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "     游戏角色状态机演示程序" << std::endl;
    std::cout << "=====================================" << std::endl;
    std::cout << std::endl;

    // 创建状态机
    auto stateMachine = CreateCharacterStateMachine();

    // 启动状态机
    if (!stateMachine->Start()) {
        std::cerr << "状态机启动失败！" << std::endl;
        return 1;
    }

    std::cout << "\n开始模拟游戏循环...\n" << std::endl;

    // 模拟游戏主循环
    int frame = 0;
    const float frameTime = 0.1f; // 每帧0.1秒（10 FPS）
    const int maxFrames = 600;    // 最多运行60秒
    const int maxBattles = 10;    // 最多进行10场战斗

    while (stateMachine->IsRunning() && frame < maxFrames) {
        // 更新状态机
        stateMachine->Update();

        // 检查是否达到战斗次数上限
        auto ctx = stateMachine->GetContext();
        auto battleCount = ctx->Get<int>("battle_count");
        if (battleCount && *battleCount >= maxBattles) {
            std::cout << "\n达到最大战斗次数(" << maxBattles << ")，结束演示。" << std::endl;
            break;
        }

        // 模拟用户输入（每5秒发送一次健康检查）
        if (frame % 50 == 0) {
            Event healthCheck("health_check");
            stateMachine->SendEvent(healthCheck, false); // 延迟处理
        }

        // 等待下一帧
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        frame++;
    }

    if (stateMachine->IsRunning()) {
        std::cout << "\n演示时间结束，停止状态机。" << std::endl;
        stateMachine->Stop();
    }

    // 输出统计信息
    auto ctx = stateMachine->GetContext();
    auto battleCount = ctx->Get<int>("battle_count");
    auto health = ctx->Get<int>("health");

    std::cout << "\n=====================================" << std::endl;
    std::cout << "        演示程序结束" << std::endl;
    std::cout << "-------------------------------------" << std::endl;
    std::cout << "统计信息：" << std::endl;
    std::cout << "  总战斗次数: " << (battleCount ? *battleCount : 0) << std::endl;
    std::cout << "  最终生命值: " << (health ? *health : 0) << "/100" << std::endl;
    std::cout << "  运行帧数: " << frame << " (" << frame * 0.1f << "秒)" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}