#ifndef __APP_UI_H__
#define __APP_UI_H__

#include "ota_receiver.h"

void App_UI_Init(void);
void App_UI_HandleOtaEvent(OtaEvent_t event, uint32_t param1, uint32_t param2);

#endif /* __APP_UI_H__ */