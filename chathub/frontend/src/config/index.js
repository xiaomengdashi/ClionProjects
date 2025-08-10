// API配置
const config = {
  // 开发环境配置 - 使用相对路径，通过Vite代理转发
  development: {
    API_BASE_URL: ''  // 空字符串表示使用相对路径
  },
  // 生产环境配置 - 使用完整的API地址
  production: {
    API_BASE_URL: import.meta.env.VITE_API_BASE_URL || 'http://localhost:5001'
  }
}

// 获取当前环境
const env = import.meta.env.MODE || 'development'

// 导出当前环境的配置
export const API_BASE_URL = config[env].API_BASE_URL

export default config[env]