<template>
  <form
    class="card upgrade-card"
    @submit.prevent="$emit('upgrade')"
  >
    <h2>下发固件</h2>
    <input
      :value="url"
      type="url"
      required
      placeholder="输入 OBS 固件下载链接"
      @input="$emit('update:url', $event.target.value)"
    >
    <button
      type="submit"
      :disabled="disabled"
    >
      {{ disabled ? '下发中...' : '一键下发升级' }}
    </button>
    <small class="hint">请填写合法的 HTTP/HTTPS 固件链接，下发后页面将自动追踪升级进度。</small>
  </form>
</template>

<script setup>
defineProps({
  url: { type: String, required: true },
  disabled: { type: Boolean, default: false },
})

defineEmits(['update:url', 'upgrade'])
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

input {
  width: 100%;
  padding: 10px;
  margin-bottom: 15px;
  border: 1px solid var(--color-border, #ccc);
  border-radius: 4px;
}

button {
  padding: 10px 20px;
  background: var(--color-primary, #0066cc);
  color: white;
  border: none;
  border-radius: 4px;
  cursor: pointer;
}

button:disabled {
  background: #ccc;
  cursor: not-allowed;
}

.hint {
  display: block;
  margin-top: 12px;
  color: var(--color-muted);
  font-size: 12px;
}
</style>