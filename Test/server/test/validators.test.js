const test = require('node:test')
const assert = require('node:assert/strict')

const { isValidFirmwareUrl } = require('../utils/validators')

test('接受合法的 HTTPS 链接', () => {
  assert.equal(isValidFirmwareUrl('https://example.com/fw.bin'), true)
})

test('接受合法的 HTTP 链接', () => {
  assert.equal(isValidFirmwareUrl('http://example.com/fw.bin'), true)
})

test('拒绝非 URL 字符串与空值', () => {
  assert.equal(isValidFirmwareUrl('not-a-url'), false)
  assert.equal(isValidFirmwareUrl(''), false)
  assert.equal(isValidFirmwareUrl(undefined), false)
  assert.equal(isValidFirmwareUrl(null), false)
})

test('拒绝非 http/https 协议', () => {
  assert.equal(isValidFirmwareUrl('ftp://example.com/fw.bin'), false)
})