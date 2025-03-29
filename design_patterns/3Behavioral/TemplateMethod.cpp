//
// Created by Kolane on 2025/3/29.
//

#include <iostream>

// 抽象类：定义模板方法
class CaffeineBeverage {
public:
    // 模板方法
    void prepareRecipe() {
        boilWater();
        brew();
        pourInCup();
        addCondiments();
    }

protected:
    virtual void brew() = 0;                // 子类必须实现的步骤
    virtual void addCondiments() = 0;        // 子类必须实现的步骤

    void boilWater() {                      // 父类实现的步骤
        std::cout << "Boiling water" << std::endl;
    }

    void pourInCup() {                      // 父类实现的步骤
        std::cout << "Pouring into cup" << std::endl;
    }
};

// 具体类：咖啡
class Coffee : public CaffeineBeverage {
protected:
    void brew() override {
        std::cout << "Dripping coffee through filter" << std::endl;
    }

    void addCondiments() override {
        std::cout << "Adding sugar and milk" << std::endl;
    }
};

// 具体类：茶
class Tea : public CaffeineBeverage {
protected:
    void brew() override {
        std::cout << "Steeping the tea" << std::endl;
    }

    void addCondiments() override {
        std::cout << "Adding lemon" << std::endl;
    }
};

int main() {
    std::cout << "Making coffee:" << std::endl;
    CaffeineBeverage* coffee = new Coffee();
    coffee->prepareRecipe();  // 使用模板方法

    std::cout << "\nMaking tea:" << std::endl;
    CaffeineBeverage* tea = new Tea();
    tea->prepareRecipe();  // 使用模板方法

    delete coffee;
    delete tea;

    return 0;
}