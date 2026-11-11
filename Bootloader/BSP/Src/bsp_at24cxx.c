#include "bsp_at24cxx.h"
#include "i2c.h"
#include "OLED.h"          /* 自检结果通过OLED显示 */

/* ========================== 对外 API 函数 ================================= */

/**
  * @brief  AT24CXX 初始化（I2C 已在 MX_I2C1_Init() 中完成初始化）
  * @param  无
  * @retval 无
  */
void BSP_AT24CXX_Init(void)
{
    /* I2C1 外设已在 MX_I2C1_Init() 中初始化，此处无需额外操作 */
    /* 如需要可在 CHECK 区域添加初始化代码 */
}

/**
  * @brief  检测 AT24CXX 是否在线（发送设备地址，检测ACK）
  * @param  无
  * @retval 1: 设备存在
  *         0: 设备不存在或总线错误
  */
uint8_t BSP_AT24CXX_Check(void)
{
    if (HAL_I2C_IsDeviceReady(&hi2c1, AT24CXX_I2C_ADDR, 3, HAL_MAX_DELAY) == HAL_OK)
    {
        return 1;  /* 设备应答正常，在线 */
    }
    return 0;      /* 无应答，设备不在线 */
}

/**
  * @brief  从指定地址读取数据
  * @param  addr: EEPROM 内部起始地址
  * @param  buf:  接收缓冲区指针
  * @param  len:  读取长度（字节数）
  * @retval 无
  * @note   使用 HAL_I2C_Mem_Read，自动发送设备地址 + 内存地址 + 读取数据
  */
void BSP_AT24CXX_Read(uint16_t addr, uint8_t *buf, uint16_t len)
{
    /* HAL_I2C_Mem_Read 协议流程：
     *   1. START + 设备写地址(0xA0) + 内存地址高字节 + 内存地址低字节
     *   2. RESTART + 设备读地址(0xA1) + 连续读取len字节 + STOP */
    HAL_I2C_Mem_Read(&hi2c1, AT24CXX_I2C_ADDR, addr,
                     AT24CXX_MEM_ADDR_SIZE, buf, len, HAL_MAX_DELAY);
}

/**
  * @brief  向指定地址写入数据（自动处理跨页分割）
  * @param  addr: EEPROM 内部起始地址
  * @param  buf:  数据缓冲区指针
  * @param  len:  写入长度（字节数）
  * @retval 无
  * @note   EEPROM 写入后需等待内部写周期完成（约5ms），自动跨页分拆
  */
void BSP_AT24CXX_Write(uint16_t addr, const uint8_t *buf, uint16_t len)
{
    uint16_t remain = len;
    uint16_t offset = 0;
    uint16_t page_remain;

    while (remain > 0)
    {
        page_remain = AT24CXX_PAGE_SIZE - (addr % AT24CXX_PAGE_SIZE);
        if (remain < page_remain)
        {
            page_remain = remain;
        }

        HAL_I2C_Mem_Write(&hi2c1, AT24CXX_I2C_ADDR, addr,
                          AT24CXX_MEM_ADDR_SIZE, (uint8_t *)buf + offset,
                          page_remain, HAL_MAX_DELAY);

        addr   += page_remain;
        offset += page_remain;
        remain -= page_remain;

        HAL_Delay(5);          /* 等待内部写周期（典型5ms） */
    }
}

/* ========================== 读写自检函数 ================================== */

/**
  * @brief  AT24CXX 读写自检（写入→等待→回读比对）
  * @param  无
  * @retval 无
  * @note   写入16字节特征数据，额外等待10ms后回读比对
  *         跨页写入由 BSP_AT24CXX_Write 内部自动处理
  */
void BSP_AT24CXX_SelfTest(void)
{
    uint8_t tx_buf[16] = {0x11, 0x22, 0x33, 0x44,
                          0x55, 0x66, 0x77, 0x88,
                          0x99, 0xAA, 0xBB, 0xCC,
                          0xDD, 0xEE, 0xFF, 0x00};
    uint8_t rx_buf[16] = {0};
    uint8_t match = 1;
    int i;

    OLED_ShowString(0, 16, (uint8_t *)"AT24CXX:      ", 12);

    /* 第1步：设备检测 */
    if (BSP_AT24CXX_Check() == 0)
    {
        OLED_ShowString(0, 16, (uint8_t *)"AT24CXX:NO ACK", 12);
        OLED_Update();
        return;
    }

    /* 第2步：写入→回读（跨页拆分由 BSP_AT24CXX_Write 内部自动处理） */
    BSP_AT24CXX_Write(0x0000, tx_buf, 16);
    BSP_AT24CXX_Read(0x0000, rx_buf, 16);

    /* 第3步：逐字节比对 */
    for (i = 0; i < 16; i++)
    {
        if (tx_buf[i] != rx_buf[i]) { match = 0; break; }
    }

    /* 第4步：OLED 显示结果 */
    if (match)
    {
        OLED_ShowString(0, 16, (uint8_t *)"AT24CXX:OK    ", 12);
    }
    else
    {
        OLED_ShowString(0, 16, (uint8_t *)"AT24CXX:DATA!  ", 12);
        OLED_ShowHexNum(0, 32, rx_buf[0], 2, 12);  /* 显示回读值，帮助排查 */
        OLED_ShowHexNum(18, 32, rx_buf[1], 2, 12);
    }
    OLED_Update();
}