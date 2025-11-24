#include "lv_port_disp.h"

#include "bsp_lcd.h"
#include "FreeRTOS.h"
#include "task.h"

#include "elog.h"

static const char *TAG = "lv_port_lcd";

/*********************
 *      VARIABLES
 *********************/
lv_disp_t *display_intf[3];//ch1,ch2,main

/**********************
 *      TYPEDEFS
 **********************/

/**********************
 *  STATIC PROTOTYPES
 **********************/
static void disp_init(void);

static void disp_flush(lv_disp_drv_t *lcd_disp_drv, const lv_area_t *area, lv_color_t *color_p);

/**********************
 *  STATIC VARIABLES
 **********************/

static const uint8_t gc9107_init_seq[] = {
    0xFE,
    0,
    0xEF,
    0,
    0xB0,
    1,
    0xC0,
    0xB1,
    1,
    0x80,
    0xB2,
    1,
    0x27,
    0xB3,
    1,
    0x13,
    0xB6,
    1,
    0x19,
    0xB7,
    1,
    0x05,
    0xAC,
    1,
    0xC8,
    0xAB,
    1,
    0x0f,
    0x3A,
    1,
    0x05,
    0xB4,
    1,
    0x04,
    0xA8,
    1,
    0x08,
    0xB8,
    1,
    0x08,
    0xEA,
    1,
    0x02,
    0xE8,
    1,
    0x2A,
    0xE9,
    1,
    0x47,
    0xE7,
    1,
    0x5F,
    0xC6,
    1,
    0x21,
    0xC7,
    1,
    0x15,
    0xF0,
    14,
    0x1D,
    0x38,
    0x09,
    0x4D,
    0x92,
    0x2F,
    0x35,
    0x52,
    0x1E,
    0x0C,
    0x04,
    0x12,
    0x14,
    0x1F,
    0xF1,
    14,
    0x16,
    0x40,
    0x1C,
    0x54,
    0xA9,
    0x2D,
    0x2E,
    0x56,
    0x10,
    0x0D,
    0x0C,
    0x1A,
    0x14,
    0x1E,
    0xF4,
    3,
    0x00,
    0x00,
    0xFF,
    0xBA,
    2,
    0xFF,
    0xFF,
    0x36, // rot
    1,
    0xC0, // 0x00,0xC0,0x60,0xA0
    0x11,
    0,
    0x29,
    0,
};

static const uint8_t gc9107_wkup_seq[] = {0x11, 0};

static const uint8_t st7789v_init_seq[] = {
    0x36, 1,
    0x00,
    0x3A, 1,
    0x05,
    0xB2, 5,
    0x0C,
    0x0C,
    0x00,
    0x33,
    0x33,
    0xB7, 1,
    0x35,
    0xBB, 1,
    0x29, // 32 Vcom=1.35V
    0xC2, 1,
    0x01,
    0xC3, 1,
    0x19, // GVDD=4.8V
    0xC4, 1,
    0x20, // VDV, 0x20:0v
    0xC5, 1,
    0x1A, // VCOM Offset Set
    0xC6, 1,
    0x1F, // 0x0F:60Hz
    0xD0, 2,
    0xA7, // A4
    0xA1,
    0xE0, 14,
    0xD0,
    0x08,
    0x0E,
    0x09,
    0x09,
    0x05,
    0x31,
    0x33,
    0x48,
    0x17,
    0x14,
    0x15,
    0x31,
    0x34,
    0xE1, 14,
    0xD0,
    0x08,
    0x0E,
    0x09,
    0x09,
    0x15,
    0x31,
    0x33,
    0x48,
    0x17,
    0x14,
    0x15,
    0x31,
    0x34,
    0x21, 0,
    0x29, 0};

static const uint8_t st7789v_wkup_seq[] = {0x11, 0};

static stm_lcd_desc_t lcd_ch1;
static stm_lcd_desc_t lcd_ch2;
static stm_lcd_desc_t lcd_main;

static lv_disp_draw_buf_t lcd_draw_buf_desc[3];
static lv_color_t buf_1_1[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));
static lv_color_t buf_1_2[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));

static lv_color_t buf_2_1[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));
static lv_color_t buf_2_2[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));

static lv_color_t buf_3_1[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));
static lv_color_t buf_3_2[LV_PORT_DISP_BUFFER_SIZE] __attribute__((section(".nocacheable")));

static lv_disp_drv_t lcd_disp_drv[3]; /*Descriptor of a display driver*/

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
    disp_init();

    /*-----------------------------
     * Create a buffer for drawing
     *----------------------------*/
    lv_disp_draw_buf_init(&lcd_draw_buf_desc[0], buf_1_1, buf_1_2, LV_PORT_DISP_BUFFER_SIZE); /*Initialize the display buffer*/
    lv_disp_draw_buf_init(&lcd_draw_buf_desc[1], buf_2_1, buf_2_2, LV_PORT_DISP_BUFFER_SIZE); /*Initialize the display buffer*/
    lv_disp_draw_buf_init(&lcd_draw_buf_desc[2], buf_3_1, buf_3_2, LV_PORT_DISP_BUFFER_SIZE); /*Initialize the display buffer*/

    /*-----------------------------------
     * Register the display in LVGL
     *----------------------------------*/
    lv_disp_drv_init(&lcd_disp_drv[0]); /*Basic initialization*/
    lv_disp_drv_init(&lcd_disp_drv[1]); /*Basic initialization*/
    lv_disp_drv_init(&lcd_disp_drv[2]); /*Basic initialization*/

    /*Set up the functions to access to your display*/
    uint16_t x = 0, y = 0;
    /*==============================================================================================*/
    /*Set the resolution of the display*/
    lcd_get_res(&lcd_ch1, &x, &y);
    lcd_disp_drv[0].hor_res = x;
    lcd_disp_drv[0].ver_res = y;
    /*Used to copy the buffer's content to the display*/
    lcd_disp_drv[0].flush_cb = disp_flush;
    /*Set a display buffer*/
    lcd_disp_drv[0].draw_buf = &lcd_draw_buf_desc[0];

    /*==============================================================================================*/
    /*Set the resolution of the display*/
    lcd_get_res(&lcd_ch2, &x, &y);
    lcd_disp_drv[1].hor_res = x;
    lcd_disp_drv[1].ver_res = y;
    /*Used to copy the buffer's content to the display*/
    lcd_disp_drv[1].flush_cb = disp_flush;
    /*Set a display buffer*/
    lcd_disp_drv[1].draw_buf = &lcd_draw_buf_desc[1];

    /*==============================================================================================*/
    /*Set the resolution of the display*/
    lcd_get_res(&lcd_main, &x, &y);
    lcd_disp_drv[2].hor_res = x;
    lcd_disp_drv[2].ver_res = y;
    /*Used to copy the buffer's content to the display*/
    lcd_disp_drv[2].flush_cb = disp_flush;
    /*Set a display buffer*/
    lcd_disp_drv[2].draw_buf = &lcd_draw_buf_desc[2];

    /*==============================================================================================*/
    /*Finally register the driver*/
    display_intf[0] = lv_disp_drv_register(&lcd_disp_drv[0]);
    display_intf[1] = lv_disp_drv_register(&lcd_disp_drv[1]);
    display_intf[2] = lv_disp_drv_register(&lcd_disp_drv[2]);
}

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*Initialize your display and the required peripherals.*/
static void disp_init(void)
{
    st_lcd_base_init(); // init dma flag
    // ch1 lcd
    stm_lcd_gpio_desc_t lcd_dc = {.gpiox = GPIOH, .pinx = GPIO_PIN_14};
    stm_lcd_gpio_desc_t lcd_rst = {.gpiox = GPIOH, .pinx = GPIO_PIN_15};
    stm_lcd_gpio_desc_t lcd_nss = {.gpiox = GPIOI, .pinx = GPIO_PIN_2};
    st_lcd_init(&lcd_ch1, &hspi2, 1, 1, 0, 128, 128, 0, 0, lcd_dc, lcd_rst, lcd_nss, NULL);
    // ch2 lcd init
    lcd_nss.gpiox = GPIOH;
    lcd_nss.pinx = GPIO_PIN_13;
    st_lcd_init(&lcd_ch2, &hspi2, 1, 1, 0, 128, 128, 0, 0, lcd_dc, lcd_rst, lcd_nss, NULL);
    // main lcd
    lcd_dc.gpiox = GPIOG;
    lcd_dc.pinx = GPIO_PIN_12;
    lcd_rst.gpiox = GPIOG;
    lcd_rst.pinx = GPIO_PIN_13;
    lcd_nss.gpiox = NULL;
    lcd_nss.pinx = NULL;
    st_lcd_init(&lcd_main, &hspi1, 0, 1, 1, 240, 320, 0, 0, lcd_dc, lcd_rst, lcd_nss, NULL);

    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_send_sequence(&lcd_ch1, gc9107_wkup_seq, sizeof(gc9107_wkup_seq)); /* 退出睡眠模式 */
    lcd_send_sequence(&lcd_ch2, gc9107_wkup_seq, sizeof(gc9107_wkup_seq)); /* 退出睡眠模式 */
    lcd_send_sequence(&lcd_main, st7789v_wkup_seq, sizeof(st7789v_wkup_seq)); /* 退出睡眠模式 */
    vTaskDelay(pdMS_TO_TICKS(120));
    lcd_send_sequence(&lcd_ch1, gc9107_init_seq, sizeof(gc9107_init_seq));
    lcd_send_sequence(&lcd_ch2, gc9107_init_seq, sizeof(gc9107_init_seq));
    lcd_send_sequence(&lcd_main, st7789v_init_seq, sizeof(st7789v_init_seq));
}

/*Flush the content of the internal buffer the specific area on the display
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_disp_flush_ready()' has to be called when finished.*/
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    if (disp_drv == &lcd_disp_drv[0])
    {
        // ch1
        lcd_wait_dma_cplt(&lcd_ch1, 0xffffffff);
        /*IMPORTANT!!!
         *Inform the graphics library that you are ready with the flushing*/
        lv_disp_flush_ready(disp_drv);
        lcd_draw_rect_DMA(&lcd_ch1, area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p, 0xffffffff);
    }
    else if (disp_drv == &lcd_disp_drv[1])
    {
        // ch2
        lcd_wait_dma_cplt(&lcd_ch2, 0xffffffff);
        /*IMPORTANT!!!
         *Inform the graphics library that you are ready with the flushing*/
        lv_disp_flush_ready(disp_drv);
        lcd_draw_rect_DMA(&lcd_ch2, area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p, 0xffffffff);
    }
    else if (disp_drv == &lcd_disp_drv[2])
    {
        // main
        lcd_wait_dma_cplt(&lcd_main, 0xffffffff);
        /*IMPORTANT!!!
         *Inform the graphics library that you are ready with the flushing*/
        lv_disp_flush_ready(disp_drv);
        lcd_draw_rect_DMA(&lcd_main, area->x1, area->y1, area->x2, area->y2, (uint8_t *)color_p, 0xffffffff);
    }
    else
    {
    }
}

// eof