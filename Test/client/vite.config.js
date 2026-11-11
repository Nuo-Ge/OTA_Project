import vue from '@vitejs/plugin-vue'
import { defineConfig, loadEnv } from 'vite'

// https://vite.dev/config/
export default defineConfig(({ mode }) => {
  const env = loadEnv(mode, process.cwd(), '')
  const apiUrl = env.VITE_API_URL || '/api'

  return {
    plugins: [vue()],
    server: {
      proxy: {
        [apiUrl]: {
          target: 'http://127.0.0.1:3000',
          changeOrigin: true,
        }
      }
    }
  }
})