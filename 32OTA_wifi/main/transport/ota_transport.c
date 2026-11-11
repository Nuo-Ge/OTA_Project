#include "ota_transport.h"
#include "../ota_config.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/semphr.h"

static const char *TAG = "OTA_TRANSPORT";
static SemaphoreHandle_t g_mutex = NULL;

static void mutex_lock(void)
{
    if (g_mutex) {
        xSemaphoreTake(g_mutex, pdMS_TO_TICKS(OTA_MUTEX_TIMEOUT_MS));
    }
}

static void mutex_unlock(void)
{
    if (g_mutex) {
        xSemaphoreGive(g_mutex);
    }
}

void ota_transport_init(void)
{
    g_mutex = xSemaphoreCreateMutex();
    if (!g_mutex) {
        ESP_LOGE(TAG, "互斥锁创建失败");
    }

    gpio_set_direction(OTA_TX_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(OTA_TX_PIN, 1);
    ota_transport_delay(OTA_UART_STABLE_MS);

    const uart_config_t cfg = {
        .baud_rate  = OTA_BAUD_RATE,
        .data_bits  = UART_DATA_8_BITS,
        .parity     = UART_PARITY_DISABLE,
        .stop_bits  = UART_STOP_BITS_1,
        .flow_ctrl  = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    ESP_ERROR_CHECK(uart_driver_install(OTA_UART_NUM, OTA_UART_RX_BUF, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(OTA_UART_NUM, &cfg));
    ESP_ERROR_CHECK(uart_set_pin(OTA_UART_NUM, OTA_TX_PIN, OTA_RX_PIN,
                                  UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_LOGI(TAG, "就绪  TX=GPIO%d  RX=GPIO%d  Baud=%d",
             OTA_TX_PIN, OTA_RX_PIN, OTA_BAUD_RATE);
}

void ota_transport_send(const uint8_t *data, size_t len)
{
    mutex_lock();
    uart_write_bytes(OTA_UART_NUM, data, len);
    mutex_unlock();
}

bool ota_transport_recv_byte(uint32_t timeout_ms, uint8_t *out)
{
    mutex_lock();
    int n = uart_read_bytes(OTA_UART_NUM, out, 1, pdMS_TO_TICKS(timeout_ms));
    mutex_unlock();
    return (n > 0);
}

void ota_transport_flush_rx(void)
{
    mutex_lock();
    uart_flush_input(OTA_UART_NUM);
    mutex_unlock();
}

void ota_transport_wait_tx_done(uint32_t timeout_ms)
{
    mutex_lock();
    uart_wait_tx_done(OTA_UART_NUM, pdMS_TO_TICKS(timeout_ms));
    mutex_unlock();
}

void ota_transport_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}