#include "ota_parser.h"
#include "crc32.h"
#include <string.h>

/* 初始化 */
void OtaParser_Init(OtaParser_t *p)
{
    // 初始化解析器上下文
    memset(p, 0, sizeof(OtaParser_t));
    p->state = PARSER_WAIT_MAGIC1;
}

/* 喂一个字节，返回：0 未完成 / 1 帧完整 / -1 CRC 错 / -2 帧尾错 */
int OtaParser_Feed(OtaParser_t *p, uint8_t ch)
{
    switch (p->state) {

    /* ============ 等待 Magic 第 1 字节 0xAA ============ */
    case PARSER_WAIT_MAGIC1:
        if (ch == OTA_MAGIC_0) {
            p->state = PARSER_WAIT_MAGIC2;
        }
        /* 否则保持原状态 */
        return 0;

    /* ============ 等待 Magic 第 2 字节 0x55 ============ */
    case PARSER_WAIT_MAGIC2:
        if (ch == OTA_MAGIC_1) {
            p->state = PARSER_WAIT_TYPE;
            p->crc = CRC32_Start();       /* 帧头对了，启动 CRC */
        } else {
            p->state = PARSER_WAIT_MAGIC1; /* 不是 0x55，退回起点 */
        }
        return 0;

    /* ============ 读取 Type ============ */
    case PARSER_WAIT_TYPE:
        p->frame.type = ch;
        p->crc = CRC32_Update(p->crc, ch);   /* 更新 CRC */
        p->state = PARSER_WAIT_SEQ;
        p->field_cnt = 0;
        p->field_tmp = 0;
        return 0;

    /* ============ 读取 Seq（2 字节，大端序）============ */
    case PARSER_WAIT_SEQ:
        p->field_tmp = (p->field_tmp << 8) | ch;   /* 高位先到 */
        p->crc = CRC32_Update(p->crc, ch);
        p->field_cnt++;
        if (p->field_cnt == 2) {
            p->frame.seq = (uint16_t)p->field_tmp;
            p->field_cnt = 0;
            p->field_tmp = 0;
            p->state = PARSER_WAIT_OFFSET;
        }
        return 0;

    /* ============ 读取 Offset（4 字节，大端序）============ */
    case PARSER_WAIT_OFFSET:
        p->field_tmp = (p->field_tmp << 8) | ch;
        p->crc = CRC32_Update(p->crc, ch);
        p->field_cnt++;
        if (p->field_cnt == 4) {
            p->frame.offset = p->field_tmp;
            p->field_cnt = 0;
            p->field_tmp = 0;
            p->state = PARSER_WAIT_LENGTH;
        }
        return 0;

    /* ============ 读取 Length（2 字节，大端序）============ */
    case PARSER_WAIT_LENGTH:
        p->field_tmp = (p->field_tmp << 8) | ch;
        p->crc = CRC32_Update(p->crc, ch);
        p->field_cnt++;
        if (p->field_cnt == 2) {
            p->frame.length = (uint16_t)p->field_tmp;
            p->field_cnt = 0;
            p->field_tmp = 0;
            p->data_cnt = 0;

            /* 数据长度为 0，跳过 DATA 直接读 CRC */
            if (p->frame.length == 0) {
                p->state = PARSER_WAIT_CRC;
            } else {
                p->state = PARSER_WAIT_DATA;
            }
        }
        return 0;

    /* ============ 读取 Data（length 字节）============ */
    case PARSER_WAIT_DATA:
        p->frame.data[p->data_cnt++] = ch;
        p->crc = CRC32_Update(p->crc, ch);
        if (p->data_cnt >= p->frame.length) {
            p->state = PARSER_WAIT_CRC;
            p->field_cnt = 0;
            p->field_tmp = 0;
        }
        return 0;

    /* ============ 读取 CRC32（4 字节，大端序）============ */
    case PARSER_WAIT_CRC:
        p->field_tmp = (p->field_tmp << 8) | ch;
        p->field_cnt++;
        if (p->field_cnt == 4) {
            p->frame.crc32 = p->field_tmp;

            /* 比较 CRC */
            uint32_t calc_crc = CRC32_Final(p->crc);
            if (calc_crc != p->frame.crc32) {
                /* CRC 不匹配 */
                p->state = PARSER_WAIT_MAGIC1;
                return -1;
            }

            p->field_cnt = 0;
            p->field_tmp = 0;
            p->state = PARSER_WAIT_TAIL;
        }
        return 0;

    /* ============ 读取 Tail（1 字节）============ */
    case PARSER_WAIT_TAIL:
        if (ch == OTA_TAIL) {
            /* 一帧完整！ */
            p->state = PARSER_WAIT_MAGIC1;
            return 1;
        } else {
            /* 帧尾不对 */
            p->state = PARSER_WAIT_MAGIC1;
            return -2;
        }

    default:
        p->state = PARSER_WAIT_MAGIC1;
        return 0;
    }
}