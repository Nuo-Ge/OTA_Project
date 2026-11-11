/**
 * 设备状态内存缓存（进程级单例）
 * 作为华为云设备影子不可用时的降级数据源
 */
const initialState = { status: 'idle', lastUpdate: Date.now() }

const state = { ...initialState }

function getState() {
  return { ...state }
}

function updateState(partial) {
  Object.assign(state, partial, { lastUpdate: Date.now() })
  return getState()
}

module.exports = { getState, updateState }