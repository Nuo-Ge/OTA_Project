#ifndef OTA_CONFIG_H
#define OTA_CONFIG_H

#include "driver/gpio.h"
#include "driver/uart.h"
#include "protocol/ota_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==================== 版本元数据 ==================== */
#define OTA_SW_VERSION_MAJOR    1
#define OTA_SW_VERSION_MINOR    0
#define OTA_SW_VERSION_PATCH    0
#define OTA_SW_VERSION_STR      "1.0.0"

/* ==================== 硬件引脚 ==================== */
#define OTA_UART_NUM        UART_NUM_1
#define OTA_TX_PIN          GPIO_NUM_5
#define OTA_RX_PIN          GPIO_NUM_6
#define OTA_BAUD_RATE       115200
#define OTA_UART_RX_BUF     4096

/* ==================== 传输参数 ==================== */
#define OTA_CHUNK_SIZE      1024
#define OTA_MAX_FIRMWARE    (1024 * 1024)   /* 1MB */

/* ==================== 唤醒命令 ==================== */
#define OTA_WAKEUP_CMD      "START_OTA\r\n"
#define OTA_WAKEUP_LEN      11
#define OTA_LINE_CLEAN      "\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n\r\n"
#define OTA_LINE_CLEAN_LEN  20

/* ==================== 时序参数 ==================== */
#define OTA_UART_STABLE_MS  200             /* TX 引脚拉高稳定时间 */
#define OTA_HANDSHAKE_MAX   10              /* 握手最大重试次数 */
#define OTA_HANDSHAKE_TO    1000            /* 握手 ACK 超时 (ms) */
#define OTA_DATA_RETRY_MAX  3               /* 数据帧最大重试次数 */
#define OTA_DATA_ACK_TO     1000            /* 数据 ACK 超时 (ms) */
#define OTA_FINISH_ACK_TO   3000            /* 结束 ACK 超时 (ms) */
#define OTA_TX_DONE_TO      100             /* 发送完成超时 (ms) */
#define OTA_STARTUP_DELAY   2000            /* 上电后等待时间 (ms) */

/* ==================== 线程安全 ==================== */
#define OTA_MUTEX_TIMEOUT_MS 100            /* 互斥锁等待超时 (ms) */

/* ==================== 固件数组符号 ==================== */
// extern const uint8_t _binary_App_bin_start[];
// extern const uint8_t _binary_App_bin_end[];

#ifdef __cplusplus
}
#endif

#endif /* OTA_CONFIG_H */