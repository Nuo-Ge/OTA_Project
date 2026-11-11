#ifndef __BOOT_CONFIG_H__
#define __BOOT_CONFIG_H__

#include "ota_config.h"

/* ================== Bootloader 特有配置 ================== */
#define BOOTLOADER_SIZE         (24 * 1024)     /* Bootloader 占用 24KB */
#define FLASH_END_ADDR          0x08010000      /* 64KB Flash 结束 */

#endif /* __BOOT_CONFIG_H__ */