#include "lv_port_lcd.h"
#include <esp_system.h>
#include <esp_log.h>
#include "bsp_lcd.h"


static const char *TAG = "lv_port_lcd";

/*********************
 *      VARIABLES
 *********************/
lv_disp_t *display_intf;
static spi_device_handle_t lcd_spi;
/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/

static void disp_flush(lv_disp_drv_t *lcd_disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/

static lv_disp_draw_buf_t lcd_draw_buf_desc;
static DMA_ATTR lv_color_t buf_1[LV_PORT_DISP_BUFFER_SIZE];
static DMA_ATTR lv_color_t buf_2[LV_PORT_DISP_BUFFER_SIZE];

static lv_disp_drv_t lcd_disp_drv; /*Descriptor of a display driver*/

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    /* 等待传输完成 */
    lcd_draw_rect_wait_busy(lcd_spi);
    /* 通知lvgl传输已完成 */
    lv_disp_flush_ready(disp_drv);

    /* 启动新的传输 */
    lcd_draw_rect(lcd_spi, area->x1, area->y1, area->x2, area->y2, (const uint8_t *)color_p);
}

void lv_port_disp_init()
{

    /*-------------------------
     * Initialize your display
     * -----------------------*/
    lcd_spi = lcd_init();

    lcd_draw_rect(lcd_spi, 0, 0, 1, 1, (const uint8_t *)buf_1);
    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/
    lv_disp_draw_buf_init(&lcd_draw_buf_desc, buf_1, buf_2, LV_PORT_DISP_BUFFER_SIZE); /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    lv_disp_drv_init(&lcd_disp_drv); /*Basic initialization*/

    /*Set up the functions to access to your display*/
    /*==============================================================================================*/
    /*Set the resolution of the display*/
    lcd_disp_drv.hor_res = 240;
    lcd_disp_drv.ver_res = 320;
    /*Used to copy the buffer's content to the display*/
    lcd_disp_drv.flush_cb = disp_flush;
    /*Set a display buffer*/
    lcd_disp_drv.draw_buf = &lcd_draw_buf_desc;

    display_intf = lv_disp_drv_register(&lcd_disp_drv);
}
