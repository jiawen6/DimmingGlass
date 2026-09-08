/**
  ******************************************************************************
  * @file    app_dimmer.h
  * @brief   调光应用：7(Y1~Y7 行) x 15(X1~X15 列) PDLC 调光区域驱动。
  *
  *          通过 22 路 GPIO 软件 PWM 控制"整行 / 整列"透光率。
  *          软件方案与外部 DeepSeek 讨论结论一致：
  *          - 所有通道周期相同、占空比固定 50%，仅调节相位(phase)；
  *          - 区域(r,c) 的透光率取决于 Xc 与 Yr 间的"电平差时间占比 D"；
  *          - 依据目标透光率反算活动电极相对参考电极的相位偏移。
  *
  ******************************************************************************
  * ============================== 底层原理 ===================================
  * 1) 每个 PWM 周期被分成 DIMMER_PWM_N 个 tick，全部通道共用同一个 tick 计数。
  *    当前 tick 下某通道输出高电平的条件(占空比 50%)：
  *        pos = (tick - phase[ch] + N) % N ;  pos < N/2  -> 高，否则 -> 低
  * 2) 任意两路(Xc, Yr)电平差时间占比：
  *        diff = |phase[Xc]-phase[Yr]| 取循环最小距离
  *        D    = 2 * min(diff, N-diff) / N          (0.0 ~ 1.0)
  * 3) 目标透光率 level(0~1000，即 0.0%~100.0%)近似当作目标电平差占比：
  *        delta = round( N * (level/1000) / 2 )
  *    活动电极相位 = (参考相位 + delta) mod N；其余电极与参考同相。
  *
  * ============================ 整行 / 整列原理 ==============================
  * 区域(r,c) = 行电极 Yr 与 列电极 Xc 的交叉点。
  *  "第 r 行整行亮到 level"：
  *      - 15 根 X 列全部同相(ref)；
  *      - Yr 相对 ref 偏移 delta(与所有列产生压差 -> 该行 15 个区域透光)；
  *      - 其余 Y 行保持与 X 同相(无压差 -> 关闭)。
  *  "第 c 列整列亮到 level" 对称(角色互换)。
  * 同一时刻仅允许单一图案(整行或整列)，后一次调用覆盖前一次；无需分时扫描。
  * level = 0 或调用 app_dimmer_all_off() 可整屏关闭。
  *
  * ============================== 底层输出时序 ===============================
  * 本模块用 HAL_GPIO_WritePin() 逐路输出(可移植、可读性好)。
  * 需要在 STM32CubeMX 中新增一个基本定时器(如 TIM6)并开启更新中断：
  *      - 时钟源 Internal Clock；
  *      - 更新中断频率 f_isr = PWM频率 x DIMMER_PWM_N
  *        例：PWM=60Hz、N=128 -> f_isr=7.68kHz；N=64 -> 3.84kHz；
  *      - NVIC 优先级建议低于(数字大于) configMAX_SYSCALL_INTERRUPT_PRIORITY
  *        对应的 5，保证不与 FreeRTOS 冲突；中断内不调用任何 RTOS API。
  * 在该定时器的中断服务函数(USER CODE 区)中调用 app_dimmer_tick_isr()。
  *
  * @note GPIO 为 0~3.3V 单极性，PDLC 需高压交流差分驱动：区域压差
  *       (Xc - Yr) 经外部驱动电路放大后施加到玻璃两端；两路同相->压差 0->不透明。
  ******************************************************************************
  */
#ifndef APP_DIMMER_H
#define APP_DIMMER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ------------------------- 矩阵规模 ------------------------- */
/* 行 = Y 电极(7)，列 = X 电极(15) */
#define DIMMER_ROW_NUM    7u
#define DIMMER_COL_NUM    15u
#define DIMMER_CH_NUM     (DIMMER_ROW_NUM + DIMMER_COL_NUM)   /* 22 */

/* ------------------------- 软 PWM 参数 ----------------------- */
/* 每 PWM 周期 tick 分辨率。需为偶数；4 的倍数或 2 的幂最佳。
   电平差占比(透光率)步进 = 2/N，相位分辨率 = 1 tick。 */
#ifndef DIMMER_PWM_N
#define DIMMER_PWM_N      128u
#endif

/* 透光率满量程(千分比：0 ~ 1000，对应 0.0% ~ 100.0%) */
#define DIMMER_LEVEL_MAX  1000u

/* ------------------------- 对外接口 -------------------------- */
/**
  * @brief  初始化调光模块(所有电极同相 -> 整屏关闭)。复位后即使不调用，
  *         静态初值也等价于全关；此函数供显式/复位重新初始化使用。
  */
void app_dimmer_init(void);

/**
  * @brief  第 row 行(Yrow, 1~7) 整行透光率设为 level(0~1000)。
  * @note   覆盖之前的列/行图案；level=0 等价整屏关闭。
  */
void app_dimmer_set_row(uint8_t row, uint16_t level);

/**
  * @brief  第 col 列(Xcol, 1~15) 整列透光率设为 level(0~1000)。
  * @note   覆盖之前的行/列图案；level=0 等价整屏关闭。
  */
void app_dimmer_set_col(uint8_t col, uint16_t level);

/**
  * @brief  整屏关闭(所有电极同相，任意区域压差为 0)。
  */
void app_dimmer_all_off(void);

/**
  * @brief  底层驱动：定时器更新中断里每个 tick 调用一次，
  *         把 22 路 GPIO 按当前相位刷新为 50% 占空比方波。
  * @note   必须在硬件定时器中断上下文调用，保持频率 = PWM频率 x DIMMER_PWM_N。
  */
void app_dimmer_tick_isr(void);

/**
  * @brief  把透光率 level(0~1000)换算为相位偏移 delta(tick，0~N/2)。
  */
uint16_t app_dimmer_level_to_delta(uint16_t level);

/**
  * @brief  读取某通道当前相位(tick)。
  * @param  ch  0..14 = X1..X15(列)，15..21 = Y1..Y7(行)。
  */
uint16_t app_dimmer_get_phase(uint8_t ch);

/**
  * @brief  计算两路相位之间的电平差时间占比(0.0 ~ 1.0)。
  */
float app_dimmer_phase_diff_duty(uint16_t phase_a, uint16_t phase_b);

/**
  * @brief  FreeRTOS 任务入口(可选)。当前"整行/整列即时生效"无需常驻任务；
  *         若后续增加分时扫描/渐变动画，可在此任务中周期调度。
  */
void app_dimmer_task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* APP_DIMMER_H */
