#ifndef __BSP_FLASH_H__
#define __BSP_FLASH_H__

#include <stdint.h>
#include "ota_config.h"

/* 错误码 */
#define BSP_FLASH_OK            0
#define BSP_FLASH_ERR_PARAM     1   /* 参数错误（如非对齐、越界） */
#define BSP_FLASH_ERR_ERASE     2   /* 擦除失败 */
#define BSP_FLASH_ERR_WRITE     3   /* 写入失败 */

/**
 * @brief 擦除内部 App 区
 * @param start_addr 起始地址（必须页对齐）
 * @param size 要擦除的字节数
 * @return BSP_FLASH_OK 成功，其他为错误码
 */
uint8_t BSP_Flash_EraseApp(uint32_t start_addr, uint32_t size);

/**
 * @brief 向内部 Flash 写入数据（需先擦除）
 * @param addr 目标地址（必须在 App 区，且 2 字节对齐）
 * @param data 数据指针
 * @param len 数据长度（会自动处理奇数长度补 0xFF）
 * @return BSP_FLASH_OK 成功，其他为错误码
 */
uint8_t BSP_Flash_WriteData(uint32_t addr, const uint8_t *data, uint32_t len);

#endif /* __BSP_FLASH_H__ */