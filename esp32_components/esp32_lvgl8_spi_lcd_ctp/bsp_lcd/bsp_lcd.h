#ifndef __BSP_LCD_H
#define __BSP_LCD_H

#include "driver/spi_master.h"

/* 屏幕参数 */
#define LCD_SPI_HOST SPI3_HOST // 即VSPI
#define LCD_SPI_CLOCK_MHZ 80   // LCD所用SPI总线的时钟频率

/* 屏幕硬件连接 */
#define LCD_PIN_CS 40
#define LCD_PIN_CLK 38
#define LCD_PIN_MOSI 47
#define LCD_PIN_DC  39

#define LCD_PIN_RST 21

/* 屏幕分辨率 */
#define LCD_X_PIXELS 480
#define LCD_Y_PIXELS 272

#define LCD_X_OFFSET 0
#define LCD_Y_OFFSET 0

spi_device_handle_t lcd_init();

void lcd_draw_rect(spi_device_handle_t spi, uint16_t x0, uint16_t y0, uint16_t x1,
                   uint16_t y1, const uint8_t *dat);
void lcd_draw_rect_wait_busy(spi_device_handle_t spi);

#endif
