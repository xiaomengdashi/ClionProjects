import MarkdownIt from 'markdown-it'
import Prism from 'prismjs'

// 注意：主题CSS现在通过ThemeSwitcher动态加载，不在这里静态导入

// 导入常用编程语言的 Prism 组件
import 'prismjs/components/prism-javascript'
import 'prismjs/components/prism-typescript'
import 'prismjs/components/prism-python'
import 'prismjs/components/prism-java'
import 'prismjs/components/prism-c'
import 'prismjs/components/prism-csharp'
import 'prismjs/components/prism-ruby'
import 'prismjs/components/prism-go'
import 'prismjs/components/prism-rust'
import 'prismjs/components/prism-bash'
import 'prismjs/components/prism-sql'
import 'prismjs/components/prism-json'
import 'prismjs/components/prism-yaml'
import 'prismjs/components/prism-css'

// 语言别名映射
const languageAliases = {
  'js': 'javascript',
  'ts': 'typescript',
  'py': 'python',
  'rb': 'ruby',
  'sh': 'bash',
  'shell': 'bash',
  'yml': 'yaml',
  'xml': 'markup',
  'html': 'markup',
  'htm': 'markup',
  'svg': 'markup',
  'vue': 'markup',
  'jsx': 'javascript',
  'tsx': 'typescript'
}

// 创建 markdown 实例
const md = new MarkdownIt({
  html: true,
  linkify: true,
  typographer: true
})

// 确保 Prism 实例可用
if (typeof window !== 'undefined') {
  window.Prism = Prism
}

// 自定义渲染器来添加复制按钮和包装器
const defaultFenceRenderer = md.renderer.rules.fence
md.renderer.rules.fence = function (tokens, idx, options, env, renderer) {
  const token = tokens[idx]
  const info = token.info ? md.utils.unescapeAll(token.info).trim() : ''
  const langName = info.split(/\s+/g)[0] || 'text'
  
  // 处理语言别名
  const actualLang = languageAliases[langName] || langName
  
  // 如果是支持的语言，使用 Prism 高亮
  if (actualLang && Prism.languages[actualLang]) {
    try {
      const highlighted = Prism.highlight(token.content, Prism.languages[actualLang], actualLang)
      return `<div class="code-block-wrapper">
        <div class="code-block-header">
          <span class="code-language">${langName}</span>
          <button class="copy-code-btn" onclick="copyCode(this)" title="复制代码">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
              <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2 2v1"></path>
            </svg>
          </button>
        </div>
        <pre class="language-${actualLang}"><code class="language-${actualLang}">${highlighted}</code></pre>
      </div>`
    } catch (error) {
      console.warn('Prism highlighting failed for language:', actualLang, error)
      // 降级到无高亮的代码块
      return `<div class="code-block-wrapper">
        <div class="code-block-header">
          <span class="code-language">${langName}</span>
          <button class="copy-code-btn" onclick="copyCode(this)" title="复制代码">
            <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
              <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
              <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2 2v1"></path>
            </svg>
          </button>
        </div>
        <pre><code>${md.utils.escapeHtml(token.content)}</code></pre>
      </div>`
    }
  }

  // 降级处理：无语言或不支持的语言
  return `<div class="code-block-wrapper">
    <div class="code-block-header">
      <span class="code-language">${langName}</span>
      <button class="copy-code-btn" onclick="copyCode(this)" title="复制代码">
        <svg width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2">
          <rect x="9" y="9" width="13" height="13" rx="2" ry="2"></rect>
          <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2 2v1"></path>
        </svg>
      </button>
    </div>
    <pre><code>${md.utils.escapeHtml(token.content)}</code></pre>
  </div>`
}

// 自定义渲染规则
md.renderer.rules.code_inline = function (tokens, idx) {
  const token = tokens[idx]
  return `<code class="inline-code">${md.utils.escapeHtml(token.content)}</code>`
}

md.renderer.rules.code_block = function (tokens, idx) {
  const token = tokens[idx]
  return `<pre class="code-block"><code>${md.utils.escapeHtml(token.content)}</code></pre>`
}

// 渲染markdown为HTML
export function renderMarkdown(content) {
  if (!content) return ''
  return md.render(content)
}

// 检查内容是否包含markdown语法
export function hasMarkdownSyntax(content) {
  if (!content) return false
  
  // 检查常见的markdown语法
  const markdownPatterns = [
    /```[\s\S]*?```/,  // 代码块
    /`[^`]+`/,         // 行内代码
    /\*\*[^*]+\*\*/,   // 粗体
    /\*[^*]+\*/,       // 斜体
    /#{1,6}\s+/,       // 标题
    /^\s*[-*+]\s+/m,   // 列表
    /^\s*\d+\.\s+/m,   // 有序列表
    /\[([^\]]+)\]\(([^)]+)\)/, // 链接
  ]
  
  return markdownPatterns.some(pattern => pattern.test(content))
}