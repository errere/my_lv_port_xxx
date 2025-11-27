#ifndef __BSP_IIC_H__
#define __BSP_IIC_H__

#include "esp_log.h"
#include <driver/i2c_master.h>
#include "esp_err.h"
#include <stdint.h>

#define MYCONF_IIC_BUS_MAX SOC_I2C_NUM

// #define BSP_IIC_DEV I2C_NUM_1

esp_err_t iic_param_set(i2c_master_bus_config_t *conf);

esp_err_t iic_device_assign(int bus_num, i2c_device_config_t *conf, i2c_master_dev_handle_t *handle);


void iic_scan(int dev);

void iic_scan_spec(int dev,uint8_t addr);

#endif