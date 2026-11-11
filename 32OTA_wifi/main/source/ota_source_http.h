#ifndef OTA_SOURCE_HTTP_H
#define OTA_SOURCE_HTTP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint8_t *ota_http_download_firmware(const char *url, uint32_t *out_size);

void ota_http_free_firmware(uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif /* OTA_SOURCE_HTTP_H */