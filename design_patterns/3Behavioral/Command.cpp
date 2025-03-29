//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <memory>
#include <utility>

// 命令接口
class Command {
public:
    virtual ~Command() = default;
    virtual void execute() = 0;
};

// 接收者：灯
class Light {
public:
    void turnOn() {
        std::cout << "灯打开了。\n";
    }

    void turnOff() {
        std::cout << "灯关闭了。\n";
    }
};

// 接收者：风扇
class Fan {
public:
    void turnOn() {
        std::cout << "风扇打开了。\n";
    }

    void turnOff() {
        std::cout << "风扇关闭了。\n";
    }
};

// 具体命令：打开灯
class LightOnCommand : public Command {
public:
    explicit LightOnCommand(std::shared_ptr<Light> light) : light(std::move(light)) {}

    void execute() override {
        light->turnOn();
    }

private:
    std::shared_ptr<Light> light;
};

// 具体命令：关闭灯
class LightOffCommand : public Command {
public:
    explicit LightOffCommand(std::shared_ptr<Light> light) : light(std::move(light)) {}

    void execute() override {
        light->turnOff();
    }

private:
    std::shared_ptr<Light> light;
};

// 具体命令：打开风扇
class FanOnCommand : public Command {
public:
    explicit FanOnCommand(std::shared_ptr<Fan> fan) : fan(std::move(fan)) {}

    void execute() override {
        fan->turnOn();
    }

private:
    std::shared_ptr<Fan> fan;
};

// 具体命令：关闭风扇
class FanOffCommand : public Command {
public:
    explicit FanOffCommand(std::shared_ptr<Fan> fan) : fan_(std::move(fan)) {}

    void execute() override {
        fan_->turnOff();
    }

private:
    std::shared_ptr<Fan> fan_;
};

// 调用者：遥控器
class RemoteControl {
public:
    void setCommand(std::shared_ptr<Command> command) {
        command_ = std::move(command);
    }

    void pressButton() {
        command_->execute();
    }

private:
    std::shared_ptr<Command> command_;
};

// 客户端
int main() {
    // 创建接收者
    auto light = std::make_shared<Light>();
    auto fan = std::make_shared<Fan>();

    // 创建命令对象
    auto lightOn = std::make_shared<LightOnCommand>(light);
    auto lightOff = std::make_shared<LightOffCommand>(light);
    auto fanOn = std::make_shared<FanOnCommand>(fan);
    auto fanOff = std::make_shared<FanOffCommand>(fan);

    // 创建遥控器
    RemoteControl remote;

    // 设置并执行命令
    remote.setCommand(lightOn);
    remote.pressButton();  // 灯打开了。

    remote.setCommand(fanOn);
    remote.pressButton();  // 风扇打开了。

    remote.setCommand(lightOff);
    remote.pressButton();  // 灯关闭了。

    remote.setCommand(fanOff);
    remote.pressButton();  // 风扇关闭了。

    return 0;
}