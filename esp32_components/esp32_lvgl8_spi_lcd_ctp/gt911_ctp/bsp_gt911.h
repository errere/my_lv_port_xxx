#ifndef __BSP_GT911_H__
#define __BSP_GT911_H__

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// idf release/5.0 : semphr.h mast include with FreeRTOS.h
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <driver/i2c_master.h>

#define GT_RST_PIN 42
#define GT_INT_PIN 17

extern SemaphoreHandle_t xSemTouchDown;

esp_err_t gt911_ctp_init();

esp_err_t gt911_ctp_set_iic_device(SemaphoreHandle_t *iic_free, i2c_master_dev_handle_t *handle);

esp_err_t gt911_debug();

esp_err_t gt911_read_touch(uint16_t *x, uint16_t *y, uint16_t *sz);

#endif