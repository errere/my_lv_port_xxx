#include "lv_port_disp.h"

#include "bsp_lcd.h"
#include "FreeRTOS.h"
#include "task.h"

#include "elog.h"

static const char *TAG = "lv_port_lcd";

/*********************
 *      VARIABLES
 *********************/
lv_disp_t *display_intf;

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
static lv_color_t buf_1[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".noCacheable_SRAM1")));
static lv_color_t buf_2[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".noCacheable_SRAM1")));

static lv_disp_drv_t lcd_disp_drv; /*Descriptor of a display driver*/

/**********************
 *      MACROS
 **********************/

/**********************
 *   GLOBAL FUNCTIONS
 **********************/

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    bsp_lcd_init();

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
    lcd_disp_drv.hor_res = 320;
    lcd_disp_drv.ver_res = 240;
    /*Used to copy the buffer's content to the display*/
    lcd_disp_drv.flush_cb = disp_flush;
    /*Set a display buffer*/
    lcd_disp_drv.draw_buf = &lcd_draw_buf_desc;

    display_intf = lv_disp_drv_register(&lcd_disp_drv);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/
/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    // ch1
    bsp_lcd_wait_dma_down();
    /*IMPORTANT!!!
     *Inform the graphics library that you are ready with the flushing*/
    lv_disp_flush_ready(disp_drv);
    bsp_lcd_draw_rect_DMA(area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p);
}

// eof