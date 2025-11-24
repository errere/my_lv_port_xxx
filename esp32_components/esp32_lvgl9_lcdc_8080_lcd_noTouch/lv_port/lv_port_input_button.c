#include "lv_port_input_button.h"
#include "tca_gpio.h"

static lv_indev_t *indev_button;

/*Initialize your buttons*/
static void button_init(void)
{
    /*Your code comes here*/
}

/*Will be called by the library to read the button*/
static int8_t button_get_pressed_id()
{
    int8_t ret = -1;
    uint8_t tmp = 0;

    if (tca_read_port(TCA_PORT_0, &tmp) == ESP_OK)
    {
        if ((tmp & TCA_IO_NUM_0) == 0)
            ret = 0;
        else if ((tmp & TCA_IO_NUM_1) == 0)
            ret = 1;
        else if ((tmp & TCA_IO_NUM_2) == 0)
            ret = 2;
        else if ((tmp & TCA_IO_NUM_3) == 0)
            ret = 3;
    }

    return ret;
}

static void button_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{

    static uint8_t last_btn = 0;

    /*Get the pressed button's ID*/
    int8_t btn_act = button_get_pressed_id();

    if (btn_act >= 0)
    {
        data->state = LV_INDEV_STATE_PR;
        last_btn = btn_act;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    /*Save the last pressed button's ID*/
    data->btn_id = last_btn;
}

void lv_port_button_init()
{
    /*------------------
     * Button
     * -----------------*/

    /*Initialize your button if you have*/
    button_init();

    /*Register a button input device*/
    indev_button = lv_indev_create();
    lv_indev_set_type(indev_button, LV_INDEV_TYPE_BUTTON);
    lv_indev_set_read_cb(indev_button, button_read);

    /*Assign buttons to points on the screen*/
    static const lv_point_t btn_points[4] = {
        {15, 310},
        {15, 310-55},
        {15, 310-110},
        {15, 310-165-15},
    };
    lv_indev_set_button_points(indev_button, btn_points);

}