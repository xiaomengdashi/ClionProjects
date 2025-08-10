<template>
  <div class="api-keys-container">
    <div class="header">
      <h1>API 密钥管理</h1>
      <p>管理各个AI模型的API密钥配置</p>
    </div>

    <!-- 添加新密钥 -->
    <a-card class="add-key-card" title="添加/更新 API 密钥">
      <a-form :model="newKey" :rules="rules" ref="keyForm" layout="vertical">
        <a-row :gutter="20">
          <a-col :span="8">
            <a-form-item label="模型平台" name="model_provider">
              <a-select v-model:value="newKey.model_provider" placeholder="选择模型平台" style="width: 100%">
                <a-select-option value="gpt-4">GPT-4</a-select-option>
                <a-select-option value="gpt-3.5-turbo">GPT-3.5 Turbo</a-select-option>
                <a-select-option value="claude-3">Claude-3</a-select-option>
                <a-select-option value="gemini-pro">Gemini Pro</a-select-option>
                <a-select-option value="ernie-bot">文心一言</a-select-option>
                <a-select-option value="qwen">通义千问</a-select-option>
                <a-select-option value="chatglm">智谱GLM</a-select-option>
                <a-select-option value="llama-2">Llama 2</a-select-option>
                <a-select-option value="siliconflow">硅基流动</a-select-option>
              </a-select>
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="API 密钥" name="api_key">
              <a-input-password 
                v-model:value="newKey.api_key" 
                placeholder="输入API密钥"
              />
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="Base URL">
              <a-input 
                v-model:value="newKey.base_url" 
                placeholder="可选，自定义API地址"
              />
            </a-form-item>
          </a-col>
        </a-row>
        <a-form-item>
          <a-button type="primary" @click="saveApiKey" :loading="saving">
            <template #icon><PlusOutlined /></template>
            保存密钥
          </a-button>
          <a-button @click="resetForm" style="margin-left: 8px;">重置</a-button>
        </a-form-item>
      </a-form>
    </a-card>

    <!-- 密钥列表 -->
    <a-card class="keys-list-card">
      <template #title>
        <span>已配置的 API 密钥</span>
      </template>
      <template #extra>
        <span style="margin-right: 16px; color: #666;">
          数据条数: {{ apiKeys.length }}
        </span>
        <a-button type="primary" size="small" @click="loadApiKeys">
          <template #icon><ReloadOutlined /></template>
          刷新
        </a-button>
      </template>

      <a-table 
        :data-source="apiKeys" 
        :loading="loading" 
        :columns="columns"
        row-key="id"
        :scroll="{ x: 1200 }"
      >
        <template #bodyCell="{ column, record }">
          <template v-if="column.dataIndex === 'model_provider'">
            <a-tag :color="getModelTagColor(record.model_provider)">
              {{ getModelDisplayName(record.model_provider) }}
            </a-tag>
          </template>
          
          <template v-else-if="column.dataIndex === 'api_key'">
            <span class="api-key-display">{{ maskApiKey(record.api_key) }}</span>
          </template>
          
          <template v-else-if="column.dataIndex === 'base_url'">
            <a-tag v-if="record.base_url" color="blue">
              {{ maskBaseUrl(record.base_url) }}
            </a-tag>
            <a-tag v-else color="default">默认</a-tag>
          </template>
          
          <template v-else-if="column.dataIndex === 'is_active'">
            <a-tag :color="record.is_active ? 'green' : 'red'">
              {{ record.is_active ? '启用' : '禁用' }}
            </a-tag>
          </template>
          
          <template v-else-if="column.dataIndex === 'updated_at'">
            {{ formatDate(record.updated_at) }}
          </template>
          
          <template v-else-if="column.dataIndex === 'action'">
            <a-space>
              <a-button 
                size="small" 
                :type="record.is_active ? 'default' : 'primary'"
                @click="toggleApiKey(record.id)"
              >
                {{ record.is_active ? '禁用' : '启用' }}
              </a-button>
              <a-button size="small" @click="editApiKey(record)">编辑</a-button>
              <a-button size="small" danger @click="deleteApiKey(record.id)">删除</a-button>
            </a-space>
          </template>
        </template>
      </a-table>
    </a-card>

    <!-- 编辑对话框 -->
    <a-modal v-model:open="editDialogVisible" title="编辑 API 密钥" width="600px">
      <a-form :model="editingKey" layout="vertical">
        <a-form-item label="模型平台">
          <a-input v-model:value="editingKey.model_provider" disabled />
        </a-form-item>
        <a-form-item label="API 密钥">
          <a-input-password 
            v-model:value="editingKey.api_key" 
            placeholder="输入新的API密钥"
          />
        </a-form-item>
        <a-form-item label="Base URL">
          <a-input 
            v-model:value="editingKey.base_url" 
            placeholder="可选，自定义API地址"
          />
        </a-form-item>
      </a-form>
      <template #footer>
        <a-button @click="editDialogVisible = false">取消</a-button>
        <a-button type="primary" @click="updateApiKey" :loading="updating">
          更新
        </a-button>
      </template>
    </a-modal>
  </div>
</template>

<script>
import { ref, reactive, onMounted } from 'vue'
import { message, Modal } from 'ant-design-vue'
import { PlusOutlined, ReloadOutlined } from '@ant-design/icons-vue'
import request from '../utils/request'

export default {
  name: 'ApiKeys',
  components: {
    PlusOutlined,
    ReloadOutlined
  },
  setup() {
    const loading = ref(false)
    const saving = ref(false)
    const updating = ref(false)
    const editDialogVisible = ref(false)
    
    const apiKeys = ref([])
    const newKey = reactive({
      model_provider: '',
      api_key: '',
      base_url: ''
    })
    
    const editingKey = reactive({
      id: null,
      model_provider: '',
      api_key: '',
      base_url: ''
    })

    // 掩码显示API Key（仅显示前10位）
    const maskApiKey = (key) => {
      if (!key) return ''
      return key.length > 10 ? key.slice(0, 10) + '...' : key
    }

    const maskBaseUrl = (url) => {
      if (!url) return ''
      return url.length > 30 ? url.slice(0, 30) + '...' : url
    }
    
    const rules = {
      model_provider: [
        { required: true, message: '请选择模型平台', trigger: 'change' }
      ],
      api_key: [
        { required: true, message: '请输入API密钥', trigger: 'blur' }
      ]
    }

    // 加载API密钥列表
    const loadApiKeys = async () => {
      loading.value = true
      try {
        const response = await request.get('/api/apikeys')
        apiKeys.value = response.data
      } catch (error) {
        console.error('加载API密钥失败:', error)
        message.error('加载API密钥失败: ' + error.message, 1)
      } finally {
        loading.value = false
      }
    }

    // 保存API密钥
    const saveApiKey = async () => {
      if (!newKey.model_provider || !newKey.api_key) {
        message.warning('请填写必要信息', 2)
        return
      }
      
      saving.value = true
      try {
        await request.post('/api/apikeys', newKey)
        message.success('API密钥保存成功', 2)
        resetForm()
        loadApiKeys()
      } catch (error) {
        message.error('保存失败: ' + error.response?.data?.error || error.message, 1)
      } finally {
        saving.value = false
      }
    }

    // 重置表单
    const resetForm = () => {
      newKey.model_provider = ''
      newKey.api_key = ''
      newKey.base_url = ''
    }

    // 编辑API密钥
    const editApiKey = (key) => {
      editingKey.id = key.id
      editingKey.model_provider = key.model_provider
      editingKey.api_key = key.api_key
      editingKey.base_url = key.base_url || ''
      editDialogVisible.value = true
    }

    // 更新API密钥
    const updateApiKey = async () => {
      if (!editingKey.api_key) {
        message.warning('请输入API密钥', 3)
        return
      }
      
      updating.value = true
      try {
        await request.put(`/api/apikeys/${editingKey.id}`, {
          api_key: editingKey.api_key,
          base_url: editingKey.base_url
        })
        message.success('API密钥更新成功', 2)
        editDialogVisible.value = false
        loadApiKeys()
      } catch (error) {
        message.error('更新失败: ' + error.response?.data?.error || error.message, 1)
      } finally {
        updating.value = false
      }
    }

    // 切换API密钥状态
    const toggleApiKey = async (keyId) => {
      const key = apiKeys.value.find(k => k.id === keyId)
      try {
        await request.post(`/api/apikeys/${keyId}/toggle`)
        message.success(`API密钥已${key && key.is_active ? '禁用' : '启用'}`, 2)
        loadApiKeys()
      } catch (error) {
        message.error('操作失败: ' + (error.response?.data?.error || error.message), 1)
      }
    }

    // 删除API密钥
    const deleteApiKey = async (keyId) => {
      // 找到对应的key对象用于显示模型名称
      const key = apiKeys.value.find(k => k.id === keyId)
      Modal.confirm({
        title: '确认删除',
        content: `确定要删除 ${key ? getModelDisplayName(key.model_provider) : 'ID:' + keyId} 的API密钥吗？`,
        okText: '确定',
        cancelText: '取消',
        onOk: async () => {
          try {
            await request.delete(`/api/apikeys/${keyId}`)
            message.success('API密钥删除成功', 2)
            loadApiKeys()
          } catch (error) {
            message.error('删除失败: ' + error.response?.data?.error || error.message, 1)
          }
        }
      })
    }

    // 获取模型显示名称
    const getModelDisplayName = (modelName) => {
      const modelMap = {
        'gpt-4': 'GPT-4',
        'gpt-3.5-turbo': 'GPT-3.5 Turbo',
        'claude-3': 'Claude-3',
        'gemini-pro': 'Gemini Pro',
        'ernie-bot': '文心一言',
        'qwen': '通义千问',
        'chatglm': '智谱GLM',
        'llama-2': 'Llama 2',
        'siliconflow': '硅基流动'
      }
      return modelMap[modelName] || modelName
    }

    // 获取模型标签颜色
    const getModelTagColor = (modelName) => {
      const colorMap = {
        'gpt-4': 'green',
        'gpt-3.5-turbo': 'blue',
        'claude-3': 'orange',
        'gemini-pro': 'cyan',
        'ernie-bot': 'red',
        'qwen': 'green',
        'chatglm': 'blue',
        'llama-2': 'orange',
        'siliconflow': 'green'
      }
      return colorMap[modelName] || 'default'
    }

    // 格式化日期
    const formatDate = (dateString) => {
      return new Date(dateString).toLocaleString('zh-CN')
    }

    // 表格列配置
    const columns = [
      {
        title: '模型平台',
        dataIndex: 'model_provider',
        width: 120,
        fixed: 'left'
      },
      {
        title: 'API 密钥',
        dataIndex: 'api_key',
        width: 200
      },
      {
        title: 'Base URL',
        dataIndex: 'base_url',
        width: 250
      },
      {
        title: '状态',
        dataIndex: 'is_active',
        width: 100
      },
      {
        title: '更新时间',
        dataIndex: 'updated_at',
        width: 180
      },
      {
        title: '操作',
        dataIndex: 'action',
        width: 200,
        fixed: 'right'
      }
    ]

    onMounted(() => {
      loadApiKeys()
    })

    return {
      loading,
      saving,
      updating,
      editDialogVisible,
      apiKeys,
      newKey,
      editingKey,
      rules,
      columns,
      loadApiKeys,
      saveApiKey,
      resetForm,
      editApiKey,
      updateApiKey,
      toggleApiKey,
      deleteApiKey,
      getModelDisplayName,
      getModelTagColor,
      formatDate,
      maskApiKey,
      maskBaseUrl
    }
  }
}
</script>

<style scoped>
.api-keys-container {
  min-height: 100vh;
  padding: 10px 24px 40px;
  max-width: 1200px;
  margin: 0 auto;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  position: relative;
  overflow-y: auto;
}

.api-keys-container::before {
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

.header {
  text-align: center;
  margin-bottom: 15px;
  position: relative;
  z-index: 1;
}

.header h1 {
  font-size: 2rem;
  font-weight: 800;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 8px;
  position: relative;
}

.header h1::after {
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

.header p {
  color: #4a5568;
  font-size: 1.1rem;
  font-weight: 500;
  max-width: 600px;
  margin: 0 auto;
  line-height: 1.6;
}

:deep(.add-key-card) {
  margin-bottom: 24px;
  border-radius: 20px;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
  z-index: 1;
}

:deep(.add-key-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

:deep(.add-key-card .ant-card-head) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
  padding: 16px 24px;
}

:deep(.add-key-card .ant-card-head-title) {
  font-size: 1.3rem;
  font-weight: 700;
  color: #2d3748;
}

:deep(.add-key-card .ant-card-body) {
  padding: 20px;
  background: white;
}

:deep(.keys-list-card) {
  margin-bottom: 32px;
  border-radius: 20px;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
  z-index: 1;
}

:deep(.keys-list-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

:deep(.keys-list-card .ant-card-head) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
  padding: 24px 32px;
}

:deep(.keys-list-card .ant-card-head-title) {
  font-size: 1.3rem;
  font-weight: 700;
  color: #2d3748;
}

:deep(.keys-list-card .ant-card-body) {
  padding: 32px;
  background: white;
}

/* 表单样式 */
:deep(.ant-form-item-label > label) {
  font-weight: 600;
  color: #2d3748;
  font-size: 1rem;
}

:deep(.ant-input) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  padding: 12px 16px;
  font-size: 1rem;
  transition: all 0.3s ease;
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
}

:deep(.ant-input-password .ant-input) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  padding: 12px 16px;
  font-size: 1rem;
  transition: all 0.3s ease;
}

:deep(.ant-input-password:hover .ant-input) {
  border-color: rgba(102, 126, 234, 0.3);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.1);
}

:deep(.ant-input-password-focused .ant-input) {
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

:deep(.ant-select) {
  border-radius: 12px;
}

:deep(.ant-select-single .ant-select-selector) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  height: 60px;
  padding: 0 12px;
  display: flex;
  align-items: center;
  transition: all 0.3s ease;
}

:deep(.ant-select:hover .ant-select-selector) {
  border-color: rgba(102, 126, 234, 0.3);
  box-shadow: 0 4px 12px rgba(102, 126, 234, 0.1);
}

:deep(.ant-select-focused .ant-select-selector) {
  border-color: #667eea;
  box-shadow: 0 0 0 3px rgba(102, 126, 234, 0.1);
}

/* 按钮样式 */
:deep(.ant-btn-primary) {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  border-radius: 12px;
  font-weight: 600;
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
  transition: all 0.3s ease;
}

:deep(.ant-btn-primary:hover) {
  transform: translateY(-2px);
  box-shadow: 0 12px 35px rgba(102, 126, 234, 0.4);
}

:deep(.ant-btn-default) {
  border-radius: 12px;
  border: 2px solid rgba(102, 126, 234, 0.1);
  font-weight: 600;
  transition: all 0.3s ease;
}

:deep(.ant-btn-default:hover) {
  border-color: #667eea;
  color: #667eea;
  transform: translateY(-1px);
  box-shadow: 0 4px 15px rgba(102, 126, 234, 0.2);
}

/* 表格样式 */
:deep(.ant-table) {
  border-radius: 16px;
  overflow: hidden;
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.06);
}

/* 表格水平滚动样式 */
:deep(.ant-table-body) {
  overflow-x: auto;
  overflow-y: hidden;
}

:deep(.ant-table-body::-webkit-scrollbar) {
  height: 8px;
}

:deep(.ant-table-body::-webkit-scrollbar-track) {
  background: #f1f1f1;
  border-radius: 4px;
}

:deep(.ant-table-body::-webkit-scrollbar-thumb) {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 4px;
}

:deep(.ant-table-body::-webkit-scrollbar-thumb:hover) {
  background: linear-gradient(135deg, #5a6fd8 0%, #6a4190 100%);
}

/* 固定列样式 */
:deep(.ant-table-cell-fix-left),
:deep(.ant-table-cell-fix-right) {
  background: rgba(255, 255, 255, 0.95);
  backdrop-filter: blur(10px);
}

:deep(.ant-table-cell-fix-left-last::after) {
  position: absolute;
  top: 0;
  right: 0;
  bottom: -1px;
  width: 30px;
  transform: translateX(100%);
  transition: box-shadow 0.3s;
  content: '';
  pointer-events: none;
  box-shadow: inset 10px 0 8px -8px rgba(0, 0, 0, 0.15);
}

:deep(.ant-table-cell-fix-right-first::after) {
  position: absolute;
  top: 0;
  left: 0;
  bottom: -1px;
  width: 30px;
  transform: translateX(-100%);
  transition: box-shadow 0.3s;
  content: '';
  pointer-events: none;
  box-shadow: inset -10px 0 8px -8px rgba(0, 0, 0, 0.15);
}

:deep(.ant-table-thead > tr > th) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  color: #2d3748;
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
  font-weight: 700;
  font-size: 1rem;
  padding: 20px 16px;
}

:deep(.ant-table-tbody > tr > td) {
  padding: 20px 16px;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
  font-weight: 500;
  color: #2d3748;
}

:deep(.ant-table-tbody > tr:hover > td) {
  background: rgba(102, 126, 234, 0.05);
}

/* 标签样式 */
:deep(.ant-tag) {
  border-radius: 12px;
  font-weight: 600;
  padding: 4px 12px;
  border: none;
  font-size: 0.85rem;
  height: 28px;
  line-height: 20px;
  display: inline-flex;
  align-items: center;
  justify-content: center;
  min-width: 60px;
}

/* 开关样式 */
:deep(.ant-switch) {
  background: #d1d5db;
}

:deep(.ant-switch-checked) {
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

/* 模态框样式 */
:deep(.ant-modal) {
  border-radius: 20px;
  overflow: hidden;
}

:deep(.ant-modal-header) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
  padding: 24px 32px;
}

:deep(.ant-modal-title) {
  font-size: 1.3rem;
  font-weight: 700;
  color: #2d3748;
}

:deep(.ant-modal-body) {
  padding: 32px;
}

:deep(.ant-modal-footer) {
  padding: 20px 32px;
  border-top: 1px solid rgba(102, 126, 234, 0.1);
}

.api-key-display {
  font-family: 'SF Mono', 'Monaco', 'Inconsolata', 'Roboto Mono', monospace;
  background: linear-gradient(135deg, rgba(102, 126, 234, 0.05) 0%, rgba(78, 205, 196, 0.05) 100%);
  padding: 8px 12px;
  border-radius: 8px;
  font-size: 0.85rem;
  font-weight: 600;
  color: #4a5568;
  border: 1px solid rgba(102, 126, 234, 0.1);
  letter-spacing: 0.5px;
}

.text-muted {
  color: #a0aec0;
  font-style: italic;
  font-weight: 500;
}

/* 空状态样式 */
:deep(.ant-empty) {
  padding: 60px 20px;
}

:deep(.ant-empty-image) {
  opacity: 0.6;
}

:deep(.ant-empty-description) {
  color: #718096;
  font-weight: 500;
  font-size: 1rem;
}

/* 加载状态 */
:deep(.ant-spin-container) {
  min-height: 200px;
}

/* 响应式设计 */
@media (max-width: 768px) {
  .api-keys-container {
    padding: 8px 16px 20px;
  }
  
  .header {
    margin-bottom: 10px;
  }
  
  .header h1 {
    font-size: 1.8rem;
    margin-bottom: 4px;
  }
  
  .header p {
    font-size: 0.9rem;
  }
  
  :deep(.add-key-card .ant-card-head),
  :deep(.keys-list-card .ant-card-head) {
    padding: 12px 16px;
  }
  
  :deep(.add-key-card .ant-card-body),
  :deep(.keys-list-card .ant-card-body) {
    padding: 16px;
  }
  
  :deep(.ant-modal-header) {
    padding: 16px 20px;
  }
  
  :deep(.ant-modal-body) {
    padding: 20px;
  }
  
  :deep(.ant-modal-footer) {
    padding: 16px 20px;
  }
}

@media (max-width: 480px) {
  .header h1 {
    font-size: 2rem;
  }
  
  .api-key-display {
    font-size: 0.75rem;
    padding: 6px 10px;
  }
  
  :deep(.ant-table-tbody > tr > td),
  :deep(.ant-table-thead > tr > th) {
    padding: 12px 8px;
    font-size: 0.9rem;
  }
  
  :deep(.ant-btn) {
    font-size: 0.9rem;
    padding: 6px 12px;
  }
}

/* 动画效果 */
:deep(.add-key-card),
:deep(.keys-list-card) {
  animation: slideUp 0.6s ease-out;
}

/* 悬停效果 */
:deep(.add-key-card:hover),
:deep(.keys-list-card:hover) {
  transform: translateY(-5px);
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.12);
  transition: all 0.3s ease;
}
</style>