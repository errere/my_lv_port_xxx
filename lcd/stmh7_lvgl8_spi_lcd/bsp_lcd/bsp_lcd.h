#ifndef __BSP_LCD_H__
#define __BSP_LCD_H__

#include <stdint.h>

void bsp_lcd_init();

// with swap
uint8_t bsp_lcd_draw_pixel(int16_t x0, int16_t y0, uint16_t color);

uint8_t bsp_lcd_draw_rect(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf);

// with swap
uint8_t bsp_lcd_draw_rect_single(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t px);

uint8_t bsp_lcd_draw_rect_DMA(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint8_t *buf);

uint8_t bsp_lcd_wait_dma_down();

void lcd_set_dma_cplt_cb(void (*cplt)());

#endif