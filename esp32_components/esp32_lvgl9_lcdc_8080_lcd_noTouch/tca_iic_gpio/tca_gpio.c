#include "tca_gpio.h"

#include "esp_log.h"
#include "esp_system.h"

#include "esp_check.h"

#define TCA_TRANS_TIMEOUT 1000
static const char *TAG = "tca_ioe";
static SemaphoreHandle_t *pSemaphoreIICFree;
static i2c_master_dev_handle_t *i2c_handle;

static esp_err_t tca_write_reg(uint8_t address, uint8_t src)
{
    if (pSemaphoreIICFree != NULL)
    {
        xSemaphoreTake(*pSemaphoreIICFree, portMAX_DELAY);
    }
    uint8_t cmdl[2] = {address, src};
    esp_err_t ret = i2c_master_transmit(*i2c_handle, (const uint8_t *)cmdl, 2, TCA_TRANS_TIMEOUT);
    if (pSemaphoreIICFree != NULL)
    {
        xSemaphoreGive(*pSemaphoreIICFree);
    }
    return ret;
}
static esp_err_t tca_read_reg(uint8_t address, uint8_t *dst)
{
    if (pSemaphoreIICFree != NULL)
    {
        xSemaphoreTake(*pSemaphoreIICFree, portMAX_DELAY);
    }
    esp_err_t ret = i2c_master_transmit_receive(*i2c_handle, &address, 1, dst, 1, TCA_TRANS_TIMEOUT);
    if (pSemaphoreIICFree != NULL)
    {
        xSemaphoreGive(*pSemaphoreIICFree);
    }
    return ret;
}

esp_err_t tca_init(SemaphoreHandle_t *iic_free, i2c_master_dev_handle_t *handle)
{
    if (iic_free == NULL)
    {
        pSemaphoreIICFree = NULL;
    }
    else
    {
        pSemaphoreIICFree = iic_free;
    }
    i2c_handle = handle;

    ESP_RETURN_ON_ERROR(tca_write_reg(TCA_REG_OUT_P1, 0x00), TAG, "write OUT1 reg err");
    ESP_RETURN_ON_ERROR(tca_write_reg(TCA_REG_OUT_P0, 0x00), TAG, "write OUT2 reg err");

    return ESP_OK;
}

esp_err_t tca_set_direction(uint8_t port, uint8_t mask, uint8_t mode)
{
    /*
    If a bit in this register is set to 1,
    the corresponding port pin is enabled as an input with a high-impedance output driver.
    If a bit in this register is cleared to 0,
    the corresponding port pin is enabled as an output.
    */
    uint8_t tmp;
    ESP_RETURN_ON_ERROR(tca_read_reg((port ? TCA_REG_DIRE_P1 : TCA_REG_DIRE_P0), &tmp), TAG, "read dire%d reg err", port);
    if (mode)
    {
        tmp = tmp | mask;
    }
    else
    {
        tmp = tmp & (~mask);
    }
    ESP_RETURN_ON_ERROR(tca_write_reg((port ? TCA_REG_DIRE_P1 : TCA_REG_DIRE_P0), tmp), TAG, "write dire%d reg err", port);
    return ESP_OK;
}

esp_err_t tca_read_pin(uint8_t port, uint8_t pin, uint8_t *val)
{
    uint8_t tmp;
    ESP_RETURN_ON_ERROR(tca_read_reg((port ? TCA_REG_IN_P1 : TCA_REG_IN_P0), &tmp), TAG, "read in%d reg err", port);
    *val = ((tmp & pin) == 1) ? 1 : 0;
    return ESP_OK;
}

esp_err_t tca_write_pin(uint8_t port, uint8_t pin, uint8_t val)
{
    uint8_t tmp;
    ESP_RETURN_ON_ERROR(tca_read_reg((port ? TCA_REG_IN_P1 : TCA_REG_IN_P0), &tmp), TAG, "read out%d reg err", port);
    if (val)
    {
        tmp = tmp | pin;
    }
    else
    {
        tmp = tmp & (~pin);
    }
    ESP_RETURN_ON_ERROR(tca_write_reg((port ? TCA_REG_OUT_P1 : TCA_REG_OUT_P0), tmp), TAG, "write out%d reg err", port);

    return ESP_OK;
}

esp_err_t tca_read_port(uint8_t port, uint8_t *val)
{
    ESP_RETURN_ON_ERROR(tca_read_reg((port ? TCA_REG_IN_P1 : TCA_REG_IN_P0), val), TAG, "read in%d reg err", port);

    return ESP_OK;
}

esp_err_t tca_write_port(uint8_t port, uint8_t val)
{
    ESP_RETURN_ON_ERROR(tca_write_reg((port ? TCA_REG_OUT_P1 : TCA_REG_OUT_P0), val), TAG, "write out%d reg err", port);

    return ESP_OK;
}
