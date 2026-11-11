# OTA 远程控制台（前端）

基于 Vue 3 + Vite 的 OTA 远程控制台前端，与后端通过 REST API 交互。

## 技术栈

- Vue 3（`<script setup>` 组合式 API）
- Vite
- Axios

## 目录结构

```
client/
├── public/
├── src/
│   ├── api/          # 接口封装（request 实例 + 各业务接口）
│   ├── components/   # 展示组件（LoginForm / OtaConsole / StatusCard / UpgradePanel）
│   ├── composables/  # 组合式逻辑（useAuth 登录态 / useOta 状态与轮询）
│   ├── App.vue
│   ├── main.js
│   └── style.css     # 全局设计变量与基础样式
├── api-spec.md       # API 规范（前后端约定的唯一事实来源）
├── index.html
└── vite.config.js
```

## 开发

```bash
npm install
npm run dev
```

开发环境通过 Vite 代理将 `/api` 转发到后端（默认 `http://127.0.0.1:3000`）。

## 构建

```bash
npm run build
```

## 环境变量

复制 `.env.example` 为 `.env.development`，并配置：

| 变量 | 说明 |
|------|------|
| `VITE_API_URL` | 后端 API 前缀，默认 `/api` |

> 登录鉴权由后端负责：前端登录成功后会把 JWT 存入 `localStorage`，
> 后续请求自动附带 `Authorization: Bearer <token>`；后端管理员账号在
> `server/.env` 的 `ADMIN_USERNAME` / `ADMIN_PASSWORD` 中配置。