const config = require('../config')
const logger = require('../utils/logger')
const { fail } = require('../utils/response')

function errorHandler(err, req, res, _next) {
  const status = err.status || 500
  logger.error('全局异常捕获:', err.stack || err.message || err)

  const detail = config.nodeEnv === 'development' ? err.message : undefined
  const message = err.status ? err.message : '服务器内部错误'
  return fail(res, status, message, detail)
}

module.exports = errorHandler