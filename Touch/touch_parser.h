#ifndef TOUCH_PARSER_H
#define TOUCH_PARSER_H

#include <stdint.h>
#include <stdbool.h>

// 厂商支持配置（编译期可选）
#define SUPPORT_VENDOR_A  1
#define SUPPORT_VENDOR_B  1

// 最大帧长度（根据实际最大帧调整，例如 B 厂 12*10+7=127 字节，可设 256）
#define TOUCH_PARSER_MAX_FRAME_LEN  256

// 初始化解析器
void touch_parser_init(void);

// 向解析器输入一个字节（通常在串口接收回调中调用）
void touch_parser_feed(uint8_t byte);

// 用户需要实现的回调函数（使用 __weak 声明，可在其他文件中覆盖）
void touch_parser_on_frame_A(uint8_t *frame, uint16_t len);
void touch_parser_on_frame_B(uint8_t *frame, uint16_t len);

#endif // TOUCH_PARSER_H