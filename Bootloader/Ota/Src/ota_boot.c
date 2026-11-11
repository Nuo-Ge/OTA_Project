#include "ota_boot.h"
#include "ota_config.h"
#include "bsp_flash.h"
#include "bsp_w25qxx.h"
#include "bsp_at24cxx.h"
#include "crc32.h"

OtaBootState_t OtaBoot_CheckAndApply(void)
{
    OtaInfo_t info;

    /* 1. 读取控制信息 */
    BSP_AT24CXX_Read(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    /* 2. 防御性校验：检查 Magic 和 Flag */
    if (info.magic != OTA_MAGIC || info.flag != OTA_FLAG_READY) {
        return OTA_BOOT_IDLE; /* 不需要升级 */
    }

    /* 3. 防御性校验：检查固件大小是否合法 */
    if (info.total_size == 0 || info.total_size > APP_MAX_SIZE) {
        return OTA_BOOT_ERR_CRC; /* 大小异常，视为非法数据 */
    }

    /* 4. 擦除内部 App 区 */
    if (BSP_Flash_EraseApp(APP_ADDRESS, info.total_size) != BSP_FLASH_OK) {
        return OTA_BOOT_ERR_ERASE;
    }

    /* 5. 分块搬运并计算 CRC */
    uint8_t chunk[256];
    uint32_t offset = 0;
    uint32_t crc = CRC32_Start();

    while (offset < info.total_size) {
        uint32_t remain = info.total_size - offset;
        uint16_t len = (remain > 256) ? 256 : (uint16_t)remain;

        /* 从外部 W25QXX 读 */
        BSP_W25QXX_Read(W25QXX_FIRMWARE_ADDR + offset, chunk, len);

        /* 写入内部 Flash */
        if (BSP_Flash_WriteData(APP_ADDRESS + offset, chunk, len) != BSP_FLASH_OK) {
            return OTA_BOOT_ERR_WRITE;
        }

        /* 流水线更新 CRC */
        crc = CRC32_UpdateBlock(crc, chunk, len);

        offset += len;
    }
    crc = CRC32_Final(crc);

    /* 6. 校验完整性 */
    if (crc != info.total_crc32) {
        return OTA_BOOT_ERR_CRC;
    }

    /* 7. 校验通过，清除升级标志 (必须先校验，后清标志) */
    info.flag = OTA_FLAG_NONE;
    BSP_AT24CXX_Write(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    return OTA_BOOT_SUCCESS;
}