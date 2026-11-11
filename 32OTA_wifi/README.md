# ESP32-S3 华为云 IoTDA 远程 OTA + 串口升级 STM32

基于 ESP32-S3 的远程固件升级系统，对接华为云 IoTDA 平台，实现云端下发 OTA 指令 → ESP32 自动从华为云 OBS 下载固件 → 通过 UART 串口升级 STM32 设备的完整闭环。

- **平台**: ESP-IDF v6.1
- **芯片**: ESP32-S3
- **云端**: 华为云 IoTDA（设备接入）+ OBS（对象存储）
- **目标**: STM32 MCU（通过 UART 串口升级）

---

## 目录结构

```
32OTA_wifi/
├── CMakeLists.txt                  # 顶层构建脚本（排除旧版 wifi_provisioning）
├── sdkconfig.defaults              # 默认 sdkconfig（分区表、组件配置）
├── partitions.csv                  # 分区表（factory 3M / nvs / phy_init）
├── main/
│   ├── main.c                       # 应用入口，总调度
│   ├── cloud_mqtt.c                 # 华为云 MQTT 通信模块
│   ├── cloud_mqtt.h
│   ├── ota_config.h                 # 硬件引脚、协议常量、时序参数
│   ├── Kconfig.projbuild            # menuconfig 云端配置项（设备信息/密钥/地址）
│   ├── CMakeLists.txt               # 构建配置
│   ├── idf_component.yml            # 组件依赖声明
│   ├── wifi/
│   │   ├── wifi_manager.c           # Wi-Fi STA 连接管理
│   │   └── wifi_manager.h
│   ├── source/
│   │   ├── ota_source.h             # 数据源抽象接口
│   │   ├── ota_source.c             # Flash 内嵌固件源
│   │   ├── ota_source_http.c        # HTTP 从 OBS 下载固件
│   │   └── ota_source_http.h
│   ├── protocol/
│   │   ├── ota_protocol.c           # 二进制帧协议 + CRC32 + ACK 状态机
│   │   └── ota_protocol.h
│   ├── transport/
│   │   ├── ota_transport.c          # UART 物理层收发
│   │   └── ota_transport.h
│   └── task/
│       ├── ota_task.c               # OTA 任务调度
│       └── ota_task.h
├── firmware/
│   └── App.bin                      # STM32 目标固件（编译时嵌入，本地 OTA 用）
├── test/
│   └── test_ota_protocol.c          # 主机侧单元测试（CRC32/帧打包/ACK 状态机）
├── managed_components/              # ESP-IDF 组件管理器自动下载
└── README.md
```

---

## 架构分层

```
┌─────────────────────────────────────────────────┐
│  main.c                应用层调度                 │
│  cloud_mqtt.c          华为云 IoTDA MQTT 对接     │
├─────────────────────────────────────────────────┤
│  task/ota_task.c       OTA 任务编排               │
│  source/ota_source_http.c  HTTP 固件下载         │
├─────────────────────────────────────────────────┤
│  protocol/ota_protocol.c  二进制帧协议 + CRC32    │
│  transport/ota_transport.c UART 物理层收发        │
├─────────────────────────────────────────────────┤
│  wifi/wifi_manager.c   Wi-Fi 网络连接             │
│  ota_config.h          硬件引脚 / 时序常量         │
└─────────────────────────────────────────────────┘
```

---

## 完整运行流程

```
1. NVS 初始化              nvs_flash_init()
2. UART 初始化              ota_transport_init()     GPIO5(TX) / GPIO6(RX) @115200
3. Wi-Fi 连接               wifi_init_sta()          凭据连接 30s 超时，失败自动回退 BLE 配网（闭环循环直到成功）
4. NTP 时间同步              esp_sntp_init()          轮询校验 tm_year ≥ 2024，20 次重试
         │
5. MQTT 连接华为云           cloud_mqtt_init()        TLS 8883 + HMAC 动态鉴权
6. 订阅命令下发主题          $oc/devices/{id}/sys/commands/#
7. 上报 idle 状态            {"services":[{"service_id":"OTA","properties":{"status":"idle"}}]}
         │
    ┌──── 等待循环 ─────────────────────────────────────┐
    │  cloud_mqtt_wait_for_ota_url() 阻塞在 FreeRTOS     │
    │  Queue 等待云端下发 OTA 指令                       │
    │  收到后解析 JSON 中的 "url" 字段                    │
    └───────────────────────────────────────────────────┘
         │
8. 上报 downloading 状态
9. HTTP 下载固件             ota_http_download_firmware()
                             OBS → PSRAM（通过 esp_http_client）
         │
10. OTA 串口传输 ─────────────────────────────────────┐
    │  握手: LINE_CLEAN → WAKEUP_CMD(×2) → PREPARE帧  │
    │  传输: DATA 帧 1024B/包 → CRC32 → 等 ACK → 循环  │
    │  结束: FINISH 帧 → 等最终 ACK                    │
    └──────────────────────────────────────────────────┘
         │
11. 上报 success 状态
```

---

## Wi-Fi 配网与自动回退（BLE）

Wi-Fi 凭据不写死在源码中，通过 BLE 动态配网由手机 APP 下发。采用 ESP-IDF v6.1 的 `network_provisioning` 组件（`NETWORK_PROV_SCHEME_BLE`）。

### 触发配网的三种场景

| 场景 | 行为 |
|------|------|
| 首次上电（NVS 无凭据） | 直接进入 BLE 配网 |
| 已有凭据但连接失败 | 尝试连接 30 秒超时 → 自动回退 BLE 配网 |
| 配网成功后需更换网络 | BOOT 键（GPIO0）长按 3 秒 → 擦除凭据 → 重启进入配网 |

### 连接闭环

`wifi_init_sta()` 内部为 `while (!connected)` 循环，**永不因连接失败而返回 false**：

```
检测 NVS 凭据
  ├─ 无凭据 ──────────────────────────┐
  └─ 有凭据 → 连接 AP（30s 超时）       │
              ├─ 成功 → 获取 IP → 返回  │
              └─ 失败/超时 ─────────────┤
                                       ▼
                        start_ble_provisioning()
                            │ 置 s_provisioning 标志抑制自动重连
                            │ 主动 esp_wifi_disconnect() + 等断开事件落地
                            │ network_prov_mgr_start_provisioning()
                            │ network_prov_mgr_wait()   ← 阻塞直到 APP 配网结束
                            │ network_prov_mgr_stop_provisioning()
                            ▼
                       回到循环顶部重新检测凭据并连接
```

### 配网资源释放

- 使用 `NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM` 方案：配网结束后 `network_prov_mgr_deinit()` **100% 释放 BLE/BT 协议栈**，避免与后续 HTTPS/TLS/OTA 流程争用资源或死锁。
- 进入配网前设置 `s_provisioning` 标志抑制断线自动重连，并主动断开 + 等待 STA 退出 connecting 状态，避免 `esp_wifi_set_config()` 被拒绝（`ESP_ERR_WIFI_STATE`）。

### 重新配网按键

| 项目 | 值 |
|------|-----|
| 按键 | BOOT（GPIO0），低电平有效 |
| 长按时长 | 3000 ms |
| 动作 | 擦除 NVS 中 Wi-Fi 凭据 → `esp_restart()` 重启进入配网 |

配网设备名：`ESP32_OTA_PROV`，安全级别：`NETWORK_PROV_SECURITY_0`（无 PoP，APP 直接下发 SSID/密码）。

---

## 华为云 IoTDA 鉴权

### MQTT 连接参数

| 参数 | 值 | 说明 |
|------|-----|------|
| 地址 | `mqtts://{实例ID}.iotda-device.cn-north-4.myhuaweicloud.com:8883` | TLS 加密 |
| ClientID | `{deviceId}_0_0_{timestamp}` | timestamp 为 UTC 时间 `YYYYMMDDHH` |
| Username | `{deviceId}` | 设备 ID |
| Password | HMAC-SHA256 结果（hex 小写） | key=时间戳, msg=设备密钥 |

### HMAC 计算方式

```
Password = HMAC-SHA256( key = UTC时间戳 , message = 设备密钥 )
         → 输出 hex 编码小写字符串（64 字符）
```

实现使用 ESP-IDF v6.1 推荐的 **PSA Crypto API**：

```c
psa_import_key( &attributes, timestamp, strlen(timestamp), &key_id );
psa_mac_compute( key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                 DEVICE_SECRET, strlen(DEVICE_SECRET), hmac_result, ... );
// 输出字符串: for (i) sprintf(password + i*2, "%02x", hmac_result[i])
```

### TLS 证书校验

通过 `esp_crt_bundle_attach` 使用 ESP-IDF 内置 CA 证书包验证服务器证书，无需额外导入证书文件。

### 主题（Topic）

| 方向 | Topic | 说明 |
|------|-------|------|
| 订阅 | `$oc/devices/{deviceId}/sys/commands/#` | 接收云端下发命令 |
| 发布 | `$oc/devices/{deviceId}/sys/properties/report` | 上报设备状态 |
| 发布 | `$oc/devices/{deviceId}/sys/commands/response/request_id={id}` | 命令响应 |

### 状态上报格式

```json
{
  "services": [{
    "service_id": "OTA",
    "properties": {
      "status": "idle|downloading|flashing_N%|success|failed"
    }
  }]
}
```

---

## OTA 二进制协议（ESP32 ↔ STM32）

### 帧结构

每一帧的格式如下：

```
│ MAGIC0  │ MAGIC1  │ TYPE │ SEQ_H │ SEQ_L │ OFFSET(4B)│ LEN(2B) │ DATA... │ CRC32(4B) │ TAIL │
│  0xAA   │  0x55   │  1B  │  1B   │  1B   │   4B       │   2B    │  N bytes │    4B     │ 0x5A │
```

### 帧类型

| 类型值 | 名称 | 方向 | 说明 |
|--------|------|------|------|
| `0x01` | PREPARE | ESP32 → STM32 | 握手帧，携带固件总大小(4B) + 固件 CRC32(4B) |
| `0x02` | DATA | ESP32 → STM32 | 数据帧，每包 1024 字节 |
| `0x03` | FINISH | ESP32 → STM32 | 结束帧，通知 STM32 校验并重启 |
| `0x81` | ACK | STM32 → ESP32 | 应答帧，仅含 MAGIC + TYPE + SEQ |

### CRC32

- 多项式：`0xEDB88320`
- 计算范围：从 TYPE 字段到 DATA 末尾（不含 MAGIC、CRC32、TAIL）
- 初始值：`0xFFFFFFFF`，结果异或 `0xFFFFFFFF`

### ACK 状态机

ACK 帧不含长度字段，接收端使用逐字节状态机解析：

```
S_M0 → S_M1 → S_TYPE → S_SEQ_H → S_SEQ_L → (匹配成功返回 true / 回到 S_M0 继续)
```

### 握手流程

```
ESP32                          STM32
  │                               │
  │── LINE_CLEAN (10×\r\n) ────→│  清空 UART 缓冲区
  │── START_OTA\r\n (×2) ──────→│  唤醒 Bootloader
  │── PREPARE 帧 ──────────────→│  告知固件大小 + CRC32
  │←── ACK(seq=0) ─────────────│  确认握手成功
  │                               │
```

---

## 硬件引脚配置

| 信号 | GPIO | 说明 |
|------|------|------|
| UART TX | GPIO5 | ESP32 发送 → STM32 接收 |
| UART RX | GPIO6 | STM32 发送 → ESP32 接收 |
| UART 波特率 | 115200 | 8 数据位 / 无校验 / 1 停止位 |
| UART 编号 | UART_NUM_1 | ESP32-S3 的 UART1 |

---

## 时序参数

| 参数 | 值 | 说明 |
|------|-----|------|
| UART_STABLE_MS | 200ms | TX 引脚上拉稳定等待时间 |
| HANDSHAKE_MAX | 10 次 | 握手最大重试次数 |
| HANDSHAKE_TO | 1000ms | 握手 ACK 超时 |
| DATA_RETRY_MAX | 3 次 | 单帧数据最大重传次数 |
| DATA_ACK_TO | 1000ms | 数据帧 ACK 超时 |
| FINISH_ACK_TO | 3000ms | 结束 ACK 超时 |
| CHUNK_SIZE | 1024 字节 | 每帧数据载荷大小 |
| MAX_FIRMWARE | 1MB | 固件最大支持大小 |

---

## 任务看门狗（Task WDT）

OTA 主任务 `ota_run()` 通过 `esp_task_wdt_add(NULL)` 把 `main` 任务纳入任务看门狗（默认 5 秒超时）。为防止长时间串口传输触发复位，各耗时阶段均主动喂狗 `esp_task_wdt_reset()`：

| 阶段 | 喂狗策略 |
|------|----------|
| HTTP 下载固件 | 由 `esp_http_client` 的 `timeout_ms=15000` 兜底，不喂狗 |
| 计算整包 CRC32 | 每 8 KB 喂一次 |
| 握手重试（10 次） | 每次重试循环开头喂一次 |
| 数据传输 | 每帧发送后喂一次 |
| FINISH 等待 ACK | 发送后、等待 ACK 前喂一次 |

> 注意：HTTP 下载发生在 `esp_task_wdt_add()` 之前，此时 `main` 任务尚未纳入看门狗，若在下载阶段调用 `esp_task_wdt_reset()` 会返回 `task not found`（无害但属误用，已移除）。

---

## 编译与烧录

### 前置条件

- ESP-IDF v6.1
- PowerShell（已配置 `idf.py` 环境变量）

### 修改设备信息（menuconfig 配置，禁止硬编码）

云端设备信息通过 `idf.py menuconfig` 配置（见 `main/Kconfig.projbuild`），
**密钥严禁硬编码到源码或提交到版本库**：

```powershell
idf.py menuconfig
# 进入 "Component config" → "OTA Cloud Configuration (华为云 IoTDA)"
```

| 配置项 | 说明 |
|--------|------|
| `OTA_DEVICE_ID` | 华为云 IoTDA 设备 ID |
| `OTA_DEVICE_SECRET` | 华为云 IoTDA 设备密钥（敏感信息） |
| `OTA_IOTDA_HOST` | IoTDA 实例接入地址（如 `f1fa601837.st1.iotda-device.cn-north-4.myhuaweicloud.com`） |
| `OTA_IOTDA_PORT` | MQTT/TLS 端口（默认 8883） |

Wi-Fi 凭据无需在源码中配置，通过 **BLE 动态配网** 由手机 APP 下发。

### 编译与烧录

```powershell
idf.py build flash monitor
```

### 放置目标固件

将 STM32 固件（`App.bin`）放到 `firmware/` 目录下，编译时会由 `target_add_binary_data` 嵌入 ESP32 Flash。用于本地 OTA（无需云端）的场景；云端 OTA 会从 OBS 下载覆盖此数据。

---

## 云端操作流程

1. 登录华为云 IoTDA 控制台
2. 创建产品 → 定义 OTA 服务 → 注册设备（获取 DeviceID + DeviceSecret）
3. 将升级固件上传至 OBS 桶
4. 在设备详情页点击 **"生成连接信息"**，验证 ClientID / Username / Password
5. 通过云端命令下发接口发送 OTA 指令：

```json
{
  "service_id": "OTA",
  "command_name": "start_ota",
  "paras": {
    "url": "https://my-ota-bucket.obs.cn-north-4.myhuaweicloud.com/App.bin"
  }
}
```

---

## 依赖项

| 组件 | 说明 |
|------|------|
| `mqtt` (espressif/mqtt) | MQTT 客户端 |
| `mbedtls` | TLS + PSA Crypto |
| `esp_wifi` | Wi-Fi STA 驱动 |
| `nvs_flash` | 非易失存储 |
| `esp_timer` | 高精度定时器 |
| `esp_http_client` | HTTP 下载 |
| `esp_driver_gpio` | GPIO 驱动（v6.1 独立组件） |
| `esp_driver_uart` | UART 驱动（v6.1 独立组件） |
| `network_provisioning` | BLE 配网（v6.1 独立组件，替代旧版 wifi_provisioning） |
| `protocomm` | 配网安全通信（prov-session） |
| `bt` | 蓝牙/BLE 协议栈 |

---

## 数据流总览

```
华为云 IoTDA                ESP32-S3                    OBS               STM32
     │                         │                         │                  │
     │── OTA 命令 ────────────→│                         │                  │
     │                         │── 上报 downloading ────→│                  │
     │                         │── HTTP GET ────────────→│                  │
     │                         │←── App.bin ────────────│                  │
     │                         │                         │                  │
     │                         │── PREPARE ──────────────────────────────→│
     │                         │←── ACK(0) ───────────────────────────────│
     │                         │── DATA(seq=1) ──────────────────────────→│
     │                         │←── ACK(1) ───────────────────────────────│
     │                         │── ... (循环直到全部数据传完)               │
     │                         │── FINISH ───────────────────────────────→│
     │                         │←── ACK(n) ───────────────────────────────│
     │                         │                         │                  │
     │←── 上报 success ───────│                         │                  │
```