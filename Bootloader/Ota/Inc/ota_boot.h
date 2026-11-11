#ifndef __OTA_BOOT_H__
#define __OTA_BOOT_H__

#include <stdint.h>

/* 搬运状态 */
typedef enum {
    OTA_BOOT_IDLE = 0,       /* 无升级任务 */
    OTA_BOOT_ERASING,        /* 正在擦除 App 区 */
    OTA_BOOT_COPYING,        /* 正在搬运 */
    OTA_BOOT_VERIFYING,      /* 正在校验 */
    OTA_BOOT_SUCCESS,        /* 搬运成功 */
    OTA_BOOT_ERR_ERASE,      /* 擦除失败 */
    OTA_BOOT_ERR_WRITE,      /* 写入失败 */
    OTA_BOOT_ERR_CRC         /* 校验失败 */
} OtaBootState_t;

/**
 * @brief 检查并执行 OTA 搬运
 * @return OtaBootState_t 当前搬运结果
 */
OtaBootState_t OtaBoot_CheckAndApply(void);

#endif /* __OTA_BOOT_H__ */