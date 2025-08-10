<template>
  <div class="conversation-history">
    <div class="page-header">
      <div class="header-content">
        <div class="title-section">
          <h1>对话历史</h1>
          <p class="subtitle">管理和查看您的所有对话记录</p>
        </div>
        <div class="header-actions">
          <a-input-search
            v-model:value="searchKeyword"
            placeholder="搜索对话..."
            style="width: 300px"
            @search="handleSearch"
            @change="handleSearch"
          />
          <a-button type="primary" @click="$router.push('/chat')">
            <PlusOutlined />
            新建对话
          </a-button>
        </div>
      </div>
    </div>

    <div class="page-content">
      <!-- 筛选器 -->
      <div class="filters">
        <a-card>
          <div class="filter-row">
            <div class="filter-item">
              <label>模型筛选：</label>
              <a-select
                v-model:value="selectedModel"
                placeholder="选择模型"
                style="width: 200px"
                @change="handleFilter"
                allowClear
              >
                <a-select-option value="">全部模型</a-select-option>
                <a-select-option v-for="model in availableModels" :key="model" :value="model">
                  {{ model }}
                </a-select-option>
              </a-select>
            </div>
            <div class="filter-item">
              <label>时间范围：</label>
              <a-range-picker
                v-model:value="dateRange"
                @change="handleFilter"
                style="width: 300px"
              />
            </div>
            <div class="filter-item">
              <label>排序方式：</label>
              <a-select
                v-model:value="sortBy"
                style="width: 150px"
                @change="handleFilter"
              >
                <a-select-option value="updated_at_desc">最近更新</a-select-option>
                <a-select-option value="created_at_desc">创建时间</a-select-option>
                <a-select-option value="title_asc">标题A-Z</a-select-option>
              </a-select>
            </div>
          </div>
        </a-card>
      </div>

      <!-- 对话列表 -->
      <div class="conversations-container">
        <a-spin :spinning="loading">
          <div v-if="filteredConversations.length === 0 && !loading" class="empty-state">
            <CommentOutlined class="empty-icon" />
            <h3>{{ searchKeyword ? '未找到匹配的对话' : '暂无对话记录' }}</h3>
            <p>{{ searchKeyword ? '尝试调整搜索条件' : '开始您的第一个对话吧' }}</p>
            <a-button type="primary" @click="$router.push('/chat')">
              <PlusOutlined />
              开始对话
            </a-button>
          </div>

          <div v-else class="conversations-grid">
            <div
              v-for="conversation in paginatedConversations"
              :key="conversation.conversation_id"
              class="conversation-card"
              @click="goToConversation(conversation.conversation_id)"
            >
              <div class="card-header">
                <div class="conversation-title">
                  {{ conversation.title }}
                </div>
                <div class="card-actions" @click.stop>
                  <a-dropdown>
                    <a-button type="text" size="small">
                      <MoreOutlined />
                    </a-button>
                    <template #overlay>
                      <a-menu>
                        <a-menu-item @click="renameConversation(conversation)">
                          <EditOutlined />
                          重命名
                        </a-menu-item>
                        <a-menu-item @click="exportConversation(conversation)">
                          <DownloadOutlined />
                          导出
                        </a-menu-item>
                        <a-menu-divider />
                        <a-menu-item 
                          @click="deleteConversation(conversation.conversation_id)"
                          class="danger-item"
                        >
                          <DeleteOutlined />
                          删除
                        </a-menu-item>
                      </a-menu>
                    </template>
                  </a-dropdown>
                </div>
              </div>

              <div class="card-content">
                <div class="conversation-meta">
                  <a-tag color="blue" size="small">{{ conversation.model }}</a-tag>
                  <span class="message-count">
                    <MessageOutlined />
                    {{ conversation.message_count || 0 }} 条消息
                  </span>
                </div>
                
                <div class="conversation-preview" v-if="conversation.last_message">
                  {{ conversation.last_message }}
                </div>
              </div>

              <div class="card-footer">
                <div class="time-info">
                  <div class="created-time">
                    创建：{{ formatTime(conversation.created_at) }}
                  </div>
                  <div class="updated-time">
                    更新：{{ formatTime(conversation.updated_at) }}
                  </div>
                </div>
                <RightOutlined class="arrow-icon" />
              </div>
            </div>
          </div>

          <!-- 分页 -->
          <div class="pagination-container" v-if="filteredConversations.length > pageSize">
            <a-pagination
              v-model:current="currentPage"
              :total="filteredConversations.length"
              :page-size="pageSize"
              :show-size-changer="true"
              :show-quick-jumper="true"
              :show-total="(total, range) => `第 ${range[0]}-${range[1]} 条，共 ${total} 条`"
              @change="handlePageChange"
              @showSizeChange="handlePageSizeChange"
            />
          </div>
        </a-spin>
      </div>
    </div>

    <!-- 重命名对话弹窗 -->
    <a-modal
      v-model:open="renameModalVisible"
      title="重命名对话"
      @ok="handleRename"
      @cancel="renameModalVisible = false"
    >
      <a-input
        v-model:value="newTitle"
        placeholder="请输入新的对话标题"
        @pressEnter="handleRename"
      />
    </a-modal>
  </div>
</template>

<script>
import request from '../utils/request'
import { message } from 'ant-design-vue'
import {
  PlusOutlined,
  CommentOutlined,
  MessageOutlined,
  MoreOutlined,
  EditOutlined,
  DeleteOutlined,
  DownloadOutlined,
  RightOutlined
} from '@ant-design/icons-vue'
import dayjs from 'dayjs'

export default {
  name: 'ConversationHistory',
  components: {
    PlusOutlined,
    CommentOutlined,
    MessageOutlined,
    MoreOutlined,
    EditOutlined,
    DeleteOutlined,
    DownloadOutlined,
    RightOutlined
  },
  data() {
    return {
      conversations: [],
      filteredConversations: [],
      availableModels: [],
      loading: false,
      searchKeyword: '',
      selectedModel: '',
      dateRange: null,
      sortBy: 'updated_at_desc',
      currentPage: 1,
      pageSize: 12,
      renameModalVisible: false,
      currentConversation: null,
      newTitle: ''
    }
  },
  computed: {
    paginatedConversations() {
      const start = (this.currentPage - 1) * this.pageSize
      const end = start + this.pageSize
      return this.filteredConversations.slice(start, end)
    }
  },
  async mounted() {
    await this.loadConversations()
  },
  methods: {
    async loadConversations() {
      this.loading = true
      try {
        const response = await request.get('/api/conversations', {
          params: { user_id: 1 }
        })
        
        this.conversations = response.data.map(conv => ({
          ...conv,
          last_message: this.getLastMessagePreview(conv),
          message_count: conv.message_count || 0
        }))
        
        // 提取可用模型
        this.availableModels = [...new Set(this.conversations.map(conv => conv.model).filter(Boolean))]
        
        this.applyFilters()
      } catch (error) {
        console.error('加载对话失败:', error)
        message.error('加载对话失败')
      } finally {
        this.loading = false
      }
    },

    getLastMessagePreview(conversation) {
      // 这里可以根据需要从后端获取最后一条消息的预览
      // 暂时返回空字符串，后续可以优化
      return '点击查看对话内容...'
    },

    handleSearch() {
      this.currentPage = 1
      this.applyFilters()
    },

    handleFilter() {
      this.currentPage = 1
      this.applyFilters()
    },

    applyFilters() {
      let filtered = [...this.conversations]

      // 搜索过滤
      if (this.searchKeyword) {
        const keyword = this.searchKeyword.toLowerCase()
        filtered = filtered.filter(conv =>
          conv.title.toLowerCase().includes(keyword)
        )
      }

      // 模型过滤
      if (this.selectedModel) {
        filtered = filtered.filter(conv => conv.model === this.selectedModel)
      }

      // 日期范围过滤
      if (this.dateRange && this.dateRange.length === 2) {
        const [startDate, endDate] = this.dateRange
        filtered = filtered.filter(conv => {
          const convDate = dayjs(conv.created_at)
          return convDate.isAfter(startDate.startOf('day')) && convDate.isBefore(endDate.endOf('day'))
        })
      }

      // 排序
      filtered.sort((a, b) => {
        switch (this.sortBy) {
          case 'updated_at_desc':
            return new Date(b.updated_at) - new Date(a.updated_at)
          case 'created_at_desc':
            return new Date(b.created_at) - new Date(a.created_at)
          case 'title_asc':
            return a.title.localeCompare(b.title)
          default:
            return 0
        }
      })

      this.filteredConversations = filtered
    },

    handlePageChange(page) {
      this.currentPage = page
    },

    handlePageSizeChange(current, size) {
      this.pageSize = size
      this.currentPage = 1
    },

    goToConversation(conversationId) {
      this.$router.push(`/chat?conversation=${conversationId}`)
    },

    renameConversation(conversation) {
      this.currentConversation = conversation
      this.newTitle = conversation.title
      this.renameModalVisible = true
    },

    async handleRename() {
      if (!this.newTitle.trim()) {
        message.error('请输入对话标题')
        return
      }

      try {
        // 这里需要后端支持重命名API
        await request.put(`/api/conversations/${this.currentConversation.conversation_id}`, {
          title: this.newTitle.trim()
        })
        
        // 更新本地数据
        const index = this.conversations.findIndex(
          conv => conv.conversation_id === this.currentConversation.conversation_id
        )
        if (index !== -1) {
          this.conversations[index].title = this.newTitle.trim()
          this.applyFilters()
        }
        
        message.success('重命名成功')
        this.renameModalVisible = false
      } catch (error) {
        console.error('重命名失败:', error)
        message.error('重命名失败')
      }
    },

    async deleteConversation(conversationId) {
      try {
        await request.delete(`/api/conversations/${conversationId}`)
        
        // 从本地数据中移除
        this.conversations = this.conversations.filter(
          conv => conv.conversation_id !== conversationId
        )
        this.applyFilters()
        
        message.success('对话已删除')
      } catch (error) {
        console.error('删除对话失败:', error)
        message.error('删除对话失败')
      }
    },

    exportConversation(conversation) {
      // 导出对话功能
      message.info('导出功能开发中...')
    },

    formatTime(timeString) {
      if (!timeString) return 'N/A'
      const time = dayjs(timeString)
      const now = dayjs()
      
      if (time.isSame(now, 'day')) {
        return time.format('HH:mm')
      } else if (time.isSame(now.subtract(1, 'day'), 'day')) {
        return '昨天 ' + time.format('HH:mm')
      } else if (time.isSame(now, 'year')) {
        return time.format('MM-DD HH:mm')
      } else {
        return time.format('YYYY-MM-DD')
      }
    }
  }
}
</script>

<style scoped>
.conversation-history {
  min-height: 100vh;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.page-header {
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(20px);
  border-bottom: 1px solid rgba(0, 0, 0, 0.1);
  padding: 24px 0;
}

.header-content {
  max-width: 1200px;
  margin: 0 auto;
  padding: 0 24px;
  display: flex;
  justify-content: space-between;
  align-items: center;
}

.title-section h1 {
  margin: 0;
  font-size: 2rem;
  font-weight: 700;
  color: #2d3748;
}

.subtitle {
  margin: 4px 0 0 0;
  color: #718096;
  font-size: 1rem;
}

.header-actions {
  display: flex;
  gap: 16px;
  align-items: center;
}

.page-content {
  max-width: 1200px;
  margin: 0 auto;
  padding: 24px;
}

.filters {
  margin-bottom: 24px;
}

.filter-row {
  display: flex;
  gap: 24px;
  align-items: center;
  flex-wrap: wrap;
}

.filter-item {
  display: flex;
  align-items: center;
  gap: 8px;
}

.filter-item label {
  font-weight: 500;
  color: #4a5568;
  white-space: nowrap;
}

.conversations-container {
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(20px);
  border-radius: 16px;
  padding: 24px;
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.1);
}

.empty-state {
  text-align: center;
  padding: 80px 20px;
}

.empty-icon {
  font-size: 4rem;
  color: #cbd5e0;
  margin-bottom: 16px;
}

.empty-state h3 {
  color: #4a5568;
  margin-bottom: 8px;
}

.empty-state p {
  color: #718096;
  margin-bottom: 24px;
}

.conversations-grid {
  display: grid;
  grid-template-columns: repeat(auto-fill, minmax(350px, 1fr));
  gap: 20px;
}

.conversation-card {
  background: white;
  border-radius: 12px;
  padding: 20px;
  border: 1px solid #e2e8f0;
  cursor: pointer;
  transition: all 0.3s ease;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.05);
}

.conversation-card:hover {
  transform: translateY(-4px);
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.15);
  border-color: #667eea;
}

.card-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 12px;
}

.conversation-title {
  font-weight: 600;
  color: #2d3748;
  font-size: 1.1rem;
  line-height: 1.4;
  flex: 1;
  margin-right: 8px;
}

.card-actions {
  opacity: 0;
  transition: opacity 0.2s ease;
}

.conversation-card:hover .card-actions {
  opacity: 1;
}

.card-content {
  margin-bottom: 16px;
}

.conversation-meta {
  display: flex;
  align-items: center;
  gap: 12px;
  margin-bottom: 8px;
}

.message-count {
  display: flex;
  align-items: center;
  gap: 4px;
  color: #718096;
  font-size: 0.875rem;
}

.conversation-preview {
  color: #718096;
  font-size: 0.875rem;
  line-height: 1.4;
  display: -webkit-box;
  -webkit-line-clamp: 2;
  -webkit-box-orient: vertical;
  overflow: hidden;
}

.card-footer {
  display: flex;
  justify-content: space-between;
  align-items: flex-end;
}

.time-info {
  font-size: 0.75rem;
  color: #a0aec0;
}

.created-time {
  margin-bottom: 2px;
}

.arrow-icon {
  color: #cbd5e0;
  transition: all 0.2s ease;
}

.conversation-card:hover .arrow-icon {
  color: #667eea;
  transform: translateX(4px);
}

.pagination-container {
  margin-top: 32px;
  text-align: center;
}

.danger-item {
  color: #e53e3e !important;
}

.danger-item:hover {
  background-color: #fed7d7 !important;
}

:deep(.ant-card-body) {
  padding: 16px;
}

:deep(.ant-pagination) {
  display: flex;
  justify-content: center;
}

@media (max-width: 768px) {
  .header-content {
    flex-direction: column;
    gap: 16px;
    align-items: stretch;
  }

  .header-actions {
    justify-content: center;
  }

  .filter-row {
    flex-direction: column;
    align-items: stretch;
  }

  .filter-item {
    flex-direction: column;
    align-items: stretch;
    gap: 4px;
  }

  .conversations-grid {
    grid-template-columns: 1fr;
  }
}
</style>