#include "lv_port_ctp.h"

#include "lv_port_lcd.h"

static lv_indev_drv_t indev_drv;

lv_indev_t *indev_touchpad;

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if (xSemaphoreTake(xSemCTPAvailable, 0) == pdPASS)
    {
        uint8_t flag;
        uint16_t x, y;
        ft_ctp_read_touch_0(&x, &y, &flag);

        if (flag == 2) // Contact
        {
            last_x = 239 - x;
            last_y = y;
            data->state = LV_INDEV_STATE_PR;
        }
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c, int xfer_time_out)
{
    ft_ctp_init(i2c_mux, i2c, xfer_time_out);

    /*Register a touchpad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_drv.disp = display_intf; // main lcd
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

// eof