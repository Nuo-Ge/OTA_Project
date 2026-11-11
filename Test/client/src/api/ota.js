import request from './request'

export function getOtaStatus() {
  return request.get('/ota/status')
}

export function startUpgrade(firmwareUrl) {
  return request.post('/ota/upgrade', { firmwareUrl })
}

export function getDeviceShadow() {
  return request.get('/ota/shadow')
}