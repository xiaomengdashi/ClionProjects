命令模式（Command Pattern）是一种行为设计模式，它将请求（命令）封装成一个对象，从而使你可以用不同的请求对客户进行参数化。命令模式允许你将请求、调用者和接收者解耦。

### 命令模式的组成部分：
1. **命令（Command）**：声明执行操作的接口，通常包括一个 `execute()` 方法。
2. **具体命令（ConcreteCommand）**：实现了 `Command` 接口并将请求委托给具体的接收者。
3. **接收者（Receiver）**：执行与请求相关的操作。
4. **调用者（Invoker）**：调用命令对象来执行请求，通常会持有一个命令对象。
5. **客户端（Client）**：创建一个具体命令对象，并设置适当的接收者。

### 命令模式的实现：

假设我们有一个遥控器来控制家电设备，遥控器可以开关灯和风扇等设备，我们通过命令模式将这些操作进行封装。

### C++ 实现命令模式：

```cpp
#include <iostream>
#include <memory>

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
    LightOnCommand(std::shared_ptr<Light> light) : light(light) {}

    void execute() override {
        light->turnOn();
    }

private:
    std::shared_ptr<Light> light;
};

// 具体命令：关闭灯
class LightOffCommand : public Command {
public:
    LightOffCommand(std::shared_ptr<Light> light) : light(light) {}

    void execute() override {
        light->turnOff();
    }

private:
    std::shared_ptr<Light> light;
};

// 具体命令：打开风扇
class FanOnCommand : public Command {
public:
    FanOnCommand(std::shared_ptr<Fan> fan) : fan(fan) {}

    void execute() override {
        fan->turnOn();
    }

private:
    std::shared_ptr<Fan> fan;
};

// 具体命令：关闭风扇
class FanOffCommand : public Command {
public:
    FanOffCommand(std::shared_ptr<Fan> fan) : fan(fan) {}

    void execute() override {
        fan->turnOff();
    }

private:
    std::shared_ptr<Fan> fan;
};

// 调用者：遥控器
class RemoteControl {
public:
    void setCommand(std::shared_ptr<Command> command) {
        this->command = command;
    }

    void pressButton() {
        command->execute();
    }

private:
    std::shared_ptr<Command> command;
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
```

### 代码解析：
1. **命令接口 `Command`**：声明了一个 `execute()` 方法，所有的命令类都需要实现这个方法。
2. **具体命令类**：
    - `LightOnCommand`：实现了 `execute()` 方法，调用 `Light` 对象的 `turnOn()` 方法来打开灯。
    - `LightOffCommand`：实现了 `execute()` 方法，调用 `Light` 对象的 `turnOff()` 方法来关闭灯。
    - `FanOnCommand` 和 `FanOffCommand` 类似，分别控制风扇的开关。
3. **接收者 `Light` 和 `Fan`**：这两个类分别表示具体的设备，包含操作设备的逻辑，如打开或关闭灯、风扇。
4. **调用者 `RemoteControl`**：遥控器类负责调用命令对象。它通过 `setCommand()` 方法设置一个命令，并通过 `pressButton()` 方法执行该命令。
5. **客户端 `main()`**：客户端创建了所有的命令、接收者，并通过遥控器来执行不同的操作。

### 输出结果：
```
灯打开了。
风扇打开了。
灯关闭了。
风扇关闭了。
```

### 总结：
- **命令模式**的关键在于将请求封装成对象，使得请求的发送者和接收者解耦。
- 调用者不需要知道命令是如何执行的，只需要调用命令对象的 `execute()` 方法。
- 通过命令模式，可以很容易地扩展不同的命令，也可以为每个命令添加不同的操作。