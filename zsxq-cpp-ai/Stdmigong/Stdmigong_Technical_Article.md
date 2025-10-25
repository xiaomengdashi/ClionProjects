一个基于 Qt 6、C++ 和 QML 技术栈开发的桌面迷宫游戏。它不仅仅是一个简单的“走迷宫”程序，是一个集成了多种核心算法、展现了现代 C++/QML 混合编程最佳实践的综合性项目。玩家需要在随机生成的迷宫中，从起点（左上角）移动到终点（右下角），同时还要躲避一个从起点出发、拥有智能寻路能力的“追逐者”。

---

## 1、项目概述与功能介绍


### 1.1 项目能做什么？

*   **动态生成随机迷宫**：每次启动或选择“新游戏”，都会生成一个独一无二的迷宫，保证了游戏的可玩性。
*   **玩家控制与交互**：玩家可以通过键盘（W/A/S/D 或方向键）或界面按钮来控制角色在迷宫中移动。
*   **智能追逐者 (Chaser AI)**：游戏开始一段时间后，一个AI追逐者会从起点出发，利用高效的寻路算法实时追踪并试图拦截玩家，极大地增加了游戏的挑战性和紧迫感。
*   **多难度级别**：内置从“简单”到“专家”四种难度，不同难度对应不同的迷宫尺寸和追逐者移动速度。
*   **游戏状态实时反馈**：界面会实时显示游戏时间、玩家当前步数、理论最短步数以及追逐者的状态。
*   **最优路径提示**：玩家可以选择显示从起点到终点的“最优路径”，路径会以动态、可视化的方式呈现在迷宫上，既是辅助功能，也展示了路径规划算法的结果。

### 1.2 可以学习到哪些知识？

涵盖了以下关键知识点：

1.  **经典算法的C++实现**：
    *   **深度优先搜索 (DFS)**：用于生成迷宫。
    *   **广度优先搜索 (BFS)**：用于计算起点到终点的最短路径。
    *   **A\* (A-Star) 寻路算法**：为追逐者 AI 提供高效、智能的实时路径规划能力。
2.  **现代 Qt C++/QML 混合编程**：
    *   如何通过 `Q_PROPERTY` 将 C++ 对象的属性暴露给 QML，实现数据绑定。
    *   如何使用 `Q_INVOKABLE` 让 QML 可以直接调用 C++ 对象的成员函数。
    *   如何利用 `signals` 和 `slots` 机制实现 C++ 后端与 QML 前端之间的解耦通信。
    *   `QML_ELEMENT` 宏在 Qt 6 中的应用，简化 C++ 类型注册流程。
3.  **MVVM 架构思想**：项目中 `MazeGame` 类承担了 ViewModel 的角色，它连接了作为 Model 的 `MazeGenerator` 和作为 View 的 `Main.qml`，实现了业务逻辑与UI展现的清晰分离。
4.  **QML 高级应用**：
    *   使用 `Repeater` 动态生成复杂的UI布局（迷宫网格）。
    *   通过属性绑定和 `Behavior` 实现流畅的动画效果。
    *   使用 `Canvas` 进行自定义绘图，实现最优路径的动态可视化。
    *   `Keys.onPressed` 事件处理，实现键盘输入控制。

---

## 2、核心算法

算法是这个项目的灵魂。精巧地运用了三种核心算法来驱动游戏的主要逻辑。

### 2.1 迷宫生成：深度优先搜索 (DFS)

迷宫的生成算法决定了迷宫的形态和复杂度。本项目采用了经典的“递归回溯”版的深度优先搜索算法来生成一个“完美迷宫”（即迷宫内任意两点之间有且仅有一条路径）。

**核心逻辑位于 `MazeGenerator::generateDFS(int x, int y)`：**

1.  将当前单元格标记为“已访问”。
2.  获取当前单元格所有未被访问过的邻居，并将它们的顺序随机打乱。
3.  遍历这些随机排序的邻居：
    *   移除当前单元格与该邻居之间的墙壁。
    *   以该邻居为新的当前单元格，递归调用 `generateDFS`。

这个过程就像一个探索者在黑暗中摸索，他会沿着一条路走到尽头，然后回溯到上一个路口，选择另一条没走过的路继续探索，直到所有单元格都被访问过。

**关键源代码 (`MazeGenerator.cpp`)**
```cpp
// ...
void MazeGenerator::generateDFS(int x, int y)
{
    // 1. 标记当前单元格为已访问
    m_maze[y][x].visited = true;

    // 定义四个方向并随机打乱
    QVector<Direction> directions = {
        {0, -1}, {1, 0}, {0, 1}, {-1, 0}
    };
    std::shuffle(directions.begin(), directions.end(), m_rng);

    // 2. 尝试每个方向
    for (const auto& dir : directions) {
        int nx = x + dir.dx;
        int ny = y + dir.dy;

        // 检查新位置是否在边界内且未被访问
        if (isInBounds(nx, ny) && !m_maze[ny][nx].visited) {
            // 3. 移除墙壁
            removeWall(x, y, nx, ny);
            // 4. 递归访问
            generateDFS(nx, ny);
        }
    }
}
// ...
```

### 2.2 最短路径计算：广度优先搜索 (BFS)

为了给玩家提供“完美通关”的目标（即以最少步数完成游戏），程序需要计算出从起点到终点的最短路径长度。广度优先搜索（BFS）是解决无权图中单源最短路径问题的完美算法。

**核心逻辑位于 `MazeGenerator::calculateShortestPath(...)` 和 `MazeGenerator::getFullPath(...)`：**

1.  创建一个队列，并将起点入队。
2.  创建一个集合，用于记录已访问的节点，避免重复搜索。
3.  当队列不为空时，循环执行：
    *   将队首元素出队作为当前节点。
    *   如果当前节点是终点，则搜索完成。
    *   遍历当前节点所有可通行的（没有墙阻挡且未访问过的）邻居，将它们入队，并标记为已访问。

由于BFS是逐层向外扩展的，所以它找到的第一条到达终点的路径，必然是步数最少的路径。`getFullPath` 在此基础上增加了一个 `parentMap`，用于记录每个节点的“来源”节点，从而在找到终点后能够回溯并重建整条路径。

**关键源代码 (`MazeGenerator.cpp`)**
```cpp
// ...
int MazeGenerator::calculateShortestPath(int startX, int startY, int endX, int endY) const
{
    // ...
    QQueue<Node> queue; // Node 包含 {x, y, distance}
    queue.enqueue({startX, startY, 0});
    visited.insert({startX, startY});

    while (!queue.isEmpty()) {
        Node current = queue.dequeue();

        if (current.x == endX && current.y == endY) {
            return current.distance; // 找到终点，返回距离
        }

        // 遍历四个方向
        for (int i = 0; i < 4; ++i) {
            // ... 检查是否可以移动
            if (canMove) {
                visited.insert({nx, ny});
                queue.enqueue({nx, ny, current.distance + 1}); // 邻居入队，距离+1
            }
        }
    }
    return -1; // 无法到达
}
// ...
```

### 2.3 追逐者 AI：A\* 寻路算法

这是项目中最具技术亮点的部分。为了让追逐者显得“智能”而不是盲目地乱撞或简单地跟随玩家的足迹，项目采用了著名的 A\* 寻路算法。A\* 算法是一种启发式搜索算法，它能以极高的效率在图中找到最优路径。

**核心逻辑位于 `MazeGame::findNextStepToTarget(...)`：**

A\* 算法的核心在于其评估函数：`f(n) = g(n) + h(n)`

*   `g(n)`: 从起点到节点 `n` 的实际移动代价。
*   `h(n)`: 从节点 `n` 到终点的**估算**代价（启发式函数）。一个好的启发式函数是 A\* 算法效率的关键。本项目中使用了**曼哈顿距离**作为启发式函数，因为它非常适合在网格地图中使用。
*   `f(n)`: 节点的综合评估值。A\* 算法总是优先探索 `f(n)` 值最小的节点。

**算法步骤：**

1.  维护一个“开放列表”（`openList`，存储待探索的节点）和一个“关闭列表”（`closedSet`，存储已探索的节点）。
2.  将起点放入开放列表。
3.  当开放列表不为空时，循环执行：
    *   从开放列表中取出 `f(n)` 值最小的节点 `n` 作为当前节点，并将其移入关闭列表。
    *   如果 `n` 是终点（即玩家当前位置），则路径已找到。
    *   遍历 `n` 的所有可通行邻居：
        *   如果邻居在关闭列表中，则忽略。
        *   计算邻居的 `g` 值和 `h` 值，从而得到 `f` 值。
        *   如果邻居不在开放列表中，或者新的路径到该邻居更优（`g` 值更小），则更新该邻居的信息，并将其放入开放列表。
4.  路径找到后，从终点开始，沿着每个节点的 `parent` 指针回溯，即可得到从起点到终点的完整路径。

追逐者在每次移动时，都会以自身位置为起点，以玩家的当前位置为终点，实时运行一次 A\* 算法，然后向着规划出路径的**下一步**移动。这使得追逐者总能以最短的路线逼近玩家。

**关键源代码 (`MazeGame.cpp`)**
```cpp
// ...
MazeGame::Position MazeGame::findNextStepToTarget(int startX, int startY, int endX, int endY)
{
    // A* 节点结构: {x, y, g, f, parent}
    // ...
    QMultiMap<int, Node*> openList; // 使用 QMultiMap 模拟优先队列
    QSet<QPair<int,int>> closedSet;
    
    // ... A* 主循环
    while (!openList.isEmpty()) {
        // 1. 取出f值最小的节点
        Node* current = openList.begin().value();
        openList.erase(openList.begin());

        if (current->x == endX && current->y == endY) {
            targetNode = current; // 2. 找到目标
            break;
        }

        closedSet.insert(qMakePair(current->x, current->y));

        // 3. 探索邻居
        for (int i = 0; i < 4; ++i) {
            // ...
            // 计算 g, h, f 值并更新或加入 openList
        }
    }

    // 4. 回溯路径找到第一步
    if (targetNode != nullptr) {
        // ... 回溯逻辑 ...
    }
    
    return nextStep;
}
// ...
```
---

## 3、核心组件

项目的优雅架构体现在其清晰的组件划分上。

### 3.1 `MazeGenerator`

这个类是纯粹的“数据模型”，它不关心游戏逻辑，只负责生成和提供迷宫数据。

*   **`struct Cell`**: 定义了迷宫的基本单元，包含了四个方向的墙壁状态和生成算法所需的 `visited` 标记。
*   **`m_maze`**: `QVector<QVector<Cell>>` 类型的二维数组，是迷宫数据的核心存储结构。
*   **`generate()`**: 核心API，调用 `generateDFS` 创建新迷宫。
*   **`getCell()` / `hasWallBetween()`**: 提供给外部查询迷宫结构（墙壁信息）的接口。
*   **`calculateShortestPath()` / `getFullPath()`**: 提供路径计算服务的接口。

### 3.2 `MazeGame` - 游戏的大脑

这是连接 C++ 逻辑与 QML 界面的核心桥梁，是典型的 ViewModel。

*   **状态管理**: 它包含了游戏的所有状态变量，如 `m_playerX`, `m_mazeWidth`, `m_isGameRunning` 等。
*   **QML 暴露**:
    *   通过 `Q_PROPERTY`，QML可以直接绑定并监听这些状态的变化。例如，QML中的玩家图标 `x` 坐标可以直接绑定到 `game.playerX`。
    ```cpp
    // MazeGame.h
    Q_PROPERTY(int playerX READ playerX NOTIFY playerPositionChanged)
    ```
    *   通过 `Q_INVOKABLE`，QML中的事件（如按钮点击）可以直接调用C++函数。
    ```cpp
    // MazeGame.h
    Q_INVOKABLE bool movePlayer(int direction);
    ```
*   **游戏逻辑**:
    *   `startNewGame()`: 初始化游戏世界。
    *   `movePlayer()`: 处理玩家移动请求，检查碰撞，更新状态，并触发胜利/失败条件检查。
    *   `moveChaserTowardsPlayer()`: 定时触发，调用 A\* 算法并更新追逐者位置。
*   **数据转换**: `getMazeData()` 函数将C++的 `Cell` 结构体二维数组转换成 QML `Repeater` 能够直接使用的 `QVariantList`，这是 C++/QML 数据交互的关键一步。

### 3.3 `Main.qml` - 华丽的舞台 (View)

QML 文件以其声明式的语法，优雅地构建了整个用户界面。

*   **游戏实例**: 首先创建了一个 `MazeGame` 的实例，这是所有UI元素的“数据源”。
```qml
// Main.qml
MazeGame {
    id: game
    // ...
}
```
*   **迷宫渲染**: 使用 `Repeater` 控件，它绑定到 `game.mazeData` 属性。`mazeData` 列表中的每一个元素（代表一个Cell），都会生成一个对应的 `delegate` 组件（代表一个单元格的UI）。
```qml
// Main.qml
Repeater {
    id: mazeRepeater
    model: game.mazeData // 绑定数据模型

    delegate: Item { // 单元格模板
        // ...
        // 根据 modelData.topWall 等属性显示或隐藏墙壁
        Rectangle { visible: modelData.topWall /* ... */ }
        // ...
    }
}
```
*   **玩家与追逐者**: 它们是两个 `Rectangle` 组件，其 `x` 和 `y` 属性直接绑定到 `game.playerX` 和 `game.chaserX` 等属性，并乘以每个单元格的尺寸。当C++中这些属性改变时，QML中的图标会自动、平滑地移动到新位置（得益于 `Behavior on x` 动画）。
```qml
// Main.qml
Rectangle {
    id: player
    // 核心绑定：将UI坐标与C++逻辑坐标关联
    x: game.playerX * mazeContainer.cellSize + ... 
    y: game.playerY * mazeContainer.cellSize + ...
    
    // 平滑移动动画
    Behavior on x { NumberAnimation { duration: 150 } }
    Behavior on y { NumberAnimation { duration: 150 } }
}
```
*   **事件处理**: 键盘事件通过 `Keys.onPressed` 捕获，并直接调用 `game.movePlayer()` C++ 函数，完成一次完整的 `View -> ViewModel` 的交互。

### 3.4 `main.cpp` - 应用程序入口

它的职责很简单：创建Qt应用实例，实例化QML引擎，并加载主QML文件。在Qt 6中，由于 `MazeGame.h` 中使用了 `QML_ELEMENT` 宏，`MazeGame` 类型会被自动注册到QML模块（在 `CMakeLists.txt` 中定义的 `Stdmigong` 模块），无需再手动调用 `qmlRegisterType`。
```cpp
// main.cpp
// ...
engine.loadFromModule("Stdmigong", "Main");
// ...
```

---

## 4、总结

项目虽然规模不大，但“五脏俱全”。它不仅是一个有趣的游戏，更是一个教科书级别的 Qt C++/QML 开发范例。它清晰地展示了如何进行关注点分离，将复杂的算法逻辑封装在C++后端，将灵活多变的UI表现层交给QML，并通过Qt强大的元对象系统将两者无缝粘合。无论是对于Qt初学者还是有经验的开发者，深入研究此项目的源代码，都将在算法应用、软件架构和跨语言编程方面获得宝贵的经验。