#ifndef __OTA_RECEIVER_H__
#define __OTA_RECEIVER_H__

#include <stdint.h>
#include "ota_protocol.h"

/* ============ OTA 业务事件枚举（UI 层订阅用）============ */
typedef enum {
    OTA_EVENT_PREPARE_START,     /* 准备开始，param1=total_size */
    OTA_EVENT_PREPARE_DONE,      /* 准备完成，param1=total_size */
    OTA_EVENT_RECEIVE_PROGRESS,  /* 接收进度，param1=received_bytes, param2=total_size */
    OTA_EVENT_VERIFY_START,      /* 校验开始 */
    OTA_EVENT_VERIFY_PASS,       /* 校验通过，param1=crc32 */
    OTA_EVENT_VERIFY_FAIL,       /* 校验失败，param1=calc_crc, param2=expected_crc */
    OTA_EVENT_ERROR_SIZE,        /* 总大小超限，param1=reported_size */
    OTA_EVENT_ERROR_OFFSET,      /* 数据帧偏移越界 */
    OTA_EVENT_REBOOT,            /* 即将软复位 */
} OtaEvent_t;

/* ============ 事件回调函数类型 ============ */
typedef void (*OtaEventCallback_t)(OtaEvent_t event, uint32_t param1, uint32_t param2);

/* ============ 初始化接收器 ============ */
void OtaReceiver_Init(void);

/* ============ 处理解析好的一帧 ============ */
void OtaReceiver_HandleFrame(OtaFrame_t *frame);

/* ============ 注册事件回调（由 UI 层调用）============ */
void OtaReceiver_SetEventCallback(OtaEventCallback_t cb);

#endif /* __OTA_RECEIVER_H__ */