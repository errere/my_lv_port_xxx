#ifndef __LV_PORT_CTP_H__
#define __LV_PORT_CTP_H__

#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "bsp_ctp_ft5x16.h"

void lv_port_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c, int xfer_time_out);

#endif