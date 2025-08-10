<template>
  <div 
    class="markdown-content" 
    v-html="renderedContent"
  ></div>
</template>

<script>
import { renderMarkdown, hasMarkdownSyntax } from '../utils/markdown'

export default {
  name: 'MarkdownRenderer',
  props: {
    content: {
      type: String,
      default: ''
    },
    forceMarkdown: {
      type: Boolean,
      default: false
    }
  },
  computed: {
    renderedContent() {
      if (!this.content) return ''
      
      // 如果强制使用markdown或检测到markdown语法，则渲染为HTML
      if (this.forceMarkdown || hasMarkdownSyntax(this.content)) {
        return renderMarkdown(this.content)
      }
      
      // 否则保持纯文本，但处理换行
      return this.content.replace(/\n/g, '<br>')
    }
  }
}
</script>

<style scoped>
.markdown-content {
  line-height: 1.6;
  word-wrap: break-word;
}

/* 代码块容器样式 */
:deep(.code-block-wrapper) {
  margin: 16px 0;
  border-radius: 8px;
  overflow: hidden;
  border: 1px solid;
  box-shadow: 
    0 4px 12px rgba(0, 0, 0, 0.15),
    0 2px 4px rgba(0, 0, 0, 0.1);
  position: relative;
  transition: all 0.2s ease;
}

/* 代码块头部样式 */
:deep(.code-block-header) {
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 5px 16px;
  background: var(--code-bg);
  border-bottom: 1px solid var(--code-border);
}

:deep(.code-language) {
  font-size: 0.75rem;
  font-weight: 600;
  text-transform: uppercase;
  letter-spacing: 0.5px;
  padding: 4px 8px;
  border-radius: 4px;
  border: 1px solid var(--code-border);
  color: var(--code-color);
  background: rgba(255, 255, 255, 0.05);
}


/* 复制按钮样式 */
:deep(.copy-code-btn) {
  border: 1px solid var(--code-border);
  border-radius: 6px;
  padding: 6px 8px;
  cursor: pointer;
  transition: all 0.2s ease;
  display: flex;
  align-items: center;
  justify-content: center;
  background: rgba(255, 255, 255, 0.05);
  color: var(--code-color);
}

:deep(.copy-code-btn:hover) {
  background: rgba(255, 255, 255, 0.1);
  border-color: var(--primary-color);
}

:deep(.copy-code-btn svg) {
  width: 16px;
  height: 16px;
}

/* 代码内容样式 */
:deep(.code-block-wrapper pre) {
  margin: 0;
  padding: 16px;
  background: transparent;
  overflow-x: auto;
  font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', 'Monaco', 'Consolas', monospace;
  font-size: 0.875rem;
  line-height: 1.5;
  position: relative;
}

:deep(.code-block-wrapper code) {
  background: transparent;
  padding: 0;
  border: none;
  font-family: inherit;
  font-size: inherit;
  color: inherit;
  white-space: pre;
  word-wrap: normal;
}

/* Prism.js 语法高亮样式 */
:deep(pre[class*="language-"]) {
  background: transparent;
  font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', 'Monaco', 'Consolas', monospace;
  font-size: 0.875rem;
  line-height: 1.5;
  padding: 16px;
  margin: 0;
  overflow: auto;
  border-radius: 0;
}

:deep(code[class*="language-"]) {
  background: transparent;
  font-family: inherit;
  font-size: inherit;
  line-height: inherit;
}

/* Token 样式 */
:deep(.token.important),
:deep(.token.bold) {
  font-weight: bold;
}

:deep(.token.italic) {
  font-style: italic;
}

:deep(.token.entity) {
  cursor: help;
}

/* 通用 code 样式 */
:deep(code) {
  font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', 'Monaco', 'Consolas', monospace;
  font-size: inherit;
}

/* 行内代码样式 */
:deep(.inline-code) {
  padding: 2px 6px;
  border-radius: 4px;
  font-family: 'JetBrains Mono', 'Fira Code', 'SF Mono', 'Monaco', 'Consolas', monospace;
  font-size: 0.875rem;
  border: 1px solid;
  transition: all 0.2s ease;
}

/* Markdown其他元素样式增强 */
:deep(h1, h2, h3, h4, h5, h6) {
  color: #2d3748;
  font-weight: 700;
  margin: 24px 0 16px 0;
  line-height: 1.3;
}

:deep(h1) {
  font-size: 2rem;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

:deep(h2) {
  font-size: 1.5rem;
  color: #4a5568;
}

:deep(h3) {
  font-size: 1.25rem;
  color: #4a5568;
}

:deep(p) {
  margin: 12px 0;
  line-height: 1.7;
  color: #4a5568;
}

:deep(ul, ol) {
  margin: 16px 0;
  padding-left: 24px;
}

:deep(li) {
  margin: 8px 0;
  line-height: 1.6;
  color: #4a5568;
}

:deep(blockquote) {
  border-left: 4px solid #667eea;
  background: rgba(102, 126, 234, 0.05);
  margin: 16px 0;
  padding: 16px 20px;
  border-radius: 0 8px 8px 0;
  font-style: italic;
  color: #4a5568;
}

:deep(a) {
  color: #667eea;
  text-decoration: none;
  font-weight: 600;
  transition: all 0.2s ease;
}

:deep(a:hover) {
  color: #764ba2;
  text-decoration: underline;
}

:deep(table) {
  width: 100%;
  border-collapse: collapse;
  margin: 20px 0;
  border-radius: 12px;
  overflow: hidden;
  box-shadow: 0 8px 25px rgba(0, 0, 0, 0.08);
}

:deep(th, td) {
  padding: 12px 16px;
  text-align: left;
  border-bottom: 1px solid rgba(102, 126, 234, 0.1);
}

:deep(th) {
  background: linear-gradient(135deg, #f7fafc 0%, #edf2f7 100%);
  font-weight: 700;
  color: #2d3748;
}

:deep(tr:hover) {
  background: rgba(102, 126, 234, 0.05);
}

/* 滚动条样式 */
:deep(.code-block-wrapper pre::-webkit-scrollbar) {
  height: 6px;
}

:deep(.code-block-wrapper pre::-webkit-scrollbar-track) {
  border-radius: 3px;
}

:deep(.code-block-wrapper pre::-webkit-scrollbar-thumb) {
  border-radius: 3px;
}

/* 响应式设计 */
@media (max-width: 768px) {
  :deep(.code-block-wrapper) {
    margin: 12px 0;
    border-radius: 8px;
  }
  
  :deep(.code-block-header) {
    padding: 10px 14px;
  }
  
  :deep(.code-block-wrapper pre) {
    padding: 14px;
    font-size: 0.8rem;
  }
  
  :deep(.code-language) {
    font-size: 0.7rem;
    padding: 3px 6px;
  }
}

@media (max-width: 480px) {
  :deep(.code-block-wrapper pre) {
    padding: 12px;
    font-size: 0.75rem;
  }
  
  :deep(.code-block-header) {
    padding: 8px 12px;
  }
  
  :deep(.inline-code) {
    padding: 2px 4px;
    font-size: 0.8rem;
  }
}
</style>