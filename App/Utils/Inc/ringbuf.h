#ifndef __RINGBUF_H__
#define __RINGBUF_H__

#include <stdint.h>

/* 缓冲区大小：2048 字节 */
#define RINGBUF_SIZE    4096

/* 环形缓冲区结构体 */
typedef struct {
    uint8_t  buffer[RINGBUF_SIZE];   /* 数据区 */
    uint16_t read;                   /* 读指针 */
    uint16_t write;                  /* 写指针 */
} RingBuf_t;

/* 初始化 */
void    RingBuf_Init(RingBuf_t *rb);

/* 判空 */
uint8_t RingBuf_IsEmpty(RingBuf_t *rb);

/* 判满 */
uint8_t RingBuf_IsFull(RingBuf_t *rb);

/* 写入 1 字节 */
uint8_t RingBuf_Write(RingBuf_t *rb, uint8_t data);

/* 读取 1 字节 */
uint8_t RingBuf_Read(RingBuf_t *rb, uint8_t *data);

#endif /* __RINGBUF_H__ */