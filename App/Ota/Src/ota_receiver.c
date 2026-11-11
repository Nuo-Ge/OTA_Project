#include "ota_receiver.h"
#include "bsp_w25qxx.h"
#include "bsp_at24cxx.h"
#include "bsp_uart.h"
#include "crc32.h"
#include <string.h>

/* ============ 文件级状态变量 ============ */
static uint32_t g_total_size     = 0;
static uint32_t g_total_crc32    = 0;
static uint32_t g_received_bytes = 0;
static OtaEventCallback_t g_event_cb = NULL;

/* ============ 内部函数声明 ============ */
static void Handle_Prepare(OtaFrame_t *frame);
static void Handle_Data(OtaFrame_t *frame);
static void Handle_Finish(OtaFrame_t *frame);
static void Send_Ack(uint16_t seq);
static void FireEvent(OtaEvent_t event, uint32_t p1, uint32_t p2);

/* ============ 初始化 ============ */
void OtaReceiver_Init(void)
{
    g_total_size     = 0;
    g_total_crc32    = 0;
    g_received_bytes = 0;
    g_event_cb       = NULL;
}

/* ============ 注册事件回调 ============ */
void OtaReceiver_SetEventCallback(OtaEventCallback_t cb)
{
    g_event_cb = cb;
}

/* ============ 帧分派 ============ */
void OtaReceiver_HandleFrame(OtaFrame_t *frame)
{
    if (frame == NULL) {
        return;
    }

    switch (frame->type) {
        case OTA_TYPE_PREPARE:
            Handle_Prepare(frame);
            break;

        case OTA_TYPE_DATA:
            Handle_Data(frame);
            break;

        case OTA_TYPE_FINISH:
            Handle_Finish(frame);
            break;

        default:
            break;
    }
}

/* ============ 事件发射 ============ */
static void FireEvent(OtaEvent_t event, uint32_t p1, uint32_t p2)
{
    if (g_event_cb != NULL) {
        g_event_cb(event, p1, p2);
    }
}

/* ============ 三个 Handler ============ */
static void Handle_Prepare(OtaFrame_t *frame)
{
    if (frame->length < 8) {
        return;
    }

    uint32_t total_size = ((uint32_t)frame->data[0] << 24) |
                          ((uint32_t)frame->data[1] << 16) |
                          ((uint32_t)frame->data[2] << 8)  |
                          ((uint32_t)frame->data[3]);

    uint32_t total_crc32 = ((uint32_t)frame->data[4] << 24) |
                           ((uint32_t)frame->data[5] << 16) |
                           ((uint32_t)frame->data[6] << 8)  |
                           ((uint32_t)frame->data[7]);

    if (total_size == 0 || total_size > W25QXX_FIRMWARE_MAX) {
        FireEvent(OTA_EVENT_ERROR_SIZE, total_size, 0);
        return;
    }

    FireEvent(OTA_EVENT_PREPARE_START, total_size, 0);

    uint32_t erase_addr = W25QXX_FIRMWARE_ADDR;
    uint32_t remaining = total_size;

    while (remaining > 0) {
        BSP_W25QXX_BlockErase(erase_addr);
        erase_addr += W25QXX_BLOCK_SIZE;
        if (remaining > W25QXX_BLOCK_SIZE) {
            remaining -= W25QXX_BLOCK_SIZE;
        } else {
            remaining = 0;
        }
    }

    OtaInfo_t info;
    info.magic       = OTA_MAGIC;
    info.flag        = OTA_FLAG_NONE;
    info.reserved[0] = 0;
    info.reserved[1] = 0;
    info.reserved[2] = 0;
    info.total_size  = total_size;
    info.total_crc32 = total_crc32;

    BSP_AT24CXX_Write(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    g_total_size     = total_size;
    g_total_crc32    = total_crc32;
    g_received_bytes = 0;

    FireEvent(OTA_EVENT_PREPARE_DONE, total_size, 0);

    Send_Ack(frame->seq);
}

static void Handle_Data(OtaFrame_t *frame)
{
    if (frame->length == 0 || frame->length > OTA_MAX_DATA_LEN) {
        return;
    }

    if (frame->offset + frame->length > g_total_size) {
        FireEvent(OTA_EVENT_ERROR_OFFSET, frame->offset, frame->length);
        return;
    }

    BSP_W25QXX_Write(W25QXX_FIRMWARE_ADDR + frame->offset,
                     frame->data,
                     frame->length);

    g_received_bytes += frame->length;

    FireEvent(OTA_EVENT_RECEIVE_PROGRESS, g_received_bytes, g_total_size);

    Send_Ack(frame->seq);
}

static void Handle_Finish(OtaFrame_t *frame)
{
    FireEvent(OTA_EVENT_VERIFY_START, 0, 0);

    uint8_t  chunk[256];
    uint32_t crc = CRC32_Start();
    uint32_t offset = 0;

    while (offset < g_total_size) {
        uint32_t remain = g_total_size - offset;
        uint16_t len = (remain > 256) ? 256 : (uint16_t)remain;

        BSP_W25QXX_Read(W25QXX_FIRMWARE_ADDR + offset, chunk, len);
        crc = CRC32_UpdateBlock(crc, chunk, len);

        offset += len;
    }
    crc = CRC32_Final(crc);

    if (crc != g_total_crc32) {
        FireEvent(OTA_EVENT_VERIFY_FAIL, crc, g_total_crc32);
        return;
    }

    OtaInfo_t info;
    BSP_AT24CXX_Read(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));
    info.flag = OTA_FLAG_READY;
    BSP_AT24CXX_Write(OTA_INFO_ADDR, (uint8_t*)&info, sizeof(info));

    FireEvent(OTA_EVENT_VERIFY_PASS, crc, 0);

    Send_Ack(frame->seq);

    FireEvent(OTA_EVENT_REBOOT, 0, 0);

    HAL_Delay(2000);
    NVIC_SystemReset();
}

/* ============ 发送 ACK ============ */
static void Send_Ack(uint16_t seq)
{
    uint8_t frame[16];
    uint8_t idx = 0;

    uint8_t payload[9];
    payload[0] = OTA_TYPE_ACK;
    payload[1] = (seq >> 8) & 0xFF;
    payload[2] = seq & 0xFF;
    payload[3] = 0x00;
    payload[4] = 0x00;
    payload[5] = 0x00;
    payload[6] = 0x00;
    payload[7] = 0x00;
    payload[8] = 0x00;

    uint32_t crc = CRC32_Calculate(payload, 9);

    frame[idx++] = OTA_MAGIC_0;
    frame[idx++] = OTA_MAGIC_1;
    memcpy(&frame[idx], payload, 9); idx += 9;
    frame[idx++] = (crc >> 24) & 0xFF;
    frame[idx++] = (crc >> 16) & 0xFF;
    frame[idx++] = (crc >> 8)  & 0xFF;
    frame[idx++] = (crc)       & 0xFF;
    frame[idx++] = OTA_TAIL;

    BSP_UART_Send(frame, idx);
}