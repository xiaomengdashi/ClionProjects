import axios from 'axios'
import router from '../router'
import { message } from 'ant-design-vue'
import { API_BASE_URL } from '../config'

// 导出API基础URL供其他模块使用
export { API_BASE_URL }

// 创建axios实例
const request = axios.create({
  baseURL: API_BASE_URL,
  timeout: 10000
})

// 请求拦截器
request.interceptors.request.use(
  config => {
    let token = localStorage.getItem('token')
    
    // 如果没有token，使用demo-token进行演示
    if (!token) {
      token = 'demo-token'
      localStorage.setItem('token', token)
      localStorage.setItem('username', 'demo')
    }
    
    if (token) {
      config.headers.Authorization = `Bearer ${token}`
    }
    return config
  },
  error => {
    return Promise.reject(error)
  }
)

// 响应拦截器
request.interceptors.response.use(
  response => {
    return response
  },
  error => {
    if (error.response) {
      switch (error.response.status) {
        case 401:
          message.error('登录已过期，请重新登录', 1)
          localStorage.removeItem('token')
          localStorage.removeItem('username')
          router.push('/login')
          break
        case 403:
          message.error('没有权限访问', 1)
          break
        case 500:
          message.error('服务器错误', 1)
          break
        default:
          message.error(error.response.data?.error || '请求失败', 1)
      }
    } else {
      message.error('网络错误', 1)
    }
    return Promise.reject(error)
  }
)

export default request