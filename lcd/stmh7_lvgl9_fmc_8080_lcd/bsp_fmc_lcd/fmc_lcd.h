#ifndef __FMC_LCD_H__
#define __FMC_LCD_H__
#include <stdint.h>

void fmc_test();

void fmc_lcd_init();

void fmc_lcd_draw_rect_single(int x_start, int y_start, int x_end, int y_end, const uint16_t c);
void fmc_lcd_draw_rect(int x_start, int y_start, int x_end, int y_end, uint16_t *c);
void fmc_lcd_draw_rect_DMA(int x_start, int y_start, int x_end, int y_end, uint16_t *c);
void fmc_lcd_wait_dma();
void lcd_set_dma_cplt_cb(void (*cplt)());
#endif