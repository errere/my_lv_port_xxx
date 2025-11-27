idf_component_register(SRCS "lv_port_lcd.c" "lv_port_tick.c" "lv_port_ctp.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_lcd_st7796 lvgl-9.4.0 esp_timer bsp_ctp_ft6x36u)
