/**
 * 校验固件下载地址是否为合法的 HTTP/HTTPS 链接
 * @param {string} value
 * @returns {boolean}
 */
function isValidFirmwareUrl(value) {
  if (typeof value !== 'string' || value.trim() === '') {
    return false
  }

  let url
  try {
    url = new URL(value)
  } catch {
    return false
  }

  return url.protocol === 'http:' || url.protocol === 'https:'
}

module.exports = { isValidFirmwareUrl }