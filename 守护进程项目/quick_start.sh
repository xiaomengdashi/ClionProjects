#!/bin/bash

# HTTP Server快速启动脚本 (单文件版)
# 用于手动启动和测试

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
SERVER_PATH="/usr/local/bin/httpserver"
PID_FILE="/tmp/httpserver.pid"

case "$1" in
    start)
        echo "启动HTTP服务器..."
        if [ -f "$PID_FILE" ]; then
            PID=$(cat $PID_FILE)
            if kill -0 $PID 2>/dev/null; then
                echo "服务器已在运行 (PID: $PID)"
                exit 1
            fi
        fi
        
        if [ ! -f "$SERVER_PATH" ]; then
            if [ -f "A.exe" ]; then
                echo "安装可执行文件..."
                cp A.exe $SERVER_PATH
                chmod +x $SERVER_PATH
            else
                echo "错误: 未找到可执行文件 A.exe"
                exit 1
            fi
        fi
        
        $SERVER_PATH &
        echo $! > $PID_FILE
        echo "服务器启动完成 (PID: $!)"
        ;;
        
    stop)
        echo "停止HTTP服务器..."
        if [ -f "$PID_FILE" ]; then
            PID=$(cat $PID_FILE)
            if kill -0 $PID 2>/dev/null; then
                kill $PID
                rm -f $PID_FILE
                echo "服务器已停止"
            else
                echo "PID文件存在但进程未运行"
                rm -f $PID_FILE
            fi
        else
            echo "服务器未运行"
        fi
        ;;
        
    restart)
        $0 stop
        sleep 2
        $0 start
        ;;
        
    status)
        if [ -f "$PID_FILE" ]; then
            PID=$(cat $PID_FILE)
            if kill -0 $PID 2>/dev/null; then
                echo "服务器运行中 (PID: $PID)"
                ps -p $PID -o pid,ppid,cmd
            else
                echo "PID文件存在但进程未运行"
                rm -f $PID_FILE
            fi
        else
            echo "服务器未运行"
        fi
        ;;
        
    logs)
        if [ -f "/var/log/httpserver/access.log" ]; then
            tail -f /var/log/httpserver/access.log
        else
            echo "日志文件不存在"
        fi
        ;;
        
    install)
        echo "安装可执行文件..."
        if [ -f "A.exe" ]; then
            cp A.exe /usr/local/bin/httpserver
            chmod +x /usr/local/bin/httpserver
            echo "安装完成"
        else
            echo "错误: 未找到 A.exe"
            exit 1
        fi
        ;;
        
    *)
        echo "使用方法: $0 {start|stop|restart|status|logs|install}"
        echo ""
        echo "命令说明:"
        echo "  start   - 启动HTTP服务器"
        echo "  stop    - 停止HTTP服务器"
        echo "  restart - 重启HTTP服务器"
        echo "  status  - 查看服务器状态"
        echo "  logs    - 查看访问日志"
        echo "  install - 安装可执行文件"
        exit 1
        ;;
esac