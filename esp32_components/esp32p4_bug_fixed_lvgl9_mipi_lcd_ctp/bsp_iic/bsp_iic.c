#include "bsp_iic.h"
#include "esp_log.h"

static i2c_master_bus_handle_t bus[MYCONF_IIC_BUS_MAX];

esp_err_t iic_param_set(i2c_master_bus_config_t *conf)
{
    return i2c_new_master_bus(conf, &bus[conf->i2c_port]);
}

esp_err_t iic_device_assign(int bus_num, i2c_device_config_t *conf, i2c_master_dev_handle_t *handle)
{
    return i2c_master_bus_add_device(bus[bus_num], conf, handle);
}

void iic_scan(int dev)
{
    ESP_LOGI("IIC_SCAN", "==========dev %d==========", dev);
    for (uint8_t i = 0; i < 0x7f; i++)
    {
        if (i2c_master_probe(bus[dev], i, 1000) == ESP_OK)
        {
            ESP_LOGI("IIC_SCAN", "0x%02x find in dev %d", i, dev);
        }
    }
    ESP_LOGI("IIC_SCAN", "========== end ==========");
}

void iic_scan_spec(int dev, uint8_t addr)
{

    if (i2c_master_probe(bus[dev], addr, 1000) == ESP_OK)
    {
        ESP_LOGI("IIC_SCAN", "0x%02x find in dev %d", addr, dev);
    }
    else
    {
        ESP_LOGW("IIC_SCAN", "0x%02x no in dev %d", addr, dev);
    }
}