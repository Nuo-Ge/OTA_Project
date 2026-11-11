<template>
  <div class="card steps-card">
    <h2>升级流程</h2>
    <ol class="steps">
      <li
        v-for="(step, i) in steps"
        :key="step"
        class="step"
        :class="stepClass(i)"
      >
        <span class="step-index">{{ stepIcon(i) }}</span>
        <span class="step-name">{{ step }}</span>
      </li>
    </ol>
  </div>
</template>

<script setup>
import { computed } from 'vue'
import { parseStatus } from '../utils/status'

const props = defineProps({
  status: { type: String, required: true },
})

const steps = ['等待', '下载', '烧录', '完成']

const meta = computed(() => parseStatus(props.status))

function stepClass(i) {
  if (meta.value.tone === 'error' && i === 3) return 'error'
  if (meta.value.tone === 'success') return 'done'
  if (i < meta.value.step) return 'done'
  if (i === meta.value.step) return 'active'
  return ''
}

function stepIcon(i) {
  if (meta.value.tone === 'error' && i === 3) return '✕'
  if (meta.value.tone === 'success' || i < meta.value.step) return '✓'
  return i + 1
}
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

.steps {
  display: flex;
  list-style: none;
  padding: 0;
  margin: 8px 0 0;
}

.step {
  flex: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 6px;
  position: relative;
  color: var(--color-muted);
}

.step::before {
  content: '';
  position: absolute;
  top: 14px;
  left: -50%;
  width: 100%;
  height: 2px;
  background: var(--color-border, #e5e4e7);
}

.step:first-child::before {
  display: none;
}

.step-index {
  width: 28px;
  height: 28px;
  border-radius: 50%;
  background: var(--color-bg, #f5f6f8);
  border: 2px solid var(--color-border, #e5e4e7);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 13px;
  position: relative;
  z-index: 1;
}

.step.done::before {
  background: var(--color-success);
}

.step.done .step-index {
  background: var(--color-success);
  border-color: var(--color-success);
  color: #fff;
}

.step.active .step-index {
  background: var(--color-primary);
  border-color: var(--color-primary);
  color: #fff;
}

.step.error .step-index {
  background: var(--color-error);
  border-color: var(--color-error);
  color: #fff;
}

.step-name {
  font-size: 13px;
}

.step.active .step-name {
  color: var(--color-text);
  font-weight: 600;
}
</style>