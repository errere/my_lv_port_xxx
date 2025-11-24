idf_component_register(SRCS "lv_port_lcd.c" "lv_port_font.c" "lv_port_tick.c" "lv_port_ctp.c" "lv_port_encoder.c"
                       INCLUDE_DIRS "."
                       REQUIRES lvgl bsp_lcd bsp_ctp_ft5x16 nvs_flash esp_timer driver esp_partition)
