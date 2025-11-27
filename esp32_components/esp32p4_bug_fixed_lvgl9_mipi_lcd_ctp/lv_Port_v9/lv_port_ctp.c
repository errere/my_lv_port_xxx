#include "lv_port_ctp.h"
#include "bsp_ctp_ft6x36u.h"

lv_indev_t *indev_touchpad;

/**********************
 *   STATIC FUNCTIONS
 **********************/

/*------------------
 * Touchpad
 * -----------------*/

/*Will be called by the library to read the touchpad*/
static void touchpad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static uint16_t last_x = 0;
    static uint16_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if (xSemaphoreTake(xSemCTPAvailable, 0) == pdPASS)
    {
        ft_ctp_read_touch_0(&last_x, &last_y);
        data->state = LV_INDEV_STATE_PRESSED;
    }
    else
    {
        data->state = LV_INDEV_STATE_RELEASED;
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_ctp_init(SemaphoreHandle_t *i2c_mux, i2c_master_dev_handle_t *i2c, int xfer_time_out)
{
    /*------------------
     * Touchpad
     * -----------------*/

    /*Initialize your touchpad if you have*/
    ft_ctp_init(i2c_mux, i2c, xfer_time_out);

    /*Register a touchpad input device*/
    indev_touchpad = lv_indev_create();
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, touchpad_read);
} // lv_port_ctp_init

// eof