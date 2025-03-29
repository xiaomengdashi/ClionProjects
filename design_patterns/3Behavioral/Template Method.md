**模板方法设计模式**（Template Method Pattern）是一种行为设计模式，它定义了一个操作中的算法框架，而将一些步骤的实现延迟到子类中。子类可以在不改变算法结构的情况下，重新定义算法的某些特定步骤。

### 模板方法设计模式的关键点：
1. **定义一个模板方法**：在父类中定义一个固定的算法框架，该算法框架由多个步骤组成，其中一些步骤可以由子类来实现。
2. **钩子方法**：子类可以通过覆盖某些钩子方法，定制算法的某些步骤。
3. **不允许修改模板方法**：模板方法通常是固定的，不能被修改，只能通过钩子方法或子类来扩展。

### C++ 示例代码：
以下是一个简单的模板方法模式示例，模拟一个制作咖啡的过程，其中的一些步骤可以由子类（例如，**制作不同种类的咖啡**）来具体实现。

```cpp
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
```

### 代码解释：
1. **CaffeineBeverage 类**：这是一个抽象类，定义了 `prepareRecipe()` 模板方法，该方法包含了制作饮料的步骤。`boilWater()`, `pourInCup()` 是父类已经实现的步骤，而 `brew()` 和 `addCondiments()` 是抽象方法，必须由子类实现。

2. **Coffee 类和 Tea 类**：这两个类是 `CaffeineBeverage` 类的具体实现，分别实现了 `brew()` 和 `addCondiments()` 方法，定义了制作咖啡和茶的具体步骤。

3. **模板方法 (`prepareRecipe`)**：该方法在父类 `CaffeineBeverage` 中定义了一个固定的制作过程。子类通过实现 `brew()` 和 `addCondiments()` 方法来改变某些具体步骤。

### 输出：
```
Making coffee:
Boiling water
Dripping coffee through filter
Pouring into cup
Adding sugar and milk

Making tea:
Boiling water
Steeping the tea
Pouring into cup
Adding lemon
```

### 模板方法模式的优缺点：

#### 优点：
1. **代码复用**：通过定义一个通用的算法框架，减少了代码重复。父类中的不变部分可以复用，而子类只需要关注可变部分。
2. **控制反转**：父类定义了算法的结构，而将具体实现交给了子类，控制了算法流程。
3. **易于维护**：公共的算法部分集中在父类中，减少了代码的冗余。

#### 缺点：
1. **类数增多**：每一个子类都需要一个单独的实现，可能导致类的数量增加。
2. **子类修改困难**：子类只能修改模板方法中的某些步骤，无法改变整个算法的流程。如果要更大程度地定制行为，可能需要通过进一步的继承来扩展。

### 总结：
模板方法模式非常适合当你有多个步骤是固定的，但其中某些步骤又可能有变动的场景。它让我们能够定义一个算法框架并通过子类来定制特定的步骤，同时保持算法的整体结构不变。