#include "MazeGame.h"
#include <QVariantMap>

/**
 * @brief 构造函数
 */
MazeGame::MazeGame(QObject *parent)
    : QObject(parent)
    , m_mazeWidth(0)
    , m_mazeHeight(0)
    , m_playerX(0)
    , m_playerY(0)
    , m_chaserX(0)
    , m_chaserY(0)
    , m_startX(0)
    , m_startY(0)
    , m_endX(0)
    , m_endY(0)
    , m_elapsedTime(0)
    , m_shortestPath(0)
    , m_currentSteps(0)
    , m_difficulty(Normal)
    , m_isGameRunning(false)
    , m_isGameWon(false)
    , m_isGameLost(false)
    , m_chaserActive(false)
    , m_chaserPathIndex(0)
    , m_showPath(false)
    , m_timerStarted(false)
{
    // 创建计时器
    m_timer = new QTimer(this);
    m_timer->setInterval(1000);  // 每秒触发一次
    connect(m_timer, &QTimer::timeout, this, &MazeGame::updateTimer);

    // 创建追逐者移动计时器
    m_chaserTimer = new QTimer(this);
    connect(m_chaserTimer, &QTimer::timeout, this, &MazeGame::moveChaserTowardsPlayer);
}

/**
 * @brief 启动新游戏
 */
void MazeGame::startNewGame(int width, int height, int difficulty)
{
    // 停止当前游戏
    m_timer->stop();
    m_chaserTimer->stop();
    m_isGameRunning = false;
    m_isGameWon = false;
    m_isGameLost = false;
    m_chaserActive = false;

    // 设置游戏难度
    m_difficulty = difficulty;

    // 设置迷宫尺寸
    m_mazeWidth = width;
    m_mazeHeight = height;

    // 生成新迷宫
    m_generator.generate(width, height);

    // 设置起点和终点
    m_startX = 0;
    m_startY = 0;
    m_endX = width - 1;
    m_endY = height - 1;

    // 计算最短路径
    m_shortestPath = m_generator.calculateShortestPath(m_startX, m_startY, m_endX, m_endY);

    // 重置玩家位置到起点
    m_playerX = m_startX;
    m_playerY = m_startY;

    // 设置追逐者初始位置（在起点，与玩家一起）
    m_chaserX = m_startX;
    m_chaserY = m_startY;

    // 清空玩家轨迹并添加起点
    m_playerPath.clear();
    m_playerPath.append(Position(m_startX, m_startY));
    m_chaserPathIndex = 0;

    // 重置计时器和步数
    m_elapsedTime = 0;
    m_currentSteps = 0;

    // 开始游戏
    m_isGameRunning = true;
    m_timerStarted = false;  // 计时器将在玩家首次移动时启动

    // 根据难度设置追逐者移动速度，但不立即启动
    updateChaserSpeed();

    // 发送信号通知QML更新
    emit mazeChanged();
    emit playerPositionChanged();
    emit chaserPositionChanged();
    emit chaserActiveChanged();
    emit elapsedTimeChanged();
    emit gameStateChanged();
    emit shortestPathChanged();
    emit currentStepsChanged();
    emit difficultyChanged();
}

/**
 * @brief 重置当前游戏
 */
void MazeGame::resetGame()
{
    // 停止计时器
    m_timer->stop();
    m_chaserTimer->stop();

    // 重置玩家位置到起点
    m_playerX = m_startX;
    m_playerY = m_startY;

    // 重置追逐者位置到起点
    m_chaserX = m_startX;
    m_chaserY = m_startY;
    m_chaserActive = false;

    // 清空玩家轨迹并添加起点
    m_playerPath.clear();
    m_playerPath.append(Position(m_startX, m_startY));
    m_chaserPathIndex = 0;

    // 重置计时器和步数
    m_elapsedTime = 0;
    m_currentSteps = 0;

    // 重置游戏状态
    m_isGameRunning = true;
    m_isGameWon = false;
    m_isGameLost = false;
    m_timerStarted = false;  // 计时器将在玩家首次移动时启动

    // 发送信号
    emit playerPositionChanged();
    emit chaserPositionChanged();
    emit chaserActiveChanged();
    emit elapsedTimeChanged();
    emit gameStateChanged();
    emit currentStepsChanged();
}

/**
 * @brief 移动玩家
 * @param direction 0=上, 1=右, 2=下, 3=左
 * @return 是否移动成功
 */
bool MazeGame::movePlayer(int direction)
{
    // 如果游戏未运行或已胜利或已失败，不允许移动
    if (!m_isGameRunning || m_isGameWon || m_isGameLost) {
        return false;
    }

    // 计算新位置
    int newX = m_playerX;
    int newY = m_playerY;

    switch (direction) {
    case 0:  // 上
        newY--;
        break;
    case 1:  // 右
        newX++;
        break;
    case 2:  // 下
        newY++;
        break;
    case 3:  // 左
        newX--;
        break;
    default:
        return false;
    }

    // 检查新位置是否在边界内
    if (newX < 0 || newX >= m_mazeWidth || newY < 0 || newY >= m_mazeHeight) {
        return false;
    }

    // 检查是否有墙阻挡
    if (m_generator.hasWallBetween(m_playerX, m_playerY, newX, newY)) {
        return false;
    }

    // 移动成功，更新玩家位置
    m_playerX = newX;
    m_playerY = newY;
    m_currentSteps++;

    // 记录玩家移动轨迹
    Position newPos(newX, newY);
    // 避免重复记录相同位置
    if (m_playerPath.isEmpty() || !(m_playerPath.last() == newPos)) {
        m_playerPath.append(newPos);
    }

    // 如果是首次移动，启动计时器
    if (!m_timerStarted) {
        m_timerStarted = true;
        m_timer->start();
    }

    // 发送位置改变信号
    emit playerPositionChanged();
    emit currentStepsChanged();

    // 检查是否被追上
    checkLoseCondition();

    // 检查是否胜利
    checkWinCondition();

    return true;
}

/**
 * @brief 检查胜利条件
 */
void MazeGame::checkWinCondition()
{
    if (m_playerX == m_endX && m_playerY == m_endY) {
        // 玩家到达终点
        m_isGameWon = true;
        m_isGameRunning = false;
        m_timer->stop();

        // 检查是否是完美通关（步数等于最短路径）
        bool isPerfect = (m_currentSteps == m_shortestPath);

        // 发送胜利信号
        emit gameStateChanged();
        emit gameWon(isPerfect);
    }
}

/**
 * @brief 定时器更新函数
 */
void MazeGame::updateTimer()
{
    if (m_isGameRunning && !m_isGameWon && !m_isGameLost) {
        m_elapsedTime++;
        emit elapsedTimeChanged();

        // 检查追逐者是否应该激活
        checkChaserActivation();
    }
}

/**
 * @brief 获取迷宫数据供QML使用
 */
QVariantList MazeGame::getMazeData() const
{
    QVariantList result;

    for (int y = 0; y < m_mazeHeight; ++y) {
        for (int x = 0; x < m_mazeWidth; ++x) {
            const Cell& cell = m_generator.getCell(x, y);

            QVariantMap cellData;
            cellData["x"] = x;
            cellData["y"] = y;
            cellData["topWall"] = cell.topWall;
            cellData["rightWall"] = cell.rightWall;
            cellData["bottomWall"] = cell.bottomWall;
            cellData["leftWall"] = cell.leftWall;

            result.append(cellData);
        }
    }

    return result;
}

/**
 * @brief 检查是否是起点位置
 */
bool MazeGame::isStartPosition(int x, int y) const
{
    return x == m_startX && y == m_startY;
}

/**
 * @brief 检查是否是终点位置
 */
bool MazeGame::isEndPosition(int x, int y) const
{
    return x == m_endX && y == m_endY;
}

/**
 * @brief 设置游戏难度
 */
void MazeGame::setDifficulty(int difficulty)
{
    if (m_difficulty != difficulty) {
        m_difficulty = difficulty;
        updateChaserSpeed();
        emit difficultyChanged();
    }
}

/**
 * @brief 根据难度更新追逐者移动速度
 * 优化：极速追逐模式，大幅提升移动速度，增强追逐压力
 */
void MazeGame::updateChaserSpeed()
{
    int interval;
    switch (m_difficulty) {
    case Easy:
        interval = 300;  // 0.3秒移动一次 - 快速追逐
        break;
    case Normal:
        interval = 250;  // 0.25秒移动一次 - 非常快
        break;
    case Hard:
        interval = 200;  // 0.2秒移动一次 - 极速追逐
        break;
    case Expert:
        interval = 150;  // 0.15秒移动一次 - 闪电般的速度
        break;
    default:
        interval = 250;
        break;
    }
    m_chaserTimer->setInterval(interval);
}

/**
 * @brief 获取追逐者延迟出发时间
 * 优化：固定为5秒，提升游戏紧张感
 */
int MazeGame::getChaserDelaySeconds() const
{
    return 5;  // 固定5秒后开始追逐
}

/**
 * @brief 获取追逐者延迟出发时间（给QML使用）
 */
int MazeGame::chaserStartDelay() const
{
    return getChaserDelaySeconds();
}

/**
 * @brief 检查追逐者是否应该激活
 */
void MazeGame::checkChaserActivation()
{
    if (!m_chaserActive && m_elapsedTime >= getChaserDelaySeconds()) {
        m_chaserActive = true;
        m_chaserTimer->start();
        emit chaserActiveChanged();
    }
}

/**
 * @brief 检查追逐者是否抓到玩家
 */
void MazeGame::checkLoseCondition()
{
    if (m_playerX == m_chaserX && m_playerY == m_chaserY) {
        // 玩家被追上，游戏失败
        m_isGameLost = true;
        m_isGameRunning = false;
        m_timer->stop();
        m_chaserTimer->stop();

        // 发送失败信号
        emit gameStateChanged();
        emit gameLost();
    }
}

/**
 * @brief 计算曼哈顿距离启发式函数
 */
int MazeGame::heuristic(int x1, int y1, int x2, int y2) const
{
    return qAbs(x1 - x2) + qAbs(y1 - y2);
}

/**
 * @brief 追逐者沿着玩家轨迹移动到下一个位置
 */
bool MazeGame::moveChaserAlongPath()
{
    // 如果追逐者还在轨迹上的有效位置
    if (m_chaserPathIndex < m_playerPath.size() - 1) {
        // 移动到下一个轨迹点
        m_chaserPathIndex++;
        Position nextPos = m_playerPath[m_chaserPathIndex];
        m_chaserX = nextPos.x;
        m_chaserY = nextPos.y;
        return true;
    }

    // 已经追上玩家的最新位置
    return false;
}

/**
 * @brief 使用A*算法寻找到目标的下一步位置
 * 优化：实时规划到玩家当前位置的最短路径
 */
MazeGame::Position MazeGame::findNextStepToTarget(int startX, int startY, int endX, int endY)
{
    // A*算法节点结构
    struct Node {
        int x, y;           // 坐标
        int g;              // 从起点到当前节点的实际代价
        int f;              // f = g + h（总代价）
        Node* parent;       // 父节点指针

        Node(int x_, int y_, int g_, int h, Node* p = nullptr)
            : x(x_), y(y_), g(g_), f(g_ + h), parent(p) {}
    };

    // 如果已经在目标位置，不移动
    if (startX == endX && startY == endY) {
        return Position(startX, startY);
    }

    // 开放列表（使用QMap模拟优先队列，key是f值）
    QMultiMap<int, Node*> openList;
    // 关闭列表（已访问的节点）
    QSet<QPair<int,int>> closedSet;
    // 用于管理内存的节点列表
    QVector<Node*> allNodes;

    // 创建起始节点
    Node* startNode = new Node(startX, startY, 0, heuristic(startX, startY, endX, endY));
    allNodes.append(startNode);
    openList.insert(startNode->f, startNode);

    Node* targetNode = nullptr;

    // A*主循环
    while (!openList.isEmpty()) {
        // 取出f值最小的节点
        auto it = openList.begin();
        Node* current = it.value();
        openList.erase(it);

        // 检查是否到达目标
        if (current->x == endX && current->y == endY) {
            targetNode = current;
            break;
        }

        // 标记为已访问
        closedSet.insert(qMakePair(current->x, current->y));

        // 探索四个方向的相邻节点
        const int dx[] = {0, 1, 0, -1};  // 右、下、左、上
        const int dy[] = {-1, 0, 1, 0};

        for (int i = 0; i < 4; ++i) {
            int nx = current->x + dx[i];
            int ny = current->y + dy[i];

            // 检查边界
            if (nx < 0 || nx >= m_mazeWidth || ny < 0 || ny >= m_mazeHeight) {
                continue;
            }

            // 检查是否已访问
            if (closedSet.contains(qMakePair(nx, ny))) {
                continue;
            }

            // 检查是否有墙阻挡
            if (m_generator.hasWallBetween(current->x, current->y, nx, ny)) {
                continue;
            }

            // 计算新节点的代价
            int newG = current->g + 1;
            int h = heuristic(nx, ny, endX, endY);

            // 创建新节点
            Node* neighbor = new Node(nx, ny, newG, h, current);
            allNodes.append(neighbor);
            openList.insert(neighbor->f, neighbor);
        }
    }

    Position nextStep(startX, startY);  // 默认不移动

    // 如果找到路径，回溯找到第一步
    if (targetNode != nullptr) {
        Node* step = targetNode;
        // 回溯到起点的下一步
        while (step->parent != nullptr && step->parent->parent != nullptr) {
            step = step->parent;
        }
        // step现在是起点的下一步
        if (step->parent != nullptr) {
            nextStep = Position(step->x, step->y);
        }
    }

    // 清理内存
    qDeleteAll(allNodes);

    return nextStep;
}

/**
 * @brief 追逐者移动槽函数
 * 优化：使用A*算法实时规划到玩家当前位置的最短路径
 */
void MazeGame::moveChaserTowardsPlayer()
{
    // 如果游戏未运行或已结束或追逐者未激活，不移动
    if (!m_isGameRunning || m_isGameWon || m_isGameLost || !m_chaserActive) {
        return;
    }

    // 使用A*算法找到到玩家当前位置的下一步
    Position nextStep = findNextStepToTarget(m_chaserX, m_chaserY, m_playerX, m_playerY);

    // 如果找到有效的下一步，移动追逐者
    if (nextStep.x != m_chaserX || nextStep.y != m_chaserY) {
        m_chaserX = nextStep.x;
        m_chaserY = nextStep.y;

        // 发送位置改变信号
        emit chaserPositionChanged();

        // 检查是否抓到玩家
        checkLoseCondition();
    }
}

/**
 * @brief 计算并显示最优路径
 */
void MazeGame::calculateAndShowPath()
{
    // 计算从起点到终点的最优路径
    m_optimalPath = m_generator.getFullPath(m_startX, m_startY, m_endX, m_endY);

    // 设置显示标志
    m_showPath = true;

    // 发送信号通知QML更新
    emit optimalPathChanged();
    emit showPathChanged();
}

/**
 * @brief 隐藏最优路径
 */
void MazeGame::hidePath()
{
    m_showPath = false;
    emit showPathChanged();
}

/**
 * @brief 获取最优路径数据供QML使用
 */
QVariantList MazeGame::getOptimalPath() const
{
    QVariantList result;

    for (const QPoint& point : m_optimalPath) {
        QVariantMap pointData;
        pointData["x"] = point.x();
        pointData["y"] = point.y();
        result.append(pointData);
    }

    return result;
}
