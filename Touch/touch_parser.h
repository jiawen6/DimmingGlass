#ifndef TOUCH_PARSER_H
#define TOUCH_PARSER_H

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>

// 厂商支持配置（编译期可选）
#define SUPPORT_VENDOR_NUOFEI  1
// #define SUPPORT_VENDOR_HUAKE  1

// 最大帧长度（根据实际最大帧调整，例如 B 厂 12*10+7=127 字节，可设 256）
#define TOUCH_PARSER_MAX_FRAME_LEN  256

// 初始化解析器
void touch_parser_init(void);

// 向解析器输入一个字节（通常在串口接收回调中调用）
uint8_t touch_parser_feed(uint8_t byte);

uint16_t Get_Touch_Gestrue(uint8_t touch_num);

/* ==================== 触控超时自动复位 ====================
 * 目的：双指滑动过程中某指抬起/信号丢失导致触控帧中断时，
 *       让 single_touch / double_touch 的状态能自动恢复。
 * 规则：距最近一帧有效触控数据超过 TOUCH_STATE_TIMEOUT_MS 后：
 *       - PRESSED / SLIDING(按下/滑动中) -> TOUCH_RELEASED(释放)
 *       - 已 RELEASED                   -> TOUCH_IDLE(初始)
 *       - 其余(如 IDLE)                 -> 不变
 * 用法(应用任务中)：
 *   - 每收到一帧有效触控数据后：touch_parser_mark_activity(HAL_GetTick());
 *   - 周期(如每 1ms)调用：touch_parser_timeout_check(HAL_GetTick());
 *     (返回 1 表示本次有点被补成 RELEASED，可在需要时再查一次手势)
 * ========================================================== */
#ifndef TOUCH_STATE_TIMEOUT_MS
#define TOUCH_STATE_TIMEOUT_MS   150u   /* 无新触控帧超时(ms)，可按需调整 */
#endif

// 收到一帧有效触控数据后调用，刷新“最近活动”时刻
void touch_parser_mark_activity(uint32_t now_ms);

// 周期调用(建议 1ms)。超时则统一推进 single/double 触点状态。
// 返回 1：本次有触点被补成“抬起”(RELEASED)；返回 0：无事件。
uint8_t touch_parser_timeout_check(uint32_t now_ms);

// 用户需要实现的回调函数（使用 __weak 声明，可在其他文件中覆盖）
void touch_parser_on_frame_A(uint8_t *frame, uint16_t len);
void touch_parser_on_frame_B(uint8_t *frame, uint16_t len);

#endif // TOUCH_PARSER_H
