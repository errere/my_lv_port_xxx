#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__
#include "lvgl.h"


#define LV_PORT_DISP_BUFFER_SIZE (80 * 320) //缓冲区大小

extern lv_disp_t *display_intf;

void lv_port_disp_init(void);

#endif
