#include "lv_port_keypad.h"

#include <stdint.h>

#include "gpio.h"
#include "stm32h7xx_hal_gpio.h"

#include "elog.h"

static const char *TAG = "lv_keypad";

lv_indev_t *indev_keypad;

static inline void clean_row(void)
{
    /*Your code comes here*/
    // row driver clean
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 1);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 1);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 1);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 1); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 1); // 7
}

// wait level stable
static void ks_delay(void)
{
    for (uint32_t i = 0; i < 50; i++)
    {
        __NOP();
    }
}

/*Get the currently being pressed key.  0 if no key is pressed*/
static uint32_t keypad_get_key(void)
{
    uint32_t xkey = 0;

    /*==========bsp==========*/
    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 0);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 1);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 1);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 1); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 1); // 7
    ks_delay();
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_4) == 0) ? 1 : 0;
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0) ? 1 : 0;

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 1);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 0);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 1);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 1); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 1); // 7
    ks_delay();
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_4) == 0) ? 1 : 0;
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0) ? 1 : 0;

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 1);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 1);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 0);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 1); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 1); // 7
    ks_delay();
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_4) == 0) ? 1 : 0;
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0) ? 1 : 0;

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 1);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 1);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 1);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 0); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 1); // 7
    ks_delay();
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_4) == 0) ? 1 : 0;
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0) ? 1 : 0;

    HAL_GPIO_WritePin(GPIOE, GPIO_PIN_5, 1);  // 3
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_8, 1);  // 4
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_9, 1);  // 5
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_10, 1); // 6
    HAL_GPIO_WritePin(GPIOI, GPIO_PIN_11, 0); // 7
    ks_delay();
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOI, GPIO_PIN_4) == 0) ? 1 : 0;
    xkey <<= 1;
    xkey |= (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0) ? 1 : 0;

    return xkey;
}

/*Will be called by the library to read the mouse*/
static void keypad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static volatile uint32_t last_key = 0;

    /*Get whether the a key is pressed and save the pressed key*/
    uint32_t act_key = keypad_get_key();
    if (act_key != 0)
    {

        data->state = LV_INDEV_STATE_PR;

        /*Translate the keys to LVGL control characters according to your key definitions*/
        switch (act_key)
        {
        case 1:
            act_key = LV_KEY_ENTER;
            break;
        case 2:
            act_key = LV_KEY_ENTER;
            break;
        case 3:
            act_key = LV_KEY_ENTER;
            break;
        case 4:
            act_key = LV_KEY_ENTER;
            break;
        case 5:
            act_key = LV_KEY_ENTER;
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

void lv_port_keypad_init(void)
{
    static lv_indev_drv_t indev_drv;
    /*------------------
     * Keypad
     * -----------------*/
    /*Initialize your keypad or keyboard if you have*/
    clean_row();

    /*Register a keypad input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv.read_cb = keypad_read;
    indev_keypad = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_keypad, group);`*/
}

// eof