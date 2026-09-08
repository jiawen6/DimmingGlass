#include "bsp_uart3.h"
#include "usart.h"    // CubeMX 生成的 USART 句柄
#include "dma.h"      // CubeMX 生成的 DMA 句柄
#include <string.h>

// 环形缓冲区实例
ring_buffer_t uart3_rb;

// DMA 接收缓冲区（需为全局或静态，防止被回收）
static uint8_t uart_rx_dma_buf[UART_RX_DMA_BUFFER_SIZE];

// 标志：接收错误发生
static volatile bool uart_error_occurred = false;

// 初始化接收
void uart_receiver_init(void)
{
    ring_buffer_init(&uart3_rb);
    // 启动 DMA + IDLE 接收
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart_rx_dma_buf, UART_RX_DMA_BUFFER_SIZE);
    // 使能 UART 错误中断（已在 CubeMX 中配置则无需重复）
    // __HAL_UART_ENABLE_IT(&huart3, UART_IT_ERR);
    __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
}

// 供解析任务读取一个字节
uint8_t uart_receiver_get_byte(void)
{
    return ring_buffer_read(&uart3_rb);
}

// 检查可用字节数
uint16_t uart_receiver_available(void)
{
    return ring_buffer_available(&uart3_rb);
}

// 接收事件回调（IDLE 或 DMA 满）
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART3) {
        // 将数据写入环形缓冲区
        for (uint16_t i = 0; i < Size; i++) {
            ring_buffer_write(&uart3_rb, uart_rx_dma_buf[i]);
        }
        // 重新启动接收（若使用 Circular 模式，HAL 内部会处理，调用此函数可确保继续）
        HAL_UARTEx_ReceiveToIdle_DMA(&huart3, uart_rx_dma_buf, UART_RX_DMA_BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
    }
}

// UART 错误回调（溢出、帧错误等）
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        uart_error_occurred = true;
        // 清除错误标志
        __HAL_UART_CLEAR_FLAG(huart, UART_CLEAR_OREF | UART_CLEAR_FEF | UART_CLEAR_NEF | UART_CLEAR_PEF);
        // 停止 DMA 接收
        HAL_UART_DMAStop(huart);
        // 重新启动接收
        HAL_UARTEx_ReceiveToIdle_DMA(huart, uart_rx_dma_buf, UART_RX_DMA_BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
        uart_error_occurred = false;
    }
}

// 可选：DMA 错误回调
void HAL_UART_DMAErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        // 处理 DMA 错误，重新启动
        HAL_UART_DMAStop(huart);
        HAL_UARTEx_ReceiveToIdle_DMA(huart, uart_rx_dma_buf, UART_RX_DMA_BUFFER_SIZE);
        __HAL_DMA_DISABLE_IT(&hdma_usart3_rx, DMA_IT_HT);
    }
}
