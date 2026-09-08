#include "touch_parser.h"

#ifdef SUPPORT_VENDOR_NUOFEI
#include "vendor_nuofei_touch.h"
#endif

#ifdef SUPPORT_VENDOR_HUAKE
#include "vendor_huake_touch.h"
#endif

extern volatile Touch_Single_t single_touch;
extern volatile Touch_Double_t Double_touch;

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


touch_point_t point_1;
touch_point_t point_2;

static bool verify_checksum(uint8_t *buf, uint16_t len);
void process_touch_status(touch_point_t* point);
static bool verify_checksum(uint8_t *buf, uint16_t len);



void touch_parser_init(void)
{
    state = STATE_IDLE;
    frame_index = 0;
    expected_len = 0;
    vendor = 0;
}

void touch_parser_feed(uint8_t byte)
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
                point->slide_dir = TOUCH_RIGHT;
            }
            else if(dx <= (-1.0f)*Slide_Length)
            {
                point->status_last = point->status;
                point->status = TOUCH_SLIDING;
                point->slide_dir = TOUCH_RIGHT;                
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
                point->slide_dir = TOUCH_RIGHT;
            }
            else if(dx <= (-1.0f)*Slide_Length)
            {
                point->status_last = point->status;
                point->slide_dir = TOUCH_RIGHT;                
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

