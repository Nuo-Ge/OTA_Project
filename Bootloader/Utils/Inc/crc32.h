#ifndef __CRC32_H__
#define __CRC32_H__

#include <stdint.h>

/* 增量 CRC32 API（适合 OTA 流式校验） */
uint32_t CRC32_Start(void);
uint32_t CRC32_Update(uint32_t crc, uint8_t byte);
uint32_t CRC32_UpdateBlock(uint32_t crc, const uint8_t *data, uint32_t len);
uint32_t CRC32_Final(uint32_t crc);

/* 一次性完整计算（兼容旧接口） */
uint32_t CRC32_Calculate(const uint8_t *data, uint32_t len);

#endif /* __CRC32_H__ */