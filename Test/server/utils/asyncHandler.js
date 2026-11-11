/**
 * 包装异步控制器，将 Promise 拒绝统一交给 errorHandler 处理
 */
module.exports = (fn) => (req, res, next) =>
  Promise.resolve(fn(req, res, next)).catch(next)