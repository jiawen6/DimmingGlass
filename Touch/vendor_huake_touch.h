#ifndef VENDOR_HUAKE_TOUCH_H
#define VENDOR_HUAKE_TOUCH_H

#define TOUCH_MEMBRANE_H    500
#define TOUCH_MEMBRANE_L    1000

#define Slide_Length    10    


typedef struct {
    uint16_t x;
    uint16_t y;
    uint16_t x_last;
    uint16_t y_last;
    uint8_t  status;
    uint8_t  status_last;
} touch_point_t;


#endif
