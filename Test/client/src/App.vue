<template>
  <div class="container">
    <OtaConsole
      v-if="isAuthenticated"
      :username="user?.username"
      @logout="logout"
    />
    <LoginForm v-else />
  </div>
</template>

<script setup>
import { onMounted } from 'vue'
import OtaConsole from './components/OtaConsole.vue'
import LoginForm from './components/LoginForm.vue'
import { useAuth } from './composables/useAuth'

const { user, isAuthenticated, logout, restoreSession } = useAuth()

onMounted(restoreSession)
</script>

<style scoped>
.container {
  max-width: 640px;
  margin: 48px auto;
  padding: 0 16px;
  text-align: center;
}
</style>