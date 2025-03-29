//
// Created by Kolane on 2025/3/29.
//

#include <iostream>
#include <string>
#include <utility>
#include <vector>

// 备忘录类
class Memento {
public:
    explicit Memento(std::string state) : state(std::move(state)) {}

    std::string getState() const {
        return state;
    }

private:
    std::string state;  // 保存的状态
};

// 发起人类：文本编辑器
class TextEditor {
public:
    void setText(const std::string& newText) {
        text = newText;
        std::cout << "文本编辑器的当前内容: " << text << std::endl;
    }

    std::string getText() const {
        return text;
    }

    // 创建备忘录
    Memento createMemento() {
        return Memento(text);
    }

    // 恢复状态
    void restoreFromMemento(const Memento& memento) {
        text = memento.getState();
        std::cout << "文本编辑器恢复到状态: " << text << std::endl;
    }

private:
    std::string text;
};

// 管理者类：保存和恢复备忘录
class Caretaker {
public:
    void addMemento(const Memento& memento) {
        mementos.push_back(memento);
    }

    Memento getMemento(int index) const {
        return mementos[index];
    }

private:
    std::vector<Memento> mementos;  // 存储备忘录的集合
};

// 客户端代码
int main() {
    TextEditor editor;
    Caretaker caretaker;

    editor.setText("Hello, World!");
    caretaker.addMemento(editor.createMemento());

    editor.setText("Hello, Memento Pattern!");
    caretaker.addMemento(editor.createMemento());

    editor.setText("Hello, Design Patterns!");

    // 恢复到上一个备忘录的状态
    editor.restoreFromMemento(caretaker.getMemento(1));

    // 恢复到最初的状态
    editor.restoreFromMemento(caretaker.getMemento(0));

    return 0;
}