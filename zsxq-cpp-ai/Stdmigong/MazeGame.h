#ifndef MAZEGAME_H
#define MAZEGAME_H

#include <QObject>
#include <QTimer>
#include <QVariantList>
#include <qqml.h>
#include "MazeGenerator.h"

/**
 * @brief 迷宫游戏管理类
 *
 * 这个类作为QML和C++逻辑之间的桥梁
 * 管理游戏状态、玩家位置、计时器和游戏逻辑
 */
class MazeGame : public QObject
{
    Q_OBJECT
    QML_ELEMENT  // Qt6注册到QML的宏

    // QML可访问的属性
    Q_PROPERTY(int mazeWidth READ mazeWidth NOTIFY mazeChanged)
    Q_PROPERTY(int mazeHeight READ mazeHeight NOTIFY mazeChanged)
    Q_PROPERTY(int playerX READ playerX NOTIFY playerPositionChanged)
    Q_PROPERTY(int playerY READ playerY NOTIFY playerPositionChanged)
    Q_PROPERTY(int chaserX READ chaserX NOTIFY chaserPositionChanged)
    Q_PROPERTY(int chaserY READ chaserY NOTIFY chaserPositionChanged)
    Q_PROPERTY(bool chaserActive READ chaserActive NOTIFY chaserActiveChanged)
    Q_PROPERTY(int chaserStartDelay READ chaserStartDelay NOTIFY difficultyChanged)
    Q_PROPERTY(int elapsedTime READ elapsedTime NOTIFY elapsedTimeChanged)
    Q_PROPERTY(bool isGameRunning READ isGameRunning NOTIFY gameStateChanged)
    Q_PROPERTY(bool isGameWon READ isGameWon NOTIFY gameStateChanged)
    Q_PROPERTY(bool isGameLost READ isGameLost NOTIFY gameStateChanged)
    Q_PROPERTY(int shortestPath READ shortestPath NOTIFY shortestPathChanged)
    Q_PROPERTY(int currentSteps READ currentSteps NOTIFY currentStepsChanged)
    Q_PROPERTY(int difficulty READ difficulty NOTIFY difficultyChanged)
    Q_PROPERTY(QVariantList mazeData READ getMazeData NOTIFY mazeChanged)
    Q_PROPERTY(QVariantList optimalPath READ getOptimalPath NOTIFY optimalPathChanged)
    Q_PROPERTY(bool showPath READ showPath NOTIFY showPathChanged)

public:
    explicit MazeGame(QObject *parent = nullptr);

    // 难度级别枚举
    enum Difficulty {
        Easy = 0,    // 简单：延迟15秒出发，移动间隔 1500ms
        Normal = 1,  // 普通：延迟10秒出发，移动间隔 1200ms
        Hard = 2,    // 困难：延迟7秒出发，移动间隔 1000ms
        Expert = 3   // 专家：延迟5秒出发，移动间隔 800ms
    };
    Q_ENUM(Difficulty)

    // 属性getter函数
    int mazeWidth() const { return m_mazeWidth; }
    int mazeHeight() const { return m_mazeHeight; }
    int playerX() const { return m_playerX; }
    int playerY() const { return m_playerY; }
    int chaserX() const { return m_chaserX; }
    int chaserY() const { return m_chaserY; }
    bool chaserActive() const { return m_chaserActive; }
    int chaserStartDelay() const;
    int elapsedTime() const { return m_elapsedTime; }
    bool isGameRunning() const { return m_isGameRunning; }
    bool isGameWon() const { return m_isGameWon; }
    bool isGameLost() const { return m_isGameLost; }
    int shortestPath() const { return m_shortestPath; }
    int currentSteps() const { return m_currentSteps; }
    int difficulty() const { return m_difficulty; }
    bool showPath() const { return m_showPath; }

    /**
     * @brief 从QML调用的函数，用于启动新游戏
     * @param width 迷宫宽度
     * @param height 迷宫高度
     * @param difficulty 游戏难度（可选，默认为Normal）
     */
    Q_INVOKABLE void startNewGame(int width, int height, int difficulty = Normal);

    /**
     * @brief 设置游戏难度
     * @param difficulty 难度级别
     */
    Q_INVOKABLE void setDifficulty(int difficulty);

    /**
     * @brief 从QML调用的函数，用于移动玩家
     * @param direction 方向：0=上, 1=右, 2=下, 3=左
     * @return 是否移动成功
     */
    Q_INVOKABLE bool movePlayer(int direction);

    /**
     * @brief 重置当前游戏（重新开始同一个迷宫）
     */
    Q_INVOKABLE void resetGame();

    /**
     * @brief 获取迷宫数据供QML渲染使用
     * @return QVariantList格式的迷宫数据
     * 返回格式：[
     *   {x: 0, y: 0, topWall: true, rightWall: false, bottomWall: true, leftWall: true},
     *   ...
     * ]
     * @note 现在作为Q_PROPERTY暴露给QML，会在mazeChanged信号时自动更新
     */
    QVariantList getMazeData() const;

    /**
     * @brief 检查是否是起点位置
     */
    Q_INVOKABLE bool isStartPosition(int x, int y) const;

    /**
     * @brief 检查是否是终点位置
     */
    Q_INVOKABLE bool isEndPosition(int x, int y) const;

    /**
     * @brief 计算并显示最优路径
     */
    Q_INVOKABLE void calculateAndShowPath();

    /**
     * @brief 隐藏最优路径
     */
    Q_INVOKABLE void hidePath();

    /**
     * @brief 获取最优路径数据供QML渲染使用
     * @return QVariantList格式的路径数据，每个元素包含{x, y}
     */
    QVariantList getOptimalPath() const;

signals:
    /**
     * @brief 迷宫改变信号（新游戏或重置）
     */
    void mazeChanged();

    /**
     * @brief 玩家位置改变信号
     */
    void playerPositionChanged();

    /**
     * @brief 追逐者位置改变信号
     */
    void chaserPositionChanged();

    /**
     * @brief 追逐者激活状态改变信号
     */
    void chaserActiveChanged();

    /**
     * @brief 游戏时间改变信号
     */
    void elapsedTimeChanged();

    /**
     * @brief 游戏状态改变信号（开始、结束、胜利等）
     */
    void gameStateChanged();

    /**
     * @brief 最短路径改变信号
     */
    void shortestPathChanged();

    /**
     * @brief 当前步数改变信号
     */
    void currentStepsChanged();

    /**
     * @brief 难度改变信号
     */
    void difficultyChanged();

    /**
     * @brief 游戏胜利信号
     * @param isPerfect 是否是完美通关（步数等于最短路径）
     */
    void gameWon(bool isPerfect);

    /**
     * @brief 游戏失败信号（被追逐者抓到）
     */
    void gameLost();

    /**
     * @brief 最优路径改变信号
     */
    void optimalPathChanged();

    /**
     * @brief 路径显示状态改变信号
     */
    void showPathChanged();

private slots:
    /**
     * @brief 定时器槽函数，每秒更新游戏时间
     */
    void updateTimer();

    /**
     * @brief 检查追逐者是否应该激活
     */
    void checkChaserActivation();

    /**
     * @brief 追逐者移动槽函数，按难度定时触发
     */
    void moveChaserTowardsPlayer();

private:
    // 玩家位置结构（必须在使用之前定义）
    struct Position {
        int x;
        int y;
        Position(int x_ = 0, int y_ = 0) : x(x_), y(y_) {}
        bool operator==(const Position& other) const {
            return x == other.x && y == other.y;
        }
    };

    /**
     * @brief 检查玩家是否到达终点
     */
    void checkWinCondition();

    /**
     * @brief 检查追逐者是否抓到玩家
     */
    void checkLoseCondition();

    /**
     * @brief 追逐者沿着玩家轨迹移动到下一个位置
     * @return 是否成功移动
     * @deprecated 已被A*路径规划替代
     */
    bool moveChaserAlongPath();

    /**
     * @brief 计算曼哈顿距离启发式函数
     */
    int heuristic(int x1, int y1, int x2, int y2) const;

    /**
     * @brief 使用A*算法寻找从起点到终点的最短路径
     * @param startX 起点X坐标
     * @param startY 起点Y坐标
     * @param endX 终点X坐标
     * @param endY 终点Y坐标
     * @return 路径中的下一步位置，如果无法移动则返回当前位置
     */
    Position findNextStepToTarget(int startX, int startY, int endX, int endY);

    /**
     * @brief 更新追逐者移动速度（根据难度）
     */
    void updateChaserSpeed();

    /**
     * @brief 获取追逐者延迟出发时间（秒）
     */
    int getChaserDelaySeconds() const;

    MazeGenerator m_generator;     // 迷宫生成器
    QTimer* m_timer;               // 游戏计时器
    QTimer* m_chaserTimer;         // 追逐者移动计时器

    // 游戏状态变量
    int m_mazeWidth;               // 迷宫宽度
    int m_mazeHeight;              // 迷宫高度
    int m_playerX;                 // 玩家X坐标
    int m_playerY;                 // 玩家Y坐标
    int m_chaserX;                 // 追逐者X坐标
    int m_chaserY;                 // 追逐者Y坐标
    int m_startX;                  // 起点X坐标
    int m_startY;                  // 起点Y坐标
    int m_endX;                    // 终点X坐标
    int m_endY;                    // 终点Y坐标
    int m_elapsedTime;             // 已用时间（秒）
    int m_shortestPath;            // 最短路径步数
    int m_currentSteps;            // 当前已走步数
    int m_difficulty;              // 游戏难度
    bool m_isGameRunning;          // 游戏是否正在进行
    bool m_isGameWon;              // 游戏是否胜利
    bool m_isGameLost;             // 游戏是否失败
    bool m_chaserActive;           // 追逐者是否已激活（开始追逐）
    bool m_timerStarted;           // 计时器是否已启动（玩家是否已开始移动）

    QVector<Position> m_playerPath;  // 玩家移动轨迹
    int m_chaserPathIndex;           // 追逐者在轨迹上的位置索引
    QVector<QPoint> m_optimalPath;   // 最优路径（从起点到终点）
    bool m_showPath;                 // 是否显示路径
};

#endif // MAZEGAME_H
