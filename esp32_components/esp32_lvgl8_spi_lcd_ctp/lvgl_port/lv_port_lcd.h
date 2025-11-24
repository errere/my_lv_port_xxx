#ifndef __LV_PORT_LCD_H
#define __LV_PORT_LCD_H

#include "lvgl.h"

#define LV_PORT_DISP_BUFFER_SIZE 30 //缓冲区大小为100倍的LCD横向分辨率

void lv_port_disp_init(void);

#endif
