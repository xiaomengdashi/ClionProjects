<template>
  <div class="models-container">
    <div class="header">
      <h1>大模型管理</h1>
      <p>管理AI大模型配置，包括模型提供商、模型名称和相关参数</p>
    </div>

    <!-- 添加新模型 -->
    <a-card class="add-model-card" title="添加新的大模型">
      <a-form :model="newModel" :rules="rules" ref="modelForm" layout="vertical">
        <a-row :gutter="20">
          <a-col :span="6">
            <a-form-item label="模型名称" name="model_name">
              <a-input 
                v-model:value="newModel.model_name" 
                placeholder="如: gpt-4, claude-3-sonnet"
              />
            </a-form-item>
          </a-col>
          <a-col :span="6">
            <a-form-item label="显示名称" name="display_name">
              <a-input 
                v-model:value="newModel.display_name" 
                placeholder="如: GPT-4, Claude-3 Sonnet"
              />
            </a-form-item>
          </a-col>
          <a-col :span="6">
            <a-form-item label="模型提供商" name="model_provider">
              <a-select v-model:value="newModel.model_provider" placeholder="选择提供商" style="width: 100%">
                <a-select-option value="openai">OpenAI</a-select-option>
                <a-select-option value="anthropic">Anthropic</a-select-option>
                <a-select-option value="google">Google</a-select-option>
                <a-select-option value="baidu">百度</a-select-option>
                <a-select-option value="alibaba">阿里巴巴</a-select-option>
                <a-select-option value="zhipu">智谱AI</a-select-option>
                <a-select-option value="meta">Meta</a-select-option>
                <a-select-option value="siliconflow">硅基流动</a-select-option>
                <a-select-option value="deepseek">DeepSeek</a-select-option>
                <a-select-option value="moonshot">月之暗面</a-select-option>
                <a-select-option value="other">其他</a-select-option>
              </a-select>
            </a-form-item>
          </a-col>

        </a-row>
        
        <a-row :gutter="20">
          <a-col :span="6">
            <a-form-item label="模型类型">
              <a-select v-model:value="newModel.model_type" style="width: 100%">
                <a-select-option value="chat">对话模型</a-select-option>
                <a-select-option value="completion">补全模型</a-select-option>
                <a-select-option value="embedding">嵌入模型</a-select-option>
                <a-select-option value="image">图像模型</a-select-option>
              </a-select>
            </a-form-item>
          </a-col>
          <a-col :span="6">
            <a-form-item label="最大Token数">
              <a-input-number 
                v-model:value="newModel.max_tokens" 
                :min="1" 
                :max="1000000"
                style="width: 100%"
              />
            </a-form-item>
          </a-col>
          <a-col :span="6">
            <a-form-item label="输入价格(每1k tokens)">
              <a-input-number 
                v-model:value="newModel.input_price_per_1k" 
                :min="0" 
                :step="0.001"
                :precision="4"
                style="width: 100%"
                placeholder="0.0000"
              >
                <template #addonBefore>$</template>
                <template #addonAfter>/1k</template>
              </a-input-number>
            </a-form-item>
          </a-col>
          <a-col :span="6">
            <a-form-item label="输出价格(每1k tokens)">
              <a-input-number 
                v-model:value="newModel.output_price_per_1k" 
                :min="0" 
                :step="0.001"
                :precision="4"
                style="width: 100%"
                placeholder="0.0000"
              >
                <template #addonBefore>$</template>
                <template #addonAfter>/1k</template>
              </a-input-number>
            </a-form-item>
          </a-col>
        </a-row>
        
        <a-row :gutter="20">
          <a-col :span="8">
            <a-form-item label="功能特性">
              <a-space direction="vertical">
                <a-checkbox v-model:checked="newModel.supports_streaming">支持流式输出</a-checkbox>
                <a-checkbox v-model:checked="newModel.supports_function_calling">支持函数调用</a-checkbox>
                <a-checkbox v-model:checked="newModel.supports_vision">支持视觉输入</a-checkbox>
              </a-space>
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="排序顺序">
              <a-input-number 
                v-model:value="newModel.sort_order" 
                :min="0" 
                style="width: 100%"
              />
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="状态">
              <a-switch 
                v-model:checked="newModel.is_active" 
                checked-children="启用" 
                un-checked-children="禁用"
              />
            </a-form-item>
          </a-col>
        </a-row>
        
        <a-form-item label="模型描述">
          <a-textarea 
            v-model:value="newModel.description" 
            placeholder="描述模型的特点和用途"
            :rows="3"
          />
        </a-form-item>
        
        <a-form-item>
          <a-button type="primary" @click="saveModel" :loading="saving">
            <template #icon><PlusOutlined /></template>
            保存模型
          </a-button>
          <a-button @click="resetForm" style="margin-left: 8px;">重置</a-button>
        </a-form-item>
      </a-form>
    </a-card>

    <!-- 模型列表 -->
    <a-card class="models-list-card">
      <template #title>
        <span>已配置的大模型</span>
      </template>
      <template #extra>
        <a-space>
          <a-select 
            v-model:value="filterProvider" 
            placeholder="筛选提供商" 
            style="width: 150px"
            allow-clear
            @change="loadModels"
          >
            <a-select-option value="">全部提供商</a-select-option>
            <a-select-option 
              v-for="provider in providers" 
              :key="provider.provider" 
              :value="provider.provider"
            >
              {{ provider.display_name }}
            </a-select-option>
          </a-select>
          <span style="color: #666;">
            数据条数: {{ models.length }}
          </span>
          <a-button type="primary" size="small" @click="loadModels">
            <template #icon><ReloadOutlined /></template>
            刷新
          </a-button>
        </a-space>
      </template>

      <a-table 
        :data-source="models" 
        :loading="loading" 
        :columns="columns"
        row-key="id"
        :scroll="{ x: 1500 }"
      >
        <template #bodyCell="{ column, record }">
          <template v-if="column.dataIndex === 'model_info'">
            <div>
              <div style="font-weight: 600; color: #1890ff;">{{ record.display_name }}</div>
              <div style="font-size: 12px; color: #666;">{{ record.model_name }}</div>
            </div>
          </template>
          
          <template v-else-if="column.dataIndex === 'provider_info'">
            <a-tag :color="getProviderTagColor(record.model_provider)">
              {{ getProviderDisplayName(record.model_provider) }}
            </a-tag>
          </template>
          
          <template v-else-if="column.dataIndex === 'model_type'">
            <a-tag :color="getTypeTagColor(record.model_type)">
              {{ getTypeDisplayName(record.model_type) }}
            </a-tag>
          </template>
          
          <template v-else-if="column.dataIndex === 'features'">
            <a-space direction="vertical" size="small">
              <a-tag v-if="record.supports_streaming" color="green" size="small">流式</a-tag>
              <a-tag v-if="record.supports_function_calling" color="blue" size="small">函数</a-tag>
              <a-tag v-if="record.supports_vision" color="orange" size="small">视觉</a-tag>
            </a-space>
          </template>
          
          <template v-else-if="column.dataIndex === 'pricing'">
            <div style="font-size: 12px; line-height: 1.4;">
              <div style="color: #52c41a; font-weight: 500;">
                <span style="color: #666;">输入:</span> ${{ (record.input_price_per_1k || 0).toFixed(4) }}/1k
              </div>
              <div style="color: #fa8c16; font-weight: 500; margin-top: 2px;">
                <span style="color: #666;">输出:</span> ${{ (record.output_price_per_1k || 0).toFixed(4) }}/1k
              </div>
              <div style="color: #1890ff; font-weight: 500; margin-top: 2px; font-size: 11px;">
                <span style="color: #666;">总计:</span> ${{ ((record.input_price_per_1k || 0) + (record.output_price_per_1k || 0)).toFixed(4) }}/1k
              </div>
            </div>
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
                @click="toggleModel(record.id)"
              >
                {{ record.is_active ? '禁用' : '启用' }}
              </a-button>
              <a-button size="small" @click="editModel(record)">编辑</a-button>
              <a-button size="small" danger @click="deleteModel(record.id)">删除</a-button>
            </a-space>
          </template>
        </template>
      </a-table>
    </a-card>

    <!-- 编辑对话框 -->
    <a-modal v-model:open="editDialogVisible" title="编辑大模型" width="800px">
      <a-form :model="editingModel" layout="vertical">
        <a-row :gutter="16">
          <a-col :span="12">
            <a-form-item label="模型名称">
              <a-input v-model:value="editingModel.model_name" />
            </a-form-item>
          </a-col>
          <a-col :span="12">
            <a-form-item label="显示名称">
              <a-input v-model:value="editingModel.display_name" />
            </a-form-item>
          </a-col>
        </a-row>
        
        <a-row :gutter="16">
          <a-col :span="12">
            <a-form-item label="模型提供商">
              <a-select v-model:value="editingModel.model_provider" style="width: 100%">
                <a-select-option value="openai">OpenAI</a-select-option>
                <a-select-option value="anthropic">Anthropic</a-select-option>
                <a-select-option value="google">Google</a-select-option>
                <a-select-option value="baidu">百度</a-select-option>
                <a-select-option value="alibaba">阿里巴巴</a-select-option>
                <a-select-option value="zhipu">智谱AI</a-select-option>
                <a-select-option value="meta">Meta</a-select-option>
                <a-select-option value="siliconflow">硅基流动</a-select-option>
                <a-select-option value="deepseek">DeepSeek</a-select-option>
                <a-select-option value="moonshot">月之暗面</a-select-option>
                <a-select-option value="other">其他</a-select-option>
              </a-select>
            </a-form-item>
          </a-col>

        </a-row>
        
        <a-row :gutter="16">
          <a-col :span="8">
            <a-form-item label="模型类型">
              <a-select v-model:value="editingModel.model_type" style="width: 100%">
                <a-select-option value="chat">对话模型</a-select-option>
                <a-select-option value="completion">补全模型</a-select-option>
                <a-select-option value="embedding">嵌入模型</a-select-option>
                <a-select-option value="image">图像模型</a-select-option>
              </a-select>
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="最大Token数">
              <a-input-number 
                v-model:value="editingModel.max_tokens" 
                :min="1" 
                style="width: 100%"
              />
            </a-form-item>
          </a-col>
          <a-col :span="8">
            <a-form-item label="排序顺序">
              <a-input-number 
                v-model:value="editingModel.sort_order" 
                :min="0" 
                style="width: 100%"
              />
            </a-form-item>
          </a-col>
        </a-row>
        
        <a-row :gutter="16">
          <a-col :span="12">
            <a-form-item label="输入价格(每1k tokens)">
              <a-input-number 
                v-model:value="editingModel.input_price_per_1k" 
                :min="0" 
                :step="0.001"
                :precision="4"
                style="width: 100%"
                placeholder="0.0000"
              >
                <template #addonBefore>$</template>
                <template #addonAfter>/1k</template>
              </a-input-number>
            </a-form-item>
          </a-col>
          <a-col :span="12">
            <a-form-item label="输出价格(每1k tokens)">
              <a-input-number 
                v-model:value="editingModel.output_price_per_1k" 
                :min="0" 
                :step="0.001"
                :precision="4"
                style="width: 100%"
                placeholder="0.0000"
              >
                <template #addonBefore>$</template>
                <template #addonAfter>/1k</template>
              </a-input-number>
            </a-form-item>
          </a-col>
        </a-row>
        
        <a-form-item label="功能特性">
          <a-space>
            <a-checkbox v-model:checked="editingModel.supports_streaming">支持流式输出</a-checkbox>
            <a-checkbox v-model:checked="editingModel.supports_function_calling">支持函数调用</a-checkbox>
            <a-checkbox v-model:checked="editingModel.supports_vision">支持视觉输入</a-checkbox>
          </a-space>
        </a-form-item>
        
        <a-form-item label="模型描述">
          <a-textarea 
            v-model:value="editingModel.description" 
            :rows="3"
          />
        </a-form-item>
      </a-form>
      <template #footer>
        <a-button @click="editDialogVisible = false">取消</a-button>
        <a-button type="primary" @click="updateModel" :loading="updating">
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
  name: 'Models',
  components: {
    PlusOutlined,
    ReloadOutlined
  },
  setup() {
    const loading = ref(false)
    const saving = ref(false)
    const updating = ref(false)
    const editDialogVisible = ref(false)
    const filterProvider = ref('')
    
    const models = ref([])
    const providers = ref([])
    
    const newModel = reactive({
      model_name: '',
      display_name: '',
      model_provider: '',
      model_type: 'chat',
      max_tokens: 4096,
      supports_streaming: true,
      supports_function_calling: false,
      supports_vision: false,
      input_price_per_1k: 0.0,
      output_price_per_1k: 0.0,
      description: '',
      is_active: true,
      sort_order: 0
    })
    
    const editingModel = reactive({
      id: null,
      model_name: '',
      display_name: '',
      model_provider: '',
      model_type: 'chat',
      max_tokens: 4096,
      supports_streaming: true,
      supports_function_calling: false,
      supports_vision: false,
      input_price_per_1k: 0.0,
      output_price_per_1k: 0.0,
      description: '',
      is_active: true,
      sort_order: 0
    })
    
    const rules = {
      model_name: [
        { required: true, message: '请输入模型名称', trigger: 'blur' }
      ],
      display_name: [
        { required: true, message: '请输入显示名称', trigger: 'blur' }
      ],
      model_provider: [
        { required: true, message: '请选择模型提供商', trigger: 'change' }
      ]
    }

    // 加载模型列表
    const loadModels = async () => {
      loading.value = true
      try {
        const params = {}
        if (filterProvider.value) {
          params.provider = filterProvider.value
        }
        
        const response = await request.get('/api/models', { params })
        models.value = response.data
      } catch (error) {
        console.error('加载模型列表失败:', error)
        message.error('加载模型列表失败: ' + error.message)
      } finally {
        loading.value = false
      }
    }

    // 加载提供商列表
    const loadProviders = async () => {
      try {
        const response = await request.get('/api/models/providers')
        providers.value = response.data
      } catch (error) {
        console.error('加载提供商列表失败:', error)
      }
    }

    // 保存模型
    const saveModel = async () => {
      if (!newModel.model_name || !newModel.display_name || !newModel.model_provider) {
        message.warning('请填写必要信息')
        return
      }
      
      saving.value = true
      try {
        await request.post('/api/models', newModel)
        message.success('大模型创建成功')
        resetForm()
        loadModels()
        loadProviders()
      } catch (error) {
        message.error('保存失败: ' + (error.response?.data?.error || error.message))
      } finally {
        saving.value = false
      }
    }

    // 重置表单
    const resetForm = () => {
      Object.assign(newModel, {
        model_name: '',
        display_name: '',
        model_provider: '',
        model_type: 'chat',
        max_tokens: 4096,
        supports_streaming: true,
        supports_function_calling: false,
        supports_vision: false,
        input_price_per_1k: 0.0,
        output_price_per_1k: 0.0,
        description: '',
        is_active: true,
        sort_order: 0
      })
    }

    // 编辑模型
    const editModel = (model) => {
      Object.assign(editingModel, model)
      editDialogVisible.value = true
    }

    // 更新模型
    const updateModel = async () => {
      if (!editingModel.model_name || !editingModel.display_name) {
        message.warning('请填写必要信息')
        return
      }
      
      updating.value = true
      try {
        await request.put(`/api/models/${editingModel.id}`, editingModel)
        message.success('大模型更新成功')
        editDialogVisible.value = false
        loadModels()
        loadProviders()
      } catch (error) {
        message.error('更新失败: ' + (error.response?.data?.error || error.message))
      } finally {
        updating.value = false
      }
    }

    // 切换模型状态
    const toggleModel = async (modelId) => {
      try {
        await request.post(`/api/models/${modelId}/toggle`)
        const model = models.value.find(m => m.id === modelId)
        message.success(`大模型已${model && model.is_active ? '禁用' : '启用'}`)
        loadModels()
      } catch (error) {
        message.error('操作失败: ' + (error.response?.data?.error || error.message))
      }
    }

    // 删除模型
    const deleteModel = async (modelId) => {
      const model = models.value.find(m => m.id === modelId)
      Modal.confirm({
        title: '确认删除',
        content: `确定要删除 ${model ? model.display_name : 'ID:' + modelId} 吗？`,
        okText: '确定',
        cancelText: '取消',
        onOk: async () => {
          try {
            await request.delete(`/api/models/${modelId}`)
            message.success('大模型删除成功')
            loadModels()
            loadProviders()
          } catch (error) {
            message.error('删除失败: ' + (error.response?.data?.error || error.message))
          }
        }
      })
    }

    // 获取提供商标签颜色
    const getProviderTagColor = (provider) => {
      const colorMap = {
        'openai': 'green',
        'anthropic': 'orange',
        'google': 'blue',
        'baidu': 'red',
        'alibaba': 'cyan',
        'zhipu': 'purple',
        'meta': 'blue',
        'siliconflow': 'green',
        'deepseek': 'geekblue',
        'moonshot': 'magenta'
      }
      return colorMap[provider] || 'default'
    }

    // 获取提供商显示名称
    const getProviderDisplayName = (provider) => {
      const providerMap = {
        'openai': 'OpenAI',
        'anthropic': 'Anthropic',
        'google': 'Google',
        'baidu': '百度',
        'alibaba': '阿里巴巴',
        'zhipu': '智谱AI',
        'meta': 'Meta',
        'siliconflow': '硅基流动',
        'deepseek': 'DeepSeek',
        'moonshot': '月之暗面',
        'other': '其他'
      }
      return providerMap[provider] || provider
    }

    // 获取类型标签颜色
    const getTypeTagColor = (type) => {
      const colorMap = {
        'chat': 'blue',
        'completion': 'green',
        'embedding': 'orange',
        'image': 'purple'
      }
      return colorMap[type] || 'default'
    }

    // 获取类型显示名称
    const getTypeDisplayName = (type) => {
      const typeMap = {
        'chat': '对话',
        'completion': '补全',
        'embedding': '嵌入',
        'image': '图像'
      }
      return typeMap[type] || type
    }

    // 格式化日期
    const formatDate = (dateString) => {
      return new Date(dateString).toLocaleString('zh-CN')
    }

    // 表格列配置
    const columns = [
      {
        title: '模型信息',
        dataIndex: 'model_info',
        width: 150,
        fixed: 'left'
      },
      {
        title: '提供商',
        dataIndex: 'provider_info',
        width: 120
      },
      {
        title: '类型',
        dataIndex: 'model_type',
        width: 80
      },
      {
        title: 'Token上限',
        dataIndex: 'max_tokens',
        width: 100
      },
      {
        title: '功能特性',
        dataIndex: 'features',
        width: 100
      },
      {
        title: '定价信息',
        dataIndex: 'pricing',
        width: 150
      },
      {
        title: '状态',
        dataIndex: 'is_active',
        width: 80
      },
      {
        title: '更新时间',
        dataIndex: 'updated_at',
        width: 150
      },
      {
        title: '操作',
        dataIndex: 'action',
        width: 200,
        fixed: 'right'
      }
    ]

    onMounted(() => {
      loadModels()
      loadProviders()
    })

    return {
      loading,
      saving,
      updating,
      editDialogVisible,
      filterProvider,
      models,
      providers,
      newModel,
      editingModel,
      rules,
      columns,
      loadModels,
      loadProviders,
      saveModel,
      resetForm,
      editModel,
      updateModel,
      toggleModel,
      deleteModel,
      getProviderTagColor,
      getProviderDisplayName,
      getTypeTagColor,
      getTypeDisplayName,
      formatDate
    }
  }
}
</script>

<style scoped>
.models-container {
  min-height: 100vh;
  padding: 10px 24px 40px;
  max-width: 1400px;
  margin: 0 auto;
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  position: relative;
  overflow-y: auto;
}

.models-container::before {
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

:deep(.add-model-card) {
  margin-bottom: 24px;
  border-radius: 20px;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
  z-index: 1;
}

:deep(.add-model-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

:deep(.models-list-card) {
  margin-bottom: 32px;
  border-radius: 20px;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
  z-index: 1;
}

:deep(.models-list-card::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

/* 表单和按钮样式继承自ApiKeys.vue的样式 */
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

:deep(.ant-table) {
  border-radius: 16px;
  overflow: hidden;
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.06);
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

/* 响应式设计 */
@media (max-width: 768px) {
  .models-container {
    padding: 8px 16px 20px;
  }
  
  .header h1 {
    font-size: 1.8rem;
  }
}
</style>