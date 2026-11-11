#ifndef OTA_SOURCE_H
#define OTA_SOURCE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief OTA 数据源抽象接口（可重入）
 *
 * 通过 ctx 传入实例上下文，使同一套读取逻辑可被多个数据源复用，
 * 避免使用全局/静态可变状态。
 */
typedef struct ota_source {
    uint32_t (*get_size)(const void *ctx);
    int      (*read)(const void *ctx, uint32_t offset, uint8_t *buf, uint32_t len);
    const void *ctx;
} OtaSource_t;

/* Flash 内嵌固件源（编译期通过 target_add_binary_data 嵌入） */
extern const OtaSource_t g_ota_source_flash;

#ifdef __cplusplus
}
#endif

#endif /* OTA_SOURCE_H */