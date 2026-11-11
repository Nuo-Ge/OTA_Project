const test = require('node:test')
const assert = require('node:assert/strict')

const { getState, updateState } = require('../services/deviceState')

test('初始状态为 idle 且 lastUpdate 为数字时间戳', () => {
  const state = getState()
  assert.equal(state.status, 'idle')
  assert.equal(typeof state.lastUpdate, 'number')
})

test('updateState 更新状态并刷新 lastUpdate', async () => {
  const before = getState().lastUpdate
  await new Promise((resolve) => setTimeout(resolve, 5))

  const updated = updateState({ status: 'downloading' })
  assert.equal(updated.status, 'downloading')
  assert.ok(updated.lastUpdate > before)
})