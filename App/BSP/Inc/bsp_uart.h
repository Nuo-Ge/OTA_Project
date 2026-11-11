#include "main.h"
#include "usart.h"
#include "ringbuf.h"

#ifndef __BSP_UART_H__
#define __BSP_UART_H__

/* DMA 接收缓冲区大小（必须是 2 的幂，方便用 & 运算优化） */
#define DMA_RX_BUF_SIZE     2048

/* 初始化 UART DMA 接收 */
void BSP_UART_Init(void);

/* 把 DMA 缓冲区里新收的数据搬进环形缓冲区 */
void BSP_UART_FlushToRingBuf(void);

/* 获取 UART 环形缓冲区句柄（给上层读数据用） */
RingBuf_t* BSP_UART_GetRxRingBuf(void);

/* 通过 UART 发送一帧数据（阻塞式） */
void BSP_UART_Send(const uint8_t *data, uint16_t len);

#endif /* __BSP_UART_H__ */
