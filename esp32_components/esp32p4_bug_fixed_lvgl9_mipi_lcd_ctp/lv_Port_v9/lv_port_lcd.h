#ifndef __LV_PORT_LCD_H
#define __LV_PORT_LCD_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "lvgl.h"

#define LV_PORT_DISP_BUFFER_SIZE 30 //缓冲区大小为100倍的LCD横向分辨率

    void lv_port_disp_init(void);

#ifdef __cplusplus
} /* extern "C" */
#endif

#endif
