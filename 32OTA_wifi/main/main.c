#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include "transport/ota_transport.h"
#include "task/ota_task.h"
#include "source/ota_source_http.h"
#include "wifi/wifi_manager.h"
#include "cloud_mqtt.h"

static const char *TAG = "APP_MAIN";

/* ==================== OTA 进度回调 ==================== */
static void on_ota_progress(uint8_t percent, ota_state_t state, const char *msg)
{
    switch (state) {
    case OTA_STATE_HANDSHAKE:
        ESP_LOGI(TAG, "[回调] 握手: %s", msg ? msg : "");
        break;
    case OTA_STATE_TRANSFER:
        if (msg) ESP_LOGI(TAG, "[回调] %s", msg);
        else     ESP_LOGI(TAG, "[回调] 进度: %d%%", percent);
        break;
    case OTA_STATE_FINISH:
        ESP_LOGI(TAG, "[回调] 结束: %s", msg ? msg : "");
        break;
    case OTA_STATE_SUCCESS:
        ESP_LOGI(TAG, "[回调] 成功: %s", msg ? msg : "");
        break;
    case OTA_STATE_ERROR:
        ESP_LOGE(TAG, "[回调] 错误: %s", msg ? msg : "");
        break;
    default:
        break;
    }

    static int last_pct = -1;
    if (state == OTA_STATE_TRANSFER && (percent / 10 != last_pct / 10 || percent == 100)) {
        last_pct = percent;
        char status_str[32];
        snprintf(status_str, sizeof(status_str), "flashing_%d%%", (int)percent);
        cloud_mqtt_report_status(status_str);
    }
}

/* ==================== 入口 ==================== */
void app_main(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    /* 尽早启动重新配网按键检测，保证即使卡在配网阶段也能长按 BOOT 键重置 */
    wifi_reconfig_button_start();

    ota_transport_init();

    if (!wifi_init_sta()) {
        ESP_LOGE(TAG, "Wi-Fi 连接失败");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    ESP_LOGI(TAG, "正在同步网络时间...");
    esp_sntp_setoperatingmode(ESP_SNTP_OPMODE_POLL);
    esp_sntp_setservername(0, "pool.ntp.org");
    esp_sntp_init();

    time_t now = 0;
    struct tm timeinfo = { 0 };
    int ntp_retry = 0;
    while (timeinfo.tm_year < (2024 - 1900) && ++ntp_retry < 20) {
        vTaskDelay(pdMS_TO_TICKS(500));
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if (timeinfo.tm_year < (2024 - 1900)) {
        ESP_LOGE(TAG, "NTP 同步失败，重启...");
        esp_restart();
    }
    ESP_LOGI(TAG, "时间已同步");

    if (!cloud_mqtt_init()) {
        ESP_LOGE(TAG, "MQTT 初始化失败，请通过 menuconfig 配置云端参数后重新烧录");
        while (1) { vTaskDelay(pdMS_TO_TICKS(1000)); }
    }

    char ota_url[256] = { 0 };
    ESP_LOGI(TAG, "系统准备完毕，等待云端下发升级指令...");

    while (1) {
        if (cloud_mqtt_wait_for_ota_url(ota_url, sizeof(ota_url))) {
            ESP_LOGI(TAG, "收到升级指令！目标 URL: %s", ota_url);

            cloud_mqtt_report_status("downloading");

            uint32_t fw_size = 0;
            uint8_t *fw_buffer = ota_http_download_firmware(ota_url, &fw_size);

            if (fw_buffer != NULL) {
                ESP_LOGI(TAG, "固件下载成功，开始串口传输...");
                cloud_mqtt_report_status("flashing");

                ota_task_start(fw_buffer, fw_size, on_ota_progress);
                ota_http_free_firmware(fw_buffer);

                cloud_mqtt_report_status("success");
                ESP_LOGI(TAG, "OTA 流程全部完成！");
            } else {
                ESP_LOGE(TAG, "固件下载失败！");
                cloud_mqtt_report_status("failed");
            }

            memset(ota_url, 0, sizeof(ota_url));
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}