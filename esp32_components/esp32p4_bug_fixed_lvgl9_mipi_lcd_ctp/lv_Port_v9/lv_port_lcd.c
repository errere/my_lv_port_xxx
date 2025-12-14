#include "lv_port_lcd.h"

#include "esp_log.h"
#include "esp_lcd_panel_io.h"

#include "lv_port_lcd.h"
#include <esp_system.h>
#include <esp_log.h>

#include "esp_lcd_types.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_mipi_dsi.h"
#include "esp_ldo_regulator.h"
#include "driver/gpio.h"

#include "esp_lcd_panel_commands.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_mipi_dsi.h"

#include "esp_lcd_st7796.h"

#include "esp_attr.h"

static const char *TAG = "lv_port_lcd_dsi";

static esp_lcd_panel_handle_t mipi_dpi_panel = NULL;

__attribute__((aligned(64))) static EXT_RAM_BSS_ATTR uint8_t buf_2_1[320 * LV_PORT_DISP_BUFFER_SIZE * 3];

__attribute__((aligned(64))) static EXT_RAM_BSS_ATTR uint8_t buf_2_2[320 * LV_PORT_DISP_BUFFER_SIZE * 3];

static void bsp_enable_dsi_phy_power(void)
{
    // Turn on the power for MIPI DSI PHY, so it can go from "No Power" state to "Shutdown" state
    esp_ldo_channel_handle_t ldo_mipi_phy = NULL;

    esp_ldo_channel_config_t ldo_mipi_phy_config = {
        .chan_id = 3,
        .voltage_mv = 2500,
    };
    ESP_ERROR_CHECK(esp_ldo_acquire_channel(&ldo_mipi_phy_config, &ldo_mipi_phy));
    ESP_LOGI(TAG, "MIPI DSI PHY Powered on");
} // bsp_enable_dsi_phy_power

static bool dsi_trans_done_event_cb(esp_lcd_panel_handle_t panel, esp_lcd_dpi_panel_event_data_t *edata, void *user_ctx)
{
    lv_display_t *disp = (lv_display_t *)user_ctx;
    lv_display_flush_ready(disp);
    return false;
} // dsi_trans_done_event_cb

/*Flush the content of the internal buffer the specific area on the display.
 *`px_map` contains the rendered image as raw pixel map and it should be copied to `area` on the display.
 *You can use DMA or any hardware acceleration to do this operation in the background but
 *'lv_display_flush_ready()' has to be called when it's finished.*/
static void disp_flush(lv_display_t *disp_drv, const lv_area_t *area, uint8_t *px_map)
{

    int offsetx1 = area->x1;
    int offsetx2 = area->x2;
    int offsety1 = area->y1;
    int offsety2 = area->y2;
    // pass the draw buffer to the driver
    esp_lcd_panel_draw_bitmap(mipi_dpi_panel, offsetx1, offsety1, offsetx2 + 1, offsety2 + 1, px_map);
} // disp_flush

static void dsi_init()
{
    bsp_enable_dsi_phy_power();

    gpio_reset_pin(LCD_RST_PIN);
    gpio_set_direction(LCD_RST_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_RST_PIN, 0);
    vTaskDelay(10);
    gpio_set_level(LCD_RST_PIN, 1);
    vTaskDelay(20);


    // create MIPI DSI bus first, it will initialize the DSI PHY as well
    esp_lcd_dsi_bus_handle_t mipi_dsi_bus;
    esp_lcd_dsi_bus_config_t bus_config = {
        .bus_id = 0,
        .num_data_lanes = 1,
        .phy_clk_src = MIPI_DSI_PHY_CLK_SRC_PLL_F20M,
        .lane_bit_rate_mbps = 500,
    };
    ESP_ERROR_CHECK(esp_lcd_new_dsi_bus(&bus_config, &mipi_dsi_bus));

    ESP_LOGI(TAG, "Install MIPI DSI LCD control IO");
    esp_lcd_panel_io_handle_t mipi_dbi_io;
    // we use DBI interface to send LCD commands and parameters
    esp_lcd_dbi_io_config_t dbi_config = {
        .virtual_channel = 0,
        .lcd_cmd_bits = 8,   // according to the LCD spec
        .lcd_param_bits = 8, // according to the LCD spec
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_dbi(mipi_dsi_bus, &dbi_config, &mipi_dbi_io));

    ESP_LOGI(TAG, "Install MIPI DSI LCD data panel");

    esp_lcd_dpi_panel_config_t dpi_config = ST7796_320_480_PANEL_60HZ_DPI_CONFIG(LCD_COLOR_PIXEL_FORMAT_RGB888);

    st7796_vendor_config_t vendor_config = {
        .mipi_config = {
            .dsi_bus = mipi_dsi_bus,
            .dpi_config = &dpi_config,
        },
        .flags.use_mipi_interface = 1,
    };

    esp_lcd_panel_dev_config_t lcd_dev_config = {
        .reset_gpio_num = 22,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 24,
        .vendor_config = &vendor_config,
    };

    esp_lcd_new_panel_st7796(mipi_dbi_io, &lcd_dev_config, &mipi_dpi_panel);
    // ESP_LOGI(TAG, "1 done");
    ESP_ERROR_CHECK(esp_lcd_panel_reset(mipi_dpi_panel));
    // ESP_LOGI(TAG, "2 done");
    ESP_ERROR_CHECK(esp_lcd_panel_init(mipi_dpi_panel));
    //  ESP_LOGI(TAG, "3 done");
    esp_lcd_panel_disp_on_off(mipi_dpi_panel, 1);
    ESP_LOGI(TAG, "lv port dsi init done");

} // dsi_init

void lv_port_disp_init(void)
{
    /*-------------------------
     * Initialize your display
     * -----------------------*/
    dsi_init();
    /*------------------------------------
     * Create a display and set a flush_cb
     * -----------------------------------*/
    lv_display_t *disp = lv_display_create(320, 480);

    // set color depth
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB888);
    lv_display_set_buffers(disp, buf_2_1, buf_2_2, sizeof(buf_2_1), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush);

    esp_lcd_dpi_panel_event_callbacks_t cbs = {
        .on_color_trans_done = dsi_trans_done_event_cb,
    };
    ESP_ERROR_CHECK(esp_lcd_dpi_panel_register_event_callbacks(mipi_dpi_panel, &cbs, disp));
}
// for lvgl9
