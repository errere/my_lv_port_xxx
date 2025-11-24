#ifndef __TCA_GPIO_H__
#define __TCA_GPIO_H__

#include "stdint.h"

#include "esp_err.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//idf release/5.0 : semphr.h mast include with FreeRTOS.h
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include <driver/i2c_master.h>

#define TCA_PORT_0 0
#define TCA_PORT_1 1


#define TCA_IO_NUM_0 (1<<0)
#define TCA_IO_NUM_1 (1<<1)
#define TCA_IO_NUM_2 (1<<2)
#define TCA_IO_NUM_3 (1<<3)
#define TCA_IO_NUM_4 (1<<4)
#define TCA_IO_NUM_5 (1<<5)
#define TCA_IO_NUM_6 (1<<6)
#define TCA_IO_NUM_7 (1<<7)

#define TCA_MODE_OUTPUT 0
#define TCA_MODE_INPUT 1

//Input Port
#define TCA_REG_IN_P0 0x00
#define TCA_REG_IN_P1 0x01
//Output Port 
#define TCA_REG_OUT_P0 0x02
#define TCA_REG_OUT_P1 0x03
//Polarity Inversion
#define TCA_REG_PI_P0 0x04
#define TCA_REG_PI_P1 0x05
//Configuration Port
#define TCA_REG_DIRE_P0 0x06
#define TCA_REG_DIRE_P1 0x07

esp_err_t tca_init(SemaphoreHandle_t *iic_free, i2c_master_dev_handle_t *handle);

esp_err_t tca_set_direction(uint8_t port, uint8_t mask, uint8_t mode);
esp_err_t tca_read_pin(uint8_t port, uint8_t pin, uint8_t *val);
esp_err_t tca_write_pin(uint8_t port, uint8_t pin, uint8_t val);
esp_err_t tca_read_port(uint8_t port, uint8_t *val);
esp_err_t tca_write_port(uint8_t port, uint8_t val);
#endif