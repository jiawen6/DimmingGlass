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
static volatile uint8_t s_restart = 0u;


typedef void (*anim_effect_fn)(void);
typedef void (*anim_render_fn)(const uint16_t *gears);

static void anim_wave_on_l2r(void);

