#include "bsp_uart.h"

/* ============ 内部变量 ============ */
static uint8_t    s_dma_rx_buf[DMA_RX_BUF_SIZE];   /* DMA 目标缓冲区 */
static uint16_t   s_dma_last_pos = 0;              /* 上次处理到的位置 */
static RingBuf_t  s_uart_rx_rb;                    /* 环形缓冲区 */

/* ============ 初始化 ============ */
void BSP_UART_Init(void)
{
    /* 1. 初始化环形缓冲区 */
    RingBuf_Init(&s_uart_rx_rb);

    /* 2. 重置位置指针 */
    s_dma_last_pos = 0;

    /* 3. 启动 DMA 接收（Circular 模式，自动循环） */
    HAL_UART_Receive_DMA(&huart1, s_dma_rx_buf, DMA_RX_BUF_SIZE);

    /* 4. 使能 UART IDLE 中断（HAL 不会自动开，要手动开） */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_IDLE);

    /* 5. 使能 UART 错误中断（溢出、噪声、帧错误等），防止毛刺死锁 */
    __HAL_UART_ENABLE_IT(&huart1, UART_IT_ERR);
}

/* ============ 把 DMA 新数据搬进环形缓冲区 ============ */
void BSP_UART_FlushToRingBuf(void)
{
    /* 1. 计算 DMA 当前写入位置 */
    uint16_t current_pos = DMA_RX_BUF_SIZE - __HAL_DMA_GET_COUNTER(&hdma_usart1_rx);

    /* 2. 没有新数据 */
    if (current_pos == s_dma_last_pos) {
        return;
    }

    /* 3. 不绕回：从 last 到 current-1 连续搬 */
    if (current_pos > s_dma_last_pos) {
        for (uint16_t i = s_dma_last_pos; i < current_pos; i++) {
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
        }
    }
    /* 4. 绕回：分两段搬 */
    else {
        /* 4a. 从 last 到缓冲区末尾 */
        for (uint16_t i = s_dma_last_pos; i < DMA_RX_BUF_SIZE; i++) {
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
        }
        /* 4b. 从缓冲区开头到 current-1 */
        for (uint16_t i = 0; i < current_pos; i++) {
            RingBuf_Write(&s_uart_rx_rb, s_dma_rx_buf[i]);
        }
    }

    /* 5. 更新 last_pos */
    s_dma_last_pos = current_pos;
}

/* ============ 获取环形缓冲区句柄 ============ */

/**
  * @brief  获取接收环形缓冲区的指针，供上层协议解析模块读取数据
  * @param  无
  * @retval 指向接收环形缓冲区 s_uart_rx_rb 的指针
  */
RingBuf_t* BSP_UART_GetRxRingBuf(void)
{
    return &s_uart_rx_rb;
}

/* ============ 发送数据（阻塞式） ============ */

/**
 * @brief  通过 USART1 阻塞式发送数据
 * @param  data : 待发送数据的指针
 * @param  len  : 发送字节数
 * @retval 无
 * @note   调用 HAL_UART_Transmit，超时时间 1000ms，
 *         发送完成前会阻塞当前任务
 */
void BSP_UART_Send(const uint8_t *data, uint16_t len)
{
    HAL_UART_Transmit(&huart1, (uint8_t*)data, len, 1000);
}

/**
 * @brief UART 错误回调（当发生噪声、溢出、帧错误时触发）
 *        只要报错，就强行重新启动 DMA 接收，防止死锁
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1) {
        /* 1. 停止当前的 DMA 接收 */
        HAL_UART_DMAStop(huart);

        /* 2. 清除所有错误标志（读 SR 和 DR 寄存器实现） */
        __HAL_UART_CLEAR_OREFLAG(huart);
        __HAL_UART_CLEAR_NEFLAG(huart);
        __HAL_UART_CLEAR_FEFLAG(huart);

        /* 3. 重置 last_pos，避免位置偏差 */
        s_dma_last_pos = 0;

        /* 4. 重新启动 DMA 循环接收 */
        HAL_UART_Receive_DMA(&huart1, s_dma_rx_buf, DMA_RX_BUF_SIZE);

        /* 5. HAL_UART_Receive_DMA 不会恢复错误中断，必须手动重开 */
        __HAL_UART_ENABLE_IT(huart, UART_IT_ERR);
    }
}