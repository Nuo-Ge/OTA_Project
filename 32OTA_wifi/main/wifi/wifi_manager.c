#include "wifi_manager.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "network_provisioning/manager.h"
#include "network_provisioning/scheme_ble.h"

static const char *TAG = "WIFI";

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT    BIT0
#define WIFI_DISCONNECTED_BIT BIT1

/* 配网期间抑制断线自动重连，避免 STA 停留在 connecting 状态导致 set_config 失败 */
static volatile bool s_provisioning = false;

/* ==================== 重新配网按键 ==================== */
#define RECONFIG_BUTTON_PIN   GPIO_NUM_0
#define RECONFIG_HOLD_MS      3000

/* BLE 配网广播设备名（service_name，最长 MAX_BLE_DEVNAME_LEN=29 字节） */
#define PROV_DEVICE_NAME      "ESP32_OTA_PROV"

/* Wi-Fi 连接/重连超时：连续重连 30 秒仍连不上才自动回退到 BLE 配网 */
#define WIFI_CONNECT_TIMEOUT_MS   30000

/* ==================== Wi-Fi 事件回调 ==================== */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "STA 已启动");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        if (!s_provisioning) {
            ESP_LOGW(TAG, "连接断开，重试中...");
            esp_wifi_connect();
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "获取 IP 地址: " IPSTR, IP2STR(&event->ip_info.ip));
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

/* ==================== 配网事件回调 ==================== */
static void prov_event_handler(void *user_data, network_prov_cb_event_t event, void *event_data)
{
    switch (event) {
    case NETWORK_PROV_START:
        ESP_LOGI(TAG, "配网服务已启动，等待手机连接...");
        break;
    case NETWORK_PROV_WIFI_CRED_RECV:
        ESP_LOGI(TAG, "收到 Wi-Fi 凭据，正在连接...");
        break;
    case NETWORK_PROV_WIFI_CRED_SUCCESS:
        ESP_LOGI(TAG, "配网成功，Wi-Fi 已连接");
        break;
    case NETWORK_PROV_WIFI_CRED_FAIL:
        ESP_LOGW(TAG, "配网失败（凭据错误或连接失败）");
        break;
    case NETWORK_PROV_END:
        ESP_LOGI(TAG, "配网服务已停止");
        break;
    default:
        break;
    }
}

/* ==================== 重新配网按键任务 ==================== */
static void reconfig_button_task(void *arg)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << RECONFIG_BUTTON_PIN),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    uint32_t pressed_ms = 0;
    while (1) {
        if (gpio_get_level(RECONFIG_BUTTON_PIN) == 0) {
            pressed_ms += 100;
            if (pressed_ms >= RECONFIG_HOLD_MS) {
                ESP_LOGW(TAG, "检测到 BOOT 键长按，擦除配网凭据并重启...");
                nvs_flash_erase();
                esp_restart();
            }
        } else {
            pressed_ms = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void wifi_reconfig_button_start(void)
{
    xTaskCreate(reconfig_button_task, "reconfig_btn", 2048, NULL, 5, NULL);
}

/* ==================== BLE 配网（安全回退） ==================== */
/**
 * @brief 进入 BLE 配网，并在需要时先把 STA 从 connecting 状态拉出来
 *
 * manager 在 start_provisioning 内部会先 esp_wifi_set_config() 清空 RAM 凭据，
 * 若此时 STA 仍处于 connecting 状态，该调用会返回 ESP_ERR_WIFI_STATE 导致启动失败。
 * 因此：先抑制断线自动重连（s_provisioning），再主动断开并等待断开事件落地。
 *
 * @param need_disconnect 连接超时回退场景为 true；首次无凭据场景 STA 本就空闲，传 false
 */
static void start_ble_provisioning(bool need_disconnect)
{
    s_provisioning = true;

    if (need_disconnect) {
        xEventGroupClearBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
        esp_wifi_disconnect();
        xEventGroupWaitBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT,
                            pdFALSE, pdTRUE, pdMS_TO_TICKS(3000));
    }

    ESP_ERROR_CHECK(network_prov_mgr_start_provisioning(
        NETWORK_PROV_SECURITY_0, NULL, PROV_DEVICE_NAME, NULL));
    network_prov_mgr_wait();
    network_prov_mgr_stop_provisioning();

    /* 配网结束，恢复断线自动重连 */
    s_provisioning = false;
}

/* ==================== 主流程 ==================== */
bool wifi_init_sta(void)
{
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    /* 必须在配网前启动 Wi-Fi，配网期间 manager 才能调 esp_wifi_connect() 验证凭据 */
    ESP_ERROR_CHECK(esp_wifi_start());

    /* ==================== BLE 配网管理器 ==================== */
    network_prov_mgr_config_t config = {
        .scheme = network_prov_scheme_ble,
        /* 关键：配网完成后释放全部 BT/Bluedroid 资源与 LwIP 钩子，
           否则后续 HTTPS/TLS 握手会死锁 */
        .scheme_event_handler = NETWORK_PROV_SCHEME_BLE_EVENT_HANDLER_FREE_BTDM,
        .app_event_handler = {
            .event_cb = prov_event_handler,
            .user_data = NULL,
        },
    };
    ESP_ERROR_CHECK(network_prov_mgr_init(config));

    /* ==================== 连接 + 失败自动回退 BLE 配网 ==================== */
    bool connected = false;
    while (!connected) {
        bool provisioned = false;
        ESP_ERROR_CHECK(network_prov_mgr_is_wifi_provisioned(&provisioned));

        if (provisioned) {
            ESP_LOGI(TAG, "检测到 Wi-Fi 凭据，尝试连接...");
            if (esp_wifi_connect() != ESP_OK) {
                ESP_LOGW(TAG, "esp_wifi_connect 返回非 OK（可能已连接）");
            }

            EventBits_t bits = xEventGroupWaitBits(
                s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                pdMS_TO_TICKS(WIFI_CONNECT_TIMEOUT_MS));
            if (bits & WIFI_CONNECTED_BIT) {
                connected = true;
                continue;
            }
            ESP_LOGW(TAG, "Wi-Fi 连接超时，自动进入 BLE 配网模式");
            start_ble_provisioning(true);
        } else {
            ESP_LOGI(TAG, "未检测到 Wi-Fi 凭据，进入 BLE 配网（使用 ESP SoftAP Prov APP 连接）...");
            start_ble_provisioning(false);
        }
    }

    /* 配网结束，100% 释放 BLE/BT 栈，之后才能安全地做 HTTPS/TLS */
    ESP_ERROR_CHECK(network_prov_mgr_deinit());
    ESP_LOGI(TAG, "BLE 栈已完全释放");

    ESP_LOGI(TAG, "Wi-Fi 已连接");
    return true;
}