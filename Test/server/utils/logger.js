/**
 * 轻量结构化日志，统一时间戳与级别前缀
 */
function format(level, args) {
  const timestamp = new Date().toISOString()
  return [`[${timestamp}] [${level}]`, ...args]
}

module.exports = {
  info: (...args) => console.log(...format('INFO', args)),
  warn: (...args) => console.warn(...format('WARN', args)),
  error: (...args) => console.error(...format('ERROR', args)),
}