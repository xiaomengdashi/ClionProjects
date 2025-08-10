import { createApp } from 'vue'
import Antd from 'ant-design-vue'
import 'ant-design-vue/dist/reset.css'
import * as AntdIcons from '@ant-design/icons-vue'
import router from './router'
import App from './App.vue'
import './styles/global.css'

const app = createApp(App)

// 注册所有图标
for (const [key, component] of Object.entries(AntdIcons)) {
  app.component(key, component)
}

// 添加全局复制代码函数
window.copyCode = function(button) {
  const codeBlock = button.closest('.code-block-wrapper').querySelector('pre code')
  const text = codeBlock.textContent || codeBlock.innerText
  
  // 复制成功后的UI反馈
  const showSuccess = () => {
    const originalHTML = button.innerHTML
    button.innerHTML = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <polyline points="20,6 9,17 4,12"></polyline>
    </svg>`
    button.style.color = '#52c41a'
    
    setTimeout(() => {
      button.innerHTML = originalHTML
      button.style.color = ''
    }, 2000)
  }
  
  // 复制失败后的UI反馈
  const showError = (error) => {
    console.error('复制失败:', error)
    const originalHTML = button.innerHTML
    button.innerHTML = `<svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
      <line x1="18" y1="6" x2="6" y2="18"></line>
      <line x1="6" y1="6" x2="18" y2="18"></line>
    </svg>`
    button.style.color = '#ff4d4f'
    
    setTimeout(() => {
      button.innerHTML = originalHTML
      button.style.color = ''
    }, 2000)
  }
  
  // 尝试使用现代 Clipboard API
  if (navigator.clipboard && window.isSecureContext) {
    navigator.clipboard.writeText(text).then(() => {
      showSuccess()
    }).catch(err => {
      // 降级到传统方法
      fallbackCopyTextToClipboard(text, showSuccess, showError)
    })
  } else {
    // 降级到传统方法
    fallbackCopyTextToClipboard(text, showSuccess, showError)
  }
}

// 降级复制方法（兼容旧浏览器和非HTTPS环境）
function fallbackCopyTextToClipboard(text, onSuccess, onError) {
  const textArea = document.createElement('textarea')
  textArea.value = text
  
  // 避免滚动到底部
  textArea.style.top = '0'
  textArea.style.left = '0'
  textArea.style.position = 'fixed'
  textArea.style.opacity = '0'
  textArea.style.pointerEvents = 'none'
  
  document.body.appendChild(textArea)
  textArea.focus()
  textArea.select()
  
  try {
    const successful = document.execCommand('copy')
    if (successful) {
      onSuccess()
    } else {
      onError(new Error('document.execCommand("copy") 返回 false'))
    }
  } catch (err) {
    onError(err)
  }
  
  document.body.removeChild(textArea)
}

app.use(Antd)
app.use(router)
app.mount('#app')