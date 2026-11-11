# OTA 远程控制台 API 规范

> 本文档是前后端约定的唯一事实来源（Single Source of Truth）。
> 任何接口变更必须先更新本文档，再实现前后端代码。

## 通用约定

- 基础路径：`/api`
- 鉴权：除「登录」接口外，所有 `/api` 请求必须携带请求头
  `Authorization: Bearer <token>`，`token` 由登录接口签发
- 请求/响应体：`application/json`

### 统一响应信封

成功（HTTP 200）：

```json
{
  "code": 200,
  "message": "ok",
  "data": {}
}
```

失败（HTTP 4xx/5xx）：

```json
{
  "code": 400,
  "message": "错误描述",
  "data": null
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| code | number | 与 HTTP 状态码一致，`200` 表示成功 |
| message | string | 人类可读的提示信息 |
| data | object / null | 业务数据，失败时为 `null` |

> 开发环境（`NODE_ENV=development`）的失败响应会额外携带 `detail` 字段用于排查。

### 错误码

| HTTP | code | 说明 |
|------|------|------|
| 400 | 400 | 参数校验失败 |
| 401 | 401 | 未登录或登录已过期 |
| 404 | 404 | 路由不存在 |
| 500 | 500 | 服务器内部错误 |

### 设备状态枚举

`idle` / `downloading` / `flashing_XX%` / `success` / `failed`

## 1. 登录

- **路径**：`POST /api/auth/login`（无需鉴权）
- **请求体**：

```json
{
  "username": "admin",
  "password": "your-password"
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| username | string | 是 | 管理员用户名 |
| password | string | 是 | 管理员密码 |

- **成功响应**：

```json
{
  "code": 200,
  "message": "登录成功",
  "data": {
    "token": "eyJhbGciOiJIUzI1NiIs...",
    "expiresIn": "8h",
    "username": "admin"
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| token | string | JWT，后续请求放入 `Authorization: Bearer <token>` |
| expiresIn | string | token 有效期 |
| username | string | 登录用户名 |

- **失败**：用户名或密码错误时返回 `401`，`message` 为「用户名或密码错误」

## 2. 获取当前用户

- **路径**：`GET /api/auth/me`（需鉴权）
- **成功响应**：

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "username": "admin"
  }
}
```

## 3. 获取设备状态

- **路径**：`GET /api/ota/status`（需鉴权）
- **响应**：`data` 结构如下

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "status": "flashing_45%",
    "lastUpdate": 1699024567000,
    "deviceId": "6ae93760cbb0cf6bb993bc20_esp32_001"
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| status | string | 设备状态，取值见「设备状态枚举」 |
| lastUpdate | number | 最后一次状态更新时间戳（ms） |
| deviceId | string | 设备 ID（用于界面展示） |

## 4. 下发升级指令

- **路径**：`POST /api/ota/upgrade`（需鉴权）
- **请求体**：

```json
{
  "firmwareUrl": "https://my-ota-firmware-2026.obs.cn-north-4.myhuaweicloud.com/App.bin"
}
```

| 字段 | 类型 | 必填 | 说明 |
|------|------|------|------|
| firmwareUrl | string | 是 | 固件 OBS 下载地址，必须为合法的 HTTP/HTTPS 链接 |

- **成功响应**：

```json
{
  "code": 200,
  "message": "升级指令已下发",
  "data": {
    "status": "downloading",
    "lastUpdate": 1699024567000
  }
}
```

## 5. 查询设备影子（调试）

- **路径**：`GET /api/ota/shadow`（需鉴权）
- **响应**：`data` 结构如下

```json
{
  "code": 200,
  "message": "ok",
  "data": {
    "shadow": {
      "status": "success"
    },
    "cached": {
      "status": "success",
      "lastUpdate": 1699024567000
    }
  }
}
```

| 字段 | 类型 | 说明 |
|------|------|------|
| shadow | object | 华为云设备影子中 OTA 服务上报的属性 |
| cached | object | 后端内存缓存中的设备状态 |