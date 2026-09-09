#include "app_dimmer.h"

#include "main.h"          /* X1~X15 / Y1~Y7 引脚宏 */
#include "FreeRTOS.h"
#include "task.h"
#include "tim.h"          // 为了 htim1
/* ============================================================================
 *  =========================== 使用方法(单行/单列调光) =======================
 *
 * 一、概念
 *   - 硬件上：区域(r,c) = 行电极 Yr 与 列电极 Xc 的交叉点。
 *     X1~X15 视为 15 列，Y1~Y7 视为 7 行。
 *   - 某个区域的透光率由其所在行、列两根电极之间的“相位差(电平差占比 D)”
 *     决定；D 越大压差越大、越透明；同相(D=0)则不透光。
 *   - 本模块同一时刻只显示“单一图案”：要么第 r 行整行亮，要么第 c 列整列亮，
 *     后调用的会覆盖之前的图案(互斥)，无需分时扫描。
 *
 * 二、函数说明
 *   - app_dimmer_set_row(row, level)：点亮第 row 行。
 *         row   : 1 ~ 7 ，对应 Y1 ~ Y7
 *         level : 0 ~ 1000，对应 0.0% ~ 100.0% 目标透光率
 *                 例：500 = 50% 透光；1000 = 最亮(与各列完全反相)。
 *   - app_dimmer_set_col(col, level)：点亮第 col 列。
 *         col   : 1 ~ 15 ，对应 X1 ~ X15
 *         level : 同上。
 *   - app_dimmer_all_off()            ：整屏关闭(所有电极同相)。
 *   - level=0 等价于整屏关闭。
 *   - 底层由 TIM1(1ms/1kHz)驱动，PWM=1000/DIMMER_PWM_N Hz(N=16 -> 62.5Hz)。
 *     N=16 时透光率按 12.5% 一档量化，因此 level 实际取 125 的倍数即可。
 *
 * 三、调用示例(可在触控任务 / 命令解析中直接调用)
 *     app_dimmer_init();              // 初始化：整屏关闭(复位后调用一次)
 *
 *     // 第 3 行(Y3)整行 50% 透光，其余行关闭
 *     app_dimmer_set_row(3, 500);
 *
 *     // 第 5 列(X5)整列 100% 透光(会覆盖上一条“第3行”的图案)
 *     app_dimmer_set_col(5, 1000);
 *
 *     // 重新点亮第 1 行，25% 透光
 *     app_dimmer_set_row(1, 250);
 *
 *     // 关闭整屏
 *     app_dimmer_all_off();           // 或 app_dimmer_set_row(1, 0);
 *
 * 四、典型应用
 *   - “整行变暗/变亮”        ：触摸或按键给出目标行号 -> app_dimmer_set_row(r, level)
 *   - “整列变暗/变亮”        ：触摸或按键给出目标列号 -> app_dimmer_set_col(c, level)
 *   - 只需显示单条亮带时      ：直接调用上述函数即可，无需额外维护状态。
 * ========================================================================== */

/* ====================== 引脚与通道定义 ====================== */
/* 通道布局：0..14 -> X1..X15(列)，15..21 -> Y1..Y7(行) */
typedef struct {
    GPIO_TypeDef *port;
    uint16_t pin;
} dimmer_pin_t;

static const dimmer_pin_t s_x_pins[DIMMER_COL_NUM] = {
    {X1_GPIO_Port, X1_Pin},
    {X2_GPIO_Port, X2_Pin},
    {X3_GPIO_Port, X3_Pin},
    {X4_GPIO_Port, X4_Pin},
    {X5_GPIO_Port, X5_Pin},
    {X6_GPIO_Port, X6_Pin},
    {X7_GPIO_Port, X7_Pin},
    {X8_GPIO_Port, X8_Pin},
    {X9_GPIO_Port, X9_Pin},
    {X10_GPIO_Port, X10_Pin},
    {X11_GPIO_Port, X11_Pin},
    {X12_GPIO_Port, X12_Pin},
    {X13_GPIO_Port, X13_Pin},
    {X14_GPIO_Port, X14_Pin},
    {X15_GPIO_Port, X15_Pin},
};

static const dimmer_pin_t s_y_pins[DIMMER_ROW_NUM] = {
    {Y1_GPIO_Port, Y1_Pin},
    {Y2_GPIO_Port, Y2_Pin},
    {Y3_GPIO_Port, Y3_Pin},
    {Y4_GPIO_Port, Y4_Pin},
    {Y5_GPIO_Port, Y5_Pin},
    {Y6_GPIO_Port, Y6_Pin},
    {Y7_GPIO_Port, Y7_Pin},
};

/* 相位表(tick)，中断中只读 */
static volatile uint16_t s_phase[DIMMER_CH_NUM];

/* ====================== 内部工具 ====================== */
/**
  * @brief 透光率 level(0~100) -> 相位偏移 delta(tick)。
  *        delta = round( N * (level/100) / 2 )
  */
uint16_t app_dimmer_level_to_delta(uint16_t level)
{
    if (level >= DIMMER_LEVEL_MAX) {
        return (uint16_t)(DIMMER_PWM_N / 2u);
    }
    /* 整数四舍五入：round(N*level/2000) */
    return (uint16_t)(((uint32_t)DIMMER_PWM_N * level + DIMMER_LEVEL_MAX)
                      / (uint32_t)(2u * DIMMER_LEVEL_MAX));
}

/**
  * @brief 两路相位之间的电平差时间占比 D = 2*min(diff,N-diff)/N, 0.0~1.0
  */
float app_dimmer_phase_diff_duty(uint16_t phase_a, uint16_t phase_b)
{
    uint16_t diff;

    diff = (phase_a >= phase_b) ? (uint16_t)(phase_a - phase_b)
                                : (uint16_t)(phase_b - phase_a);
    if (diff > DIMMER_PWM_N / 2u) {
        diff = (uint16_t)(DIMMER_PWM_N - diff);
    }
    return (2.0f * (float)diff) / (float)DIMMER_PWM_N;
}

uint16_t app_dimmer_get_phase(uint8_t ch)
{
    if (ch >= DIMMER_CH_NUM) {
        return 0u;
    }
    return s_phase[ch];
}

/* ====================== 对外接口 ====================== */
void app_dimmer_init(void)
{
    uint8_t i;

    /* 所有电极同相(参考相位 0) -> 任意区域压差 0 -> 整屏关闭 */
    for (i = 0u; i < DIMMER_CH_NUM; i++) {
        s_phase[i] = 0u;
    }
}

void app_dimmer_all_off(void)
{
    /* 关闭中断，保证中断输出的相位表不被撕裂更新 */
    __disable_irq();
    for (uint8_t i = 0u; i < DIMMER_CH_NUM; i++) {
        s_phase[i] = s_phase[0];   /* 全部与 X1 参考同相 */
    }
    __enable_irq();
}

/**
  * @brief 第 row 行(Yrow, 1~7)整行亮。
  *        X 列全部同相(参考)，Yrow 偏移 delta，其余 Y 与 X 同相。
  */
void app_dimmer_set_row(uint8_t row, uint16_t level)
{
    uint16_t ref, delta, yph;
    uint8_t  i;

    if ((row < 1u) || (row > DIMMER_ROW_NUM)) {
        return;
    }
    delta = app_dimmer_level_to_delta(level);
    ref   = s_phase[0];                    /* 以 X1 当前相位为参考 */

    __disable_irq();
    for (i = 0u; i < DIMMER_COL_NUM; i++) {
        s_phase[i] = ref;                  /* 所有列同相 -> 供整行压差 */
    }
    /* 其余行与列同相(无压差) */
    for (i = 0u; i < DIMMER_ROW_NUM; i++) {
        s_phase[DIMMER_COL_NUM + i] = ref;
    }
    /* 活动行相对参考偏移 delta -> 与每一列产生压差 */
    yph = (uint16_t)((ref + delta) % DIMMER_PWM_N);
    s_phase[DIMMER_COL_NUM + (row - 1u)] = yph;
    __enable_irq();
}

/**
  * @brief 第 col 列(Xcol, 1~15)整列亮。
  *        Y 行全部同相(参考)，Xcol 偏移 delta，其余 X 与 Y 同相。
  */
void app_dimmer_set_col(uint8_t col, uint16_t level)
{
    uint16_t ref, delta, xph;
    uint8_t  i;

    if ((col < 1u) || (col > DIMMER_COL_NUM)) {
        return;
    }
    delta = app_dimmer_level_to_delta(level);
    ref   = s_phase[DIMMER_COL_NUM];       /* 以 Y1 当前相位为参考 */

    __disable_irq();
    for (i = 0u; i < DIMMER_COL_NUM; i++) {
        s_phase[i] = ref;                  /* 非活动列与行同相(无压差) */
    }
    for (i = 0u; i < DIMMER_ROW_NUM; i++) {
        s_phase[DIMMER_COL_NUM + i] = ref; /* 所有行同相 -> 供整列压差 */
    }
    /* 活动列相对参考偏移 delta -> 与每一行产生压差 */
    xph = (uint16_t)((ref + delta) % DIMMER_PWM_N);
    s_phase[col - 1u] = xph;
    __enable_irq();
}

/* ====================== 底层输出：定时器中断 ====================== */
/**
  * @brief 定时器更新中断入口。TIM1 每 1ms(1kHz)调用一次：推进 1 个 PWM tick，
  *        并按相位表刷新 22 路 GPIO(50% 占空比)。
  *        PWM频率 = 1000/DIMMER_PWM_N Hz；N=16 -> 62.5Hz。
  * @note  必须在硬件定时器中断上下文调用；中断内不得调用任何 RTOS API。
  */
void app_dimmer_tick_isr(void)
{
    static uint16_t tick = 0u;
    uint8_t ch;
    uint16_t ph, pos;

    /* X 列 1..15 */
    for (ch = 0u; ch < DIMMER_COL_NUM; ch++) {
        ph = s_phase[ch];
        pos = (tick >= ph) ? (uint16_t)(tick - ph)
                           : (uint16_t)(tick + DIMMER_PWM_N - ph);
        HAL_GPIO_WritePin(s_x_pins[ch].port, s_x_pins[ch].pin,
                          (pos < DIMMER_PWM_N / 2u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    /* Y 行 1..7 */
    for (ch = 0u; ch < DIMMER_ROW_NUM; ch++) {
        ph = s_phase[DIMMER_COL_NUM + ch];
        pos = (tick >= ph) ? (uint16_t)(tick - ph)
                           : (uint16_t)(tick + DIMMER_PWM_N - ph);
        HAL_GPIO_WritePin(s_y_pins[ch].port, s_y_pins[ch].pin,
                          (pos < DIMMER_PWM_N / 2u) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    }

    tick++;
    if (tick >= DIMMER_PWM_N) {
        tick = 0u;
    }
}




// 手动重写这个弱函数
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1)  // 判断是否是定时器1
    {
        app_dimmer_tick_isr();   // 调用你的应用函数
    }
}



/* ====================== FreeRTOS 任务(可选) ====================== */
void app_dimmer_task(void *argument)
{
    (void)argument;

    /* 当前"整行/整列即时生效"，无需常驻任务。
       若后续需要分时扫描刷新整幅矩阵 / 渐变动画，
       可在此任务中周期调用 app_dimmer_set_row/col 等。 */
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}