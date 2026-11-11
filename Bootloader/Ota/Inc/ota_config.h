#ifndef __OTA_CONFIG_H__
#define __OTA_CONFIG_H__

#include <stdint.h>

/* ================== 内部 Flash 分区配置 ================== */
#define BOOTLOADER_ADDR         0x08000000
#define APP_ADDRESS             0x08006000      /* App 起始地址 24KB 处 */
#define APP_MAX_SIZE            (40 * 1024)     /* App 最大 40KB */

/* 内部 Flash 物理参数 (C8T6) */
#define INTERNAL_FLASH_PAGE_SIZE    1024        /* 内部 Flash 每页 1KB */

/* ================== 外部 W25QXX 固件存储区 ================== */
#define W25QXX_FIRMWARE_ADDR    0x000000        /* W25QXX 中固件的起始地址 */
#define W25QXX_FIRMWARE_MAX     0x100000        /* W25QXX 中给固件预留 1MB 空间 */

/* ================== 外部 AT24CXX 控制信息区 ================== */
#define OTA_INFO_ADDR           0x00            /* AT24CXX 中控制信息的起始地址 */

/* ================== OTA 控制标志 ================== */
#define OTA_MAGIC               0xA5A5A5A5
#define OTA_FLAG_NONE           0x00
#define OTA_FLAG_READY          0x01

/* ================== OTA 通信协议 ================== */
#define OTA_MAX_DATA_LEN        1024            /* 单帧最大数据长度 */

/* ================== 升级控制信息结构体 ================== */
typedef struct __attribute__((packed)) {
    uint32_t magic;          /* 必须是 OTA_MAGIC */
    uint8_t  flag;           /* 升级标志：NONE / READY */
    uint8_t  reserved[3];    /* 补齐到 4 字节对齐 */
    uint32_t total_size;     /* 固件总大小 */
    uint32_t total_crc32;    /* 整包 CRC32 */
} OtaInfo_t;                 /* 共 16 字节 */

#endif /* __OTA_CONFIG_H__ */