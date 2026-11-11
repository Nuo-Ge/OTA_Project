const test = require('node:test')
const assert = require('node:assert/strict')

const { signToken, verifyToken } = require('../services/auth')

test('signToken / verifyToken 往返校验', () => {
  const secret = 'test-secret'
  const token = signToken('admin', secret, '1h')
  const payload = verifyToken(token, secret)

  assert.equal(payload.username, 'admin')
})

test('verifyToken 拒绝无效 token', () => {
  assert.throws(
    () => verifyToken('invalid-token', 'test-secret'),
    /登录已过期/,
  )
})

test('verifyToken 拒绝用错误密钥签发的 token', () => {
  const token = signToken('admin', 'secret-a', '1h')
  assert.throws(
    () => verifyToken(token, 'secret-b'),
    /登录已过期/,
  )
})