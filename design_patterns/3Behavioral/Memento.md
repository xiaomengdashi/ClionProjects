备忘录模式（Memento Pattern）是一种行为设计模式，它用于在不暴露对象实现细节的情况下，捕获和外部化对象的内部状态，以便在以后需要时恢复对象的状态。备忘录模式通常用于实现“撤销”操作，允许你保存对象的某个状态并在需要时恢复该状态。

### 备忘录模式的主要组成部分：
1. **发起人（Originator）**：需要保存和恢复状态的对象，负责创建备忘录并使用备忘录恢复状态。
2. **备忘录（Memento）**：存储发起人对象的内部状态，可以是浅拷贝或深拷贝。备忘录只能通过发起人类访问。
3. **管理者（Caretaker）**：负责保存备忘录的对象，但不允许对备忘录的内容进行修改或查看。管理者可以在需要时恢复备忘录中的状态。

### 备忘录模式的实现：

假设我们有一个文本编辑器，用户可以编辑文本并希望能够撤销编辑。我们可以利用备忘录模式来保存文本的状态，并在需要时恢复。

### C++ 实现备忘录模式：

```cpp
#include <iostream>
#include <string>
#include <vector>

// 备忘录类
class Memento {
public:
    Memento(std::string state) : state(state) {}

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
```

### 代码解析：
1. **备忘录类（`Memento`）**：用于存储发起人类（`TextEditor`）的状态。该类通常只提供状态的访问方法，而不允许外部直接修改状态。
2. **发起人类（`TextEditor`）**：它是一个文本编辑器，保存当前文本内容。`createMemento()` 方法创建备忘录对象，`restoreFromMemento()` 方法根据备忘录恢复状态。
3. **管理者类（`Caretaker`）**：负责保存备忘录。在这个示例中，`Caretaker` 类有一个 `mementos` 向量，保存了多个备忘录。它可以提供指定索引的备忘录，用于恢复发起人的状态。
4. **客户端**：客户端代码模拟了一个文本编辑器的使用，首先保存了几个编辑状态，然后通过管理者类恢复到不同的文本状态。

### 输出：
```
文本编辑器的当前内容: Hello, World!
文本编辑器的当前内容: Hello, Memento Pattern!
文本编辑器的当前内容: Hello, Design Patterns!
文本编辑器恢复到状态: Hello, Memento Pattern!
文本编辑器恢复到状态: Hello, World!
```

### 总结：
- **备忘录模式**的核心思想是将对象的内部状态保存到一个独立的对象（备忘录）中，避免对象的外部暴露其内部状态。
- **发起人**可以通过备忘录保存和恢复其状态，但无法直接访问备忘录的内部状态。
- **管理者**类负责保存和检索备忘录，它不应该修改备忘录的内容。
- 备忘录模式常用于需要支持撤销操作的场景，如文本编辑器、图形应用程序等。

### 优缺点：
- **优点**：
    - 允许恢复到先前的状态，可以实现撤销、重做功能。
    - 降低了对象的复杂性，因为发起人不需要知道如何保存和恢复其状态。
- **缺点**：
    - 如果状态过大，备忘录对象可能会消耗大量内存，特别是在频繁保存状态的情况下。
    - 备忘录模式可能导致管理大量备忘录对象，增加了系统的复杂度。

