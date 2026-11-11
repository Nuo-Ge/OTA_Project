/**
 * 创建一个携带 HTTP 状态码的业务错误，供 errorHandler 统一识别
 */
class HttpError extends Error {
  constructor(status, message) {
    super(message)
    this.name = 'HttpError'
    this.status = status
  }
}

module.exports = (status, message) => new HttpError(status, message)