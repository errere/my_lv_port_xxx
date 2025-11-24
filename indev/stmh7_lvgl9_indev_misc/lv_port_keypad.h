#ifndef __LV_PORT_KEYPAD_H__
#define __LV_PORT_KEYPAD_H__

#include "lvgl.h"
#include "main.h"
#include "stm32h7xx_hal_gpio.h"
#include "gpio.h"

extern lv_indev_t *indev_keypad;

void lv_port_keypad_init(void);

#endif
