#include "lv_port_tick.h"

#ifndef LV_PORT_TICK_USE_RTOS
static TIM_HandleTypeDef *htim = &htim15;

static void lv_tim_isr(TIM_HandleTypeDef *htim)
{
    lv_tick_inc(LV_TICK_INC_MS);
}

void lv_port_tick_init()
{
    // lvgl tick
    __HAL_TIM_SET_COUNTER(htim, 0);
    __HAL_TIM_CLEAR_IT(htim, TIM_IT_UPDATE);
    HAL_TIM_RegisterCallback(htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, lv_tim_isr);
    HAL_TIM_Base_Start_IT(htim);
}
#else

#include "FreeRTOS.h"
#include "timers.h"
// #include "elog.h"

void tim_isr(TimerHandle_t xTimer)
{
    lv_tick_inc(LV_TICK_INC_MS);
    // log_d("lv_t");
}

void lv_port_tick_init()
{
    // lvgl tick

    static TimerHandle_t xTimer;
    xTimer = xTimerCreate("lv", LV_TICK_INC_MS, pdTRUE, NULL, tim_isr);
    xTimerStart(xTimer, portMAX_DELAY);
}

#endif
