#ifndef __BSP_AT24CXX_H__
#define __BSP_AT24CXX_H__

#include "main.h"
#include "i2c.h"

/* ======================= AT24CXX I2C 地址 ================================ */
/* 7位地址: 1010 + A2A1A0 = 0x50; 8位写地址 = 0xA0 */
#define AT24CXX_I2C_ADDR    0xA0

/* ======================= AT24CXX 型号选择 ================================ */
/* 0x01=AT24C01, 0x02=AT24C02, 0x04=AT24C04, 0x08=AT24C08, 0x10=AT24C16
   0x20=AT24C32, 0x40=AT24C64, 0x80=AT24C128, 0x100=AT24C256 */
#define AT24CXX_TYPE        0x02  /* 选择AT24C256（可修改为实际型号） */

/* ======================= 内存地址宽度选择 ================================ */
/* AT24C32及以上（容量>2KB）使用16位内存地址，其余使用8位 */
#if AT24CXX_TYPE > 0x10
#define AT24CXX_MEM_ADDR_SIZE  I2C_MEMADD_SIZE_16BIT
#else
#define AT24CXX_MEM_ADDR_SIZE  I2C_MEMADD_SIZE_8BIT
#endif

/* ======================= 页大小（根据型号自动选择）======================== */
#if AT24CXX_TYPE <= 0x02
#define AT24CXX_PAGE_SIZE   8    /* AT24C01/02: 每页8字节 */
#elif AT24CXX_TYPE <= 0x10
#define AT24CXX_PAGE_SIZE   16   /* AT24C04/08/16: 每页16字节 */
#elif AT24CXX_TYPE <= 0x40
#define AT24CXX_PAGE_SIZE   32   /* AT24C32/64: 每页32字节 */
#else
#define AT24CXX_PAGE_SIZE   64   /* AT24C128/256: 每页64字节 */
#endif

/* ======================= 函数声明 ========================================= */
void     BSP_AT24CXX_Init(void);
uint8_t  BSP_AT24CXX_Check(void);
void     BSP_AT24CXX_Read(uint16_t addr, uint8_t *buf, uint16_t len);
void     BSP_AT24CXX_Write(uint16_t addr, const uint8_t *buf, uint16_t len);
void     BSP_AT24CXX_SelfTest(void);   /* 读写自检函数 */

#endif /* __BSP_AT24CXX_H__ */