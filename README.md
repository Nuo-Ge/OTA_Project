# IoT OTA 一体化系统（ESP32 + STM32 + Web）

> 基于「华为云 IoTDA + OBS」的远程固件升级系统：Web 控制台下发指令 → 华为云命令下发 → ESP32 从 OBS 下载固件 → 通过 UART 串口升级 STM32 → STM32 Bootloader 搬运固件并校验跳转 → 状态实时回传前端。项目为 Monorepo 结构，设备端、引导程序与 Web 端同仓管理。

## 项目组成

| 目录 | 说明 | 技术栈 |
|------|------|--------|
| [32OTA_wifi](32OTA_wifi/) | ESP32-S3 固件：华为云 MQTT 通信 + OTA 发送端 | ESP-IDF v6.1 |
| [App](App/) | STM32F103C8T6 应用固件：OTA 接收端 | STM32CubeIDE + HAL + CMake + ARM GCC |
| [Bootloader](Bootloader/) | STM32F103C8T6 Bootloader：离线 OTA 搬运 | STM32CubeIDE + HAL + CMake + ARM GCC |
| [Test/server](Test/server/) | Web 后端：指令下发 + 状态管理 | Node.js + Express 4 |
| [Test/client](Test/client/) | Web 前端：OTA 远程控制台 | Vue 3 + Vite + Axios |

## 系统架构与数据流

```
浏览器(OTA控制台)
    │  REST /api  (JWT 鉴权)
    ▼
Express 后端 ──────► 华为云 IoTDA REST API（命令下发 / 设备影子）
    ▲                     │
    │ 内存缓存降级          │ 命令下发（MQTT）
    │                     ▼
    │              ESP32-S3（32OTA_wifi）
    │                 │ 1. 收到 OTA 命令(含 OBS url)
    │                 │ 2. HTTP 从 OBS 下载固件
    │                 │ 3. UART(GPIO5/6) 串口发送
    │                 ▼
    │              STM32F103C8T6
    │                 ├─ App：接收帧 → 写 W25QXX → 回 ACK
    │                 └─ 复位后 Bootloader：W25QXX → 内部 Flash → 校验 → 跳转
    │
    └── 状态回传（idle/downloading/flashing_N%/success/failed）
```

完整闭环：

1. 用户在 Web 控制台登录（JWT），填写固件 OBS 地址并下发升级指令。
2. 后端调用华为云 IoTDA REST API 向设备下发 `start_ota` 命令。
3. ESP32 通过 MQTT 订阅命令主题收到 OTA 指令，解析其中的 `url`。
4. ESP32 通过 HTTP 从 OBS 下载固件到 PSRAM。
5. ESP32 通过 UART（115200-8-N-1）以停等协议逐帧发送给 STM32（PREPARE/DATA/FINISH，每包 1024B + CRC32）。
6. STM32 App 将固件写入外部 SPI Flash（W25QXX），并回 ACK；FINISH 后整包 CRC32 校验。
7. STM32 软复位进入 Bootloader，Bootloader 将 W25QXX 中的新固件搬运到内部 Flash App 区，校验后跳转运行。
8. 全程 ESP32 通过 MQTT 上报状态，后端缓存并供前端轮询展示。

## 目录结构

```
ota-platform/
├── 32OTA_wifi/          # ESP32-S3 固件（ESP-IDF）
│   ├── main/            #   应用逻辑（MQTT / 协议 / 传输 / 任务 / Wi-Fi）
│   ├── firmware/        #   本地 OTA 用 STM32 固件（编译时嵌入）
│   ├── test/            #   主机侧单元测试
│   ├── sdkconfig.defaults / partitions.csv
│   └── README.md
├── App/                 # STM32 应用固件（OTA 接收端）
│   ├── Core/ BSP/ Ota/ Utils/ Drivers/
│   ├── App.ioc / CMakeLists.txt / CMakePresets.json
│   └── README.md
├── Bootloader/          # STM32 Bootloader（离线 OTA 搬运）
│   ├── Core/ BSP/ Ota/ Utils/ Drivers/ cmake/
│   ├── Bootloader.ioc / CMakeLists.txt / CMakePresets.json
│   └── README.md
└── Test/                # Web 端
    ├── server/          #   Express 后端（config/controllers/routes/services...）
    ├── client/          #   Vue3 前端（src/api、components、composables...）
    ├── api-spec.md      #   API 规范（前后端唯一事实来源）
    └── README.md
```

## 各模块说明

### 1. ESP32-S3（32OTA_wifi）

- **平台**：ESP-IDF v6.1，目标芯片 ESP32-S3。
- **云端**：华为云 IoTDA（MQTT/TLS 8883，HMAC-SHA256 动态鉴权）+ OBS（HTTP 下载固件）。
- **配网**：Wi-Fi 凭据不硬编码，通过 BLE 动态配网（`network_provisioning`）下发；连接失败自动回退配网，BOOT 键长按 3 秒重新配网。
- **OTA 发送端**：与 STM32 之间采用二进制帧协议（MAGIC + Type + Seq + Offset + Length + Data + CRC32 + Tail），停等 + ACK + 超时重传。
- **关键配置**：设备 ID / 密钥 / 接入地址通过 `idf.py menuconfig` 注入（`main/Kconfig.projbuild`），**严禁硬编码到源码**。

### 2. STM32 App（接收端）

- **芯片**：STM32F103C8T6（Cortex-M3，64KB Flash / 20KB SRAM）。
- **接收**：USART1（PA9/PA10）+ DMA1 双缓冲环形接收，IDLE/TC/HT 三重触发。
- **存储**：固件写入外部 SPI Flash W25QXX（SPI1），OTA 控制信息写入 EEPROM AT24CXX（I2C1）。
- **协议**：逐字节状态机解析 OTA 帧，PREPARE 擦除 / DATA 写入 / FINISH 整包 CRC32 校验并回 ACK。
- **显示**：0.96 寸 OLED（软件 I2C）实时显示进度，LED 心跳指示。

### 3. STM32 Bootloader（离线 OTA 搬运）

- **职责**：只负责「搬运 + 校验 + 跳转」，不负责固件接收。
- **分区**：内部 Flash 64KB —— Bootloader 区 24KB（`0x08000000`），App 区 40KB（`0x08006000`）。
- **流程**：上电检查 AT24CXX 中的 `OtaInfo_t` 标志 → 若有待升级固件则从 W25QXX 搬运到内部 Flash App 区并 CRC32 校验 → 校验 App 栈顶/ResetHandler 合法性 → 关中断、重映射向量表、设置 MSP 后跳转。

### 4. Web 控制台（Test）

- **后端（server）**：Node.js + Express 4，对接华为云 IoTDA SDK，JWT + bcryptjs 鉴权；设备影子不可用时回退本地内存缓存。统一响应信封 `{ code, message, data }`。
- **前端（client）**：Vue 3（`<script setup>`）+ Vite + Axios；`useAuth` 管理登录态，`useOta` 状态轮询（活跃 1s / 终态 10s 动态调频）。
- **接口**（详见 [api-spec.md](Test/client/api-spec.md)）：

| 方法 | 路径 | 鉴权 | 说明 |
|------|------|------|------|
| POST | `/api/auth/login` | 否 | 登录，签发 JWT |
| GET | `/api/auth/me` | 是 | 获取当前用户 |
| GET | `/api/ota/status` | 是 | 获取设备状态 |
| POST | `/api/ota/upgrade` | 是 | 下发升级指令 |
| GET | `/api/ota/shadow` | 是 | 查询设备影子（调试） |

## OTA 串口协议（ESP32 ↔ STM32）

帧结构：

```
Byte:  0      1      2      3    4      5    6    7    8      9    10    11..(N+10)  (N+11)..(N+14)  N+15
     ┌──────┬──────┬──────┬──────────┬────────────────┬──────────┬──────────────┬───────────────┬──────┐
     │ 0xAA │ 0x55 │ Type │  Seq(2)  │  Offset(4)     │ Len(2)   │  Data(N)     │  CRC32(4)     │ 0x5A │
     └──────┴──────┴──────┴──────────┴────────────────┴──────────┴──────────────┴───────────────┴──────┘
```

| 类型值 | 名称 | 方向 | 说明 |
|--------|------|------|------|
| `0x01` | PREPARE | ESP32 → STM32 | 握手，携带固件总大小 + 整包 CRC32 |
| `0x02` | DATA | ESP32 → STM32 | 数据帧，每包 1024 字节 |
| `0x03` | FINISH | ESP32 → STM32 | 结束帧，触发整包校验与复位 |
| `0x81` | ACK | STM32 → ESP32 | 应答帧，回传原 seq |

- CRC32：多项式 `0xEDB88320`，初始值 `0xFFFFFFFF`，结果异或 `0xFFFFFFFF`。
- 帧 CRC 计算范围：从 Type 到 Data 末尾（不含 Magic、CRC32、Tail）。
- 传输策略：停等协议，超时重传（DATA 最多 3 次）。

## STM32 硬件引脚速查

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA0（App）/ PA1（Bootloader） | LED | 心跳 / 状态指示 |
| PA4 | W25QXX CS | SPI Flash 片选 |
| PA5 / PA6 / PA7 | SPI1 SCK / MISO / MOSI | W25QXX 通信 |
| PA9 / PA10 | USART1 TX / RX | 与 ESP32 串口通信 |
| PB6 / PB7 | I2C1 SCL / SDA | AT24CXX EEPROM |
| PB8 / PB9 | OLED CL / DA | 软件 I2C 显示屏 |

ESP32 侧：UART1，TX=GPIO5，RX=GPIO6，115200-8-N-1。

## 构建与运行

### ESP32（需 ESP-IDF v6.1 环境）

```powershell
cd 32OTA_wifi
idf.py menuconfig        # 配置 OTA_DEVICE_ID / OTA_DEVICE_SECRET / OTA_IOTDA_HOST 等
idf.py build flash monitor
```

> 若项目是从其它路径复制而来，首次构建前先删除 `build/` 和 `managed_components/`（含旧绝对路径）再执行 `idf.py build`。

### STM32 App / Bootloader

使用 STM32CubeIDE 打开对应工程的 `.ioc`（`App/App.ioc` 或 `Bootloader/Bootloader.ioc`），或使用 CMake（已提供 `CMakePresets.json`）构建，产物输出至 `build/Debug/`。

### Web（后端 + 前端）

```bash
# 后端（默认 http://127.0.0.1:3000）
cd Test/server
npm install
cp .env.example .env    # 填入华为云 AK/SK、JWT_SECRET 等（见下方安全说明）
npm run dev

# 前端（Vite 已配置 /api 代理到后端）
cd Test/client
npm install
npm run dev
```

## 配置与密钥安全

- **ESP32 设备密钥**：存储在 `32OTA_wifi/sdkconfig`（由 `menuconfig` 生成），**严禁提交到版本库**，已在根 `.gitignore` 中排除。
- **Web 端华为云 AK/SK**：配置在 `Test/server/.env`，基于 `Test/server/.env.example` 复制填写，**切勿提交真实 AK/SK / JWT_SECRET**。
- **前端环境变量**：基于 `Test/client/.env.example` 复制为 `.env.development`，配置 `VITE_API_URL`。

## 技术栈汇总

| 层 | 技术 |
|----|------|
| 设备端（ESP32） | ESP-IDF v6.1、FreeRTOS、MQTT、mbedTLS/PSA Crypto、network_provisioning |
| 设备端（STM32） | STM32 HAL、CMSIS、CMake + ARM GCC、CRC32、DMA/UART/SPI/I2C |
| 后端 | Node.js、Express 4、华为云 IoTDA SDK、JWT、bcryptjs、dotenv |
| 前端 | Vue 3、Vite、Axios、ESLint + Prettier |