#include "bsp_flash.h"
#include "boot_config.h"
#include "stm32f1xx_hal.h"

/* ================== 内部 Flash 操作 ================== */

/**
 * @brief 擦除内部 App 区
 * @param start_addr 起始地址（必须页对齐）
 * @param size 要擦除的字节数
 * @return BSP_FLASH_OK 成功，其他为错误码
 */
uint8_t BSP_Flash_EraseApp(uint32_t start_addr, uint32_t size) {
    FLASH_EraseInitTypeDef erase_init;
    uint32_t page_error = 0;
    HAL_StatusTypeDef hal_ret;

    /* ---- 参数防御性检查 ---- */
    if (start_addr < APP_ADDRESS) {
        return BSP_FLASH_ERR_PARAM;  /* 不允许擦除 App 之外的区域 */
    }

    if (start_addr % INTERNAL_FLASH_PAGE_SIZE != 0) {
        return BSP_FLASH_ERR_PARAM;  /* 地址必须页对齐（1KB 边界） */
    }

    if (size == 0) {
        return BSP_FLASH_ERR_PARAM;
    }

    if (start_addr + size > FLASH_END_ADDR) {
        return BSP_FLASH_ERR_PARAM;  /* 超出 Flash 范围 */
    }

    /* ---- 计算页范围 ---- */
    uint32_t nb_pages = (size + INTERNAL_FLASH_PAGE_SIZE - 1) / INTERNAL_FLASH_PAGE_SIZE;

    /* ---- 执行擦除 ---- */
    HAL_FLASH_Unlock();

    erase_init.TypeErase    = FLASH_TYPEERASE_PAGES;
    erase_init.Banks        = FLASH_BANK_1;
    erase_init.PageAddress  = start_addr;
    erase_init.NbPages      = nb_pages;

    hal_ret = HAL_FLASHEx_Erase(&erase_init, &page_error);
    HAL_FLASH_Lock();

    if (hal_ret != HAL_OK) {
        return BSP_FLASH_ERR_ERASE;
    }

    return BSP_FLASH_OK;
}

/**
 * @brief 向内部 Flash 写入数据（需先擦除）
 * @param addr 目标地址（必须在 App 区，且 2 字节对齐）
 * @param data 数据指针
 * @param len 数据长度（会自动处理奇数长度补 0xFF）
 * @return BSP_FLASH_OK 成功，其他为错误码
 */
uint8_t BSP_Flash_WriteData(uint32_t addr, const uint8_t *data, uint32_t len) {
    uint32_t i;
    uint16_t half_word;

    /* ---- 参数防御性检查 ---- */
    if (data == NULL) {
        return BSP_FLASH_ERR_PARAM;
    }

    if (len == 0) {
        return BSP_FLASH_ERR_PARAM;
    }

    if (addr < APP_ADDRESS) {
        return BSP_FLASH_ERR_PARAM;  /* 不允许写 App 之外的区域 */
    }

    if (addr % 2 != 0) {
        return BSP_FLASH_ERR_PARAM;  /* STM32F1 Flash 必须半字（2字节）对齐写入 */
    }

    if (addr + len > FLASH_END_ADDR) {
        return BSP_FLASH_ERR_PARAM;  /* 超出 Flash 范围 */
    }

    /* ---- 半字写入 ---- */
    HAL_FLASH_Unlock();

    for (i = 0; i < len; i += 2) {
        /* 组装半字：高字节在前（小端模式） */
        half_word  = data[i];
        half_word |= (uint16_t)((i + 1 < len) ? data[i + 1] : 0xFF) << 8;

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, half_word) != HAL_OK) {
            HAL_FLASH_Lock();
            return BSP_FLASH_ERR_WRITE;
        }
    }

    HAL_FLASH_Lock();
    return BSP_FLASH_OK;
}