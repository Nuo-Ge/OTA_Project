<template>
  <form
    class="login-card"
    @submit.prevent="handleSubmit"
  >
    <h1>OTA 远程控制台</h1>
    <input
      v-model="username"
      type="text"
      required
      placeholder="用户名"
      autocomplete="username"
    >
    <input
      v-model="password"
      type="password"
      required
      placeholder="密码"
      autocomplete="current-password"
    >
    <button
      type="submit"
      :disabled="loading"
    >
      {{ loading ? '登录中...' : '登录' }}
    </button>
    <p
      v-if="error"
      class="error"
    >
      {{ error }}
    </p>
  </form>
</template>

<script setup>
import { ref } from 'vue'
import { useAuth } from '../composables/useAuth'

const { login } = useAuth()

const username = ref('')
const password = ref('')
const loading = ref(false)
const error = ref('')

async function handleSubmit() {
  loading.value = true
  error.value = ''
  try {
    await login(username.value, password.value)
  } catch (e) {
    error.value = e.userMessage || '登录失败'
  } finally {
    loading.value = false
  }
}
</script>

<style scoped>
.login-card {
  display: flex;
  flex-direction: column;
  gap: 12px;
  max-width: 320px;
  margin: 0 auto;
  padding: 24px;
  background: var(--color-card-bg, #fff);
  border-radius: 8px;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.08);
}

input {
  width: 100%;
  padding: 10px;
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

.error {
  color: var(--color-error);
  font-size: 14px;
}
</style>