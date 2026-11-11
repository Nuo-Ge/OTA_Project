#include "ota_task.h"
#include "../ota_config.h"
#include "../protocol/ota_protocol.h"
#include "../transport/ota_transport.h"
#include "../source/ota_source.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include <string.h>

static const char *TAG = "OTA";

/* ==================== 会话上下文（可重入） ==================== */
typedef struct {
    const uint8_t *data;
    uint32_t       size;
} ota_buffer_t;

typedef struct {
    const OtaSource_t *source;
    ota_buffer_t       buffer;    /* 内存缓冲源数据 */
    ota_callback_t     callback;
} ota_session_t;

/* ==================== 内存缓冲数据源 ==================== */
static uint32_t buffer_get_size(const void *ctx)
{
    const ota_buffer_t *b = (const ota_buffer_t *)ctx;
    return b ? b->size : 0;
}

static int buffer_read(const void *ctx, uint32_t offset, uint8_t *out, uint32_t len)
{
    const ota_buffer_t *b = (const ota_buffer_t *)ctx;
    if (!b || offset >= b->size) return 0;
    uint32_t remain = b->size - offset;
    uint32_t actual = (len < remain) ? len : remain;
    memcpy(out, b->data + offset, actual);
    return (int)actual;
}

static OtaSource_t buffer_source_create(const ota_buffer_t *buffer)
{
    OtaSource_t src = {
        .get_size = buffer_get_size,
        .read     = buffer_read,
        .ctx      = buffer,
    };
    return src;
}

/* ==================== 通知 ==================== */
static void notify(ota_session_t *s, uint8_t pct, ota_state_t state, const char *msg)
{
    if (s && s->callback) s->callback(pct, state, msg);
}

/* ==================== 等待 ACK ==================== */
static bool wait_ack(uint16_t expected_seq, uint32_t timeout_ms)
{
    ota_ack_parser_t parser;
    ota_ack_parser_init(&parser);

    int64_t deadline = esp_timer_get_time() + (int64_t)timeout_ms * 1000;
    while (esp_timer_get_time() < deadline) {
        uint8_t ch;
        if (!ota_transport_recv_byte(10, &ch)) continue;
        if (ota_ack_parser_feed(&parser, ch, expected_seq)) return true;
    }
    return false;
}

static void build_prepare(uint8_t prep[8], uint32_t size, uint32_t crc)
{
    prep[0] = (uint8_t)(size >> 24);
    prep[1] = (uint8_t)(size >> 16);
    prep[2] = (uint8_t)(size >>  8);
    prep[3] = (uint8_t)(size & 0xFF);
    prep[4] = (uint8_t)(crc >> 24);
    prep[5] = (uint8_t)(crc >> 16);
    prep[6] = (uint8_t)(crc >>  8);
    prep[7] = (uint8_t)(crc & 0xFF);
}

static bool do_handshake(ota_session_t *s, const uint8_t *prep)
{
    notify(s, 0, OTA_STATE_HANDSHAKE, "握手开始");

    for (int i = 0; i < OTA_HANDSHAKE_MAX; i++) {
        /* 每次握手重试约 1.3s，必须喂狗，否则 10 次重试期间任务看门狗会触发 */
        esp_task_wdt_reset();

        ota_transport_flush_rx();

        ota_transport_send((const uint8_t *)OTA_LINE_CLEAN, OTA_LINE_CLEAN_LEN);
        ota_transport_wait_tx_done(OTA_TX_DONE_TO);
        ota_transport_delay(50);

        ota_transport_send((const uint8_t *)OTA_WAKEUP_CMD, OTA_WAKEUP_LEN);
        ota_transport_send((const uint8_t *)OTA_WAKEUP_CMD, OTA_WAKEUP_LEN);
        ota_transport_wait_tx_done(OTA_TX_DONE_TO);
        ota_transport_delay(100);

        uint8_t frame[OTA_CHUNK_SIZE + 32];
        size_t n = ota_frame_build(frame, OTA_TYPE_PREPARE, 0, 0, prep, 8);
        ota_transport_send(frame, n);

        if (wait_ack(0, OTA_HANDSHAKE_TO)) {
            notify(s, 0, OTA_STATE_HANDSHAKE, "握手成功");
            return true;
        }
        ESP_LOGW(TAG, "握手重试 %d/%d", i + 1, OTA_HANDSHAKE_MAX);
    }

    notify(s, 0, OTA_STATE_ERROR, "握手失败");
    return false;
}

static bool do_data_transfer(ota_session_t *s, uint32_t total_size)
{
    notify(s, 0, OTA_STATE_TRANSFER, "开始传输");
    uint8_t chunk[OTA_CHUNK_SIZE];
    uint32_t offset = 0;
    uint16_t seq    = 1;

    while (offset < total_size) {
        uint32_t remain = total_size - offset;
        uint32_t len = (remain > OTA_CHUNK_SIZE) ? OTA_CHUNK_SIZE : remain;

        if (s->source->read(s->source->ctx, offset, chunk, len) <= 0) {
            ESP_LOGE(TAG, "读取数据源失败 offset=%lu", (unsigned long)offset);
            notify(s, 0, OTA_STATE_ERROR, "读取数据源失败");
            return false;
        }

        bool ok = false;
        for (int r = 0; r < OTA_DATA_RETRY_MAX && !ok; r++) {
            ota_transport_flush_rx();

            uint8_t frame[OTA_CHUNK_SIZE + 32];
            size_t n = ota_frame_build(frame, OTA_TYPE_DATA, seq, offset, chunk, len);
            ota_transport_send(frame, n);

            ok = wait_ack(seq, OTA_DATA_ACK_TO);
            if (!ok) ESP_LOGW(TAG, "seq=%d 重试 %d/%d", seq, r + 1, OTA_DATA_RETRY_MAX);
        }
        if (!ok) {
            ESP_LOGE(TAG, "seq=%d 传输失败", seq);
            notify(s, 0, OTA_STATE_ERROR, "传输中断");
            return false;
        }

        uint8_t pct = (uint8_t)((offset + len) * 100 / total_size);
        notify(s, pct, OTA_STATE_TRANSFER, NULL);
        ESP_LOGI(TAG, "%d%%  (%lu/%lu)", pct,
                 (unsigned long)(offset + len), (unsigned long)total_size);
        esp_task_wdt_reset();

        offset += len;
        seq++;
    }

    notify(s, 100, OTA_STATE_TRANSFER, "传输完成");
    return true;
}

static void do_finish(ota_session_t *s, uint16_t seq)
{
    notify(s, 100, OTA_STATE_FINISH, "等待 STM32 确认");

    ota_transport_flush_rx();

    uint8_t frame[32];
    size_t n = ota_frame_build(frame, OTA_TYPE_FINISH, seq, 0, NULL, 0);
    ota_transport_send(frame, n);

    /* FINISH ACK 最长等 3s，喂狗防止与上一块传输的间隔累计超过看门狗超时 */
    esp_task_wdt_reset();
    if (wait_ack(seq, OTA_FINISH_ACK_TO)) {
        ESP_LOGI(TAG, "FINISH ACK OK  STM32 即将重启");
        notify(s, 100, OTA_STATE_SUCCESS, "升级完成");
    } else {
        ESP_LOGW(TAG, "FINISH 无 ACK（STM32 可能已复位）");
        notify(s, 100, OTA_STATE_SUCCESS, "升级完成(无ACK)");
    }
}

static uint32_t calc_total_crc(ota_session_t *s, uint32_t total_size)
{
    uint8_t buf[512];
    uint32_t offset = 0;
    uint32_t wdt_counter = 0;
    ota_crc32_t crc;
    ota_crc32_init(&crc);

    while (offset < total_size) {
        uint32_t remain = total_size - offset;
        uint32_t len = (remain > sizeof(buf)) ? sizeof(buf) : remain;
        int n = s->source->read(s->source->ctx, offset, buf, len);
        if (n <= 0) break;

        ota_crc32_update(&crc, buf, (size_t)n);
        offset += (uint32_t)n;

        wdt_counter += (uint32_t)n;
        if (wdt_counter >= 8192) {
            esp_task_wdt_reset();
            wdt_counter = 0;
        }
    }
    return ota_crc32_finalize(&crc);
}

static void ota_run(ota_session_t *s)
{
    /* 将当前任务纳入任务看门狗，防止长耗时传输触发复位 */
    esp_task_wdt_add(NULL);
    esp_task_wdt_reset();

    uint32_t total_size = s->source->get_size(s->source->ctx);
    if (total_size == 0 || total_size > OTA_MAX_FIRMWARE) {
        ESP_LOGE(TAG, "固件大小异常: %lu", (unsigned long)total_size);
        notify(s, 0, OTA_STATE_ERROR, "固件大小异常");
        goto done;
    }

    ESP_LOGI(TAG, "计算整包 CRC32...");
    uint32_t total_crc = calc_total_crc(s, total_size);
    ESP_LOGI(TAG, "固件: %lu 字节  CRC32: 0x%08lX",
             (unsigned long)total_size, (unsigned long)total_crc);

    uint8_t prep[8];
    build_prepare(prep, total_size, total_crc);

    ESP_LOGI(TAG, "等待 STM32...");
    if (!do_handshake(s, prep)) goto done;

    if (!do_data_transfer(s, total_size)) goto done;

    ESP_LOGI(TAG, "发送 FINISH");
    do_finish(s, (total_size + OTA_CHUNK_SIZE - 1) / OTA_CHUNK_SIZE + 1);

    ESP_LOGI(TAG, "OTA 完成");

done:
    esp_task_wdt_delete(NULL);
}

void ota_task_start(const uint8_t *firmware_data, uint32_t firmware_size, ota_callback_t callback)
{
    if (!firmware_data || firmware_size == 0) {
        ESP_LOGE(TAG, "无效的固件缓冲区");
        return;
    }

    ota_session_t session = {
        .source   = NULL,
        .buffer   = { .data = firmware_data, .size = firmware_size },
        .callback = callback,
    };
    OtaSource_t source = buffer_source_create(&session.buffer);
    session.source = &source;

    ota_run(&session);
}