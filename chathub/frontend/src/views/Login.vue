<template>
  <div class="login-container">
    <div class="login-background">
      <div class="background-pattern"></div>
    </div>
    
    <div class="login-card">
      <div class="login-header">
        <div class="logo">
          <CommentOutlined class="logo-icon" />
          <h1>ChatHub</h1>
        </div>
        <p class="subtitle">AI API 集成平台</p>
      </div>

      <a-form 
        :model="loginForm" 
        :rules="rules" 
        ref="loginFormRef" 
        class="login-form"
        @submit.prevent="handleLogin"
        layout="vertical"
      >
        <a-form-item name="username">
          <a-input
            v-model:value="loginForm.username"
            placeholder="用户名"
            size="large"
            allow-clear
          >
            <template #prefix><UserOutlined /></template>
          </a-input>
        </a-form-item>
        
        <a-form-item name="password">
          <a-input-password
            v-model:value="loginForm.password"
            placeholder="密码"
            size="large"
            @keyup.enter="handleLogin"
          >
            <template #prefix><LockOutlined /></template>
          </a-input-password>
        </a-form-item>
        
        <a-form-item>
          <a-checkbox v-model:checked="loginForm.remember">记住我</a-checkbox>
        </a-form-item>
        
        <a-form-item>
          <a-button 
            type="primary" 
            size="large" 
            class="login-button"
            :loading="loading"
            @click="handleLogin"
            block
          >
            <span v-if="!loading">登录</span>
            <span v-else>登录中...</span>
          </a-button>
        </a-form-item>
      </a-form>

      <div class="login-footer">
        <p class="demo-info">
          <InfoCircleOutlined />
          演示账号：admin / kolane
        </p>
        <p class="register-link">
          还没有账户？ <router-link to="/register">立即注册</router-link>
        </p>
      </div>
    </div>
  </div>
</template>

<script>
import { ref, reactive, onMounted } from 'vue'
import { useRouter } from 'vue-router'
import { message } from 'ant-design-vue'
import { 
  CommentOutlined, 
  UserOutlined, 
  LockOutlined, 
  InfoCircleOutlined 
} from '@ant-design/icons-vue'
import request from '../utils/request'

export default {
  name: 'Login',
  components: {
    CommentOutlined,
    UserOutlined,
    LockOutlined,
    InfoCircleOutlined
  },
  setup() {
    const router = useRouter()
    const loading = ref(false)
    const loginFormRef = ref()
    
    const loginForm = reactive({
      username: 'admin',
      password: 'kolane',
      remember: true
    })
    
    const rules = {
      username: [
        { required: true, message: '请输入用户名', trigger: 'blur' }
      ],
      password: [
        { required: true, message: '请输入密码', trigger: 'blur' },
        { min: 3, message: '密码长度至少3位', trigger: 'blur' }
      ]
    }

    // 检查是否已登录
    const checkLoginStatus = () => {
      const token = localStorage.getItem('token')
      const username = localStorage.getItem('username')
      if (token && username) {
        router.push('/')
      }
    }

    // 处理登录
    const handleLogin = async () => {
      if (!loginFormRef.value) return
      
      try {
        await loginFormRef.value.validate()
        loading.value = true
        
        // 发送登录请求
        const response = await request.post('/api/auth/login', {
          username: loginForm.username,
          password: loginForm.password
        })
        
        if (response.data.success) {
          // 保存登录信息
          localStorage.setItem('token', response.data.token)
          localStorage.setItem('username', response.data.user.username)
          localStorage.setItem('user_info', JSON.stringify(response.data.user))
          
          if (loginForm.remember) {
            localStorage.setItem('remember_login', 'true')
          }
          
          message.success('登录成功！', 1)
          
          // 跳转到首页
          setTimeout(() => {
            router.push('/')
          }, 1000)
        } else {
          message.error(response.data.message || '登录失败')
        }
      } catch (error) {
        if (error.response?.data?.message) {
          message.error(error.response.data.message)
        } else {
          message.error('登录失败，请检查网络连接')
        }
      } finally {
        loading.value = false
      }
    }

    onMounted(() => {
      checkLoginStatus()
      
      // 如果记住登录，自动填充用户名
      if (localStorage.getItem('remember_login')) {
        loginForm.username = 'admin'
      }
    })

    return {
      loginForm,
      rules,
      loading,
      loginFormRef,
      handleLogin
    }
  }
}
</script>

<style scoped>
.login-container {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  overflow: hidden;
}

.login-background {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  overflow: hidden;
}

.background-pattern {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-image: 
    radial-gradient(circle at 25% 25%, rgba(255,255,255,0.08) 0%, transparent 50%),
    radial-gradient(circle at 75% 75%, rgba(255,255,255,0.08) 0%, transparent 50%),
    radial-gradient(circle at 50% 50%, rgba(78, 205, 196, 0.1) 0%, transparent 70%);
  background-size: 150px 150px, 200px 200px, 300px 300px;
  animation: float 25s ease-in-out infinite;
}

@keyframes float {
  0%, 100% { 
    transform: translateY(0px) rotate(0deg) scale(1); 
    opacity: 0.8;
  }
  33% { 
    transform: translateY(-30px) rotate(120deg) scale(1.1); 
    opacity: 0.6;
  }
  66% { 
    transform: translateY(20px) rotate(240deg) scale(0.9); 
    opacity: 1;
  }
}

.login-card {
  background: rgba(255, 255, 255, 0.98);
  backdrop-filter: blur(20px);
  border-radius: 24px;
  padding: 50px;
  width: 100%;
  max-width: 450px;
  box-shadow: 
    0 25px 50px rgba(0, 0, 0, 0.15),
    0 0 0 1px rgba(255, 255, 255, 0.2);
  position: relative;
  z-index: 1;
  transition: all 0.3s ease;
}

.login-card:hover {
  transform: translateY(-5px);
  box-shadow: 
    0 35px 70px rgba(0, 0, 0, 0.2),
    0 0 0 1px rgba(255, 255, 255, 0.3);
}

.login-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 6px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  border-radius: 24px 24px 0 0;
}

.login-header {
  text-align: center;
  margin-bottom: 40px;
}

.logo {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 12px;
  margin-bottom: 16px;
}

.logo-icon {
  font-size: 40px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  filter: drop-shadow(0 2px 4px rgba(102, 126, 234, 0.3));
}

.logo h1 {
  margin: 0;
  background: linear-gradient(135deg, #2d3748 0%, #4a5568 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  font-size: 32px;
  font-weight: 800;
  letter-spacing: -0.5px;
}

.subtitle {
  color: #718096;
  margin: 0;
  font-size: 16px;
  font-weight: 500;
  opacity: 0.9;
}

.login-form {
  margin-bottom: 24px;
}

:deep(.ant-form-item) {
  margin-bottom: 24px;
}

:deep(.ant-input) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  padding: 12px 16px;
  font-size: 16px;
  transition: all 0.3s ease;
  background: rgba(255, 255, 255, 0.8);
  backdrop-filter: blur(10px);
}

:deep(.ant-input:hover) {
  border-color: rgba(102, 126, 234, 0.3);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.1);
}

:deep(.ant-input:focus) {
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

:deep(.ant-input-password) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  transition: all 0.3s ease;
  background: rgba(255, 255, 255, 0.8);
  backdrop-filter: blur(10px);
}

:deep(.ant-input-password:hover) {
  border-color: rgba(102, 126, 234, 0.3);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.1);
}

:deep(.ant-input-password-focused) {
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

:deep(.ant-checkbox-wrapper) {
  color: #4a5568;
  font-weight: 500;
  transition: all 0.3s ease;
}

:deep(.ant-checkbox-wrapper:hover) {
  color: #2d3748;
}

:deep(.ant-checkbox-checked .ant-checkbox-inner) {
  background-color: #667eea;
  border-color: #667eea;
}

:deep(.login-button) {
  width: 100%;
  height: 50px;
  font-size: 16px;
  font-weight: 700;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  border-radius: 12px;
  transition: all 0.3s ease;
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
  position: relative;
  overflow: hidden;
}

:deep(.login-button::before) {
  content: '';
  position: absolute;
  top: 0;
  left: -100%;
  width: 100%;
  height: 100%;
  background: linear-gradient(90deg, transparent, rgba(255, 255, 255, 0.2), transparent);
  transition: left 0.5s ease;
}

:deep(.login-button:hover) {
  transform: translateY(-2px);
  box-shadow: 0 12px 35px rgba(102, 126, 234, 0.4);
}

:deep(.login-button:hover::before) {
  left: 100%;
}

:deep(.login-button:active) {
  transform: translateY(0);
}

.login-footer {
  text-align: center;
  padding-top: 24px;
  border-top: 1px solid rgba(102, 126, 234, 0.1);
}

.demo-info {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 10px;
  color: #4a5568;
  font-size: 14px;
  margin: 0;
  padding: 16px;
  background: linear-gradient(135deg, rgba(102, 126, 234, 0.05) 0%, rgba(78, 205, 196, 0.05) 100%);
  border-radius: 12px;
  border: 1px solid rgba(102, 126, 234, 0.1);
  font-weight: 500;
  transition: all 0.3s ease;
}

.demo-info:hover {
  background: linear-gradient(135deg, rgba(102, 126, 234, 0.08) 0%, rgba(78, 205, 196, 0.08) 100%);
  transform: translateY(-1px);
}

:deep(.demo-info .anticon) {
  color: #667eea;
  font-size: 16px;
}

.register-link {
  color: #666;
  margin: 16px 0 0 0;
  font-size: 14px;
}

.register-link a {
  color: #667eea;
  text-decoration: none;
  font-weight: 600;
  transition: all 0.3s ease;
}

.register-link a:hover {
  color: #764ba2;
  text-decoration: underline;
}

/* 表单验证样式 */
:deep(.ant-form-item-has-error .ant-input) {
  border-color: #ff4d4f;
}

:deep(.ant-form-item-has-error .ant-input:hover) {
  border-color: #ff4d4f;
}

:deep(.ant-form-item-has-error .ant-input:focus) {
  border-color: #ff4d4f;
  box-shadow: 0 0 0 3px rgba(255, 77, 79, 0.1);
}

:deep(.ant-form-item-explain-error) {
  color: #ff4d4f;
  font-weight: 500;
  margin-top: 8px;
}

/* 加载状态 */
:deep(.ant-btn-loading) {
  pointer-events: none;
}

:deep(.ant-spin-dot) {
  color: white;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .login-card {
    margin: 20px;
    padding: 40px 30px;
    max-width: 400px;
  }
  
  .logo h1 {
    font-size: 28px;
  }
  
  .logo-icon {
    font-size: 36px;
  }
  
  .subtitle {
    font-size: 14px;
  }
}

@media (max-width: 480px) {
  .login-card {
    margin: 16px;
    padding: 30px 24px;
    border-radius: 20px;
  }
  
  .logo h1 {
    font-size: 24px;
  }
  
  .logo-icon {
    font-size: 32px;
  }
  
  .subtitle {
    font-size: 13px;
  }
  
  :deep(.login-button) {
    height: 46px;
    font-size: 15px;
  }
  
  :deep(.ant-input) {
    padding: 10px 14px;
    font-size: 15px;
  }
  
  .demo-info {
    padding: 12px;
    font-size: 13px;
  }
}

/* 动画效果 */
.login-card {
  animation: slideUp 0.6s ease-out;
}

/* 焦点可见性 */
:deep(.ant-input:focus-visible),
:deep(.ant-btn:focus-visible),
:deep(.ant-checkbox:focus-visible) {
  outline: 2px solid #667eea;
  outline-offset: 2px;
}
</style>