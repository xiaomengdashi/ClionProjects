<template>
  <div class="register-container">
    <div class="register-background">
      <div class="background-pattern"></div>
    </div>
    
    <div class="register-card">
      <div class="register-header">
        <div class="logo">
          <CommentOutlined class="logo-icon" />
          <h1>ChatHub</h1>
        </div>
        <p class="subtitle">创建您的账户</p>
      </div>

      <a-form 
        :model="registerForm" 
        :rules="rules" 
        ref="registerFormRef" 
        class="register-form"
        @submit.prevent="handleRegister"
        layout="vertical"
      >
        <a-form-item name="username">
          <a-input
            v-model:value="registerForm.username"
            placeholder="用户名 (3-20个字符)"
            size="large"
            allow-clear
          >
            <template #prefix><UserOutlined /></template>
          </a-input>
        </a-form-item>
        
        <a-form-item name="email">
          <a-input
            v-model:value="registerForm.email"
            placeholder="邮箱地址"
            size="large"
            allow-clear
          >
            <template #prefix><MailOutlined /></template>
          </a-input>
        </a-form-item>
        
        <a-form-item name="password">
          <a-input-password
            v-model:value="registerForm.password"
            placeholder="密码 (至少6个字符)"
            size="large"
          >
            <template #prefix><LockOutlined /></template>
          </a-input-password>
        </a-form-item>
        
        <a-form-item name="confirmPassword">
          <a-input-password
            v-model:value="registerForm.confirmPassword"
            placeholder="确认密码"
            size="large"
            @keyup.enter="handleRegister"
          >
            <template #prefix><LockOutlined /></template>
          </a-input-password>
        </a-form-item>
        
        <a-form-item name="agreement">
          <a-checkbox v-model:checked="registerForm.agreement">
            我已阅读并同意 <a href="#" @click.prevent>服务条款</a> 和 <a href="#" @click.prevent>隐私政策</a>
          </a-checkbox>
        </a-form-item>
        
        <a-form-item>
          <a-button 
            type="primary" 
            size="large" 
            class="register-button"
            :loading="loading"
            @click="handleRegister"
            block
          >
            <span v-if="!loading">注册</span>
            <span v-else>注册中...</span>
          </a-button>
        </a-form-item>
      </a-form>

      <div class="register-footer">
        <p class="login-link">
          已有账户？ <router-link to="/login">立即登录</router-link>
        </p>
      </div>
    </div>
  </div>
</template>

<script>
import { ref, reactive } from 'vue'
import { useRouter } from 'vue-router'
import { message } from 'ant-design-vue'
import { 
  CommentOutlined, 
  UserOutlined, 
  LockOutlined, 
  MailOutlined 
} from '@ant-design/icons-vue'
import request from '../utils/request'

export default {
  name: 'Register',
  components: {
    CommentOutlined,
    UserOutlined,
    LockOutlined,
    MailOutlined
  },
  setup() {
    const router = useRouter()
    const loading = ref(false)
    const registerFormRef = ref()
    
    const registerForm = reactive({
      username: '',
      email: '',
      password: '',
      confirmPassword: '',
      agreement: false
    })
    
    // 验证确认密码
    const validateConfirmPassword = (rule, value) => {
      if (value === '') {
        return Promise.reject('请确认密码')
      } else if (value !== registerForm.password) {
        return Promise.reject('两次输入的密码不一致')
      } else {
        return Promise.resolve()
      }
    }
    
    // 验证邮箱格式
    const validateEmail = (rule, value) => {
      const emailRegex = /^[^\s@]+@[^\s@]+\.[^\s@]+$/
      if (value === '') {
        return Promise.reject('请输入邮箱地址')
      } else if (!emailRegex.test(value)) {
        return Promise.reject('请输入有效的邮箱地址')
      } else {
        return Promise.resolve()
      }
    }
    
    const rules = {
      username: [
        { required: true, message: '请输入用户名', trigger: 'blur' },
        { min: 3, max: 20, message: '用户名长度必须在3-20个字符之间', trigger: 'blur' }
      ],
      email: [
        { validator: validateEmail, trigger: 'blur' }
      ],
      password: [
        { required: true, message: '请输入密码', trigger: 'blur' },
        { min: 6, message: '密码长度至少6个字符', trigger: 'blur' }
      ],
      confirmPassword: [
        { validator: validateConfirmPassword, trigger: 'blur' }
      ],
      agreement: [
        { 
          validator: (rule, value) => {
            if (!value) {
              return Promise.reject('请同意服务条款和隐私政策')
            }
            return Promise.resolve()
          }, 
          trigger: 'change' 
        }
      ]
    }

    // 处理注册
    const handleRegister = async () => {
      if (!registerFormRef.value) return
      
      try {
        await registerFormRef.value.validate()
        loading.value = true
        
        // 发送注册请求
        const response = await request.post('/api/auth/register', {
          username: registerForm.username,
          email: registerForm.email,
          password: registerForm.password
        })
        
        if (response.data.success) {
          message.success('注册成功！请登录您的账户', 2)
          
          // 跳转到登录页面
          setTimeout(() => {
            router.push('/login')
          }, 2000)
        } else {
          message.error(response.data.error || '注册失败')
        }
      } catch (error) {
        if (error.response?.data?.error) {
          message.error(error.response.data.error)
        } else {
          message.error('注册失败，请检查网络连接')
        }
      } finally {
        loading.value = false
      }
    }

    return {
      registerForm,
      rules,
      loading,
      registerFormRef,
      handleRegister
    }
  }
}
</script>

<style scoped>
.register-container {
  min-height: 100vh;
  display: flex;
  align-items: center;
  justify-content: center;
  position: relative;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  overflow: hidden;
}

.register-background {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  z-index: 1;
}

.background-pattern {
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background-image: 
    radial-gradient(circle at 25% 25%, rgba(255, 255, 255, 0.1) 0%, transparent 50%),
    radial-gradient(circle at 75% 75%, rgba(255, 255, 255, 0.1) 0%, transparent 50%);
  animation: float 20s ease-in-out infinite;
}

@keyframes float {
  0%, 100% { transform: translateY(0px) rotate(0deg); }
  50% { transform: translateY(-20px) rotate(180deg); }
}

.register-card {
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
  border-radius: 20px;
  padding: 40px;
  width: 100%;
  max-width: 420px;
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.1);
  z-index: 2;
  position: relative;
}

.register-header {
  text-align: center;
  margin-bottom: 30px;
}

.logo {
  display: flex;
  align-items: center;
  justify-content: center;
  gap: 12px;
  margin-bottom: 10px;
}

.logo-icon {
  font-size: 32px;
  color: #667eea;
}

.logo h1 {
  margin: 0;
  font-size: 28px;
  font-weight: 700;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.subtitle {
  color: #666;
  margin: 0;
  font-size: 16px;
}

.register-form {
  margin-bottom: 20px;
}

.register-form .ant-form-item {
  margin-bottom: 20px;
}

.register-button {
  height: 48px;
  font-size: 16px;
  font-weight: 600;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  border-radius: 8px;
  transition: all 0.3s ease;
}

.register-button:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
}

.register-footer {
  text-align: center;
  padding-top: 20px;
  border-top: 1px solid #f0f0f0;
}

.login-link {
  color: #666;
  margin: 0;
}

.login-link a {
  color: #667eea;
  text-decoration: none;
  font-weight: 600;
}

.login-link a:hover {
  color: #764ba2;
}

/* 响应式设计 */
@media (max-width: 480px) {
  .register-card {
    margin: 20px;
    padding: 30px 20px;
  }
  
  .logo h1 {
    font-size: 24px;
  }
  
  .subtitle {
    font-size: 14px;
  }
}
</style>