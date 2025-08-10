<template>
  <div id="app">
    <a-layout class="app-container">
      <!-- 顶部导航栏 -->
      <a-layout-header class="app-header">
        <div class="header-content">
          <div class="logo">
            <CommentOutlined class="logo-icon" />
            <span class="logo-text">ChatHub</span>
          </div>
          
          <a-menu
            v-model:selectedKeys="selectedKeys"
            class="nav-menu"
            mode="horizontal"
            @click="handleMenuClick"
          >
            <a-menu-item key="/">首页</a-menu-item>
            <a-menu-item v-if="isLoggedIn" key="/chat">聊天</a-menu-item>
            <a-menu-item key="/pricing">价格</a-menu-item>
            <a-menu-item v-if="isLoggedIn" key="/dashboard">控制台</a-menu-item>
            <a-menu-item v-if="isLoggedIn && userRole === 'admin'" key="/api-keys">API密钥</a-menu-item>
            <a-menu-item v-if="isLoggedIn && userRole === 'admin'" key="/models">大模型管理</a-menu-item>
            <a-menu-item v-if="isLoggedIn && userRole === 'admin'" key="/users">用户管理</a-menu-item>
          </a-menu>
          
          <div class="header-actions">
            <template v-if="isLoggedIn">
              <span class="username">{{ username }}</span>
              <a-button @click="logout" type="primary" ghost size="small">退出</a-button>
            </template>
            <template v-else>
              <a-button @click="$router.push('/register')" type="primary" ghost size="small">注册</a-button>
              <a-button @click="$router.push('/login')" type="primary" size="small">登录</a-button>
            </template>
          </div>
        </div>
      </a-layout-header>
      
      <!-- 主要内容区域 -->
      <a-layout-content class="app-main">
        <router-view />
      </a-layout-content>
      

    </a-layout>
  </div>
</template>

<script>
import { CommentOutlined } from '@ant-design/icons-vue'

export default {
  name: 'App',
  components: {
    CommentOutlined
  },
  data() {
    return {
      isLoggedIn: false,
      username: '',
      userRole: '',
      selectedKeys: [this.$route.path]
    }
  },
  watch: {
    '$route'(to) {
      this.selectedKeys = [to.path]
    }
  },
  mounted() {
     this.checkLoginStatus()
     this.selectedKeys = [this.$route.path]
     // 监听路由变化，更新登录状态
     this.$router.afterEach(() => {
       this.checkLoginStatus()
     })
   },
   methods: {
     handleMenuClick({ key }) {
       this.$router.push(key)
     },
     checkLoginStatus() {
      const token = localStorage.getItem('token')
      const username = localStorage.getItem('username')
      
      if (token && username) {
        // 验证token是否过期
        try {
          const payload = JSON.parse(atob(token.split('.')[1]))
          if (payload.exp > Date.now() / 1000) {
            this.isLoggedIn = true
            this.username = username
            this.userRole = payload.role || 'user'
          } else {
            // token过期，清除登录信息
            this.clearLoginInfo()
          }
        } catch (error) {
          // token格式错误，清除登录信息
          this.clearLoginInfo()
        }
      } else {
        this.isLoggedIn = false
        this.username = ''
        this.userRole = ''
      }
    },
    clearLoginInfo() {
      localStorage.removeItem('token')
      localStorage.removeItem('username')
      localStorage.removeItem('user_info')
      this.isLoggedIn = false
      this.username = ''
      this.userRole = ''
    },
    logout() {
      this.clearLoginInfo()
      this.$router.push('/')
    }
  }
}
</script>

<style scoped>
.app-container {
  min-height: 100vh;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.app-header {
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
  padding: 0;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.06);
  position: sticky;
  top: 0;
  z-index: 1000;
}

.header-content {
  max-width: 1200px;
  margin: 0 auto;
  display: flex;
  align-items: center;
  justify-content: space-between;
  height: 100%;
  padding: 0 24px;
}

.logo {
  display: flex;
  align-items: center;
  font-size: 24px;
  font-weight: 700;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  transition: all 0.3s ease;
}

.logo:hover {
  transform: scale(1.05);
}

.logo-icon {
  margin-right: 12px;
  font-size: 28px;
  color: #667eea;
  transition: all 0.3s ease;
}

.logo:hover .logo-icon {
  transform: rotate(360deg);
}

.nav-menu {
  flex: 1;
  margin: 0 40px;
  border-bottom: none;
  background: transparent;
}

:deep(.ant-menu-horizontal) {
  border-bottom: none;
  background: transparent;
}

:deep(.ant-menu-item) {
  border-radius: 8px;
  margin: 0 4px;
  transition: all 0.3s ease;
  font-weight: 500;
}

:deep(.ant-menu-item:hover) {
  background: rgba(102, 126, 234, 0.1);
  color: #667eea;
}

:deep(.ant-menu-item-selected) {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white !important;
}

:deep(.ant-menu-item-selected::after) {
  display: none;
}

.header-actions {
  display: flex;
  align-items: center;
  gap: 16px;
}

.username {
  color: #4a5568;
  font-size: 14px;
  font-weight: 500;
  padding: 8px 12px;
  border-radius: 20px;
}

.app-main {
  min-height: calc(100vh - 64px);
  padding: 0;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
}

/* 全局按钮样式增强 */
:deep(.ant-btn-primary) {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  border-radius: 8px;
  font-weight: 500;
  transition: all 0.3s ease;
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.3);
}

:deep(.ant-btn-primary:hover) {
  transform: translateY(-2px);
  box-shadow: 0 6px 20px rgba(102, 126, 234, 0.4);
}

:deep(.ant-btn-primary.ant-btn-background-ghost) {
  background: transparent;
  border: 2px solid #667eea;
  color: #667eea;
  box-shadow: none;
}

:deep(.ant-btn-primary.ant-btn-background-ghost:hover) {
  background: #667eea;
  color: white;
  transform: translateY(-2px);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.3);
}

/* 响应式设计 */
@media (max-width: 768px) {
  .header-content {
    padding: 0 16px;
  }
  
  .nav-menu {
    margin: 0 20px;
  }
  
  .logo {
    font-size: 20px;
  }
  
  .logo-icon {
    font-size: 24px;
    margin-right: 8px;
  }
  

}

@media (max-width: 480px) {
  .header-content {
    flex-wrap: wrap;
    height: auto;
    padding: 12px 16px;
  }
  
  .nav-menu {
    order: 3;
    width: 100%;
    margin: 12px 0 0 0;
  }
  
  :deep(.ant-menu-horizontal) {
    justify-content: center;
  }
}
</style>