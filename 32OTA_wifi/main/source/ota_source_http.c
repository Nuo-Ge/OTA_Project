#include "ota_source_http.h"
#include "../ota_config.h"
#include <string.h>
#include <time.h>
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include "esp_crt_bundle.h"

static const char *TAG = "HTTP_OTA";

static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
        break;
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "TCP 已连接，开始 TLS 握手");
        break;
    case HTTP_EVENT_ON_FINISH:
        ESP_LOGI(TAG, "数据接收完成");
        break;
    case HTTP_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "连接已关闭");
        break;
    case HTTP_EVENT_REDIRECT:
        ESP_LOGW(TAG, "跟随重定向");
        break;
    default:
        break;
    }
    return ESP_OK;
}

uint8_t *ota_http_download_firmware(const char *url, uint32_t *out_size)
{
    if (!url || !out_size) {
        ESP_LOGE(TAG, "参数无效");
        return NULL;
    }
    *out_size = 0;

    ESP_LOGI(TAG, "开始下载固件: %s", url);

    /* 证书校验依赖系统时间，最多等待 10s */
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    while (timeinfo.tm_year < (2020 - 1900) && ++retry < 10) {
        vTaskDelay(pdMS_TO_TICKS(1000));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (timeinfo.tm_year < (2020 - 1900)) {
        ESP_LOGW(TAG, "时间未同步，HTTPS 证书校验可能失败");
    }

    esp_http_client_config_t config = {
        .url = url,
        .timeout_ms = 15000,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .event_handler = http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        return NULL;
    }

    esp_http_client_set_header(client, "User-Agent",
                               "ESP32-S3-OTA/1.0");

    ESP_LOGI(TAG, "可用内部 RAM: %d, 可用 PSRAM: %d",
             (int)heap_caps_get_free_size(MALLOC_CAP_INTERNAL),
             (int)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    esp_err_t err = esp_http_client_open(client, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 连接失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return NULL;
    }

    int content_length = esp_http_client_fetch_headers(client);
    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP 状态码: %d", status_code);

    if (status_code != 200) {
        ESP_LOGE(TAG, "服务器返回错误状态码: %d", status_code);
        esp_http_client_cleanup(client);
        return NULL;
    }

    if (content_length <= 0 || content_length > (int)OTA_MAX_FIRMWARE) {
        ESP_LOGE(TAG, "固件长度非法: %d（上限 %lu）",
                 content_length, (unsigned long)OTA_MAX_FIRMWARE);
        esp_http_client_cleanup(client);
        return NULL;
    }
    ESP_LOGI(TAG, "固件大小: %d 字节", content_length);

    uint8_t *buffer = (uint8_t *)heap_caps_malloc(content_length, MALLOC_CAP_SPIRAM);
    if (!buffer) {
        ESP_LOGE(TAG, "PSRAM 不足，申请 %d 字节失败", content_length);
        esp_http_client_cleanup(client);
        return NULL;
    }

    int total_read = 0;
    int last_pct = -1;
    while (total_read < content_length) {
        int read_len = esp_http_client_read(client,
                                            (char *)(buffer + total_read),
                                            content_length - total_read);
        if (read_len <= 0) {
            ESP_LOGE(TAG, "网络读取中断（已读 %d/%d）", total_read, content_length);
            heap_caps_free(buffer);
            esp_http_client_cleanup(client);
            return NULL;
        }
        total_read += read_len;

        int pct = total_read * 100 / content_length;
        if (pct / 10 != last_pct / 10) {
            last_pct = pct;
            ESP_LOGI(TAG, "下载进度: %d%%", pct);
        }
    }

    esp_http_client_close(client);
    esp_http_client_cleanup(client);

    *out_size = (uint32_t)content_length;
    ESP_LOGI(TAG, "固件下载完成: %d 字节", content_length);
    return buffer;
}

void ota_http_free_firmware(uint8_t *buffer)
{
    if (buffer) heap_caps_free(buffer);
}