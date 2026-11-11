const config = require('../config')
const httpError = require('../utils/httpError')
const { verifyToken } = require('../services/auth')

/**
 * 校验 Authorization: Bearer <token>，并将解析结果挂到 req.user
 */
function requireAuth(req, res, next) {
  const header = req.headers.authorization || ''
  const [scheme, token] = header.split(' ')

  if (scheme !== 'Bearer' || !token) {
    return next(httpError(401, '未登录：请先登录'))
  }

  try {
    req.user = verifyToken(token, config.jwtSecret)
    next()
  } catch (err) {
    next(err)
  }
}

module.exports = requireAuth