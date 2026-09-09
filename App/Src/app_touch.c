#include "app_touch.h"
#include "bsp_uart3.h"
#include "touch_parser.h"
#include "FreeRTOS.h"
#include "task.h"

// 实现回调：处理 A 厂帧
//void touch_parser_on_frame_A(uint8_t *frame, uint16_t len)
//{
//    uint8_t data_len = frame[3];
//    uint8_t point_count = (data_len - 1) / 6;
//    // uint8_t total_points = frame[4 + data_len - 1]; // 可选

//    for (uint8_t i = 0; i < point_count; i++) {
//        uint8_t *p = &frame[4 + i * 6];
//        uint8_t status = p[0];
//        uint16_t x = p[2] | (p[3] << 8);
//        uint16_t y = p[4] | (p[5] << 8);
//        // 在这里处理触点，例如更新 PWM 相位
//    }
//}

//// 实现回调：处理 B 厂帧
//void touch_parser_on_frame_B(uint8_t *frame, uint16_t len)
//{
//    uint16_t data_len = frame[2] | (frame[3] << 8);
//    uint8_t point_count = (data_len - 2) / 12;
//    // uint8_t total_points = frame[4 + data_len - 1];

//    for (uint8_t i = 0; i < point_count; i++) {
//        uint8_t *p = &frame[5 + i * 12];
//        uint8_t status = p[0];
//        uint16_t x = p[2] | (p[3] << 8);
//        uint16_t y = p[4] | (p[5] << 8);
//        uint16_t width = p[6] | (p[7] << 8);
//        uint16_t height = p[8] | (p[9] << 8);
//        // 处理触点
//    }
//}

// 解析任务
void app_touch_task(void *argument)
{
    // 初始化接收和解析器
    uart_receiver_init();
    touch_parser_init();
    uint8_t data_result = 0;
    uint8_t Touch_Gestrue = 0;
    uint32_t now_ms = 0u;   /* 时间基(1ms)，用于触控超时复位 */

    while (1) {
        now_ms = HAL_GetTick();

        // 如果环形缓冲区有数据，逐个字节送入解析器
        while (uart_receiver_available() > 0) {
            uint8_t byte = uart_receiver_get_byte();
            data_result = touch_parser_feed(byte);
            if(data_result) 
            {
                touch_parser_mark_activity(now_ms);   /* 刷新“最近有触控数据” */
                Touch_Gestrue = Get_Touch_Gestrue();
            }
            StateMatch(Touch_Gestrue);
        }

        // 超时复位：某指抬起/丢帧导致无新数据超过阈值即统一推进触点状态
        touch_parser_timeout_check(now_ms);

        // 没有数据时让出 CPU
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

/*
// 在 FreeRTOS 初始化后创建任务
xTaskCreate(app_touch_task, "touch_task", 512, NULL, 2, NULL);
*/
