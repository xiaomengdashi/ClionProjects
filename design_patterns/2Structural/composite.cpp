#include <iostream>
#include <memory>
#include <vector>
#include <algorithm>

// 组件基类
class Component {
protected:
    Component* parent_;

public:
    Component() : parent_(nullptr) {}
    virtual ~Component() = default;

    void setParent(Component* parent) {
        this->parent_ = parent;
    }

    Component* getParent() const {
        return this->parent_;
    }

    virtual void add(std::unique_ptr<Component> component) {}
    virtual void remove(Component* component) {}

    virtual bool isComposite() const {
        return false;
    }

    virtual std::string operation() const = 0;
};

// 叶子节点
class Leaf : public Component {
public:
    std::string operation() const override {
        return "Leaf";
    }
};

// 组合节点
class Composite : public Component {
protected:
    std::vector<std::unique_ptr<Component>> children_;

public:
    void add(std::unique_ptr<Component> component) override {
        component->setParent(this);
        this->children_.push_back(std::move(component));
    }

    void remove(Component* component) override {
        auto it = std::find_if(children_.begin(), children_.end(),
            [component](const std::unique_ptr<Component>& child) {
                return child.get() == component;
            });
        
        if (it != children_.end()) {
            (*it)->setParent(nullptr);
            children_.erase(it);
        }
    }

    bool isComposite() const override {
        return true;
    }

    std::string operation() const override {
        std::string result = "Branch(";
        for (size_t i = 0; i < children_.size(); ++i) {
            result += children_[i]->operation();
            if (i != children_.size() - 1) {
                result += "+";
            }
        }
        return result + ")";
    }
};

// 客户端代码
void clientCode(Component* component) {
    std::cout << "RESULT: " << component->operation() << std::endl;
}

void clientCode2(Component* component1, Component* component2) {
    if (component1->isComposite()) {
        component1->add(std::make_unique<Leaf>());
    }
    std::cout << "RESULT: " << component1->operation() << std::endl;
}

int main() {
    std::cout << "=== Composite Pattern Demo ===" << std::endl;
    
    // 简单叶子节点
    auto simple = std::make_unique<Leaf>();
    std::cout << "Client: I've got a simple component:" << std::endl;
    clientCode(simple.get());
    std::cout << std::endl;

    // 复杂组合树
    auto tree = std::make_unique<Composite>();
    auto branch1 = std::make_unique<Composite>();
    
    branch1->add(std::make_unique<Leaf>());
    branch1->add(std::make_unique<Leaf>());
    
    auto branch2 = std::make_unique<Composite>();
    branch2->add(std::make_unique<Leaf>());
    
    tree->add(std::move(branch1));
    tree->add(std::move(branch2));
    
    std::cout << "Client: Now I've got a composite tree:" << std::endl;
    clientCode(tree.get());
    std::cout << std::endl;

    std::cout << "Client: I don't need to check the components classes even when managing the tree:" << std::endl;
    clientCode2(tree.get(), simple.get());

    return 0;
}