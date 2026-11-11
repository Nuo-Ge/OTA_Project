#include "RingBuf.h"

/* 初始化：两个指针都置 0 */
void RingBuf_Init(RingBuf_t *rb)
{
    rb->read  = 0;
    rb->write = 0;
}

/* 判空：读指针 == 写指针 */
uint8_t RingBuf_IsEmpty(RingBuf_t *rb)
{
    return (rb->read == rb->write) ? 1 : 0;
}

/* 判满：写指针的下一个位置 == 读指针 */
uint8_t RingBuf_IsFull(RingBuf_t *rb)
{
    return (((rb->write + 1) % RINGBUF_SIZE) == rb->read) ? 1 : 0;
}

/* 写入 1 字节 */
uint8_t RingBuf_Write(RingBuf_t *rb, uint8_t data)
{
    /* 1. 满 → 写失败 */
    if (RingBuf_IsFull(rb)) {
        return 0;
    }

    /* 2. 数据放到写指针位置 */
    rb->buffer[rb->write] = data;

    /* 3. 写指针加 1，取模，防止越界 */
    rb->write = (rb->write + 1) % RINGBUF_SIZE;

    /* 4. 成功 */
    return 1;
}

/* 读取 1 字节 —— 这部分留给你自己写！ */
uint8_t RingBuf_Read(RingBuf_t *rb, uint8_t *data)
{
    /* TODO: 你来实现 */
    /* 1. 空 → 读失败 */
    if (RingBuf_IsEmpty(rb)) {
        return 0;
    }

    /* 2. 从读指针位置取数据 */
    *data = rb->buffer[rb->read];

    /* 3. 读指针加 1，取模，防止越界 */
    rb->read = (rb->read + 1) % RINGBUF_SIZE;

    /* 4. 成功 */
    return 1;
}