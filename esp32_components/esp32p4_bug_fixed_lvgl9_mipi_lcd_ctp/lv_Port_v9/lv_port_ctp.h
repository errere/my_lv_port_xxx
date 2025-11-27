#ifndef __LV_PORT_CTP_H
#define __LV_PORT_CTP_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/i2c_master.h"

    void lv_port_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c, int xfer_time_out);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
