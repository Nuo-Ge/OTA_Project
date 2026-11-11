#ifndef OTA_TRANSPORT_H
#define OTA_TRANSPORT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void ota_transport_init(void);

void ota_transport_send(const uint8_t *data, size_t len);

bool ota_transport_recv_byte(uint32_t timeout_ms, uint8_t *out);

void ota_transport_flush_rx(void);
void ota_transport_wait_tx_done(uint32_t timeout_ms);
void ota_transport_delay(uint32_t ms);

#ifdef __cplusplus
}
#endif

#endif /* OTA_TRANSPORT_H */