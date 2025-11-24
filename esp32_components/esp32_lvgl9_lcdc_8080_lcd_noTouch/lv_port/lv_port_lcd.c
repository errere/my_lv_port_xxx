#include "lv_port_lcd.h"

#include "esp_log.h"
#include "esp_lcd_panel_io.h"

#include "lv_port_lcd.h"
#include <esp_system.h>
#include <esp_log.h>
#include "bsp_lcd.h"

// for lvgl9

static lv_color_t buf_2_1[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE];
static lv_color_t buf_2_2[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE];
static lv_display_t *disp;

static const char *TAG = "lv_port_lcd";

static bool i80_notify_trans_done(esp_lcd_panel_io_handle_t panel_io, esp_lcd_panel_io_event_data_t *edata, void *user_ctx)
{
    lv_display_flush_ready(disp);
    return false;
}

static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    // lv_draw_sw_rgb565_swap(px_map, ((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)));
    /* 启动新的传输 */
    lcd_draw_rect(area->x1, area->y1, area->x2, area->y2, (const void *)px_map);
}

// static void disp_clear(lv_color_t *buf)
// {
//    uint16_t i;
//    for (i = 0; i < LCD_Y_PIXELS - LV_PORT_DISP_BUFFER_SIZE; i += LV_PORT_DISP_BUFFER_SIZE)
//    {
//       lcd_draw_rect(lcd_spi, 0, i, LCD_X_PIXELS - 1, i + LV_PORT_DISP_BUFFER_SIZE - 1,
//                     (const uint8_t *)buf);
//       lcd_draw_rect_wait_busy(lcd_spi);
//    }
//    /* 最后一次清屏指令不等待结束 从而使后续的传输可以等待到信号量 */
//    lcd_draw_rect(lcd_spi, 0, i, LCD_X_PIXELS - 1, LCD_Y_PIXELS - 1,
//                  (const uint8_t *)buf);
// }

void lv_port_disp_init()
{
    /* 初始化LCD总线与寄存器 */
    lcd_init(i80_notify_trans_done, NULL);

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    disp = lv_display_create(LCD_X_PIXELS, LCD_Y_PIXELS);
    lv_display_set_flush_cb(disp, disp_flush);
    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/

    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
}
