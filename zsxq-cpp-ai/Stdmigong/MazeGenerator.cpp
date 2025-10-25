#include "MazeGenerator.h"
#include <QQueue>
#include <QSet>
#include <QMap>
#include <algorithm>
#include <chrono>

/**
 * @brief 构造函数
 */
MazeGenerator::MazeGenerator(QObject *parent)
    : QObject(parent)
    , m_width(0)
    , m_height(0)
{
    // 使用当前时间作为随机数种子
    auto seed = std::chrono::system_clock::now().time_since_epoch().count();
    m_rng.seed(seed);
}

/**
 * @brief 生成新的迷宫
 */
void MazeGenerator::generate(int width, int height)
{
    m_width = width;
    m_height = height;

    // 初始化迷宫，所有单元格都有四面墙
    m_maze.clear();
    m_maze.resize(height);
    for (int i = 0; i < height; ++i) {
        m_maze[i].resize(width);
    }

    // 从(0,0)开始使用DFS生成迷宫
    generateDFS(0, 0);
}

/**
 * @brief 递归的深度优先搜索生成迷宫
 *
 * 算法步骤：
 * 1. 标记当前单元格为已访问
 * 2. 随机选择一个未访问的相邻单元格
 * 3. 移除两个单元格之间的墙
 * 4. 递归访问该相邻单元格
 * 5. 重复2-4直到所有方向都被尝试
 */
void MazeGenerator::generateDFS(int x, int y)
{
    // 标记当前单元格为已访问
    m_maze[y][x].visited = true;

    // 定义四个方向：上、右、下、左
    struct Direction {
        int dx, dy;
    };
    QVector<Direction> directions = {
        {0, -1},  // 上
        {1, 0},   // 右
        {0, 1},   // 下
        {-1, 0}   // 左
    };

    // 随机打乱方向顺序
    std::shuffle(directions.begin(), directions.end(), m_rng);

    // 尝试每个方向
    for (const auto& dir : directions) {
        int nx = x + dir.dx;
        int ny = y + dir.dy;

        // 检查新位置是否在边界内且未被访问
        if (isInBounds(nx, ny) && !m_maze[ny][nx].visited) {
            // 移除当前单元格和相邻单元格之间的墙
            removeWall(x, y, nx, ny);

            // 递归访问相邻单元格
            generateDFS(nx, ny);
        }
    }
}

/**
 * @brief 检查坐标是否在迷宫范围内
 */
bool MazeGenerator::isInBounds(int x, int y) const
{
    return x >= 0 && x < m_width && y >= 0 && y < m_height;
}

/**
 * @brief 移除两个相邻单元格之间的墙
 */
void MazeGenerator::removeWall(int x1, int y1, int x2, int y2)
{
    // 计算方向
    int dx = x2 - x1;
    int dy = y2 - y1;

    // 移除第一个单元格的墙
    if (dx == 1) {
        m_maze[y1][x1].rightWall = false;
    } else if (dx == -1) {
        m_maze[y1][x1].leftWall = false;
    } else if (dy == 1) {
        m_maze[y1][x1].bottomWall = false;
    } else if (dy == -1) {
        m_maze[y1][x1].topWall = false;
    }

    // 移除第二个单元格对应的墙
    if (dx == 1) {
        m_maze[y2][x2].leftWall = false;
    } else if (dx == -1) {
        m_maze[y2][x2].rightWall = false;
    } else if (dy == 1) {
        m_maze[y2][x2].topWall = false;
    } else if (dy == -1) {
        m_maze[y2][x2].bottomWall = false;
    }
}

/**
 * @brief 获取指定位置的单元格
 */
const Cell& MazeGenerator::getCell(int x, int y) const
{
    return m_maze[y][x];
}

/**
 * @brief 检查两个相邻单元格之间是否有墙
 */
bool MazeGenerator::hasWallBetween(int x1, int y1, int x2, int y2) const
{
    if (!isInBounds(x1, y1) || !isInBounds(x2, y2)) {
        return true;  // 边界外视为有墙
    }

    int dx = x2 - x1;
    int dy = y2 - y1;

    // 检查第一个单元格的墙
    if (dx == 1) {
        return m_maze[y1][x1].rightWall;
    } else if (dx == -1) {
        return m_maze[y1][x1].leftWall;
    } else if (dy == 1) {
        return m_maze[y1][x1].bottomWall;
    } else if (dy == -1) {
        return m_maze[y1][x1].topWall;
    }

    return true;  // 不相邻的单元格视为有墙
}

/**
 * @brief 使用BFS计算最短路径
 *
 * 广度优先搜索确保找到的第一条路径就是最短路径
 */
int MazeGenerator::calculateShortestPath(int startX, int startY, int endX, int endY) const
{
    if (!isInBounds(startX, startY) || !isInBounds(endX, endY)) {
        return -1;
    }

    // 已访问集合
    QSet<QPair<int, int>> visited;

    // BFS队列，存储{x, y, distance}
    struct Node {
        int x, y, distance;
    };
    QQueue<Node> queue;

    // 起点入队
    queue.enqueue({startX, startY, 0});
    visited.insert({startX, startY});

    // 四个方向
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {-1, 0, 1, 0};

    while (!queue.isEmpty()) {
        Node current = queue.dequeue();

        // 到达终点
        if (current.x == endX && current.y == endY) {
            return current.distance;
        }

        // 尝试四个方向
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            // 检查是否可以移动到新位置
            if (isInBounds(nx, ny) &&
                !visited.contains({nx, ny}) &&
                !hasWallBetween(current.x, current.y, nx, ny)) {

                visited.insert({nx, ny});
                queue.enqueue({nx, ny, current.distance + 1});
            }
        }
    }

    return -1;  // 无法到达
}

/**
 * @brief 获取从起点到终点的完整最短路径
 *
 * 使用BFS算法并记录父节点，最后回溯得到完整路径
 */
QVector<QPoint> MazeGenerator::getFullPath(int startX, int startY, int endX, int endY) const
{
    QVector<QPoint> path;

    if (!isInBounds(startX, startY) || !isInBounds(endX, endY)) {
        return path;  // 返回空路径
    }

    // 如果起点就是终点
    if (startX == endX && startY == endY) {
        path.append(QPoint(startX, startY));
        return path;
    }

    // 已访问集合和父节点映射
    QMap<QPair<int, int>, QPair<int, int>> parentMap;  // 记录每个节点的父节点
    QSet<QPair<int, int>> visited;

    // BFS队列
    struct Node {
        int x, y;
    };
    QQueue<Node> queue;

    // 起点入队
    queue.enqueue({startX, startY});
    visited.insert({startX, startY});
    parentMap[{startX, startY}] = {-1, -1};  // 起点没有父节点

    // 四个方向
    const int dx[] = {0, 1, 0, -1};
    const int dy[] = {-1, 0, 1, 0};

    bool found = false;

    while (!queue.isEmpty() && !found) {
        Node current = queue.dequeue();

        // 尝试四个方向
        for (int i = 0; i < 4; ++i) {
            int nx = current.x + dx[i];
            int ny = current.y + dy[i];

            // 检查是否可以移动到新位置
            if (isInBounds(nx, ny) &&
                !visited.contains({nx, ny}) &&
                !hasWallBetween(current.x, current.y, nx, ny)) {

                visited.insert({nx, ny});
                parentMap[{nx, ny}] = {current.x, current.y};
                queue.enqueue({nx, ny});

                // 到达终点
                if (nx == endX && ny == endY) {
                    found = true;
                    break;
                }
            }
        }
    }

    if (!found) {
        return path;  // 无法到达，返回空路径
    }

    // 从终点回溯到起点
    QPair<int, int> current = {endX, endY};
    while (current.first != -1 && current.second != -1) {
        path.prepend(QPoint(current.first, current.second));
        current = parentMap[current];
    }

    return path;
}
