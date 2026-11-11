const express = require('express')
const cors = require('cors')
const path = require('path')
const morgan = require('morgan')

const config = require('./config')
const otaRoutes = require('./routes')
const authRoutes = require('./routes/auth')
const requireAuth = require('./middleware/requireAuth')
const notFound = require('./middleware/notFound')
const errorHandler = require('./middleware/errorHandler')

const app = express()

// ---- 基础安全与通用中间件 ----
app.disable('x-powered-by')
app.use(cors())
app.use(express.json({ limit: '1mb' }))
app.use(morgan(config.nodeEnv === 'production' ? 'combined' : 'dev'))

// ---- 托管 Vue 打包后的静态文件 dist/ ----
app.use(express.static(path.join(__dirname, 'dist')))

// ---- API 路由：登录接口公开，其余需登录鉴权 ----
app.use('/api/auth', authRoutes)
app.use('/api/ota', requireAuth, otaRoutes)

// ---- SPA 回退：非 /api 的 GET 请求统一交给 Vue 的 index.html ----
app.use((req, res, next) => {
  if (req.method === 'GET' && !req.path.startsWith('/api')) {
    return res.sendFile(path.join(__dirname, 'dist', 'index.html'))
  }
  next()
})

// ---- 兜底处理 ----
app.use(notFound)
app.use(errorHandler)

module.exports = app