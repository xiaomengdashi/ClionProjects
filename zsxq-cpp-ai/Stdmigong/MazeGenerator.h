#ifndef MAZEGENERATOR_H
#define MAZEGENERATOR_H

#include <QObject>
#include <QVector>
#include <QPoint>
#include <random>

/**
 * @brief 迷宫单元格结构
 *
 * 每个单元格存储四个方向的墙壁状态
 */
struct Cell {
    bool topWall = true;      // 上墙
    bool rightWall = true;    // 右墙
    bool bottomWall = true;   // 下墙
    bool leftWall = true;     // 左墙
    bool visited = false;     // 访问标记（用于生成算法）
};

/**
 * @brief 迷宫生成器类
 *
 * 使用深度优先搜索（DFS）算法生成随机迷宫
 * 也包含广度优先搜索（BFS）计算最短路径的功能
 */
class MazeGenerator : public QObject
{
    Q_OBJECT

public:
    explicit MazeGenerator(QObject *parent = nullptr);

    /**
     * @brief 生成新的迷宫
     * @param width 迷宫宽度（列数）
     * @param height 迷宫高度（行数）
     */
    void generate(int width, int height);

    /**
     * @brief 获取迷宫宽度
     */
    int getWidth() const { return m_width; }

    /**
     * @brief 获取迷宫高度
     */
    int getHeight() const { return m_height; }

    /**
     * @brief 获取指定位置的单元格
     * @param x 列索引
     * @param y 行索引
     */
    const Cell& getCell(int x, int y) const;

    /**
     * @brief 检查两个相邻单元格之间是否有墙
     * @param x1, y1 第一个单元格坐标
     * @param x2, y2 第二个单元格坐标
     * @return true表示有墙，false表示可通行
     */
    bool hasWallBetween(int x1, int y1, int x2, int y2) const;

    /**
     * @brief 计算从起点到终点的最短路径步数
     * @param startX, startY 起点坐标
     * @param endX, endY 终点坐标
     * @return 最短路径步数，-1表示无法到达
     */
    int calculateShortestPath(int startX, int startY, int endX, int endY) const;

    /**
     * @brief 获取从起点到终点的完整最短路径
     * @param startX, startY 起点坐标
     * @param endX, endY 终点坐标
     * @return 路径上所有点的坐标列表，包括起点和终点；如果无法到达则返回空列表
     */
    QVector<QPoint> getFullPath(int startX, int startY, int endX, int endY) const;

private:
    /**
     * @brief DFS递归生成迷宫
     * @param x 当前单元格X坐标
     * @param y 当前单元格Y坐标
     */
    void generateDFS(int x, int y);

    /**
     * @brief 检查坐标是否在迷宫范围内
     */
    bool isInBounds(int x, int y) const;

    /**
     * @brief 移除两个相邻单元格之间的墙
     */
    void removeWall(int x1, int y1, int x2, int y2);

    int m_width;                           // 迷宫宽度
    int m_height;                          // 迷宫高度
    QVector<QVector<Cell>> m_maze;         // 二维迷宫数据
    std::mt19937 m_rng;                    // 随机数生成器
};

#endif // MAZEGENERATOR_H
