#ifndef __BSP_W25QXX_H__
#define __BSP_W25QXX_H__

#include "main.h"
#include "spi.h"

/* ========================== W25QXX CS 引脚操作宏 ========================== */
#define W25QXX_CS_LOW()   HAL_GPIO_WritePin(W25CS_GPIO_Port, W25CS_Pin, GPIO_PIN_RESET)
#define W25QXX_CS_HIGH()  HAL_GPIO_WritePin(W25CS_GPIO_Port, W25CS_Pin, GPIO_PIN_SET)

/* ========================== W25QXX 指令集 ================================= */
#define W25QXX_CMD_WRITE_ENABLE      0x06  /* 写使能 */
#define W25QXX_CMD_WRITE_DISABLE     0x04  /* 写禁止 */
#define W25QXX_CMD_READ_STATUS1      0x05  /* 读状态寄存器1 */
#define W25QXX_CMD_READ_STATUS2      0x35  /* 读状态寄存器2 */
#define W25QXX_CMD_WRITE_STATUS      0x01  /* 写状态寄存器 */
#define W25QXX_CMD_READ_DATA         0x03  /* 读数据 */
#define W25QXX_CMD_FAST_READ         0x0B  /* 快速读 */
#define W25QXX_CMD_PAGE_PROGRAM      0x02  /* 页编程（最大256字节/页） */
#define W25QXX_CMD_SECTOR_ERASE      0x20  /* 扇区擦除（4KB） */
#define W25QXX_CMD_BLOCK_ERASE_32K   0x52  /* 32KB 块擦除 */
#define W25QXX_CMD_BLOCK_ERASE_64K   0xD8  /* 64KB 块擦除 */
#define W25QXX_CMD_CHIP_ERASE        0xC7  /* 整片擦除 */
#define W25QXX_CMD_POWER_DOWN        0xB9  /* 掉电模式 */
#define W25QXX_CMD_RELEASE_POWERDOWN 0xAB  /* 唤醒 */
#define W25QXX_CMD_DEVICE_ID         0x90  /* 读制造商/设备ID */
#define W25QXX_CMD_JEDEC_ID          0x9F  /* 读JEDEC ID */
#define W25QXX_CMD_READ_UNIQUE_ID    0x4B  /* 读唯一ID */

/* ========================== W25QXX 参数 =================================== */
#define W25QXX_PAGE_SIZE             256   /* 页大小：256 字节 */
#define W25QXX_SECTOR_SIZE           4096  /* 扇区大小：4KB */
#define W25QXX_BLOCK_SIZE            65536 /* 块大小：64KB */

/* ========================== 函数声明 ====================================== */
void BSP_W25QXX_Init(void);
void BSP_W25QXX_ReadID(uint8_t *id);
uint8_t BSP_W25QXX_ReadStatus(void);
void BSP_W25QXX_Read(uint32_t addr, uint8_t *buf, uint32_t len);
void BSP_W25QXX_PageProgram(uint32_t addr, const uint8_t *buf, uint16_t len);
void BSP_W25QXX_Write(uint32_t addr, const uint8_t *buf, uint32_t len);
void BSP_W25QXX_SectorErase(uint32_t addr);
void BSP_W25QXX_BlockErase(uint32_t addr);
void BSP_W25QXX_ChipErase(void);
void BSP_W25QXX_SelfTest(void);       /* 读写自检函数 */

#endif /* __BSP_W25QXX_H__ */