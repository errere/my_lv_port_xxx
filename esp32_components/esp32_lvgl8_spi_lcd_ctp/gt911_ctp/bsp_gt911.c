/*
 * MIT License
 *
 * Copyright (c) 2025 G.C.Hikari
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "bsp_gt911.h"

#include "esp_log.h"
#include "esp_system.h"

#include "esp_check.h"

#include "driver/gpio.h"

#define CTP_IIC_TIMEOUT 1000

static const char *TAG = "bsp_gt911";

static SemaphoreHandle_t *pSemaphoreIICFree;
static i2c_master_dev_handle_t *i2c_handle;

SemaphoreHandle_t xSemTouchDown;

/**
 * @brief read any byte reg
 */
static esp_err_t ctp_iic_readRegs(uint16_t address, uint8_t len, uint8_t *dat)
{
    if (pSemaphoreIICFree != 0)
    {
        xSemaphoreTake(*pSemaphoreIICFree, portMAX_DELAY);
    }
    uint8_t reg[2] = {(address >> 8) & 0xff, (address) & 0xff};
    esp_err_t ret = i2c_master_transmit_receive(*i2c_handle, reg, 2, dat, len, CTP_IIC_TIMEOUT);

    if (pSemaphoreIICFree != 0)
    {
        xSemaphoreGive(*pSemaphoreIICFree);
    }

    return ret;
}

/**
 * @brief write one byte reg
 */
static esp_err_t ctp_iic_writeReg(uint16_t address, uint8_t dat)
{
    if (pSemaphoreIICFree != 0)
    {
        xSemaphoreTake(*pSemaphoreIICFree, portMAX_DELAY);
    }

    uint8_t reg[3] = {(address >> 8) & 0xff, (address) & 0xff, dat};
    esp_err_t ret = i2c_master_transmit(*i2c_handle, reg, 3, CTP_IIC_TIMEOUT);

    if (pSemaphoreIICFree != 0)
    {
        xSemaphoreGive(*pSemaphoreIICFree);
    }

    return ret;
}

static void IRAM_ATTR gt911_int_isr_handler(void *arg)
{
    BaseType_t sw = 0;
    xSemaphoreGiveFromISR(xSemTouchDown, &sw);
    if (sw)
    {
        portYIELD_FROM_ISR(sw);
    }
}

static void ctp_rst_seq()
{
    // ctp int & rst drive
    gpio_reset_pin(GT_INT_PIN); // CTP_INT
    gpio_set_direction(GT_INT_PIN, GPIO_MODE_OUTPUT);
    gpio_reset_pin(GT_RST_PIN); // CTP RST
    gpio_set_direction(GT_RST_PIN, GPIO_MODE_OUTPUT);

    gpio_set_level(GT_RST_PIN, 0); // rst=0
    gpio_set_level(GT_INT_PIN, 0); // int=0
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GT_INT_PIN, 1); // int=1
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(GT_RST_PIN, 1); // rst=1

    vTaskDelay(pdMS_TO_TICKS(150));

    gpio_set_level(GT_INT_PIN, 0); // int=0
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_reset_pin(GT_INT_PIN); // int=hiz

    // add interrupt support
    // zero-initialize the config structure.
    gpio_config_t io_conf = {};
    // interrupt of rising edge
    io_conf.intr_type = GPIO_INTR_POSEDGE;
    // bit mask of the pins, use GPIO4/5 here
    io_conf.pin_bit_mask = (1UL << GT_INT_PIN);
    // set as input mode
    io_conf.mode = GPIO_MODE_INPUT;
    // disable pull-down mode
    io_conf.pull_down_en = 0;
    // enable pull-up mode
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // install gpio isr service
    gpio_install_isr_service(0);

    // hook isr handler for specific gpio pin
    gpio_isr_handler_add(GT_INT_PIN, gt911_int_isr_handler, NULL);
}

static esp_err_t gt911_ctp_read_id(uint8_t *dst, uint8_t len)
{
    return ctp_iic_readRegs(0x8140, len, dst);
}

/*==========================================================================================================*/

esp_err_t gt911_ctp_init()
{
    xSemTouchDown = xSemaphoreCreateBinary();//xSemaphoreCreateCounting(64, 0);
    assert(xSemTouchDown);

    uint8_t id[4] = {0, 0, 0, 0};
    ctp_rst_seq();
    ESP_RETURN_ON_ERROR(gt911_ctp_read_id(id, 4), TAG, "error on read id IO");
    id[3] = 0;
    ESP_LOGI(TAG, "gt911 id = %s", id);
    return ESP_OK;
}

esp_err_t gt911_ctp_set_iic_device(SemaphoreHandle_t *iic_free, i2c_master_dev_handle_t *handle)
{
    pSemaphoreIICFree = iic_free;
    i2c_handle = handle;
    return ESP_OK;
}

esp_err_t gt911_debug()
{
    return ESP_OK;
}

esp_err_t gt911_read_touch(uint16_t *x, uint16_t *y, uint16_t *sz)
{
    uint8_t reg;
    uint8_t p[8];
    ctp_iic_readRegs(0x814e, 1, &reg);
    if (reg & 0x80)
    {
        ctp_iic_readRegs(0x814e, 8, p);
        *x = p[3] << 8 | p[2];
        *y = p[5] << 8 | p[4];
        *sz = p[7] << 8 | p[6];
        ctp_iic_writeReg(0x814e, 0x00);
    }else{
        *sz = 0;
    }
    return ESP_OK;
}

// eof