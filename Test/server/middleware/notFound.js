const { fail } = require('../utils/response')

function notFound(req, res) {
  return fail(res, 404, `路由 ${req.method} ${req.path} 不存在`)
}

module.exports = notFound