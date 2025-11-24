#include "lv_port_input_keypad.h"
//#include "esp_log.h"
#include "driver/gpio.h"
#include "tca_gpio.h"
//for lvgl9

// #include "esp_log.h"

// static const char *TAG = "keyPad";

static lv_indev_t * indev_keypad;

/*------------------
 * Keypad
 * -----------------*/

/*Initialize your keypad*/
static void keypad_init(void)
{
    //keypad init code e.g:gpi init

    gpio_reset_pin(15);
    gpio_reset_pin(0);

    gpio_set_direction(15,GPIO_MODE_INPUT);
    gpio_set_direction(0,GPIO_MODE_INPUT);

}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
    uint8_t tmp = 0;
    if (gpio_get_level(15) == 0)
        return 1;
    else if (gpio_get_level(0) == 0)
        return 2;
    else if (tca_read_port(TCA_PORT_0, &tmp) == ESP_OK)
    {
        if ((tmp & TCA_IO_NUM_4) == 0)
            return 3;
        else if ((tmp & TCA_IO_NUM_5) == 0)
            return 4;
    }
    return 0;
}

/*Will be called by the library to read the mouse*/
static void keypad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    static uint32_t last_key = 0;

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = keypad_get_key();
    if (act_key != 0)
    {
        data->state = LV_INDEV_STATE_PR;
        //ESP_LOGI("TAG", "press : %ld", act_key);

        // /*Translate the keys to LVGL control characters according to your key definitions*/
        // switch (act_key)
        // {
        // case 1:
        //     act_key = LV_KEY_ESC;
        //     break;
        // case 2:
        //     act_key = LV_KEY_ENTER;
        //     break;
        // case 3:
        //     act_key = LV_KEY_PREV;
        //     break;
        // case 4:
        //     act_key = LV_KEY_NEXT;
        //     break;
        // // case 5:
        // //     act_key = LV_KEY_RIGHT;
        // //     break;
        // // case 6:
        // //     act_key = LV_KEY_PREV;
        // //     break;
        // }

                /*Translate the keys to LVGL control characters according to your key definitions*/
        switch (act_key)
        {
        case 1:
            act_key = LV_KEY_DOWN;
            break;
        case 2:
            act_key = LV_KEY_UP;
            break;
        case 3:
            act_key = LV_KEY_NEXT;
            break;
        case 4:
            act_key = LV_KEY_ENTER;
            break;
        // case 5:
        //     act_key = LV_KEY_RIGHT;
        //     break;
        // case 6:
        //     act_key = LV_KEY_PREV;
        //     break;
        }

        last_key = act_key;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

void lv_port_keypad_init(lv_group_t ** group)
{
    /*------------------
     * Keypad
     * -----------------*/

    /*Initialize your keypad or keyboard if you have*/
    keypad_init();

    /*Register a keypad input device*/
    indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/
    lv_indev_set_group(indev_keypad, *group);
}