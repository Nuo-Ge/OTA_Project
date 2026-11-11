#include "bsp_w25qxx.h"
#include "spi.h"
#include "OLED.h"          /* 自检结果通过OLED显示 */

/* ========================== SPI 底层收发函数 ============================== */

/**
  * @brief  发送指令 + 24位地址（4字节合并为一次 HAL 调用）
  * @param  cmd:  1字节操作指令
  * @param  addr: 24位 Flash 地址（仅低24位有效）
  * @retval 无
  * @note   大多数读/写/擦除操作的指令头都是 cmd + addr[23:0]，
  *         此函数将 4 字节合并发送，避免多次 HAL 调用开销
  */
static inline void W25QXX_SendCmdAddr(uint8_t cmd, uint32_t addr)
{
    uint8_t header[4];
    header[0] = cmd;
    header[1] = (uint8_t)(addr >> 16);
    header[2] = (uint8_t)(addr >> 8);
    header[3] = (uint8_t)(addr);
    HAL_SPI_Transmit(&hspi1, header, 4, HAL_MAX_DELAY);
}

/**
  * @brief  通过硬件 SPI 发送单个字节
  * @param  data: 待发送的字节
  * @retval 无
  */
static inline void W25QXX_SPI_WriteByte(uint8_t data)
{
    HAL_SPI_Transmit(&hspi1, &data, 1, HAL_MAX_DELAY);
}

/**
  * @brief  通过硬件 SPI 接收单个字节
  * @param  无
  * @retval 接收到的字节
  */
static inline uint8_t W25QXX_SPI_ReadByte(void)
{
    uint8_t data;
    HAL_SPI_Receive(&hspi1, &data, 1, HAL_MAX_DELAY);
    return data;
}

/* ========================== 内部辅助函数 ================================== */

/**
  * @brief  发送写使能指令（每次写/擦除前必须执行）
  */
static void W25QXX_WriteEnable(void)
{
    W25QXX_CS_LOW();
    W25QXX_SPI_WriteByte(W25QXX_CMD_WRITE_ENABLE);
    W25QXX_CS_HIGH();
}

/**
  * @brief  轮询等待 Flash 内部操作完成（检查 BUSY 位，带超时保护）
  * @note   超时时间约 100000 次循环（防止硬件异常导致死循环）
  */
static void W25QXX_WaitBusy(void)
{
    uint32_t timeout = 100000;
    uint8_t status;

    W25QXX_CS_LOW();
    W25QXX_SPI_WriteByte(W25QXX_CMD_READ_STATUS1);
    do {
        status = W25QXX_SPI_ReadByte();
        if (--timeout == 0) break;          /* 超时退出，防止死循环 */
    } while (status & 0x01);
    W25QXX_CS_HIGH();
}

/* ========================== 对外 API 函数 ================================= */

void BSP_W25QXX_Init(void)
{
    W25QXX_CS_HIGH();
}

void BSP_W25QXX_ReadID(uint8_t *id)
{
    uint8_t cmd = W25QXX_CMD_JEDEC_ID;

    W25QXX_CS_LOW();
    HAL_SPI_Transmit(&hspi1, &cmd, 1, HAL_MAX_DELAY);   /* 发送指令 */
    HAL_SPI_Receive(&hspi1, id, 3, HAL_MAX_DELAY);       /* 连续收3字节ID */
    W25QXX_CS_HIGH();
}

uint8_t BSP_W25QXX_ReadStatus(void)
{
    uint8_t status;
    W25QXX_CS_LOW();
    W25QXX_SPI_WriteByte(W25QXX_CMD_READ_STATUS1);
    status = W25QXX_SPI_ReadByte();
    W25QXX_CS_HIGH();
    return status;
}

void BSP_W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len)
{
    W25QXX_CS_LOW();
    W25QXX_SendCmdAddr(W25QXX_CMD_READ_DATA, addr);     /* 指令+地址，1次HAL调用 */
    HAL_SPI_Receive(&hspi1, buf, len, HAL_MAX_DELAY);   /* 批量读取数据 */
    W25QXX_CS_HIGH();
}

void BSP_W25QXX_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    W25QXX_SendCmdAddr(W25QXX_CMD_PAGE_PROGRAM, addr);  /* 指令+地址 */
    HAL_SPI_Transmit(&hspi1, buf, len, HAL_MAX_DELAY);  /* 批量写入数据 */
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

void BSP_W25QXX_Write(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t remain = len;
    uint32_t offset = 0;
    uint16_t page_remain;

    while (remain > 0)
    {
        page_remain = W25QXX_PAGE_SIZE - (addr % W25QXX_PAGE_SIZE);
        if (remain < page_remain)
        {
            page_remain = (uint16_t)remain;
        }
        BSP_W25QXX_PageProgram(addr, buf + offset, page_remain);
        addr   += page_remain;
        offset += page_remain;
        remain -= page_remain;
    }
}

void BSP_W25QXX_SectorErase(uint32_t addr)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    W25QXX_SendCmdAddr(W25QXX_CMD_SECTOR_ERASE, addr);   /* 指令+地址 */
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

void BSP_W25QXX_BlockErase(uint32_t addr)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    W25QXX_SendCmdAddr(W25QXX_CMD_BLOCK_ERASE_64K, addr);
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

void BSP_W25QXX_ChipErase(void)
{
    W25QXX_WriteEnable();
    W25QXX_CS_LOW();
    W25QXX_SPI_WriteByte(W25QXX_CMD_CHIP_ERASE);         /* 整片擦除只需1字节指令 */
    W25QXX_CS_HIGH();
    W25QXX_WaitBusy();
}

/* ========================== 读写自检函数 ================================== */

void BSP_W25QXX_SelfTest(void)
{
    uint8_t id_buf[3];
    uint8_t tx_buf[100];
    uint8_t rx_buf[100];
    uint16_t id;
    uint8_t match = 1;
    int i;

    BSP_W25QXX_ReadID(id_buf);
    id = ((uint16_t)id_buf[0] << 8) | id_buf[1];

    for (i = 0; i < 100; i++) tx_buf[i] = (uint8_t)i;

    BSP_W25QXX_SectorErase(0x000000);
    BSP_W25QXX_Write(0x000000, tx_buf, 100);
    BSP_W25QXX_Read(0x000000, rx_buf, 100);

    for (i = 0; i < 100; i++)
    {
        if (tx_buf[i] != rx_buf[i]) { match = 0; break; }
    }

    if (match)
    {
        OLED_ShowString(0, 0, (uint8_t *)"W25QXX:OK  ", 12);
    }
    else
    {
        OLED_ShowString(0, 0, (uint8_t *)"W25QXX:FAIL", 12);
    }
    OLED_ShowHexNum(78, 0, id, 4, 12);
    OLED_Update();
}