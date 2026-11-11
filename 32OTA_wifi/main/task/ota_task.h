#ifndef OTA_TASK_H
#define OTA_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    OTA_STATE_IDLE      = 0,
    OTA_STATE_HANDSHAKE = 1,
    OTA_STATE_TRANSFER  = 2,
    OTA_STATE_FINISH    = 3,
    OTA_STATE_SUCCESS   = 4,
    OTA_STATE_ERROR     = 5,
} ota_state_t;

typedef void (*ota_callback_t)(uint8_t percent, ota_state_t state, const char *msg);

/**
 * @brief 同步执行一次 OTA 升级（阻塞直到完成）
 *
 * @param firmware_data 固件数据缓冲区（整个流程期间必须保持有效）
 * @param firmware_size 固件字节数
 * @param callback      进度/状态回调，可为 NULL（表示不回调）
 */
void ota_task_start(const uint8_t *firmware_data, uint32_t firmware_size, ota_callback_t callback);

#ifdef __cplusplus
}
#endif

#endif /* OTA_TASK_H */