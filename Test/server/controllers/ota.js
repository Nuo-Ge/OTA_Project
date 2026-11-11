const { getDeviceShadow, sendOtaCommand } = require('../services/iotda')
const { getState, updateState } = require('../services/deviceState')
const config = require('../config')
const asyncHandler = require('../utils/asyncHandler')
const httpError = require('../utils/httpError')
const { isValidFirmwareUrl } = require('../utils/validators')
const { success } = require('../utils/response')
const logger = require('../utils/logger')

/**
 * GET /api/ota/status
 * 自动从华为云设备影子同步真实状态，失败时兜底内存缓存
 */
const getStatus = asyncHandler(async (req, res) => {
  try {
    const props = await getDeviceShadow()
    if (props.status) {
      updateState({ status: props.status })
    }
  } catch (err) {
    logger.warn('查询设备影子失败，回退到本地缓存:', err.message)
  }

  return success(res, { ...getState(), deviceId: config.deviceId })
})

/**
 * POST /api/ota/upgrade
 * 校验参数后，通过华为云 REST API 向设备下发 START_OTA 命令
 */
const startUpgrade = asyncHandler(async (req, res) => {
  const { firmwareUrl } = req.body || {}

  if (!isValidFirmwareUrl(firmwareUrl)) {
    throw httpError(400, 'firmwareUrl 必须是合法的 HTTP/HTTPS 链接')
  }

  await sendOtaCommand(firmwareUrl)

  const state = updateState({ status: 'downloading' })
  return success(res, state, '升级指令已下发')
})

/**
 * GET /api/ota/shadow
 * 直接返回设备影子原始数据（调试用）
 */
const getShadow = asyncHandler(async (req, res) => {
  const props = await getDeviceShadow()
  return success(res, { shadow: props, cached: getState() })
})

module.exports = { getStatus, startUpgrade, getShadow }