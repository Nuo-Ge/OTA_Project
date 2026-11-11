/*
*********************************************************************************************************
*
*	模块名称 : OLED驱动模块
*	文件名称 : bsp_oled.h
*	版    本 : V1.1
*	说    明 : OLED驱动(HAL库软件I2C版本，PB8=SCL/PB9=SDA)
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2025-12-11  waroyal  正式发布
*		V1.1    2026-09-23  waroyal  移植到HAL库，软件I2C
*
*	Copyright (C), 2010-2026, 瓦罗伊电子 Waroyal Studio
*
*********************************************************************************************************
*/

#ifndef _OLED_H_
#define _OLED_H_

#include "main.h"
#include <math.h>

#define USE_096_IIC_OLED    1
#define USE_096_SPI_OLED    2
#define USE_130_IIC_OLED    3

#define USE_OLED_TYPE       USE_096_IIC_OLED

#define OLED_X_WIDTH        128
#define OLED_Y_WIDTH        64

#define OLED_UNFILLED       0
#define OLED_FILLED         1

extern uint8_t OLED_DisplayBuf[128][8];

void OLED_Init(void);

void OLED_Update(void);
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

void OLED_Clear(void);
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);
void OLED_Reverse(void);
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height);

void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode);
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *p, uint8_t size);
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize);
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize);
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image);

void OLED_DrawPoint(int16_t x, int16_t y, uint8_t t);
uint8_t OLED_GetPoint(int16_t X, int16_t Y);
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1);
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled);
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled);
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled);
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled);
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled);

void OLED_ShowChinese12(uint8_t x, uint8_t y, uint8_t *str);
void OLED_ShowChinese16(uint8_t x, uint8_t y, uint8_t *str);
void OLED_ShowChinese(uint8_t x, uint8_t y, uint8_t *p, uint8_t size);

#endif

/***************************** 瓦罗伊电子 Waroyal Studio (END OF FILE) *********************************/
