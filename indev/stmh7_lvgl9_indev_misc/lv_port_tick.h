#ifndef __LV_PORT_TICK_H__
#define __LV_PORT_TICK_H__
#include "lvgl.h"
#include "main.h"
#include "tim.h"
#include "stm32h7xx_hal_tim.h"

#define LV_PORT_TICK_USE_RTOS

#ifndef LV_PORT_TICK_USE_RTOS
#if (USE_HAL_TIM_REGISTER_CALLBACKS == 0)
#error "lv port tick need USE_HAL_TIM_REGISTER_CALLBACKS = 1 , goto cubemx project manager right side to open it"
#endif

#define LV_TICK_INC_MS 1

#else

#define LV_TICK_INC_MS 1

#endif

void lv_port_tick_init();

#endif
