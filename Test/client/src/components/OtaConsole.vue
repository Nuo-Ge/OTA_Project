<template>
  <div>
    <header class="topbar">
      <div class="brand">
        <h1>OTA 远程控制台</h1>
        <span
          v-if="deviceId"
          class="device"
        >
          设备 {{ deviceId }}
        </span>
      </div>
      <div class="user">
        <span
          v-if="username"
          class="username"
        >
          {{ username }}
        </span>
        <button
          type="button"
          class="logout"
          @click="$emit('logout')"
        >
          退出登录
        </button>
      </div>
    </header>

    <StatusCard
      :status="deviceStatus"
      :last-update-text="lastUpdateText"
      :device-id="deviceId"
    />

    <ProgressSteps :status="deviceStatus" />

    <UpgradePanel
      v-model:url="firmwareUrl"
      :disabled="isUpgrading"
      @upgrade="handleUpgrade"
    />

    <p
      v-if="feedback.text"
      class="feedback"
      :class="feedback.type"
    >
      {{ feedback.text }}
    </p>
  </div>
</template>

<script setup>
import StatusCard from './StatusCard.vue'
import ProgressSteps from './ProgressSteps.vue'
import UpgradePanel from './UpgradePanel.vue'
import { useOta } from '../composables/useOta'

defineProps({
  username: { type: String, default: '' },
})

defineEmits(['logout'])

const {
  deviceStatus,
  deviceId,
  firmwareUrl,
  isUpgrading,
  feedback,
  lastUpdateText,
  handleUpgrade,
} = useOta()
</script>

<style scoped>
.topbar {
  display: flex;
  justify-content: space-between;
  align-items: center;
  margin-bottom: 20px;
  text-align: left;
}

.brand h1 {
  margin: 0;
}

.device {
  color: var(--color-muted);
  font-size: 13px;
}

.user {
  display: flex;
  align-items: center;
  gap: 12px;
}

.username {
  color: var(--color-muted);
  font-size: 14px;
}

.logout {
  padding: 6px 14px;
  background: transparent;
  color: var(--color-text);
  border: 1px solid var(--color-border, #ccc);
  border-radius: 4px;
  cursor: pointer;
}

.feedback {
  padding: 12px 16px;
  border-radius: 6px;
  font-size: 14px;
  text-align: left;
}

.feedback.success {
  background: rgba(40, 167, 69, 0.1);
  color: var(--color-success);
}

.feedback.error {
  background: rgba(220, 53, 69, 0.1);
  color: var(--color-error);
}
</style>