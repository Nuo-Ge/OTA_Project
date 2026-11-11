#include "crc32.h"

/*
 * CRC32 标准实现（与 zlib / 以太网 / PNG / ZIP 一致）
 *   多项式  : 0xEDB88320
 *   初始值  : 0xFFFFFFFF
 *   输出异或: 0xFFFFFFFF
 *
 * 标准测试向量：
 *   CRC32("123456789") = 0xCBF43926
 */

/* 开始 CRC32 计算：返回初始值 */
uint32_t CRC32_Start(void)
{
    return 0xFFFFFFFFU;
}

/* 增量更新：喂一个字节 */
uint32_t CRC32_Update(uint32_t crc, uint8_t byte)
{
    crc ^= (uint32_t)byte;

    for (int j = 0; j < 8; j++) {
        if (crc & 1U) {
            crc = (crc >> 1) ^ 0xEDB88320U;
        } else {
            crc = (crc >> 1);
        }
    }
    return crc;
}

/* 批量增量更新：一次喂一整块数据，减少函数调用开销 */
uint32_t CRC32_UpdateBlock(uint32_t crc, const uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        crc ^= (uint32_t)data[i];
        for (int j = 0; j < 8; j++) {
            crc = (crc & 1U) ? ((crc >> 1) ^ 0xEDB88320U) : (crc >> 1);
        }
    }
    return crc;
}

/* 结束：取反得到最终值 */
uint32_t CRC32_Final(uint32_t crc)
{
    return crc ^ 0xFFFFFFFFU;
}

/* 完整计算（基于增量 API 实现，保持接口兼容） */
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len)
{
    uint32_t crc = CRC32_Start();
    for (uint32_t i = 0; i < len; i++) {
        crc = CRC32_Update(crc, data[i]);
    }
    return CRC32_Final(crc);
}