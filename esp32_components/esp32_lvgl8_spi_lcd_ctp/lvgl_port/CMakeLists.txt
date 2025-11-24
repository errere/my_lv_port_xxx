idf_component_register(SRCS "lv_port_lcd.c" "lv_port_tick.c" "lv_port_ctp.c" "lv_port_font_ram.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl bsp_lcd gt911_ctp nvs_flash esp_partition)
