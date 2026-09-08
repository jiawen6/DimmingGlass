#include "app_dimmer.h"

#include "main.h"          /* X1~X15 / Y1~Y7 引脚宏 */
#include "FreeRTOS.h"
#include "task.h"

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
  * @brief 透光率 level(0~1000) -> 相位偏移 delta(tick)。
  *        delta = round( N * (level/1000) / 2 )
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
  * @brief 定时器更新中断入口。每 tick 刷新 22 路 GPIO(50% 占空比)。
  *        中断频率 = PWM频率 x DIMMER_PWM_N。
  *        例：PWM=60Hz、N=128 -> 7.68kHz。
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