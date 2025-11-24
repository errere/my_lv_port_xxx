#ifndef __BSP_CTP_FT5X16_H__
#define __BSP_CTP_FT5X16_H__

#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "driver/i2c_master.h"

extern SemaphoreHandle_t xSemCTPAvailable;

esp_err_t ft_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c,int xfer_time_out);
uint8_t ft_ctp_read_touch_0(uint16_t *x, uint16_t *y, uint8_t *flag);

void ft_ctp_debug();
#endif