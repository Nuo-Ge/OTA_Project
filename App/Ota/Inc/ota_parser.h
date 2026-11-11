#ifndef __OTA_PARSER_H__
#define __OTA_PARSER_H__

#include <stdint.h>
#include "ota_protocol.h"

/* 解析器状态 */
typedef enum {
    PARSER_WAIT_MAGIC1,      /* 等待 0xAA */
    PARSER_WAIT_MAGIC2,      /* 等待 0x55 */
    PARSER_WAIT_TYPE,        /* 等待类型 */
    PARSER_WAIT_SEQ,         /* 等待序号（2 字节）*/
    PARSER_WAIT_OFFSET,      /* 等待偏移（4 字节）*/
    PARSER_WAIT_LENGTH,      /* 等待长度（2 字节）*/
    PARSER_WAIT_DATA,        /* 等待数据 */
    PARSER_WAIT_CRC,         /* 等待 CRC32（4 字节）*/
    PARSER_WAIT_TAIL,        /* 等待帧尾 0x5A */
} OtaParserState_t;

/* 解析器上下文 */
typedef struct {
    OtaParserState_t state;      /* 当前状态 */
    OtaFrame_t       frame;      /* 正在拼接的帧 */
    uint16_t         field_cnt;  /* 当前字段已收多少字节 */
    uint16_t         data_cnt;   /* 数据已收多少字节 */
    uint32_t         crc;        /* 当前 CRC32 值 */
    uint32_t         field_tmp;  /* 当前字段临时累加值 */
} OtaParser_t;

/* 初始化解析器 */
void OtaParser_Init(OtaParser_t *p);

/* 喂一个字节给解析器。
 * 返回：
 *   0 = 还没拼出完整帧
 *   1 = 拼出一帧完整的（放在 p->frame 里）
 *  -1 = 错误（帧尾错 / CRC 错）
 *  -2 = 帧尾错误
 */
int OtaParser_Feed(OtaParser_t *p, uint8_t ch);

#endif /* __OTA_PARSER_H__ */