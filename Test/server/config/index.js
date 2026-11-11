require('dotenv').config()

const config = {
  port: parseInt(process.env.PORT, 10) || 3000,
  host: process.env.HOST || '127.0.0.1',
  projectId: process.env.PROJECT_ID,
  ak: process.env.AK,
  sk: process.env.SK,
  deviceId: process.env.DEVICE_ID,
  instanceId: process.env.INSTANCE_ID,
  endpoint: process.env.ENDPOINT || 'https://iotda.cn-north-4.myhuaweicloud.com',
  nodeEnv: process.env.NODE_ENV || 'development',
  // 登录鉴权配置
  adminUsername: process.env.ADMIN_USERNAME,
  adminPassword: process.env.ADMIN_PASSWORD,
  adminPasswordHash: process.env.ADMIN_PASSWORD_HASH,
  jwtSecret: process.env.JWT_SECRET,
  jwtExpiresIn: process.env.JWT_EXPIRES_IN || '8h',
}

// 启动时严格校验必需的环境变量
const required = ['projectId', 'ak', 'sk', 'deviceId', 'instanceId', 'adminUsername', 'jwtSecret']
const missing = required.filter((key) => !config[key])

if (missing.length > 0) {
  console.error(`缺少必需的环境变量: ${missing.join(', ')}`)
  process.exit(1)
}

// 管理员密码支持明文（自动哈希）或预生成哈希两种方式，二者至少配置其一
if (!config.adminPassword && !config.adminPasswordHash) {
  console.error('缺少管理员凭据：请设置 ADMIN_PASSWORD 或 ADMIN_PASSWORD_HASH')
  process.exit(1)
}

module.exports = config