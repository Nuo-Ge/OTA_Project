const app = require('./app')
const config = require('./config')
const logger = require('./utils/logger')

app.listen(config.port, config.host, () => {
  logger.info(`后端服务已启动: http://${config.host}:${config.port}`)
})