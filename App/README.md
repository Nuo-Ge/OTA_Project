# STM32 OTA 固件无线升级 —— 接收端项目文档

> **芯片**: STM32F103C8T6  |  **发送端**: ESP32（UART）  |  **固件存储**: W25QXX 外部 Flash  
> **编译工具链**: STM32CubeMX + HAL + CMake/Ninja + ARM GCC

---

## 一、硬件架构

| 外设 | 引脚 / 总线 | 作用 |
|------|------------|------|
| **USART1** (RX: PA10, TX: PA9) | APB2, 115200-8-N-1 | 接收 ESP32 发来的 OTA 数据，回复 ACK |
| **DMA1 Ch5** (USART1_RX) | Circular 模式 | 双缓冲 DMA 自动循环接收，不丢字节 |
| **W25QXX** (SPI1: SCK=PA5, MISO=PA6, MOSI=PA7, CS=PA4) | APB2, Mode3 | 外部 Flash，存储接收的固件镜像 |
| **AT24CXX** (I2C1: SCL=PB6, SDA=PB7) | APB1, 100KHz | EEPROM，存储 OTA 控制信息（16 字节） |
| **OLED** (I2C1 模拟: CLK=PB8, DA=PB9) | 软件 I2C | 显示 OTA 进度和状态 |
| **LED** (PA0) | GPIO | 500ms 翻转，心跳指示 |
| **HSE** (8MHz) × PLL×9 | 72MHz SYSCLK | 系统时钟 |

### 时钟树

```
HSE(8MHz) → PLL×9 → 72MHz = SYSCLK (HCLK)
                      ├─ APB1 = 36MHz (I2C1, USART2/3/4/5)
                      └─ APB2 = 72MHz (USART1, SPI1, GPIO)
```

### 引脚速查表

```
PA0  → LED (推挽输出)
PA4  → W25QXX CS
PA5  → SPI1 SCK
PA6  → SPI1 MISO
PA7  → SPI1 MOSI
PA9  → USART1 TX
PA10 → USART1 RX
PB6  → I2C1 SCL
PB7  → I2C1 SDA
PB8  → OLED SCL (模拟I2C)
PB9  → OLED SDA (模拟I2C)
```

---

## 二、目录结构

```
App/
├── Core/
│   ├── Inc/
│   │   ├── main.h              # IO 引脚宏定义（W25CS_Pin, OLED_CL/DA）
│   │   ├── usart.h             # huart1 / hdma_usart1_rx / hdma_usart1_tx 外部声明
│   │   ├── app_core.h          # AppCore_Init / AppCore_Loop 声明
│   │   ├── app_sensor.h        # 传感器模块（OTA 时停止）
│   │   ├── app_ui.h            # UI 抽象（OLED 显示 + OTA 事件回调）
│   │   └── app_version.h       # 固件版本号
│   └── Src/
│       ├── main.c              # ★ 入口：初始化外设 → AppCore_Init → 主循环(LED+AppCore_Loop)
│       ├── app_core.c          # ★★ 应用状态机：IDLE 匹配唤醒命令 / OTA 喂解析器
│       ├── app_sensor.c        # 传感器采集（OTA 时停止以释放 CPU）
│       ├── app_ui.c            # OLED 显示 + OTA 事件回调实现
│       ├── stm32f1xx_it.c      # ★ 中断服务：USART1_IRQHandler（IDLE+DMA半满/满→环形缓冲）
│       ├── stm32f1xx_hal_msp.c # HAL MSP 初始化（NVIC/DMA配置）
│       ├── usart.c             # USART1 初始化（115200-8-N-1）
│       ├── dma.c               # DMA1 Ch4(TX)/Ch5(RX) 初始化
│       ├── spi.c               # SPI1 Full-Duplex Master
│       ├── i2c.c               # I2C1 初始化
│       ├── gpio.c              # PA0(OUT) PA4(OUT) PB6/PB7(OD)
│       └── system_stm32f1xx.c  # SystemInit / SystemClock_Config
│
├── BSP/                         # 板级支持包
│   ├── Inc/
│   │   ├── bsp_uart.h          # DMA_RX_BUF_SIZE(2048) / 函数声明
│   │   ├── bsp_w25qxx.h        # W25QXX 指令宏、扇区/块大小
│   │   ├── bsp_at24cxx.h       # AT24CXX 型号选择、页大小
│   │   └── OLED.h / bsp_ascii_font.h
│   └── Src/
│       ├── bsp_uart.c          # ★ DMA双缓冲→环形缓冲→错误恢复
│       ├── bsp_w25qxx.c        # SPI Flash 读写擦除
│       ├── bsp_at24cxx.c       # I2C EEPROM 读写
│       └── OLED.c              # 软件I2C OLED 驱动
│
├── Ota/                         # ★ OTA 协议核心
│   ├── Inc/
│   │   ├── ota_protocol.h      # 帧格式常量、OtaFrame_t 结构体
│   │   ├── ota_config.h        # 存储分区、OtaInfo_t(16字节)
│   │   ├── ota_parser.h        # OtaParserState_t 状态机枚举
│   │   └── ota_receiver.h      # OtaEvent_t 事件枚举、回调类型
│   └── Src/
│       ├── ota_parser.c        # ★ 逐字节帧解析状态机
│       └── ota_receiver.c      # ★ 帧处理（PREPARE/DATA/FINISH）+ ACK 发送
│
├── Utils/                       # 工具库
│   ├── Inc/
│   │   ├── crc32.h             # 增量 CRC32 API
│   │   └── ringbuf.h           # 环形缓冲区（4096 字节）
│   └── Src/
│       ├── crc32.c             # CRC32 实现（EDB88320, 初始0xFFFFFFFF）
│       └── ringbuf.c           # 环形缓冲区读写实现
│
└── Drivers/                     # STM32 HAL 驱动（ST 官方库）
```

---

## 三、核心数据流全景

```
                          STM32F103C8T6
┌──────────┐   UART(115200)   ┌──────────┐   DMA Ch5    ┌──────────────┐
│  ESP32   │ ──────────────→  │  USART1  │ ───circular→  │ s_dma_rx_buf │
│ 发送端   │ ←────────────── │  (PA9/10) │              │  [2048 字节] │
└──────────┘    ACK 回复      └──────────┘              └──────┬───────┘
                                                ┌──────────────┤
                                     IDLE/TC/HT 中断触发     ↓
                                                │   BSP_UART_FlushToRingBuf()
                                                ↓              ↓
                                    ┌──────────────────────────────┐
                                    │  RingBuf (环形缓冲区)         │
                                    │  s_uart_rx_rb [4096 字节]    │
                                    └──────────────┬───────────────┘
                                                   │
                                    AppCore_Loop() 逐字节读取
                                                   ↓
                              ┌──────────────────────────────┐
                              │  s_app_state ?                │
                              ├──────────────┬───────────────┤
                              │  IDLE        │  OTA           │
                              │  匹配文本     │  喂 OtaParser  │
                              │  "START_OTA\r"│  (状态机)      │
                              │  → 切换状态   │               │
                              │  清空 RingBuf │               │
                              └──────────────┴───────┬───────┘
                                                      │ 返回 1 = 完整帧
                                                      ↓
                                          OtaReceiver_HandleFrame()
                                                      │
                              ┌───────────────────────┼───────────────┐
                              │ PREPARE(0x01)    DATA(0x02)   FINISH(0x03) │
                              │ 擦除 W25QXX      写入 Flash    CRC32 整包校验 │
                              │ 写 EEPROM 信息   回 ACK        写 EEPROM flag │
                              │ 回 ACK                       软复位         │
                              └───────────────────────────────────────────────┘
```

**关键设计点**：
1. **DMA Circular 循环接收**：USART1 RX 使用 DMA1 Ch5 在 2048 字节缓冲区上环形写入，硬件自动覆盖旧数据
2. **IDLE + TC + HT 三重触发**：IDLE 检测帧间隙、TC（传输完成）和 HT（半满）作为补充触发，确保数据及时搬入 RingBuf
3. **主循环非阻塞**：`AppCore_Loop()` 每次最多处理 256 字节（`LOOP_BATCH`），防止串口风暴饿死 UI 刷新和 LED

---

## 四、OTA 通信协议（完整帧格式）

### 4.1 帧结构

```
Byte:  0      1      2      3    4      5    6    7    8      9    10    11..(N+10)  (N+11)..(N+14)  N+15
     ┌──────┬──────┬──────┬──────────┬────────────────┬──────────┬──────────────┬───────────────┬──────┐
     │ 0xAA │ 0x55 │ Type │  Seq(2)  │  Offset(4)     │ Len(2)   │  Data(N)     │  CRC32(4)     │ 0x5A │
     └──────┴──────┴──────┴──────────┴────────────────┴──────────┴──────────────┴───────────────┴──────┘
      Magic   Magic   帧类型  包序号     数据偏移        数据长度     N≤1024字节    帧CRC32校验      帧尾
```

**帧固定开销** = 2(Magic) + 1(Type) + 2(Seq) + 4(Offset) + 2(Length) + 4(CRC32) + 1(Tail) = **16 字节**

**C 结构体定义**（`ota_protocol.h`）：

```c
#define OTA_HEADER_LEN  11   // Magic(2) + Type(1) + Seq(2) + Offset(4) + Length(2)
#define OTA_CRC_LEN     4
#define OTA_TAIL_LEN    1
#define OTA_FIXED_LEN   16   // 帧固定开销

typedef struct {
    uint8_t  type;
    uint16_t seq;        // 大端序（已由解析器转换为主机序）
    uint32_t offset;     // 大端序
    uint16_t length;     // 大端序, 最大1024
    uint8_t  data[1024];
    uint32_t crc32;      // 大端序
} OtaFrame_t;
```

### 4.2 CRC32 规范

| 参数 | 值 |
|------|-----|
| 多项式 | `0xEDB88320`（与 zlib / 以太网 / PNG 一致） |
| 初始值 | `0xFFFFFFFF` |
| 最终 XOR | `0xFFFFFFFF` |
| 标准测试向量 | `CRC32("123456789")` = `0xCBF43926` |
| **帧 CRC 计算范围** | **从 Type（第2字节）开始，到 Data 最后一字节结束**，不包含 Magic0/Magic1、CRC32 自身和 Tail |
| **整包 CRC 计算范围** | 完整固件二进制文件的全部字节 |

### 4.3 四种帧类型详述

#### (1) PREPARE 帧 (Type=0x01)

```
Data[0..3]: total_size   (4字节, 大端序) —— 固件总大小，须 ≤ 0x100000 (1MB)
Data[4..7]: total_crc32  (4字节, 大端序) —— 完整固件的 CRC32
```

**STM32 处理流程**（`Handle_Prepare()`）：
1. 校验 `total_size`：不能为 0、不能超过 1MB
2. 发射 `OTA_EVENT_PREPARE_START` 事件（OLED 显示 "Erasing..."）
3. 按 64KB 块擦除 W25QXX 对应区域（块擦除指令 `0xD8`）
4. 将 `{magic=0xA5A5A5A5, flag=NONE, total_size, total_crc32}` 写入 AT24CXX
5. 发射 `OTA_EVENT_PREPARE_DONE`
6. **回复 ACK（seq=接收到的 seq）**

#### (2) DATA 帧 (Type=0x02)

```
Data[0..N-1]: 固件分片数据（N = Length 字段值, 1~1024）
Offset: 该分片在固件二进制中的字节偏移
Seq: 包序号（从 PREPARE 之后递增）
```

**STM32 处理流程**（`Handle_Data()`）：
1. 校验 `length` 范围 (1~1024)
2. 校验 `offset + length ≤ total_size`（防越界）
3. `BSP_W25QXX_Write()` 写入 W25QXX 对应偏移
4. 发射 `OTA_EVENT_RECEIVE_PROGRESS`（OLED 更新百分比）
5. **回复 ACK（seq=接收到的 seq）**

#### (3) FINISH 帧 (Type=0x03)

```
Data: 空（Length=0）
```

**STM32 处理流程**（`Handle_Finish()`）：
1. 发射 `OTA_EVENT_VERIFY_START`
2. 从 W25QXX 逐 256 字节读出全部固件，**增量**计算 CRC32
3. 与 PREPARE 帧中约定的 `total_crc32` 比较
   - **不一致** → 发射 `OTA_EVENT_VERIFY_FAIL`，**不回复 ACK**，结束
   - **一致** → 发射 `OTA_EVENT_VERIFY_PASS`
4. 更新 AT24CXX 中 `flag = OTA_FLAG_READY`
5. **回复 ACK**
6. 发射 `OTA_EVENT_REBOOT`
7. `HAL_Delay(2000)` → `NVIC_SystemReset()` 软复位

#### (4) ACK 帧 (Type=0x81, STM32→ESP32)

```
AA 55 | 0x81 | Seq(2B) | Offset=0(4B) | Length=0(2B) | CRC32(4B) | 0x5A
```

- Seq = 回复对应接收帧的 seq（**原值回传**，不加/不减）
- Offset = 0x00000000
- Length = 0x0000，Data 为空
- CRC32 覆盖从 Type(0x81) 到 Length(0x0000) 共 **9 字节**

**STM32 端 `Send_Ack()` 实现**：

```c
static void Send_Ack(uint16_t seq)
{
    uint8_t frame[16];
    uint8_t idx = 0;

    uint8_t payload[9];
    payload[0] = OTA_TYPE_ACK;          // 0x81
    payload[1] = (seq >> 8) & 0xFF;     // Seq 高字节（大端）
    payload[2] = seq & 0xFF;            // Seq 低字节
    payload[3] = 0x00;                  // Offset=0
    payload[4] = 0x00;
    payload[5] = 0x00;
    payload[6] = 0x00;
    payload[7] = 0x00;                  // Length=0
    payload[8] = 0x00;

    uint32_t crc = CRC32_Calculate(payload, 9);  // CRC 覆盖 9 字节 payload

    frame[idx++] = OTA_MAGIC_0;         // 0xAA
    frame[idx++] = OTA_MAGIC_1;         // 0x55
    memcpy(&frame[idx], payload, 9); idx += 9;
    frame[idx++] = (crc >> 24) & 0xFF;  // CRC32 大端
    frame[idx++] = (crc >> 16) & 0xFF;
    frame[idx++] = (crc >> 8)  & 0xFF;
    frame[idx++] = (crc)       & 0xFF;
    frame[idx++] = OTA_TAIL;            // 0x5A

    BSP_UART_Send(frame, idx);          // 阻塞式发送完整 ACK
}
```

---

## 五、OTA 完整通信时序

```
ESP32 发送端                              STM32 接收端
    │                                         │
    │──── "START_OTA\r" (10字节文本) ────────→│ 匹配→切换到 OTA 模式→清空 RingBuf
    │                                         │
    │──── PREPARE 帧 ─────────────────────────→│ 擦除 W25QXX → 写 EEPROM
    │←─────────────── ACK(seq=0) ───────────│
    │                                         │
    │──── DATA 帧 (seq=1, offset=0) ─────────→│ 写入 W25QXX
    │←─────────────── ACK(seq=1) ───────────│
    │──── DATA 帧 (seq=2, offset=1024) ──────→│ 写入 W25QXX
    │←─────────────── ACK(seq=2) ───────────│
    │      ... (重复直到全部数据发完) ...       │
    │                                         │
    │──── FINISH 帧 (seq=N+1) ───────────────→│ CRC32 整包校验
    │←─────────────── ACK(seq=N+1) ──────────│ 写 EEPROM flag=READY
    │                                         │ 2秒后软复位
```

**ESP32 端要点**：
1. 每发一帧 → 等 ACK → 收到 ACK 发下一帧（停等协议）
2. 超时未收到 ACK 可重试（建议 3 次）
3. 发帧前先 `uart_flush_input()` 清空接收缓冲区残留（防止上次超时残余干扰）

---

## 六、核心模块代码详解

### 6.1 `app_core.c` —— 应用层状态机

```c
#define START_OTA_CMD   "START_OTA\r\n"    // 源码定义
#define START_OTA_LEN   10                 // 实际匹配 10 个字符: "START_OTA\r"
```

**状态机设计**：

```
APP_STATE_IDLE ──(匹配 "START_OTA\r")──→ APP_STATE_OTA
                                             │
                                      收到完整帧 → OtaReceiver_HandleFrame()
```

**IDLE 模式细节**：逐字节匹配唤醒命令，支持**中途重匹配**：

```c
if (ch == ((const uint8_t*)START_OTA_CMD)[s_cmd_match_idx]) {
    s_cmd_match_idx++;
    if (s_cmd_match_idx >= START_OTA_LEN) {
        s_app_state = APP_STATE_OTA;
        BSP_UART_GetRxRingBuf()->read = BSP_UART_GetRxRingBuf()->write; // 清空 RingBuf
        OtaParser_Init(&s_parser);
        OtaReceiver_Init();
    }
} else {
    s_cmd_match_idx = 0;
    if (ch == 'S') s_cmd_match_idx = 1;  // 允许从 'S' 重新开始
}
```

**为什么启动 OTA 模式后要清空 RingBuf？** ESP32 可能多次重试握手，每次重试都发 `"\r\n\r\nSTART_OTA\r"` 文本，这些字节会堆积在环形缓冲区。OTA 模式下的解析器期望第一个字节就是 `0xAA`（帧头），如果前面有垃圾文本就会失同步。把 `read` 指针直接跳到 `write` 位置，等于丢弃所有未处理数据。

**批次限制**：`LOOP_BATCH = 256`，每轮主循环最多消费 256 字节，防止连续大量数据导致 OLED 无法刷新。

### 6.2 `usp_uart.c` —— DMA 双缓冲 + 环形缓冲 + 错误恢复

**架构**：

```
USART1 RX 中断
   ├── DMA Ch5 (Circular, 2048B)        ← 硬件自动循环写入 s_dma_rx_buf
   ├── IDLE 中断: 检测帧间隙 → Flush     ← 数据帧之间的停顿
   ├── TC 中断:   DMA 满一圈 → Flush     ← HAL_UART_RxCpltCallback
   └── HT 中断:   DMA 过半 → Flush       ← HAL_UART_RxHalfCpltCallback
```

**`FlushToRingBuf()` 实现细节**：

```c
void BSP_UART_FlushToRingBuf(void)
{
    uint16_t current_pos = DMA_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

    if (current_pos == s_dma_last_pos) return;  // 无新数据

    if (current_pos > s_dma_last_pos) {
        // 正常情况：从 last 到 current-1 连续搬运
        for (uint16_t i = s_dma_last_pos; i < current_pos; i++)
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
    } else {
        // DMA 绕回：先搬 [last, end)，再搬 [0, current)
        for (uint16_t i = s_dma_last_pos; i < DMA_RX_BUF_SIZE; i++)
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
        for (uint16_t i = 0; i < current_pos; i++)
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
    }
    s_dma_last_pos = current_pos;
}
```

**DMA `CNDTR` 寄存器特性**：`__HAL_DMA_GET_COUNTER()` 返回 DMA 剩余传输次数。DMA Circular 模式不断从 0→2047→0 循环，`CNDTR` 从 2048 递减到 0 时自动重装。因此当前位置 = `2048 - CNDTR`。

**错误恢复机制**（`HAL_UART_ErrorCallback`）：

```c
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        HAL_UART_DMAStop(huart);                           // ① 停 DMA
        __HAL_UART_CLEAR_OREFLAG(huart);                  // ② 清溢出标志
        __HAL_UART_CLEAR_NEFLAG(huart);                   // ② 清噪声标志
        __HAL_UART_CLEAR_FEFLAG(huart);                   // ② 清帧错误标志
        s_dma_last_pos = 0;                                 // ③ 重置位置指针
        HAL_UART_Receive_DMA(&huart1, s_dma_rx_buf, 2048); // ④ 重启 DMA
        __HAL_UART_ENABLE_IT(huart, UART_IT_ERR);          // ⑤ 重新使能错误中断
    }
}
```

**为什么步骤⑤必须手动执行？** `HAL_UART_DMAStop()` 会关闭 UART 模块使能位（`UE`），`HAL_UART_Receive_DMA()` 重新初始化时只恢复 `RXNE`/`IDLE`/`PE` 中断，**不会恢复**我们手动开启的 `UART_IT_ERR`。不加这一步，错误恢复就变成"一次性"的。

### 6.3 `ota_parser.c` —— 逐字节帧解析状态机

**状态机流程**（与 `OtaParserState_t` 完全对应）：

```
                  ch==0xAA?
PARSER_WAIT_MAGIC1 ───────→ PARSER_WAIT_MAGIC2
       ↑  ≠0xAA                  │
       │                         │ ch==0x55?  ──── 启动 CRC32 计算
       │                         ↓              crc = CRC32_Start()
       │                   PARSER_WAIT_TYPE     ← 记录 frame.type, 喂 CRC
       │                         ↓
       │                   PARSER_WAIT_SEQ      ← 累积 2 字节 → frame.seq (大端), 喂 CRC
       │                         ↓
       │                   PARSER_WAIT_OFFSET   ← 累积 4 字节 → frame.offset (大端), 喂 CRC
       │                         ↓
       │                   PARSER_WAIT_LENGTH   ← 累积 2 字节 → frame.length (大端), 喂 CRC
       │                         ↓
       │                   ┌──── if length==0: 跳过 DATA 直接读 CRC
       │                   ↓
       │              PARSER_WAIT_DATA          ← 逐 byte 存 frame.data[], 喂 CRC
       │                   │ 收满 length 字节
       │                   ↓
       │              PARSER_WAIT_CRC           ← 累积 4 字节 → frame.crc32
       │                   ↓
       │              CRC32_Final(crc) == frame.crc32 ?
       │                   │ Y                    │ N
       │                   ↓                      ↓
       │              PARSER_WAIT_TAIL       return -1 (CRC错)
       │                   │
       │              ch==0x5A? → return 1 (完整帧)
       │              ch≠0x5A? → return -2 (帧尾错)
       └──────────────────────────┘  ← 任何错误都回到 MAGIC1
```

**关键设计**：CRC 从 Type 字节开始累积，输入 `CRC32_Update()` 的字节与帧传输顺序一致。解析器不参与 Magic0/Magic1 的 CRC 计算，这与协议设计完全匹配。

### 6.4 `ota_receiver.c` —— 帧处理与 ACK 回复

**事件驱动架构**：`OtaReceiver_HandleFrame()` 内部通过 `FireEvent()` 向 UI 层发射事件，UI 层通过 `OtaReceiver_SetEventCallback()` 注册回调。

```c
// 事件枚举
OTA_EVENT_PREPARE_START    → OLED 显示 "Erasing..."
OTA_EVENT_PREPARE_DONE     → OLED 显示 "Waiting Data..."
OTA_EVENT_RECEIVE_PROGRESS → OLED 显示 "XX%" 进度
OTA_EVENT_VERIFY_START     → OLED 显示 "Verifying..."
OTA_EVENT_VERIFY_PASS      → OLED 显示 "OK! Rebooting..."
OTA_EVENT_VERIFY_FAIL      → OLED 显示 "CRC FAIL!"
OTA_EVENT_ERROR_SIZE       → OLED 显示 "Size Error!"
OTA_EVENT_ERROR_OFFSET     → OLED 显示 "Offset Error!"
OTA_EVENT_REBOOT           → OLED 显示 "Rebooting..."
```

### 6.5 `crc32.c` —— 增量 CRC32

提供**流式计算**和**一次性计算**两套 API：

```c
// 流式 API（适合 OTA 逐包校验）
uint32_t CRC32_Start(void);                                      // 返回 0xFFFFFFFF
uint32_t CRC32_Update(uint32_t crc, uint8_t byte);               // 逐字节喂入
uint32_t CRC32_UpdateBlock(uint32_t crc, const uint8_t *data, uint32_t len); // 批量喂入
uint32_t CRC32_Final(uint32_t crc);                              // XOR 0xFFFFFFFF

// 便捷 API
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len);     // 一次性计算
```

**实现**：使用按位查表法（非查表法），代码精简。每字节迭代 8 次，每次判断最低位是否为 1 决定是否 XOR 多项式。

### 6.6 `ringbuf.c` —— 环形缓冲区

```c
#define RINGBUF_SIZE  4096

typedef struct {
    uint8_t  buffer[4096];
    uint16_t read;    // 读指针
    uint16_t write;   // 写指针
} RingBuf_t;
```

- **空**：`read == write`
- **满**：`(write + 1) % 4096 == read`（保留一个字节空间防止混淆）
- **写**：写指针位置存入数据 → `write = (write + 1) % 4096`
- **读**：`*data = buffer[read]` → `read = (read + 1) % 4096`

### 6.7 `stm32f1xx_it.c` —— 中断服务

**USART1_IRQHandler** 是数据流入的总入口：

```c
void USART1_IRQHandler(void)
{
    // ① 先检查 IDLE → 立即搬运（IDLE 表明一帧结束）
    if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_IDLE)) {
        __HAL_UART_CLEAR_IDLEFLAG(&huart1);
        BSP_UART_FlushToRingBuf();
    }
    // ② HAL 标准处理（会触发 HAL_UART_RxCpltCallback / HAL_UART_ErrorCallback）
    HAL_UART_IRQHandler(&huart1);
}
```

**DMA1_Channel5_IRQHandler** 处理 DMA 接收完成/半满中断：

```c
void DMA1_Channel5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&hdma_usart1_rx); // HAL 内部调用 RxCpltCallback / RxHalfCpltCallback
}
```

**主循环中的 LED 心跳**（`main.c`）：

```c
static uint32_t led_tick = 0;
if (HAL_GetTick() - led_tick >= 500) {
    HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_0);  // PA0 500ms 翻转
    led_tick = HAL_GetTick();
}
```

---

## 七、存储分区设计

| 存储介质 | 起始地址 | 大小 | 用途 |
|----------|---------|------|------|
| 内部 Flash | `0x08000000` | 24KB | Bootloader（预留） |
| 内部 Flash | `0x08006000` | 40KB | App 区（当前运行程序） |
| W25QXX | `0x000000` | 1MB | 固件镜像暂存区 |
| AT24CXX | `0x00` | 16B | OTA 控制信息 |

### AT24CXX 存储结构（16 字节）

```c
typedef struct __attribute__((packed)) {
    uint32_t magic;          // 0x00: 必须是 0xA5A5A5A5
    uint8_t  flag;           // 0x04: NONE=0x00 / READY=0x01
    uint8_t  reserved[3];    // 0x05-0x07: 保留
    uint32_t total_size;     // 0x08: 固件总大小（大端）
    uint32_t total_crc32;    // 0x0C: 整包 CRC32（大端）
} OtaInfo_t;
```

### W25QXX 固件存储区

```
W25QXX 逻辑地址空间
┌────────────────────────────────────────────────────┐
│ 0x000000                                           │
│   ████████████████████████████████████████████████ │
│   ███████  固件镜像 (≤1MB)  ██████████████████████ │
│   ████████████████████████████████████████████████ │
│                                         0x0FFFFF   │
├────────────────────────────────────────────────────┤
│ 0x100000 ~ 片尾 (剩余空间，未使用)                   │
└────────────────────────────────────────────────────┘
```

---

## 八、ESP32 发送端对接指南

### 8.1 必须实现的协议要点

| 项目 | 要求 |
|------|------|
| **波特率** | 115200-8-N-1，无流控 |
| **唤醒命令** | 先发 `"\r\n\r\n"`（4字节），再发 `"START_OTA\r"`（10字节） |
| **帧构建** | `AA 55 Type Seq(2B) Offset(4B) Length(2B) Data CRC32(4B) 5A` |
| **大端序** | Seq/Offset/Length/CRC32 全部大端序（ESP32 是 little-endian，需 `htonl`/`htons` 或手动移位） |
| **CRC32 计算规则** | 与 STM32 端完全一致（多项式 0xEDB88320，初始 0xFFFFFFFF，最终 XOR） |
| **帧 CRC 范围** | 从 Type 到 Data 末尾（不包含 Magic0/Magic1/CRC32自身/Tail） |
| **ACK 匹配** | Seq 原值回传；ACK 帧 16 字节：`AA 55 81 Seq(2B) 00 00 00 00 00 00 CRC32(4B) 5A` |
| **超时重发** | 每发一帧等 1~2 秒，超时重试 3 次 |

### 8.2 ESP32 端伪代码模板

```
1. uart_flush_input()                    // 发前清接收缓冲
2. 发送 "\r\n\r\n" + "START_OTA\r"
3. vTaskDelay(50ms)
4. 构建 PREPARE 帧:
   data[0..3] = htonl(total_size)
   data[4..7] = htonl(total_crc32)
   send_frame(type=0x01, seq=0, offset=0, data, len=8)
5. wait_ack(seq=0, timeout=2000ms)       // 超时 2s (W25QXX 擦除需时间)
   → 失败则重试 (最多 10 次)
6. for each 1024-byte chunk:
     uart_flush_input()                  // 发前清缓冲
     send_frame(type=0x02, seq++, offset, chunk, len)
     wait_ack(seq, timeout=1000ms)
     → 失败重试最多 3 次
7. uart_flush_input()
   send_frame(type=0x03, seq, offset=0, NULL, len=0)
   wait_ack(seq, timeout=3000ms)         // CRC 校验可能较慢
8. STM32 校验通过后自动软复位
```

### 8.3 ACK 帧接收要点

**同步字状态机解析**（推荐，比滑动窗口更鲁棒）：

```
S_MAGIC0 ──ch==0xAA──→ S_MAGIC1 ──ch==0x55──→ S_TYPE ──ch==0x81──→ S_SEQ_H
     ↑                     │  ≠0x55               │ ≠0x81               │
     │                     ↓                      ↓                     ↓
     └───── 任意状态失败都回退到 MAGIC0 ──────────┘               S_SEQ_L
                                                                     │
                                                            seq = (H<<8)|L
                                                            匹配 expected_seq?
```

**核心原则**：`uart_flush_input()` 必须在 `send_frame()` **之前**调用，**绝不在** `wait_ack()` 内部调用，否则会把刚到或即将到的 ACK 冲掉。

---

## 九、编译与烧录

```bash
# 使用 CMake + Ninja
mkdir build && cd build
cmake .. -G Ninja -DCMAKE_BUILD_TYPE=Debug
ninja

# 输出文件
build/Debug/App.bin    # 二进制固件 (给 ESP32 OTA 用的就是这个)
build/Debug/App.hex    # Intel HEX (给 ST-LINK/J-Link 烧录用)
build/Debug/App.elf    # ELF (调试用)
```

**烧录**：
```bash
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg -c "program build/Debug/App.hex verify reset exit"
```

---

## 十、关键注意事项

1. **环形缓冲区清空时机**：切换到 OTA 模式时 `read = write`，否则 ESP32 重试握手的垃圾文本会导致解析器失同步
2. **DMA Circular 绕回处理**：`FlushToRingBuf` 必须分两段搬运（last→end, 0→current），否则数据丢失
3. **错误中断必须手动重开**：`HAL_UART_Receive_DMA()` 不恢复 `UART_IT_ERR`
4. **PREPARE 超时至少 2 秒**：W25QXX 64KB 块擦除典型时间 150ms，固件越大擦除时间越长
5. **CRC32 计算范围**：帧 CRC 从 Type 开始（跳过 Magic），整包 CRC 覆盖完整固件二进制
6. **ACK seq 必须原值回传**：不加也不减，ESP32 端用该值确认响应的对应帧
7. **CMD 匹配支持 `'S'` 重新开始**：即使匹配到一半收到乱码，只要下一个是 `'S'` 就能重来