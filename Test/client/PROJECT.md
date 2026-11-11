# OTA 远程控制台 · 项目说明文档

> 本文档对整个项目做统一说明（涵盖 `server` 与 `client` 两端）。
> 因 IDE 当前限制了根目录写入，本文件暂存于 `client/`，可自行移动到
> 项目根目录 `d:\SchoolLearn\java\Test\README.md`。

## 一、项目简介

基于 **华为云 IoTDA** 的 ESP32 设备 OTA 远程升级控制台，采用「前端 Vue 3 + 后端 Express」前后端分离架构，遵循 **SDD（规范驱动开发）** 与工业化工程实践。

- **目标**：通过网页远程向 ESP32 设备下发固件升级指令，并实时追踪升级进度
- **数据链路**：浏览器 → Express 后端 → 华为云 IoTDA REST API → 设备（ESP32）
- **降级策略**：华为云设备影子不可用时，后端自动回退到本地内存缓存，保证页面可用

## 二、技术栈

| 端 | 技术 |
|----|------|
| 前端 | Vue 3（`<script setup>`）、Vite、Axios |
| 后端 | Node.js、Express 4、华为云 IoTDA SDK、JSON Web Token |
| 工程化 | ESLint（flat config）、Prettier、dotenv、Node 内置测试（`node --test`） |

## 三、目录结构

```
d:\SchoolLearn\java\Test\
├── server/                    # 后端服务
│   ├── config/                # 环境变量读取与启动校验
│   ├── controllers/           # 控制器（ota / auth）
│   ├── middleware/            # 中间件（鉴权 / 404 / 错误处理）
│   ├── routes/                # 路由（业务 / auth）
│   ├── services/              # 业务服务（IoTDA / 设备状态 / 鉴权）
│   ├── utils/                 # 通用工具（响应信封 / 异步包装 / 校验 / 日志）
│   ├── test/                  # 单元测试
│   ├── app.js                 # 组装 Express 应用（可测试、可导入）
│   ├── server.js              # 启动入口
│   ├── .env.example           # 环境变量模板
│   └── eslint.config.js / .prettierrc
│
└── client/                    # 前端应用
    ├── src/
    │   ├── api/               # 接口封装（request 实例 / auth / ota）
    │   ├── components/        # 展示组件（登录 / 控制台 / 状态 / 流程 / 下发）
    │   ├── composables/       # 组合式逻辑（useAuth / useOta）
    │   ├── utils/             # 纯函数工具（状态解析）
    │   ├── App.vue / main.js / style.css
    ├── api-spec.md            # API 规范（唯一事实来源）
    ├── .env.example
    └── vite.config.js / eslint.config.js / .prettierrc
```

## 四、SDD 规范驱动开发

- 前后端以 `client/api-spec.md` 为**唯一事实来源**，接口变更必须先更新文档、再实现代码
- 统一响应信封：`{ code, message, data }`，`code` 与 HTTP 状态码一致
- 统一错误码：`400` 参数错误 / `401` 未登录 / `404` 路由不存在 / `500` 服务错误

## 五、接口一览

| 方法 | 路径 | 鉴权 | 说明 |
|------|------|------|------|
| POST | `/api/auth/login` | 否 | 登录，签发 JWT |
| GET | `/api/auth/me` | 是 | 获取当前用户 |
| GET | `/api/ota/status` | 是 | 获取设备状态与设备 ID |
| POST | `/api/ota/upgrade` | 是 | 下发固件升级指令 |
| GET | `/api/ota/shadow` | 是 | 查询设备影子（调试） |

设备状态枚举：`idle` / `downloading` / `flashing_XX%` / `success` / `failed`。

## 六、鉴权设计

- 登录接口校验用户名密码（`bcryptjs` 哈希，常量时间比较，防时序攻击），成功后签发 JWT
- 除登录外，所有 `/api` 请求需携带 `Authorization: Bearer <token>`
- 前端登录态存于 `localStorage`，请求拦截器自动附带 token；401 时自动清除并回到登录页
- 支持 `ADMIN_PASSWORD`（明文，启动时哈希）或 `ADMIN_PASSWORD_HASH`（预生成哈希）两种配置

## 七、后端分层架构

```
routes → controllers → services → utils
              ↑
         middleware（鉴权 / 错误处理）
```

- `controllers` 只做参数校验与响应组装，业务逻辑下沉到 `services`
- 统一 `asyncHandler` 捕获异步异常，`httpError` 抛出结构化错误
- `deviceState` 为进程级内存缓存，作为设备影子不可用时的降级数据源

## 八、前端组合式架构

- `useAuth`：模块级单例登录态（token / user / 登录 / 退出 / 会话恢复）
- `useOta`：设备状态轮询 + 下发逻辑，动态调频（活跃态 1s、终态 10s），页面可见性管理，防定时器泄漏
- `utils/status.js`：纯函数 `parseStatus` 把状态串映射为文案 / 进度 / 步骤 / 色调
- 展示组件与业务逻辑分离：`StatusCard` / `ProgressSteps` / `UpgradePanel` 均为纯展示

## 九、环境变量

### server/.env

| 变量 | 必填 | 说明 |
|------|------|------|
| `PORT` / `HOST` | 否 | 监听端口 / 地址 |
| `PROJECT_ID` / `AK` / `SK` | 是 | 华为云 IoTDA 项目与 AK/SK |
| `INSTANCE_ID` / `ENDPOINT` | 是 | 实例 ID 与接入地址 |
| `DEVICE_ID` | 是 | 设备 ID |
| `ADMIN_USERNAME` | 是 | 管理员用户名 |
| `ADMIN_PASSWORD` 或 `ADMIN_PASSWORD_HASH` | 是 | 管理员密码（二选一） |
| `JWT_SECRET` | 是 | JWT 签名密钥 |
| `JWT_EXPIRES_IN` | 否 | token 有效期，默认 `8h` |

### client/.env

| 变量 | 必填 | 说明 |
|------|------|------|
| `VITE_API_URL` | 否 | 后端 API 前缀，默认 `/api` |

> 参考 `server/.env.example` 与 `client/.env.example` 复制配置。**切勿提交真实 AK/SK 到版本库。**

## 十、本地开发

```bash
# 后端（默认 http://127.0.0.1:3000）
cd server
npm install
npm run dev        # 开发热重载；生产用 npm start

# 前端（Vite 已配置 /api 代理到后端）
cd client
npm install
npm run dev
```

## 十一、测试与代码质量

```bash
cd server
npm run lint       # ESLint
npm test           # 单元测试（node --test，9 个用例）
npm run format     # Prettier 格式化

cd client
npm run lint
npm run build      # 构建产物输出到 client/dist
npm run format
```

## 十二、构建与部署

1. 前端 `npm run build` 生成 `client/dist`
2. 将 `client/dist` 产物放入 `server/dist`（后端已配置静态托管与 SPA 回退）
3. 配置好 `server/.env` 后 `cd server && npm start`

## 十三、后续优化方向

- **鉴权增强**：多用户、角色权限；接入 `helmet`、`express-rate-limit` 限流
- **状态持久化**：接入 MQTT 订阅设备上报（依赖已引入 `mqtt`），或用 Redis 缓存
- **可观测性**：结构化日志（pino/winston）、`/healthz` 健康检查、APM
- **可靠性**：下发幂等（requestId）、优雅停机、重试机制
- **工程化**：集成测试（supertest）、前端单测（Vitest）、CI/CD、TypeScript