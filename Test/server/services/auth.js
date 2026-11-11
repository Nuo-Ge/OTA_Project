const bcrypt = require('bcryptjs')
const jwt = require('jsonwebtoken')

const config = require('../config')
const httpError = require('../utils/httpError')

const SALT_ROUNDS = 10

// 启动时确定密码哈希：优先使用预生成的哈希，否则由明文派生（不保留明文）
let passwordHash = config.adminPasswordHash
if (!passwordHash) {
  passwordHash = bcrypt.hashSync(config.adminPassword, SALT_ROUNDS)
}

/**
 * 校验用户名与密码
 * @param {string} username
 * @param {string} password
 * @returns {Promise<{token: string, expiresIn: string, username: string}>}
 */
async function authenticate(username, password) {
  // 无论用户名是否正确都执行 bcrypt 比较，降低时序侧信道泄露用户名的风险
  const [passwordOk, userOk] = await Promise.all([
    bcrypt.compare(password, passwordHash),
    Promise.resolve(username === config.adminUsername),
  ])

  if (!userOk || !passwordOk) {
    throw httpError(401, '用户名或密码错误')
  }

  return {
    token: signToken(username, config.jwtSecret, config.jwtExpiresIn),
    expiresIn: config.jwtExpiresIn,
    username,
  }
}

/**
 * 签发 JWT
 */
function signToken(username, secret, expiresIn) {
  return jwt.sign({ username }, secret, { expiresIn })
}

/**
 * 校验 JWT，失败时抛出 401 错误
 */
function verifyToken(token, secret) {
  try {
    return jwt.verify(token, secret)
  } catch {
    throw httpError(401, '登录已过期，请重新登录')
  }
}

module.exports = { authenticate, signToken, verifyToken }