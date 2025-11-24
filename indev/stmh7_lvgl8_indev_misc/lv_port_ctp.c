#include "lv_port_ctp.h"

#include "lv_port_disp.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"

#include "bsp_ctp_ft6x36u.h"

#include "elog.h"

static lv_indev_drv_t indev_drv;

lv_indev_t *indev_touchpad;

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if (xSemaphoreTake(xSemTouchAvailable, 0) == pdPASS)
    {
        uint16_t x, y;
        ctp_read(&x, &y);
        last_x = 319 - y;
        last_y = x;
        data->state = LV_INDEV_STATE_PR;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_ctp_init(void)
{
    elog_w("lv_port_ctp", "this code modify for hor layout");
    ctp_init();
    ctp_read_id();

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_drv.disp = display_intf; // main lcd
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

// eof