#include "ota_protocol.h"
#include <string.h>

/* ==================== CRC32（增量 + 一次性） ==================== */
void ota_crc32_init(ota_crc32_t *crc)
{
    if (crc) crc->crc = 0xFFFFFFFFU;
}

void ota_crc32_update(ota_crc32_t *crc, const uint8_t *data, size_t len)
{
    if (!crc || (!data && len)) return;

    uint32_t c = crc->crc;
    for (size_t i = 0; i < len; i++) {
        c ^= data[i];
        for (int j = 0; j < 8; j++) {
            c = (c & 1U) ? ((c >> 1) ^ 0xEDB88320U) : (c >> 1);
        }
    }
    crc->crc = c;
}

uint32_t ota_crc32_finalize(ota_crc32_t *crc)
{
    if (!crc) return 0;
    return crc->crc ^ 0xFFFFFFFFU;
}

uint32_t ota_crc32(const uint8_t *data, size_t len)
{
    ota_crc32_t crc;
    ota_crc32_init(&crc);
    ota_crc32_update(&crc, data, len);
    return ota_crc32_finalize(&crc);
}

size_t ota_frame_build(uint8_t *buf, uint8_t type, uint16_t seq,
                       uint32_t offset, const uint8_t *data, uint16_t len)
{
    uint16_t i = 0;

    buf[i++] = OTA_MAGIC_0;
    buf[i++] = OTA_MAGIC_1;
    buf[i++] = type;
    buf[i++] = (uint8_t)(seq >> 8);
    buf[i++] = (uint8_t)(seq & 0xFF);
    buf[i++] = (uint8_t)(offset >> 24);
    buf[i++] = (uint8_t)(offset >> 16);
    buf[i++] = (uint8_t)(offset >> 8);
    buf[i++] = (uint8_t)(offset & 0xFF);
    buf[i++] = (uint8_t)(len >> 8);
    buf[i++] = (uint8_t)(len & 0xFF);

    if (len && data) {
        memcpy(&buf[i], data, len);
        i += len;
    }

    uint32_t crc = ota_crc32(&buf[2], i - 2);
    buf[i++] = (uint8_t)(crc >> 24);
    buf[i++] = (uint8_t)(crc >> 16);
    buf[i++] = (uint8_t)(crc >> 8);
    buf[i++] = (uint8_t)(crc & 0xFF);
    buf[i++] = OTA_TAIL;

    return i;
}

/* ==================== ACK 帧状态机 ==================== */
typedef enum {
    ACK_ST_MAGIC0 = 0,
    ACK_ST_MAGIC1,
    ACK_ST_TYPE,
    ACK_ST_SEQ_H,
    ACK_ST_SEQ_L,
} ack_state_t;

void ota_ack_parser_init(ota_ack_parser_t *parser)
{
    if (!parser) return;
    parser->state = ACK_ST_MAGIC0;
    parser->seq   = 0;
}

bool ota_ack_parser_feed(ota_ack_parser_t *parser, uint8_t ch, uint16_t expected_seq)
{
    if (!parser) return false;

    switch (parser->state) {
    case ACK_ST_MAGIC0:
        if (ch == OTA_MAGIC_0) parser->state = ACK_ST_MAGIC1;
        break;
    case ACK_ST_MAGIC1:
        if (ch == OTA_MAGIC_1) parser->state = ACK_ST_TYPE;
        else parser->state = (ch == OTA_MAGIC_0) ? ACK_ST_MAGIC1 : ACK_ST_MAGIC0;
        break;
    case ACK_ST_TYPE:
        if (ch == OTA_TYPE_ACK) parser->state = ACK_ST_SEQ_H;
        else parser->state = (ch == OTA_MAGIC_0) ? ACK_ST_MAGIC1 : ACK_ST_MAGIC0;
        break;
    case ACK_ST_SEQ_H:
        parser->seq   = (uint16_t)ch << 8;
        parser->state = ACK_ST_SEQ_L;
        break;
    case ACK_ST_SEQ_L:
        parser->seq  |= ch;
        parser->state = (ch == OTA_MAGIC_0) ? ACK_ST_MAGIC1 : ACK_ST_MAGIC0;
        if (parser->seq == expected_seq) return true;
        break;
    default:
        parser->state = ACK_ST_MAGIC0;
        break;
    }
    return false;
}