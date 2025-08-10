#!/bin/bash

# ChatHub 启动脚本

echo "🚀 启动 ChatHub AI API 集成平台..."

# 检查是否安装了必要的依赖
check_dependency() {
    if ! command -v $1 &> /dev/null; then
        echo "❌ $1 未安装，请先安装 $1"
        exit 1
    fi
}

echo "📋 检查依赖..."
check_dependency "python3"
check_dependency "node"
check_dependency "npm"

# 创建数据库目录
echo "📁 创建数据库目录..."
mkdir -p database

# 启动后端
echo "🔧 启动后端服务..."
cd backend

# 检查是否存在虚拟环境
if [ ! -d "venv" ]; then
    echo "📦 创建Python虚拟环境..."
    python3 -m venv venv
fi

# 激活虚拟环境
source venv/bin/activate

# 安装依赖
echo "📦 安装后端依赖..."
pip install -r requirements.txt

# 启动Flask应用
echo "🌐 启动Flask服务器 (端口 5003)..."
python app.py &
BACKEND_PID=$!

# 等待后端启动
sleep 3

# 启动前端
echo "🎨 启动前端服务..."
cd ../frontend

# 安装依赖
if [ ! -d "node_modules" ]; then
    echo "📦 安装前端依赖..."
    npm install
fi

# 启动开发服务器
echo "🌐 启动前端开发服务器 (端口 3000)..."
npm run dev &
FRONTEND_PID=$!

echo ""
echo "✅ ChatHub 启动成功！"
echo ""
echo "🔗 访问地址:"
echo "   前端: http://localhost:3000"
echo "   后端API: http://localhost:5001"
echo ""
echo "📝 使用说明:"
echo "   - 在浏览器中打开 http://localhost:3000"
echo "   - 点击'聊天'开始与AI对话"
echo "   - 查看'价格'了解不同方案"
echo "   - 访问'控制台'查看使用统计"
echo ""
echo "⏹️  停止服务: Ctrl+C"

# 等待用户中断
wait

# 清理进程
echo "🛑 正在停止服务..."
kill $BACKEND_PID 2>/dev/null
kill $FRONTEND_PID 2>/dev/null
echo "✅ 服务已停止"