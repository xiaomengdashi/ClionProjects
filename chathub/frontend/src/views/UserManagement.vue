<template>
  <div class="user-management">
    <div class="header">
      <h1>用户管理</h1>
      <a-button type="primary" @click="showCreateModal">
        <PlusOutlined />
        添加用户
      </a-button>
    </div>

    <!-- 搜索和筛选 -->
    <div class="filters">
      <a-input-search
        v-model:value="searchText"
        placeholder="搜索用户名或邮箱"
        style="width: 300px"
        @search="loadUsers"
        @pressEnter="loadUsers"
      />
      <a-select
        v-model:value="roleFilter"
        placeholder="筛选角色"
        style="width: 120px; margin-left: 16px"
        @change="loadUsers"
      >
        <a-select-option value="">全部</a-select-option>
        <a-select-option value="admin">管理员</a-select-option>
        <a-select-option value="user">普通用户</a-select-option>
      </a-select>
    </div>

    <!-- 用户列表 -->
    <a-table
      :columns="columns"
      :data-source="users"
      :loading="loading"
      :pagination="pagination"
      @change="handleTableChange"
      row-key="id"
    >
      <template #bodyCell="{ column, record }">
        <template v-if="column.key === 'role'">
          <a-tag :color="record.role === 'admin' ? 'red' : 'blue'">
            {{ record.role === 'admin' ? '管理员' : '普通用户' }}
          </a-tag>
        </template>
        <template v-else-if="column.key === 'is_active'">
          <a-tag :color="record.is_active ? 'green' : 'red'">
            {{ record.is_active ? '活跃' : '禁用' }}
          </a-tag>
        </template>
        <template v-else-if="column.key === 'subscription_type'">
          <a-tag :color="getSubscriptionColor(record.subscription_type)">
            {{ getSubscriptionText(record.subscription_type) }}
          </a-tag>
        </template>
        <template v-else-if="column.key === 'action'">
          <a-space>
            <a-button size="small" @click="editUser(record)">编辑</a-button>
            <a-button size="small" @click="resetPassword(record)">重置密码</a-button>
            <a-popconfirm
              title="确定要删除这个用户吗？"
              @confirm="deleteUser(record.id)"
            >
              <a-button size="small" danger :disabled="record.id === currentUserId">删除</a-button>
            </a-popconfirm>
          </a-space>
        </template>
      </template>
    </a-table>

    <!-- 创建/编辑用户模态框 -->
    <a-modal
      v-model:open="modalVisible"
      :title="isEditing ? '编辑用户' : '创建用户'"
      @ok="handleSubmit"
      @cancel="resetForm"
      :confirm-loading="submitting"
    >
      <a-form
        ref="formRef"
        :model="form"
        :rules="rules"
        layout="vertical"
      >
        <a-form-item label="用户名" name="username">
          <a-input v-model:value="form.username" :disabled="isEditing" />
        </a-form-item>
        <a-form-item label="邮箱" name="email">
          <a-input v-model:value="form.email" />
        </a-form-item>
        <a-form-item label="角色" name="role">
          <a-select v-model:value="form.role">
            <a-select-option value="admin">管理员</a-select-option>
            <a-select-option value="user">普通用户</a-select-option>
          </a-select>
        </a-form-item>
        <a-form-item label="订阅类型" name="subscription_type">
          <a-select v-model:value="form.subscription_type">
            <a-select-option value="free">免费</a-select-option>
            <a-select-option value="premium">高级</a-select-option>
            <a-select-option value="enterprise">企业</a-select-option>
          </a-select>
        </a-form-item>
        <a-form-item label="使用限制" name="usage_limit">
          <a-input-number v-model:value="form.usage_limit" :min="0" style="width: 100%" />
        </a-form-item>
        <a-form-item label="状态" name="is_active">
          <a-switch v-model:checked="form.is_active" />
          <span style="margin-left: 8px">{{ form.is_active ? '活跃' : '禁用' }}</span>
        </a-form-item>
        <a-form-item v-if="!isEditing" label="密码" name="password">
          <a-input-password v-model:value="form.password" />
        </a-form-item>
        <a-form-item v-if="isEditing" label="新密码" name="password">
          <a-input-password v-model:value="form.password" placeholder="留空则不修改密码" />
        </a-form-item>
      </a-form>
    </a-modal>
  </div>
</template>

<script>
import { ref, reactive, onMounted, computed } from 'vue'
import { message } from 'ant-design-vue'
import { PlusOutlined } from '@ant-design/icons-vue'
import axios from 'axios'

export default {
  name: 'UserManagement',
  components: {
    PlusOutlined
  },
  setup() {
    const users = ref([])
    const loading = ref(false)
    const modalVisible = ref(false)
    const isEditing = ref(false)
    const submitting = ref(false)
    const searchText = ref('')
    const roleFilter = ref('')
    const formRef = ref()
    const currentUserId = ref(null)

    const pagination = reactive({
      current: 1,
      pageSize: 10,
      total: 0,
      showSizeChanger: true,
      showQuickJumper: true,
      showTotal: (total, range) => `第 ${range[0]}-${range[1]} 条，共 ${total} 条`
    })

    const form = reactive({
      id: null,
      username: '',
      email: '',
      role: 'user',
      subscription_type: 'free',
      usage_limit: 1000,
      is_active: true,
      password: ''
    })

    const rules = {
      username: [
        { required: true, message: '请输入用户名' },
        { min: 3, max: 50, message: '用户名长度应在3-50个字符之间' }
      ],
      email: [
        { required: true, message: '请输入邮箱' },
        { type: 'email', message: '请输入有效的邮箱地址' }
      ],
      role: [{ required: true, message: '请选择角色' }],
      subscription_type: [{ required: true, message: '请选择订阅类型' }],
      usage_limit: [{ required: true, message: '请输入使用限制' }],
      password: [
        { 
          validator: (rule, value) => {
            if (!isEditing.value && !value) {
              return Promise.reject('请输入密码')
            }
            if (value && value.length < 6) {
              return Promise.reject('密码长度至少6个字符')
            }
            return Promise.resolve()
          }
        }
      ]
    }

    const columns = [
      {
        title: 'ID',
        dataIndex: 'id',
        key: 'id',
        width: 80
      },
      {
        title: '用户名',
        dataIndex: 'username',
        key: 'username'
      },
      {
        title: '邮箱',
        dataIndex: 'email',
        key: 'email'
      },
      {
        title: '角色',
        dataIndex: 'role',
        key: 'role'
      },
      {
        title: '订阅类型',
        dataIndex: 'subscription_type',
        key: 'subscription_type'
      },
      {
        title: '使用限制',
        dataIndex: 'usage_limit',
        key: 'usage_limit'
      },
      {
        title: '状态',
        dataIndex: 'is_active',
        key: 'is_active'
      },
      {
        title: '创建时间',
        dataIndex: 'created_at',
        key: 'created_at',
        customRender: ({ text }) => new Date(text).toLocaleString()
      },
      {
        title: '操作',
        key: 'action',
        width: 200
      }
    ]

    // 获取当前用户信息
    const getCurrentUser = async () => {
      try {
        const token = localStorage.getItem('token')
        const response = await axios.get('/api/auth/verify', {
          headers: { Authorization: `Bearer ${token}` }
        })
        currentUserId.value = response.data.user.id
      } catch (error) {
        console.error('获取当前用户信息失败:', error)
      }
    }

    // 加载用户列表
    const loadUsers = async () => {
      loading.value = true
      try {
        const token = localStorage.getItem('token')
        const params = {
          page: pagination.current,
          per_page: pagination.pageSize
        }
        
        if (searchText.value) {
          params.search = searchText.value
        }
        if (roleFilter.value) {
          params.role = roleFilter.value
        }

        const response = await axios.get('/api/users', {
          headers: { Authorization: `Bearer ${token}` },
          params
        })

        users.value = response.data.users
        pagination.total = response.data.total
      } catch (error) {
        message.error('加载用户列表失败')
        console.error('加载用户列表失败:', error)
      } finally {
        loading.value = false
      }
    }

    // 显示创建模态框
    const showCreateModal = () => {
      isEditing.value = false
      modalVisible.value = true
      resetForm()
    }

    // 编辑用户
    const editUser = (user) => {
      isEditing.value = true
      modalVisible.value = true
      Object.assign(form, {
        id: user.id,
        username: user.username,
        email: user.email,
        role: user.role,
        subscription_type: user.subscription_type,
        usage_limit: user.usage_limit,
        is_active: user.is_active,
        password: ''
      })
    }

    // 重置表单
    const resetForm = () => {
      Object.assign(form, {
        id: null,
        username: '',
        email: '',
        role: 'user',
        subscription_type: 'free',
        usage_limit: 1000,
        is_active: true,
        password: ''
      })
      if (formRef.value) {
        formRef.value.resetFields()
      }
    }

    // 提交表单
    const handleSubmit = async () => {
      try {
        await formRef.value.validate()
        submitting.value = true

        const token = localStorage.getItem('token')
        const data = { ...form }
        
        // 如果是编辑且密码为空，则不发送密码字段
        if (isEditing.value && !data.password) {
          delete data.password
        }

        if (isEditing.value) {
          await axios.put(`/api/users/${form.id}`, data, {
            headers: { Authorization: `Bearer ${token}` }
          })
          message.success('用户更新成功')
        } else {
          await axios.post('/api/auth/admin/register', data, {
            headers: { Authorization: `Bearer ${token}` }
          })
          message.success('用户创建成功')
        }

        modalVisible.value = false
        loadUsers()
      } catch (error) {
        if (error.response?.data?.message) {
          message.error(error.response.data.message)
        } else {
          message.error(isEditing.value ? '更新用户失败' : '创建用户失败')
        }
        console.error('提交表单失败:', error)
      } finally {
        submitting.value = false
      }
    }

    // 删除用户
    const deleteUser = async (userId) => {
      try {
        const token = localStorage.getItem('token')
        await axios.delete(`/api/users/${userId}`, {
          headers: { Authorization: `Bearer ${token}` }
        })
        message.success('用户删除成功')
        loadUsers()
      } catch (error) {
        message.error('删除用户失败')
        console.error('删除用户失败:', error)
      }
    }

    // 重置密码
    const resetPassword = async (user) => {
      try {
        const token = localStorage.getItem('token')
        const response = await axios.post(`/api/users/${user.id}/reset-password`, {}, {
          headers: { Authorization: `Bearer ${token}` }
        })
        message.success(`密码重置成功，新密码：${response.data.new_password}`)
      } catch (error) {
        message.error('重置密码失败')
        console.error('重置密码失败:', error)
      }
    }

    // 表格变化处理
    const handleTableChange = (pag) => {
      pagination.current = pag.current
      pagination.pageSize = pag.pageSize
      loadUsers()
    }

    // 获取订阅类型颜色
    const getSubscriptionColor = (type) => {
      const colors = {
        free: 'default',
        premium: 'gold',
        enterprise: 'purple'
      }
      return colors[type] || 'default'
    }

    // 获取订阅类型文本
    const getSubscriptionText = (type) => {
      const texts = {
        free: '免费',
        premium: '高级',
        enterprise: '企业'
      }
      return texts[type] || type
    }

    onMounted(() => {
      getCurrentUser()
      loadUsers()
    })

    return {
      users,
      loading,
      modalVisible,
      isEditing,
      submitting,
      searchText,
      roleFilter,
      formRef,
      currentUserId,
      pagination,
      form,
      rules,
      columns,
      loadUsers,
      showCreateModal,
      editUser,
      resetForm,
      handleSubmit,
      deleteUser,
      resetPassword,
      handleTableChange,
      getSubscriptionColor,
      getSubscriptionText
    }
  }
}
</script>

<style scoped>
.user-management {
  padding: 24px;
}

.header {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 24px;
}

.header h1 {
  margin: 0;
  font-size: 24px;
  font-weight: 600;
}

.filters {
  margin-bottom: 16px;
  display: flex;
  align-items: center;
}
</style>