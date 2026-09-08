#include "vendor_nuofei_touch.h"


volatile Touch_Single_t single_touch;
volatile Touch_Double_t Double_touch;


void touch_parser_on_frame_nuofei(uint8_t *frame, uint16_t len) 
{
    if(len == 12)
    {
        uint16_t touch_x = (frame[7] << 8) | frame[6];
        uint16_t touch_y = (frame[9] << 8) | frame[8];
        single_touch.point.x = (float)touch_x * TOUCH_MEMBRANE_width / 32767;
        single_touch.point.y = (float)touch_y * TOUCH_MEMBRANE_Height / 32767;
        single_touch.point.id = frame[5];
    }

}


