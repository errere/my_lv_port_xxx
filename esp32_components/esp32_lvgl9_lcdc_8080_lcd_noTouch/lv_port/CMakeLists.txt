idf_component_register(SRCS "lv_port_lcd.c" "lv_port_input_keypad.c" "lv_port_tick.c" "lv_port_input_button.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver bsp_lcd_pal16 lvgl-9.1.0 tca_iic_gpio)
