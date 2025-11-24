#include "lv_port_encoder.h"
// #include "elog.h"

lv_indev_t *indev_encoder[2];
static int32_t encoder_diff[2];
static lv_indev_state_t encoder_state[2];

static void encoder_init(void);
static void encoder_read(lv_indev_t *indev, lv_indev_data_t *data);
static void encoder_handler(void);
/*------------------
 * Encoder
 * -----------------*/

/*Initialize your encoder*/
static void encoder_init(void)
{
    /*Your code comes here*/
    // 开启编码器
    __HAL_TIM_SET_COUNTER(&htim2, 0x7ff);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_1);
    HAL_TIM_Encoder_Start(&htim2, TIM_CHANNEL_2);

    // 开启编码器
    __HAL_TIM_SET_COUNTER(&htim3, 0x7ff);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_1);
    HAL_TIM_Encoder_Start(&htim3, TIM_CHANNEL_2);
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_t *indev_drv, lv_indev_data_t *data)
{
    int32_t x;
    if (indev_drv == indev_encoder[0])
    {
        x = __HAL_TIM_GET_COUNTER(&htim2);
        data->enc_diff = x - 0x7ff;
        //data->enc_diff = data->enc_diff / 2;
        __HAL_TIM_SET_COUNTER(&htim2, 0x7ff);
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_3) == 0)
        {
            data->state = LV_INDEV_STATE_PR;
        }
        else
        {
            data->state = LV_INDEV_STATE_REL;
        }
        // log_i("in enctim2 : %d", data->enc_diff);
    }
    else if (indev_drv == indev_encoder[1])
    {
        x = __HAL_TIM_GET_COUNTER(&htim3);
        data->enc_diff = x - 0x7ff;
        //data->enc_diff = data->enc_diff / 2;
        __HAL_TIM_SET_COUNTER(&htim3, 0x7ff);
        if (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_8) == 0)
        {
            data->state = LV_INDEV_STATE_PR;
        }
        else
        {
            data->state = LV_INDEV_STATE_REL;
        }
        // log_i("in enctim3 : %d", data->enc_diff);
    }
}

void lv_port_encoder_init(void)
{
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    indev_encoder[0] = lv_indev_create();
    lv_indev_set_type(indev_encoder[0], LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder[0], encoder_read);

    /*Register a encoder input device*/
    indev_encoder[1] = lv_indev_create();
    lv_indev_set_type(indev_encoder[1], LV_INDEV_TYPE_ENCODER);
    lv_indev_set_read_cb(indev_encoder[1], encoder_read);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_encoder, group);`*/
}
// eof