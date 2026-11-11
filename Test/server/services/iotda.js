const core = require('@huaweicloud/huaweicloud-sdk-core')
const {
  IoTDAClient,
  CreateCommandRequest,
  DeviceCommandRequest,
  ShowDeviceShadowRequest,
} = require('@huaweicloud/huaweicloud-sdk-iotda')

const config = require('../config')

// 构建认证凭据（衍生签名，适配专享实例）
const credentials = new core.BasicCredentials()
  .withAk(config.ak)
  .withSk(config.sk)
  .withProjectId(config.projectId)
  .withDerivedPredicate(
    core.BasicCredentials.getDefaultDerivedPredicate.bind(core.BasicCredentials)
  )

// 单例客户端
const client = IoTDAClient.newBuilder()
  .withCredential(credentials)
  .withEndpoint(config.endpoint)
  .build()

/**
 * 查询设备影子
 * @returns {Object} OTA 服务的 reported.properties，无数据时返回 {}
 */
async function getDeviceShadow() {
  const request = new ShowDeviceShadowRequest(config.deviceId)
  request.instanceId = config.instanceId

  const response = await client.showDeviceShadow(request)

  const otaService = (response.shadow || []).find(
    (s) => s.service_id === 'OTA'
  )

  if (!otaService || !otaService.reported || !otaService.reported.properties) {
    return {}
  }

  return otaService.reported.properties
}

/**
 * 下发 OTA 升级指令
 * @param {string} firmwareUrl - 固件 OBS 下载地址
 * @returns {Object} 华为云 API 响应
 */
async function sendOtaCommand(firmwareUrl) {
  const request = new CreateCommandRequest()
  request.deviceId = config.deviceId
  request.headers = { 'Instance-Id': config.instanceId }

  const body = new DeviceCommandRequest()
  body.serviceId = 'OTA'
  body.commandName = 'START_OTA'
  body.paras = { url: firmwareUrl }

  request.body = body

  const response = await client.createCommand(request)
  console.log('✅ 华为云 API 响应:', JSON.stringify(response))
  return response
}

module.exports = { getDeviceShadow, sendOtaCommand }