#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "esp_err.h"
#include "esp_lcd_panel_io.h"

/* 屏幕参数 */
#define LCD_CLOCK_MHZ 25 * 1000 * 1000 // LCD所用总线的时钟频率
#define LCD_CMD_LENGTH 16
#define LCD_PARAM_LENGTH 16
#define LCD_BPP 16

// /* 屏幕硬件连接 */
#define LCD_PIN_BUS_WIDTH 16

#define LCD_PIN_CS 1
#define LCD_PIN_DC 2
#define LCD_PIN_WR 42
#define LCD_PIN_RST 41

#define LCD_PIN_Dx { \
    40,              \
    39,              \
    38,              \
    37,              \
    36,              \
    35,              \
    48,              \
    47,              \
    21,              \
    14,              \
    13,              \
    12,              \
    11,              \
    10,              \
    9,               \
    46,              \
}

// /* 屏幕分辨率 */
#define LCD_X_PIXELS 480
#define LCD_Y_PIXELS 320

#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0

void lcd_init(esp_lcd_panel_io_color_trans_done_cb_t tans_done_cb,void* user_ctx);
esp_err_t lcd_draw_rect(int x_start, int y_start, int x_end, int y_end, const void *color_data);

// spi_device_handle_t lcd_init();
// void lcd_display_switch(spi_device_handle_t spi, bool status);

// void lcd_draw_rect(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1,
//                    uint16_t y1, const uint8_t *dat);
// void lcd_draw_rect_wait_busy(spi_device_handle_t spi);

#endif
