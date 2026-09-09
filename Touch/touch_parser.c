#include "touch_parser.h"

#ifdef SUPPORT_VENDOR_NUOFEI
#include "vendor_nuofei_touch.h"
#endif

#ifdef SUPPORT_VENDOR_HUAKE
#include "vendor_huake_touch.h"
#endif

extern volatile Touch_Single_t single_touch;
extern volatile Touch_Double_t double_touch;

typedef enum {
    STATE_IDLE = 0,
    STATE_HEADER_A1,
    STATE_HEADER_A2,
    STATE_HEADER_A3,
    STATE_HEADER_B1,
    STATE_RECV_A,
    STATE_RECV_B
} parser_state_t;



static uint8_t frame_buf[TOUCH_PARSER_MAX_FRAME_LEN];
static uint16_t frame_index = 0;
static uint16_t expected_len = 0;
static parser_state_t state = STATE_IDLE;
static uint8_t vendor = 0;

/* 触控超时自动复位：内部计时(ms) */
static uint32_t s_last_activity_ms = 0u;   /* 最近一次有效触控帧时刻 */
static uint32_t s_rel_ms_single = 0u;      /* 单点进入 RELEASED 的时刻 */
static uint32_t s_rel_ms_double1 = 0u;     /* 双点1 进入 RELEASED 的时刻 */
static uint32_t s_rel_ms_double2 = 0u;     /* 双点2 进入 RELEASED 的时刻 */


touch_point_t point_1;
touch_point_t point_2;

static bool verify_checksum(uint8_t *buf, uint16_t len);
void process_touch_status(touch_point_t* point);



void touch_parser_init(void)
{
    state = STATE_IDLE;
    frame_index = 0;
    expected_len = 0;
    vendor = 0;
    s_last_activity_ms = 0u;
    s_rel_ms_single = 0u;
    s_rel_ms_double1 = 0u;
    s_rel_ms_double2 = 0u;
}

uint8_t touch_parser_feed(uint8_t byte)
{
    switch (state) {
    case STATE_IDLE:
        if (byte == 0x55) {
            frame_buf[0] = byte;
            frame_index = 1;
            state = STATE_HEADER_A1;
        } else if (byte == 0x1F) {
            frame_buf[0] = byte;
            frame_index = 1;
            state = STATE_HEADER_B1;
        }
        break;

    case STATE_HEADER_A1:
        if (byte == 0xAA) {
            frame_buf[frame_index++] = byte;
            state = STATE_HEADER_A2;
        } else {
            state = STATE_IDLE;
            frame_index = 0;
            touch_parser_feed(byte);  // 重新处理当前字节
        }
        break;

    case STATE_HEADER_A2:
        if (byte == 0x80) {
            frame_buf[frame_index++] = byte;
            state = STATE_HEADER_A3;
        } else {
            state = STATE_IDLE;
            frame_index = 0;
            touch_parser_feed(byte);
        }
        break;

    case STATE_HEADER_A3:
        if (byte == 0x07){
            frame_buf[frame_index++] = byte;
            vendor = 1;
            state = STATE_RECV_A;
            expected_len = 12;
        } else if(byte == 0x0D){
            frame_buf[frame_index++] = byte;
            vendor = 1;
            state = STATE_RECV_A;
            expected_len = 18;
        } else{            
            state = STATE_IDLE;
            frame_index = 0;
            touch_parser_feed(byte);
        }
        break;

    case STATE_HEADER_B1:
        if (byte == 0xF7) {
            frame_buf[frame_index++] = byte;
            vendor = 2;
            state = STATE_RECV_B;
            expected_len = 0;
        } else {
            state = STATE_IDLE;
            frame_index = 0;
            touch_parser_feed(byte);
        }
        break;

    case STATE_RECV_A:
        frame_buf[frame_index++] = byte;
        // if (frame_index == 4) {
        //     uint8_t data_len = frame_buf[3];
        //     expected_len = 3 + 1 + data_len + 1;
        //     if (expected_len > TOUCH_PARSER_MAX_FRAME_LEN) {
        //         state = STATE_IDLE;
        //         frame_index = 0;
        //     }
        // }
        if (frame_index == expected_len) {
            if (verify_checksum(frame_buf, expected_len)) {
                #if SUPPORT_VENDOR_NUOFEI
                    touch_parser_on_frame_nuofei(frame_buf, expected_len);
                    if(expected_len == 12)
                    {
                        process_touch_status(&single_touch.point);
                        return 1;
                    }
                    else if(expected_len == 18)
                    {
                        process_touch_status(&double_touch.point_1);
                        process_touch_status(&double_touch.point_2);
                        return 2;
                    }
                #endif
            }
            state = STATE_IDLE;
            frame_index = 0;
        }
        break;

    case STATE_RECV_B:
        frame_buf[frame_index++] = byte;
        if (frame_index == 4) {
            uint16_t data_len = frame_buf[2] | (frame_buf[3] << 8);
            expected_len = 2 + 2 + data_len + 1;
            if (expected_len > TOUCH_PARSER_MAX_FRAME_LEN) {
                state = STATE_IDLE;
                frame_index = 0;
            }
        }
        if (frame_index == expected_len) {
            if (verify_checksum(frame_buf, expected_len)) {
                #if SUPPORT_VENDOR_HUAKE
                touch_parser_on_frame_B(frame_buf, expected_len);
                #endif
            }
            state = STATE_IDLE;
            frame_index = 0;
        }
        break;

    default:
        state = STATE_IDLE;
        frame_index = 0;
        break;
    }
    return 0;
}

uint16_t Get_Touch_Gestrue(uint8_t touch_num)
{
    if(touch_num == 1)
    {
        if(single_touch.point.status == TOUCH_RELEASED 
            && single_touch.point.status_last == TOUCH_PRESSED
            && single_touch.point.slide_dir == SLIDE_IDLE)
        {
            //从按下到抬起
            uint8_t region = Get_Region(single_touch.point);
            return (region << 8) + 0x0000;
        }
        else if(single_touch.point.status == TOUCH_SLIDING)
        {
            if(single_touch.point.slide_dir != SLIDE_IDLE)
            {
                uint8_t region = Get_Region(single_touch.point);
                single_touch.point.x_start = single_touch.point.x;
                single_touch.point.y_start = single_touch.point.y;

                switch(single_touch.point.slide_dir)
                {
                    case SLIDE_UP:
                        single_touch.point.slide_dir = SLIDE_IDLE;
                        return (region << 8) + 0x0001;
                        break;
                    case SLIDE_DOWN:
                        single_touch.point.slide_dir = SLIDE_IDLE;
                        return (region << 8) + 0x0002;
                        break;                        
                    case SLIDE_LEFT:
                        single_touch.point.slide_dir = SLIDE_IDLE;
                        return (region << 8) + 0x0001;
                        break;
                    case SLIDE_RIGHT:
                        single_touch.point.slide_dir = SLIDE_IDLE;
                        return (region << 8) + 0x0001;
                        break;
                    default:
                        break;
                }
            }
        }
        return 0;
    }
    else if(touch_num == 2)
    {
        if(double_touch.point_1.status == TOUCH_SLIDING 
            && double_touch.point_2.status == TOUCH_SLIDING)
        {
            if(double_touch.point_1.slide_dir == SLIDE_UP 
                && double_touch.point_2.slide_dir == SLIDE_UP)
            {
                double_touch.point_1.slide_dir = SLIDE_IDLE;
                double_touch.point_2.slide_dir = SLIDE_IDLE;
                return 0xAA11;
            }
            else if(double_touch.point_1.slide_dir == SLIDE_DOWN 
                && double_touch.point_2.slide_dir == SLIDE_DOWN)
            {
                double_touch.point_1.slide_dir = SLIDE_IDLE;
                double_touch.point_2.slide_dir = SLIDE_IDLE;
                return 0xAA22;
            }
            else if(double_touch.point_1.slide_dir == SLIDE_LEFT 
                && double_touch.point_2.slide_dir == SLIDE_LEFT)
            {
                double_touch.point_1.slide_dir = SLIDE_IDLE;
                double_touch.point_2.slide_dir = SLIDE_IDLE;
                return 0xAA33;
            }
            else if(double_touch.point_1.slide_dir == SLIDE_RIGHT 
                && double_touch.point_2.slide_dir == SLIDE_RIGHT)
            {
                double_touch.point_1.slide_dir = SLIDE_IDLE;
                double_touch.point_2.slide_dir = SLIDE_IDLE;
                return 0xAA44;
            }                        
        }
        return 0;
    }
}



static bool verify_checksum(uint8_t *buf, uint16_t len)
{
    if (len < 2) return false;
    uint8_t sum = 0;
    for (uint16_t i = 0; i < len - 1; i++) {
        sum += buf[i];
    }
    return (sum == buf[len - 1]);
}

void process_touch_status(touch_point_t* point)
{
    if(point == NULL)
    {
        return;
    }

    switch(point->status)
    {
        float dx;
        float dy;

        case TOUCH_IDLE:
            point->x_start = point->x;
            point->y_start = point->y;
            point->status_last = point->status;
            point->status = TOUCH_PRESSED;
            break;

        case TOUCH_PRESSED:
            dx = point->x - point->x_start;
            dy = point->y - point->y_start;

            if(dx >= Slide_Length)
            {
                point->status_last = point->status;
                point->status = TOUCH_SLIDING;
                point->slide_dir = SLIDE_RIGHT;
            }
            else if(dx <= (-1.0f)*Slide_Length)
            {
                point->status_last = point->status;
                point->status = TOUCH_SLIDING;
                point->slide_dir = SLIDE_LEFT;                
            }
            else if(dy >= Slide_Length)
            {

            }
            else if(dy <= (-1.0f)*Slide_Length)
            {

            }

            if(point->slide_dir != SLIDE_IDLE)
            {
                point->x_start = point->x;
                point->y_start = point->y;                
            }
            break;

        case TOUCH_SLIDING:
            dx = point->x - point->x_start;
            dy = point->y - point->y_start;

            if(dx >= Slide_Length)
            {
                point->status_last = point->status;
                point->slide_dir = SLIDE_RIGHT;
            }
            else if(dx <= (-1.0f)*Slide_Length)
            {
                point->status_last = point->status;
                point->slide_dir = SLIDE_LEFT;                
            }
            else if(dy >= Slide_Length)
            {

            }
            else if(dy <= (-1.0f)*Slide_Length)
            {

            }

            if(point->slide_dir != SLIDE_IDLE)
            {
                point->x_start = point->x;
                point->y_start = point->y;                
            }
            break;

        case TOUCH_RELEASED:
            break;

        default:
            break;

    }

}

/* ====================== 触控超时自动复位 ====================== */
/**
  * @brief 对单个触点执行一次超时推进。
  * @param  pt     目标触点(single / double 的某个 point)
  * @param  rel_ms 该触点进入 RELEASED 的时刻(出参记录)
  * @param  now_ms 当前时间(ms)
  * @retval 1 = 本次从按压/滑动补成“抬起”；0 = 无此动作
  */
static uint8_t touch_timeout_step(touch_point_t *pt, uint32_t *rel_ms, uint32_t now_ms)
{
    uint8_t lifted = 0u;

    if ((pt == NULL) || (rel_ms == NULL)) {
        return 0u;
    }

    switch (pt->status)
    {
    case TOUCH_PRESSED:
    case TOUCH_SLIDING:
        /* 长时间无新帧：认为手指已离开，转为“释放” */
        pt->status_last = pt->status;
        pt->status      = TOUCH_RELEASED;
        *rel_ms         = now_ms;
        lifted          = 1u;
        break;

    case TOUCH_RELEASED:
        /* “释放”已持续一个超时周期仍无新帧：回到初始态，等待下次按下 */
        if ((now_ms - *rel_ms) >= TOUCH_STATE_TIMEOUT_MS)
        {
            pt->status_last = pt->status;
            pt->status      = TOUCH_IDLE;
            pt->slide_dir   = SLIDE_IDLE;
            pt->x_start     = pt->x;
            pt->y_start     = pt->y;
        }
        break;

    case TOUCH_IDLE:          /* 空闲无需处理 */
    default:
        break;
    }

    return lifted;
}

/**
  * @brief 刷新“最近一次有效触控帧”时刻。每收到一帧有效触控数据后调用。
  * @param  now_ms 当前时间(ms)，建议 HAL_GetTick()
  */
void touch_parser_mark_activity(uint32_t now_ms)
{
    s_last_activity_ms = now_ms;
}

/**
  * @brief 周期超时检测（建议任务里每 1ms 调用一次）。
  *        距最近有效帧超过 TOUCH_STATE_TIMEOUT_MS 时，统一对
  *        single_touch / double_touch 触点做状态推进。
  * @param  now_ms 当前时间(ms)
  * @retval 1 = 本次有触点被补成“抬起”，调用方可再查一次手势；
  * @retval 0 = 无事件
  */
uint8_t touch_parser_timeout_check(uint32_t now_ms)
{
    uint8_t lifted;

    /* 仍在持续收到新帧(手指未离开)则不动作 */
    if ((now_ms - s_last_activity_ms) < TOUCH_STATE_TIMEOUT_MS) {
        return 0u;
    }

    lifted  = touch_timeout_step(&single_touch.point, &s_rel_ms_single, now_ms);
    lifted |= touch_timeout_step(&double_touch.point_1, &s_rel_ms_double1, now_ms);
    lifted |= touch_timeout_step(&double_touch.point_2, &s_rel_ms_double2, now_ms);
    return lifted;
}

