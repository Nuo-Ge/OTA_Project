#include "app_core.h"
#include "app_sensor.h"
#include "app_ui.h"
#include "bsp_at24cxx.h"
#include "bsp_uart.h"
#include "bsp_w25qxx.h"
#include "OLED.h"
#include "ota_parser.h"
#include "ota_protocol.h"
#include "ota_receiver.h"
#include <stdint.h>

/* ---- 文本唤醒指令 ---- */
#define START_OTA_CMD   "START_OTA\r\n"
#define START_OTA_LEN   10

/* ---- 应用状态 ---- */
typedef enum {
    APP_STATE_IDLE,
    APP_STATE_OTA
} AppState_t;

/* ---- 模块级静态变量 ---- */
static AppState_t   s_app_state = APP_STATE_IDLE;
static OtaParser_t  s_parser;
static uint8_t      s_cmd_match_idx = 0;

/* 每轮 Loop 最多处理 256 字节，防止串口风暴饿死其他任务 */
#define LOOP_BATCH  256

void AppCore_Init(void)
{
    App_UI_Init();
    AppSensor_Init();

    BSP_UART_Init();
    BSP_W25QXX_Init();
    BSP_AT24CXX_Init();

    OtaParser_Init(&s_parser);
    OtaReceiver_Init();
    OtaReceiver_SetEventCallback(App_UI_HandleOtaEvent);
}

void AppCore_Loop(void)
{
    RingBuf_t *rb = BSP_UART_GetRxRingBuf();
    uint8_t ch;
    uint16_t batch = LOOP_BATCH;

    while (batch > 0 && RingBuf_Read(rb, &ch)) {
        batch--;

        if (s_app_state == APP_STATE_IDLE) {
            if (ch == ((const uint8_t*)START_OTA_CMD)[s_cmd_match_idx]) {
                s_cmd_match_idx++;
                if (s_cmd_match_idx >= START_OTA_LEN) {
                    s_app_state = APP_STATE_OTA;
                    s_cmd_match_idx = 0;

                    AppSensor_Stop();

                    /* ★ 清空环形缓冲区：丢弃唤醒命令之后残留的脏数据
                     *   （如多余的 \n、ESP32 重发时的文本垃圾等），
                     *   确保解析器从下一帧的 0xAA 开始干净同步 */
                    BSP_UART_GetRxRingBuf()->read = BSP_UART_GetRxRingBuf()->write;

                    OtaParser_Init(&s_parser);
                    OtaReceiver_Init();

                    OLED_Clear();
                    OLED_ShowString(0, 0, (uint8_t*)"OTA MODE", 16);
                    OLED_ShowString(0, 16, (uint8_t*)"Waiting...", 16);
                    OLED_Update();
                }
            } else {
                s_cmd_match_idx = 0;
                if (ch == 'S') {
                    s_cmd_match_idx = 1;
                }
            }
        } else {
            int ret = OtaParser_Feed(&s_parser, ch);
            if (ret == 1) {
                OtaReceiver_HandleFrame(&s_parser.frame);
            }
        }
    }
}