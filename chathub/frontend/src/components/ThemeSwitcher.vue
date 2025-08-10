<template>
  <div class="theme-switcher">
    <label class="theme-label">代码主题</label>
    <select 
      v-model="selectedTheme" 
      @change="changeTheme"
      class="theme-select"
    >
      <option value="one-dark">One Dark Pro</option>
      <option value="one-light">One Light</option>
      <option value="github">GitHub</option>
      <option value="monokai">Monokai</option>
    </select>
  </div>
</template>

<script>
export default {
  name: 'ThemeSwitcher',
  data() {
    return {
      selectedTheme: localStorage.getItem('codeTheme') || 'one-dark'
    }
  },
  mounted() {
    // 初始化主题
    this.applyTheme(this.selectedTheme)
  },
  methods: {
    changeTheme() {
      this.applyTheme(this.selectedTheme)
      localStorage.setItem('codeTheme', this.selectedTheme)
    },
    applyTheme(theme) {
      // 移除所有主题类
      const themes = ['one-dark', 'one-light', 'github', 'monokai']
      themes.forEach(t => {
        document.documentElement.classList.remove(`theme-${t}`)
      })
      
      // 添加新主题类
      document.documentElement.classList.add(`theme-${theme}`)
      
      // 动态加载主题样式
      this.loadThemeStyles(theme)
    },
    async loadThemeStyles(theme) {
      // 移除旧的主题样式
      const oldLink = document.getElementById('prism-theme')
      if (oldLink) {
        oldLink.remove()
      }
      
      try {
        // 直接导入CSS内容并注入到页面
        let cssContent = ''
        switch(theme) {
          case 'one-dark':
            cssContent = await import('../styles/prism-one-dark.css?inline')
            break
          case 'one-light':
            cssContent = await import('../styles/prism-one-light.css?inline')
            break
          case 'github':
            cssContent = await import('../styles/prism-github.css?inline')
            break
          case 'monokai':
            cssContent = await import('../styles/prism-monokai.css?inline')
            break
          default:
            cssContent = await import('../styles/prism-one-dark.css?inline')
        }
        
        // 创建style标签并注入CSS
        const style = document.createElement('style')
        style.id = 'prism-theme'
        style.textContent = cssContent.default || cssContent
        document.head.appendChild(style)
      } catch (error) {
        console.error('Failed to load theme:', theme, error)
        // 降级处理：使用fetch获取CSS内容
        try {
          const response = await fetch(`/src/styles/prism-${theme}.css`)
          const cssText = await response.text()
          const style = document.createElement('style')
          style.id = 'prism-theme'
          style.textContent = cssText
          document.head.appendChild(style)
        } catch (fetchError) {
          console.error('Fallback CSS loading failed:', fetchError)
        }
      }
    }
  }
}
</script>

<style scoped>
.theme-switcher {
  display: flex;
  align-items: center;
  gap: 8px;
  margin: 16px 0;
}

.theme-label {
  font-size: 14px;
  font-weight: 600;
  color: #4a5568;
}

.theme-select {
  padding: 6px 12px;
  border: 1px solid #d0d0d0;
  border-radius: 6px;
  background: white;
  color: #383a42;
  font-size: 14px;
  cursor: pointer;
  transition: all 0.2s ease;
}

.theme-select:hover {
  border-color: #61afef;
}

.theme-select:focus {
  outline: none;
  border-color: #61afef;
  box-shadow: 0 0 0 2px rgba(97, 175, 239, 0.2);
}

/* 深色模式下的样式 */
@media (prefers-color-scheme: dark) {
  .theme-label {
    color: #c5cad3;
  }
  
  .theme-select {
    background: #21252b;
    border-color: #2c313c;
    color: #c5cad3;
  }
  
  .theme-select:hover {
    border-color: #61afef;
  }
}
</style>