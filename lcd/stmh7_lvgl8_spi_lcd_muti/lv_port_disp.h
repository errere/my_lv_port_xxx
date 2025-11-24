#ifndef __LV_PORT_DISP_H__
#define __LV_PORT_DISP_H__
#include "lvgl.h"


#define LV_PORT_DISP_BUFFER_SIZE (20 * 240) //缓冲区大小

extern lv_disp_t *display_intf[3];

void lv_port_disp_init(void);

#endif
