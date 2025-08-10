import { createRouter, createWebHistory } from 'vue-router'
import Home from '../views/Home.vue'
import Chat from '../views/Chat.vue'
import Pricing from '../views/Pricing.vue'
import Dashboard from '../views/Dashboard.vue'
import ApiKeys from '../views/ApiKeys.vue'
import Models from '../views/Models.vue'
import Login from '../views/Login.vue'
import Register from '../views/Register.vue'
import UserManagement from '../views/UserManagement.vue'


// 检查用户是否已登录
function isAuthenticated() {
  const token = localStorage.getItem('token')
  if (!token) return false
  
  try {
    const payload = JSON.parse(atob(token.split('.')[1]))
    return payload.exp > Date.now() / 1000
  } catch (e) {
    return false
  }
}

// 检查用户是否为管理员
function isAdmin() {
  const token = localStorage.getItem('token')
  if (!token) return false
  
  try {
    const payload = JSON.parse(atob(token.split('.')[1]))
    return payload.role === 'admin'
  } catch (e) {
    return false
  }
}
const routes = [
  {
    path: '/',
    name: 'Home',
    component: Home
  },
  {
      path: '/chat',
      name: 'Chat',
      component: Chat,
      meta: { requiresAuth: true }
    },
  {
    path: '/pricing',
    name: 'Pricing',
    component: Pricing
  },
  {
    path: '/dashboard',
    name: 'Dashboard',
    component: Dashboard,
    meta: { requiresAuth: true }
  },
  {
    path: '/api-keys',
    name: 'ApiKeys',
    component: ApiKeys,
    meta: { requiresAuth: true, requiresAdmin: true }
  },
  {
    path: '/models',
    name: 'Models',
    component: Models,
    meta: { requiresAuth: true, requiresAdmin: true }
  },
  {
    path: '/users',
    name: 'UserManagement',
    component: UserManagement,
    meta: { requiresAuth: true, requiresAdmin: true }
  },
  {
    path: '/login',
    name: 'Login',
    component: Login
  },
  {
    path: '/register',
    name: 'Register',
    component: Register
  }
]

const router = createRouter({
  history: createWebHistory(),
  routes
})

// 路由守卫
router.beforeEach((to, from, next) => {
  if (to.matched.some(record => record.meta.requiresAuth)) {
    if (!isAuthenticated()) {
      next('/login')
    } else if (to.matched.some(record => record.meta.requiresAdmin)) {
      if (!isAdmin()) {
        next('/chat') // 非管理员重定向到聊天页面
      } else {
        next()
      }
    } else {
      next()
    }
  } else {
    next()
  }
})

export default router