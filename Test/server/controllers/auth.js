const asyncHandler = require('../utils/asyncHandler')
const httpError = require('../utils/httpError')
const { success } = require('../utils/response')
const { authenticate } = require('../services/auth')

/**
 * POST /api/auth/login
 * 校验用户名密码，签发 JWT
 */
const login = asyncHandler(async (req, res) => {
  const { username, password } = req.body || {}

  if (!username || !password) {
    throw httpError(400, '请输入用户名和密码')
  }

  const data = await authenticate(username, password)
  return success(res, data, '登录成功')
})

/**
 * GET /api/auth/me
 * 返回当前登录用户信息
 */
const me = asyncHandler(async (req, res) => {
  return success(res, { username: req.user.username })
})

module.exports = { login, me }