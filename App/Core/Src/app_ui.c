#include "app_ui.h"
#include "OLED.h"

void App_UI_Init(void)
{
    OLED_Init();
    OLED_Clear();
    OLED_ShowString(0, 0, (uint8_t*)"APP READY", 16);
    OLED_Update();
}

void App_UI_HandleOtaEvent(OtaEvent_t event, uint32_t param1, uint32_t param2)
{
    switch (event) {
        case OTA_EVENT_PREPARE_START:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Preparing...", 16);
            OLED_ShowNum(0, 16, param1, 8, 16);
            OLED_Update();
            break;

        case OTA_EVENT_PREPARE_DONE:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Prepared!", 16);
            OLED_ShowNum(0, 16, param1, 8, 16);
            OLED_Update();
            break;

        case OTA_EVENT_RECEIVE_PROGRESS: {
            uint32_t percent = (param2 > 0) ? (param1 * 100 / param2) : 0;
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Receiving...", 16);
            OLED_ShowNum(0, 16, param1, 8, 16);
            OLED_ShowString(72, 16, (uint8_t*)"/", 16);
            OLED_ShowNum(88, 16, param2, 8, 16);
            OLED_ShowNum(0, 32, percent, 3, 16);
            OLED_ShowString(24, 32, (uint8_t*)"%", 16);
            OLED_Update();
            break;
        }

        case OTA_EVENT_VERIFY_START:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Verifying...", 16);
            OLED_Update();
            break;

        case OTA_EVENT_VERIFY_PASS:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"VERIFIED!", 16);
            OLED_ShowString(0, 16, (uint8_t*)"CRC:", 16);
            OLED_ShowHexNum(48, 16, param1, 8, 16);
            OLED_ShowString(0, 32, (uint8_t*)"Flag set!", 16);
            OLED_Update();
            break;

        case OTA_EVENT_VERIFY_FAIL:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"CRC FAIL!", 16);
            OLED_ShowString(0, 16, (uint8_t*)"calc:", 16);
            OLED_ShowHexNum(48, 16, param1, 8, 16);
            OLED_ShowString(0, 32, (uint8_t*)"recv:", 16);
            OLED_ShowHexNum(48, 32, param2, 8, 16);
            OLED_Update();
            break;

        case OTA_EVENT_ERROR_SIZE:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Size Error!", 16);
            OLED_ShowNum(0, 16, param1, 8, 16);
            OLED_Update();
            break;

        case OTA_EVENT_ERROR_OFFSET:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Offset ERR!", 16);
            OLED_ShowHexNum(0, 16, param1, 8, 16);
            OLED_Update();
            break;

        case OTA_EVENT_REBOOT:
            OLED_Clear();
            OLED_ShowString(0, 0,  (uint8_t*)"Rebooting...", 16);
            OLED_Update();
            break;

        default:
            break;
    }
}