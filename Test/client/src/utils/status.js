/**
 * 设备状态解析：把后端返回的状态字符串映射为
 * 展示文案、进度百分比、流程步骤与语义色调（tone）。
 * 纯函数，便于复用与单元测试。
 */

/**
 * @param {string} status 设备状态字符串
 * @returns {{label: string, progress: number, step: number, tone: string}}
 */
export function parseStatus(status) {
  const s = (status || '').toLowerCase()

  if (s.includes('success')) {
    return { label: '升级成功', progress: 100, step: 3, tone: 'success' }
  }
  if (s.includes('failed')) {
    return { label: '升级失败', progress: 0, step: 3, tone: 'error' }
  }
  if (s.includes('flashing')) {
    const match = s.match(/(\d+)%/)
    const percent = match ? Number(match[1]) : 0
    return { label: `正在烧录 ${percent}%`, progress: percent, step: 2, tone: 'progress' }
  }
  if (s.includes('downloading')) {
    return { label: '正在下载固件', progress: 0, step: 1, tone: 'progress' }
  }
  return { label: '等待升级', progress: 0, step: 0, tone: 'idle' }
}