/**
 * @file main.cpp
 * @brief 迷宫逃离游戏主程序入口
 *
 * 初始化Qt应用程序并加载QML界面
 */

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include "MazeGame.h"  // 包含游戏逻辑类

int main(int argc, char *argv[])
{
    // 创建应用程序实例
    QGuiApplication app(argc, argv);

    // 设置应用程序信息
    app.setOrganizationName("MazeGame");
    app.setApplicationName("迷宫逃离");
    app.setApplicationVersion("1.0.0");

    // 创建QML引擎
    QQmlApplicationEngine engine;

    // 连接对象创建失败信号，失败时退出应用
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);

    // 从QML模块加载主界面
    // MazeGame类已通过QML_ELEMENT宏自动注册到"Stdmigong"模块
    engine.loadFromModule("Stdmigong", "Main");

    // 运行应用程序事件循环
    return app.exec();
}
