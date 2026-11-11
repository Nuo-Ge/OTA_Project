#ifndef __OTA_PROTOCOL_H__
#define __OTA_PROTOCOL_H__

#include <stdint.h>
#include "ota_config.h"

/* ==================== 帧头/帧尾常量 ==================== */
#define OTA_MAGIC_0         0xAA
#define OTA_MAGIC_1         0x55
#define OTA_TAIL            0x5A

/* 固定头长度（不含 Data 和 CRC/Tail） */
#define OTA_HEADER_LEN      11    /* Magic(2) + Type(1) + Seq(2) + Offset(4) + Length(2) */
#define OTA_CRC_LEN         4     /* CRC32 校验值长度 */
#define OTA_TAIL_LEN        1     /* 帧尾长度 */
#define OTA_FIXED_LEN       (OTA_HEADER_LEN + OTA_CRC_LEN + OTA_TAIL_LEN)  /* 16 字节 */

/* ==================== 帧类型 ==================== */
#define OTA_TYPE_PREPARE    0x01    /* 准备升级 */
#define OTA_TYPE_DATA       0x02    /* 数据帧 */
#define OTA_TYPE_FINISH     0x03    /* 完成升级 */
#define OTA_TYPE_ACK        0x81    /* 肯定应答 */
#define OTA_TYPE_NACK       0x82    /* 否定应答 */

/* ==================== 协议帧结构 ==================== */
typedef struct {
    uint8_t  type;                      /* 帧类型 */
    uint16_t seq;                       /* 包序号（大端序解析后） */
    uint32_t offset;                    /* 数据偏移（大端序解析后） */
    uint16_t length;                    /* 数据长度（大端序解析后） */
    uint8_t  data[OTA_MAX_DATA_LEN];    /* 数据体 */
    uint32_t crc32;                     /* CRC32 校验值 */
} OtaFrame_t;

#endif /* __OTA_PROTOCOL_H__ */