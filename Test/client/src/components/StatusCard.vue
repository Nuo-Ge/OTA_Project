<template>
  <div class="card status-card">
    <div class="status-header">
      <div>
        <h2>设备状态</h2>
        <small
          v-if="deviceId"
          class="device-id"
        >
          设备 ID：{{ deviceId }}
        </small>
      </div>
      <span
        class="badge"
        :class="meta.tone"
      >
        {{ meta.label }}
      </span>
    </div>

    <div class="status-body">
      <span
        class="dot"
        :class="meta.tone"
      />
      <span class="status-text">{{ meta.label }}</span>
    </div>

    <div class="progress-track">
      <div
        class="progress-bar"
        :class="{ indeterminate: isIndeterminate }"
        :style="barStyle"
      />
    </div>

    <small class="last-update">最后更新：{{ lastUpdateText }}</small>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { parseStatus } from '../utils/status'

const props = defineProps({
  status: { type: String, required: true },
  lastUpdateText: { type: String, required: true },
  deviceId: { type: String, default: '' },
})

const meta = computed(() => parseStatus(props.status))
const isIndeterminate = computed(() =>
  (props.status || '').toLowerCase().includes('downloading'),
)

const barStyle = computed(() => {
  if (isIndeterminate.value) return {}
  return { width: `${meta.value.progress}%` }
})
</script>

<style scoped>
.card {
  background: var(--color-card-bg, #fff);
  padding: 20px;
  border-radius: 8px;
  margin-bottom: 20px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
  text-align: left;
}

.status-header {
  display: flex;
  justify-content: space-between;
  align-items: flex-start;
  margin-bottom: 14px;
}

.device-id {
  color: var(--color-muted);
}

.badge {
  padding: 4px 10px;
  border-radius: 999px;
  font-size: 13px;
  font-weight: 600;
}

.badge.idle {
  background: rgba(108, 117, 125, 0.12);
  color: var(--color-muted);
}

.badge.progress {
  background: rgba(255, 193, 7, 0.15);
  color: #b8860b;
}

.badge.success {
  background: rgba(40, 167, 69, 0.12);
  color: var(--color-success);
}

.badge.error {
  background: rgba(220, 53, 69, 0.12);
  color: var(--color-error);
}

.status-body {
  display: flex;
  align-items: center;
  gap: 8px;
  margin-bottom: 14px;
}

.dot {
  width: 12px;
  height: 12px;
  border-radius: 50%;
  background: var(--color-muted);
  flex-shrink: 0;
}

.dot.progress {
  background: var(--color-progress);
}

.dot.success {
  background: var(--color-success);
}

.dot.error {
  background: var(--color-error);
}

.status-text {
  font-size: 20px;
  font-weight: 600;
}

.progress-track {
  height: 8px;
  background: var(--color-border, #e5e4e7);
  border-radius: 999px;
  overflow: hidden;
  margin-bottom: 8px;
}

.progress-bar {
  height: 100%;
  background: var(--color-primary);
  border-radius: 999px;
  transition: width 0.4s ease;
}

.progress-bar.indeterminate {
  width: 40%;
  background: linear-gradient(90deg, var(--color-progress), var(--color-primary));
  animation: loading 1.2s infinite linear;
}

@keyframes loading {
  from {
    transform: translateX(-100%);
  }

  to {
    transform: translateX(250%);
  }
}

.last-update {
  color: var(--color-muted);
  font-size: 12px;
}
</style>