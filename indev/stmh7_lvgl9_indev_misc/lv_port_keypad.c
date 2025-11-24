#include "lv_port_keypad.h"
// #include "elog.h"

lv_indev_t *indev_keypad;

static void keypad_init(void);
static void keypad_read(lv_indev_t *indev, lv_indev_data_t *data);
static uint32_t keypad_get_key(void);

/*------------------
 * Keypad
 * -----------------*/

/*Will be called by the library to read the mouse*/
static void keypad_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    static uint32_t last_key = 0;

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = keypad_get_key();
    // log_d("k = %d", act_key);
    if (act_key != 0)
    {
        data->state = LV_INDEV_STATE_PR;

        /*Translate the keys to LVGL control characters according to your key definitions*/
        switch (act_key)
        {
        case 1:
            act_key = LV_KEY_NEXT;
            break;
        case 2:
            act_key = LV_KEY_ENTER;
            break;
        case 3:
            act_key = LV_KEY_PREV;
            break;
        }

        last_key = act_key;
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
    }

    data->key = last_key;
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
    /*Your code comes here*/

    // if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == 0)
    // {
    //     return 1;
    // }
    // if (HAL_GPIO_ReadPin(KEY2_GPIO_Port, KEY2_Pin) == 0)
    // {
    //     return 2;
    // }
    // if (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == 0)
    // {
    //     return 3;
    // }

    return 0;
}

void lv_port_keypad_init(void)
{
    /*------------------
     * Keypad
     * -----------------*/

    /*Register a keypad input device*/
    indev_keypad = lv_indev_create();
    lv_indev_set_type(indev_keypad, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev_keypad, keypad_read);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/
}