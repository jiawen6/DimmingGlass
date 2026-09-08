#ifndef UART_RECEIVER_H
#define UART_RECEIVER_H

#include "main.h"
#include "ring_buffer.h"

// 用户可根据实际修改
#define UART_RX_DMA_BUFFER_SIZE  256    // DMA 缓冲区大小，需大于最大帧长

// 全局环形缓冲区实例（供解析任务读取）
extern ring_buffer_t uart3_rb;

// 初始化 UART 接收（启动 DMA 接收）
void uart_receiver_init(void);

// 从环形缓冲区读取一个字节（供解析任务调用）
uint8_t uart_receiver_get_byte(void);

// 检查环形缓冲区是否有数据
uint16_t uart_receiver_available(void);

#endif
