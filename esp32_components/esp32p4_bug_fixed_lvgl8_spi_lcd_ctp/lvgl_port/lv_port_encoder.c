#include "lv_port_encoder.h"
#include "driver/pulse_cnt.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "driver/gpio.h"

static const char *TAG = "encoder";

#define _lV_ENCODER_PCNT_HIGH_LIMIT SHRT_MAX
#define _lV_ENCODER_PCNT_LOW_LIMIT SHRT_MIN

#define _lV_ENCODER_EC11_GPIO_A 21
#define _lV_ENCODER_EC11_GPIO_B 23
#define _lV_ENCODER_EC11_GPIO_KEY 22

static void encoder_init(void);
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data);

static lv_indev_drv_t indev_drv;
static lv_indev_t *lv_indev_encoder;

static pcnt_unit_handle_t pcnt_unit;

void lv_port_encoder_init(lv_group_t **group)
{
    /*------------------
     * Encoder
     * -----------------*/

    /*Initialize your encoder if you have*/
    encoder_init();

    /*Register a encoder input device*/
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_ENCODER;
    indev_drv.read_cb = encoder_read;
    lv_indev_encoder = lv_indev_drv_register(&indev_drv);

    /*Later you should create group(s) with `lv_group_t * group = lv_group_create()`,
     *add objects to the group with `lv_group_add_obj(group, obj)`
     *and assign this input device to group to navigate in it:
     *`lv_indev_set_group(indev_encoder, group);`*/
    *group = lv_group_create();
    lv_indev_set_group(lv_indev_encoder, *group);
}

/*------------------
 * Encoder
 * -----------------*/

/*Initialize your Encoder*/
static void encoder_init(void)
{
    ESP_LOGI(TAG, "this code fix for no key");
    /*Your code comes here*/
    gpio_reset_pin(_lV_ENCODER_EC11_GPIO_KEY);
    gpio_set_direction(_lV_ENCODER_EC11_GPIO_KEY, GPIO_MODE_INPUT);

    // ESP_LOGI(TAG, "install pcnt unit");
    pcnt_unit_config_t unit_config = {
        .high_limit = _lV_ENCODER_PCNT_HIGH_LIMIT,
        .low_limit = _lV_ENCODER_PCNT_LOW_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_config, &pcnt_unit));

    // ESP_LOGI(TAG, "set glitch filter");
    pcnt_glitch_filter_config_t filter_config = {
        .max_glitch_ns = 1000,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(pcnt_unit, &filter_config));

    // ESP_LOGI(TAG, "install pcnt channels");
    pcnt_chan_config_t chan_a_config = {
        .edge_gpio_num = _lV_ENCODER_EC11_GPIO_A,
        .level_gpio_num = _lV_ENCODER_EC11_GPIO_B,
    };
    pcnt_channel_handle_t pcnt_chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_a_config, &pcnt_chan_a));

    pcnt_chan_config_t chan_b_config = {
        .edge_gpio_num = _lV_ENCODER_EC11_GPIO_B,
        .level_gpio_num = _lV_ENCODER_EC11_GPIO_A,
    };
    pcnt_channel_handle_t pcnt_chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(pcnt_unit, &chan_b_config, &pcnt_chan_b));

    // ESP_LOGI(TAG, "set edge and level actions for pcnt channels");
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_a, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_a, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));
    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(pcnt_chan_b, PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(pcnt_chan_b, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    // ESP_LOGI(TAG, "enable pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_enable(pcnt_unit));
    // ESP_LOGI(TAG, "clear pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_clear_count(pcnt_unit));
    // ESP_LOGI(TAG, "start pcnt unit");
    ESP_ERROR_CHECK(pcnt_unit_start(pcnt_unit));
}

/*Will be called by the library to read the encoder*/
static void encoder_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    /*Your code comes here*/
    static int pulse_count = 0;

    ESP_ERROR_CHECK(pcnt_unit_get_count(pcnt_unit, &pulse_count));
    pulse_count = pulse_count / 4;
    if (pulse_count)
        pcnt_unit_clear_count(pcnt_unit);

    // ESP_LOGI(TAG, "pulse_count = %d", pulse_count);
    data->enc_diff += pulse_count;
    // data->state = (gpio_get_level(_lV_ENCODER_EC11_GPIO_KEY) == 0) ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
    data->state = LV_INDEV_STATE_REL;
}