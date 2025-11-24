#ifndef __LV_PORT_ENCODER_H__
#define __LV_PORT_ENCODER_H__

#include "lvgl.h"

#include "gpio.h"
#include "stm32h7xx_hal_gpio.h"

#include "tim.h"
#include "stm32h7xx_hal_tim.h"

extern lv_indev_t *indev_encoder[2];

void lv_port_encoder_init(void);

#endif
