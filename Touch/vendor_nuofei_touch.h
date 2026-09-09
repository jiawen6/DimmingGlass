#ifndef VENDOR_HUAKE_TOUCH_H
#define VENDOR_HUAKE_TOUCH_H

#include <stdint.h>

/*
触控膜长957mm，高448mm
*/
#define TOUCH_MEMBRANE_Height   448
#define TOUCH_MEMBRANE_width    957

#define Slide_Length    10    

typedef enum
{
  TOUCH_IDLE,
  TOUCH_PRESSED,
  TOUCH_RELEASED,
  TOUCH_SLIDING,
  TOUCH_RECOVERY
} TouchState;

typedef enum
{
  SLIDE_IDLE,
  SLIDE_UP,
  SLIDE_DOWN,
  TOUCH_LEFT,
  TOUCH_RIGHT
} SlideDirection;

typedef struct {
    float x;
    float y;
    float x_start;
    float y_start;
    uint8_t  status;
    uint8_t  status_last;
    uint8_t  id;
    SlideDirection slide_dir;
} touch_point_t;

typedef struct
{
    touch_point_t point;
} Touch_Single_t;

typedef struct
{
    touch_point_t point_1;
    touch_point_t point_2;
} Touch_Double_t;

void touch_parser_on_frame_nuofei(uint8_t *frame, uint16_t len);

#endif


