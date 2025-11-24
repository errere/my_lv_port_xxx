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
#include "lv_port_lcd.h"
#include <esp_system.h>
#include <esp_log.h>

#include "bsp_lcd.h"

static const char *TAG = "lv_port_lcd";

static spi_device_handle_t lcd_spi;

static lv_disp_draw_buf_t draw_buf_dsc; // 需要全程生命周期，设置为静态变量

static lv_disp_drv_t disp_drv;      // 显示设备描述结构

/* 申请双缓冲区 */
static lv_color_t buf_3_1[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE]; /*A screen sized buffer*/
static lv_color_t buf_3_2[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE]; /*Another screen sized buffer*/

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
   /* 等待传输完成 */
   lcd_draw_rect_wait_busy(lcd_spi);
   /* 通知lvgl传输已完成 */
   lv_disp_flush_ready(disp_drv);

   /* 启动新的传输 */
   lcd_draw_rect(lcd_spi, area->x1, area->y1, area->x2, area->y2, (const uint8_t *)color_p);
}

static void disp_clear(lv_color_t *buf)
{
   uint16_t i;
   for (i = 0; i < LCD_Y_PIXELS - LV_PORT_DISP_BUFFER_SIZE; i += LV_PORT_DISP_BUFFER_SIZE)
   {
      lcd_draw_rect(lcd_spi, 0, i, LCD_X_PIXELS - 1, i + LV_PORT_DISP_BUFFER_SIZE - 1,
                    (const uint8_t *)buf);
      lcd_draw_rect_wait_busy(lcd_spi);
   }

   /* 最后一次清屏指令不等待结束 从而使后续的传输可以等待到信号量 */
   lcd_draw_rect(lcd_spi, 0, i, LCD_X_PIXELS - 1, LCD_Y_PIXELS - 1,
                 (const uint8_t *)buf);
}

void lv_port_disp_init()
{

   /* 初始化LCD总线与寄存器 */
   lcd_spi = lcd_init();

   /* 向lvgl注册申请的缓冲区 */
   lv_disp_draw_buf_init(&draw_buf_dsc, buf_3_1, buf_3_2,
                         LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE);

   ESP_LOGI(TAG, "free heap after lvgl buffer allocation: %ld bytes", esp_get_free_heap_size());

   /* 清空LCD显存 */
   disp_clear(buf_3_1);

   /* 创建并初始化用于在LVGL中注册显示设备的结构 */

   lv_disp_drv_init(&disp_drv); // 使用默认值初始化该结构

   /* 设置显示分辨率 */
   disp_drv.hor_res = LCD_X_PIXELS;
   disp_drv.ver_res = LCD_Y_PIXELS;

   /* 设置显示函数 用于在将矩形缓冲区刷新到屏幕上 */
   disp_drv.flush_cb = disp_flush;

   disp_drv.draw_buf = &draw_buf_dsc;

   /* 注册显示设备 */
   lv_disp_drv_register(&disp_drv);

}
