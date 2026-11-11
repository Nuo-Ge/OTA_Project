import { ref, computed, onMounted, onUnmounted } from 'vue'
import { getOtaStatus, startUpgrade } from '../api/ota'

const DEFAULT_FIRMWARE_URL =
  'https://my-ota-firmware-2026.obs.cn-north-4.myhuaweicloud.com/App.bin'
const FAST_INTERVAL = 1000 // 活跃态：每秒轮询
const SLOW_INTERVAL = 10000 // 终态（success/failed）：每 10 秒

/**
 * 判断设备状态是否已进入终态（无需高频轮询）
 */
function isTerminal(status) {
  return status.includes('success') || status.includes('failed')
}

export function useOta() {
  const deviceStatus = ref('等待连接...')
  const deviceId = ref('')
  const lastUpdate = ref(0)
  const firmwareUrl = ref(DEFAULT_FIRMWARE_URL)
  const isUpgrading = ref(false)
  const feedback = ref({ type: '', text: '' })

  let timer = null
  let stopped = false

  const lastUpdateText = computed(() =>
    lastUpdate.value ? new Date(lastUpdate.value).toLocaleString() : '--'
  )

  async function fetchStatus() {
    try {
      const res = await getOtaStatus()
      const data = res?.data || res
      deviceStatus.value = data.status
      lastUpdate.value = data.lastUpdate
      deviceId.value = data.deviceId || ''
    } catch (e) {
      console.error('获取状态失败:', e.userMessage || e.message)
    }
  }

  async function poll() {
    if (stopped) return
    await fetchStatus()
    if (stopped) return
    const delay = isTerminal(deviceStatus.value) ? SLOW_INTERVAL : FAST_INTERVAL
    timer = setTimeout(poll, delay)
  }

  function stopPolling() {
    stopped = true
    if (timer) {
      clearTimeout(timer)
      timer = null
    }
  }

  function startPolling() {
    stopPolling()
    stopped = false
    poll()
  }

  /** 切回高频轮询（下发升级时调用） */
  function resetToFastPolling() {
    stopPolling()
    stopped = false
    timer = setTimeout(poll, FAST_INTERVAL)
  }

  function onVisibilityChange() {
    if (document.hidden) {
      stopPolling()
    } else {
      startPolling()
    }
  }

  async function handleUpgrade() {
    if (!firmwareUrl.value) {
      feedback.value = { type: 'error', text: '请输入固件链接' }
      return
    }

    isUpgrading.value = true
    feedback.value = { type: '', text: '' }

    try {
      await startUpgrade(firmwareUrl.value)
      feedback.value = { type: 'success', text: '指令下发成功，正在追踪 OTA 进度' }
      resetToFastPolling()
    } catch (e) {
      feedback.value = { type: 'error', text: e.userMessage || '下发失败' }
    } finally {
      isUpgrading.value = false
    }
  }

  onMounted(() => {
    startPolling()
    document.addEventListener('visibilitychange', onVisibilityChange)
  })

  onUnmounted(() => {
    stopPolling()
    document.removeEventListener('visibilitychange', onVisibilityChange)
  })

  return {
    deviceStatus,
    deviceId,
    firmwareUrl,
    isUpgrading,
    feedback,
    lastUpdateText,
    handleUpgrade,
  }
}