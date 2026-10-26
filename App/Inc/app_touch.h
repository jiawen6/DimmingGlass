#ifndef APP_TOUCH_H
#define APP_TOUCH_H

#include <stdint.h>

void app_touch_enable_switch(void);

void app_touch_enable(uint8_t enable);

uint8_t app_touch_is_enabled(void);

#endif
