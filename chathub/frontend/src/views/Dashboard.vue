<template>
  <div class="dashboard">
    <div class="container">
      <!-- 页面标题 -->
      <div class="dashboard-header">
        <h1 class="page-title">控制台</h1>
        <p class="page-subtitle">管理您的AI使用情况和账户设置</p>
      </div>

      <!-- 标签页导航 -->
      <div class="dashboard-tabs">
        <a-tabs v-model:activeKey="activeTab" @change="handleTabChange">
          <a-tab-pane key="overview" tab="概览">
            <div class="tab-content">
              <!-- 统计卡片 -->
              <div class="stats-grid">
                <div class="stat-card conversations">
                  <div class="stat-icon-wrapper">
                    <CommentOutlined class="stat-icon" />
                  </div>
                  <div class="stat-content">
                    <div class="stat-number">{{ userStats.total_conversations || 0 }}</div>
                    <div class="stat-label">总对话数</div>
                  </div>
                  <div class="stat-decoration"></div>
                </div>
                
                <div class="stat-card messages">
                  <div class="stat-icon-wrapper">
                    <MessageOutlined class="stat-icon" />
                  </div>
                  <div class="stat-content">
                    <div class="stat-number">{{ userStats.total_messages || 0 }}</div>
                    <div class="stat-label">总消息数</div>
                  </div>
                  <div class="stat-decoration"></div>
                </div>
                
                <div class="stat-card tokens">
                  <div class="stat-icon-wrapper">
                    <LineChartOutlined class="stat-icon" />
                  </div>
                  <div class="stat-content">
                    <div class="stat-number">{{ userStats.tokens_used || 0 }}</div>
                    <div class="stat-label">已使用Token</div>
                  </div>
                  <div class="stat-decoration"></div>
                </div>
                
                <div class="stat-card active-days">
                  <div class="stat-icon-wrapper">
                    <TrophyOutlined class="stat-icon" />
                  </div>
                  <div class="stat-content">
                    <div class="stat-number">{{ userStats.days_active || 0 }}</div>
                    <div class="stat-label">活跃天数</div>
                  </div>
                  <div class="stat-decoration"></div>
                </div>
              </div>
              
              <!-- 使用情况 -->
              <div class="usage-section">
                <a-card class="usage-card">
                  <template #title>
                    <div class="card-header">
                      <h3>使用情况</h3>
                      <a-button type="text" @click="refreshStats">
                        <ReloadOutlined />
                        刷新
                      </a-button>
                    </div>
                  </template>
                  
                  <div class="usage-content">
                    <div class="usage-progress">
                      <a-progress 
                        :percent="userStats.usage_percentage || 0"
                        :stroke-color="getProgressColor()"
                        :stroke-width="12"
                        :show-info="true"
                      />
                    </div>
                    
                    <div class="usage-details">
                      <div class="usage-item">
                        <span class="label">本月使用</span>
                        <span class="value">{{ userStats.monthly_usage || 0 }} tokens</span>
                      </div>
                      <div class="usage-item">
                        <span class="label">配额限制</span>
                        <span class="value">{{ userStats.quota_limit || 0 }} tokens</span>
                      </div>
                      <div class="usage-item">
                        <span class="label">剩余配额</span>
                        <span class="value">{{ (userStats.quota_limit || 0) - (userStats.monthly_usage || 0) }} tokens</span>
                      </div>
                      <div class="usage-item">
                        <span class="label">状态</span>
                        <a-tag :color="getUsageTagType()">
                          {{ userStats.usage_percentage >= 90 ? '即将用完' : userStats.usage_percentage >= 70 ? '使用较多' : '正常' }}
                        </a-tag>
                      </div>
                    </div>
                  </div>
                </a-card>
              </div>
              
              <!-- 账户信息和设置 -->
              <div class="account-section">
                <div class="section-grid">
                  <!-- 账户信息 -->
                  <a-card class="account-card">
                    <template #title>
                      <h3>账户信息</h3>
                    </template>
                    
                    <div class="account-info">
                      <div class="info-item">
                        <span class="label">用户名:</span>
                        <span class="value">{{ userStats.user?.username || 'N/A' }}</span>
                      </div>
                      <div class="info-item">
                        <span class="label">邮箱:</span>
                        <span class="value">{{ userStats.user?.email || 'N/A' }}</span>
                      </div>
                      <div class="info-item">
                        <span class="label">订阅类型:</span>
                        <a-tag :color="getSubscriptionTagType()">
                          {{ getSubscriptionLabel() }}
                        </a-tag>
                      </div>
                      <div class="info-item">
                        <span class="label">注册时间:</span>
                        <span class="value">{{ formatDate(userStats.user?.created_at) }}</span>
                      </div>
                    </div>
                    
                    <div class="account-actions">
                      <a-button type="primary" @click="$router.push('/pricing')">
                        升级方案
                      </a-button>
                      <a-button @click="showApiKey = !showApiKey">
                        {{ showApiKey ? '隐藏' : '显示' }} API Key
                      </a-button>
                    </div>
                    
                    <div v-if="showApiKey" class="api-key-section">
                      <div class="api-key-label">API Key:</div>
                      <div class="api-key-value">
                        <a-input-password 
                          :value="userStats.user?.api_key || 'N/A'" 
                          readonly
                          :visible-password="true"
                        />
                      </div>
                    </div>
                  </a-card>
                  
                  <!-- 快速操作 -->
                  <a-card class="actions-card">
                    <template #title>
                      <h3>快速操作</h3>
                    </template>
                    
                    <div class="quick-actions">
                      <a-button 
                        type="primary" 
                        size="large"
                        class="action-btn"
                        @click="$router.push('/chat')"
                      >
                        <CommentOutlined />
                        开始新对话
                      </a-button>
                      
                      <a-button 
                        size="large"
                        class="action-btn"
                        @click="$router.push('/api-keys')"
                      >
                        <KeyOutlined />
                        API密钥
                      </a-button>
                      
                      <a-button 
                        size="large"
                        class="action-btn"
                        @click="$router.push('/pricing')"
                      >
                        <DollarOutlined />
                        查看价格
                      </a-button>
                      
                      <a-button 
                        size="large"
                        class="action-btn"
                        @click="refreshStats"
                      >
                        <ReloadOutlined />
                        刷新数据
                      </a-button>
                      
                      <a-button 
                        size="large"
                        class="action-btn"
                        type="default"
                        @click="exportData"
                      >
                        <DownloadOutlined />
                        导出数据
                      </a-button>
                    </div>
                  </a-card>
                </div>
              </div>
            </div>
          </a-tab-pane>
          
          <a-tab-pane key="recent-conversations" tab="最近对话">
            <div class="tab-content">
              <div class="recent-conversations-full">
                <a-card>
                  <template #title>
                    <div class="card-header">
                      <h3>最近对话</h3>
                      <a-button type="text" @click="activeTab = 'conversations'">
                        查看全部历史
                      </a-button>
                    </div>
                  </template>
                  
                  <div v-if="allRecentConversations.length === 0" class="empty-state">
                    <CommentOutlined class="empty-icon" />
                    <p>暂无对话记录</p>
                    <a-button type="primary" @click="$router.push('/chat')">
                      开始第一个对话
                    </a-button>
                  </div>
                  
                  <div v-else class="conversations-list">
                    <div 
                      v-for="conversation in allRecentConversations" 
                      :key="conversation.conversation_id"
                      class="conversation-item"
                      @click="goToConversation(conversation.conversation_id)"
                    >
                      <div class="conversation-content">
                        <div class="conversation-title">{{ conversation.title }}</div>
                        <div class="conversation-meta">
                          <a-tag size="small">{{ conversation.model }}</a-tag>
                          <span class="time">{{ formatTime(conversation.updated_at) }}</span>
                        </div>
                        <div class="conversation-preview">
                          {{ conversation.preview || '暂无预览' }}
                        </div>
                      </div>
                      <RightOutlined class="arrow-icon" />
                    </div>
                  </div>
                </a-card>
              </div>
            </div>
          </a-tab-pane>
          
          <a-tab-pane key="conversations" tab="对话历史">
            <div class="tab-content">
              <ConversationHistory />
            </div>
          </a-tab-pane>
        </a-tabs>
      </div>
    </div>
  </div>
</template>

<script>
import request from '../utils/request'
import { message } from 'ant-design-vue'
import ConversationHistory from '../components/ConversationHistory.vue'
import { 
  CommentOutlined, 
  MessageOutlined,
  LineChartOutlined,
  TrophyOutlined,
  KeyOutlined,
  DollarOutlined,
  ReloadOutlined,
  DownloadOutlined,
  RightOutlined
} from '@ant-design/icons-vue'

export default {
  name: 'Dashboard',
  components: {
    CommentOutlined,
    MessageOutlined,
    LineChartOutlined,
    TrophyOutlined,
    KeyOutlined,
    DollarOutlined,
    ReloadOutlined,
    DownloadOutlined,
    RightOutlined,
    ConversationHistory
  },
  data() {
    return {
      activeTab: 'overview',
      userStats: {},
      recentConversations: [],
      allRecentConversations: [],
      showApiKey: false,
      loading: false
    }
  },
  async mounted() {
    await this.loadDashboardData()
  },
  methods: {
    handleTabChange(activeKey) {
      this.activeTab = activeKey
    },
    
    async loadDashboardData() {
      this.loading = true
      try {
        // 加载用户统计
        const statsResponse = await request.get('/api/stats', {
          params: { user_id: 1 }
        })
        this.userStats = statsResponse.data
        
        // 加载最近对话
        const conversationsResponse = await request.get('/api/conversations', {
          params: { user_id: 1 }
        })
        this.recentConversations = conversationsResponse.data.slice(0, 5)
        
        // 为最近对话标签页加载更多对话
        const allConversationsResponse = await request.get('/api/conversations', {
          params: { user_id: 1, limit: 20 }
        })
        this.allRecentConversations = allConversationsResponse.data || []
        
      } catch (error) {
        console.error('加载控制台数据失败:', error)
        message.error('加载数据失败', 1)
      } finally {
        this.loading = false
      }
    },
    
    async refreshStats() {
      await this.loadDashboardData()
      message.success('数据已刷新', 2)
    },
    
    exportData() {
      // 导出数据逻辑
      message.info('数据导出功能开发中...', 2)
    },
    
    goToConversation(conversationId) {
      this.$router.push(`/chat?conversation=${conversationId}`)
    },
    
    getUsageTagType() {
      const percentage = this.userStats.usage_percentage || 0
      if (percentage >= 90) return 'red'
      if (percentage >= 70) return 'orange'
      return 'green'
    },
    
    getProgressColor() {
      const percentage = this.userStats.usage_percentage || 0
      if (percentage >= 90) return '#f56c6c'
      if (percentage >= 70) return '#e6a23c'
      return '#67c23a'
    },
    
    getSubscriptionTagType() {
      const type = this.userStats.user?.subscription_type
      switch (type) {
        case 'pro': return 'green'
        case 'enterprise': return 'gold'
        default: return 'blue'
      }
    },
    
    getSubscriptionLabel() {
      const type = this.userStats.user?.subscription_type
      switch (type) {
        case 'free': return '免费版'
        case 'pro': return '专业版'
        case 'enterprise': return '企业版'
        default: return '未知'
      }
    },
    
    formatDate(dateString) {
      if (!dateString) return 'N/A'
      return new Date(dateString).toLocaleDateString('zh-CN', {
        year: 'numeric',
        month: 'long',
        day: 'numeric'
      })
    },
    
    formatTime(timestamp) {
      const date = new Date(timestamp)
      const now = new Date()
      const diff = now - date
      
      if (diff < 60000) return '刚刚'
      if (diff < 3600000) return `${Math.floor(diff / 60000)}分钟前`
      if (diff < 86400000) return `${Math.floor(diff / 3600000)}小时前`
      
      return date.toLocaleDateString('zh-CN', {
        month: 'short',
        day: 'numeric',
        hour: '2-digit',
        minute: '2-digit'
      })
    }
  }
}
</script>

<style scoped>
.dashboard {
  min-height: calc(100vh - 140px);
  padding: 40px 24px;
  max-width: 1200px;
  margin: 0 auto;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  position: relative;
}

.dashboard::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  bottom: 0;
  background: 
    radial-gradient(circle at 20% 20%, rgba(102, 126, 234, 0.05) 0%, transparent 50%),
    radial-gradient(circle at 80% 80%, rgba(118, 75, 162, 0.05) 0%, transparent 50%);
  pointer-events: none;
}

.dashboard-header {
  text-align: center;
  margin-bottom: 40px;
}

.page-title {
  font-size: 3rem;
  font-weight: 800;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 16px;
  position: relative;
  z-index: 1;
}

.page-subtitle {
  font-size: 1.1rem;
  color: #718096;
  margin: 0;
}

/* 标签页样式 */
.dashboard-tabs {
  margin-top: 24px;
}

.tab-content {
  padding-top: 16px;
}

/* 统计卡片样式 */
.stats-grid {
  display: grid;
  grid-template-columns: repeat(4, 1fr);
  gap: 24px;
  margin-bottom: 40px;
}

.stat-card {
  background: white;
  border-radius: 24px;
  padding: 32px 24px;
  transition: all 0.4s cubic-bezier(0.4, 0, 0.2, 1);
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(255, 255, 255, 0.2);
  position: relative;
  overflow: hidden;
  display: flex;
  flex-direction: column;
  align-items: center;
  text-align: center;
  min-height: 180px;
  backdrop-filter: blur(10px);
}

.stat-card:hover {
  transform: translateY(-8px) scale(1.02);
  box-shadow: 0 20px 40px rgba(0, 0, 0, 0.12);
}

/* 不同卡片的主题色 */
.stat-card.conversations {
  background: linear-gradient(135deg, rgba(102, 126, 234, 0.1) 0%, rgba(118, 75, 162, 0.05) 100%);
  border: 1px solid rgba(102, 126, 234, 0.2);
}

.stat-card.messages {
  background: linear-gradient(135deg, rgba(78, 205, 196, 0.1) 0%, rgba(68, 160, 141, 0.05) 100%);
  border: 1px solid rgba(78, 205, 196, 0.2);
}

.stat-card.tokens {
  background: linear-gradient(135deg, rgba(255, 107, 107, 0.1) 0%, rgba(238, 90, 82, 0.05) 100%);
  border: 1px solid rgba(255, 107, 107, 0.2);
}

.stat-card.active-days {
  background: linear-gradient(135deg, rgba(255, 193, 7, 0.1) 0%, rgba(255, 152, 0, 0.05) 100%);
  border: 1px solid rgba(255, 193, 7, 0.2);
}

.stat-icon-wrapper {
  width: 80px;
  height: 80px;
  border-radius: 50%;
  display: flex;
  align-items: center;
  justify-content: center;
  margin-bottom: 20px;
  position: relative;
  transition: all 0.4s ease;
}

.stat-card.conversations .stat-icon-wrapper {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  box-shadow: 0 8px 32px rgba(102, 126, 234, 0.3);
}

.stat-card.messages .stat-icon-wrapper {
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  box-shadow: 0 8px 32px rgba(78, 205, 196, 0.3);
}

.stat-card.tokens .stat-icon-wrapper {
  background: linear-gradient(135deg, #ff6b6b 0%, #ee5a52 100%);
  box-shadow: 0 8px 32px rgba(255, 107, 107, 0.3);
}

.stat-card.active-days .stat-icon-wrapper {
  background: linear-gradient(135deg, #ffc107 0%, #ff9800 100%);
  box-shadow: 0 8px 32px rgba(255, 193, 7, 0.3);
}

.stat-icon {
  font-size: 2.2rem;
  color: white;
  transition: all 0.3s ease;
}

.stat-card:hover .stat-icon-wrapper {
  transform: scale(1.1) rotate(5deg);
}

.stat-card:hover .stat-icon {
  transform: scale(1.1);
}

.stat-content {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
}

.stat-number {
  font-size: 2.8rem;
  font-weight: 900;
  margin-bottom: 8px;
  background: linear-gradient(135deg, #2d3748 0%, #4a5568 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  line-height: 1;
}

.stat-label {
  font-size: 1rem;
  color: #718096;
  font-weight: 600;
  letter-spacing: 0.5px;
  text-transform: uppercase;
}

.stat-decoration {
  position: absolute;
  bottom: 0;
  left: 0;
  right: 0;
  height: 4px;
  border-radius: 0 0 24px 24px;
  transition: all 0.3s ease;
}

.stat-card.conversations .stat-decoration {
  background: linear-gradient(90deg, #667eea 0%, #764ba2 100%);
}

.stat-card.messages .stat-decoration {
  background: linear-gradient(90deg, #4ecdc4 0%, #44a08d 100%);
}

.stat-card.tokens .stat-decoration {
  background: linear-gradient(90deg, #ff6b6b 0%, #ee5a52 100%);
}

.stat-card.active-days .stat-decoration {
  background: linear-gradient(90deg, #ffc107 0%, #ff9800 100%);
}

.stat-card:hover .stat-decoration {
  height: 6px;
}

/* 使用情况样式 */
.usage-section {
  margin-bottom: 50px;
  position: relative;
  z-index: 1;
}

:deep(.usage-card) {
  background: white;
  border-radius: 20px;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
}

:deep(.usage-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

:deep(.usage-card .ant-card-head) {
  background: transparent;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
  padding: 24px 24px 16px;
}

:deep(.usage-card .ant-card-head-title) {
  color: #2d3748;
  font-weight: 700;
  font-size: 1.25rem;
}

:deep(.usage-card .ant-card-body) {
  background: transparent;
  padding: 24px;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  width: 100%;
}

.card-header h3 {
  margin: 0;
  color: #2d3748;
  font-weight: 700;
  font-size: 1.25rem;
}

.usage-progress {
  margin-bottom: 30px;
}

:deep(.usage-progress .ant-progress-text) {
  color: #2d3748;
  font-weight: 600;
}

:deep(.usage-progress .ant-progress-bg) {
  background: #e2e8f0;
}

.usage-details {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 24px;
}

.usage-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 20px;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  border-radius: 12px;
  border: 1px solid rgba(102, 126, 234, 0.1);
  transition: all 0.3s ease;
}

.usage-item:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.1);
}

.usage-item .label {
  color: #4a5568;
  font-weight: 600;
}

.usage-item .value {
  font-weight: 700;
  color: #2d3748;
  font-size: 1.1rem;
}

/* 账户信息样式 */
.account-section {
  margin-bottom: 50px;
  position: relative;
  z-index: 1;
}

.section-grid {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 30px;
}

:deep(.account-card),
:deep(.actions-card) {
  background: white;
  border-radius: 20px;
  box-shadow: 0 10px 30px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
}

:deep(.account-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

:deep(.actions-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #ff6b6b 0%, #ee5a52 100%);
}

:deep(.account-card .ant-card-head),
:deep(.actions-card .ant-card-head) {
  background: transparent;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
  padding: 24px 24px 16px;
}

:deep(.account-card .ant-card-head-title),
:deep(.actions-card .ant-card-head-title) {
  color: #2d3748;
  font-weight: 700;
  font-size: 1.25rem;
}

:deep(.account-card .ant-card-body),
:deep(.actions-card .ant-card-body) {
  background: transparent;
  padding: 24px;
}

.account-info {
  margin-bottom: 24px;
}

.info-item {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 12px 0;
  border-bottom: 1px solid #f0f0f0;
}

.info-item:last-child {
  border-bottom: none;
}

.info-item .label {
  color: #4a5568;
  font-weight: 600;
}

.info-item .value {
  font-weight: 500;
  color: #2d3748;
}

.account-actions {
  display: flex;
  gap: 12px;
  margin-bottom: 20px;
}

.api-key-section {
  margin-top: 20px;
  padding: 16px;
  background: #f7fafc;
  border-radius: 8px;
  border: 1px solid #e2e8f0;
}

.api-key-label {
  font-weight: 600;
  color: #4a5568;
  margin-bottom: 8px;
}

.quick-actions {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 16px;
}

.action-btn {
  height: 60px;
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  gap: 8px;
  border-radius: 12px;
  transition: all 0.3s ease;
}

.action-btn:hover {
  transform: translateY(-2px);
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.1);
}

/* 最近对话样式 */
.recent-conversations {
  margin-top: 24px;
}

.recent-conversations-full {
  margin-top: 0;
}

.conversations-list {
  display: flex;
  flex-direction: column;
  gap: 12px;
}

.conversation-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 16px;
  border: 1px solid #f0f0f0;
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.3s ease;
}

.conversation-item:hover {
  border-color: #1890ff;
  box-shadow: 0 2px 8px rgba(24, 144, 255, 0.1);
}

.conversation-content {
  flex: 1;
}

.conversation-title {
  font-weight: 500;
  margin-bottom: 8px;
  color: #262626;
}

.conversation-meta {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 8px;
}

.conversation-meta .time {
  color: #8c8c8c;
  font-size: 12px;
}

.conversation-preview {
  color: #595959;
  font-size: 14px;
  line-height: 1.4;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.arrow-icon {
  color: #bfbfbf;
  font-size: 12px;
}

.empty-state {
  text-align: center;
  padding: 40px 20px;
  color: #8c8c8c;
}

.empty-icon {
  font-size: 48px;
  color: #d9d9d9;
  margin-bottom: 16px;
}

/* 响应式设计 */
@media (max-width: 1200px) {
  .section-grid {
    grid-template-columns: 1fr;
    gap: 24px;
  }
  
  .quick-actions {
    grid-template-columns: 1fr;
  }
}

@media (min-width: 1024px) {
  .stats-grid {
    grid-template-columns: repeat(4, 1fr);
  }
}

@media (max-width: 1023px) and (min-width: 769px) {
  .stats-grid {
    grid-template-columns: repeat(4, 1fr);
  }
  
  .stat-card {
    min-height: 160px;
    padding: 24px 16px;
  }
  
  .stat-icon-wrapper {
    width: 60px;
    height: 60px;
    margin-bottom: 16px;
  }
  
  .stat-icon {
    font-size: 1.8rem;
  }
  
  .stat-number {
    font-size: 2.2rem;
  }
  
  .stat-label {
    font-size: 0.9rem;
  }
}

@media (max-width: 768px) {
  .dashboard {
    padding: 20px 16px;
  }
  
  .page-title {
    font-size: 2.5rem;
  }
  
  .stats-grid {
    grid-template-columns: repeat(2, 1fr);
    gap: 16px;
  }
  
  .stat-card {
    min-height: 140px;
    padding: 20px 16px;
  }
  
  .stat-icon-wrapper {
    width: 50px;
    height: 50px;
    margin-bottom: 12px;
  }
  
  .stat-icon {
    font-size: 1.5rem;
  }
  
  .stat-number {
    font-size: 1.8rem;
  }
  
  .stat-label {
    font-size: 0.8rem;
  }
  
  .usage-details {
    grid-template-columns: 1fr;
  }
  
  .section-grid {
    grid-template-columns: 1fr;
  }
  
  .quick-actions {
    grid-template-columns: 1fr;
  }
  
  .account-actions {
    flex-direction: column;
  }
}

@media (max-width: 480px) {
  .stats-grid {
    grid-template-columns: 1fr;
  }
  
  .page-title {
    font-size: 2rem;
  }
  
  .conversation-meta {
    flex-direction: column;
    align-items: flex-start;
    gap: 4px;
  }
}
</style>