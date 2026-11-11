# STM32F103C8T6 Bootloader — 带 OTA 固件升级

> 基于 STM32F103C8T6 + W25QXX(SPI Flash) + AT24CXX(EEPROM) + OLED(I2C) 的离线 OTA Bootloader。

## 文档信息

| 项目名称 | STM32F103C8T6 Bootloader（离线 OTA 固件升级） |
|---------|-----------------------------------------------|
| 主控芯片 | STM32F103C8T6（Cortex-M3，64KB Flash / 20KB SRAM） |
| 开发环境 | STM32CubeMX + CMake + ARM GCC |
| 文档版本 | V1.0 |

---

## 目录

1. [项目概述](#1-项目概述)
2. [硬件平台与引脚映射](#2-硬件平台与引脚映射)
3. [系统架构与 Flash 分区](#3-系统架构与-flash-分区)
4. [目录结构](#4-目录结构)
5. [启动流程详解](#5-启动流程详解)
6. [核心模块代码分析](#6-核心模块代码分析)
   - [6.1 配置层 — boot_config.h + ota_config.h](#61-配置层--boot_configh--ota_configh)
   - [6.2 App 检测与跳转 — boot_jump.c](#62-app-检测与跳转--boot_jumpc)
   - [6.3 OTA 搬运引擎 — ota_boot.c](#63-ota-搬运引擎--ota_bootc)
   - [6.4 内部 Flash 操作 — bsp_flash.c](#64-内部-flash-操作--bsp_flashc)
   - [6.5 外部 SPI Flash — bsp_w25qxx.c](#65-外部-spi-flash--bsp_w25qxxc)
   - [6.6 EEPROM 控制信息 — bsp_at24cxx.c](#66-eeprom-控制信息--bsp_at24cxxc)
   - [6.7 CRC32 校验 — crc32.c](#67-crc32-校验--crc32c)
   - [6.8 OLED 显示屏驱动 — OLED.c](#68-oled-显示屏驱动--oledc)
7. [外设初始化配置](#7-外设初始化配置)
8. [链接脚本与内存布局](#8-链接脚本与内存布局)
9. [构建系统 — CMake](#9-构建系统--cmake)
10. [OTA 数据流与协议](#10-ota-数据流与协议)
11. [错误状态码汇总](#11-错误状态码汇总)
12. [OLED 状态显示说明](#12-oled-状态显示说明)
13. [注意事项](#13-注意事项)

---

## 1. 项目概述

本项目是一个运行在 **STM32F103C8T6**（64KB Flash / 20KB SRAM）上的 Bootloader，具备以下核心能力：

- **App 完整性校验**：检查 App 区栈顶地址和 ResetHandler 地址是否合法
- **安全跳转**：关闭全部中断、重映射向量表、设置 MSP 后跳转至 App
- **离线 OTA**：从外部 SPI Flash（W25QXX）搬运固件到内部 Flash 的 App 分区
- **CRC32 完整性校验**：搬运过程中流水线计算 CRC32，搬运完成后比对校验
- **升级状态管理**：通过外部 EEPROM（AT24CXX）持久化升级控制信息
- **OLED 状态显示**：通过软件 I2C OLED 实时显示启动状态和 OTA 结果

> **注意**：本 Bootloader 仅执行"搬运 + 校验 + 跳转"，**不负责固件接收**。固件应由上位机/通信模块预先写入外部 W25QXX，并将升级控制信息写入 AT24CXX。

---

## 2. 硬件平台与引脚映射

| 芯片/模块 | 型号 | 通信接口 |
|-----------|------|----------|
| MCU | STM32F103C8T6 | — |
| 外部 Flash | W25QXX (SPI) | SPI1 |
| EEPROM | AT24CXX (I2C) | I2C1（硬件 I2C） |
| OLED | 0.96寸 I2C | PB8(SCL) / PB9(SDA) 软件 I2C |

### 引脚映射表

| 引脚 | 功能 | 说明 |
|------|------|------|
| PA1 | LED | 状态指示（低电平点亮） |
| PA4 | W25CS | W25QXX 片选（CS） |
| PA5 | SPI1_SCK | SPI 时钟 |
| PA6 | SPI1_MISO | SPI 主入从出 |
| PA7 | SPI1_MOSI | SPI 主出从入 |
| PB6 | I2C1_SCL | 硬件 I2C 时钟（AT24CXX） |
| PB7 | I2C1_SDA | 硬件 I2C 数据（AT24CXX） |
| PB8 | OLED_CL | OLED 软件 I2C 时钟 |
| PB9 | OLED_DA | OLED 软件 I2C 数据 |

---

## 3. 系统架构与 Flash 分区

### 3.1 内部 Flash 分区（STM32F103C8T6, 64KB）

```
0x08010000 ┌─────────────────────────────┐ ← FLASH_END_ADDR
           │                             │
           │        App 区               │
           │        (40KB)               │
           │   APP_MAX_SIZE = 40KB       │
           │                             │
0x08006000 ├─────────────────────────────┤ ← APP_ADDRESS (偏移 24KB)
           │                             │
           │        Bootloader           │
           │        (24KB)               │
           │   BOOTLOADER_SIZE = 24KB    │
           │                             │
0x08000000 └─────────────────────────────┘ ← BOOTLOADER_ADDR
```

**分区说明**：
- **Bootloader 区** (`0x08000000 ~ 0x08005FFF`)：24KB，存放 Bootloader 代码
- **App 区** (`0x08006000 ~ 0x0800FFFF`)：40KB，存放用户应用程序
- **FLASH_END_ADDR** (`0x08010000`)：64KB Flash 末地址

### 3.2 外部存储

| 存储器件 | 地址空间 | 用途 |
|----------|----------|------|
| W25QXX | `0x000000 ~ 0x0FFFFF` (1MB) | 存放待升级固件镜像 |
| AT24CXX | `0x00 ~ 0x0F` (16 字节) | 存放 `OtaInfo_t` 升级控制信息 |

### 3.3 软件分层架构

```
┌─────────────────────────────────────────────┐
│              应用层（main.c）                 │
│   启动流程编排、状态显示、错误处理              │
├─────────────────────────────────────────────┤
│            OTA 层（ota_boot.c）              │
│   升级状态机、固件搬运、CRC 校验              │
├─────────────────────────────────────────────┤
│          Boot 跳转层（boot_jump.c）           │
│   App 合法性检测、向量表重映射、安全跳转        │
├─────────────────────────────────────────────┤
│            BSP 板级驱动层                     │
│   bsp_flash（内部Flash）bsp_w25qxx（SPI Flash）│
│   bsp_at24cxx（EEPROM）OLED（显示屏）          │
├─────────────────────────────────────────────┤
│           Utils 工具层（crc32.c）             │
│   CRC32 增量/批量校验                          │
├─────────────────────────────────────────────┤
│         HAL 库 + CMSIS 驱动层                 │
│   GPIO/SPI/I2C/Flash/RCC/中断                 │
└─────────────────────────────────────────────┘
```

---

## 4. 目录结构

```
Bootloader/
├── BSP/                          # 板级支持包
│   ├── Inc/
│   │   ├── OLED.h                # OLED 显示驱动头文件
│   │   ├── bsp_ascii_font.h      # ASCII 字库
│   │   ├── bsp_at24cxx.h         # AT24CXX EEPROM 驱动头文件
│   │   ├── bsp_flash.h           # 内部 Flash 操作头文件
│   │   └── bsp_w25qxx.h          # W25QXX SPI Flash 驱动头文件
│   └── Src/
│       ├── OLED.c                # OLED 显示驱动（软件 I2C，128×64）
│       ├── bsp_at24cxx.c         # AT24CXX EEPROM 驱动
│       ├── bsp_flash.c           # 内部 Flash 擦除/写入
│       └── bsp_w25qxx.c          # W25QXX SPI Flash 驱动
├── Core/                          # STM32CubeMX 生成核心代码
│   ├── Inc/
│   │   ├── boot_config.h         # Bootloader 专属配置
│   │   ├── boot_jump.h           # App 检测与跳转函数声明
│   │   ├── gpio.h / i2c.h / spi.h / main.h
│   │   ├── stm32f1xx_hal_conf.h  # HAL 库配置
│   │   └── stm32f1xx_it.h        # 中断服务函数声明
│   └── Src/
│       ├── main.c                # 入口 + 主流程
│       ├── boot_jump.c           # App 跳转实现
│       ├── gpio.c / i2c.c / spi.c # 外设初始化
│       ├── stm32f1xx_hal_msp.c   # HAL MSP 初始化
│       ├── stm32f1xx_it.c        # 中断向量（SysTick 等）
│       ├── syscalls.c / sysmem.c # 系统调用 / 内存管理桩
│       └── system_stm32f1xx.c    # 系统时钟配置
├── Drivers/                       # STM32 HAL 库 + CMSIS
│   ├── CMSIS/
│   └── STM32F1xx_HAL_Driver/
├── Ota/                           # OTA 核心模块
│   ├── Inc/
│   │   ├── ota_boot.h            # OTA 搬运状态机 + 接口
│   │   ├── ota_config.h          # OTA 分区 / 协议 / 控制结构体
│   │   └── ota_protocol.h        # OTA 通信协议（预留扩展）
│   └── Src/
│       └── ota_boot.c            # OTA 搬运核心逻辑
├── Utils/                         # 工具库
│   ├── Inc/
│   │   └── crc32.h               # CRC32 增量/批量计算API
│   └── Src/
│       └── crc32.c               # CRC32 标准实现
├── cmake/                         # CMake 工具链
│   ├── stm32cubemx/CMakeLists.txt
│   ├── gcc-arm-none-eabi.cmake   # GCC ARM 工具链
│   └── starm-clang.cmake         # ARM Clang 工具链（备用）
├── build/                         # 构建输出
├── STM32F103XX_FLASH.ld           # 链接脚本
├── CMakeLists.txt                 # 顶层 CMake
├── CMakePresets.json              # CMake 预设（Debug/Release）
└── Bootloader.ioc                 # STM32CubeMX 工程文件
```

---

## 5. 启动流程详解

```
上电复位
    │
    ▼
HAL_Init()                ← 初始化 HAL 库 + SysTick
    │
    ▼
SystemClock_Config()      ← 配置系统时钟 72MHz（HSE+PLL）
    │
    ▼
MX_GPIO_Init()            ← GPIO 初始化（LED, W25CS, OLED）
MX_I2C1_Init()            ← I2C1 硬件初始化（100KHz）
MX_SPI1_Init()            ← SPI1 主机初始化（Mode 0, 9MHz）
    │
    ▼
System_Init()             ← BSP_AT24CXX_Init() + BSP_W25QXX_Init()
    │
    ▼
OLED 显示 "Bootloader v1.0"
    │
    ▼
OtaBoot_CheckAndApply()   ← ★核心：检查AT24CXX标志，执行固件搬运
    │
    ├── 返回 OTA_BOOT_IDLE      → 无升级任务，继续
    ├── 返回 OTA_BOOT_SUCCESS   → 显示 "OTA Update OK!"，继续
    └── 返回 ERR_*              → 显示错误，死循环
    │
    ▼
Boot_CheckApp(APP_ADDRESS) ← 检查App区是否有有效固件
    │
    ├── 返回 0 (无效) → 显示 "No valid App\nWait for OTA..." → 死循环
    └── 返回 1 (有效) → 显示 "Jumping to App" → 延迟500ms
    │
    ▼
Boot_JumpToApp(APP_ADDRESS)← 关闭全局中断、重映射向量表、跳转
```

---

## 6. 核心模块代码分析

### 6.1 配置层 — boot_config.h + ota_config.h

#### boot_config.h — Bootloader 特有配置

```c
#define BOOTLOADER_SIZE         (24 * 1024)     // Bootloader 占用 24KB
#define FLASH_END_ADDR          0x08010000      // 64KB Flash 结束
```

**设计要点**：
- `BOOTLOADER_SIZE` 定义了 Bootloader 自身占用的 Flash 空间，用于编译期约束（Build 后检查 ELF 大小不超过此值）
- `FLASH_END_ADDR` 为硬件地址边界，在 `bsp_flash.c` 的擦除和写入中用作**越界保护**

#### ota_config.h — OTA 全局配置

```c
// ========== 内部 Flash 分区 ==========
#define BOOTLOADER_ADDR         0x08000000
#define APP_ADDRESS             0x08006000      // App 起始地址 24KB 处
#define APP_MAX_SIZE            (40 * 1024)     // App 最大 40KB
#define INTERNAL_FLASH_PAGE_SIZE    1024        // 内部 Flash 每页 1KB

// ========== 外部存储器 ==========
#define W25QXX_FIRMWARE_ADDR    0x000000        // W25QXX 固件起始地址
#define W25QXX_FIRMWARE_MAX     0x100000        // 预留 1MB 空间
#define OTA_INFO_ADDR           0x00            // AT24CXX 控制信息起始地址

// ========== OTA 控制标志 ==========
#define OTA_MAGIC               0xA5A5A5A5      // 魔数（4字节）
#define OTA_FLAG_NONE           0x00            // 无升级任务
#define OTA_FLAG_READY          0x01            // 有待搬运固件

// ========== 协议参数 ==========
#define OTA_MAX_DATA_LEN        1024            // 单帧最大数据长度

// ========== 升级控制信息结构体 ==========
typedef struct __attribute__((packed)) {
    uint32_t magic;          // 必须是 OTA_MAGIC (0xA5A5A5A5)
    uint8_t  flag;           // 升级标志：NONE / READY
    uint8_t  reserved[3];    // 补齐到 4 字节对齐
    uint32_t total_size;     // 固件总大小 (字节)
    uint32_t total_crc32;    // 整包 CRC32 校验值
} OtaInfo_t;                 // 共 16 字节
```

**结构体设计解析**：

| 字段 | 偏移 | 大小 | 说明 |
|------|------|------|------|
| `magic` | 0 | 4 | Magic Number `0xA5A5A5A5`，防止未初始化的 EEPROM 数据被误读 |
| `flag` | 4 | 1 | `0x00`=无任务，`0x01`=待搬运 |
| `reserved` | 5 | 3 | 保留字节，确保 `total_size` 4 字节对齐 |
| `total_size` | 8 | 4 | 固件总大小，搬运前校验合法性（`0 < size ≤ APP_MAX_SIZE`） |
| `total_crc32` | 12 | 4 | 整包 CRC32 值，用于搬运后完整性校验 |

**关键设计考虑**：
- `__attribute__((packed))` 防止编译器插入对齐填充，确保结构体精确 16 字节
- `magic` 字段使用 `0xA5A5A5A5`（二进制 `10100101` 重复），该值在空白 EEPROM（通常为 `0xFF`）中不会出现，有效区分"有任务"和"无任务"
- `reserved[3]` 手动补齐而非依赖编译器对齐，保证跨平台一致性

---

### 6.2 App 检测与跳转 — boot_jump.c

#### Boot_CheckApp() — App 合法性校验

```c
uint8_t Boot_CheckApp(uint32_t app_addr) {
    uint32_t stack_top     = *(volatile uint32_t*)app_addr;
    uint32_t reset_handler = *(volatile uint32_t*)(app_addr + 4);

    // 栈顶地址必须在 SRAM 范围：0x20000000 ~ 0x20004FFF (20KB)
    if ((stack_top & 0x2FFE0000) != 0x20000000) {
        return 0;
    }

    // ResetHandler 必须在 Flash 范围
    if (reset_handler < 0x08000000 || reset_handler > 0x08010000) {
        return 0;
    }

    return 1;
}
```

**ARM Cortex-M 向量表布局**（App 区首地址处）：
```
偏移       内容
+0x00      初始栈顶地址 (MSP initial value)  → stack_top
+0x04      Reset_Handler 入口地址             → reset_handler
+0x08      NMI_Handler
+0x0C      HardFault_Handler
...        ...
```

**校验逻辑**：
1. **栈顶地址校验**：`(stack_top & 0x2FFE0000) != 0x20000000`
   - STM32F103C8T6 的 SRAM 范围是 `0x20000000 ~ 0x20004FFF`（20KB）
   - 掩码 `0x2FFE0000` 将地址的高位保留 SRAM 基地址特征，`0x20000000` 匹配
2. **ResetHandler 地址校验**：必须在 `0x08000000 ~ 0x08010000`（内部 Flash 64KB 范围）

#### Boot_JumpToApp() — 安全跳转实现

```c
void Boot_JumpToApp(uint32_t app_addr) {
    typedef void (*pFunction)(void);
    pFunction JumpToApplication;
    uint32_t JumpAddress;

    if (Boot_CheckApp(app_addr) == 0) return;

    // 步骤1: 关闭所有中断
    __disable_irq();

    // 步骤2: 关闭 SysTick（避免跳转后触发 SysTick_Handler 跑飞）
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    // 步骤3: 清除所有 NVIC 中断使能与挂起标志
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;  // 中断清除使能寄存器
        NVIC->ICPR[i] = 0xFFFFFFFF;  // 中断清除挂起寄存器
    }

    // 步骤4: 重定位向量表 → App 首地址
    SCB->VTOR = app_addr;

    // 步骤5: 设置 MSP = App 栈顶
    __set_MSP(*(volatile uint32_t*)app_addr);

    // 步骤6: 跳转到 App 的 Reset_Handler
    JumpAddress = *(volatile uint32_t*)(app_addr + 4);
    JumpToApplication = (pFunction)JumpAddress;
    JumpToApplication();  // 从此不再返回

    while (1) {}
}
```

**跳转步骤详解**：

| 步骤 | 操作 | 目的 |
|------|------|------|
| 1 | `__disable_irq()` | 关闭全局中断（PRIMASK=1），防止跳转过程中被打断 |
| 2 | 清零 SysTick | SysTick 由 HAL 初始化时配置为 1ms 中断，不关闭会在跳转后进 HardFault |
| 3 | 清空 NVIC | 遍历 NVIC 的 8 组 32 位寄存器（Cortex-M3 最多 240 个中断线），清除所有使能和挂起 |
| 4 | `SCB->VTOR = app_addr` | 重映射向量表基地址到 App 区，此后所有中断/异常将使用 App 的向量表 |
| 5 | `__set_MSP(...)` | 设置主栈指针为 App 的初始栈顶地址，完成栈切换 |
| 6 | 函数指针跳转 | 读取 App 向量表偏移+4 处的 Reset_Handler 地址，通过函数指针调用 |

> ⚠️ **注意事项**：
> - 步骤 2~3 必须在 Bootloader 环境中完成，**不能在跳转后交给 App 处理**（因为跳转瞬间已不再执行 Bootloader 代码）
> - `NVIC->ICER[i]` / `NVIC->ICPR[i]` 每组 32 位对应 32 条中断线，共 8 组覆盖全部 240 条
> - 跳转前不开启 MSP 会导致 App 启动时栈错误

---

### 6.3 OTA 搬运引擎 — ota_boot.c

#### OtaBoot_CheckAndApply() — 核心搬运流程

```c
OtaBootState_t OtaBoot_CheckAndApply(void)
{
    OtaInfo_t info;

    // 步骤1: 从 AT24CXX 读取控制信息 (16字节)
    BSP_AT24CXX_Read(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    // 步骤2: Magic + Flag 双重校验
    if (info.magic != OTA_MAGIC || info.flag != OTA_FLAG_READY) {
        return OTA_BOOT_IDLE;  // 无升级任务
    }

    // 步骤3: 固件大小合法性检查
    if (info.total_size == 0 || info.total_size > APP_MAX_SIZE) {
        return OTA_BOOT_ERR_CRC;  // 异常数据拒绝搬运
    }

    // 步骤4: 擦除 App 区
    if (BSP_Flash_EraseApp(APP_ADDRESS, info.total_size) != BSP_FLASH_OK) {
        return OTA_BOOT_ERR_ERASE;
    }

    // 步骤5: 分块搬运 + 流水线 CRC32
    uint8_t chunk[256];
    uint32_t offset = 0;
    uint32_t crc = CRC32_Start();

    while (offset < info.total_size) {
        uint32_t remain = info.total_size - offset;
        uint16_t len = (remain > 256) ? 256 : (uint16_t)remain;

        BSP_W25QXX_Read(W25QXX_FIRMWARE_ADDR + offset, chunk, len);    // 从外部 Flash 读
        BSP_Flash_WriteData(APP_ADDRESS + offset, chunk, len);         // 写入内部 Flash
        crc = CRC32_UpdateBlock(crc, chunk, len);                      // 增量更新 CRC
        offset += len;
    }
    crc = CRC32_Final(crc);

    // 步骤6: CRC 完整性校验
    if (crc != info.total_crc32) {
        return OTA_BOOT_ERR_CRC;
    }

    // 步骤7: 清除升级标志→标记为已完成
    info.flag = OTA_FLAG_NONE;
    BSP_AT24CXX_Write(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    return OTA_BOOT_SUCCESS;
}
```

**流程解析**：

```
AT24CXX 读取 OtaInfo_t (16字节)
    ↓
Magic≠0xA5A5A5A5 或 Flag≠0x01?
    ├── 是 → OTA_BOOT_IDLE（无任务，正常启动）
    └── 否 → 继续
    ↓
total_size == 0 或 > 40KB?
    ├── 是 → OTA_BOOT_ERR_CRC（数据异常）
    └── 否 → 继续
    ↓
擦除内部 Flash App 区
    ├── 失败 → OTA_BOOT_ERR_ERASE
    └── 成功 → 继续
    ↓
循环搬运（每块 256 字节）:
    W25QXX 读取 → 内部 Flash 写入 → CRC32 增量更新
    ├── 写入失败 → OTA_BOOT_ERR_WRITE
    └── 继续直到搬运完毕
    ↓
CRC32 比对
    ├── 不匹配 → OTA_BOOT_ERR_CRC（保留 flag=READY，下次上电重试）
    └── 匹配 → 清除 flag=NONE → OTA_BOOT_SUCCESS
```

**关键设计**：

1. **搬运块大小 = 256 字节**：
   - 匹配 W25QXX 页大小（256B），一次页编程刚好一页
   - 内部 Flash 按半字（2 字节）写入，256 是偶数，不存在半字对齐问题
   - 栈上分配 `chunk[256]`，堆栈消耗可控

2. **流水线 CRC32 计算**：
   - 使用增量 CRC API（`Start → UpdateBlock → Final`），在搬运过程中同步计算
   - 无需"先搬运完 → 再读回 → 再计算 CRC"的二次遍历，节省时间

3. **失败处理策略**：
   - **CRC 失败时不清除 flag**：保留 `OTA_FLAG_READY`，下次上电重新搬运
   - **其他失败直接死循环**（在 `main.c` 的 `Handle_BootState()` 中处理）

#### OtaBootState_t 状态枚举

```c
typedef enum {
    OTA_BOOT_IDLE = 0,       // 无升级任务
    OTA_BOOT_ERASING,        // 正在擦除 App 区
    OTA_BOOT_COPYING,        // 正在搬运
    OTA_BOOT_VERIFYING,      // 正在校验
    OTA_BOOT_SUCCESS,        // 搬运成功
    OTA_BOOT_ERR_ERASE,      // 擦除失败
    OTA_BOOT_ERR_WRITE,      // 写入失败
    OTA_BOOT_ERR_CRC         // 校验失败
} OtaBootState_t;
```

---

### 6.4 内部 Flash 操作 — bsp_flash.c

#### BSP_Flash_EraseApp() — 擦除 App 区

```c
uint8_t BSP_Flash_EraseApp(uint32_t start_addr, uint32_t size) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;

    // ===== 防御性检查 =====
    if (start_addr < APP_ADDRESS)                    return BSP_FLASH_ERR_PARAM;
    if (start_addr % INTERNAL_FLASH_PAGE_SIZE != 0)  return BSP_FLASH_ERR_PARAM;
    if (size == 0)                                   return BSP_FLASH_ERR_PARAM;
    if (start_addr + size > FLASH_END_ADDR)          return BSP_FLASH_ERR_PARAM;

    // ===== 计算页数 =====
    uint32_t nb_pages = (size + INTERNAL_FLASH_PAGE_SIZE - 1) / INTERNAL_FLASH_PAGE_SIZE;

    // ===== 执行页擦除 =====
    HAL_FLASH_Unlock();
    erase_init.TypeErase    = FLASH_TYPEERASE_PAGES;
    erase_init.Banks        = FLASH_BANK_1;
    erase_init.PageAddress  = start_addr;
    erase_init.NbPages      = nb_pages;
    HAL_StatusTypeDef hal_ret = HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();

    return (hal_ret == HAL_OK) ? BSP_FLASH_OK : BSP_FLASH_ERR_ERASE;
}
```

**防御性检查清单**：

| 检查项 | 条件 | 目的 |
|--------|------|------|
| 地址下限 | `start_addr >= APP_ADDRESS` | 防止误擦 Bootloader 自身 |
| 页对齐 | `start_addr % 1024 == 0` | STM32F1 Flash 页擦除要求页对齐 |
| 大小非零 | `size > 0` | 防止空擦除 |
| 地址上限 | `start_addr + size ≤ FLASH_END_ADDR` | 防止越界到非 Flash 区域 |

#### BSP_Flash_WriteData() — 半字写入

```c
uint8_t BSP_Flash_WriteData(uint32_t addr, const uint8_t *data, uint32_t len) {
    // ===== 防御性检查 =====
    if (data == NULL)           return BSP_FLASH_ERR_PARAM;
    if (len == 0)               return BSP_FLASH_ERR_PARAM;
    if (addr < APP_ADDRESS)     return BSP_FLASH_ERR_PARAM;
    if (addr % 2 != 0)          return BSP_FLASH_ERR_PARAM;  // 半字对齐
    if (addr + len > FLASH_END_ADDR) return BSP_FLASH_ERR_PARAM;

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < len; i += 2) {
        // 组装半字：data[i] 为低字节，data[i+1] 为高字节（小端）
        uint16_t half_word  = data[i];
        half_word |= (uint16_t)((i + 1 < len) ? data[i + 1] : 0xFF) << 8;

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD,
                              addr + i, half_word) != HAL_OK) {
            HAL_FLASH_Lock();
            return BSP_FLASH_ERR_WRITE;
        }
    }

    HAL_FLASH_Lock();
    return BSP_FLASH_OK;
}
```

**关键注意**：
- **STM32F1 Flash 仅支持半字（16 位）编程**，不能按字节写入
- **奇数长度处理**：最后不足半字时，高位补 `0xFF`（Flash 擦除后的默认值即为 `0xFF`，写入 `0xFF` 不会改变 Flash 内容）
- **写入前必须先擦除**：Flash 只能从 `1→0`，不能从 `0→1`；要写 `0→1` 必须先整页擦除

---

### 6.5 外部 SPI Flash — bsp_w25qxx.c

#### SPI 底层通信函数

```c
// 合并发送：1 字节指令 + 3 字节 24 位地址 → 一次 HAL 调用
static inline void W25QXX_SendCmdAddr(uint8_t cmd, uint32_t addr) {
    uint8_t header[4];
    header[0] = cmd;
    header[1] = (uint8_t)(addr >> 16);
    header[2] = (uint8_t)(addr >> 8);
    header[3] = (uint8_t)(addr);
    HAL_SPI_Transmit(&hspi1, header, 4, HAL_MAX_DELAY);
}
```

**优化要点**：将"指令 + 24位地址"合并为单次 SPI 传输，避免多次 HAL 调用开销，减少 CS 操作间的空闲时钟。

#### W25QXX_Write() — 跨页自动分割写入

```c
void BSP_W25QXX_Write(uint32_t addr, const uint8_t *buf, uint32_t len) {
    uint32_t remain = len;
    uint32_t offset = 0;

    while (remain > 0) {
        // 计算当前页剩余空间
        uint16_t page_remain = W25QXX_PAGE_SIZE - (addr % W25QXX_PAGE_SIZE);
        if (remain < page_remain) page_remain = (uint16_t)remain;

        BSP_W25QXX_PageProgram(addr, buf + offset, page_remain);
        addr   += page_remain;
        offset += page_remain;
        remain -= page_remain;
    }
}
```

**页编程限制**：W25QXX 每次页编程（PageProgram, `0x02`）最多 256 字节，且不能跨物理页边界。此函数自动计算每页剩余空间并分段写入。

#### W25QXX_WaitBusy() — 带超时保护的状态轮询

```c
static void W25QXX_WaitBusy(void) {
    uint32_t timeout = 100000;
    uint8_t status;

    W25QXX_CS_LOW();
    W25QXX_SPI_WriteByte(W25QXX_CMD_READ_STATUS1);
    do {
        status = W25QXX_SPI_ReadByte();
        if (--timeout == 0) break;  // 超时保护
    } while (status & 0x01);        // BUSY 位 (bit0)
    W25QXX_CS_HIGH();
}
```

#### W25QXX 指令集一览

| 指令 | 代码 | 功能 |
|------|------|------|
| WRITE_ENABLE | `0x06` | 写使能（写/擦除前必须） |
| READ_STATUS1 | `0x05` | 读状态寄存器 |
| READ_DATA | `0x03` | 读数据（标准模式） |
| PAGE_PROGRAM | `0x02` | 页编程（≤256字节/页） |
| SECTOR_ERASE | `0x20` | 扇区擦除（4KB） |
| BLOCK_ERASE_64K | `0xD8` | 64KB 块擦除 |
| CHIP_ERASE | `0xC7` | 整片擦除 |
| JEDEC_ID | `0x9F` | 读厂商/设备 ID |

---

### 6.6 EEPROM 控制信息 — bsp_at24cxx.c

#### AT24CXX 型号自适应配置

```c
#define AT24CXX_TYPE        0x02  // AT24C256

// 内存地址宽度自动选择
#if AT24CXX_TYPE > 0x10
#define AT24CXX_MEM_ADDR_SIZE  I2C_MEMADD_SIZE_16BIT  // >2KB: 16位地址
#else
#define AT24CXX_MEM_ADDR_SIZE  I2C_MEMADD_SIZE_8BIT   // ≤2KB: 8位地址
#endif

// 页大小自动选择
#if AT24CXX_TYPE <= 0x02
#define AT24CXX_PAGE_SIZE   8    // AT24C01/02: 8字节/页
#elif AT24CXX_TYPE <= 0x10
#define AT24CXX_PAGE_SIZE   16   // AT24C04/08/16: 16字节/页
#elif AT24CXX_TYPE <= 0x40
#define AT24CXX_PAGE_SIZE   32   // AT24C32/64: 32字节/页
#else
#define AT24CXX_PAGE_SIZE   64   // AT24C128/256: 64字节/页
#endif
```

**型号说明**：`AT24CXX_TYPE` 的值含义——`0x02`=AT24C02，`0x10`=AT24C16，`0x100`=AT24C256。修改此宏即可适配不同容量的 EEPROM。

#### BSP_AT24CXX_Write() — 跨页自动分割

```c
void BSP_AT24CXX_Write(uint16_t addr, const uint8_t *buf, uint16_t len) {
    uint16_t remain = len;
    uint16_t offset = 0;

    while (remain > 0) {
        uint16_t page_remain = AT24CXX_PAGE_SIZE - (addr % AT24CXX_PAGE_SIZE);
        if (remain < page_remain) page_remain = remain;

        HAL_I2C_Mem_Write(&hi2c1, AT24CXX_I2C_ADDR, addr,
                          AT24CXX_MEM_ADDR_SIZE, (uint8_t *)buf + offset,
                          page_remain, HAL_MAX_DELAY);

        addr   += page_remain;
        offset += page_remain;
        remain -= page_remain;

        HAL_Delay(5);  // 等待内部写周期（典型 5ms）
    }
}
```

**EEPROM 写入特性**：
- AT24CXX 系列内部写周期约 5ms，每写完一页必须等待
- 跨页写入需手动分割（不同于 W25QXX 的"页内回绕"行为）
- I2C 地址：7 位地址 `0x50`（`1010000`），写入时 HAL 自动扩展为 8 位 `0xA0`/`0xA1`

---

### 6.7 CRC32 校验 — crc32.c

#### CRC32 标准参数（与 zlib/PNG/ZIP/Ethernet 兼容）

| 参数 | 值 |
|------|-----|
| 多项式 | `0xEDB88320`（反射 `0x04C11DB7`） |
| 初始值 | `0xFFFFFFFF` |
| 输出异或 | `0xFFFFFFFF` |
| 测试向量 | `CRC32("123456789")` = `0xCBF43926` |

#### 增量 API 设计

```c
// 流式计算三步骤
uint32_t crc = CRC32_Start();           // 返回 0xFFFFFFFF
crc = CRC32_UpdateBlock(crc, data, len); // 批量喂数据
crc = CRC32_Final(crc);                 // 取反得到最终 CRC
```

**计算核心**（逐比特法，处理一个字节）：
```c
uint32_t CRC32_Update(uint32_t crc, uint8_t byte) {
    crc ^= (uint32_t)byte;
    for (int j = 0; j < 8; j++) {
        crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
    }
    return crc;
}
```

**为什么用逐比特法而非查表法**：
- STM32F1 仅有 64KB Flash，8KB（256×32bit）查表空间代价大
- 逐比特法虽慢但在 Cortex-M3 @72MHz 下足够：每个字节 8 次循环，每循环约 3~4 条指令
- `UpdateBlock()` 批量处理减少函数调用开销

---

### 6.8 OLED 显示屏驱动 — OLED.c

#### 通信方式：软件 I2C（PB8=SCL, PB9=SDA）

```c
#define OLED_SCL_PORT  GPIOB
#define OLED_SDA_PORT  GPIOB
#define OLED_SCL_PIN   GPIO_PIN_8
#define OLED_SDA_PIN   GPIO_PIN_9
```

**为什么不使用硬件 I2C**：
- 硬件 I2C1 已被 AT24CXX EEPROM 占用
- 软件 I2C 对低速 OLED 屏足够（无需高速通信）
- GPIO 配置为 **开漏输出**（`GPIO_MODE_OUTPUT_OD`），模拟 I2C 总线

#### 软件 I2C 实现核心

```c
static void OLED_I2C_Start(void) {
    OLED_SDA_H(); OLED_SCL_H(); OLED_I2C_Delay();
    OLED_SDA_L(); OLED_I2C_Delay(); OLED_SCL_L();
}

static void OLED_I2C_SendByte(uint8_t Byte) {
    for (int i = 0; i < 8; i++) {
        OLED_SCL_L();
        if (Byte & 0x80) OLED_SDA_H(); else OLED_SDA_L();
        Byte <<= 1;
        OLED_SCL_H();
    }
    // 第9个时钟：释放 SDA 等待 ACK
    OLED_SCL_L(); OLED_SDA_H();
    OLED_SCL_H(); OLED_SCL_L();
}
```

#### 显存机制

```c
uint8_t OLED_DisplayBuf[128][8];  // 128列 × 8页 = 128×64像素
```

OLED 使用 **页寻址模式**，每页 8 个像素行，共 8 页（64 行）。所有绘制函数只操作显存数组，调用 `OLED_Update()` 后才整屏刷新到硬件。

#### OLED 初始化序列（关键命令）

```
0xAE  → 关闭显示
0xD5/0x80 → 时钟分频比/振荡器频率
0xA8/0x3F → 多路复用率（64 行）
0xD3/0x00 → 显示偏移 = 0
0x40  → 显示起始行 = 0
0xA1  → 段重映射（左右正常）
0xC8  → COM 扫描方向（上下正常）
0xDA/0x12 → COM 引脚硬件配置
0x81/0xCF → 对比度 = 207
0xD9/0xF1 → 预充电周期
0xDB/0x30 → VCOMH 电压
0xA4  → 正常显示（非全屏点亮）
0xA6  → 正常显示（非反色）
0x8D/0x14 → 启用充电泵（3.3V供电必须）
0xAF  → 开启显示
```

---

## 7. 外设初始化配置

### 7.1 系统时钟 (SystemClock_Config)

| 参数 | 值 |
|------|-----|
| 时钟源 | HSE（外部高速晶振，通常 8MHz） |
| PLL 源 | HSE |
| PLL 倍频 | ×9 |
| 系统时钟 SYSCLK | 72MHz |
| AHB 分频 | ÷1 = 72MHz |
| APB1 分频 | ÷2 = 36MHz |
| APB2 分频 | ÷1 = 72MHz |
| Flash 等待周期 | 2（72MHz 必须） |

### 7.2 SPI1 — W25QXX

| 参数 | 值 | 说明 |
|------|-----|------|
| 模式 | Master | 全双工主机 |
| 数据位宽 | 8bit | |
| 时钟极性 CPOL | Low (0) | SPI Mode 0 |
| 时钟相位 CPHA | 1 Edge | 第 1 个边沿采样 |
| NSS | Software | 软件控制 CS |
| 波特率分频 | ÷8 | 72MHz ÷ 8 = 9MHz |
| 引脚 | PA5(SCK), PA6(MISO), PA7(MOSI) | |

### 7.3 I2C1 — AT24CXX

| 参数 | 值 |
|------|-----|
| 时钟速度 | 100KHz（标准模式） |
| 占空比 | DutyCycle_2 |
| 寻址模式 | 7 位地址 |
| 引脚 | PB6(SCL), PB7(SDA) |
| 引脚模式 | 复用开漏（AF_OD） |

### 7.4 GPIO 配置

| 引脚 | 模式 | 速度 | 默认输出 |
|------|------|------|----------|
| PA1 (LED) | 推挽输出 | Low | 低（灯灭） |
| PA4 (W25CS) | 推挽输出 | Low | 高（不选中） |
| PB8 (OLED_CL) | 开漏输出 | Low | 高 |
| PB9 (OLED_DA) | 开漏输出 | Low | 高 |

### 7.5 中断向量

在 `stm32f1xx_it.c` 中，仅 **SysTick_Handler** 被有效使用（调用 `HAL_IncTick()`），其余异常处理函数（NMI、HardFault、MemManage、BusFault、UsageFault）均为空死循环。这是 Bootloader 的典型设计——不需要处理复杂中断，出错时安全停机。

---

## 8. 链接脚本与内存布局

### STM32F103XX_FLASH.ld 关键信息

```
MEMORY
{
    RAM   (xrw) : ORIGIN = 0x20000000, LENGTH = 20K
    FLASH (rx) : ORIGIN = 0x08000000, LENGTH = 64K
}

_estack = 0x20000000 + 20K = 0x20005000   // 栈顶 = SRAM 末尾
_Min_Heap_Size = 0x200 (512B)             // 最小堆
_Min_Stack_Size = 0x400 (1KB)             // 最小栈
```

**段布局**：
```
FLASH:
  .isr_vector       → 中断向量表
  .text             → 代码段
  .rodata           → 只读数据
  .ARM / .ARM.extab → 异常处理表
  .init_array       → C++ 全局构造
  .data (LMA)       → 已初始化数据的 Flash 副本

RAM:
  .data             → 已初始化全局变量
  .bss              → 未初始化全局变量（零初始化）
  ._user_heap_stack → 堆 + 栈
```

---

## 9. 构建系统 — CMake

### 工具链

| 工具链文件 | 用途 |
|-----------|------|
| `cmake/gcc-arm-none-eabi.cmake` | GNU ARM Embedded Toolchain（主用） |
| `cmake/starm-clang.cmake` | ARM Clang（备用） |

### 编译选项

```cmake
-Os                          # 优化大小
-g0                          # 不包含调试信息（减少体积）
-ffunction-sections          # 每个函数独立段
-fdata-sections              # 每个数据独立段
-fno-common                  # 禁止 common 段
-fno-builtin                 # 禁用内建函数
-Wl,--gc-sections            # 链接时移除未使用段
-Wl,-Map=Bootloader.map      # 生成 map 文件
```

### 构建步骤

```bash
# 配置 (Debug)
cmake --preset Debug

# 构建
cmake --build build/Debug

# 或直接
cmake --preset Release
cmake --build build/Release
```

### 源文件列表（CMakeLists.txt）

```
可执行目标: Bootloader

用户源文件:
  BSP/Src/bsp_w25qxx.c      # W25QXX 驱动
  BSP/Src/bsp_at24cxx.c     # AT24CXX 驱动
  BSP/Src/bsp_flash.c       # 内部 Flash 操作
  BSP/Src/OLED.c            # OLED 显示
  Ota/Src/ota_boot.c        # OTA 搬运
  Utils/Src/crc32.c         # CRC32 校验
  Core/Src/boot_jump.c      # App 跳转

用户头文件目录:
  BSP/Inc, Ota/Inc, Utils/Inc, Core/Inc

STM32CubeMX 子项目 (stm32cubemx):
  Core/Src/main.c, gpio.c, i2c.c, spi.c,
  system_stm32f1xx.c, stm32f1xx_it.c, ...
  Drivers/STM32F1xx_HAL_Driver/Src/*.c
```

---

## 10. OTA 数据流与协议

### 10.1 升级数据流

```
┌──────────────┐     ①写入固件      ┌──────────────┐
│  上位机/     │ ─────────────────→ │   W25QXX      │
│  通信模块    │                    │ (SPI Flash)   │
│              │     ②写入升级信息   └──────────────┘
│              │ ─────────────────→ ┌──────────────┐
│              │                    │  AT24CXX      │
└──────────────┘                    │ (EEPROM)      │
                                    └──────────────┘
                                           │
                                   ③上电/复位触发
                                           │
                                           ▼
                                    ┌──────────────┐
                                    │  Bootloader   │
                                    │  读取 AT24CXX │
                                    │  搬运+校验+跳 │
                                    └──────┬───────┘
                                           │
                                   ④写入内部 Flash
                                           │
                                           ▼
                                    ┌──────────────┐
                                    │ 内部 Flash    │
                                    │ App 分区      │
                                    │ (0x08006000)  │
                                    └──────────────┘
```

### 10.2 升级前置条件

上位机在触发升级前，需要完成两步写入：

1. **将固件二进制写入 W25QXX**（从地址 `0x000000` 开始）
2. **将 `OtaInfo_t` 写入 AT24CXX**（从地址 `0x00` 开始）：
   ```c
   OtaInfo_t info = {
       .magic      = 0xA5A5A5A5,    // 魔数
       .flag       = 0x01,           // READY
       .reserved   = {0, 0, 0},
       .total_size = firmware_size,  // 固件实际字节数
       .total_crc32 = crc32_value    // 整包 CRC32
   };
   BSP_AT24CXX_Write(0x00, (uint8_t*)&info, 16);
   ```

3. **复位 MCU**，Bootloader 重新运行，自动检测并搬运。

### 10.3 升级状态机

```
  ┌─────────┐   magic!=0xA5A5A5A5   ┌──────┐
  │ 上电启动 │ ────────────────────→ │ IDLE │ → 正常跳转 App
  └────┬────┘  或 flag!=READY        └──────┘
       │
       │ magic=OK && flag=READY
       ▼
  ┌──────────┐
  │ 校验大小  │ size==0 或 size>40KB → ERR_CRC（死循环）
  └────┬─────┘
       │ 通过
       ▼
  ┌──────────┐
  │ 擦除 App │ 失败 → ERR_ERASE（死循环）
  └────┬─────┘
       │ 成功
       ▼
  ┌──────────┐
  │ 分块搬运  │ 写入失败 → ERR_WRITE（死循环）
  └────┬─────┘
       │ 搬运完毕
       ▼
  ┌──────────┐
  │ CRC 校验  │ 不匹配 → ERR_CRC（死循环）
  └────┬─────┘
       │ 匹配
       ▼
  ┌──────────┐
  │ 清除 Flag │ 写 AT24CXX: flag=NONE
  └────┬─────┘
       │
       ▼
  ┌──────────┐
  │ SUCCESS  │ → 跳转 App
  └──────────┘
```

---

## 11. 错误状态码汇总

### OTA 搬运状态 (OtaBootState_t)

| 枚举值 | 含义 | 系统行为 |
|--------|------|----------|
| `OTA_BOOT_IDLE` | 无升级任务 | 正常跳转 App |
| `OTA_BOOT_SUCCESS` | 搬运校验成功 | OLED 显示 "OTA Update OK!"，跳转 App |
| `OTA_BOOT_ERR_ERASE` | App 区擦除失败 | OLED 显示 "Error: Erase"，死循环 |
| `OTA_BOOT_ERR_WRITE` | 写入内部 Flash 失败 | OLED 显示 "Error: Write"，死循环 |
| `OTA_BOOT_ERR_CRC` | CRC32 校验不匹配 | OLED 显示 "Error: CRC"，死循环 |

### Flash 操作错误码 (BSP_FLASH)

| 枚举值 | 含义 |
|--------|------|
| `BSP_FLASH_OK` | 操作成功 |
| `BSP_FLASH_ERR_PARAM` | 参数不合法（非对齐、越界、空指针等） |
| `BSP_FLASH_ERR_ERASE` | HAL 擦除操作失败 |
| `BSP_FLASH_ERR_WRITE` | HAL 写入操作失败 |

---

## 12. OLED 状态显示说明

| 显示内容 | 含义 |
|----------|------|
| `Bootloader v1.0` | 启动成功 |
| `OTA Update OK!` | 升级搬运校验成功 |
| `Error: Erase` | App 区擦除失败（死循环） |
| `Error: Write` | 内部 Flash 写入失败（死循环） |
| `Error: CRC` | CRC 校验失败（死循环） |
| `No valid App / Wait for OTA...` | App 区无有效固件，等待升级 |
| `Jumping to App` | 即将跳转到 App |

---

## 13. 注意事项

1. **跳转前必须关闭全部中断并重置 SysTick**，否则 App 启动后可能进入 HardFault；
2. **内部 Flash 必须先擦除再写入**，且只能半字（2 字节对齐）写入；
3. **升级固件大小不能超过 40KB**（`APP_MAX_SIZE`），否则会被判定为非法；
4. **CRC 校验失败时不清除 flag**，确保下次上电可自动重试升级；
5. **AT24CXX 每次写页后需等待约 5ms** 的写周期；
6. **Bootloader 编译产物不能超过 24KB**，否则会覆盖 App 区。

---

## 附录：关键地址速查

| 宏定义 | 值 | 说明 |
|--------|-----|------|
| `BOOTLOADER_ADDR` | `0x08000000` | Bootloader 起始地址 |
| `APP_ADDRESS` | `0x08006000` | App 起始地址（偏移 24KB） |
| `FLASH_END_ADDR` | `0x08010000` | Flash 末地址（64KB） |
| `BOOTLOADER_SIZE` | 24KB | Bootloader 最大占用 |
| `APP_MAX_SIZE` | 40KB | App 最大允许大小 |
| `INTERNAL_FLASH_PAGE_SIZE` | 1KB | 内部 Flash 页大小 |
| `W25QXX_FIRMWARE_ADDR` | `0x000000` | W25QXX 固件起始地址 |
| `W25QXX_FIRMWARE_MAX` | 1MB | W25QXX 固件预留空间 |
| `OTA_INFO_ADDR` | `0x00` | AT24CXX 控制信息地址 |
| `OTA_MAGIC` | `0xA5A5A5A5` | 升级魔数 |
| `OTA_FLAG_READY` | `0x01` | 待升级标志 |
| `OTA_FLAG_NONE` | `0x00` | 无升级标志 |