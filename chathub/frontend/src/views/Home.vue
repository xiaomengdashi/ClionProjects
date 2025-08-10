<template>
  <div class="home">
    <!-- 英雄区域 -->
    <section class="hero">
      <div class="hero-content">
        <h1 class="hero-title">
          统一访问所有
          <span class="gradient-text">AI模型</span>
        </h1>
        <p class="hero-subtitle">
          一个平台，多种选择。GPT-4、Claude、Gemini等顶级AI模型，为您提供最佳的AI体验。
        </p>
        <div class="hero-actions">
          <a-button type="primary" size="large" @click="$router.push('/chat')">
            <template #icon><CommentOutlined /></template>
            开始聊天
          </a-button>
          <a-button v-if="isLoggedIn" size="large" @click="$router.push('/pricing')">
            查看价格
          </a-button>
        </div>
      </div>
      
      <div class="hero-image" v-if="isLoggedIn">
        <div class="floating-cards">
          <div class="ai-card" v-for="(model, index) in featuredModels" :key="index">
            <div class="card-icon">🤖</div>
            <div class="card-name">{{ model.name }}</div>
            <div class="card-provider">{{ model.provider }}</div>
          </div>
        </div>
      </div>
    </section>
    
    <!-- 特性区域 - 仅登录用户可见 -->
    <section class="features" v-if="isLoggedIn">
      <div class="container">
        <h2 class="section-title">为什么选择 ChatHub？</h2>
        <div class="features-grid">
          <div class="feature-card" v-for="feature in features" :key="feature.title">
            <div class="feature-icon">
              <component :is="feature.icon" />
            </div>
            <h3>{{ feature.title }}</h3>
            <p>{{ feature.description }}</p>
          </div>
        </div>
      </div>
    </section>
    
    <!-- 模型展示区域 - 仅登录用户可见 -->
    <section class="models" v-if="isLoggedIn">
      <div class="container">
        <h2 class="section-title">支持的AI模型</h2>
        <p class="section-subtitle">集成全球顶级AI模型，为您提供最佳的智能对话体验</p>
        
        <!-- 加载状态 -->
        <div v-if="modelsLoading" class="loading-container">
          <a-spin size="large" tip="正在加载模型信息...">
            <div class="loading-placeholder"></div>
          </a-spin>
        </div>
        
        <!-- 模型网格 -->
        <div v-else class="models-grid">
          <div class="model-card" v-for="model in allModels" :key="model.id">
            <!-- 模型头部 -->
            <div class="model-header">
              <div class="model-title-section">
                <div class="model-icon">
                  <component :is="getProviderIcon(model.model_provider)" />
                </div>
                <div class="model-title-info">
                  <h3>{{ model.display_name }}</h3>
                  <span class="model-provider">{{ getProviderName(model.model_provider) }}</span>
                </div>
              </div>
              <div class="model-status" :class="{ active: model.is_active }">
                <span v-if="model.is_active">可用</span>
                <span v-else>维护中</span>
              </div>
            </div>
            

          </div>
        </div>
        
        <!-- 空状态 -->
        <div v-if="!modelsLoading && allModels.length === 0" class="empty-state">
          <div class="empty-icon">🤖</div>
          <h3>暂无可用模型</h3>
          <p>请联系管理员配置AI模型</p>
        </div>
      </div>
    </section>
  </div>
</template>

<script>
import request from '@/utils/request'
import { 
  CommentOutlined,
  AppstoreOutlined,
  ApiOutlined,
  BarChartOutlined,
  LockOutlined,
  DollarOutlined,
  ThunderboltOutlined,
  EyeOutlined,
  RobotOutlined,
  CloudOutlined,
  ExperimentOutlined,
  BulbOutlined,
  StarOutlined
} from '@ant-design/icons-vue'

export default {
  name: 'Home',
  components: {
    CommentOutlined,
    AppstoreOutlined,
    ApiOutlined,
    BarChartOutlined,
    LockOutlined,
    DollarOutlined,
    ThunderboltOutlined,
    EyeOutlined,
    RobotOutlined,
    CloudOutlined,
    ExperimentOutlined,
    BulbOutlined,
    StarOutlined
  },
  data() {
    return {
      featuredModels: [
        { name: 'GPT-4', provider: 'OpenAI' },
        { name: 'Claude 3', provider: 'Anthropic' },
        { name: 'Gemini Pro', provider: 'Google' }
      ],
      features: [
        {
          title: '多模型支持',
          description: '集成GPT-4、Claude、Gemini等主流AI模型',
          icon: 'AppstoreOutlined'
        },
        {
          title: '统一接口',
          description: '一个API密钥，访问所有模型',
          icon: 'ApiOutlined'
        },
        {
          title: '实时聊天',
          description: '流畅的对话体验，支持上下文记忆',
          icon: 'CommentOutlined'
        },
        {
          title: '使用统计',
          description: '详细的使用分析和成本控制',
          icon: 'BarChartOutlined'
        },
        {
          title: '安全可靠',
          description: '企业级安全保障，数据隐私保护',
          icon: 'LockOutlined'
        },
        {
          title: '灵活定价',
          description: '按需付费，多种套餐选择',
          icon: 'DollarOutlined'
        }
      ],
      allModels: [],
      modelsLoading: false
    }
  },
  computed: {
    isLoggedIn() {
      const token = localStorage.getItem('token')
      if (!token) return false
      
      try {
        const payload = JSON.parse(atob(token.split('.')[1]))
        return payload.exp > Date.now() / 1000
      } catch (error) {
        return false
      }
    }
  },
  async mounted() {
    if (this.isLoggedIn) {
      await this.loadModels()
    }
  },
  methods: {
    async loadModels() {
      try {
        this.modelsLoading = true
        const response = await request.get('/api/models')
        this.allModels = response.data
      } catch (error) {
        console.error('加载模型失败:', error)
        this.$message.error('加载模型失败，请稍后重试')
      } finally {
        this.modelsLoading = false
      }
    },
    
    getProviderIcon(provider) {
      const iconMap = {
        'openai': 'BulbOutlined',
        'anthropic': 'RobotOutlined',
        'google': 'CloudOutlined',
        'qwen': 'StarOutlined',
        'baidu': 'ExperimentOutlined',
        'alibaba': 'CloudOutlined',
        'zhipu': 'StarOutlined',
        'meta': 'RobotOutlined',
        'siliconflow': 'CloudOutlined',
        'deepseek': 'BulbOutlined',
        'moonshot': 'StarOutlined'
      }
      return iconMap[provider] || 'RobotOutlined'
    },
    
    getProviderName(provider) {
      const nameMap = {
        'openai': 'OpenAI',
        'anthropic': 'Anthropic',
        'google': 'Google',
        'qwen': '通义千问',
        'baidu': '百度',
        'alibaba': '阿里巴巴',
        'zhipu': '智谱AI',
        'meta': 'Meta',
        'siliconflow': '硅基流动',
        'deepseek': 'DeepSeek',
        'moonshot': '月之暗面'
      }
      return nameMap[provider] || provider
    },
    
    formatTokens(tokens) {
      if (tokens >= 1000000) {
        return `${(tokens / 1000000).toFixed(1)}M`
      } else if (tokens >= 1000) {
        return `${(tokens / 1000).toFixed(0)}K`
      }
      return tokens.toString()
    },
    
    startChatWithModel(model) {
      // 跳转到聊天页面并选择指定模型
      this.$router.push({
        path: '/chat',
        query: { model: model.model_name }
      })
    },
    
    viewModelDetails(model) {
      // 显示模型详情弹窗
      this.$modal.info({
        title: `${model.display_name} 详细信息`,
        width: 600,
        content: h => h('div', [
          h('p', `模型名称: ${model.model_name}`),
          h('p', `提供商: ${this.getProviderName(model.model_provider)}`),
          h('p', `最大上下文: ${this.formatTokens(model.max_tokens)} tokens`),
          h('p', `输入价格: ¥${(model.input_price_per_1k * 1000).toFixed(3)}/1K tokens`),
          h('p', `输出价格: ¥${(model.output_price_per_1k * 1000).toFixed(3)}/1K tokens`),
          h('p', `描述: ${model.description}`)
        ])
      })
    }
  }
}
</script>

<style scoped>
.home {
  min-height: calc(100vh - 140px);
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  position: relative;
}

.home::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    radial-gradient(circle at 20% 80%, rgba(120, 119, 198, 0.3) 0%, transparent 50%),
    radial-gradient(circle at 80% 20%, rgba(255, 119, 198, 0.3) 0%, transparent 50%),
    radial-gradient(circle at 40% 40%, rgba(120, 219, 255, 0.2) 0%, transparent 50%);
  pointer-events: none;
}

.hero {
  display: flex;
  align-items: center;
  min-height: 85vh;
  padding: 60px 24px;
  max-width: 1200px;
  margin: 0 auto;
  position: relative;
  z-index: 1;
}

.hero-content {
  flex: 1;
  padding-right: 60px;
  animation: slideInLeft 1s ease-out;
}

.hero-title {
  font-size: 4rem;
  font-weight: 800;
  color: white;
  margin-bottom: 24px;
  line-height: 1.1;
  text-shadow: 0 4px 20px rgba(0, 0, 0, 0.3);
}

.gradient-text {
  background: linear-gradient(135deg, #ff6b6b 0%, #4ecdc4 50%, #45b7d1 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  animation: gradientShift 3s ease-in-out infinite alternate;
}

@keyframes gradientShift {
  0% { filter: hue-rotate(0deg); }
  100% { filter: hue-rotate(30deg); }
}

.hero-subtitle {
  font-size: 1.4rem;
  color: rgba(255, 255, 255, 0.95);
  margin-bottom: 40px;
  line-height: 1.7;
  font-weight: 400;
  text-shadow: 0 2px 10px rgba(0, 0, 0, 0.2);
}

.hero-actions {
  display: flex;
  gap: 20px;
  margin-top: 40px;
}

:deep(.hero-actions .ant-btn) {
  height: 50px;
  padding: 0 32px;
  font-size: 16px;
  font-weight: 600;
  border-radius: 25px;
  border: none;
  transition: all 0.3s ease;
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.15);
}

:deep(.hero-actions .ant-btn-primary) {
  background: linear-gradient(135deg, #ff6b6b 0%, #ee5a52 100%);
  transform: translateY(0);
}

:deep(.hero-actions .ant-btn-primary:hover) {
  transform: translateY(-3px);
  box-shadow: 0 12px 35px rgba(255, 107, 107, 0.4);
}

:deep(.hero-actions .ant-btn-default) {
  background: rgba(255, 255, 255, 0.15);
  backdrop-filter: blur(10px);
  color: white;
  border: 2px solid rgba(255, 255, 255, 0.3);
}

:deep(.hero-actions .ant-btn-default:hover) {
  background: rgba(255, 255, 255, 0.25);
  transform: translateY(-3px);
  box-shadow: 0 12px 35px rgba(255, 255, 255, 0.2);
}

.hero-image {
  flex: 1;
  display: flex;
  justify-content: center;
  align-items: center;
  animation: slideInRight 1s ease-out;
}

.floating-cards {
  position: relative;
  width: 350px;
  height: 350px;
}

.ai-card {
  position: absolute;
  background: rgba(255, 255, 255, 0.15);
  backdrop-filter: blur(20px);
  border: 1px solid rgba(255, 255, 255, 0.3);
  border-radius: 20px;
  padding: 24px;
  text-align: center;
  color: white;
  animation: float 6s ease-in-out infinite;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
  transition: all 0.3s ease;
}

.ai-card:hover {
  transform: scale(1.05) translateY(-10px);
  box-shadow: 0 15px 45px rgba(0, 0, 0, 0.2);
}

.ai-card:nth-child(1) {
  top: 0;
  left: 0;
  animation-delay: 0s;
  background: linear-gradient(135deg, rgba(255, 107, 107, 0.2) 0%, rgba(255, 255, 255, 0.1) 100%);
}

.ai-card:nth-child(2) {
  top: 60px;
  right: 0;
  animation-delay: 2s;
  background: linear-gradient(135deg, rgba(78, 205, 196, 0.2) 0%, rgba(255, 255, 255, 0.1) 100%);
}

.ai-card:nth-child(3) {
  bottom: 0;
  left: 60px;
  animation-delay: 4s;
  background: linear-gradient(135deg, rgba(69, 183, 209, 0.2) 0%, rgba(255, 255, 255, 0.1) 100%);
}

.card-icon {
  font-size: 2.5rem;
  margin-bottom: 12px;
  filter: drop-shadow(0 4px 8px rgba(0, 0, 0, 0.2));
}

.card-name {
  font-weight: 700;
  margin-bottom: 8px;
  font-size: 1.1rem;
}

.card-provider {
  font-size: 0.9rem;
  opacity: 0.9;
  font-weight: 500;
}

@keyframes float {
  0%, 100% { transform: translateY(0px) rotate(0deg); }
  33% { transform: translateY(-15px) rotate(1deg); }
  66% { transform: translateY(-5px) rotate(-1deg); }
}

@keyframes slideInLeft {
  from { opacity: 0; transform: translateX(-50px); }
  to { opacity: 1; transform: translateX(0); }
}

@keyframes slideInRight {
  from { opacity: 0; transform: translateX(50px); }
  to { opacity: 1; transform: translateX(0); }
}

.features, .models {
  padding: 100px 0;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  position: relative;
}

.features::before, .models::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    radial-gradient(circle at 10% 20%, rgba(102, 126, 234, 0.05) 0%, transparent 50%),
    radial-gradient(circle at 90% 80%, rgba(118, 75, 162, 0.05) 0%, transparent 50%);
  pointer-events: none;
}

/* 使用全局 container 样式 */

.section-title {
  text-align: center;
  font-size: 3rem;
  font-weight: 800;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 60px;
  position: relative;
}

.section-title::after {
  content: '';
  position: absolute;
  bottom: -10px;
  left: 50%;
  transform: translateX(-50%);
  width: 80px;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 2px;
}

.section-subtitle {
  text-align: center;
  font-size: 1.2rem;
  color: #4a5568;
  margin-bottom: 50px;
  font-weight: 400;
  line-height: 1.6;
}

.loading-container {
  display: flex;
  justify-content: center;
  align-items: center;
  min-height: 300px;
}

.loading-placeholder {
  width: 100%;
  height: 200px;
}

.features-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(350px, 1fr));
  gap: 40px;
}

.feature-card {
  background: white;
  border-radius: 20px;
  padding: 40px;
  text-align: center;
  transition: all 0.4s ease;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  position: relative;
  overflow: hidden;
}

.feature-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.feature-card:hover {
  transform: translateY(-10px);
  box-shadow: 0 20px 50px rgba(102, 126, 234, 0.15);
}

.feature-icon {
  font-size: 3.5rem;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 24px;
  transition: all 0.3s ease;
}

.feature-card:hover .feature-icon {
  transform: scale(1.1);
}

.feature-card h3 {
  font-size: 1.6rem;
  margin-bottom: 16px;
  color: #2d3748;
  font-weight: 700;
}

.feature-card p {
  color: #4a5568;
  line-height: 1.7;
  font-size: 1rem;
}

.models-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(300px, 1fr));
  gap: 24px;
}

.model-card {
  background: white;
  border-radius: 16px;
  padding: 20px;
  transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
  box-shadow: 0 6px 20px rgba(0, 0, 0, 0.06);
  border: 1px solid rgba(102, 126, 234, 0.1);
  position: relative;
  overflow: hidden;
}

.model-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

.model-card:hover {
  transform: translateY(-12px);
  box-shadow: 0 25px 60px rgba(78, 205, 196, 0.2);
}

.model-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 20px;
}

.model-title-section {
  display: flex;
  align-items: center;
  gap: 16px;
  flex: 1;
}

.model-icon {
  width: 48px;
  height: 48px;
  border-radius: 12px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  display: flex;
  align-items: center;
  justify-content: center;
  color: white;
  font-size: 24px;
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.3);
}

.model-title-info h3 {
  font-size: 1.5rem;
  margin: 0 0 4px 0;
  color: #2d3748;
  font-weight: 700;
}

.model-provider {
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  color: white;
  padding: 4px 12px;
  border-radius: 12px;
  font-size: 0.8rem;
  font-weight: 600;
}

.model-status {
  padding: 6px 12px;
  border-radius: 12px;
  font-size: 0.8rem;
  font-weight: 600;
  text-align: center;
  min-width: 60px;
}

.model-status.active {
  background: linear-gradient(135deg, #48bb78 0%, #38a169 100%);
  color: white;
}

.model-status:not(.active) {
  background: linear-gradient(135deg, #ed8936 0%, #dd6b20 100%);
  color: white;
}

.model-description {
  color: #4a5568;
  margin-bottom: 12px;
  line-height: 1.6;
  font-size: 0.95rem;
}



.empty-state {
  text-align: center;
  padding: 80px 20px;
  color: #4a5568;
}

.empty-icon {
  font-size: 4rem;
  margin-bottom: 20px;
  opacity: 0.6;
}

.empty-state h3 {
  font-size: 1.5rem;
  margin-bottom: 12px;
  color: #2d3748;
}

.empty-state p {
  font-size: 1rem;
  opacity: 0.8;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .hero {
    flex-direction: column;
    text-align: center;
    padding: 40px 20px;
    min-height: 70vh;
  }
  
  .hero-content {
    padding-right: 0;
    margin-bottom: 50px;
  }
  
  .hero-title {
    font-size: 2.8rem;
  }
  
  .hero-subtitle {
    font-size: 1.2rem;
  }
  
  .hero-actions {
    justify-content: center;
    flex-direction: column;
    align-items: center;
    gap: 16px;
  }
  
  :deep(.hero-actions .ant-btn) {
    width: 200px;
  }
  
  .floating-cards {
    width: 280px;
    height: 280px;
  }
  
  .features-grid {
    grid-template-columns: 1fr;
    gap: 30px;
  }
  
  .models-grid {
    grid-template-columns: 1fr;
    gap: 25px;
  }
  
  .model-card {
    padding: 24px;
  }
  
  .model-title-section {
    gap: 12px;
  }
  
  .model-icon {
    width: 40px;
    height: 40px;
    font-size: 20px;
  }
  

  
  .section-title {
    font-size: 2.2rem;
  }
}

@media (max-width: 480px) {
  .hero-title {
    font-size: 2.2rem;
  }
  
  .hero-subtitle {
    font-size: 1.1rem;
  }
  
  .floating-cards {
    width: 240px;
    height: 240px;
  }
  
  .ai-card {
    padding: 16px;
  }
  
  .feature-card {
    padding: 24px;
  }
  
  .model-card {
    padding: 20px;
  }
  
  .model-title-section {
    gap: 10px;
  }
  
  .model-icon {
    width: 36px;
    height: 36px;
    font-size: 18px;
  }
  
  .model-title-info h3 {
    font-size: 1.3rem;
  }
}
</style>