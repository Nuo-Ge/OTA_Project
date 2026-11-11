#include "cloud_mqtt.h"
#include "sdkconfig.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "esp_log.h"
#include "esp_crt_bundle.h"
#include "mqtt_client.h"
#include "psa/crypto.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

static const char *TAG = "CLOUD_MQTT";

/* 云端参数统一由 menuconfig(Kconfig.projbuild) 注入，严禁在源码硬编码密钥 */
#define DEVICE_ID     CONFIG_OTA_DEVICE_ID
#define DEVICE_SECRET CONFIG_OTA_DEVICE_SECRET
#define IOTDA_HOST    CONFIG_OTA_IOTDA_HOST
#define IOTDA_PORT    CONFIG_OTA_IOTDA_PORT

#define URL_MAX_LEN        256
#define REQUEST_ID_MAX_LEN 64
#define TOPIC_MAX_LEN      256

static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static QueueHandle_t s_url_queue = NULL;

/* ==================== 配置校验 ==================== */
static bool config_is_valid(void)
{
    if (DEVICE_ID[0] == '\0') {
        ESP_LOGE(TAG, "未配置设备 ID，请在 menuconfig 中设置 OTA_DEVICE_ID");
        return false;
    }
    if (DEVICE_SECRET[0] == '\0') {
        ESP_LOGE(TAG, "未配置设备密钥，请在 menuconfig 中设置 OTA_DEVICE_SECRET");
        return false;
    }
    if (IOTDA_HOST[0] == '\0') {
        ESP_LOGE(TAG, "未配置接入地址，请在 menuconfig 中设置 OTA_IOTDA_HOST");
        return false;
    }
    return true;
}

/* ==================== 华为云 IoTDA 鉴权 ==================== */
static void get_utc_timestamp(char *buf, size_t len)
{
    time_t now;
    time(&now);
    struct tm timeinfo;
    gmtime_r(&now, &timeinfo);
    strftime(buf, len, "%Y%m%d%H", &timeinfo);
}

static bool generate_auth_params(char *client_id, size_t client_id_len,
                                 char *username, size_t username_len,
                                 char *password, size_t password_len)
{
    char timestamp[16] = {0};
    get_utc_timestamp(timestamp, sizeof(timestamp));

    int n = snprintf(client_id, client_id_len, "%s_0_0_%s", DEVICE_ID, timestamp);
    if (n < 0 || (size_t)n >= client_id_len) {
        ESP_LOGE(TAG, "ClientID 缓冲区不足");
        return false;
    }
    snprintf(username, username_len, "%s", DEVICE_ID);

    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "PSA Crypto 初始化失败: %ld", (long)status);
        return false;
    }

    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE);
    psa_set_key_algorithm(&attributes, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);

    psa_key_id_t key_id = 0;
    status = psa_import_key(&attributes, (const uint8_t *)timestamp,
                            strlen(timestamp), &key_id);
    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "PSA 导入密钥失败: %ld", (long)status);
        return false;
    }

    uint8_t hmac_result[PSA_MAC_MAX_SIZE] = {0};
    size_t hmac_len = 0;
    status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             (const uint8_t *)DEVICE_SECRET, strlen(DEVICE_SECRET),
                             hmac_result, sizeof(hmac_result), &hmac_len);
    psa_destroy_key(key_id);

    if (status != PSA_SUCCESS) {
        ESP_LOGE(TAG, "HMAC 计算失败: %ld", (long)status);
        return false;
    }
    if (hmac_len * 2 + 1 > password_len) {
        ESP_LOGE(TAG, "密码缓冲区不足");
        return false;
    }

    for (size_t i = 0; i < hmac_len; i++) {
        snprintf(password + i * 2, password_len - i * 2, "%02x", hmac_result[i]);
    }
    password[hmac_len * 2] = '\0';

    ESP_LOGI(TAG, "时间戳: %s", timestamp);
    ESP_LOGI(TAG, "ClientID: %s", client_id);
    /* 敏感信息（密码/密钥）不打印到日志 */
    return true;
}

/* ==================== 轻量 JSON 字段提取 ==================== */
static bool json_extract_string(const char *json, const char *key,
                                char *out, size_t out_len)
{
    if (!json || !key || !out || out_len == 0) return false;

    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);

    const char *key_pos = strstr(json, pattern);
    if (!key_pos) return false;

    const char *colon = strchr(key_pos, ':');
    if (!colon) return false;

    const char *start = strchr(colon, '"');
    if (!start) return false;
    start++;

    const char *end = strchr(start, '"');
    if (!end) return false;

    size_t len = (size_t)(end - start);
    if (len >= out_len) len = out_len - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return len > 0;
}

static void extract_request_id(const char *topic, int topic_len,
                               char *out, size_t out_len)
{
    if (!topic || !out || out_len == 0) return;

    const char *key = "request_id=";
    const char *p = strstr(topic, key);
    if (!p) return;
    p += strlen(key);

    int remain = topic_len - (int)(p - topic);
    if (remain <= 0) return;
    if ((size_t)remain >= out_len) remain = (int)out_len - 1;

    memcpy(out, p, remain);
    out[remain] = '\0';
}

/* ==================== MQTT 事件处理 ==================== */
static void mqtt_event_handler(void *handler_args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    (void)handler_args;
    (void)base;
    (void)event_id;

    esp_mqtt_event_handle_t event = (esp_mqtt_event_handle_t)event_data;

    if (event->event_id == MQTT_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "已连接华为云 IoTDA MQTT");

        char sub_topic[TOPIC_MAX_LEN];
        snprintf(sub_topic, sizeof(sub_topic),
                 "$oc/devices/%s/sys/commands/#", DEVICE_ID);
        esp_mqtt_client_subscribe(s_mqtt_client, sub_topic, 0);
        cloud_mqtt_report_status("idle");

    } else if (event->event_id == MQTT_EVENT_DATA) {
        ESP_LOGI(TAG, "收到云端命令下发");

        char safe_url[URL_MAX_LEN] = {0};
        char request_id[REQUEST_ID_MAX_LEN] = {0};

        extract_request_id(event->topic, event->topic_len,
                           request_id, sizeof(request_id));

        if (event->data && event->data_len > 0) {
            char *payload = malloc(event->data_len + 1);
            if (payload) {
                memcpy(payload, event->data, event->data_len);
                payload[event->data_len] = '\0';

                if (json_extract_string(payload, "url", safe_url, sizeof(safe_url))) {
                    ESP_LOGI(TAG, "解析到 URL: %s", safe_url);
                    if (xQueueSend(s_url_queue, safe_url, pdMS_TO_TICKS(100)) != pdTRUE) {
                        ESP_LOGW(TAG, "URL 队列已满，丢弃本次指令");
                    }
                } else {
                    ESP_LOGW(TAG, "未在命令中解析到 url 字段");
                }
                free(payload);
            } else {
                ESP_LOGE(TAG, "分配解析缓冲区失败");
            }
        }

        if (request_id[0] != '\0') {
            char resp_topic[TOPIC_MAX_LEN];
            snprintf(resp_topic, sizeof(resp_topic),
                     "$oc/devices/%s/sys/commands/response/request_id=%s",
                     DEVICE_ID, request_id);
            const char *resp_payload =
                "{\"result_code\": 0, \"response_name\": \"COMMAND_RESPONSE\"}";
            esp_mqtt_client_publish(s_mqtt_client, resp_topic, resp_payload, 0, 1, 0);
            ESP_LOGI(TAG, "已回复 Command Response");
        }
    }
}

/* ==================== 公共接口 ==================== */
bool cloud_mqtt_init(void)
{
    if (!config_is_valid()) {
        ESP_LOGE(TAG, "云端配置无效，请通过 idf.py menuconfig 配置 OTA 云端参数");
        return false;
    }

    s_url_queue = xQueueCreate(3, URL_MAX_LEN);
    if (!s_url_queue) {
        ESP_LOGE(TAG, "创建 URL 队列失败");
        return false;
    }

    char client_id[128] = {0};
    char username[128] = {0};
    char password[65] = {0};
    if (!generate_auth_params(client_id, sizeof(client_id),
                              username, sizeof(username),
                              password, sizeof(password))) {
        ESP_LOGE(TAG, "生成鉴权参数失败");
        return false;
    }

    char broker_uri[TOPIC_MAX_LEN];
    snprintf(broker_uri, sizeof(broker_uri), "mqtts://%s:%d", IOTDA_HOST, IOTDA_PORT);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = broker_uri,
        .broker.verification.crt_bundle_attach = esp_crt_bundle_attach,
        .credentials.client_id = client_id,
        .credentials.username = username,
        .credentials.authentication.password = password,
        .session.keepalive = 120,
        .session.disable_clean_session = false,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "MQTT 客户端初始化失败");
        return false;
    }

    esp_err_t err = esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                                   mqtt_event_handler, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "注册 MQTT 事件回调失败");
        return false;
    }

    err = esp_mqtt_client_start(s_mqtt_client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 MQTT 客户端失败");
        return false;
    }
    return true;
}

void cloud_mqtt_report_status(const char *status)
{
    if (s_mqtt_client == NULL || status == NULL) return;

    char topic[TOPIC_MAX_LEN];
    snprintf(topic, sizeof(topic),
             "$oc/devices/%s/sys/properties/report", DEVICE_ID);

    char payload[TOPIC_MAX_LEN];
    snprintf(payload, sizeof(payload),
             "{\"services\":[{\"service_id\":\"OTA\",\"properties\":{\"status\":\"%s\"}}]}",
             status);

    esp_mqtt_client_publish(s_mqtt_client, topic, payload, 0, 1, 0);
    ESP_LOGI(TAG, "上报状态: %s", status);
}

bool cloud_mqtt_wait_for_ota_url(char *url_buffer, size_t max_len)
{
    if (!url_buffer || max_len == 0) return false;

    char temp_buf[URL_MAX_LEN] = {0};
    if (xQueueReceive(s_url_queue, temp_buf, portMAX_DELAY) == pdTRUE) {
        strncpy(url_buffer, temp_buf, max_len - 1);
        url_buffer[max_len - 1] = '\0';
        return true;
    }
    return false;
}