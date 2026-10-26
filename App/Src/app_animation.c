#include "app_animation.h"
#include "FreeRTOS.h"
#include "task.h"
#include "cmsis_os2.h"
#include "app_dimmer.h"
#include "app_touch.h"

extern osThreadId_t AnimationHandle;



/*动画参数*/
#define ANIM_FRAME_100MS    100u
#define ANIM_FRAME_200MS    200u
#define ANIM_FRAME_300MS    300u
#define ANIM_LEVEL          100u
#define ANIM_GEAR_NUM       7u
#define ANIM_GAP_MS         1000u
#define ANIM_HOLD_MS        3000u

#define ANIM_ARR_MAX        (DIMMER_COL_NUM > DIMMER_ROW_NUM ? DIMMER_COL_NUM : DIMMER_ROW_NUM)
#define ANIM_FRED_NONE      0XFFu

static volatile uint8_t s_anim_running = 0u;
static volatile uint8_t s_restart_req = 0u;


typedef void (*anim_effect_fn)(void);
typedef void (*anim_render_fn)(const uint16_t *gears);

static void anim_wave_on_l2r(void);
static void anim_wave_off_r2l(void);
static void anim_wave_on_b2t(void);
static void anim_wave_off_t2b(void);
static void anim_on_center_outward(void);
static void anim_off_center_outward(void);
static void anim_on_center_outward_row(void);
static void anim_off_center_outward_row(void);
static void anim_row_col_alternate(void);
static void anim_row_col_expansion(void);
static void anim_row_col_contraction(void);

static void anim_bright(void);
static void anim_dark(void);
static void anim_all_dark(void);

static const anim_effect_fn a_anim_sequence[] = {
    anim_wave_on_l2r,
    anim_wave_off_r2l,
    anim_wave_on_b2t,
    anim_wave_off_t2b,
    anim_on_center_outward,
    anim_off_center_outward,
    anim_on_center_outward_row,
    anim_off_center_outward_row,
    anim_row_col_alternate,
    anim_row_col_contraction,
    anim_row_col_expansion,
    anim_bright,
    anim_dark,
};

#define ANIM_SEQ_NUM    (sizeof(s_anim_sequence)/sizeof(s_anim_sequence[0]))


void app_animation_task(void *argument)
{
    uint8_t seq_idx = 0u;
    (void)argument;

    anim_all_dark();

    for(;;){
        if(s_restart_req)
        {
            s_restart_req = 0u;
            seq_idx = 0u;
            anim_all_dark();
        }
        a_anim_sequence[seq_idx]();
        seq_idx = (uint8_t)((seq_idx + 1u) % ANIM_SEQ_NUM);
    }
}

void app_animation_start(void)
{
    if(s_anim_running){
        return;
    }
    s_anim_running = 1u;
    app_touch_enable(0u);
    s_restart_req = 1u;
    (void)osThreadResume(AnimationHandle);
}

void app_animation_stop(void)
{
    if(!s_anim_running){
        return;
    }
    s_anim_running = 0u;
    (void)osThreadSuspend(AnimationHandle);
    anim_all_dark();
    app_touch_enable(1u);
}

uint8_t app_animation_is_running(void)
{
    return s_anim_running;
}


static void anim_frame_delay(uint16_t delay_ms)
{
    vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

static void anim_all_dark(void)
{
    app_dimmer_all_off();
}

static uint16_t anim_gear_level(uint16_t gear)
{
    return (uint16_t)((uint32_t)gear * ANIM_LEVEL / ANIM_GEAR_NUM);
}


static void anim_render_cols(const uint16_t *gears)
{
    uint16_t rows[DIMMER_ROW_NUM] = {0};
    uint16_t cols[DIMMER_COL_NUM];
    uint8_t c;
    for(c = 0; c < DIMMER_COL_NUM; c++)
    {
        cols[c] = anim_gear_level(gears[c]);
    }
    app_dimmer_set_matrix(rows, cols);
}

static void anim_render_rows(const uint16_t *gears)
{
    uint16_t rows[DIMMER_ROW_NUM];
    uint16_t cols[DIMMER_COL_NUM] = {0};
    uint8_t r;
    for(r = 0; r < DIMMER_ROW_NUM; r++)
    {
        rows[r] = anim_gear_level(gears[r]);
    }
    app_dimmer_set_matrix(rows, cols);
}

static void anim_pred_head_first(uint8_t *pred, uint8_t num)
{
    uint8_t  i;
    for(i = 0; k < num; i++)
    {
        pred[i] = (uint8_r)((i == 0u) ? ANIM_PRED_NONE : i - 1u);
    }
}

static void anim_pred_tail_first(uint8_t *pred, uint8_t num)
{
    uint8_t last = num - 1u;
    uint8_t  i;

    for(i = 0; i < num; i++)
    {
        pred[i] = (uint8_t)((i == last) ? ANIM_PRED_NONE : i + 1u);
    }
}

static void anim_pred_center_out(uint8_t * pred, uint8_t num)
{
    uint8_t mid = num / 2u;
    uint8_t i;

    for(i = 0; i < num; i++)
    {
        if(i == mid){
            pred[i] = ANIM_PRED_NONE;
        }else{
            pred[i] = (uint8_t)((i < mid) ? (i + 1u) : (i - 1u));
        }
    }
}

static void anim_pred_center_in(uint8_t *pred, uint8_t num)
{
    uint8_t last = num - 1u;
    uint8_t mid = num / 2u;
    uint8_t  i;
    for(i = 0; i < num; i++)
    {
        if((i == 0u) || (i == last)){
            pred[i] = ANIM_PRED_NONE;
        }else {
            pred[i] = (uint8_t)((i <= mid) ? (i - 1u) : (i + 1u));
        }
    }
}


static void anim_gradient(uint16_t *gears, const uint8_t *pred, uint8_t num,
                            uint8_t rising, anim_render, uint16_t delay_ms)
{
    for(;;)
    {
        uint16_t start[ANIM_ARR_MAX];
        uint8_t any_active = 0;
        uint8_t  i;

        for(i = 0; i < num; i++)
        {
            start[i] = gears[i];
        }

        for(i = 0; i < num; i++)
        {
            uint8_t pred_started = (pred[i] == ANIM_PRED_NONE) ? 1u : 0u;

            if(0u == pred_started){
                if(rising)
                {
                    pred_started = (start[pred[i]] >= 1u);
                }
                else {
                    pred_started = (start[pred[i]] <= (ANIM_GEAR_NUM -1u));
                }
            }

            if(rising){
                if((gears[i] < ANIM_GEAR_NUM) && pred_started){
                    gears[i]++;
                    any_active = 1u;
                }
            }else {
                if((gears[i] > 0u) && pred_started) {
                    gears[i]--;
                    any_active = 1u;
                }
            }
        }   
        
        render(gears);
        if(0u == any_active){
            break;
        }
        anim_frame_delay(delay_ms);
    }
}


static void anim_wave_on_l2r(void)
{
    uint16_t gears[DIMMER_COL_NUM] = {0};
    uint8_t pred[DIMMER_COL_NUM];

    anim_pred_head_first(pred, DIMMER_COL_NUM);
    anim_render_cols(gears);
    anim_gradient(gears, pred, DIMMER_COL_NUM, 1u, anim_render_cols, ANIM_FRAME_100MS);

    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}

static void anim_wave_off_r2l(void)
{
    uint16_t gears[DIMMER_COL_NUM];
    uint8_t pred[DIMMER_COL_NUM];
    uint8_t c;

    for(c = 0; c < DIMMER_COL_NUM; c++) { gears[c] = ANIM_GEAR_NUM; }
    anim_pred_tail_first(pred, DIMMER_COL_NUM);
    anim_render_cols(gears);
    anim_gradient(gears, pred, DIMMER_COL_NUM, 0u, anim_render_cols, ANIM_FRAME_100MS);
    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}

static void anim_wave_on_b2t(void)
{
    uint16_t gears[DIMMER_ROW_NUM] = {0};
    uint8_t pred[DIMMER_ROW_NUM];

    anim_pred_head_first(pred, DIMMER_ROW_NUM);
    anim_render_rows(gears);
    anim_gradient(gears, pred, DIMMER_ROW_NUM, 1u, anim_render_rows, ANIM_FRAME_200MS);

    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}

static void anim_wave_off_t2b(void)
{
    uint16_t gears[DIMMER_ROW_NUM];
    uint8_t pred[DIMMER_ROW_NUM];
    uint8_t c;

    for(c = 0; c < DIMMER_ROW_NUM; c++) { gears[c] = ANIM_GEAR_NUM; }
    anim_pred_tail_first(pred, DIMMER_ROW_NUM);
    anim_render_rows(gears);
    anim_gradient(gears, pred, DIMMER_ROW_NUM, 0u, anim_render_cols, ANIM_FRAME_200MS);
    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}


static void anim_wave_on_center_outward(void)
{
    uint16_t gears[DIMMER_COL_NUM] = {0};
    uint8_t pred[DIMMER_COL_NUM];

    anim_pred_center_out(pred, DIMMER_COL_NUM);
    anim_render_cols(gears);
    anim_gradient(gears, pred, DIMMER_COL_NUM, 1u, anim_render_cols, ANIM_FRAME_100MS);

    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}

static void anim_off_center_outward(void)
{
    uint16_t gears[DIMMER_COL_NUM];
    uint8_t pred[DIMMER_COL_NUM];
    uint8_t c;

    for(c = 0; c < DIMMER_COL_NUM; c++) { gears[c] = ANIM_GEAR_NUM; }
    anim_pred_center_in(pred, DIMMER_COL_NUM);
    anim_render_cols(gears);
    anim_gradient(gears, pred, DIMMER_COL_NUM, 0u, anim_render_cols, ANIM_FRAME_100MS);
    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}


static void anim_wave_on_center_outward_row(void)
{
    uint16_t gears[DIMMER_ROW_NUM] = {0};
    uint8_t pred[DIMMER_ROW_NUM];

    anim_pred_center_out(pred, DIMMER_ROW_NUM);
    anim_render_rows(gears);
    anim_gradient(gears, pred, DIMMER_ROW_NUM, 1u, anim_render_rows, ANIM_FRAME_200MS);

    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}

static void anim_off_center_outward_row(void)
{
    uint16_t gears[DIMMER_ROW_NUM];
    uint8_t pred[DIMMER_ROW_NUM];
    uint8_t r;

    for(r = 0; r < DIMMER_ROW_NUM; c++) { gears[r] = ANIM_GEAR_NUM; }
    anim_pred_center_in(pred, DIMMER_ROW_NUM);
    anim_render_rows(gears);
    anim_gradient(gears, pred, DIMMER_ROW_NUM, 0u, anim_render_rows, ANIM_FRAME_200MS);
    vTaskDelay(pdMS_TO_TICKS(ANIM_GAP_MS));
}









