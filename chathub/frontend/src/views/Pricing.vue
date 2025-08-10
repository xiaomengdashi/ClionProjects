<template>
  <div class="pricing">
    <!-- 页面标题 -->
    <div class="pricing-header">
      <h1 class="page-title">选择适合您的方案</h1>
      <p class="page-subtitle">
        灵活的定价方案，满足个人用户到企业客户的不同需求
      </p>
    </div>
      
      <!-- 定价卡片 -->
      <div class="pricing-grid">
        <div 
          class="pricing-card" 
          v-for="plan in pricingPlans" 
          :key="plan.name"
          :class="{ 'featured': plan.featured }"
        >
          <div class="card-header">
            <h3 class="plan-name">{{ plan.name }}</h3>
            <div class="plan-price">
              <span class="currency">¥</span>
              <span class="amount">{{ plan.price }}</span>
              <span class="period">/月</span>
            </div>
            <p class="plan-description">{{ plan.description }}</p>
          </div>
          
          <div class="card-body">
            <ul class="features-list">
              <li v-for="feature in plan.features" :key="feature" class="feature-item">
                <CheckOutlined class="check-icon" />
                {{ feature }}
              </li>
            </ul>
          </div>
          
          <div class="card-footer">
            <a-button 
              :type="plan.featured ? 'primary' : 'default'"
              size="large"
              class="select-plan-btn"
              @click="selectPlan(plan)"
              block
            >
              {{ plan.buttonText }}
            </a-button>
          </div>
          
          <div v-if="plan.featured" class="featured-badge">
            推荐
          </div>
        </div>
      </div>
      
      <!-- 模型定价表 -->
      <div class="model-pricing">
        <h2 class="section-title">模型定价详情</h2>
        <div class="pricing-table">
          <a-table :dataSource="modelPricing" :pagination="false" rowKey="id">
            <a-table-column title="模型名称" dataIndex="name" width="200">
              <template #default="{ record }">
                <div class="model-name">
                  <strong>{{ record.name }}</strong>
                  <span class="provider">{{ record.provider }}</span>
                </div>
              </template>
            </a-table-column>
            <a-table-column title="描述" dataIndex="description" />
            <a-table-column title="定价信息" width="250">
              <template #default="{ record }">
                <div class="pricing-details">
                  <div class="price-row">
                    <span class="price-label">输入:</span>
                    <span class="price-value input-price">${{ (record.input_price || 0).toFixed(4) }}/1k</span>
                  </div>
                  <div class="price-row">
                    <span class="price-label">输出:</span>
                    <span class="price-value output-price">${{ (record.output_price || 0).toFixed(4) }}/1k</span>
                  </div>
                  <div class="price-row total-price">
                    <span class="price-label">每消息约:</span>
                    <span class="price-value">¥{{ record.cost_per_message }}</span>
                  </div>
                </div>
              </template>
            </a-table-column>
            <a-table-column title="适用场景" width="200">
              <template #default="{ record }">
                <a-tag 
                  v-for="tag in record.tags" 
                  :key="tag" 
                  size="small"
                  class="scenario-tag"
                >
                  {{ tag }}
                </a-tag>
              </template>
            </a-table-column>
          </a-table>
        </div>
      </div>
      
      <!-- FAQ 部分 -->
      <div class="faq-section">
        <h2 class="section-title">常见问题</h2>
        <a-collapse v-model:activeKey="activeNames" class="faq-collapse">
          <a-collapse-panel 
            v-for="(faq, index) in faqs" 
            :key="index"
            :header="faq.question"
          >
            <p>{{ faq.answer }}</p>
          </a-collapse-panel>
        </a-collapse>
      </div>
    </div>
</template>

<script>
import request from '@/utils/request'
import { message } from 'ant-design-vue'
import { CheckOutlined } from '@ant-design/icons-vue'

export default {
  name: 'Pricing',
  components: {
    CheckOutlined
  },
  data() {
    return {
      activeNames: [],
      pricingPlans: [
        {
          name: '免费版',
          price: 0,
          description: '适合个人用户体验',
          features: [
            '每月100条消息',
            '基础模型访问',
            '社区支持',
            '基础API访问'
          ],
          buttonText: '免费开始',
          featured: false
        },
        {
          name: '专业版',
          price: 99,
          description: '适合专业用户和小团队',
          features: [
            '每月1000条消息',
            '所有模型访问',
            '优先支持',
            '高级API功能',
            '使用统计分析',
            '自定义集成'
          ],
          buttonText: '立即订阅',
          featured: true
        },
        {
          name: '企业版',
          price: 299,
          description: '适合大型团队和企业',
          features: [
            '无限消息',
            '所有模型访问',
            '专属客户经理',
            '企业级API',
            '详细分析报告',
            '自定义部署',
            'SLA保障',
            '数据隐私保护'
          ],
          buttonText: '联系销售',
          featured: false
        }
      ],
      modelPricing: [],
      faqs: [
        {
          question: '订阅方案包括什么？',
          answer: 'ChatHub的订阅方案除了为用户提供所有会员功能外，还包括访问流行的AI模型，如GPT-4、Claude 3和Gemini Pro等，无需单独的API Key。我们每月提供一定的使用额度，使其成为寻求无忧AI体验的用户的理想选择。'
        },
        {
          question: '订阅ChatHub后，我还需要单独订阅ChatGPT Plus或Claude Pro吗？',
          answer: '不需要，您的ChatHub订阅已经提供了对GPT-4、Claude 3.5 Sonnet和Claude Opus等高级模型的访问。因此，您不需要单独订阅ChatGPT Plus或Claude Pro。'
        },
        {
          question: 'AI服务使用额度是如何工作的？',
          answer: '每个月AI服务方案的用户会收到一个预定的访问AI模型的额度。高级AI模型比基础AI模型更强大，但同时也有更高的推理成本，因此我们为它们设定了不同的月度使用限制。'
        },
        {
          question: '支持哪些AI模型？',
          answer: '我们为不同的用例提供了各种热门AI模型，如GPT-4、Claude 3、Gemini Pro等。您可以在模型定价表中找到所有可用模型的完整列表。'
        },
        {
          question: '如何取消订阅？',
          answer: '您可以随时在账户设置中取消订阅。取消后，您的服务将在当前计费周期结束时停止，但您仍可以使用剩余的额度。'
        },
        {
          question: '是否提供退款？',
          answer: '我们提供7天无理由退款保证。如果您在订阅后7天内不满意我们的服务，可以申请全额退款。'
        }
      ]
    }
  },
  async mounted() {
    await this.loadModelPricing()
  },
  methods: {
    async loadModelPricing() {
      try {
        const response = await request.get('/api/models')
        this.modelPricing = response.data.map(model => ({
          id: model.id,
          name: model.display_name || model.model_name,
          provider: model.provider_display_name || model.model_provider,
          description: model.description,
          cost_per_message: this.calculateCostPerMessage(model.input_price_per_1k, model.output_price_per_1k),
          input_price: model.input_price_per_1k,
          output_price: model.output_price_per_1k,
          tags: this.getModelTags(model.model_name)
        }))
      } catch (error) {
        console.error('加载模型定价失败:', error)
      }
    },
    calculateCostPerMessage(inputPrice, outputPrice) {
      // 假设每条消息平均使用 100 个 token 输入和 100 个 token 输出
      const avgInputTokens = 100
      const avgOutputTokens = 100
      const inputCost = (inputPrice || 0) * (avgInputTokens / 1000)
      const outputCost = (outputPrice || 0) * (avgOutputTokens / 1000)
      return (inputCost + outputCost).toFixed(4)
    },
    getModelTags(modelId) {
      const tagMap = {
        'gpt-4': ['创作', '分析', '编程'],
        'gpt-3.5-turbo': ['日常对话', '快速响应'],
        'claude-3-sonnet': ['长文本', '推理'],
        'claude-3-opus': ['复杂任务', '创意写作'],
        'gemini-pro': ['多模态', '搜索']
      }
      return tagMap[modelId] || ['通用']
    },
    selectPlan(plan) {
      if (plan.name === '免费版') {
        this.$router.push('/chat')
      } else if (plan.name === '企业版') {
        // 联系销售逻辑
        message.info('请联系我们的销售团队获取企业版方案', 2)
      } else {
        // 订阅逻辑
        message.success(`正在为您开通${plan.name}...`, 2)
        setTimeout(() => {
          this.$router.push('/dashboard')
        }, 1500)
      }
    }
  }
}
</script>

<style scoped>
:root {
  --primary-gradient: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  --secondary-gradient: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  --background-gradient: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
}

.pricing {
  min-height: calc(100vh - 140px);
  padding: 60px 24px;
  max-width: 1200px;
  margin: 0 auto;
  background: var(--background-gradient);
  position: relative;
}

.pricing::before {
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

.page-title {
  text-align: center;
  font-size: 3.5rem;
  font-weight: 800;
  color: #667eea;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 60px;
  position: relative;
  z-index: 1;
}

/* 备用样式，确保在不支持渐变文字的浏览器中文字可见 */
@supports not (-webkit-background-clip: text) {
  .page-title {
    color: #667eea !important;
    background: none !important;
  }
}

.page-title::after {
  content: '';
  position: absolute;
  bottom: -15px;
  left: 50%;
  transform: translateX(-50%);
  width: 100px;
  height: 4px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 2px;
}

.page-subtitle {
  text-align: center;
  font-size: 1.2rem;
  color: #718096;
  margin-bottom: 60px;
  font-weight: 500;
}

.pricing-grid {
  display: grid;
  grid-template-columns: repeat(3, 1fr);
  gap: 32px;
  max-width: 1200px;
  margin: 0 auto;
  align-items: stretch;
}

.pricing-card {
  background: white;
  border-radius: 24px;
  padding: 40px 32px;
  transition: all 0.4s ease;
  position: relative;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  display: flex;
  flex-direction: column;
  height: 100%;
}

.pricing-card::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 6px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

.pricing-card:hover {
  transform: translateY(-15px);
  box-shadow: 0 25px 60px rgba(102, 126, 234, 0.15);
}

.pricing-card.featured {
  border: 2px solid #4ecdc4;
  transform: scale(1.02);
  box-shadow: 0 20px 50px rgba(78, 205, 196, 0.2);
  z-index: 2;
}

.pricing-card.featured::before {
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  height: 8px;
}

.pricing-card.featured:hover {
  transform: scale(1.02) translateY(-12px);
  box-shadow: 0 30px 70px rgba(78, 205, 196, 0.25);
}

.featured-badge {
  position: absolute;
  top: -15px;
  right: 25px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  color: white;
  padding: 10px 24px;
  border-radius: 25px;
  font-size: 0.95rem;
  font-weight: 700;
  box-shadow: 0 4px 15px rgba(78, 205, 196, 0.3);
  white-space: nowrap;
  z-index: 10;
}

.card-header {
  text-align: center;
  margin-bottom: 32px;
}

.card-body {
  flex: 1;
  display: flex;
  flex-direction: column;
}

.card-footer {
  margin-top: auto;
  padding-top: 24px;
}

.plan-name {
  font-size: 1.8rem;
  font-weight: 800;
  margin-bottom: 20px;
  color: #2d3748;
}

.plan-price {
  display: flex;
  align-items: baseline;
  justify-content: center;
  margin-bottom: 20px;
}

.currency {
  font-size: 1.4rem;
  margin-right: 8px;
  color: #4a5568;
  font-weight: 600;
}

.amount {
  font-size: 3.5rem;
  font-weight: 900;
  color: #4ecdc4;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

/* 备用样式，确保在不支持渐变文字的浏览器中价格数字可见 */
@supports not (-webkit-background-clip: text) {
  .amount {
    color: #4ecdc4 !important;
    background: none !important;
  }
}

.period {
  font-size: 1.1rem;
  color: #718096;
  margin-left: 8px;
  font-weight: 500;
}

.plan-description {
  color: #4a5568;
  line-height: 1.6;
  font-size: 1rem;
  font-weight: 500;
}

.features-list {
  list-style: none;
  padding: 0;
  margin: 0;
  flex: 1;
}

.feature-item {
  display: flex;
  align-items: center;
  margin-bottom: 14px;
  font-size: 0.95rem;
  color: #2d3748;
  font-weight: 500;
}

.feature-item:last-child {
  margin-bottom: 0;
}

.check-icon {
  color: #4ecdc4;
  margin-right: 12px;
  font-size: 1.3rem;
  filter: drop-shadow(0 2px 4px rgba(78, 205, 196, 0.3));
}

:deep(.select-plan-btn) {
  height: 50px;
  font-size: 1.1rem;
  font-weight: 700;
  border-radius: 12px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border: none;
  transition: all 0.3s ease;
  box-shadow: 0 8px 25px rgba(102, 126, 234, 0.3);
}

:deep(.select-plan-btn:hover) {
  transform: translateY(-2px);
  box-shadow: 0 12px 35px rgba(102, 126, 234, 0.4);
}

:deep(.pricing-card.featured .select-plan-btn) {
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  box-shadow: 0 8px 25px rgba(78, 205, 196, 0.3);
}

:deep(.pricing-card.featured .select-plan-btn:hover) {
  box-shadow: 0 12px 35px rgba(78, 205, 196, 0.4);
}

.model-pricing {
  margin-bottom: 100px;
  position: relative;
  z-index: 1;
}

.section-title {
  text-align: center;
  font-size: 2.8rem;
  font-weight: 800;
  color: #667eea;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  margin-bottom: 50px;
  position: relative;
}

/* 备用样式，确保section标题可见 */
@supports not (-webkit-background-clip: text) {
  .section-title {
    color: #667eea !important;
    background: none !important;
  }
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

.pricing-table {
  background: white;
  border-radius: 20px;
  padding: 30px;
  box-shadow: 0 15px 40px rgba(0, 0, 0, 0.08);
  border: 1px solid rgba(102, 126, 234, 0.1);
  position: relative;
  overflow: hidden;
}

.pricing-table::before {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 4px;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
}

.model-name {
  display: flex;
  flex-direction: column;
}

.model-name strong {
  color: #2d3748;
  font-weight: 700;
}

.provider {
  font-size: 0.85rem;
  color: #718096;
  margin-top: 4px;
  font-weight: 500;
}

.price-cell {
  font-weight: 800;
  color: #4ecdc4;
  background: linear-gradient(135deg, #4ecdc4 0%, #44a08d 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
  font-size: 1.2rem;
}

/* 备用样式，确保价格表格中的价格可见 */
@supports not (-webkit-background-clip: text) {
  .price-cell {
    color: #4ecdc4 !important;
    background: none !important;
  }
}

.pricing-details {
  display: flex;
  flex-direction: column;
  gap: 4px;
}

.price-row {
  display: flex;
  justify-content: space-between;
  align-items: center;
  font-size: 0.9rem;
}

.price-label {
  color: #718096;
  font-weight: 500;
  min-width: 60px;
}

.price-value {
  font-weight: 700;
  color: #2d3748;
}

.input-price {
  color: #4ecdc4;
}

.output-price {
  color: #667eea;
}

.total-price {
  margin-top: 4px;
  padding-top: 4px;
  border-top: 1px solid rgba(0, 0, 0, 0.1);
  font-weight: 600;
}

.total-price .price-value {
  color: #e53e3e;
  font-size: 1rem;
}

:deep(.scenario-tag) {
  margin-right: 6px;
  margin-bottom: 4px;
  border-radius: 12px;
  font-weight: 500;
  font-size: 0.8rem;
}

.faq-section {
  margin-bottom: 60px;
  position: relative;
  z-index: 1;
}

:deep(.ant-table) {
  background: transparent;
  border-radius: 12px;
  overflow: hidden;
}

:deep(.ant-table-thead > tr > th) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  color: #2d3748;
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
  font-weight: 700;
  font-size: 1rem;
}

:deep(.ant-table-tbody > tr > td) {
  background: transparent;
  color: #2d3748;
  border-bottom: 1px solid rgba(0, 0, 0, 0.06);
  font-weight: 500;
}

:deep(.ant-table-tbody > tr:hover > td) {
  background: rgba(102, 126, 234, 0.05);
}

:deep(.ant-collapse) {
  background: transparent;
  border: none;
}

:deep(.ant-collapse-item) {
  background: white;
  border-radius: 16px;
  margin-bottom: 16px;
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.06);
  border: 1px solid rgba(102, 126, 234, 0.1);
  overflow: hidden;
  position: relative;
}

:deep(.ant-collapse-item::before) {
  content: '';
  position: absolute;
  top: 0;
  left: 0;
  right: 0;
  height: 3px;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
}

:deep(.ant-collapse-header) {
  background: transparent;
  color: #2d3748;
  border: none;
  padding: 24px;
  font-weight: 700;
  font-size: 1.1rem;
  transition: all 0.3s ease;
}

:deep(.ant-collapse-item-active .ant-collapse-header) {
  background: rgba(102, 126, 234, 0.05);
}

:deep(.ant-collapse-content) {
  background: transparent;
  color: #4a5568;
  border: none;
}

:deep(.ant-collapse-content-box) {
  padding: 0 24px 24px;
  line-height: 1.7;
  font-weight: 500;
}

/* 响应式设计 */
@media (max-width: 1024px) {
  .pricing-grid {
    gap: 24px;
    max-width: 900px;
  }
  
  .pricing-card {
    padding: 32px 24px;
  }
  
  .featured-badge {
    top: -12px;
    right: 20px;
    padding: 8px 20px;
    font-size: 0.9rem;
  }
}

@media (max-width: 768px) {
  .pricing {
    padding: 40px 16px;
  }
  
  .page-title {
    font-size: 2.5rem;
  }
  
  .section-title {
    font-size: 2.2rem;
  }
  
  .pricing-grid {
    grid-template-columns: 1fr;
    gap: 30px;
    max-width: 400px;
  }
  
  .pricing-card {
    padding: 32px 28px;
  }
  
  .pricing-card.featured {
    transform: none;
  }
  
  .pricing-card.featured:hover {
    transform: translateY(-10px);
  }
  
  .featured-badge {
    top: -10px;
    right: 25px;
    padding: 8px 18px;
    font-size: 0.85rem;
    border-radius: 20px;
  }
  
  .plan-name {
    font-size: 1.5rem;
  }
  
  .amount {
    font-size: 2.8rem;
  }
}

@media (max-width: 480px) {
  .page-title {
    font-size: 2rem;
  }
  
  .section-title {
    font-size: 1.8rem;
  }
  
  .pricing-card {
    padding: 24px;
    border-radius: 16px;
  }
  
  .pricing-table {
    padding: 20px;
    border-radius: 16px;
  }
  
  :deep(.ant-collapse-item) {
    border-radius: 12px;
  }
  
  :deep(.ant-collapse-header) {
    padding: 16px;
    font-size: 1rem;
  }
  
  :deep(.ant-collapse-content-box) {
    padding: 0 16px 16px;
  }
}
</style>