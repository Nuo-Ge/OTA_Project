/*
*********************************************************************************************************
*
*	模块名称 : OLED驱动模块
*	文件名称 : bsp_oled.c
*	版    本 : V1.1
*	说    明 : OLED驱动(HAL库软件I2C版本)
*
*	修改记录 :
*		版本号  日期        作者     说明
*		V1.0    2025-12-11  waroyal  正式发布(当前支持1.3寸及0.96寸IIC接口的OLED)
*		V1.1    2026-09-23  waroyal  移植到HAL库，PB8=SCL/PB9=SDA软件I2C
*
*	Copyright (C), 2010-2026, 瓦罗伊电子 Waroyal Studio
*
*********************************************************************************************************
*/

/* 头文件包含区 */
#include "OLED.h"
#include "bsp_ascii_font.h"
#include <math.h>

/* 变量定义区 */
/**
  * OLED显存数组
  * 所有的显示函数，都只是对此显存数组进行读写
  * 随后调用OLED_Update函数或OLED_UpdateArea函数
  * 才会将显存数组的数据发送到OLED硬件，进行显示
  */
uint8_t OLED_DisplayBuf[128][8];

/* 外部变量申明区 */


/*
*********************************************************************************************************

*                   HAL库软件I2C底层驱动 (PB8=SCL, PB9=SDA)
*********************************************************************************************************
*/

#define OLED_SCL_PORT    GPIOB
#define OLED_SDA_PORT    GPIOB
#define OLED_SCL_PIN     GPIO_PIN_8
#define OLED_SDA_PIN     GPIO_PIN_9

#define OLED_SCL_H()     HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_SET)
#define OLED_SCL_L()     HAL_GPIO_WritePin(OLED_SCL_PORT, OLED_SCL_PIN, GPIO_PIN_RESET)
#define OLED_SDA_H()     HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_SET)
#define OLED_SDA_L()     HAL_GPIO_WritePin(OLED_SDA_PORT, OLED_SDA_PIN, GPIO_PIN_RESET)
#define OLED_SDA_READ()  HAL_GPIO_ReadPin(OLED_SDA_PORT, OLED_SDA_PIN)

static void OLED_I2C_Delay(void)
{
    uint32_t i = 20;
    while (i--) { __NOP(); }
}

static void OLED_I2C_Start(void)
{
    OLED_SDA_H();
    OLED_SCL_H();
    OLED_I2C_Delay();
    OLED_SDA_L();
    OLED_I2C_Delay();
    OLED_SCL_L();
}

static void OLED_I2C_Stop(void)
{
    OLED_SDA_L();
    OLED_SCL_H();
    OLED_I2C_Delay();
    OLED_SDA_H();
    OLED_I2C_Delay();
}

static void OLED_I2C_SendByte(uint8_t Byte)
{
    uint8_t i;
    for (i = 0; i < 8; i++)
    {
        OLED_SCL_L();
        OLED_I2C_Delay();
        if (Byte & 0x80)
            OLED_SDA_H();
        else
            OLED_SDA_L();
        Byte <<= 1;
        OLED_I2C_Delay();
        OLED_SCL_H();
        OLED_I2C_Delay();
    }
    /* 第9个时钟：释放SDA，等待从机ACK */
    OLED_SCL_L();
    OLED_I2C_Delay();
    OLED_SDA_H();
    OLED_I2C_Delay();
    OLED_SCL_H();
    OLED_I2C_Delay();
    OLED_SCL_L();
    OLED_I2C_Delay();
}

/*
*********************************************************************************************************
*	函 数 名: OLED_WriteCommand
*	功能说明: OLED写命令
*	形    参：uint8_t Command 要写入的命令值，范围：0x00~0xFF
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_WriteCommand(uint8_t Command)
{
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x00);
	OLED_I2C_SendByte(Command);
	OLED_I2C_Stop();
}

/*
*********************************************************************************************************
*	函 数 名: OLED_WriteData
*	功能说明: OLED写数据
*	形    参：uint8_t *Data 要写入数据的起始地址

*             uint8_t Count 要写入数据的数量
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_WriteData(uint8_t *Data, uint8_t Count)
{
	uint8_t i;
	
	OLED_I2C_Start();
	OLED_I2C_SendByte(0x78);
	OLED_I2C_SendByte(0x40);
	for (i = 0; i < Count; i ++)
	{
		OLED_I2C_SendByte(Data[i]);
	}
	OLED_I2C_Stop();
}

/*
*********************************************************************************************************
*	函 数 名: OLED_Init
*	功能说明: OLED初始化函数
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_Init(void)
{
		/* GPIO已在MX_GPIO_Init()中初始化，此处无需额外初始化 */
	
	/*写入一系列的命令，对OLED进行初始化配置*/
	OLED_WriteCommand(0xAE);	//设置显示开启/关闭，0xAE关闭，0xAF开启
	
	OLED_WriteCommand(0xD5);	//设置显示时钟分频比/振荡器频率
	OLED_WriteCommand(0x80);	//0x00~0xFF
	
	OLED_WriteCommand(0xA8);	//设置多路复用率
	OLED_WriteCommand(0x3F);	//0x0E~0x3F
	
	OLED_WriteCommand(0xD3);	//设置显示偏移
	OLED_WriteCommand(0x00);	//0x00~0x7F
	
	OLED_WriteCommand(0x40);	//设置显示开始行，0x40~0x7F
	
	OLED_WriteCommand(0xA1);	//设置左右方向，0xA1正常，0xA0左右反置
	
	OLED_WriteCommand(0xC8);	//设置上下方向，0xC8正常，0xC0上下反置

	OLED_WriteCommand(0xDA);	//设置COM引脚硬件配置
	OLED_WriteCommand(0x12);
	
	OLED_WriteCommand(0x81);	//设置对比度
	OLED_WriteCommand(0xCF);	//0x00~0xFF

	OLED_WriteCommand(0xD9);	//设置预充电周期
	OLED_WriteCommand(0xF1);

	OLED_WriteCommand(0xDB);	//设置VCOMH取消选择级别
	OLED_WriteCommand(0x30);

	OLED_WriteCommand(0xA4);	//设置整个显示打开/关闭

	OLED_WriteCommand(0xA6);	//设置正常/反色显示，0xA6正常，0xA7反色

	OLED_WriteCommand(0x8D);	//设置充电泵
	OLED_WriteCommand(0x14);

	OLED_WriteCommand(0xAF);	//开启显示
	
	OLED_Clear();				//清空显存数组
	OLED_Update();				//更新显示，清屏，防止初始化后未显示内容时花屏
}

/*
*********************************************************************************************************
*	函 数 名: OLED_SetCursor
*	功能说明: OLED设置显示光标位置
*	形    参：X 指定光标所在的X轴坐标，范围：0~127
*             Page 指定光标所在的页，范围：0~7
*	返 回 值: 无
*   说    明: OLED默认的Y轴，只能8个Bit为一组写入，即1页等于8个Y轴坐标
*********************************************************************************************************
*/
void OLED_SetCursor(uint8_t X, uint8_t Page)
{
	/*如果使用此程序驱动1.3寸的OLED显示屏，则需要解除此注释*/
	/*因为1.3寸的OLED驱动芯片（SH1106）有132列*/
	/*屏幕的起始列接在了第2列，而不是第0列*/
	/*所以需要将X加2，才能正常显示*/
//	X += 2;
#if USE_OLED_TYPE == USE_130_IIC_OLED
    X += 2;
#endif
	
	/*通过指令设置页地址和列地址*/
	OLED_WriteCommand(0xB0 | Page);					//设置页位置
	OLED_WriteCommand(0x10 | ((X & 0xF0) >> 4));	//设置X位置高4位
	OLED_WriteCommand(0x00 | (X & 0x0F));			//设置X位置低4位
}

/*
*********************************************************************************************************
*	函 数 名: OLED_Pow
*	功能说明: 次方函数
*	形    参：X 底数
*             Y 指数
*	返 回 值: 等于X的Y次方
*********************************************************************************************************
*/
uint32_t OLED_Pow(uint32_t X, uint32_t Y)
{
	uint32_t Result = 1;	//结果默认为1
	while (Y --)			//累乘Y次
	{
		Result *= X;		//每次把X累乘到结果上
	}
	return Result;
}

/*
*********************************************************************************************************
*	函 数 名: OLED_pnpoly
*	功能说明: 判断指定点是否在指定多边形内部
*	形    参：nvert 多边形的顶点数
*             vertx verty 包含多边形顶点的x和y坐标的数组
*             testx testy 测试点的X和y坐标
*	返 回 值: 指定点是否在指定多边形内部，1：在内部，0：不在内部
*********************************************************************************************************
*/
uint8_t OLED_pnpoly(uint8_t nvert, int16_t *vertx, int16_t *verty, int16_t testx, int16_t testy)
{
	int16_t i, j, c = 0;
	
	/*此算法由W. Randolph Franklin提出*/
	/*参考链接：https://wrfranklin.org/Research/Short_Notes/pnpoly.html*/
	for (i = 0, j = nvert - 1; i < nvert; j = i++)
	{
		if (((verty[i] > testy) != (verty[j] > testy)) &&
			(testx < (vertx[j] - vertx[i]) * (testy - verty[i]) / (verty[j] - verty[i]) + vertx[i]))
		{
			c = !c;
		}
	}
	return c;
}

/*
*********************************************************************************************************
*	函 数 名: OLED_IsInAngle
*	功能说明: 判断指定点是否在指定角度内部
*	形    参：X Y 指定点的坐标
*             StartAngle EndAngle 起始角度和终止角度，范围：-180~180
*             水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
*	返 回 值: 指定点是否在指定角度内部，1：在内部，0：不在内部
*********************************************************************************************************
*/
uint8_t OLED_IsInAngle(int16_t X, int16_t Y, int16_t StartAngle, int16_t EndAngle)
{
	int16_t PointAngle;
	PointAngle = atan2(Y, X) / 3.14 * 180;	//计算指定点的弧度，并转换为角度表示
	if (StartAngle < EndAngle)	//起始角度小于终止角度的情况
	{
		/*如果指定角度在起始终止角度之间，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle && PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	else			//起始角度大于于终止角度的情况
	{
		/*如果指定角度大于起始角度或者小于终止角度，则判定指定点在指定角度*/
		if (PointAngle >= StartAngle || PointAngle <= EndAngle)
		{
			return 1;
		}
	}
	return 0;		//不满足以上条件，则判断判定指定点不在指定角度
}

/*
*********************************************************************************************************
*	函 数 名: OLED_Update
*	功能说明: 将OLED显存数组更新到OLED屏幕
*	形    参：无
*	返 回 值: 无
*   说    明: 所有的显示函数，都只是对OLED显存数组进行读写
*             随后调用OLED_Update函数或OLED_UpdateArea函数
*             才会将显存数组的数据发送到OLED硬件，进行显示
*             故调用显示函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_Update(void)
{    
    int i,n;
    for(i=0;i<8;i++)
	{
        OLED_SetCursor(0, i);        
		for(n=0;n<128;n++)
        {
            OLED_WriteData(&OLED_DisplayBuf[n][i],1);
        }
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_UpdateArea
*	功能说明: 将OLED显存数组部分更新到OLED屏幕
*	形    参：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Width 指定区域的宽度，范围：0~128
*             Height 指定区域的高度，范围：0~64
*	返 回 值: 无
*   说    明: 此函数会至少更新参数指定的区域
*             如果更新区域Y轴只包含部分页，则同一页的剩余部分会跟随一起更新
*             所有的显示函数，都只是对OLED显存数组进行读写
*             随后调用OLED_Update函数或OLED_UpdateArea函数
*             才会将显存数组的数据发送到OLED硬件，进行显示
*             故调用显示函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_UpdateArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t j;
	int16_t Page, Page1;
	
	/*负数坐标在计算页地址时需要加一个偏移*/
	/*(Y + Height - 1) / 8 + 1的目的是(Y + Height) / 8并向上取整*/
	Page = Y / 8;
	Page1 = (Y + Height - 1) / 8 + 1;
	if (Y < 0)
	{
		Page -= 1;
		Page1 -= 1;
	}
	
	/*遍历指定区域涉及的相关页*/
	for (j = Page; j < Page1; j ++)
	{
		if (X >= 0 && X <= 127 && j >= 0 && j <= 7)		//超出屏幕的内容不显示
		{
			/*设置光标位置为相关页的指定列*/
			OLED_SetCursor(X,j);
			/*连续写入Width个数据，将显存数组的数据写入到OLED硬件*/
			//OLED_WriteData(&OLED_DisplayBuf[X][j], Width);
            for(uint8_t offset=0;offset<Width;offset++)
            {
                OLED_WriteData(&OLED_DisplayBuf[X][j], 1);
            }
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_Clear
*	功能说明: 将OLED显存数组全部清零
*	形    参：无
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_Clear(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//遍历8页
	{
		for (i = 0; i < 128; i ++)			//遍历128列
		{
			OLED_DisplayBuf[i][j] = 0x00;	//将显存数组数据全部清零
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ClearArea
*	功能说明: 将OLED显存数组部分清零
*	形    参：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Width 指定区域的宽度，范围：0~128
*             Height 指定区域的高度，范围：0~64
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ClearArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;
	
	for (j = Y; j < Y + Height; j ++)		//遍历指定页
	{
		for (i = X; i < X + Width; i ++)	//遍历指定列
		{
			if (i >= 0 && i <= 127 && j >=0 && j <= 63)				//超出屏幕的内容不显示
			{
				OLED_DisplayBuf[i][j / 8] &= ~(0x01 << (j % 8));	//将显存数组指定数据清零
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_Reverse
*	功能说明: 将OLED显存数组全部取反
*	形    参：无
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_Reverse(void)
{
	uint8_t i, j;
	for (j = 0; j < 8; j ++)				//遍历8页
	{
		for (i = 0; i < 128; i ++)			//遍历128列
		{
			OLED_DisplayBuf[j][j] ^= 0xFF;	//将显存数组数据全部取反
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ReverseArea
*	功能说明: 将OLED显存数组部分取反
*	形    参：X 指定区域左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定区域左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Width 指定区域的宽度，范围：0~128
*             Height 指定区域的高度，范围：0~64
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ReverseArea(int16_t X, int16_t Y, uint8_t Width, uint8_t Height)
{
	int16_t i, j;
	
	for (j = Y; j < Y + Height; j ++)		//遍历指定页
	{
		for (i = X; i < X + Width; i ++)	//遍历指定列
		{
			if (i >= 0 && i <= 127 && j >=0 && j <= 63)			//超出屏幕的内容不显示
			{
				OLED_DisplayBuf[i][j / 8] ^= 0x01 << (j % 8);	//将显存数组指定数据取反
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawPoint
*	功能说明: 画点
*	形    参：x 指定点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             y 指定点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             t 1:填充 0:清空
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawPoint(int16_t x,int16_t y,uint8_t t)
{
	uint8_t pos,bx,temp=0;
    #if 0
	if(x>127||y>63)
        return;//超出范围了.
	pos=7-y/8;
	bx=y%8;
	temp=1<<(7-bx);
    #else
    if (x < 0 && x > 127 && y < 0 && y > 63)		//超出屏幕的内容不显示
        return;
    pos=y/8;
	bx=y%8;
	temp=1<<bx;
//    pos=7-y/8;
//	bx=y%8;
//	temp=1<<(7-bx);
    #endif
	if(t)
    {
        OLED_DisplayBuf[x][pos]|=temp;
    }
	else 
    {
        OLED_DisplayBuf[x][pos]&=~temp;
    }        
}

/*
*********************************************************************************************************
*	函 数 名: OLED_GetPoint
*	功能说明: OLED获取指定位置点的值
*	形    参：X 指定点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*	返 回 值: 指定位置点是否处于点亮状态，1：点亮，0：熄灭
*********************************************************************************************************
*/
uint8_t OLED_GetPoint(int16_t X, int16_t Y)
{
	if (X >= 0 && X <= 127 && Y >=0 && Y <= 63)		//超出屏幕的内容不读取
	{
		/*判断指定位置的数据*/
		if (OLED_DisplayBuf[X][Y / 8] & 0x01 << (Y % 8))
		{
			return 1;	//为1，返回1
		}
	}
	
	return 0;		//否则，返回0
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawLine
*	功能说明: OLED画线
*	形    参：X0 指定一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y0 指定一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             X1 指定另一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y1 指定另一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawLine(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1)
{
	int16_t x, y, dx, dy, d, incrE, incrNE, temp;
	int16_t x0 = X0, y0 = Y0, x1 = X1, y1 = Y1;
	uint8_t yflag = 0, xyflag = 0;
	
	if (y0 == y1)		//横线单独处理
	{
		/*0号点X坐标大于1号点X坐标，则交换两点X坐标*/
		if (x0 > x1) {temp = x0; x0 = x1; x1 = temp;}
		
		/*遍历X坐标*/
		for (x = x0; x <= x1; x ++)
		{
			OLED_DrawPoint(x, y0, 1);	//依次画点
		}
	}
	else if (x0 == x1)	//竖线单独处理
	{
		/*0号点Y坐标大于1号点Y坐标，则交换两点Y坐标*/
		if (y0 > y1) {temp = y0; y0 = y1; y1 = temp;}
		
		/*遍历Y坐标*/
		for (y = y0; y <= y1; y ++)
		{
			OLED_DrawPoint(x0, y, 1);	//依次画点
		}
	}
	else				//斜线
	{
		/*使用Bresenham算法画直线，可以避免耗时的浮点运算，效率更高*/
		/*参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
		/*参考教程：https://www.bilibili.com/video/BV1364y1d7Lo*/
		
		if (x0 > x1)	//0号点X坐标大于1号点X坐标
		{
			/*交换两点坐标*/
			/*交换后不影响画线，但是画线方向由第一、二、三、四象限变为第一、四象限*/
			temp = x0; x0 = x1; x1 = temp;
			temp = y0; y0 = y1; y1 = temp;
		}
		
		if (y0 > y1)	//0号点Y坐标大于1号点Y坐标
		{
			/*将Y坐标取负*/
			/*取负后影响画线，但是画线方向由第一、四象限变为第一象限*/
			y0 = -y0;
			y1 = -y1;
			
			/*置标志位yflag，记住当前变换，在后续实际画线时，再将坐标换回来*/
			yflag = 1;
		}
		
		if (y1 - y0 > x1 - x0)	//画线斜率大于1
		{
			/*将X坐标与Y坐标互换*/
			/*互换后影响画线，但是画线方向由第一象限0~90度范围变为第一象限0~45度范围*/
			temp = x0; x0 = y0; y0 = temp;
			temp = x1; x1 = y1; y1 = temp;
			
			/*置标志位xyflag，记住当前变换，在后续实际画线时，再将坐标换回来*/
			xyflag = 1;
		}
		
		/*以下为Bresenham算法画直线*/
		/*算法要求，画线方向必须为第一象限0~45度范围*/
		dx = x1 - x0;
		dy = y1 - y0;
		incrE = 2 * dy;
		incrNE = 2 * (dy - dx);
		d = 2 * dy - dx;
		x = x0;
		y = y0;
		
		/*画起始点，同时判断标志位，将坐标换回来*/
		if (yflag && xyflag){OLED_DrawPoint(y, -x, 1);}
		else if (yflag)		{OLED_DrawPoint(x, -y, 1);}
		else if (xyflag)	{OLED_DrawPoint(y, x, 1);}
		else				{OLED_DrawPoint(x, y, 1);}
		
		while (x < x1)		//遍历X轴的每个点
		{
			x ++;
			if (d < 0)		//下一个点在当前点东方
			{
				d += incrE;
			}
			else			//下一个点在当前点东北方
			{
				y ++;
				d += incrNE;
			}
			
			/*画每一个点，同时判断标志位，将坐标换回来*/
			if (yflag && xyflag){OLED_DrawPoint(y, -x, 1);}
			else if (yflag)		{OLED_DrawPoint(x, -y, 1);}
			else if (xyflag)	{OLED_DrawPoint(y, x, 1);}
			else				{OLED_DrawPoint(x, y, 1);}
		}	
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawRectangle
*	功能说明: OLED矩形
*	形    参：X 指定矩形左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定矩形左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Width 指定矩形的宽度，范围：0~128
*             Height 指定矩形的高度，范围：0~64
*             IsFilled 指定矩形是否填充，范围：OLED_UNFILLED不填充 OLED_FILLED填充
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawRectangle(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, uint8_t IsFilled)
{
	int16_t i, j;
	if (!IsFilled)		//指定矩形不填充
	{
		/*遍历上下X坐标，画矩形上下两条线*/
		for (i = X; i < X + Width; i ++)
		{
			OLED_DrawPoint(i, Y, 1);
			OLED_DrawPoint(i, Y + Height - 1, 1);
		}
		/*遍历左右Y坐标，画矩形左右两条线*/
		for (i = Y; i < Y + Height; i ++)
		{
			OLED_DrawPoint(X, i, 1);
			OLED_DrawPoint(X + Width - 1, i, 1);
		}
	}
	else				//指定矩形填充
	{
		/*遍历X坐标*/
		for (i = X; i < X + Width; i ++)
		{
			/*遍历Y坐标*/
			for (j = Y; j < Y + Height; j ++)
			{
				/*在指定区域画点，填充满矩形*/
				OLED_DrawPoint(i, j, 1);
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawTriangle
*	功能说明: OLED三角形
*	形    参：X0 指定第一个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y0 指定第一个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             X1 指定第二个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y1 指定第二个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             X2 指定第三个端点的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y2 指定第三个端点的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             IsFilled 指定三角形是否填充，范围：OLED_UNFILLED不填充 OLED_FILLED填充
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawTriangle(int16_t X0, int16_t Y0, int16_t X1, int16_t Y1, int16_t X2, int16_t Y2, uint8_t IsFilled)
{
	int16_t minx = X0, miny = Y0, maxx = X0, maxy = Y0;
	int16_t i, j;
	int16_t vx[] = {X0, X1, X2};
	int16_t vy[] = {Y0, Y1, Y2};
	
	if (!IsFilled)			//指定三角形不填充
	{
		/*调用画线函数，将三个点用直线连接*/
		OLED_DrawLine(X0, Y0, X1, Y1);
		OLED_DrawLine(X0, Y0, X2, Y2);
		OLED_DrawLine(X1, Y1, X2, Y2);
	}
	else					//指定三角形填充
	{
		/*找到三个点最小的X、Y坐标*/
		if (X1 < minx) {minx = X1;}
		if (X2 < minx) {minx = X2;}
		if (Y1 < miny) {miny = Y1;}
		if (Y2 < miny) {miny = Y2;}
		
		/*找到三个点最大的X、Y坐标*/
		if (X1 > maxx) {maxx = X1;}
		if (X2 > maxx) {maxx = X2;}
		if (Y1 > maxy) {maxy = Y1;}
		if (Y2 > maxy) {maxy = Y2;}
		
		/*最小最大坐标之间的矩形为可能需要填充的区域*/
		/*遍历此区域中所有的点*/
		/*遍历X坐标*/		
		for (i = minx; i <= maxx; i ++)
		{
			/*遍历Y坐标*/	
			for (j = miny; j <= maxy; j ++)
			{
				/*调用OLED_pnpoly，判断指定点是否在指定三角形之中*/
				/*如果在，则画点，如果不在，则不做处理*/
				if (OLED_pnpoly(3, vx, vy, i, j)) {OLED_DrawPoint(i, j, 1);}
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawCircle
*	功能说明: OLED画圆
*	形    参：X 指定圆的圆心横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定圆的圆心纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Radius 指定圆的半径，范围：0~255
*             IsFilled 指定圆是否填充，范围：OLED_UNFILLED不填充 OLED_FILLED填充
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawCircle(int16_t X, int16_t Y, uint8_t Radius, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/*使用Bresenham算法画圆，可以避免耗时的浮点运算，效率更高*/
	/*参考文档：https://www.cs.montana.edu/courses/spring2009/425/dslectures/Bresenham.pdf*/
	/*参考教程：https://www.bilibili.com/video/BV1VM4y1u7wJ*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*画每个八分之一圆弧的起始点*/
	OLED_DrawPoint(X + x, Y + y, 1);
	OLED_DrawPoint(X - x, Y - y, 1);
	OLED_DrawPoint(X + y, Y + x, 1);
	OLED_DrawPoint(X - y, Y - x, 1);
	
	if (IsFilled)		//指定圆填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在指定区域画点，填充部分圆*/
			OLED_DrawPoint(X, Y + j, 1);
		}
	}
	
	while (x < y)		//遍历X轴的每个点
	{
		x ++;
		if (d < 0)		//下一个点在当前点东方
		{
			d += 2 * x + 1;
		}
		else			//下一个点在当前点东南方
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*画每个八分之一圆弧的点*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X + y, Y + x, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - y, Y - x, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
		OLED_DrawPoint(X + y, Y - x, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X - y, Y + x, 1);
		
		if (IsFilled)	//指定圆填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点，填充部分圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
			
			/*遍历两侧部分*/
			for (j = -x; j < x; j ++)
			{
				/*在指定区域画点，填充部分圆*/
				OLED_DrawPoint(X - y, Y + j, 1);
				OLED_DrawPoint(X + y, Y + j, 1);
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawEllipse
*	功能说明: OLED画椭圆
*	形    参：X 指定椭圆的圆心横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定椭圆的圆心纵坐标，范围：-32768~32767，屏幕区域：0~63
*             A 指定椭圆的横向半轴长度，范围：0~255
*             B 指定椭圆的纵向半轴长度，范围：0~255
*             IsFilled 指定椭圆是否填充，范围：OLED_UNFILLED不填充 OLED_FILLED填充
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawEllipse(int16_t X, int16_t Y, uint8_t A, uint8_t B, uint8_t IsFilled)
{
	int16_t x, y, j;
	int16_t a = A, b = B;
	float d1, d2;
	
	/*使用Bresenham算法画椭圆，可以避免部分耗时的浮点运算，效率更高*/
	/*参考链接：https://blog.csdn.net/myf_666/article/details/128167392*/
	
	x = 0;
	y = b;
	d1 = b * b + a * a * (-b + 0.5);
	
	if (IsFilled)	//指定椭圆填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在指定区域画点，填充部分椭圆*/
			OLED_DrawPoint(X, Y + j, 1);
			OLED_DrawPoint(X, Y + j, 1);
		}
	}
	
	/*画椭圆弧的起始点*/
	OLED_DrawPoint(X + x, Y + y, 1);
	OLED_DrawPoint(X - x, Y - y, 1);
	OLED_DrawPoint(X - x, Y + y, 1);
	OLED_DrawPoint(X + x, Y - y, 1);
	
	/*画椭圆中间部分*/
	while (b * b * (x + 1) < a * a * (y - 0.5))
	{
		if (d1 <= 0)		//下一个点在当前点东方
		{
			d1 += b * b * (2 * x + 3);
		}
		else				//下一个点在当前点东南方
		{
			d1 += b * b * (2 * x + 3) + a * a * (-2 * y + 2);
			y --;
		}
		x ++;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点，填充部分椭圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
		}
		
		/*画椭圆中间部分圆弧*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
	}
	
	/*画椭圆两侧部分*/
	d2 = b * b * (x + 0.5) * (x + 0.5) + a * a * (y - 1) * (y - 1) - a * a * b * b;
	
	while (y > 0)
	{
		if (d2 <= 0)		//下一个点在当前点东方
		{
			d2 += b * b * (2 * x + 2) + a * a * (-2 * y + 3);
			x ++;
			
		}
		else				//下一个点在当前点东南方
		{
			d2 += a * a * (-2 * y + 3);
		}
		y --;
		
		if (IsFilled)	//指定椭圆填充
		{
			/*遍历两侧部分*/
			for (j = -y; j < y; j ++)
			{
				/*在指定区域画点，填充部分椭圆*/
				OLED_DrawPoint(X + x, Y + j, 1);
				OLED_DrawPoint(X - x, Y + j, 1);
			}
		}
		
		/*画椭圆两侧部分圆弧*/
		OLED_DrawPoint(X + x, Y + y, 1);
		OLED_DrawPoint(X - x, Y - y, 1);
		OLED_DrawPoint(X - x, Y + y, 1);
		OLED_DrawPoint(X + x, Y - y, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_DrawArc
*	功能说明: OLED画圆弧
*	形    参：X 指定圆弧的圆心横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定圆弧的圆心纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Radius 指定圆弧的半径，范围：0~255
*             StartAngle 指定圆弧的起始角度，范围：-180~180，水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
*             EndAngle 指定圆弧的终止角度，范围：-180~180，水平向右为0度，水平向左为180度或-180度，下方为正数，上方为负数，顺时针旋转
*             IsFilled 指定圆弧是否填充，填充后为扇形，范围：OLED_UNFILLED不填充 OLED_FILLED填充
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_DrawArc(int16_t X, int16_t Y, uint8_t Radius, int16_t StartAngle, int16_t EndAngle, uint8_t IsFilled)
{
	int16_t x, y, d, j;
	
	/*此函数借用Bresenham算法画圆的方法*/
	
	d = 1 - Radius;
	x = 0;
	y = Radius;
	
	/*在画圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
	if (OLED_IsInAngle(x, y, StartAngle, EndAngle))	{OLED_DrawPoint(X + x, Y + y, 1);}
	if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y, 1);}
	if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x, 1);}
	if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x, 1);}
	
	if (IsFilled)	//指定圆弧填充
	{
		/*遍历起始点Y坐标*/
		for (j = -y; j < y; j ++)
		{
			/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
			if (OLED_IsInAngle(0, j, StartAngle, EndAngle)) {OLED_DrawPoint(X, Y + j, 1);}
		}
	}
	
	while (x < y)		//遍历X轴的每个点
	{
		x ++;
		if (d < 0)		//下一个点在当前点东方
		{
			d += 2 * x + 1;
		}
		else			//下一个点在当前点东南方
		{
			y --;
			d += 2 * (x - y) + 1;
		}
		
		/*在画圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
		if (OLED_IsInAngle(x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + y, 1);}
		if (OLED_IsInAngle(y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + x, 1);}
		if (OLED_IsInAngle(-x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y - y, 1);}
		if (OLED_IsInAngle(-y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y - x, 1);}
		if (OLED_IsInAngle(x, -y, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y - y, 1);}
		if (OLED_IsInAngle(y, -x, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y - x, 1);}
		if (OLED_IsInAngle(-x, y, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + y, 1);}
		if (OLED_IsInAngle(-y, x, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + x, 1);}
		
		if (IsFilled)	//指定圆弧填充
		{
			/*遍历中间部分*/
			for (j = -y; j < y; j ++)
			{
				/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
				if (OLED_IsInAngle(x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + x, Y + j, 1);}
				if (OLED_IsInAngle(-x, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - x, Y + j, 1);}
			}
			
			/*遍历两侧部分*/
			for (j = -x; j < x; j ++)
			{
				/*在填充圆的每个点时，判断指定点是否在指定角度内，在，则画点，不在，则不做处理*/
				if (OLED_IsInAngle(-y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X - y, Y + j, 1);}
				if (OLED_IsInAngle(y, j, StartAngle, EndAngle)) {OLED_DrawPoint(X + y, Y + j, 1);}
			}
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowChar
*	功能说明: 在指定位置显示一个字符,包括部分字符
*	形    参：x 范围：0~127
*             y 范围：0~63
*             chr 要显示的字符
*             size 选择字体 12/16/24
*             mode 0:反白显示 1:正常显示
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size, uint8_t mode)
{
    uint8_t temp, t, t1;
    uint8_t y0 = y;
    uint8_t csize = (size / 8 + ((size % 8) ? 1 : 0)) * (size / 2);

    /* 当前 Bootloader 只保留 16x8 字体 */
    if (size != 16) return;

    chr = chr - ' ';

    for (t = 0; t < csize; t++) {
        temp = asc2_1608[chr][t];
        for (t1 = 0; t1 < 8; t1++) {
            if (temp & 0x80)
                OLED_DrawPoint(x, y, mode);
            else
                OLED_DrawPoint(x, y, !mode);
            temp <<= 1;
            y++;
            if ((y - y0) == size) {
                y = y0;
                x++;
                break;
            }
        }
    }
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowString
*	功能说明: 显示字符串
*	形    参：x 起点横坐标
*             y 起点纵坐标
*             *p 字符串起始地址
*             size 字体大小
*	返 回 值: 无
*********************************************************************************************************
*/
void OLED_ShowString(uint8_t x,uint8_t y,uint8_t *p,uint8_t size)
{	
    while((*p<='~')&&(*p>=' '))//判断是不是非法字符!
    {       
        if(x>(OLED_X_WIDTH-(size/2)))
        {
            x=0;y+=size;
        }
        if(y>(OLED_Y_WIDTH-size))
        {
            y=x=0;OLED_Clear();
        }
        OLED_ShowChar(x,y,*p,size,1);	 
        x+=size/2;
        p++;
    }  	
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowNum
*	功能说明: OLED显示数字（十进制，正整数）
*	形    参：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Number 指定要显示的数字，范围：0~4294967295
*             Length 指定数字的长度，范围：0~10
*             FontSize 指定字体大小，范围：OLED_8X16宽8像素高16像素 OLED_6X8宽6像素高8像素
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ShowNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)		//遍历数字的每一位							
	{
		/*调用OLED_ShowChar函数，依次显示每个数字*/
		/*Number / OLED_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位*/
		/*+ '0' 可将数字转换为字符格式*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowSignedNum
*	功能说明: OLED显示有符号数字（十进制，整数）
*	形    参：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Number 指定要显示的数字，范围：-2147483648~2147483647
*             Length 指定数字的长度，范围：0~10
*             FontSize 指定字体大小，范围：OLED_8X16宽8像素高16像素 OLED_6X8宽6像素高8像素
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ShowSignedNum(int16_t X, int16_t Y, int32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	uint32_t Number1;
	
	if (Number >= 0)						//数字大于等于0
	{
		OLED_ShowChar(X, Y, '+', FontSize, 1);	//显示+号
		Number1 = Number;					//Number1直接等于Number
	}
	else									//数字小于0
	{
		OLED_ShowChar(X, Y, '-', FontSize, 1);	//显示-号
		Number1 = -Number;					//Number1等于Number取负
	}
	
	for (i = 0; i < Length; i++)			//遍历数字的每一位								
	{
		/*调用OLED_ShowChar函数，依次显示每个数字*/
		/*Number1 / OLED_Pow(10, Length - i - 1) % 10 可以十进制提取数字的每一位*/
		/*+ '0' 可将数字转换为字符格式*/
		OLED_ShowChar(X + (i + 1) * FontSize, Y, Number1 / OLED_Pow(10, Length - i - 1) % 10 + '0', FontSize, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowHexNum
*	功能说明: OLED显示十六进制数字（十六进制，正整数）
*	形    参：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
*             Length 指定数字的长度，范围：0~8
*             FontSize 指定字体大小，范围：OLED_8X16宽8像素高16像素 OLED_6X8宽6像素高8像素
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ShowHexNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i, SingleNumber;
	for (i = 0; i < Length; i++)		//遍历数字的每一位
	{
		/*以十六进制提取数字的每一位*/
		SingleNumber = Number / OLED_Pow(16, Length - i - 1) % 16;
		
		if (SingleNumber < 10)			//单个数字小于10
		{
			/*调用OLED_ShowChar函数，显示此数字*/
			/*+ '0' 可将数字转换为字符格式*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber + '0', FontSize, 1);
		}
		else							//单个数字大于10
		{
			/*调用OLED_ShowChar函数，显示此数字*/
			/*+ 'A' 可将数字转换为从A开始的十六进制字符*/
			OLED_ShowChar(X + i * FontSize, Y, SingleNumber - 10 + 'A', FontSize, 1);
		}
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowBinNum
*	功能说明: OLED显示二进制数字（二进制，正整数）
*	形    参：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Number 指定要显示的数字，范围：0x00000000~0xFFFFFFFF
*             Length 指定数字的长度，范围：0~16
*             FontSize 指定字体大小，范围：OLED_8X16宽8像素高16像素 OLED_6X8宽6像素高8像素
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ShowBinNum(int16_t X, int16_t Y, uint32_t Number, uint8_t Length, uint8_t FontSize)
{
	uint8_t i;
	for (i = 0; i < Length; i++)		//遍历数字的每一位	
	{
		/*调用OLED_ShowChar函数，依次显示每个数字*/
		/*Number / OLED_Pow(2, Length - i - 1) % 2 可以二进制提取数字的每一位*/
		/*+ '0' 可将数字转换为字符格式*/
		OLED_ShowChar(X + i * FontSize, Y, Number / OLED_Pow(2, Length - i - 1) % 2 + '0', FontSize, 1);
	}
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowFloatNum
*	功能说明: OLED显示浮点数字（十进制，小数）
*	形    参：X 指定数字左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定数字左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Number 指定要显示的数字，范围：-4294967295.0~4294967295.0
*             IntLength 指定数字的整数位长度，范围：0~10
*             FraLength 指定数字的小数位长度，范围：0~9，小数进行四舍五入显示
*             FontSize 指定字体大小，范围：OLED_8X16宽8像素高16像素 OLED_6X8宽6像素高8像素
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*********************************************************************************************************
*/
void OLED_ShowFloatNum(int16_t X, int16_t Y, double Number, uint8_t IntLength, uint8_t FraLength, uint8_t FontSize)
{
	uint32_t PowNum, IntNum, FraNum;
	
	if (Number >= 0)						//数字大于等于0
	{
		OLED_ShowChar(X, Y, '+', FontSize, 1);	//显示+号
	}
	else									//数字小于0
	{
		OLED_ShowChar(X, Y, '-', FontSize, 1);	//显示-号
		Number = -Number;					//Number取负
	}
	
	/*提取整数部分和小数部分*/
	IntNum = Number;						//直接赋值给整型变量，提取整数
	Number -= IntNum;						//将Number的整数减掉，防止之后将小数乘到整数时因数过大造成错误
	PowNum = OLED_Pow(10, FraLength);		//根据指定小数的位数，确定乘数
	FraNum = round(Number * PowNum);		//将小数乘到整数，同时四舍五入，避免显示误差
	IntNum += FraNum / PowNum;				//若四舍五入造成了进位，则需要再加给整数
	
	/*显示整数部分*/
	OLED_ShowNum(X + FontSize, Y, IntNum, IntLength, FontSize);
	
	/*显示小数点*/
	OLED_ShowChar(X + (IntLength + 1) * FontSize, Y, '.', FontSize, 1);
	
	/*显示小数部分*/
	OLED_ShowNum(X + (IntLength + 2) * FontSize, Y, FraNum, FraLength, FontSize);
}

/*
*********************************************************************************************************
*	函 数 名: OLED_ShowImage
*	功能说明: OLED显示图像
*	形    参：X 指定图像左上角的横坐标，范围：-32768~32767，屏幕区域：0~127
*             Y 指定图像左上角的纵坐标，范围：-32768~32767，屏幕区域：0~63
*             Width 指定图像的宽度，范围：0~128
*             Height 指定图像的高度，范围：0~64
*             *Image 指定要显示的图像
*	返 回 值: 无
*   说    明: 调用此函数后，要想真正地呈现在屏幕上，还需调用更新函数
*             使用Image2Lcd软件，数据水平字节垂直，单色，字节内像素数据反序，高位在前
*********************************************************************************************************
*/
void OLED_ShowImage(int16_t X, int16_t Y, uint8_t Width, uint8_t Height, const uint8_t *Image)
{
    uint16_t x, y;
    uint8_t page, bit, data;
    
    /*将图像所在区域清空*/
	OLED_ClearArea(X, Y, Width, Height);

    // 遍历图片的每一个点 Y（纵向）
    for (y = 0; y < Height; y++)
    {
        page = y / 8;   // 计算在图片的第几页
        bit  = y % 8;   // 计算在页内第几位

        // 遍历图片的每一个点 X（横向）
        for (x = 0; x < Width; x++)
        {
            // 取出图片当前字节
            data = Image[page * Width + x];

            // 完全调用你的描点函数画点
            if (data & (1 << bit))
            {
                OLED_DrawPoint(X + x, Y + y, 1);  // 点亮
            }
            else
            {
                OLED_DrawPoint(X + x, Y + y, 0);  // 熄灭
            }
        }
    }
}
///////////////////////////////////////

/***************************** 瓦罗伊电子 Waroyal Studio (END OF FILE) *********************************/

