#include "ota_source.h"
#include "../ota_config.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "OTA_SRC";

#ifdef OTA_HAS_FLASH_FIRMWARE

extern const uint8_t _binary_App_bin_start[];
extern const uint8_t _binary_App_bin_end[];

static uint32_t flash_get_size(const void *ctx)
{
    (void)ctx;
    return (uint32_t)(_binary_App_bin_end - _binary_App_bin_start);
}

static int flash_read(const void *ctx, uint32_t offset, uint8_t *buf, uint32_t len)
{
    (void)ctx;
    uint32_t total = flash_get_size(NULL);
    if (offset >= total) return 0;
    uint32_t remain = total - offset;
    uint32_t actual = (len < remain) ? len : remain;
    memcpy(buf, _binary_App_bin_start + offset, actual);
    return (int)actual;
}

const OtaSource_t g_ota_source_flash = {
    .get_size = flash_get_size,
    .read     = flash_read,
    .ctx      = NULL,
};

#else

static uint32_t flash_get_size_stub(const void *ctx)
{
    (void)ctx;
    return 0;
}

static int flash_read_stub(const void *ctx, uint32_t offset, uint8_t *buf, uint32_t len)
{
    (void)ctx; (void)offset; (void)buf; (void)len;
    ESP_LOGW(TAG, "Flash 源未启用，请配置 CMakeLists.txt 或使用 HTTP 下载");
    return 0;
}

const OtaSource_t g_ota_source_flash = {
    .get_size = flash_get_size_stub,
    .read     = flash_read_stub,
    .ctx      = NULL,
};

#endif /* OTA_HAS_FLASH_FIRMWARE */