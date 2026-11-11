import { ref, computed } from 'vue'
import { login as apiLogin, getMe } from '../api/auth'
import { TOKEN_KEY } from '../api/request'

// 模块级单例状态：所有调用方共享同一份登录态
const token = ref(localStorage.getItem(TOKEN_KEY) || '')
const user = ref(null)

const isAuthenticated = computed(() => Boolean(token.value))

async function login(username, password) {
  const res = await apiLogin(username, password)
  const data = res?.data || res

  token.value = data.token
  user.value = { username: data.username }
  localStorage.setItem(TOKEN_KEY, data.token)

  return data
}

async function restoreSession() {
  if (!token.value) return
  try {
    const res = await getMe()
    user.value = res?.data || res
  } catch {
    logout()
  }
}

function logout() {
  token.value = ''
  user.value = null
  localStorage.removeItem(TOKEN_KEY)
}

// 响应 request.js 触发的 401 事件，保持内存状态与 localStorage 同步
function handleUnauthorized() {
  token.value = ''
  user.value = null
}

if (typeof window !== 'undefined') {
  window.addEventListener('auth:logout', handleUnauthorized)
}

export function useAuth() {
  return { token, user, isAuthenticated, login, logout, restoreSession }
}