#include "app_sensor.h"

void AppSensor_Init(void)
{
    /* TODO: 初始化传感器通信（GPIO、USART、I2C 等） */
}

void AppSensor_Stop(void)
{
    /* TODO: 通知传感器暂停上报数据
     *
     * 实现方式取决于你的传感器:
     *   - 发 AT 指令:  USART_SendString("AT+STOP\r\n");
     *   - 拉低 GPIO:   HAL_GPIO_WritePin(SENSOR_EN_GPIO_Port, SENSOR_EN_Pin, GPIO_PIN_RESET);
     *   - 关传感器电源: 控制 MOS 管或 LDO 使能引脚
     *
     * OTA 完成后 NVIC_SystemReset() 会重启系统，
     * AppSensor_Init() 会被再次调用，传感器自动恢复。
     */
}