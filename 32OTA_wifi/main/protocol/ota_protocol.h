#ifndef OTA_PROTOCOL_H
#define OTA_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* ==================== 协议帧常量 ==================== */
#define OTA_MAGIC_0         0xAA
#define OTA_MAGIC_1         0x55
#define OTA_TAIL            0x5A

#define OTA_TYPE_PREPARE    0x01
#define OTA_TYPE_DATA       0x02
#define OTA_TYPE_FINISH     0x03
#define OTA_TYPE_ACK        0x81

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 增量 CRC32 计算器（可重入）
 *
 * 用于大块固件的分段校验，中间可穿插任务看门狗复位，避免一次性长计算。
 */
typedef struct {
    uint32_t crc;
} ota_crc32_t;

void     ota_crc32_init(ota_crc32_t *crc);
void     ota_crc32_update(ota_crc32_t *crc, const uint8_t *data, size_t len);
uint32_t ota_crc32_finalize(ota_crc32_t *crc);

/* 一次性 CRC32（小数据块便捷接口，如单帧校验） */
uint32_t ota_crc32(const uint8_t *data, size_t len);

size_t ota_frame_build(uint8_t *buf, uint8_t type, uint16_t seq,
                       uint32_t offset, const uint8_t *data, uint16_t len);

/**
 * @brief ACK 帧解析器（可重入）
 *
 * 实例由调用方持有，避免使用全局状态，便于多任务复用与单元测试。
 */
typedef struct {
    uint8_t  state;
    uint16_t seq;
} ota_ack_parser_t;

void ota_ack_parser_init(ota_ack_parser_t *parser);
bool ota_ack_parser_feed(ota_ack_parser_t *parser, uint8_t ch, uint16_t expected_seq);

#ifdef __cplusplus
}
#endif

#endif /* OTA_PROTOCOL_H */