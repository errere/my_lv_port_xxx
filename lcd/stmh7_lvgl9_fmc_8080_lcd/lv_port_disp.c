#include "lv_port_disp.h"
#include "fmc_lcd.h"
/*********************
 *      DEFINES
 *********************/
#ifndef MY_DISP_HOR_RES
#define MY_DISP_HOR_RES 480
#endif

#ifndef MY_DISP_VER_RES
#define MY_DISP_VER_RES 320
#endif

// for lvgl9

// static spi_device_handle_t lcd_spi;
//  static lv_disp_draw_buf_t draw_buf_dsc;

// static DMA_ATTR lv_color_t lvgl_draw_buff1[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE];
// static DMA_ATTR lv_color_t lvgl_draw_buff2[LCD_X_PIXELS * LV_PORT_DISP_BUFFER_SIZE];

static lv_color_t buf_2_1[MY_DISP_HOR_RES * LV_PORT_DISP_BUFFER_SIZE]__attribute__((section(".nocacheable")));
static lv_color_t buf_2_2[MY_DISP_HOR_RES * LV_PORT_DISP_BUFFER_SIZE]__attribute__((section(".nocacheable")));
static lv_display_t *disp;

static const char *TAG = "lv_port_lcd";

static void disp_ready();

static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{
    // lv_draw_sw_rgb565_swap(px_map, ((area->x2 - area->x1 + 1) * (area->y2 - area->y1 + 1)));
    /* 等待传输完成 */
    fmc_lcd_wait_dma();

    /* 启动新的传输 */
    fmc_lcd_draw_rect_DMA(area->x1, area->y1, area->x2, area->y2, (uint16_t *)px_map);
}

void lv_port_disp_init()
{

    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);
    lv_display_set_flush_cb(disp, disp_flush);

    /* Example 2
     * Two buffers for partial rendering
     * In flush_cb DMA or similar hardware should be used to update the display in the background.*/

    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
#ifndef LVPORT_SKIP_LCD_INIT
    bsp_lcd_init();
#endif
    lcd_set_dma_cplt_cb(disp_ready);
    /* 开显示 */
    // lcd_display_switch(lcd_spi, true);
}

static void disp_ready()
{
    lv_display_flush_ready(disp);
}
