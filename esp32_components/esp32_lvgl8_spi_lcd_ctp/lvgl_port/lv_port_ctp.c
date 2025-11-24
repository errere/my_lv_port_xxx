/*
 * MIT License
 *
 * Copyright (c) 2025 G.C.Hikari
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#include "lv_port_ctp.h"
#include "lvgl.h"
#include "esp_log.h"
#include "bsp_gt911.h"

//static const char *TAG = "lv_port_ctp";

lv_indev_t *indev_touchpad;

static void touchpad_read(lv_indev_drv_t *indev_drv, lv_indev_data_t *data)
{
    static lv_coord_t last_x = 0;
    static lv_coord_t last_y = 0;

    /*Save the pressed coordinates and the state*/
    if (xSemaphoreTake(xSemTouchDown, 0) == pdPASS)
    {
        uint16_t tch[3];
        gt911_read_touch(&tch[0], &tch[1], &tch[2]);
        last_x = tch[0];
        last_y = tch[1];
        if (tch[2] && last_x < 480 && last_y < 272)
        {
            //ESP_LOGI(TAG, "x = %d , y = %d , sz = %d", tch[0], tch[1], tch[2]);
            data->state = LV_INDEV_STATE_PR;
        }
        else
        {
            data->state = LV_INDEV_STATE_REL;
        }

        
    }
    else
    {
        data->state = LV_INDEV_STATE_REL;
        // ESP_LOGI(TAG,"np");
    }

    /*Set the last pressed coordinates*/
    data->point.x = last_x;
    data->point.y = last_y;
}

void lv_port_ctp_init()
{
    static lv_indev_drv_t indev_drv;

    gt911_ctp_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    indev_touchpad = lv_indev_drv_register(&indev_drv);
}

// eof