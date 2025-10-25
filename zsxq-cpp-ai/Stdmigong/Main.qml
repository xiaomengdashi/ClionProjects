/**
 * @file Main.qml
 * @brief 迷宫逃离游戏主界面
 *
 * 包含迷宫渲染、玩家控制、游戏状态显示等功能
 */

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Window {
    id: root
    width: 1000
    height: 800
    visible: true
    title: qsTr("迷宫逃离 - Maze Escape")
    color: "#2c3e50"

    // 游戏控制器实例
    MazeGame {
        id: game

        // 游戏开始时自动生成迷宫
        Component.onCompleted: {
            startNewGame(15, 15)
        }

        // 监听游戏胜利信号
        onGameWon: function(isPerfect) {
            winDialog.isPerfect = isPerfect
            winDialog.visible = true
        }

        // 监听游戏失败信号
        onGameLost: function() {
            loseDialog.visible = true
        }
    }

    // 主布局
    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 15
        spacing: 15

        // 顶部标题栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 60
            color: "#34495e"
            radius: 10

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 8

                // 游戏标题
                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "🎮 迷宫逃离游戏"
                    font.pixelSize: 22
                    font.bold: true
                    color: "#ecf0f1"
                }

                // 游戏状态信息栏
                RowLayout {
                    Layout.fillWidth: true
                    spacing: 20

                    // 时间显示
                    Rectangle {
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 30
                        color: "#2c3e50"
                        radius: 5

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            Text {
                                text: "⏱️ 时间:"
                                color: "#bdc3c7"
                                font.pixelSize: 14
                            }
                            Text {
                                text: Math.floor(game.elapsedTime / 60) + ":" +
                                      (game.elapsedTime % 60).toString().padStart(2, '0')
                                color: "#3498db"
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }
                    }

                    // 步数显示
                    Rectangle {
                        Layout.preferredWidth: 180
                        Layout.preferredHeight: 30
                        color: "#2c3e50"
                        radius: 5

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            Text {
                                text: "👣 步数:"
                                color: "#bdc3c7"
                                font.pixelSize: 14
                            }
                            Text {
                                text: game.currentSteps + " / " + game.shortestPath
                                color: game.currentSteps <= game.shortestPath ? "#2ecc71" : "#e74c3c"
                                font.pixelSize: 16
                                font.bold: true
                            }
                        }
                    }

                    // 追逐者状态显示
                    Rectangle {
                        Layout.preferredWidth: 200
                        Layout.preferredHeight: 30
                        color: "#2c3e50"
                        radius: 5

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 8

                            Text {
                                text: game.chaserActive ? "👻 追逐者:" : "😴 追逐者:"
                                color: "#bdc3c7"
                                font.pixelSize: 14
                            }
                            Text {
                                text: {
                                    if (game.chaserActive) {
                                        return "追逐中!"
                                    } else {
                                        var remaining = game.chaserStartDelay - game.elapsedTime
                                        return remaining > 0 ? remaining + "秒后出发" : "即将出发..."
                                    }
                                }
                                color: game.chaserActive ? "#e74c3c" : "#f39c12"
                                font.pixelSize: 14
                                font.bold: true
                            }
                        }
                    }

                    // 游戏状态
                    Rectangle {
                        Layout.fillWidth: true
                        Layout.preferredHeight: 30
                        color: "#2c3e50"
                        radius: 5

                        Text {
                            anchors.centerIn: parent
                            text: game.isGameLost ? "💀 失败!" :
                                  game.isGameWon ? "🎉 胜利!" :
                                  game.isGameRunning ? "🎯 游戏进行中" : "⏸️ 暂停"
                            color: game.isGameLost ? "#e74c3c" :
                                   game.isGameWon ? "#f39c12" : "#95a5a6"
                            font.pixelSize: 14
                            font.bold: true
                        }
                    }
                }
            }
        }

        // 中间游戏区域
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "#34495e"
            radius: 10

            // 迷宫容器
            Item {
                id: mazeContainer
                anchors.centerIn: parent
                width: Math.min(parent.width - 20, parent.height - 20)
                height: width

                // 计算单元格大小
                property real cellSize: width / Math.max(game.mazeWidth, game.mazeHeight)

                // 背景网格
                Rectangle {
                    anchors.centerIn: parent
                    width: game.mazeWidth * mazeContainer.cellSize
                    height: game.mazeHeight * mazeContainer.cellSize
                    color: "#ecf0f1"
                    border.color: "#34495e"
                    border.width: 2
                    radius: 5
                }

                // 迷宫渲染器
                Repeater {
                    id: mazeRepeater
                    model: game.mazeData

                    delegate: Item {
                        id: cellItem
                        x: modelData.x * mazeContainer.cellSize +
                           (mazeContainer.width - game.mazeWidth * mazeContainer.cellSize) / 2
                        y: modelData.y * mazeContainer.cellSize +
                           (mazeContainer.height - game.mazeHeight * mazeContainer.cellSize) / 2
                        width: mazeContainer.cellSize
                        height: mazeContainer.cellSize

                        // 单元格背景
                        Rectangle {
                            anchors.fill: parent
                            color: {
                                // 起点 - 绿色
                                if (game.isStartPosition(modelData.x, modelData.y))
                                    return "#d5f4e6"
                                // 终点 - 金色
                                if (game.isEndPosition(modelData.x, modelData.y))
                                    return "#fff9e6"
                                // 普通单元格
                                return "#ecf0f1"
                            }

                            // 起点标记
                            Text {
                                anchors.centerIn: parent
                                visible: game.isStartPosition(modelData.x, modelData.y)
                                text: "🚩"
                                font.pixelSize: mazeContainer.cellSize * 0.5
                            }

                            // 终点标记
                            Text {
                                anchors.centerIn: parent
                                visible: game.isEndPosition(modelData.x, modelData.y)
                                text: "🏆"
                                font.pixelSize: mazeContainer.cellSize * 0.5
                            }
                        }

                        // 上墙
                        Rectangle {
                            visible: modelData.topWall
                            x: 0
                            y: 0
                            width: parent.width
                            height: 3
                            color: "#2c3e50"
                        }

                        // 右墙
                        Rectangle {
                            visible: modelData.rightWall
                            x: parent.width - 3
                            y: 0
                            width: 3
                            height: parent.height
                            color: "#2c3e50"
                        }

                        // 下墙
                        Rectangle {
                            visible: modelData.bottomWall
                            x: 0
                            y: parent.height - 3
                            width: parent.width
                            height: 3
                            color: "#2c3e50"
                        }

                        // 左墙
                        Rectangle {
                            visible: modelData.leftWall
                            x: 0
                            y: 0
                            width: 3
                            height: parent.height
                            color: "#2c3e50"
                        }
                    }
                }

                // 最优路径可视化 - 路径连线
                Canvas {
                    id: pathCanvas
                    anchors.centerIn: parent
                    width: game.mazeWidth * mazeContainer.cellSize
                    height: game.mazeHeight * mazeContainer.cellSize
                    opacity: game.showPath ? 0.6 : 0

                    Behavior on opacity {
                        NumberAnimation { duration: 300 }
                    }

                    onPaint: {
                        if (!game.showPath || game.optimalPath.length < 2) {
                            return
                        }

                        var ctx = getContext("2d")
                        ctx.clearRect(0, 0, width, height)

                        // 绘制路径连线
                        ctx.strokeStyle = "#3498db"
                        ctx.lineWidth = mazeContainer.cellSize * 0.15
                        ctx.lineCap = "round"
                        ctx.lineJoin = "round"

                        // 创建渐变效果（从绿色到金色）
                        var gradient = ctx.createLinearGradient(0, 0, width, height)
                        gradient.addColorStop(0, "#2ecc71")
                        gradient.addColorStop(0.5, "#f39c12")
                        gradient.addColorStop(1, "#e74c3c")
                        ctx.strokeStyle = gradient

                        ctx.beginPath()
                        for (var i = 0; i < game.optimalPath.length; i++) {
                            var point = game.optimalPath[i]
                            var px = point.x * mazeContainer.cellSize + mazeContainer.cellSize / 2
                            var py = point.y * mazeContainer.cellSize + mazeContainer.cellSize / 2

                            if (i === 0) {
                                ctx.moveTo(px, py)
                            } else {
                                ctx.lineTo(px, py)
                            }
                        }
                        ctx.stroke()
                    }

                    Connections {
                        target: game
                        function onOptimalPathChanged() {
                            pathCanvas.requestPaint()
                        }
                        function onShowPathChanged() {
                            pathCanvas.requestPaint()
                        }
                    }
                }

                // 最优路径可视化 - 方向箭头和路径点
                Repeater {
                    id: pathRepeater
                    model: game.showPath ? game.optimalPath : []

                    delegate: Item {
                        id: pathPoint
                        x: modelData.x * mazeContainer.cellSize +
                           (mazeContainer.width - game.mazeWidth * mazeContainer.cellSize) / 2
                        y: modelData.y * mazeContainer.cellSize +
                           (mazeContainer.height - game.mazeHeight * mazeContainer.cellSize) / 2
                        width: mazeContainer.cellSize
                        height: mazeContainer.cellSize

                        // 计算方向（指向下一个点）
                        property int nextDx: {
                            if (index < game.optimalPath.length - 1) {
                                return game.optimalPath[index + 1].x - modelData.x
                            }
                            return 0
                        }
                        property int nextDy: {
                            if (index < game.optimalPath.length - 1) {
                                return game.optimalPath[index + 1].y - modelData.y
                            }
                            return 0
                        }

                        // 根据路径进度计算颜色（从绿色渐变到红色）
                        property color pointColor: {
                            var progress = index / Math.max(1, game.optimalPath.length - 1)
                            if (progress < 0.5) {
                                // 绿色到金色
                                return Qt.rgba(
                                    0.18 + progress * 1.9,      // R: 0.18 -> 0.95
                                    0.8 - progress * 0.3,       // G: 0.8 -> 0.65
                                    0.44 - progress * 0.37,     // B: 0.44 -> 0.07
                                    1.0
                                )
                            } else {
                                // 金色到红色
                                var p = (progress - 0.5) * 2
                                return Qt.rgba(
                                    0.95 - p * 0.05,            // R: 0.95 -> 0.90
                                    0.65 - p * 0.35,            // G: 0.65 -> 0.30
                                    0.07 - p * 0.01,            // B: 0.07 -> 0.06
                                    1.0
                                )
                            }
                        }

                        // 路径点圆圈
                        Rectangle {
                            anchors.centerIn: parent
                            width: mazeContainer.cellSize * 0.35
                            height: width
                            color: pathPoint.pointColor
                            radius: width / 2
                            opacity: 0.85
                            border.color: Qt.lighter(pathPoint.pointColor, 1.3)
                            border.width: 2

                            // 脉动动画
                            SequentialAnimation on scale {
                                running: game.showPath
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.15; duration: 800; easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                            }
                        }

                        // 方向箭头指示器
                        Text {
                            anchors.centerIn: parent
                            visible: index < game.optimalPath.length - 1  // 最后一个点不显示箭头
                            text: {
                                if (pathPoint.nextDx === 0 && pathPoint.nextDy === -1) return "⬆"
                                if (pathPoint.nextDx === 1 && pathPoint.nextDy === 0) return "➡"
                                if (pathPoint.nextDx === 0 && pathPoint.nextDy === 1) return "⬇"
                                if (pathPoint.nextDx === -1 && pathPoint.nextDy === 0) return "⬅"
                                return "●"
                            }
                            font.pixelSize: mazeContainer.cellSize * 0.3
                            color: "white"
                            style: Text.Outline
                            styleColor: "#2c3e50"

                            // 箭头动画 - 沿着方向移动
                            SequentialAnimation on x {
                                running: game.showPath && index < game.optimalPath.length - 1
                                loops: Animation.Infinite
                                NumberAnimation {
                                    to: pathPoint.nextDx * mazeContainer.cellSize * 0.08
                                    duration: 600
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: 0
                                    duration: 600
                                    easing.type: Easing.InOutQuad
                                }
                            }
                            SequentialAnimation on y {
                                running: game.showPath && index < game.optimalPath.length - 1
                                loops: Animation.Infinite
                                NumberAnimation {
                                    to: pathPoint.nextDy * mazeContainer.cellSize * 0.08
                                    duration: 600
                                    easing.type: Easing.InOutQuad
                                }
                                NumberAnimation {
                                    to: 0
                                    duration: 600
                                    easing.type: Easing.InOutQuad
                                }
                            }
                        }

                        // 起点特殊标识
                        Text {
                            anchors.centerIn: parent
                            visible: index === 0
                            text: "🎯"
                            font.pixelSize: mazeContainer.cellSize * 0.5
                            rotation: 0

                            SequentialAnimation on rotation {
                                running: game.showPath
                                loops: Animation.Infinite
                                NumberAnimation { to: 360; duration: 3000; easing.type: Easing.Linear }
                            }
                        }

                        // 终点特殊标识
                        Rectangle {
                            anchors.centerIn: parent
                            visible: index === game.optimalPath.length - 1
                            width: mazeContainer.cellSize * 0.6
                            height: width
                            color: "transparent"
                            border.color: "#e74c3c"
                            border.width: 3
                            radius: width / 2

                            SequentialAnimation on scale {
                                running: game.showPath
                                loops: Animation.Infinite
                                NumberAnimation { to: 1.3; duration: 1000; easing.type: Easing.InOutQuad }
                                NumberAnimation { to: 1.0; duration: 1000; easing.type: Easing.InOutQuad }
                            }

                            Text {
                                anchors.centerIn: parent
                                text: "🏁"
                                font.pixelSize: mazeContainer.cellSize * 0.4
                            }
                        }
                    }
                }

                // 追逐者头像
                Rectangle {
                    id: chaser
                    x: game.chaserX * mazeContainer.cellSize +
                       (mazeContainer.width - game.mazeWidth * mazeContainer.cellSize) / 2 +
                       mazeContainer.cellSize / 2 - width / 2
                    y: game.chaserY * mazeContainer.cellSize +
                       (mazeContainer.height - game.mazeHeight * mazeContainer.cellSize) / 2 +
                       mazeContainer.cellSize / 2 - height / 2
                    width: mazeContainer.cellSize * 0.7
                    height: width
                    // 未激活时显示灰色，激活后显示红色
                    color: game.chaserActive ? "#e74c3c" : "#95a5a6"
                    radius: width / 2
                    border.color: game.chaserActive ? "#c0392b" : "#7f8c8d"
                    border.width: 3
                    // 未激活时半透明
                    opacity: game.chaserActive ? 1.0 : 0.5

                    // 平滑移动动画
                    Behavior on x {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.InOutQuad
                        }
                    }
                    Behavior on y {
                        NumberAnimation {
                            duration: 200
                            easing.type: Easing.InOutQuad
                        }
                    }

                    // 平滑透明度变化
                    Behavior on opacity {
                        NumberAnimation {
                            duration: 300
                            easing.type: Easing.InOutQuad
                        }
                    }

                    // 追逐者表情（未激活时睡眠，激活后显示幽灵）
                    Text {
                        anchors.centerIn: parent
                        text: game.chaserActive ? "👻" : "😴"
                        font.pixelSize: parent.width * 0.6
                    }

                    // 脉动效果（只在激活时运行）
                    SequentialAnimation on scale {
                        running: game.chaserActive && game.isGameRunning && !game.isGameWon && !game.isGameLost
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.15; duration: 600; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 600; easing.type: Easing.InOutQuad }
                    }
                }

                // 玩家头像
                Rectangle {
                    id: player
                    x: game.playerX * mazeContainer.cellSize +
                       (mazeContainer.width - game.mazeWidth * mazeContainer.cellSize) / 2 +
                       mazeContainer.cellSize / 2 - width / 2
                    y: game.playerY * mazeContainer.cellSize +
                       (mazeContainer.height - game.mazeHeight * mazeContainer.cellSize) / 2 +
                       mazeContainer.cellSize / 2 - height / 2
                    width: mazeContainer.cellSize * 0.7
                    height: width
                    color: "#3498db"
                    radius: width / 2
                    border.color: "#2980b9"
                    border.width: 3

                    // 平滑移动动画
                    Behavior on x {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutQuad
                        }
                    }
                    Behavior on y {
                        NumberAnimation {
                            duration: 150
                            easing.type: Easing.OutQuad
                        }
                    }

                    // 玩家表情
                    Text {
                        anchors.centerIn: parent
                        text: game.isGameLost ? "😱" : game.isGameWon ? "😄" : "🤠"
                        font.pixelSize: parent.width * 0.6
                    }

                    // 脉动效果
                    SequentialAnimation on scale {
                        running: !game.isGameWon && !game.isGameLost
                        loops: Animation.Infinite
                        NumberAnimation { to: 1.1; duration: 800; easing.type: Easing.InOutQuad }
                        NumberAnimation { to: 1.0; duration: 800; easing.type: Easing.InOutQuad }
                    }
                }
            }
        }

        // 底部控制栏
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 140
            color: "#34495e"
            radius: 10

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                // 方向控制按钮
                GridLayout {
                    Layout.alignment: Qt.AlignHCenter
                    columns: 3
                    rowSpacing: 4
                    columnSpacing: 4

                    // 空白占位
                    Item { width: 50; height: 50 }

                    // 上箭头
                    Button {
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 50
                        text: "⬆️"
                        font.pixelSize: 20
                        onClicked: game.movePlayer(0)

                        background: Rectangle {
                            color: parent.pressed ? "#2980b9" : "#3498db"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 空白占位
                    Item { width: 50; height: 50 }

                    // 左箭头
                    Button {
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 50
                        text: "⬅️"
                        font.pixelSize: 20
                        onClicked: game.movePlayer(3)

                        background: Rectangle {
                            color: parent.pressed ? "#2980b9" : "#3498db"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 下箭头
                    Button {
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 50
                        text: "⬇️"
                        font.pixelSize: 20
                        onClicked: game.movePlayer(2)

                        background: Rectangle {
                            color: parent.pressed ? "#2980b9" : "#3498db"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 右箭头
                    Button {
                        Layout.preferredWidth: 50
                        Layout.preferredHeight: 50
                        text: "➡️"
                        font.pixelSize: 20
                        onClicked: game.movePlayer(1)

                        background: Rectangle {
                            color: parent.pressed ? "#2980b9" : "#3498db"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }

                // 游戏控制按钮
                RowLayout {
                    Layout.alignment: Qt.AlignHCenter
                    spacing: 10

                    // 新游戏按钮
                    Button {
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 38
                        text: "🎲 新游戏"
                        font.pixelSize: 14
                        font.bold: true
                        onClicked: game.startNewGame(15, 15)

                        background: Rectangle {
                            color: parent.pressed ? "#27ae60" : "#2ecc71"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 重新开始按钮
                    Button {
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 38
                        text: "🔄 重新开始"
                        font.pixelSize: 14
                        font.bold: true
                        onClicked: game.resetGame()

                        background: Rectangle {
                            color: parent.pressed ? "#d35400" : "#e67e22"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }

                    // 难度选择按钮
                    Button {
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 38
                        text: "⚙️ 难度"
                        font.pixelSize: 14
                        font.bold: true
                        onClicked: difficultyMenu.open()

                        background: Rectangle {
                            color: parent.pressed ? "#8e44ad" : "#9b59b6"
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        // 难度选择菜单
                        Menu {
                            id: difficultyMenu

                            MenuItem {
                                text: "简单 (10x10) - 追逐者慢"
                                onTriggered: game.startNewGame(10, 10, 0)  // Easy
                            }
                            MenuItem {
                                text: "普通 (15x15) - 追逐者正常"
                                onTriggered: game.startNewGame(15, 15, 1)  // Normal
                            }
                            MenuItem {
                                text: "困难 (20x20) - 追逐者快"
                                onTriggered: game.startNewGame(20, 20, 2)  // Hard
                            }
                            MenuItem {
                                text: "专家 (25x25) - 追逐者很快"
                                onTriggered: game.startNewGame(25, 25, 3)  // Expert
                            }
                        }
                    }

                    // 显示路径按钮
                    Button {
                        Layout.preferredWidth: 130
                        Layout.preferredHeight: 38
                        text: game.showPath ? "🚫 隐藏路径" : "🗺️ 显示路径"
                        font.pixelSize: 14
                        font.bold: true
                        onClicked: {
                            if (game.showPath) {
                                game.hidePath()
                            } else {
                                game.calculateAndShowPath()
                            }
                        }

                        background: Rectangle {
                            color: game.showPath ?
                                   (parent.pressed ? "#c0392b" : "#e74c3c") :
                                   (parent.pressed ? "#16a085" : "#1abc9c")
                            radius: 8
                        }
                        contentItem: Text {
                            text: parent.text
                            font: parent.font
                            color: "white"
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                    }
                }
            }
        }
    }

    // 胜利对话框
    Rectangle {
        id: winDialog
        visible: false
        anchors.centerIn: parent
        width: 400
        height: 300
        color: "#34495e"
        radius: 15
        border.color: "#f39c12"
        border.width: 3

        property bool isPerfect: false

        // 半透明背景
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1000
            color: "#80000000"
            z: -1

            MouseArea {
                anchors.fill: parent
                onClicked: winDialog.visible = false
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 20

            // 胜利标题
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "🎉 恭喜通关! 🎉"
                font.pixelSize: 32
                font.bold: true
                color: "#f39c12"
            }

            // 完美通关提示
            Text {
                Layout.alignment: Qt.AlignHCenter
                visible: winDialog.isPerfect
                text: "⭐ 完美通关! ⭐"
                font.pixelSize: 24
                color: "#2ecc71"
            }

            // 游戏统计
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "用时: " + Math.floor(game.elapsedTime / 60) + " 分 " +
                          (game.elapsedTime % 60) + " 秒"
                    font.pixelSize: 18
                    color: "#ecf0f1"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "步数: " + game.currentSteps + " (最短: " + game.shortestPath + ")"
                    font.pixelSize: 18
                    color: winDialog.isPerfect ? "#2ecc71" : "#ecf0f1"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    visible: !winDialog.isPerfect
                    text: "还差 " + (game.currentSteps - game.shortestPath) + " 步达到最短路径"
                    font.pixelSize: 14
                    color: "#95a5a6"
                }
            }

            // 关闭按钮
            Button {
                Layout.alignment: Qt.AlignHCenter
                Layout.preferredWidth: 200
                Layout.preferredHeight: 50
                text: "继续游戏"
                font.pixelSize: 18
                font.bold: true
                onClicked: winDialog.visible = false

                background: Rectangle {
                    color: parent.pressed ? "#27ae60" : "#2ecc71"
                    radius: 10
                }
                contentItem: Text {
                    text: parent.text
                    font: parent.font
                    color: "white"
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }
    }

    // 失败对话框
    Rectangle {
        id: loseDialog
        visible: false
        anchors.centerIn: parent
        width: 400
        height: 300
        color: "#34495e"
        radius: 15
        border.color: "#e74c3c"
        border.width: 3

        // 半透明背景
        Rectangle {
            anchors.fill: parent
            anchors.margins: -1000
            color: "#80000000"
            z: -1

            MouseArea {
                anchors.fill: parent
                onClicked: loseDialog.visible = false
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.margins: 30
            spacing: 20

            // 失败标题
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "💀 游戏失败! 💀"
                font.pixelSize: 32
                font.bold: true
                color: "#e74c3c"
            }

            // 失败提示
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: "你被追逐者抓住了！"
                font.pixelSize: 20
                color: "#ecf0f1"
            }

            // 游戏统计
            ColumnLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "坚持时间: " + Math.floor(game.elapsedTime / 60) + " 分 " +
                          (game.elapsedTime % 60) + " 秒"
                    font.pixelSize: 18
                    color: "#ecf0f1"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "移动步数: " + game.currentSteps
                    font.pixelSize: 18
                    color: "#ecf0f1"
                }

                Text {
                    Layout.alignment: Qt.AlignHCenter
                    text: "继续努力，下次一定能成功！"
                    font.pixelSize: 14
                    color: "#95a5a6"
                }
            }

            // 控制按钮
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 15

                // 重新开始按钮
                Button {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 45
                    text: "重新开始"
                    font.pixelSize: 16
                    font.bold: true
                    onClicked: {
                        loseDialog.visible = false
                        game.resetGame()
                    }

                    background: Rectangle {
                        color: parent.pressed ? "#d35400" : "#e67e22"
                        radius: 10
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }

                // 新游戏按钮
                Button {
                    Layout.preferredWidth: 150
                    Layout.preferredHeight: 45
                    text: "新游戏"
                    font.pixelSize: 16
                    font.bold: true
                    onClicked: {
                        loseDialog.visible = false
                        game.startNewGame(15, 15, 1)
                    }

                    background: Rectangle {
                        color: parent.pressed ? "#27ae60" : "#2ecc71"
                        radius: 10
                    }
                    contentItem: Text {
                        text: parent.text
                        font: parent.font
                        color: "white"
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
        }
    }

    // 键盘控制焦点项
    // Window本身不能接收focus，需要一个Item来处理键盘事件
    Item {
        anchors.fill: parent
        focus: true
        Keys.onPressed: function(event) {
            if (event.key === Qt.Key_Up || event.key === Qt.Key_W) {
                game.movePlayer(0)
                event.accepted = true
            } else if (event.key === Qt.Key_Right || event.key === Qt.Key_D) {
                game.movePlayer(1)
                event.accepted = true
            } else if (event.key === Qt.Key_Down || event.key === Qt.Key_S) {
                game.movePlayer(2)
                event.accepted = true
            } else if (event.key === Qt.Key_Left || event.key === Qt.Key_A) {
                game.movePlayer(3)
                event.accepted = true
            }
        }
        z: -100  // 放在最底层，不干扰其他元素
    }
}
