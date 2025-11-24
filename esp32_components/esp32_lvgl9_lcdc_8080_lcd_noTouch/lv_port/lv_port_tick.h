#ifndef __LV_PORT_TICK_H
#define __LV_PORT_TICK_H

#ifdef __cplusplus
extern "C"
{
#endif

#define LV_TICK_PERIOD_MS 1

#include "esp_system.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "freertos/semphr.h"

    void lv_port_tick_init();

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
