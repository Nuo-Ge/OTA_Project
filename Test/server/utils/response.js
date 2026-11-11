/**
 * 统一 API 响应信封（与 api-spec.md 保持一致）
 *
 * 成功: { code: 200, message: 'ok', data: {...} }
 * 失败: { code: <httpStatus>, message: '...', data: null }
 */

function success(res, data = null, message = 'ok') {
  return res.status(200).json({ code: 200, message, data })
}

function fail(res, httpStatus, message, detail) {
  const body = { code: httpStatus, message, data: null }
  if (detail !== undefined) {
    body.detail = detail
  }
  return res.status(httpStatus).json(body)
}

module.exports = { success, fail }