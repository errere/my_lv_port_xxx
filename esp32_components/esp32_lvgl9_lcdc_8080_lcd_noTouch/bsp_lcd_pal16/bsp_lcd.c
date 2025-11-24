// /* ST7789V driver for ESP32

//    This example code is in the Public Domain (or CC0 licensed, at your option.)

//    Unless required by applicable law or agreed to in writing, this
//    software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//    CONDITIONS OF ANY KIND, either express or implied.
// */

#include "bsp_lcd.h"

#include "esp_check.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <esp_system.h>
#include <esp_log.h>

#include <driver/gpio.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/gpio.h"

const static char *TAG = "bsp_lcd";

#define __LCD_TRANS_TYPE uint16_t

static esp_lcd_panel_io_handle_t i80_handle;

static void init_i80_bus(esp_lcd_panel_io_handle_t *io_handle, esp_lcd_panel_io_color_trans_done_cb_t trans_done_cb, void *user_ctx)
{
    ESP_LOGI(TAG, "Initialize Intel 8080 bus");
    esp_lcd_i80_bus_handle_t i80_bus = NULL;
    esp_lcd_i80_bus_config_t bus_config = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dc_gpio_num = LCD_PIN_DC,
        .wr_gpio_num = LCD_PIN_WR,
        .data_gpio_nums = LCD_PIN_Dx,
        .bus_width = LCD_PIN_BUS_WIDTH,
        .max_transfer_bytes = LCD_X_PIXELS * 100 * sizeof(uint16_t),
        .psram_trans_align = 64,
        .sram_trans_align = 4,
    };
    ESP_ERROR_CHECK(esp_lcd_new_i80_bus(&bus_config, &i80_bus));

    esp_lcd_panel_io_i80_config_t io_config = {
        .cs_gpio_num = LCD_PIN_CS,
        .pclk_hz = LCD_CLOCK_MHZ,
        .trans_queue_depth = 10,
        .dc_levels = {
            .dc_idle_level = 0,
            .dc_cmd_level = 0,
            .dc_dummy_level = 0,
            .dc_data_level = 1,
        },
        .flags = {
            .swap_color_bytes = 0, // Swap can be done in LvGL (default) or DMA.
            .reverse_color_bits = 0,
            .pclk_idle_low = 1,
        },
        .on_color_trans_done = trans_done_cb,
        .user_ctx = user_ctx,
        .lcd_cmd_bits = LCD_CMD_LENGTH,
        .lcd_param_bits = LCD_PARAM_LENGTH,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i80(i80_bus, &io_config, io_handle));
}

static void lcd_rst()
{
    gpio_reset_pin(LCD_PIN_RST);
    gpio_set_direction(LCD_PIN_RST, GPIO_MODE_OUTPUT);

    gpio_set_level(LCD_PIN_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(LCD_PIN_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

static esp_err_t init_lcd_reg(esp_lcd_panel_io_handle_t io)
{
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (__LCD_TRANS_TYPE[]){0xC3}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (__LCD_TRANS_TYPE[]){0x96}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    // Memory Access Control
    // vec ,  ic is down
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (__LCD_TRANS_TYPE[]){0x48}, 1*sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    // vec ,  ic is up
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (__LCD_TRANS_TYPE[]){0x88}, 1*sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    // hor ,  ic is right
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (__LCD_TRANS_TYPE[]){0x28}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    // hor ,  ic is left
    // ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x36, (__LCD_TRANS_TYPE[]){0xe8}, 1*sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x3A, (__LCD_TRANS_TYPE[]){0x55}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB4, (__LCD_TRANS_TYPE[]){0x01}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB1, (__LCD_TRANS_TYPE[]){0x80, 0x10}, 2 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB5, (__LCD_TRANS_TYPE[]){0x1F, 0x50, 0x00, 0x20}, 4 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xB6, (__LCD_TRANS_TYPE[]){0x8A, 0x07, 0x3B}, 3 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC0, (__LCD_TRANS_TYPE[]){0x80, 0x64}, 2 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC1, (__LCD_TRANS_TYPE[]){0x13}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC2, (__LCD_TRANS_TYPE[]){0xA7}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xC5, (__LCD_TRANS_TYPE[]){0x09}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE8, (__LCD_TRANS_TYPE[]){0x40, 0x8a, 0x00, 0x00, 0x29, 0x19, 0xA5, 0x33}, 8 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE0, (__LCD_TRANS_TYPE[]){0xF0, 0x06, 0x0B, 0x07, 0x06, 0x05, 0x2E, 0x33, 0x47, 0x3A, 0x17, 0x16, 0x2E, 0x31}, 14 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xE1, (__LCD_TRANS_TYPE[]){0xF0, 0x09, 0x0D, 0x09, 0x08, 0x23, 0x2E, 0x33, 0x46, 0x38, 0x13, 0x13, 0x2C, 0x32}, 14 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (__LCD_TRANS_TYPE[]){0x3C}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0xF0, (__LCD_TRANS_TYPE[]){0x69}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2A, (__LCD_TRANS_TYPE[]){0x00, 0x00, 0x01, 0x3F}, 4 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x2B, (__LCD_TRANS_TYPE[]){0x00, 0x00, 0x01, 0xDF}, 4 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x35, (__LCD_TRANS_TYPE[]){0x00}, 1 * sizeof(__LCD_TRANS_TYPE)), TAG, "io tx param failed");

    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x11, NULL, 0), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x29, NULL, 0), TAG, "io tx param failed");
    vTaskDelay(pdMS_TO_TICKS(50));
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(io, 0x21, NULL, 0), TAG, "io tx param failed");

    return ESP_OK;
}

void lcd_init(esp_lcd_panel_io_color_trans_done_cb_t tans_done_cb, void *user_ctx)
{
    init_i80_bus(&i80_handle, tans_done_cb, user_ctx);
    lcd_rst();
    init_lcd_reg(i80_handle);
}
esp_err_t lcd_draw_rect(int x_start, int y_start, int x_end, int y_end, const void *color_data)
{

    x_start += LCD_X_OFFSET;
    y_start += LCD_Y_OFFSET;

    // define an area of frame memory where MCU can access
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(i80_handle, 0x2a, (__LCD_TRANS_TYPE[]){
                                                                        (x_start >> 8) & 0xFF,
                                                                        x_start & 0xFF,
                                                                        ((x_end ) >> 8) & 0xFF,
                                                                        (x_end ) & 0xFF,
                                                                    },
                                                  4 * sizeof(__LCD_TRANS_TYPE)),
                        TAG, "io tx param failed");
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_param(i80_handle, 0x2b, (__LCD_TRANS_TYPE[]){
                                                                        (y_start >> 8) & 0xFF,
                                                                        y_start & 0xFF,
                                                                        ((y_end ) >> 8) & 0xFF,
                                                                        (y_end ) & 0xFF,
                                                                    },
                                                  4 * sizeof(__LCD_TRANS_TYPE)),
                        TAG, "io tx param failed");

    // transfer frame buffer
    size_t len = (x_end - x_start+1) * (y_end - y_start+1) * LCD_BPP / 8;
    ESP_RETURN_ON_ERROR(esp_lcd_panel_io_tx_color(i80_handle, 0x2c, color_data, len), TAG, "io tx color failed");

    return ESP_OK;
}
