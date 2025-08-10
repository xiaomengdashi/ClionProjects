# AI API 集成网站

一个现代化的AI API集成平台，提供多种AI模型的统一访问接口。

## 项目结构

```
chathub/
├── frontend/          # Vue.js 前端应用
├── backend/           # Flask 后端API
├── database/          # SQLite 数据库文件
└── README.md
```

## 功能特性

- 🤖 多AI模型集成 (GPT-4, Claude, Gemini等)
- 💬 实时聊天界面
- 📊 使用统计和额度管理
- 🔐 用户认证和授权
- 💾 聊天记录持久化存储
- 📱 响应式设计

## 快速开始

### 后端启动
```bash
cd backend
pip install -r requirements.txt
python app.py
```

### 前端启动
```bash
cd frontend
npm install
npm run dev
```

## 技术栈

- **前端**: Vue 3 + Vite + Element Plus
- **后端**: Flask + SQLAlchemy
- **数据库**: SQLite
- **样式**: CSS3 + 响应式设计