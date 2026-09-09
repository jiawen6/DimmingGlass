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


uint16_t Get_Region(touch_point_t point)
{
    uint8_t region_x = point.x % (957.0f/15.0f) + 1;
    if(region_x > 15) region_x = 15;

    uint8_t region_y = point.y % (448.0f/7.0f) + 1;
    if(region_y > 15) region_y = 7;

    return region_x << 8 + region_y;
}

uint8_t Get_Region_x(touch_point_t point)
{
    uint8_t region_x = point.x % (957.0f/15.0f) + 1;
    if(region_x > 15) region_x = 15;

    return region_x;
}

uint8_t Get_Region_y(touch_point_t point)
{
    uint8_t region_y = point.y % (448.0f/7.0f) + 1;
    if(region_y > 15) region_y = 7;

    return region_y;
}
