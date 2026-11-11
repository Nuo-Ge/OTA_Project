import axios from 'axios'

export const TOKEN_KEY = 'ota_token'

const request = axios.create({
  baseURL: import.meta.env.VITE_API_URL || '/api',
  timeout: 10000,
})

// 请求拦截器：自动附带登录 token
request.interceptors.request.use((config) => {
  const token = localStorage.getItem(TOKEN_KEY)
  if (token) {
    config.headers.Authorization = `Bearer ${token}`
  }
  return config
})

// 响应拦截器：统一解包信封 + 401 时清除本地登录态
request.interceptors.response.use(
  (response) => response.data,
  (error) => {
    const message =
      error.response?.data?.message || error.message || '请求失败，请稍后重试'
    error.userMessage = message

    if (error.response?.status === 401) {
      const hadToken = Boolean(localStorage.getItem(TOKEN_KEY))
      localStorage.removeItem(TOKEN_KEY)
      if (hadToken) {
        window.dispatchEvent(new Event('auth:logout'))
      }
    }

    console.error('请求失败:', message)
    return Promise.reject(error)
  },
)

export default request