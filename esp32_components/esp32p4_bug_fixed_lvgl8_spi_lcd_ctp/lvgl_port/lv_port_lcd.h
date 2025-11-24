#ifndef __LV_PORT_LCD_H
#define __LV_PORT_LCD_H

#include "lvgl.h"

#ifdef __cplusplus
extern "C"
{
#endif

extern lv_disp_t *display_intf;

#define LV_PORT_DISP_BUFFER_SIZE (20*LCD_X_PIXELS) // 缓冲区大小为100倍的LCD横向分辨率

    void lv_port_disp_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
