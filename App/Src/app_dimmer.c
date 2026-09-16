#include "app_dimmer.h"

#include "main.h"          /* X1~X15 / Y1~Y7 引脚宏 */
#include "FreeRTOS.h"
#include "task.h"
#include "tim.h"          // 为了 htim1
#include <math.h>
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

/**
  * @brief  同时设置所有行与列的透光率。
  * @note   内部实现：以 s_phase[0] 为参考相位，把未设置的通道与参考对齐；
  *         对于 row_levels/col_levels 中大于 0 的元素，按 level->delta 偏移相位。
  */
void app_dimmer_set_matrix(const uint16_t row_levels[DIMMER_ROW_NUM],
                           const uint16_t col_levels[DIMMER_COL_NUM])
{
    uint16_t ref, delta;
    uint8_t i;

    /* 以 X1 当前相位为参考(任意通道均可) */
    ref = s_phase[0];

    __disable_irq();
    /* 先把所有通道与参考对齐(无压差) */
    for (i = 0u; i < DIMMER_COL_NUM; i++) {
        s_phase[i] = ref;
    }
    for (i = 0u; i < DIMMER_ROW_NUM; i++) {
        s_phase[DIMMER_COL_NUM + i] = ref;
    }

    /* 为每一列应用偏移(若提供并且非 0) */
    if (col_levels != NULL) {
        for (i = 0u; i < DIMMER_COL_NUM; i++) {
            if (col_levels[i] != 0u) {
                delta = app_dimmer_level_to_delta(col_levels[i]);
                s_phase[i] = (uint16_t)((ref + delta) % DIMMER_PWM_N);
            }
        }
    }

    /* 为每一行应用偏移(若提供并且非 0) */
    if (row_levels != NULL) {
        for (i = 0u; i < DIMMER_ROW_NUM; i++) {
            if (row_levels[i] != 0u) {
                delta = app_dimmer_level_to_delta(row_levels[i]);
                s_phase[DIMMER_COL_NUM + i] = (uint16_t)((ref + delta) % DIMMER_PWM_N);
            }
        }
    }

    __enable_irq();
}

void app_dimmer_set_rows(const uint16_t row_levels[DIMMER_ROW_NUM])
{
    app_dimmer_set_matrix(row_levels, NULL);
}

void app_dimmer_set_cols(const uint16_t col_levels[DIMMER_COL_NUM])
{
    app_dimmer_set_matrix(NULL, col_levels);
}

/* ========== 统一动画任务: 可切换横/纵波浪等效果 ========== */
static TaskHandle_t s_anim_task = NULL;
static volatile struct {
    app_dimmer_effect_t effect;
    uint16_t peak;
    uint32_t delay_ms;
    uint8_t width;
    uint8_t dir;
    volatile uint8_t running;
    float pos;
    float step; /* 列/行的移动步进 (可为小数以平滑) */
} s_anim_cfg;

static void app_dimmer_anim_task(void *argument)
{
    (void)argument;

    uint16_t cols[DIMMER_COL_NUM];
    uint16_t rows[DIMMER_ROW_NUM];

    /* 初始化位置根据方向和效果 */
    if (s_anim_cfg.effect == APP_DIMMER_EFFECT_HORZ_WAVE) {
        if (s_anim_cfg.dir == 0) s_anim_cfg.pos = -(float)s_anim_cfg.width;
        else s_anim_cfg.pos = (float)(DIMMER_COL_NUM - 1 + s_anim_cfg.width);
    } else if (s_anim_cfg.effect == APP_DIMMER_EFFECT_VERT_WAVE) {
        if (s_anim_cfg.dir == 0) s_anim_cfg.pos = -(float)s_anim_cfg.width;
        else s_anim_cfg.pos = (float)(DIMMER_ROW_NUM - 1 + s_anim_cfg.width);
    }

    while (s_anim_cfg.running) {
        app_dimmer_effect_t effect = s_anim_cfg.effect;

        if (effect == APP_DIMMER_EFFECT_HORZ_WAVE) {
            for (uint8_t i = 0; i < DIMMER_COL_NUM; i++) {
                float d = fabsf((float)i - s_anim_cfg.pos);
                float v = 0.0f;
                if ((int)s_anim_cfg.width <= 0) {
                    v = (float)s_anim_cfg.peak;
                } else if (d <= (float)s_anim_cfg.width) {
                    v = (float)s_anim_cfg.peak * (1.0f - d / (float)s_anim_cfg.width);
                }
                if (v < 0.0f) v = 0.0f;
                if (v > (float)DIMMER_LEVEL_MAX) v = (float)DIMMER_LEVEL_MAX;
                cols[i] = (uint16_t)(v + 0.5f);
            }
            app_dimmer_set_cols(cols);

            /* 更新位置 */
            if (s_anim_cfg.dir == 0) {
                s_anim_cfg.pos += s_anim_cfg.step;
                if (s_anim_cfg.pos > (float)(DIMMER_COL_NUM - 1 + s_anim_cfg.width)) {
                    s_anim_cfg.pos = -((float)s_anim_cfg.width);
                }
            } else {
                s_anim_cfg.pos -= s_anim_cfg.step;
                if (s_anim_cfg.pos < -((float)s_anim_cfg.width)) {
                    s_anim_cfg.pos = (float)(DIMMER_COL_NUM - 1 + s_anim_cfg.width);
                }
            }
        } else if (effect == APP_DIMMER_EFFECT_VERT_WAVE) {
            for (uint8_t i = 0; i < DIMMER_ROW_NUM; i++) {
                float d = fabsf((float)i - s_anim_cfg.pos);
                float v = 0.0f;
                if ((int)s_anim_cfg.width <= 0) {
                    v = (float)s_anim_cfg.peak;
                } else if (d <= (float)s_anim_cfg.width) {
                    v = (float)s_anim_cfg.peak * (1.0f - d / (float)s_anim_cfg.width);
                }
                if (v < 0.0f) v = 0.0f;
                if (v > (float)DIMMER_LEVEL_MAX) v = (float)DIMMER_LEVEL_MAX;
                rows[i] = (uint16_t)(v + 0.5f);
            }
            app_dimmer_set_rows(rows);

            /* 更新位置 */
            if (s_anim_cfg.dir == 0) {
                s_anim_cfg.pos += s_anim_cfg.step;
                if (s_anim_cfg.pos > (float)(DIMMER_ROW_NUM - 1 + s_anim_cfg.width)) {
                    s_anim_cfg.pos = -((float)s_anim_cfg.width);
                }
            } else {
                s_anim_cfg.pos -= s_anim_cfg.step;
                if (s_anim_cfg.pos < -((float)s_anim_cfg.width)) {
                    s_anim_cfg.pos = (float)(DIMMER_ROW_NUM - 1 + s_anim_cfg.width);
                }
            }
        } else {
            /* 未知或 NONE：空转，稍作延时 */
        }

        vTaskDelay(pdMS_TO_TICKS(s_anim_cfg.delay_ms));
    }

    /* 退出：清理句柄并删除自身 */
    s_anim_task = NULL;
    vTaskDelete(NULL);
}

void app_dimmer_anim_start(app_dimmer_effect_t effect, uint16_t peak_level, uint32_t step_ms, uint8_t width, uint8_t direction)
{
    if (peak_level > DIMMER_LEVEL_MAX) peak_level = DIMMER_LEVEL_MAX;
    if (width == 0u) width = 1u;
    if (step_ms == 0u) step_ms = 50u;

    /* 若任务已存在，仅更新参数并返回（实现热切换） */
    if (s_anim_task != NULL) {
        s_anim_cfg.effect = effect;
        s_anim_cfg.peak = peak_level;
        s_anim_cfg.delay_ms = step_ms;
        s_anim_cfg.width = width;
        s_anim_cfg.dir = (direction == 0u) ? 0u : 1u;
        s_anim_cfg.step = 0.5f;
        return;
    }

    s_anim_cfg.effect = effect;
    s_anim_cfg.peak = peak_level;
    s_anim_cfg.delay_ms = step_ms;
    s_anim_cfg.width = width;
    s_anim_cfg.dir = (direction == 0u) ? 0u : 1u;
    s_anim_cfg.running = 1u;
    s_anim_cfg.step = 0.5f;

    xTaskCreate(app_dimmer_anim_task, "dmm_anim", 384, NULL, tskIDLE_PRIORITY + 2, &s_anim_task);
}

void app_dimmer_anim_change(app_dimmer_effect_t effect, uint16_t peak_level, uint32_t step_ms, uint8_t width, uint8_t direction)
{
    /* 简单封装：如果任务不存在，则启动；存在则更新配置 */
    if (s_anim_task == NULL) {
        app_dimmer_anim_start(effect, peak_level, step_ms, width, direction);
        return;
    }
    if (peak_level > DIMMER_LEVEL_MAX) peak_level = DIMMER_LEVEL_MAX;
    if (width == 0u) width = 1u;
    if (step_ms == 0u) step_ms = 50u;

    s_anim_cfg.effect = effect;
    s_anim_cfg.peak = peak_level;
    s_anim_cfg.delay_ms = step_ms;
    s_anim_cfg.width = width;
    s_anim_cfg.dir = (direction == 0u) ? 0u : 1u;
}

void app_dimmer_anim_stop(void)
{
    if (s_anim_task == NULL) return;
    s_anim_cfg.running = 0u;
    for (int i = 0; i < 50 && s_anim_task != NULL; i++) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/* 向后兼容：以前的波浪接口调用统一接口（横向） */
void app_dimmer_wave_start(uint16_t peak_level, uint32_t step_ms, uint8_t width, uint8_t direction)
{
    app_dimmer_anim_start(APP_DIMMER_EFFECT_HORZ_WAVE, peak_level, step_ms, width, direction);
}

void app_dimmer_wave_stop(void)
{
    app_dimmer_anim_stop();
}

/*
接口说明（简要）:

app_dimmer_set_matrix(const uint16_t row_levels[DIMMER_ROW_NUM], const uint16_t col_levels[DIMMER_COL_NUM])
同时为所有行与列设置透光率（单位与现有接口一致，0..100）。
若参数为 NULL，视为全部通道为 0（关闭）。
函数把未设置的通道与参考相位对齐（无压差），对提供的每个非 0 level 计算相位偏移并应用。
行与列同时存在时，单个区域 (r,c) 的实际透光率由 phase[Xc] 与 phase[Yr] 的相位差决定。
便捷包装：
app_dimmer_set_rows(const uint16_t row_levels[DIMMER_ROW_NUM])
app_dimmer_set_cols(const uint16_t col_levels[DIMMER_COL_NUM])
使用示例:

同时设置第 2、4 行为 50% 与 75%，第 3、5 列为 100% 与 25%：
创建数组（示例）：
uint16_t rows[DIMMER_ROW_NUM] = {0, 50, 0, 75, 0, 0, 0}; // Y1..Y7
uint16_t cols[DIMMER_COL_NUM] = {0,0,100,0,25,0,0,0,0,0,0,0,0,0,0}; // X1..X15
调用：
app_dimmer_set_matrix(rows, cols);



示例用法：

启动横向波浪（左->右，峰值 100，40ms，半宽 4）：
app_dimmer_anim_start(APP_DIMMER_EFFECT_HORZ_WAVE, 100, 40, 4, 0);
切换为纵向波浪（顶->底，峰值 80，60ms，半宽 2）：
app_dimmer_anim_change(APP_DIMMER_EFFECT_VERT_WAVE, 80, 60, 2, 0);
停止动画：
app_dimmer_anim_stop();


*/
